using System.Buffers.Binary;
using System.Diagnostics;
using System.IO.MemoryMappedFiles;
using System.Net;
using System.Net.Sockets;
using System.Net.WebSockets;
using System.Security.Cryptography;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Magpie.BrowserVideoHost;

// A native-messaging port owns this server. Closing the port closes the socket
// and shared memory. No frames are saved, and no non-loopback listener exists.
internal static class FrameServer
{
    private const int HeaderSize = 64;
    private const int MaxPixels = 3840 * 2160;
    private const int MaxBytes = HeaderSize + MaxPixels * 4;
    private const uint Magic = 0x31465642; // BVF1, little endian

    internal static void Run(JsonElement request, Action<object> respond)
    {
        string origin = new Uri(request.GetProperty("pageUrl").GetString()!).GetLeftPart(UriPartial.Authority);
        if (!origin.StartsWith("https://") && !origin.StartsWith("http://"))
            throw new InvalidDataException("Unsupported video page origin.");
        string id = Convert.ToHexString(RandomNumberGenerator.GetBytes(16));
        string token = Convert.ToHexString(RandomNumberGenerator.GetBytes(32));
        string mapName = "Local\\MagpieBrowserFrame_" + id;
        using var memory = MemoryMappedFile.CreateNew(mapName, MaxBytes, MemoryMappedFileAccess.ReadWrite);
        using var view = memory.CreateViewAccessor();
        using var frameLock = new Mutex(false, mapName + "_Mutex");
        using var cancelled = new CancellationTokenSource();
        using var listener = new TcpListener(IPAddress.Loopback, 0);
        listener.Start(1);
        // Native stdin EOF is the lifetime boundary, including browser shutdown.
        _ = Task.Run(() => { try { while (Console.OpenStandardInput().ReadByte() != -1) { } } finally { cancelled.Cancel(); listener.Stop(); } });
        respond(new { ok = true, state = "frames", mapName, url = $"ws://127.0.0.1:{((IPEndPoint)listener.LocalEndpoint).Port}/{token}" });
        try { Serve(listener, origin, token, view, frameLock, cancelled.Token).GetAwaiter().GetResult(); }
        catch (OperationCanceledException) { }
        catch (SocketException) when (cancelled.IsCancellationRequested) { }
    }

    private static async Task Serve(TcpListener listener, string origin, string token,
        MemoryMappedViewAccessor view, Mutex frameLock, CancellationToken cancellation)
    {
        using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellation);
        deadline.CancelAfter(TimeSpan.FromSeconds(20));
        using TcpClient client = await listener.AcceptTcpClientAsync(deadline.Token);
        client.NoDelay = true;
        client.ReceiveBufferSize = 4 * 1024 * 1024;
        using NetworkStream stream = client.GetStream();
        var header = new List<byte>();
        var single = new byte[1];
        while (header.Count < 8192)
        {
            if (await stream.ReadAsync(single, deadline.Token) != 1) throw new IOException("Incomplete WebSocket handshake.");
            header.Add(single[0]);
            int count = header.Count;
            if (count >= 4 && header[count - 4] == 13 && header[count - 3] == 10 && header[count - 2] == 13 && header[count - 1] == 10) break;
        }
        string[] lines = Encoding.ASCII.GetString(header.ToArray()).Split("\r\n");
        var fields = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        foreach (string line in lines.Skip(1))
        {
            int colon = line.IndexOf(':');
            if (colon > 0) fields[line[..colon]] = line[(colon + 1)..].Trim();
        }
        string? connectionOrigin = fields.GetValueOrDefault("Origin");
        if (lines[0] != $"GET /{token} HTTP/1.1" ||
            (connectionOrigin != origin && connectionOrigin != "chrome-extension://peflachlahjaaldgfbjkiniphnplmhgp") ||
            !string.Equals(fields.GetValueOrDefault("Upgrade"), "websocket", StringComparison.OrdinalIgnoreCase) ||
            fields.GetValueOrDefault("Sec-WebSocket-Version") != "13" || !fields.TryGetValue("Sec-WebSocket-Key", out string? key) ||
            Convert.FromBase64String(key).Length != 16)
            throw new InvalidDataException("Frame connection authentication failed.");
        string accept = Convert.ToBase64String(SHA1.HashData(Encoding.ASCII.GetBytes(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
        await stream.WriteAsync(Encoding.ASCII.GetBytes($"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: {accept}\r\n\r\n"), deadline.Token);
        deadline.CancelAfter(Timeout.Infinite);
        using WebSocket socket = WebSocket.CreateFromStream(stream, true, null, TimeSpan.FromSeconds(10));
        byte[] packet = GC.AllocateUninitializedArray<byte>(MaxBytes);
        ulong sequence = 0;
        while (!cancellation.IsCancellationRequested && socket.State == WebSocketState.Open)
        {
            var receiveWatch = Stopwatch.StartNew();
            int length = 0;
            ValueWebSocketReceiveResult received;
            do
            {
                received = await socket.ReceiveAsync(packet.AsMemory(length), cancellation);
                if (received.MessageType == WebSocketMessageType.Close) return;
                if (received.MessageType != WebSocketMessageType.Binary || length + received.Count >= MaxBytes && !received.EndOfMessage)
                    throw new InvalidDataException("Invalid frame message.");
                length += received.Count;
            } while (!received.EndOfMessage);
            ValidatePacket(packet.AsSpan(0, length));
            double receiveMs = receiveWatch.Elapsed.TotalMilliseconds;
            BinaryPrimitives.WriteUInt64LittleEndian(packet.AsSpan(24), ++sequence);
            BinaryPrimitives.WriteInt64LittleEndian(packet.AsSpan(32), (long)(Stopwatch.GetTimestamp() * (10000000.0 / Stopwatch.Frequency)));
            // Mutex ownership stays on this thread: never await inside the lock.
            var writeWatch = Stopwatch.StartNew();
            if (!frameLock.WaitOne(2))
            {
                // A GPU resource rebuild can briefly hold the reader lock.
                // Drop this picture rather than disconnecting the whole player.
                await socket.SendAsync(Encoding.UTF8.GetBytes("{\"ok\":true,\"dropped\":true}"), WebSocketMessageType.Text, true, cancellation);
                continue;
            }
            double lockMs = writeWatch.Elapsed.TotalMilliseconds;
            try { Marshal.Copy(packet, 0, view.SafeMemoryMappedViewHandle.DangerousGetHandle(), length); }
            finally { frameLock.ReleaseMutex(); }
            byte[] ack = JsonSerializer.SerializeToUtf8Bytes(new { ok = true, receiveMs, lockMs, writeMs = writeWatch.Elapsed.TotalMilliseconds });
            await socket.SendAsync(ack.AsMemory(), WebSocketMessageType.Text, true, cancellation);
        }
    }

    internal static void ValidatePacket(ReadOnlySpan<byte> packet)
    {
        if (packet.Length < HeaderSize || BinaryPrimitives.ReadUInt32LittleEndian(packet) != Magic ||
            BinaryPrimitives.ReadUInt32LittleEndian(packet[4..]) != 1)
            throw new InvalidDataException("Invalid decoded-frame header.");
        uint width = BinaryPrimitives.ReadUInt32LittleEndian(packet[8..]);
        uint height = BinaryPrimitives.ReadUInt32LittleEndian(packet[12..]);
        uint stride = BinaryPrimitives.ReadUInt32LittleEndian(packet[16..]);
        uint bytes = BinaryPrimitives.ReadUInt32LittleEndian(packet[20..]);
        uint format = BinaryPrimitives.ReadUInt32LittleEndian(packet[48..]);
        uint matrix = BinaryPrimitives.ReadUInt32LittleEndian(packet[52..]);
        uint fullRange = BinaryPrimitives.ReadUInt32LittleEndian(packet[56..]);
        bool yuv = format is 1 or 2;
        if (width < 64 || height < 64 || width > 4096 || height > 4096 ||
            (ulong)width * height > MaxPixels || format > 2 ||
            (yuv && ((width | height) % 2 != 0 || matrix is < 1 or > 3 || fullRange > 1)) ||
            stride != width * (yuv ? 1 : 4) || bytes != (ulong)width * height * (yuv ? 3UL : 8UL) / 2 ||
            packet.Length != HeaderSize + bytes)
            throw new InvalidDataException("Invalid decoded-frame dimensions or payload size.");
    }
}

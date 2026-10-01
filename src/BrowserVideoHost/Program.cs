using System.Buffers.Binary;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Magpie.BrowserVideoHost;

internal static class Program
{
    private const int MaxMessageBytes = 64 * 1024;
    private const string RequestFileName = "browser-video-request.json";
    private const string MessageName = "WM_MAGPIE_BROWSER_VIDEO";
    private static readonly nint HwndBroadcast = new(0xffff);

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern uint RegisterWindowMessageW(string messageName);

    [DllImport("user32.dll", SetLastError = true)]
    private static extern bool PostMessageW(nint hwnd, uint message, nint wParam, nint lParam);

    [DllImport("user32.dll", CharSet = CharSet.Unicode)]
    private static extern nint FindWindowW(string className, string? windowName);

    public static int Main()
    {
        try
        {
            using JsonDocument request = ReadRequest();
            ValidateRequest(request.RootElement);

            if (request.RootElement.GetProperty("action").GetString() == "frames")
            {
                if (!IsMagpieRunning())
                {
                    WriteResponse(new { ok = true, state = "stopped" });
                    return 0;
                }
                FrameServer.Run(request.RootElement, WriteResponse);
                return 0;
            }

            string configDirectory = ResolveConfigDirectory();
            Directory.CreateDirectory(configDirectory);
            string requestPath = Path.Combine(configDirectory, RequestFileName);

            using var requestLock = new Mutex(false, "Local\\MagpieBrowserVideoAutoRequest");
            if (!requestLock.WaitOne(TimeSpan.FromSeconds(5)))
            {
                throw new InvalidOperationException("Timed out while another browser-video update was being delivered.");
            }

            try
            {
                string action = request.RootElement.GetProperty("action").GetString()!;
                // Only an explicit switch-on may launch Magpie. Geometry and
                // playback notifications must respect a tray exit or task kill.
                if (action == "enable")
                {
                    File.Delete(requestPath);
                    EnsureMagpieIsRunning();
                    WriteResponse(new { ok = true, delivered = false, state = "running" });
                    return 0;
                }
                if (!IsMagpieRunning())
                {
                    File.Delete(requestPath);
                    WriteResponse(new { ok = true, delivered = false, state = "stopped" });
                    return 0;
                }

                string temporaryPath = requestPath + "." + Environment.ProcessId + ".tmp";
                File.WriteAllText(temporaryPath, request.RootElement.GetRawText(), new UTF8Encoding(false));
                File.Move(temporaryPath, requestPath, true);

                uint message = RegisterWindowMessageW(MessageName);
                if (message == 0 || !PostMessageW(HwndBroadcast, message, nint.Zero, nint.Zero))
                {
                    throw new InvalidOperationException($"Failed to notify Magpie (Win32 error {Marshal.GetLastWin32Error()}).");
                }
            }
            finally
            {
                requestLock.ReleaseMutex();
            }

            WriteResponse(new { ok = true, delivered = true, state = "running" });
            return 0;
        }
        catch (Exception exception)
        {
            try
            {
                string logs = Path.Combine(AppContext.BaseDirectory, "logs");
                Directory.CreateDirectory(logs);
                string path = Path.Combine(logs, "browser-video-host-errors.log");
                if (File.Exists(path) && new FileInfo(path).Length > 1024 * 1024) File.Move(path, path + ".previous", true);
                File.AppendAllText(path, $"{DateTimeOffset.Now:O} {exception}\n");
            }
            catch { /* Diagnostics must not replace the native error response. */ }
            WriteResponse(new { ok = false, error = exception.Message });
            return 1;
        }
    }

    private static JsonDocument ReadRequest()
    {
        Stream input = Console.OpenStandardInput();
        Span<byte> lengthBytes = stackalloc byte[4];
        input.ReadExactly(lengthBytes);
        int length = BinaryPrimitives.ReadInt32LittleEndian(lengthBytes);
        if (length is <= 0 or > MaxMessageBytes)
        {
            throw new InvalidDataException("The native message length is invalid.");
        }

        byte[] payload = GC.AllocateUninitializedArray<byte>(length);
        input.ReadExactly(payload);
        return JsonDocument.Parse(payload);
    }

    private static void ValidateRequest(JsonElement request)
    {
        if (request.ValueKind != JsonValueKind.Object ||
            !request.TryGetProperty("action", out JsonElement actionElement) ||
            actionElement.ValueKind != JsonValueKind.String)
        {
            throw new InvalidDataException("The native message must contain an action.");
        }

        string? action = actionElement.GetString();
        if (action is not ("enable" or "update" or "disable" or "frames"))
        {
            throw new InvalidDataException("The native message action is not supported.");
        }
    }

    private static string ResolveConfigDirectory()
    {
        string runtimeDirectory = AppContext.BaseDirectory;
        string portableDirectory = Path.Combine(runtimeDirectory, "config", "v4e");
        if (File.Exists(Path.Combine(portableDirectory, "config.json")))
        {
            return portableDirectory;
        }

        return Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "Magpie", "config", "v4e");
    }

    private static void EnsureMagpieIsRunning()
    {
        if (IsMagpieRunning())
        {
            return;
        }

        string magpiePath = Path.Combine(AppContext.BaseDirectory, "Magpie.exe");
        if (!File.Exists(magpiePath))
        {
            throw new FileNotFoundException("Magpie.exe was not found next to the native host.", magpiePath);
        }

        Process.Start(new ProcessStartInfo(magpiePath, "-t")
        {
            WorkingDirectory = AppContext.BaseDirectory,
            UseShellExecute = true,
            WindowStyle = ProcessWindowStyle.Hidden
        });

        Stopwatch wait = Stopwatch.StartNew();
        while (wait.Elapsed < TimeSpan.FromSeconds(10))
        {
            Thread.Sleep(100);
            if (IsMagpieRunning())
            {
                // The process becomes visible before its hidden notification
                // windows and message loop are ready.
                Thread.Sleep(900);
                return;
            }
        }

        throw new TimeoutException("Magpie did not become ready within 10 seconds.");
    }

    private static bool IsMagpieRunning()
    {
        if (FindWindowW("Magpie_Main", null) != nint.Zero ||
            FindWindowW("Magpie_NotifyIcon", null) != nint.Zero ||
            FindWindowW("Magpie_Hotkey", null) != nint.Zero)
        {
            return true;
        }

        foreach (Process process in Process.GetProcessesByName("Magpie"))
        {
            using (process)
            {
                try
                {
                    if (!process.HasExited) return true;
                }
                catch
                {
                    // A process that disappeared between enumeration and the
                    // check is not a running Magpie instance.
                }
            }
        }
        return false;
    }

    private static void WriteResponse(object response)
    {
        byte[] payload = JsonSerializer.SerializeToUtf8Bytes(response);
        Span<byte> lengthBytes = stackalloc byte[4];
        BinaryPrimitives.WriteInt32LittleEndian(lengthBytes, payload.Length);
        Stream output = Console.OpenStandardOutput();
        output.Write(lengthBytes);
        output.Write(payload);
        output.Flush();
    }
}

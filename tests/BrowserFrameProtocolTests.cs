using System.Buffers.Binary;
using Magpie.BrowserVideoHost;

static byte[] Packet(uint width, uint height, uint format)
{
    uint bytes = width * height * (format == 0 ? 8U : 3U) / 2;
    var packet = new byte[64 + bytes];
    uint[] fields = [0x31465642, 1, width, height, width * (format == 0 ? 4U : 1U), bytes];
    for (int i = 0; i < fields.Length; i++) BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(i * 4), fields[i]);
    BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(48), format);
    BinaryPrimitives.WriteUInt32LittleEndian(packet.AsSpan(52), 1);
    return packet;
}
static void Reject(byte[] packet)
{
    try { FrameServer.ValidatePacket(packet); }
    catch (InvalidDataException) { return; }
    throw new Exception("Malformed frame accepted");
}
foreach (uint format in new uint[] { 0, 1, 2 }) FrameServer.ValidatePacket(Packet(1920, 1080, format));
Reject([]);
Reject(Packet(1920, 1080, 0)[..^1]);
Reject(Packet(65, 1080, 1));
var oversized = Packet(64, 64, 0);
BinaryPrimitives.WriteUInt32LittleEndian(oversized.AsSpan(8), uint.MaxValue);
Reject(oversized);
var invalidColor = Packet(64, 64, 2);
BinaryPrimitives.WriteUInt32LittleEndian(invalidColor.AsSpan(52), 99);
Reject(invalidColor);
var invalidStride = Packet(64, 64, 1);
BinaryPrimitives.WriteUInt32LittleEndian(invalidStride.AsSpan(16), 65);
Reject(invalidStride);
var invalidMagic = Packet(64, 64, 0); invalidMagic[0] = 0;
Reject(invalidMagic);
Console.WriteLine("PASS: RGBA/NV12/I420 decoded-frame bounds, stride, color metadata and truncation validation");

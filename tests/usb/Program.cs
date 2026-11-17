using System.Buffers.Binary;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Xml.Linq;
using UnkxwynoMirror.Desktop.Services.Usb;

var failures = new List<string>();
void Check(bool value, string message) { if (!value) failures.Add(message); }
async Task Reject<T>(Func<Task> action, string message) where T : Exception
{
    try { await action(); failures.Add(message); }
    catch (T) { }
}
const string result = "<plist><dict><key>MessageType</key><string>Result</string><key>Number</key><integer>0</integer></dict></plist>";
const string devices = "<plist><dict><key>DeviceList</key><array><dict><key>DeviceID</key><integer>7</integer><key>Properties</key><dict><key>ConnectionType</key><string>USB</string><key>ProductID</key><integer>4776</integer></dict></dict><dict><key>DeviceID</key><integer>9</integer><key>Properties</key><dict><key>ConnectionType</key><string>Network</string></dict></dict></array></dict></plist>";

async Task Run(Func<NetworkStream, Task> server, Func<UsbmuxClient, Task> test)
{
    using var listener = new TcpListener(IPAddress.Loopback, 0);
    listener.Start();
    var port = ((IPEndPoint)listener.LocalEndpoint).Port;
    using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(6));
    var worker = Task.Run(async () =>
    {
        using var peer = await listener.AcceptTcpClientAsync(deadline.Token);
        await server(peer.GetStream());
    });
    try { await test(new UsbmuxClient(port, TimeSpan.FromMilliseconds(700))); }
    finally { await worker.WaitAsync(deadline.Token); }
}
async Task<XDocument> Request(NetworkStream stream)
{
    using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(3));
    byte[] header = new byte[16];
    await stream.ReadExactlyAsync(header, deadline.Token);
    Check(BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(4)) == 1 && BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(8)) == 8,
        "Request must use plist protocol version 1/type 8.");
    var length = BinaryPrimitives.ReadUInt32LittleEndian(header);
    if (length < 16 || length > 4096) throw new InvalidDataException("Test server rejected request bounds.");
    byte[] payload = new byte[(int)length - 16];
    await stream.ReadExactlyAsync(payload, deadline.Token);
    return XDocument.Parse(Encoding.UTF8.GetString(payload));
}
async Task Reply(NetworkStream stream, string xml, uint tag = 1, uint? declaredLength = null)
{
    var payload = Encoding.UTF8.GetBytes(xml);
    byte[] header = new byte[16];
    BinaryPrimitives.WriteUInt32LittleEndian(header, declaredLength ?? (uint)(16 + payload.Length));
    BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(4), 1);
    BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(8), 8);
    BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(12), tag);
    // Fragment both header and payload to exercise real stream reassembly.
    foreach (var value in header) await stream.WriteAsync(new byte[] { value });
    await stream.WriteAsync(payload);
}

await Run(async stream => { await Request(stream); await Reply(stream, devices); }, async client =>
{
    var found = await client.ListUsbDevicesAsync();
    Check(found is [{ DeviceId: 7, ProductId: 4776 }], "USB enumeration must exclude network devices.");
});
await Run(async stream =>
{
    var request = await Request(stream);
    var values = request.Descendants("dict").First().Elements().ToArray();
    var index = Array.FindIndex(values, item => item.Name == "key" && item.Value == "PortNumber");
    Check(index >= 0 && values[index + 1].Value == "32767", "Connect port 65407 must be serialized in network byte order as 32767.");
    await Reply(stream, result);
    await stream.WriteAsync("USB!"u8.ToArray());
    byte[] ack = new byte[4];
    using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(2));
    await stream.ReadExactlyAsync(ack, deadline.Token);
    Check(ack.SequenceEqual("ACK!"u8.ToArray()), "Tunnel must carry bidirectional raw bytes after Result.");
}, async client =>
{
    await using var tunnel = await client.OpenTunnelAsync(7, 65407);
    byte[] greeting = new byte[4];
    using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(2));
    await tunnel.Stream.ReadExactlyAsync(greeting, deadline.Token);
    Check(greeting.SequenceEqual("USB!"u8.ToArray()), "Connect must preserve bytes arriving immediately after the reply.");
    await tunnel.Stream.WriteAsync("ACK!"u8.ToArray());
});
await Run(async s => { await Request(s); await Reply(s, result, tag: 2); }, c => Reject<InvalidDataException>(async () => await c.ListUsbDevicesAsync(), "Mismatched reply tag must fail."));
await Run(async s => { await Request(s); await Reply(s, result, declaredLength: uint.MaxValue); }, c => Reject<InvalidDataException>(async () => await c.ListUsbDevicesAsync(), "Oversized reply must fail before allocation."));
await Run(async s => { await Request(s); await Reply(s, result.Replace("<integer>0", "<integer>3")); }, c => Reject<UsbmuxException>(async () => await c.OpenTunnelAsync(7, 49791), "Refused tunnel must fail."));
await Run(async s => { await Request(s); await Task.Delay(1100); }, c => Reject<OperationCanceledException>(async () => await c.ListUsbDevicesAsync(), "Stalled service must hit the deadline."));

if (args.Contains("--host"))
{
    try
    {
        var found = await new UsbmuxClient().ListUsbDevicesAsync();
        Console.WriteLine($"HOST: Apple USB service responded; {found.Count} physical USB device(s). No identifiers logged.");
    }
    catch (Exception exception) { failures.Add($"HOST: {exception.GetType().Name}: {exception.Message}"); }
}
foreach (var failure in failures) Console.Error.WriteLine($"FAIL: {failure}");
if (failures.Count != 0) return 1;
Console.WriteLine("USB protocol boundary tests passed (simulated service; physical mirroring not asserted).");
return 0;

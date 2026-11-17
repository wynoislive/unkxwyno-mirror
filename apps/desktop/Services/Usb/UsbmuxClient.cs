using System.Buffers.Binary;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Xml;
using System.Xml.Linq;

namespace UnkxwynoMirror.Desktop.Services.Usb;

public sealed record AppleUsbDevice(uint DeviceId, ushort ProductId);

public sealed class UsbmuxException(uint resultCode) : IOException($"Apple USB service rejected the request (code {resultCode}).")
{
    public uint ResultCode { get; } = resultCode;
}

// Independent implementation of the public usbmux plist framing contract.
// A tunnel only supplies bytes. It does not establish capture, trust or a stream.
public sealed class UsbmuxClient
{
    public const int DefaultPort = 27015;
    public const int MaximumMessageBytes = 1024 * 1024;
    private readonly int _servicePort;
    private readonly TimeSpan _timeout;

    public UsbmuxClient(int servicePort = DefaultPort, TimeSpan? timeout = null)
    {
        ArgumentOutOfRangeException.ThrowIfLessThan(servicePort, 1);
        ArgumentOutOfRangeException.ThrowIfGreaterThan(servicePort, 65535);
        _servicePort = servicePort;
        _timeout = timeout ?? TimeSpan.FromSeconds(4);
        if (_timeout <= TimeSpan.Zero) throw new ArgumentOutOfRangeException(nameof(timeout));
    }

    public async Task<IReadOnlyList<AppleUsbDevice>> ListUsbDevicesAsync(CancellationToken cancellationToken = default)
    {
        using var deadline = CreateDeadline(cancellationToken);
        using var socket = await OpenAsync(deadline.Token).ConfigureAwait(false);
        var reply = await RequestAsync(socket.GetStream(), "ListDevices", [], deadline.Token).ConfigureAwait(false);
        if (reply.TryGetValue("Number", out _)) CheckResult(reply);
        if (!reply.TryGetValue("DeviceList", out var list) || list.Name.LocalName != "array")
            throw new InvalidDataException("Apple USB service omitted DeviceList.");

        List<AppleUsbDevice> devices = [];
        HashSet<uint> identifiers = [];
        foreach (var item in list.Elements())
        {
            var device = ReadDictionary(item);
            if (!device.TryGetValue("Properties", out var propertiesElement))
                throw new InvalidDataException("USB device omitted Properties.");
            var properties = ReadDictionary(propertiesElement);
            // Network-paired devices must never be reported as USB-connected.
            if (!properties.TryGetValue("ConnectionType", out var connection) ||
                connection.Name.LocalName != "string" || connection.Value != "USB") continue;
            var id = ReadUnsigned(device, "DeviceID");
            var product = ReadUnsigned(properties, "ProductID");
            if (id == 0 || product > ushort.MaxValue || !identifiers.Add(id))
                throw new InvalidDataException("Invalid or duplicate USB device identity.");
            devices.Add(new(id, (ushort)product));
        }
        return devices;
    }

    public async Task<UsbTunnel> OpenTunnelAsync(uint deviceId, ushort companionPort, CancellationToken cancellationToken = default)
    {
        if (deviceId == 0) throw new ArgumentOutOfRangeException(nameof(deviceId));
        if (companionPort == 0) throw new ArgumentOutOfRangeException(nameof(companionPort));
        using var deadline = CreateDeadline(cancellationToken);
        var socket = await OpenAsync(deadline.Token).ConfigureAwait(false);
        try
        {
            var reply = await RequestAsync(socket.GetStream(), "Connect", [
                new("DeviceID", new XElement("integer", deviceId)),
                new("PortNumber", new XElement("integer", BinaryPrimitives.ReverseEndianness(companionPort))),
            ], deadline.Token).ConfigureAwait(false);
            CheckResult(reply);
            return new UsbTunnel(socket);
        }
        catch
        {
            socket.Dispose();
            throw;
        }
    }

    private CancellationTokenSource CreateDeadline(CancellationToken cancellationToken)
    {
        var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancellationToken);
        deadline.CancelAfter(_timeout);
        return deadline;
    }

    private async Task<TcpClient> OpenAsync(CancellationToken cancellationToken)
    {
        var socket = new TcpClient(AddressFamily.InterNetwork) { NoDelay = true };
        try
        {
            await socket.ConnectAsync(IPAddress.Loopback, _servicePort, cancellationToken).ConfigureAwait(false);
            return socket;
        }
        catch { socket.Dispose(); throw; }
    }

    private static async Task<Dictionary<string, XElement>> RequestAsync(
        Stream stream, string messageType, KeyValuePair<string, XElement>[] extra, CancellationToken token)
    {
        var dictionary = new XElement("dict",
            new XElement("key", "MessageType"), new XElement("string", messageType),
            new XElement("key", "ClientVersionString"), new XElement("string", "UNKXWYNO Mirror 0.1.0"),
            new XElement("key", "ProgName"), new XElement("string", "UnkxwynoMirror"),
            new XElement("key", "kLibUSBMuxVersion"), new XElement("integer", 3));
        foreach (var pair in extra) dictionary.Add(new XElement("key", pair.Key), pair.Value);
        var payload = Encoding.UTF8.GetBytes(new XDocument(new XElement("plist", new XAttribute("version", "1.0"), dictionary)).ToString(SaveOptions.DisableFormatting));
        var header = new byte[16];
        BinaryPrimitives.WriteUInt32LittleEndian(header, (uint)(16 + payload.Length));
        BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(4), 1);
        BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(8), 8);
        BinaryPrimitives.WriteUInt32LittleEndian(header.AsSpan(12), 1);
        await stream.WriteAsync(header, token).ConfigureAwait(false);
        await stream.WriteAsync(payload, token).ConfigureAwait(false);

        await stream.ReadExactlyAsync(header, token).ConfigureAwait(false);
        var length = BinaryPrimitives.ReadUInt32LittleEndian(header);
        if (length < 16 || length > MaximumMessageBytes ||
            BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(4)) != 1 ||
            BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(8)) != 8 ||
            BinaryPrimitives.ReadUInt32LittleEndian(header.AsSpan(12)) != 1)
            throw new InvalidDataException("Invalid Apple USB reply header.");
        var response = new byte[(int)length - 16];
        await stream.ReadExactlyAsync(response, token).ConfigureAwait(false);
        using var memory = new MemoryStream(response, writable: false);
        using var reader = XmlReader.Create(memory, new XmlReaderSettings
        {
            DtdProcessing = DtdProcessing.Ignore,
            XmlResolver = null,
            MaxCharactersInDocument = MaximumMessageBytes,
        });
        var document = XDocument.Load(reader);
        if (document.Root?.Name.LocalName != "plist" || document.Root.Elements().Count() != 1)
            throw new InvalidDataException("Invalid Apple USB property list.");
        return ReadDictionary(document.Root.Elements().Single());
    }

    private static Dictionary<string, XElement> ReadDictionary(XElement element)
    {
        if (element.Name.LocalName != "dict") throw new InvalidDataException("Expected a USB property dictionary.");
        var children = element.Elements().ToArray();
        if (children.Length % 2 != 0) throw new InvalidDataException("Incomplete USB property dictionary.");
        Dictionary<string, XElement> result = new(StringComparer.Ordinal);
        for (var index = 0; index < children.Length; index += 2)
        {
            if (children[index].Name.LocalName != "key" || !result.TryAdd(children[index].Value, children[index + 1]))
                throw new InvalidDataException("Invalid or duplicate USB property key.");
        }
        return result;
    }

    private static uint ReadUnsigned(IReadOnlyDictionary<string, XElement> dictionary, string key)
    {
        if (!dictionary.TryGetValue(key, out var element) || element.Name.LocalName != "integer" ||
            !uint.TryParse(element.Value, System.Globalization.NumberStyles.None, System.Globalization.CultureInfo.InvariantCulture, out var value))
            throw new InvalidDataException($"Invalid USB property {key}.");
        return value;
    }

    private static void CheckResult(IReadOnlyDictionary<string, XElement> reply)
    {
        if (!reply.TryGetValue("MessageType", out var type) || type.Value != "Result")
            throw new InvalidDataException("Expected an Apple USB result.");
        var code = ReadUnsigned(reply, "Number");
        if (code != 0) throw new UsbmuxException(code);
    }
}

public sealed class UsbTunnel : IDisposable, IAsyncDisposable
{
    private readonly TcpClient _socket;
    internal UsbTunnel(TcpClient socket) { _socket = socket; }
    public Stream Stream => _socket.GetStream();
    public void Dispose() => _socket.Dispose();
    public ValueTask DisposeAsync() { Dispose(); return ValueTask.CompletedTask; }
}

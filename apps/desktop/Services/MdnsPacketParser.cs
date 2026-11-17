using System.Buffers.Binary;
using System.Text;

namespace UnkxwynoMirror.Desktop.Services;

public sealed record MdnsPointer(string InstanceName, uint TimeToLive);
public sealed record MdnsService(string InstanceName, string HostName, ushort Port, uint TimeToLive);
public sealed record MdnsServiceTarget(string HostName, ushort Port);
public sealed record MdnsPacket(IReadOnlyList<MdnsPointer> Pointers, IReadOnlyList<MdnsService> Services);

public static class MdnsPacketParser
{
    private const ushort PointerRecord = 12;
    private const ushort ServiceRecord = 33;
    private const int HeaderLength = 12;
    private const int MaximumNameJumps = 32;

    public static byte[] CreatePtrQuery(string serviceType, bool requestUnicastResponse)
    {
        var name = NormalizeName(serviceType);
        using var stream = new MemoryStream();
        stream.Write(new byte[HeaderLength]);
        stream.Position = 4;
        WriteUInt16(stream, 1);
        stream.Position = HeaderLength;
        WriteName(stream, name);
        WriteUInt16(stream, PointerRecord);
        WriteUInt16(stream, requestUnicastResponse ? (ushort)0x8001 : (ushort)1);
        return stream.ToArray();
    }

    public static MdnsPacket Parse(ReadOnlySpan<byte> packet, string expectedServiceType)
    {
        List<MdnsPointer> pointers = [];
        List<MdnsService> services = [];
        if (packet.Length < HeaderLength)
        {
            return new(pointers, services);
        }

        var questionCount = ReadUInt16(packet, 4);
        var recordCount = (uint)ReadUInt16(packet, 6) + ReadUInt16(packet, 8) + ReadUInt16(packet, 10);
        var offset = HeaderLength;

        for (var index = 0; index < questionCount; index++)
        {
            if (!TryReadName(packet, ref offset, out _) || !TrySkip(packet, ref offset, 4))
            {
                return new(pointers, services);
            }
        }

        var expected = NormalizeName(expectedServiceType);
        for (uint index = 0; index < recordCount; index++)
        {
            if (!TryReadName(packet, ref offset, out var owner) || !TryReadRecordHeader(packet, ref offset, out var type, out var ttl, out var dataLength))
            {
                break;
            }

            var dataOffset = offset;
            var dataEnd = dataOffset + dataLength;
            if (dataEnd < dataOffset || dataEnd > packet.Length)
            {
                break;
            }

            if (type == PointerRecord && NameEquals(owner, expected))
            {
                var pointerOffset = dataOffset;
                if (TryReadName(packet, ref pointerOffset, out var instance))
                {
                    pointers.Add(new(NormalizeName(instance), ttl));
                }
            }
            else if (type == ServiceRecord && dataLength >= 6)
            {
                var port = ReadUInt16(packet, dataOffset + 4);
                var targetOffset = dataOffset + 6;
                if (TryReadName(packet, ref targetOffset, out var host))
                {
                    services.Add(new(NormalizeName(owner), NormalizeName(host), port, ttl));
                }
            }

            offset = dataEnd;
        }
        return new(pointers, services);
    }

    public static string DisplayName(string instanceName, string serviceType)
    {
        var instance = NormalizeName(instanceName);
        var service = NormalizeName(serviceType);
        return instance.EndsWith(service, StringComparison.OrdinalIgnoreCase)
            ? instance[..^service.Length].TrimEnd('.')
            : instance.TrimEnd('.');
    }

    private static bool TryReadRecordHeader(ReadOnlySpan<byte> packet, ref int offset, out ushort type, out uint ttl, out int dataLength)
    {
        type = 0;
        ttl = 0;
        dataLength = 0;
        if (offset > packet.Length - 10)
        {
            return false;
        }
        type = ReadUInt16(packet, offset);
        ttl = BinaryPrimitives.ReadUInt32BigEndian(packet.Slice(offset + 4, 4));
        dataLength = ReadUInt16(packet, offset + 8);
        offset += 10;
        return true;
    }

    private static bool TryReadName(ReadOnlySpan<byte> packet, ref int offset, out string name)
    {
        name = string.Empty;
        if ((uint)offset >= (uint)packet.Length)
        {
            return false;
        }

        List<string> labels = [];
        var cursor = offset;
        var returnOffset = -1;
        var jumps = 0;
        while ((uint)cursor < (uint)packet.Length)
        {
            var length = packet[cursor++];
            if (length == 0)
            {
                offset = returnOffset >= 0 ? returnOffset : cursor;
                name = labels.Count == 0 ? "." : string.Join('.', labels) + ".";
                return true;
            }

            if ((length & 0xC0) == 0xC0)
            {
                if ((uint)cursor >= (uint)packet.Length || ++jumps > MaximumNameJumps)
                {
                    return false;
                }
                var pointer = ((length & 0x3F) << 8) | packet[cursor++];
                if ((uint)pointer >= (uint)packet.Length)
                {
                    return false;
                }
                returnOffset = returnOffset >= 0 ? returnOffset : cursor;
                cursor = pointer;
                continue;
            }

            if ((length & 0xC0) != 0 || length > 63 || cursor > packet.Length - length)
            {
                return false;
            }
            labels.Add(Encoding.UTF8.GetString(packet.Slice(cursor, length)));
            cursor += length;
        }
        return false;
    }

    private static void WriteName(Stream stream, string name)
    {
        foreach (var label in name.TrimEnd('.').Split('.'))
        {
            var bytes = Encoding.UTF8.GetBytes(label);
            if (bytes.Length is 0 or > 63)
            {
                throw new ArgumentException("A DNS label must contain between 1 and 63 UTF-8 bytes.", nameof(name));
            }
            stream.WriteByte((byte)bytes.Length);
            stream.Write(bytes);
        }
        stream.WriteByte(0);
    }

    private static bool TrySkip(ReadOnlySpan<byte> packet, ref int offset, int count)
    {
        if (count < 0 || offset > packet.Length - count)
        {
            return false;
        }
        offset += count;
        return true;
    }

    private static ushort ReadUInt16(ReadOnlySpan<byte> packet, int offset) =>
        BinaryPrimitives.ReadUInt16BigEndian(packet.Slice(offset, 2));

    private static void WriteUInt16(Stream stream, ushort value)
    {
        Span<byte> buffer = stackalloc byte[2];
        BinaryPrimitives.WriteUInt16BigEndian(buffer, value);
        stream.Write(buffer);
    }

    private static string NormalizeName(string value)
    {
        var trimmed = value.Trim();
        return trimmed.EndsWith('.') ? trimmed : trimmed + ".";
    }

    private static bool NameEquals(string left, string right) =>
        string.Equals(NormalizeName(left), NormalizeName(right), StringComparison.OrdinalIgnoreCase);
}

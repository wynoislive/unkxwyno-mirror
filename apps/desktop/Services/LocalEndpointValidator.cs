using System.Net;
using System.Net.Sockets;

namespace UnkxwynoMirror.Desktop.Services;

public static class LocalEndpointValidator
{
    public static bool TryValidate(string input, out string reason)
    {
        reason = string.Empty;
        var candidate = input.Trim();
        if (candidate.Length == 0 ||
            !Uri.TryCreate($"tcp://{candidate}", UriKind.Absolute, out var uri) ||
            uri.Port is <= 0 or > 65535 ||
            !string.IsNullOrEmpty(uri.UserInfo) ||
            uri.AbsolutePath != "/" ||
            !string.IsNullOrEmpty(uri.Query) ||
            !string.IsNullOrEmpty(uri.Fragment))
        {
            reason = "Enter a host and port, for example 192.168.1.20:49791.";
            return false;
        }

        var host = uri.Host.Trim('[', ']', '.');
        if (IPAddress.TryParse(host, out var address))
        {
            if (IsLocalAddress(address))
            {
                return true;
            }

            reason = "Only private, loopback, link-local, or unique-local addresses are accepted.";
            return false;
        }

        if (host.Equals("localhost", StringComparison.OrdinalIgnoreCase) ||
            host.EndsWith(".local", StringComparison.OrdinalIgnoreCase))
        {
            return true;
        }

        reason = "Host names must be localhost or end in .local; public Internet names are not accepted.";
        return false;
    }

    private static bool IsLocalAddress(IPAddress address)
    {
        if (address.IsIPv4MappedToIPv6)
        {
            address = address.MapToIPv4();
        }

        if (IPAddress.IsLoopback(address))
        {
            return true;
        }

        if (address.AddressFamily == AddressFamily.InterNetworkV6)
        {
            if (address.IsIPv6LinkLocal)
            {
                return true;
            }

            var bytes = address.GetAddressBytes();
            return bytes.Length == 16 && (bytes[0] & 0xFE) == 0xFC;
        }

        var octets = address.GetAddressBytes();
        return octets.Length == 4 &&
            (octets[0] == 10 ||
             (octets[0] == 172 && octets[1] is >= 16 and <= 31) ||
             (octets[0] == 192 && octets[1] == 168) ||
             (octets[0] == 169 && octets[1] == 254));
    }
}

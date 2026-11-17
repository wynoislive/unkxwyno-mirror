using System.Buffers.Binary;
using System.Net;
using System.Net.Sockets;
using System.Net.Security;
using System.Security.Authentication;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Xml;
using System.Xml.Linq;
using UnkxwynoMirror.Desktop.Services.Usb;

static XElement? Value(XElement dict, string name)
{
    var entries = dict.Elements().ToArray();
    for (var i = 0; i + 1 < entries.Length; i += 2) if (entries[i].Value == name) return entries[i + 1];
    return null;
}
static async Task<XElement> Read(Stream stream, CancellationToken cancel)
{
    var prefix = new byte[4]; await stream.ReadExactlyAsync(prefix, cancel);
    var size = BinaryPrimitives.ReadUInt32BigEndian(prefix); if (size > 1024 * 1024) throw new IOException("Mock control frame too large.");
    var body = new byte[(int)size]; await stream.ReadExactlyAsync(body, cancel);
    var xmlText = Encoding.UTF8.GetString(body);
    Check(xmlText.StartsWith("<?xml", StringComparison.Ordinal) && xmlText.Contains("<!DOCTYPE plist", StringComparison.Ordinal), "Control requests must include the standard plist declaration and document type used by the working read-only handshake.");
    using var reader = XmlReader.Create(new MemoryStream(body), new XmlReaderSettings { DtdProcessing = DtdProcessing.Ignore, XmlResolver = null });
    return XDocument.Load(reader).Root!.Element("dict")!;
}
static async Task Reply(Stream stream, CancellationToken cancel, params XElement[] fields)
{
    var body = Encoding.UTF8.GetBytes(new XElement("plist", new XElement("dict", fields)).ToString(SaveOptions.DisableFormatting));
    var prefix = new byte[4]; BinaryPrimitives.WriteUInt32BigEndian(prefix, (uint)body.Length);
    // Fragment both framing and XML to exercise real stream reads.
    foreach (var b in prefix) await stream.WriteAsync(new[] { b }, cancel);
    for (var p = 0; p < body.Length; p += 37) await stream.WriteAsync(body.AsMemory(p, Math.Min(37, body.Length - p)), cancel);
}
static void Check(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
static async Task PairingCase(bool wrongServerKey)
{
    using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(20)); var cancel = deadline.Token;
    var directory = Path.Combine(Path.GetTempPath(), "unkxwyno-pairing-test-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(directory);
    using var deviceKey = RSA.Create(2048);
    using var listener = new TcpListener(IPAddress.Loopback, 0); listener.Start();
    var endpoint = (IPEndPoint)listener.LocalEndpoint;
    var paired = false; var tlsReached = false;
    var server = Task.Run(async () =>
    {
        using var accepted = await listener.AcceptTcpClientAsync(cancel); var stream = accepted.GetStream();
        X509Certificate2? deviceCertificate = null, hostCertificate = null;
        try
        {
            while (true)
            {
                var request = await Read(stream, cancel); var verb = Value(request, "Request")!.Value;
                if (verb == "GetValue")
                {
                    Check(Value(request, "Key")!.Value == "DevicePublicKey", "Pairing must request only the public device key.");
                    await Reply(stream, cancel, new("key", "Value"), new("data", Convert.ToBase64String(Encoding.UTF8.GetBytes(deviceKey.ExportRSAPublicKeyPem()))));
                }
                else if (verb == "Pair")
                {
                    var record = Value(request, "PairRecord")!;
                    foreach (var name in new[] { "DeviceCertificate", "HostCertificate", "RootCertificate" })
                        Check(Encoding.UTF8.GetString(Convert.FromBase64String(Value(record,name)!.Value)).EndsWith("-----END CERTIFICATE-----\n",StringComparison.Ordinal), "Pairing certificates must preserve the reference PEM final newline.");
                    using (var hostShape = X509Certificate2.CreateFromPem(Encoding.UTF8.GetString(Convert.FromBase64String(Value(record,"HostCertificate")!.Value))))
                        Check(hostShape.Extensions.OfType<X509KeyUsageExtension>().Any(e => e.Critical && e.KeyUsages == (X509KeyUsageFlags.DigitalSignature | X509KeyUsageFlags.KeyEncipherment)), "Pairing host certificate must carry the reference's critical signing/encipherment usage.");
                    using (var rootShape = X509Certificate2.CreateFromPem(Encoding.UTF8.GetString(Convert.FromBase64String(Value(record, "RootCertificate")!.Value))))
                    {
                        Check(rootShape.SubjectName.RawData.SequenceEqual(new byte[] { 0x30, 0 }) && rootShape.GetSerialNumber().All(b => b == 0),
                            "The pairing root must retain the reference protocol's empty distinguished name and zero serial.");
                    }
                    Check(Value(record, "HostPrivateKey") is null && Value(record, "RootPrivateKey") is null, "No private key may be sent in a pairing request.");
                    Check(Guid.TryParse(Value(record, "HostID")!.Value, out _), "Pairing requires this app's host identity.");
                    deviceCertificate?.Dispose(); hostCertificate?.Dispose();
                    var pem = Encoding.UTF8.GetString(Convert.FromBase64String(Value(record, "DeviceCertificate")!.Value));
                    using var publicCert = X509Certificate2.CreateFromPem(pem);
                    Check(publicCert.Extensions.OfType<X509SubjectKeyIdentifierExtension>().Any(), "The device pairing certificate must include the reference protocol's subject-key identifier.");
                    using var signedDevice = publicCert.CopyWithPrivateKey(deviceKey);
                    deviceCertificate = X509CertificateLoader.LoadPkcs12(signedDevice.Export(X509ContentType.Pfx), null, X509KeyStorageFlags.UserKeySet);
                    // lockdownd authenticates the pairing root identity, rather
                    // than a generic host leaf certificate (upstream idevice.c).
                    hostCertificate = X509Certificate2.CreateFromPem(Encoding.UTF8.GetString(Convert.FromBase64String(Value(record, "RootCertificate")!.Value)));
                    paired = true; await Reply(stream, cancel, new("key", "Request"), new("string", "Pair"));
                }
                else if (verb == "StartSession")
                {
                    Check(paired, "A newly generated host identity must pair before requesting StartSession.");
                    await Reply(stream, cancel, new("key", "SessionID"), new("string", "mock-session"), new("key", "EnableSessionSSL"), new("true"));
                    using var wrongKey = RSA.Create(2048);
                    var wrongRequest = new CertificateRequest("CN=Wrong USB peer", wrongKey, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
                    using var wrongCert = wrongRequest.CreateSelfSigned(DateTimeOffset.UtcNow.AddHours(-1), DateTimeOffset.UtcNow.AddDays(1));
                    using var wrongTlsCert = X509CertificateLoader.LoadPkcs12(wrongCert.Export(X509ContentType.Pfx), null, X509KeyStorageFlags.UserKeySet);
                    using var ssl = new SslStream(stream, true, (_, certificate, _, _) =>
                        certificate is not null && hostCertificate is not null && CryptographicOperations.FixedTimeEquals(SHA256.HashData(certificate.GetRawCertData()), SHA256.HashData(hostCertificate.RawData)));
                    try
                    {
                        await ssl.AuthenticateAsServerAsync(new SslServerAuthenticationOptions { ServerCertificate = wrongServerKey ? wrongTlsCert : deviceCertificate,
                            ClientCertificateRequired = true, EnabledSslProtocols = SslProtocols.Tls12 | SslProtocols.Tls13, CertificateRevocationCheckMode = X509RevocationMode.NoCheck }, cancel);
                        tlsReached = true; var b = new byte[1]; _ = await ssl.ReadAsync(b, cancel);
                    }
                    catch (Exception e) when ((wrongServerKey || tlsReached) && e is AuthenticationException or IOException) { }
                    return;
                }
                else throw new IOException("Unexpected mock request.");
            }
        }
        catch (Exception e) { Console.Error.WriteLine("Mock peer failure: " + e.GetType().Name + ": " + e.Message); throw; }
        finally { deviceCertificate?.Dispose(); hostCertificate?.Dispose(); }
    }, cancel);
    try
    {
        using var client = new TcpClient(); await client.ConnectAsync(IPAddress.Loopback, endpoint.Port, cancel);
        await using (var session = new UsbTrustedSession(client.GetStream(), _ => { }, directory))
        {
            var rejected = false;
            try { await session.AuthenticateAsync(cancel); }
            catch (AuthenticationException e) when (wrongServerKey && e.Message == "The encrypted USB peer does not match the paired iPhone public key.") { rejected = true; }
            Check(rejected == wrongServerKey, "TLS must accept the pinned device and reject a different device key.");
        }
        await server;
        Check(paired, "The normal Pair request must complete before StartSession.");
        Check(tlsReached, "Both fixtures must reach mutual TLS before testing the device-key pin; an unrelated connection failure is not a rejection pass.");
        var identityFile = Directory.GetFiles(directory, "*.dpapi").Single(); var stored = File.ReadAllBytes(identityFile);
        Check(stored.Length > 128 && !Encoding.UTF8.GetString(stored).Contains("PRIVATE KEY", StringComparison.Ordinal), "The generated identity must be protected at rest.");
    }
    finally
    {
        deadline.Cancel(); listener.Stop();
        // This UUID directory was created by this test and contains only its own identities.
        var absolute = Path.GetFullPath(directory);
        if (absolute.StartsWith(Path.GetFullPath(Path.GetTempPath()), StringComparison.OrdinalIgnoreCase) && Path.GetFileName(absolute).StartsWith("unkxwyno-pairing-test-", StringComparison.Ordinal)) Directory.Delete(absolute, true);
    }
}
static async Task ResumeAfterApprovalReset()
{
    using var timeout = new CancellationTokenSource(TimeSpan.FromSeconds(20)); var cancel = timeout.Token;
    var directory = Path.Combine(Path.GetTempPath(), "unkxwyno-pairing-test-" + Guid.NewGuid().ToString("N")); Directory.CreateDirectory(directory);
    using var deviceKey = RSA.Create(2048);
    using var listener = new TcpListener(IPAddress.Loopback, 0); listener.Start(); var endpoint = (IPEndPoint)listener.LocalEndpoint;
    string? approvedHost = null; var pairRequests = 0;
    var server = Task.Run(async () =>
    {
        for (var connection = 0; connection < 2; connection++)
        {
            using var peer = await listener.AcceptTcpClientAsync(cancel); var stream = peer.GetStream();
            var publicKey = await Read(stream, cancel); Check(Value(publicKey, "Key")?.Value == "DevicePublicKey", "Reconnect must bind to the same public device key.");
            await Reply(stream, cancel, new("key", "Value"), new("data", Convert.ToBase64String(Encoding.UTF8.GetBytes(deviceKey.ExportRSAPublicKeyPem()))));
            var request = await Read(stream, cancel);
            if (connection == 0)
            {
                Check(Value(request, "Request")?.Value == "Pair", "Fresh identity must request Pair."); pairRequests++;
                approvedHost = Value(Value(request, "PairRecord")!, "HostID")!.Value;
                await Reply(stream, cancel, new("key", "Error"), new("string", "PairingDialogResponsePending"));
                // Simulate approval followed by the control socket closing before
                // the final successful Pair response reaches the host.
            }
            else
            {
                Check(Value(request, "Request")?.Value == "StartSession" && Value(request, "HostID")?.Value == approvedHost,
                    "A reconnect must try the identity already approved by the device, without another Pair request.");
                await Reply(stream, cancel, new("key", "SessionID"), new("string", "resumed-session"), new("key", "EnableSessionSSL"), new("false"));
            }
        }
    }, cancel);
    try
    {
        using (var first = new TcpClient())
        {
            await first.ConnectAsync(endpoint.Address, endpoint.Port, cancel);
            await using var session = new UsbTrustedSession(first.GetStream(), _ => { }, directory);
            var disconnected = false;
            try { await session.AuthenticateAsync(cancel); } catch (IOException) { disconnected = true; }
            Check(disconnected, "The first control reset must be surfaced, not reported as authentication success.");
        }
        using (var second = new TcpClient())
        {
            await second.ConnectAsync(endpoint.Address, endpoint.Port, cancel);
            await using var session = new UsbTrustedSession(second.GetStream(), _ => { }, directory); await session.AuthenticateAsync(cancel);
        }
        await server; Check(pairRequests == 1, "Approval recovery must not create another host identity or repeat Pair.");
    }
    finally
    {
        timeout.Cancel(); listener.Stop(); var absolute = Path.GetFullPath(directory);
        if (absolute.StartsWith(Path.GetFullPath(Path.GetTempPath()), StringComparison.OrdinalIgnoreCase) && Path.GetFileName(absolute).StartsWith("unkxwyno-pairing-test-", StringComparison.Ordinal)) Directory.Delete(absolute, true);
    }
}
await PairingCase(false);
await PairingCase(true);
await ResumeAfterApprovalReset();
Console.WriteLine("Trusted USB session tests passed: normal pairing, approval/reset recovery, mutual TLS, pinned-device rejection, fragmented replies and protected local identity. No hardware claim.");

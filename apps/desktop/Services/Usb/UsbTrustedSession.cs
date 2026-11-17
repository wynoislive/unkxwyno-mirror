using System.Buffers.Binary;
using System.Net.Security;
using System.Runtime.InteropServices;
using System.Security.Authentication;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.Json;
using System.Xml;
using System.Xml.Linq;

namespace UnkxwynoMirror.Desktop.Services.Usb;

/// <summary>Pairs this application normally and honors lockdownd's TLS requirement.
/// Only this app's newly generated identity is persisted, protected with user DPAPI.
/// No Apple/iTunes pair record, device private key, or existing credential is read.</summary>
public sealed class UsbTrustedSession : IAsyncDisposable
{
    private readonly Stream _transport;
    private Stream _stream;
    private readonly Action<string> _progress;
    private readonly string? _identityDirectory;
    private readonly Action<string>? _diagnostics;
    private Identity? _identity;
    private string? _sessionId;
    private sealed record StoredIdentity(string HostId, string SystemBuid, string RootPfx, string HostPfx, bool Paired = false, int DeviceCertificateFormat = 1);
    private sealed class Identity(StoredIdentity stored, X509Certificate2 root, X509Certificate2 host, string path, bool loadedFromDisk) : IDisposable
    {
        public StoredIdentity Stored { get; set; } = stored;
        public string Path { get; } = path;
        public bool LoadedFromDisk { get; } = loadedFromDisk;
        public X509Certificate2 Root { get; } = root;
        public X509Certificate2 Host { get; } = host;
        public void Dispose() { Root.Dispose(); Host.Dispose(); }
    }
    public UsbTrustedSession(Stream transport, Action<string> progress, string? identityDirectory = null, Action<string>? diagnostics = null)
    { _transport = transport; _stream = transport; _progress = progress; _identityDirectory = identityDirectory; _diagnostics=diagnostics; }

    public async Task AuthenticateAsync(CancellationToken cancellation, bool refreshPairRecord = false)
    {
        var keyReply = await RequestAsync("GetValue", [new XElement("key", "Key"), new XElement("string", "DevicePublicKey")], cancellation);
        CheckError(keyReply);
        var publicBytes = Convert.FromBase64String(Value(keyReply, "Value")?.Value ?? throw new IOException("The iPhone did not provide its public pairing key."));
        using var deviceKey = RSA.Create();
        deviceKey.ImportFromPem(Encoding.UTF8.GetString(publicBytes));
        _progress("The iPhone's public pairing identity was received.");
        var publicKey = deviceKey.ExportSubjectPublicKeyInfo();
        _identity = LoadOrCreateIdentity(publicKey, _identityDirectory);
        // Approval may have succeeded on the device just before its socket was
        // reset. A saved app-owned identity can safely attempt StartSession;
        // success and certificate verification, not the file, establish trust.
        var canResume = !refreshPairRecord && _identity.LoadedFromDisk && _identity.Stored.DeviceCertificateFormat >= 3;
        if (canResume) _progress("Opening a device session with this app's saved identity.");
        XElement? start = canResume ? await StartAsync(cancellation) : null;
        if (start is null || Value(start, "Error") is not null)
        {
            var error = start is null ? null : Value(start, "Error")?.Value;
            if (error is not null && error is not ("InvalidHostID" or "MissingPairRecord" or "InvalidPairRecord" or "InvalidPairRecordError")) CheckError(start!);
            _progress("Registering this app's pairing record. Approve Trust on the unlocked iPhone if prompted.");
            var issuer = _identity.Root;
            var deviceRequest = new CertificateRequest(new X500DistinguishedName(new byte[] { 0x30, 0 }), deviceKey, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
            deviceRequest.CertificateExtensions.Add(new X509BasicConstraintsExtension(false, false, 0, true));
            deviceRequest.CertificateExtensions.Add(new X509KeyUsageExtension(X509KeyUsageFlags.DigitalSignature | X509KeyUsageFlags.KeyEncipherment, true));
            deviceRequest.CertificateExtensions.Add(new X509SubjectKeyIdentifierExtension(deviceRequest.PublicKey, false));
            using var deviceCertificate = deviceRequest.Create(issuer, issuer.NotBefore.ToUniversalTime(), issuer.NotAfter.ToUniversalTime(), new byte[] { 0 });
            // Preserve this app's existing host key and identity when refreshing
            // its public record. No new private identity or vendor record is used.
            using var hostKey = _identity.Host.GetRSAPublicKey() ?? throw new IOException("The app's host certificate has no RSA public key.");
            var hostRequest = new CertificateRequest(new X500DistinguishedName(new byte[] { 0x30, 0 }), hostKey, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
            hostRequest.CertificateExtensions.Add(new X509BasicConstraintsExtension(false, false, 0, true));
            hostRequest.CertificateExtensions.Add(new X509KeyUsageExtension(X509KeyUsageFlags.DigitalSignature | X509KeyUsageFlags.KeyEncipherment, true));
            using var hostCertificate = hostRequest.Create(issuer, issuer.NotBefore.ToUniversalTime(), issuer.NotAfter.ToUniversalTime(), new byte[] { 0 });
            XElement Data(string name, X509Certificate2 cert) => new("dict", new XElement("key", name), new XElement("data", Convert.ToBase64String(Encoding.UTF8.GetBytes(cert.ExportCertificatePem()+"\n"))));
            var record = new XElement("dict",
                new XElement("key", "HostID"), new XElement("string", _identity.Stored.HostId),
                new XElement("key", "SystemBUID"), new XElement("string", _identity.Stored.SystemBuid));
            foreach (var pair in new[] { Data("DeviceCertificate", deviceCertificate), Data("HostCertificate", hostCertificate), Data("RootCertificate", issuer) })
                record.Add(pair.Elements().ToArray());
            for (var attempt = 0; ; attempt++)
            {
                var paired = await RequestAsync("Pair", [new XElement("key", "PairRecord"), new XElement(record),
                    new XElement("key", "ProtocolVersion"), new XElement("string", "2"),
                    new XElement("key", "PairingOptions"), new XElement("dict", new XElement("key", "ExtendedPairingErrors"), new XElement("true"))], cancellation);
                var pairError = Value(paired, "Error")?.Value;
                if (pairError is null) break;
                if (pairError != "PairingDialogResponsePending" || attempt >= 20) CheckError(paired);
                if (_identity.Stored.DeviceCertificateFormat < 3)
                {
                    _identity.Stored = _identity.Stored with { DeviceCertificateFormat = 3 };
                    SaveIdentity(_identity.Path, _identity.Stored, overwrite: true);
                }
                await Task.Delay(2000, cancellation);
            }
            _identity.Stored = _identity.Stored with { Paired = true, DeviceCertificateFormat = 3 };
            SaveIdentity(_identity.Path, _identity.Stored, overwrite: true);
            start = await StartAsync(cancellation);
        }
        CheckError(start);
        _sessionId = Value(start, "SessionID")?.Value ?? throw new IOException("The iPhone did not establish a trusted session.");
        var sslFlag = Value(start, "EnableSessionSSL");
        if (sslFlag is null || sslFlag.Name.LocalName is not ("true" or "false")) throw new IOException("The iPhone did not specify its trusted-session TLS policy.");
        if (sslFlag.Name.LocalName == "true")
        {
            _progress("The iPhone accepted StartSession and requested an encrypted connection.");
            var ssl = new OpenSslUsbStream(_transport, _identity.Root, publicKey, _diagnostics);
            _stream = ssl;
            await ssl.AuthenticateAsync(cancellation);
            _progress("The paired iPhone's encrypted connection is verified (" + ssl.NegotiatedProtocol + ").");
        }
        _progress("This app's trusted iPhone session is established.");
        if (!_identity.Stored.Paired) { _identity.Stored = _identity.Stored with { Paired = true }; SaveIdentity(_identity.Path, _identity.Stored, overwrite: true); }
    }

    public async Task MonitorAsync(CancellationToken cancellation)
    {
        var single = new byte[1];
        var count = await _stream.ReadAsync(single, cancellation);
        throw new IOException(count == 0 ? "The iPhone closed its trusted USB session." : "The iPhone sent an unexpected trusted-session message.");
    }

    private Task<XElement> StartAsync(CancellationToken cancellation) => RequestAsync("StartSession",
        [new XElement("key", "HostID"), new XElement("string", _identity!.Stored.HostId),
         new XElement("key", "SystemBUID"), new XElement("string", _identity.Stored.SystemBuid)], cancellation);

    private async Task<XElement> RequestAsync(string request, XElement[] fields, CancellationToken cancellation)
    {
        var dict = new XElement("dict", new XElement("key", "Label"), new XElement("string", "iOSScreenCaptureAssistant"),
            new XElement("key", "Request"), new XElement("string", request), fields);
        var document = new XDocument(new XDeclaration("1.0", "UTF-8", null),
            new XDocumentType("plist", "-//Apple//DTD PLIST 1.0//EN", "http://www.apple.com/DTDs/PropertyList-1.0.dtd", null),
            new XElement("plist", new XAttribute("version", "1.0"), dict));
        using var encoded = new MemoryStream();
        using (var writer = XmlWriter.Create(encoded, new XmlWriterSettings { Encoding = new UTF8Encoding(false), Indent = false, CloseOutput = false })) document.Save(writer);
        var payload = encoded.ToArray();
        var frame = new byte[payload.Length + 4]; BinaryPrimitives.WriteUInt32BigEndian(frame, checked((uint)payload.Length)); payload.CopyTo(frame, 4);
        await _stream.WriteAsync(frame, cancellation); await _stream.FlushAsync(cancellation);
        var length = new byte[4];
        await _stream.ReadExactlyAsync(length, cancellation);
        var size = BinaryPrimitives.ReadUInt32BigEndian(length);
        if (size is 0 or > 1024 * 1024) throw new IOException("The iPhone control reply exceeds the allowed size.");
        var response = new byte[checked((int)size)]; await _stream.ReadExactlyAsync(response, cancellation);
        using var reader = XmlReader.Create(new MemoryStream(response), new XmlReaderSettings { DtdProcessing = DtdProcessing.Ignore, XmlResolver = null, MaxCharactersInDocument = 1024 * 1024 });
        return XDocument.Load(reader).Root?.Element("dict") ?? throw new IOException("The iPhone returned an invalid control reply.");
    }

    private static XElement? Value(XElement dict, string key)
    {
        var entries = dict.Elements().ToArray();
        for (var i = 0; i + 1 < entries.Length; i += 2) if (entries[i].Name.LocalName == "key" && entries[i].Value == key) return entries[i + 1];
        return null;
    }
    private static void CheckError(XElement reply)
    {
        var error = Value(reply, "Error")?.Value;
        if (error is null) return;
        throw new IOException(error switch
        {
            "PasswordProtected" or "DeviceLocked" => "Unlock the iPhone before establishing its trusted USB session.",
            "UserDeniedPairing" => "The iPhone declined pairing. Approve Trust on the phone to use USB capture.",
            "PairingDialogResponsePending" => "The iPhone is still waiting for its Trust prompt to be approved.",
            _ => "The iPhone rejected trusted USB session initialization (" + new string(error.Where(c => char.IsAsciiLetterOrDigit(c)).Take(64).ToArray()) + ").",
        });
    }

    private static Identity LoadOrCreateIdentity(byte[] publicKey, string? identityDirectory)
    {
        var directory = identityDirectory ?? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UNKXWYNO Mirror", "usb-identities");
        Directory.CreateDirectory(directory);
        // Preserve earlier identities whose root was restricted to signing CAs.
        // Version 3 also preserves the reference's empty X.509 names/zero serial.
        var path = Path.Combine(directory, Convert.ToHexString(SHA256.HashData(publicKey)) + "-v3.dpapi");
        StoredIdentity stored;
        var loadedFromDisk = File.Exists(path);
        if (loadedFromDisk)
        {
            if (new FileInfo(path).Length > 128 * 1024) throw new IOException("This app's local USB identity exceeds its size limit.");
            var file = File.ReadAllBytes(path);
            if (file.Length > 128 * 1024) throw new IOException("This app's local USB identity exceeds its size limit.");
            var clear = Protect(file, decrypt: true);
            try { stored = JsonSerializer.Deserialize<StoredIdentity>(clear) ?? throw new IOException("This app's USB identity is invalid."); }
            finally { CryptographicOperations.ZeroMemory(clear); }
        }
        else
        {
            using var rootKey = RSA.Create(2048); using var hostKey = RSA.Create(2048);
            var emptyName = new X500DistinguishedName(new byte[] { 0x30, 0 });
            var rootRequest = new CertificateRequest(emptyName, rootKey, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
            rootRequest.CertificateExtensions.Add(new X509BasicConstraintsExtension(true, false, 0, true));
            using var rootPublic = rootRequest.Create(emptyName, X509SignatureGenerator.CreateForRSA(rootKey, RSASignaturePadding.Pkcs1), DateTimeOffset.UtcNow.AddHours(-1), DateTimeOffset.UtcNow.AddYears(10), new byte[] { 0 });
            using var root = rootPublic.CopyWithPrivateKey(rootKey);
            var hostRequest = new CertificateRequest(emptyName, hostKey, HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);
            hostRequest.CertificateExtensions.Add(new X509BasicConstraintsExtension(false, false, 0, true));
            using var hostPublic = hostRequest.Create(root, root.NotBefore.ToUniversalTime(), root.NotAfter.ToUniversalTime(), new byte[] { 0 });
            using var host = hostPublic.CopyWithPrivateKey(hostKey);
            stored = new(Guid.NewGuid().ToString("D").ToUpperInvariant(), Guid.NewGuid().ToString("D").ToUpperInvariant(),
                Convert.ToBase64String(root.Export(X509ContentType.Pfx)), Convert.ToBase64String(host.Export(X509ContentType.Pfx)));
            SaveIdentity(path, stored, overwrite: false);
        }
        return new(stored, X509CertificateLoader.LoadPkcs12(Convert.FromBase64String(stored.RootPfx), null, X509KeyStorageFlags.UserKeySet | X509KeyStorageFlags.Exportable),
            // Windows Schannel requires a user key container for the TLS client
            // certificate. The app-owned PFX remains DPAPI-protected at rest.
            X509CertificateLoader.LoadPkcs12(Convert.FromBase64String(stored.HostPfx), null, X509KeyStorageFlags.UserKeySet), path, loadedFromDisk);
    }
    private static void SaveIdentity(string path, StoredIdentity stored, bool overwrite)
    {
        var clear = JsonSerializer.SerializeToUtf8Bytes(stored); var temporary = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
        try { File.WriteAllBytes(temporary, Protect(clear, decrypt: false)); File.Move(temporary, path, overwrite); }
        finally { CryptographicOperations.ZeroMemory(clear); if (File.Exists(temporary)) File.Delete(temporary); }
    }
    [StructLayout(LayoutKind.Sequential)] private struct Blob { public int Size; public nint Data; }
    [DllImport("crypt32", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CryptProtectData(ref Blob data, nint description, nint entropy, nint reserved, nint prompt, uint flags, out Blob output);
    [DllImport("crypt32", SetLastError = true)] [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool CryptUnprotectData(ref Blob data, nint description, nint entropy, nint reserved, nint prompt, uint flags, out Blob output);
    [DllImport("kernel32")] private static extern nint LocalFree(nint memory);
    private static byte[] Protect(byte[] bytes, bool decrypt)
    {
        var input = new Blob { Size = bytes.Length, Data = Marshal.AllocHGlobal(bytes.Length) }; Blob output = default;
        try
        {
            Marshal.Copy(bytes, 0, input.Data, bytes.Length);
            var ok = decrypt ? CryptUnprotectData(ref input, 0, 0, 0, 0, 1, out output) : CryptProtectData(ref input, 0, 0, 0, 0, 1, out output);
            if (!ok) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(), "Windows could not protect this app's USB identity.");
            var result = new byte[output.Size]; Marshal.Copy(output.Data, result, 0, result.Length); return result;
        }
        finally
        {
            Marshal.Copy(new byte[bytes.Length], 0, input.Data, bytes.Length); Marshal.FreeHGlobal(input.Data);
            if (output.Data != 0) { if (decrypt) Marshal.Copy(new byte[output.Size], 0, output.Data, output.Size); LocalFree(output.Data); }
        }
    }
    public async ValueTask DisposeAsync()
    {
        // A canceled/disconnected capture may already have closed the native pipe.
        try { if (!ReferenceEquals(_stream, _transport)) await _stream.DisposeAsync(); } catch (IOException) { }
        _identity?.Dispose(); _transport.Dispose();
    }
}

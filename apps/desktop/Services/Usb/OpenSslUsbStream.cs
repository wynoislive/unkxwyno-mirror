using System.Runtime.InteropServices;
using System.Security.Authentication;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text;

namespace UnkxwynoMirror.Desktop.Services.Usb;

/// <summary>OpenSSL over the owned USBMux byte stream, using memory BIOs.
/// Keys belong to this app, stay in memory, and are never sent to the peer.
/// Peer identity is pinned before any encrypted application data is accepted.</summary>
public sealed class OpenSslUsbStream : Stream
{
    private readonly Stream _transport;
    private readonly byte[] _peerKeyHash;
    private readonly Action<string>? _diagnostics;
    private nint _context, _ssl, _readBio, _writeBio;
    private bool _authenticated, _disposed, _biosAttached;
    public string NegotiatedProtocol => Marshal.PtrToStringUTF8(Api.SSL_get_version(_ssl)) ?? "TLS";
    public OpenSslUsbStream(Stream transport, X509Certificate2 identity, byte[] devicePublicKey, Action<string>? diagnostics = null)
    {
        _transport=transport;_peerKeyHash=SHA256.HashData(devicePublicKey);_diagnostics=diagnostics;
        try
        {
            _context=Api.SSL_CTX_new(Api.TLS_client_method());Require(_context!=0,"Create USB TLS context");
            // Negotiate TLS 1.2 or newer without assuming a device's maximum.
            // No system policy, security level or peer-key pin is relaxed.
            Require(Api.SSL_CTX_ctrl(_context,123,0x0303,0)!=0 && Api.SSL_CTX_ctrl(_context,124,0,0)!=0,"Configure USB TLS version");
            var certificate=Encoding.ASCII.GetBytes(identity.ExportCertificatePem());
            using var rsa=identity.GetRSAPrivateKey() ?? throw new AuthenticationException("The app's pairing root has no private credential.");
            var key=Encoding.ASCII.GetBytes(rsa.ExportPkcs8PrivateKeyPem());
            var certificatePin=GCHandle.Alloc(certificate,GCHandleType.Pinned);var keyPin=GCHandle.Alloc(key,GCHandleType.Pinned);
            nint certBio=0,keyBio=0,cert=0,privateKey=0;
            try
            {
                certBio=Api.BIO_new_mem_buf(certificatePin.AddrOfPinnedObject(),certificate.Length);keyBio=Api.BIO_new_mem_buf(keyPin.AddrOfPinnedObject(),key.Length);
                Require(certBio!=0 && keyBio!=0,"Read app-owned TLS identity");
                cert=Api.PEM_read_bio_X509(certBio,0,0,0);privateKey=Api.PEM_read_bio_PrivateKey(keyBio,0,0,0);
                Require(cert!=0 && privateKey!=0,"Decode app-owned TLS identity");
                Require(Api.SSL_CTX_use_certificate(_context,cert)==1 && Api.SSL_CTX_use_PrivateKey(_context,privateKey)==1 && Api.SSL_CTX_check_private_key(_context)==1,"Configure pairing-root TLS client identity");
            }
            finally
            {
                if(cert!=0)Api.X509_free(cert);if(privateKey!=0)Api.EVP_PKEY_free(privateKey);
                if(certBio!=0)Api.BIO_free(certBio);if(keyBio!=0)Api.BIO_free(keyBio);CryptographicOperations.ZeroMemory(key);certificatePin.Free();keyPin.Free();
            }
            _ssl=Api.SSL_new(_context);Require(_ssl!=0,"Create USB TLS session");
            _readBio=Api.BIO_new(Api.BIO_s_mem());_writeBio=Api.BIO_new(Api.BIO_s_mem());Require(_readBio!=0 && _writeBio!=0,"Create USB TLS buffers");
            Api.SSL_set_bio(_ssl,_readBio,_writeBio);_biosAttached=true;Api.SSL_set_connect_state(_ssl);
        }
        catch { Dispose();throw; }
    }
    private static void Require(bool condition,string operation)
    {
        if(condition)return;
        var code=Api.ERR_get_error();var text=new byte[256];Api.ERR_error_string_n(code,text,(nuint)text.Length);
        var zero=Array.IndexOf(text,(byte)0);throw new AuthenticationException(operation+": "+Encoding.ASCII.GetString(text,0,zero<0?text.Length:zero));
    }
    private async Task DrainAsync(CancellationToken cancel)
    {
        var bytes=new byte[16384];
        while(Api.BIO_ctrl(_writeBio,10,0,0)>0)
        {
            var count=Api.BIO_read(_writeBio,bytes,bytes.Length);Require(count>0,"Read USB TLS output");
            Metadata("sent",bytes.AsSpan(0,count));
            await _transport.WriteAsync(bytes.AsMemory(0,count),cancel);
        }
    }
    private async Task ReceiveAsync(CancellationToken cancel)
    {
        var bytes=new byte[16384];var read=await _transport.ReadAsync(bytes,cancel);
        if(read==0)throw new EndOfStreamException("The iPhone closed the encrypted USB control transport.");
        Metadata("received",bytes.AsSpan(0,read));
        Require(Api.BIO_write(_readBio,bytes,read)==read,"Supply USB TLS input");
    }
    private void Metadata(string direction, ReadOnlySpan<byte> data)
    {
        // Header-only diagnostic. Never log certificate/key/record contents.
        if(_diagnostics is null || data.Length<5)return;
        var kind=data[0];var version=(data[1]<<8)|data[2];var size=(data[3]<<8)|data[4];
        var handshake=kind==22 && data.Length>5?data[5]:-1;
        _diagnostics($"TLS {direction}: recordType={kind}, version={version:X4}, recordBytes={size}, transferBytes={data.Length}, handshakeType={handshake}");
    }
    public async Task AuthenticateAsync(CancellationToken cancel)
    {
        for(var step=0;step<1024;step++)
        {
            cancel.ThrowIfCancellationRequested();var result=Api.SSL_do_handshake(_ssl);var error=result==1?0:Api.SSL_get_error(_ssl,result);
            await DrainAsync(cancel);
            if(result==1)
            {
                var peer=Api.SSL_get1_peer_certificate(_ssl);Require(peer!=0,"Read iPhone TLS certificate");
                try
                {
                    var length=Api.i2d_X509(peer,0);Require(length>0 && length<65536,"Validate iPhone TLS certificate size");
                    var der=new byte[length];var storage=Marshal.AllocHGlobal(length);
                    try {var pointer=storage;Require(Api.i2d_X509_copy(peer,ref pointer)==length,"Decode iPhone TLS certificate");Marshal.Copy(storage,der,0,length);}
                    finally{Marshal.FreeHGlobal(storage);}
                    using var certificate=X509CertificateLoader.LoadCertificate(der);using var key=certificate.GetRSAPublicKey();
                    if(key is null || !CryptographicOperations.FixedTimeEquals(_peerKeyHash,SHA256.HashData(key.ExportSubjectPublicKeyInfo())))throw new AuthenticationException("The encrypted USB peer does not match the paired iPhone public key.");
                }
                finally{Api.X509_free(peer);}
                _authenticated=true;return;
            }
            if(error==2){await ReceiveAsync(cancel);continue;}
            if(error==3)continue;
            Require(false,"iPhone USB TLS handshake failed ("+error+")");
        }
        throw new AuthenticationException("The iPhone TLS handshake exceeded its work bound.");
    }
    public override async ValueTask<int> ReadAsync(Memory<byte> buffer,CancellationToken cancellationToken=default)
    {
        ObjectDisposedException.ThrowIf(_disposed,this);if(!_authenticated)throw new InvalidOperationException("USB TLS is not authenticated.");if(buffer.IsEmpty)return 0;
        var temporary=new byte[Math.Min(buffer.Length,16384)];
        while(true)
        {
            cancellationToken.ThrowIfCancellationRequested();var read=Api.SSL_read(_ssl,temporary,temporary.Length);var error=read>0?0:Api.SSL_get_error(_ssl,read);
            await DrainAsync(cancellationToken);
            if(read>0){temporary.AsMemory(0,read).CopyTo(buffer);return read;}
            if(error==6)return 0;
            if(error==2){await ReceiveAsync(cancellationToken);continue;}
            if(error==3)continue;
            Require(false,"Read encrypted USB control ("+error+")");
        }
    }
    public override async ValueTask WriteAsync(ReadOnlyMemory<byte> buffer,CancellationToken cancellationToken=default)
    {
        ObjectDisposedException.ThrowIf(_disposed,this);if(!_authenticated)throw new InvalidOperationException("USB TLS is not authenticated.");
        while(!buffer.IsEmpty)
        {
            cancellationToken.ThrowIfCancellationRequested();var data=buffer[..Math.Min(buffer.Length,16384)].ToArray();var count=Api.SSL_write(_ssl,data,data.Length);var error=count>0?0:Api.SSL_get_error(_ssl,count);
            await DrainAsync(cancellationToken);
            if(count>0){buffer=buffer[count..];continue;}
            if(error==2){await ReceiveAsync(cancellationToken);continue;}if(error==3)continue;
            Require(false,"Write encrypted USB control ("+error+")");
        }
    }
    public override bool CanRead=>!_disposed;public override bool CanWrite=>!_disposed;public override bool CanSeek=>false;
    public override long Length=>throw new NotSupportedException();public override long Position{get=>throw new NotSupportedException();set=>throw new NotSupportedException();}
    public override void Flush(){}public override long Seek(long offset,SeekOrigin origin)=>throw new NotSupportedException();public override void SetLength(long length)=>throw new NotSupportedException();
    public override int Read(byte[] buffer,int offset,int count)=>ReadAsync(buffer.AsMemory(offset,count)).AsTask().GetAwaiter().GetResult();
    public override void Write(byte[] buffer,int offset,int count)=>WriteAsync(buffer.AsMemory(offset,count)).AsTask().GetAwaiter().GetResult();
    protected override void Dispose(bool disposing)
    {
        if(!_disposed)
        {
            _disposed=true;if(_ssl!=0)Api.SSL_free(_ssl);if(!_biosAttached){if(_readBio!=0)Api.BIO_free(_readBio);if(_writeBio!=0)Api.BIO_free(_writeBio);}
            if(_context!=0)Api.SSL_CTX_free(_context);_ssl=_context=_readBio=_writeBio=0;
        }
        base.Dispose(disposing);
    }
    private static class Api
    {
        static Api()
        {
            NativeLibrary.SetDllImportResolver(typeof(OpenSslUsbStream).Assembly,(name,_,_)=>
            {
                var file=name=="unkxwyno_ssl"?"libssl-3-x64.dll":name=="unkxwyno_crypto"?"libcrypto-3-x64.dll":null;if(file is null)return 0;
                var handle=LoadLibraryExW(Path.Combine(AppContext.BaseDirectory,file),0,0x100|0x800);
                if(handle==0)throw new DllNotFoundException("The verified USB TLS dependency could not load: "+file);return handle;
            });
            // Load the companion crypto module before SSL resolves its imports.
            _=OpenSSL_version(0);
        }
        [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)]private static extern nint LoadLibraryExW(string file,nint reserved,uint flags);
        [DllImport("unkxwyno_crypto")]public static extern nint OpenSSL_version(int type);
        [DllImport("unkxwyno_ssl")]public static extern nint TLS_client_method();
        [DllImport("unkxwyno_ssl")]public static extern nint SSL_CTX_new(nint method);
        [DllImport("unkxwyno_ssl")]public static extern void SSL_CTX_free(nint context);
        [DllImport("unkxwyno_ssl")]public static extern nint SSL_CTX_ctrl(nint context,int command,nint argument,nint pointer);
        [DllImport("unkxwyno_ssl")]public static extern int SSL_CTX_use_certificate(nint context,nint certificate);
        [DllImport("unkxwyno_ssl")]public static extern int SSL_CTX_use_PrivateKey(nint context,nint key);
        [DllImport("unkxwyno_ssl")]public static extern int SSL_CTX_check_private_key(nint context);
        [DllImport("unkxwyno_ssl")]public static extern nint SSL_new(nint context);
        [DllImport("unkxwyno_ssl")]public static extern void SSL_free(nint ssl);
        [DllImport("unkxwyno_ssl")]public static extern void SSL_set_bio(nint ssl,nint input,nint output);
        [DllImport("unkxwyno_ssl")]public static extern void SSL_set_connect_state(nint ssl);
        [DllImport("unkxwyno_ssl")]public static extern int SSL_do_handshake(nint ssl);
        [DllImport("unkxwyno_ssl")]public static extern int SSL_get_error(nint ssl,int result);
        [DllImport("unkxwyno_ssl")]public static extern int SSL_read(nint ssl,[Out]byte[] bytes,int count);
        [DllImport("unkxwyno_ssl")]public static extern int SSL_write(nint ssl,byte[] bytes,int count);
        [DllImport("unkxwyno_ssl")]public static extern nint SSL_get1_peer_certificate(nint ssl);
        [DllImport("unkxwyno_ssl")]public static extern nint SSL_get_version(nint ssl);
        [DllImport("unkxwyno_crypto")]public static extern nint BIO_s_mem();
        [DllImport("unkxwyno_crypto")]public static extern nint BIO_new(nint method);
        [DllImport("unkxwyno_crypto")]public static extern nint BIO_new_mem_buf(nint bytes,int count);
        [DllImport("unkxwyno_crypto")]public static extern int BIO_free(nint bio);
        [DllImport("unkxwyno_crypto")]public static extern nint BIO_ctrl(nint bio,int command,nint argument,nint pointer);
        [DllImport("unkxwyno_crypto")]public static extern int BIO_read(nint bio,[Out]byte[] bytes,int count);
        [DllImport("unkxwyno_crypto")]public static extern int BIO_write(nint bio,byte[] bytes,int count);
        [DllImport("unkxwyno_crypto")]public static extern nint PEM_read_bio_X509(nint bio,nint certificate,nint callback,nint context);
        [DllImport("unkxwyno_crypto")]public static extern nint PEM_read_bio_PrivateKey(nint bio,nint key,nint callback,nint context);
        [DllImport("unkxwyno_crypto")]public static extern void X509_free(nint certificate);
        [DllImport("unkxwyno_crypto")]public static extern void EVP_PKEY_free(nint key);
        [DllImport("unkxwyno_crypto")]public static extern int i2d_X509(nint certificate,nint output);
        [DllImport("unkxwyno_crypto",EntryPoint="i2d_X509")]public static extern int i2d_X509_copy(nint certificate,ref nint output);
        [DllImport("unkxwyno_crypto")]public static extern nuint ERR_get_error();
        [DllImport("unkxwyno_crypto")]public static extern void ERR_error_string_n(nuint code,[Out]byte[] text,nuint count);
    }
}

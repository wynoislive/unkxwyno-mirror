using UnkxwynoMirror.Desktop.Services.Usb;

// Explicit hardware harness; it is never part of the unattended test lane.
// Exercises the same trusted-session orchestrator and decoder as the desktop.
if (args.Length == 1 && args[0] is "--control-only" or "--control-pause-before-tls" or "--control-refresh-pairing")
{
    try
    {
    using var deadline = new CancellationTokenSource(TimeSpan.FromSeconds(45));
    var client = new UsbmuxClient(); var devices = await client.ListUsbDevicesAsync(deadline.Token);
    if (devices.Count != 1) { Console.Error.WriteLine("Control comparison requires exactly one USB iPhone."); return 2; }
    await using var tunnel = await client.OpenTunnelAsync(devices[0].DeviceId, 62078, deadline.Token);
    Stream control = args[0] == "--control-pause-before-tls" ? new BeforeTlsProbe(tunnel.Stream) : tunnel.Stream;
    await using var session = new UsbTrustedSession(control, Console.WriteLine, diagnostics: Console.WriteLine);
    await session.AuthenticateAsync(deadline.Token, refreshPairRecord: args[0] == "--control-refresh-pairing");
    Console.WriteLine("Control-only trusted session passed through the Apple USB service. This is not a screen-mirroring pass.");
    return 0;
    }
    catch (Exception exception) when (exception is IOException or System.Security.Authentication.AuthenticationException or System.Net.Sockets.SocketException or OperationCanceledException)
    { Console.Error.WriteLine("Trusted USB control test failed: " + exception.Message); return 1; }
}
if (args.Length != 1 || args[0] != "--hardware")
{
    Console.Error.WriteLine("Pass --hardware to start the connected iPhone capture test.");
    return 2;
}
using var mirror = new NativeUsbMirror(3840, 2160, Console.WriteLine);
var clock = System.Diagnostics.Stopwatch.StartNew();
mirror.StatusChanged += status => Console.WriteLine($"t={clock.Elapsed.TotalSeconds:F3} state={status.State} {status.Message} dimensions={status.Width}x{status.Height} displayed_frames={status.DisplayedFrames} displayed_fps={status.DisplayedFps:F1}");
Console.CancelKeyPress += (_, e) => { e.Cancel = true; mirror.Stop(); };
return await mirror.RunAsync();

// Hardware-only experiment: observe whether the transport dies before sending
// ClientHello. No payload, device identity or credential is recorded.
sealed class BeforeTlsProbe(Stream inner) : Stream
{
    private bool observed;
    public override async ValueTask WriteAsync(ReadOnlyMemory<byte> bytes, CancellationToken cancel = default)
    {
        if (!observed && bytes.Length >= 5 && bytes.Span[0] == 22 && bytes.Span[1] == 3)
        {
            observed = true;
            Console.WriteLine("Probe: withholding the first TLS write for two seconds; observing transport closure only.");
            using var wait = CancellationTokenSource.CreateLinkedTokenSource(cancel);
            wait.CancelAfter(TimeSpan.FromSeconds(2));
            try
            {
                var count = await inner.ReadAsync(new byte[1], wait.Token);
                throw new IOException(count == 0 ? "Probe: peer closed BEFORE ClientHello was sent." : "Probe: peer sent data BEFORE ClientHello; experiment stopped without consuming further data.");
            }
            catch (OperationCanceledException) when (!cancel.IsCancellationRequested)
            { Console.WriteLine("Probe: transport remained open before ClientHello. Sending the original TLS message now."); }
            catch (IOException e) { throw new IOException("Probe before ClientHello: " + e.Message, e); }
        }
        await inner.WriteAsync(bytes, cancel);
    }
    public override ValueTask<int> ReadAsync(Memory<byte> bytes,CancellationToken cancel=default)=>inner.ReadAsync(bytes,cancel);
    public override bool CanRead=>true;public override bool CanWrite=>true;public override bool CanSeek=>false;
    public override long Length=>throw new NotSupportedException();public override long Position{get=>throw new NotSupportedException();set=>throw new NotSupportedException();}
    public override void Flush()=>inner.Flush();public override long Seek(long o,SeekOrigin s)=>throw new NotSupportedException();public override void SetLength(long n)=>throw new NotSupportedException();
    public override int Read(byte[] b,int o,int n)=>inner.Read(b,o,n);
    public override void Write(byte[] b,int o,int n)=>WriteAsync(b.AsMemory(o,n)).AsTask().GetAwaiter().GetResult();
}

using UnkxwynoMirror.Desktop.Services.Usb;

static class UsbMirrorSessionTests
{
    private static void Check(bool value, string message) { if (!value) throw new InvalidOperationException(message); }
    private sealed class Capture(int state, string message) : IUsbMirrorCapture
    {
        public UsbMirrorStatus? LastStatus { get; private set; }
        public event Action<UsbMirrorStatus>? StatusChanged;
        public Task<int> RunAsync()
        {
            LastStatus = new(state, message, 0, 0, 0, 0); StatusChanged?.Invoke(LastStatus);
            return Task.FromResult(state == 0 ? 0 : 1);
        }
        public void Stop() { }
        public void Dispose() { }
    }
    private sealed class Lease(bool failRestore = false) : IAsyncDisposable
    {
        public bool Released { get; private set; }
        public ValueTask DisposeAsync()
        { Released = true; if (failRestore) throw new IOException("Recovery required."); return ValueTask.CompletedTask; }
    }
    public static async Task RunAsync()
    {
        var attempts = 0; var acquisitions = 0; var lease = new Lease();
        using (var session = new UsbMirrorSession(
            () => ++attempts == 1 ? new Capture(4, "Driver required.") : new Capture(3, "TLS reset."),
            (_, _) => { acquisitions++; return Task.FromResult<IAsyncDisposable>(lease); }))
        {
            Check(await session.RunAsync() == 1 && session.LastStatus?.Message == "TLS reset.", "A TLS failure after driver preparation must remain visible.");
            Check(attempts == 2 && acquisitions == 1 && lease.Released, "Driver preparation must be tried exactly once and released after TLS failure.");
        }
        attempts = acquisitions = 0;
        using (var session = new UsbMirrorSession(() => { attempts++; return new Capture(3, "TLS reset."); },
            (_, _) => { acquisitions++; throw new InvalidOperationException("Must not elevate for TLS errors."); }))
            Check(await session.RunAsync() == 1 && attempts == 1 && acquisitions == 0, "Ordinary capture failures must not switch drivers.");

        attempts = 0;
        using (var session = new UsbMirrorSession(() => { attempts++; return new Capture(4, "Driver required."); },
            (_, _) => throw new IOException("Windows approval canceled. No driver changed.")))
            Check(await session.RunAsync() == 1 && attempts == 1 && session.LastStatus?.Message.Contains("approval canceled") == true, "Declined elevation must stop without a capture retry.");

        using (var session = new UsbMirrorSession(() => new Capture(4, "Driver required."),
            async (_, cancel) => { await Task.Delay(Timeout.Infinite, cancel); return new Lease(); }))
        {
            var run = session.RunAsync(); session.Stop();
            Check(await run == 0 && session.LastStatus?.State == 0, "Stop must cancel pending acquisition rather than report success or continue capture.");
        }
        attempts = 0; lease = new Lease(true);
        using (var session = new UsbMirrorSession(() => ++attempts == 1 ? new Capture(4, "Driver required.") : new Capture(0, "Stopped."),
            (_, _) => Task.FromResult<IAsyncDisposable>(lease)))
            Check(await session.RunAsync() == 1 && lease.Released && session.LastStatus?.Message.Contains("restoration was not verified") == true,
                "A driver-restoration failure must fail the session even after an otherwise normal stop.");

        attempts = 0; lease = new Lease();
        using (var session = new UsbMirrorSession(() => { attempts++; return new Capture(4, "Still inaccessible."); },
            (_, _) => Task.FromResult<IAsyncDisposable>(lease)))
            Check(await session.RunAsync() == 1 && attempts == 2 && lease.Released && session.LastStatus?.State == 3,
                "Repeated capture-access failure must restore once and surface an error without a switch loop.");
        Console.WriteLine("USB session ownership tests passed: driver-only retry, TLS failure, canceled elevation/acquisition and verified restoration failure handling. No driver was changed.");
    }
}

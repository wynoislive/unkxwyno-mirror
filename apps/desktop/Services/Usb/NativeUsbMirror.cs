using System.Runtime.InteropServices;

namespace UnkxwynoMirror.Desktop.Services.Usb;

/// <summary>Owns one independent native capture session. No process or driver installation.</summary>
public sealed class NativeUsbMirror : IUsbMirrorCapture
{
    private readonly object _gate = new();
    private nint _session;
    private readonly StatusCallback _callback;
    private bool _started;
    private bool _disposed;
    private UsbMirrorStatus? _lastStatus;
    private readonly CancellationTokenSource _pairCancellation = new();
    private Task? _pairTask;
    private readonly Action<string>? _diagnostics;
    public UsbMirrorStatus? LastStatus { get { lock (_gate) return _lastStatus; } }
    public event Action<UsbMirrorStatus>? StatusChanged;

    public NativeUsbMirror(uint displayWidth, uint displayHeight, Action<string>? diagnostics = null, uint targetFps = 60)
    {
        _diagnostics=diagnostics;
        _callback = OnStatus;
        _session = Create(displayWidth, displayHeight, targetFps);
        if (_session == 0) throw new InvalidOperationException("Cannot allocate the USB capture session.");
        try { if (EnablePairing(_session) != 0) throw new InvalidOperationException("Cannot enable the trusted USB control session."); }
        catch { Destroy(_session); _session = 0; throw; }
    }

    public Task<int> RunAsync()
    {
        lock (_gate)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            if (_started) throw new InvalidOperationException("USB capture has already started.");
            _started = true;
            return Task.Run(() =>
            {
                try { return Run(_session, _callback, 0); }
                finally
                {
                    _pairCancellation.Cancel();
                    Task? pairing;
                    lock (_gate) pairing = _pairTask;
                    try { pairing?.GetAwaiter().GetResult(); } catch (OperationCanceledException) { }
                    lock (_gate)
                    {
                        Destroy(_session);
                        _session = 0;
                    }
                    GC.KeepAlive(_callback);
                }
            });
        }
    }

    public void Stop()
    {
        lock (_gate) { if (_session != 0) StopNative(_session); }
    }

    private void OnStatus(nint context, int state, nint message, uint width, uint height, ulong frames, double fps)
    {
        // Managed exceptions must never cross the native callback boundary.
        try
        {
            var status = new UsbMirrorStatus(state, Marshal.PtrToStringUTF8(message) ?? "USB capture status unavailable.", width, height, frames, fps);
            lock (_gate) _lastStatus = status;
            if (state == 5)
            {
                lock (_gate) _pairTask ??= Task.Run(AuthenticateAsync);
            }
            StatusChanged?.Invoke(status);
        }
        catch (Exception exception) { System.Diagnostics.Debug.WriteLine(exception); }
    }

    private async Task AuthenticateAsync()
    {
        try
        {
            await using var session = new UsbTrustedSession(new ControlStream(this), message =>
            {
                var status = new UsbMirrorStatus(1, message, 0, 0, 0, 0);
                lock (_gate) _lastStatus = status;
                StatusChanged?.Invoke(status);
            }, diagnostics: _diagnostics);
            await session.AuthenticateAsync(_pairCancellation.Token);
            lock (_gate) { if (_session != 0) AuthResult(_session, 1, ""); }
            await session.MonitorAsync(_pairCancellation.Token);
        }
        catch (OperationCanceledException) when (_pairCancellation.IsCancellationRequested) { }
        catch (Exception exception)
        {
            if (_pairCancellation.IsCancellationRequested) return;
            lock (_gate) { if (_session != 0) AuthResult(_session, 0, "Trusted iPhone session failed: " + exception.Message); }
        }
    }

    private sealed class ControlStream(NativeUsbMirror owner) : Stream
    {
        public override bool CanRead => true;
        public override bool CanWrite => true;
        public override bool CanSeek => false;
        public override long Length => throw new NotSupportedException();
        public override long Position { get => throw new NotSupportedException(); set => throw new NotSupportedException(); }
        public override void Flush() { }
        public override long Seek(long offset, SeekOrigin origin) => throw new NotSupportedException();
        public override void SetLength(long value) => throw new NotSupportedException();
        private int ReadData(byte[] buffer, int offset, int count, CancellationToken cancellation)
        {
            if (count == 0) return 0;
            var temporary = new byte[Math.Min(count, 16 * 1024)];
            while (true)
            {
                cancellation.ThrowIfCancellationRequested();
                // RunAsync joins the pairing task before destroying the native
                // handle. Do not hold the UI/status lock across a 200 ms read:
                // the native viewer invokes OnStatus on its message thread.
                nint session;
                lock (owner._gate) session = owner._session;
                var read = session == 0 ? -2 : ControlRead(session, temporary, (uint)temporary.Length);
                if (read < 0) return 0;
                if (read == 0) continue;
                temporary.AsSpan(0, read).CopyTo(buffer.AsSpan(offset, read)); return read;
            }
        }
        public override int Read(byte[] buffer, int offset, int count) => ReadData(buffer, offset, count, CancellationToken.None);
        public override async ValueTask<int> ReadAsync(Memory<byte> buffer, CancellationToken cancellationToken = default)
        {
            var temporary = new byte[Math.Min(buffer.Length, 16 * 1024)];
            var read = await Task.Run(() => ReadData(temporary, 0, temporary.Length, cancellationToken), cancellationToken);
            temporary.AsMemory(0, read).CopyTo(buffer); return read;
        }
        public override void Write(byte[] buffer, int offset, int count)
        {
            if (count == 0) return;
            var bytes = buffer.AsSpan(offset, count).ToArray(); int written;
            lock (owner._gate) written = owner._session == 0 ? -1 : ControlWrite(owner._session, bytes, (uint)bytes.Length);
            if (written != count) throw new IOException("The iPhone USB control stream closed while sending.");
        }
        public override ValueTask WriteAsync(ReadOnlyMemory<byte> buffer, CancellationToken cancellationToken = default)
        { cancellationToken.ThrowIfCancellationRequested(); var bytes = buffer.ToArray(); Write(bytes, 0, bytes.Length); return ValueTask.CompletedTask; }
    }

    public void Dispose()
    {
        lock (_gate)
        {
            if (_disposed) return;
            _disposed = true;
            _pairCancellation.Cancel();
            if (_session == 0) return;
            StopNative(_session);
            // The running task owns destruction until the native worker has joined.
            if (!_started) { Destroy(_session); _session = 0; }
        }
        GC.SuppressFinalize(this);
    }

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void StatusCallback(nint context, int state, nint message, uint width, uint height, ulong frames, double fps);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_create", CallingConvention = CallingConvention.Cdecl)]
    private static extern nint Create(uint width, uint height, uint targetFps);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_run", CallingConvention = CallingConvention.Cdecl)]
    private static extern int Run(nint session, StatusCallback callback, nint context);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_stop", CallingConvention = CallingConvention.Cdecl)]
    private static extern void StopNative(nint session);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_destroy", CallingConvention = CallingConvention.Cdecl)]
    private static extern void Destroy(nint session);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_enable_pairing", CallingConvention = CallingConvention.Cdecl)]
    private static extern int EnablePairing(nint session);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_control_read", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ControlRead(nint session, [Out] byte[] buffer, uint capacity);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_control_write", CallingConvention = CallingConvention.Cdecl)]
    private static extern int ControlWrite(nint session, byte[] buffer, uint length);
    [DllImport("unkxwyno_core", EntryPoint = "uxm_usb_mirror_auth_result", CallingConvention = CallingConvention.Cdecl)]
    private static extern void AuthResult(nint session, int trusted, [MarshalAs(UnmanagedType.LPUTF8Str)] string error);
}

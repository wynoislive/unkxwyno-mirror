namespace UnkxwynoMirror.Desktop.Services.Usb;

/// <summary>Owns capture and, only when required, a temporary driver lease.</summary>
public sealed class UsbMirrorSession(
    Func<IUsbMirrorCapture> createCapture,
    Func<Action<string>, CancellationToken, Task<IAsyncDisposable>> acquireDriver) : IUsbMirrorCapture
{
    private readonly object _gate = new();
    private readonly CancellationTokenSource _stop = new();
    private IUsbMirrorCapture? _capture;
    private UsbMirrorStatus? _lastStatus;
    private bool _started, _disposed;
    public UsbMirrorStatus? LastStatus { get { lock (_gate) return _lastStatus; } }
    public event Action<UsbMirrorStatus>? StatusChanged;

    private void Report(UsbMirrorStatus status)
    {
        lock (_gate) _lastStatus = status;
        StatusChanged?.Invoke(status);
    }
    private void Progress(string message) => Report(new(1, message, 0, 0, 0, 0));
    private async Task<int> AttemptAsync()
    {
        using var capture = createCapture();
        lock (_gate) { _capture = capture; if (_stop.IsCancellationRequested) capture.Stop(); }
        capture.StatusChanged += Report;
        try
        {
            var result = await capture.RunAsync();
            if (capture.LastStatus is { } status) Report(status);
            return result;
        }
        finally { capture.StatusChanged -= Report; lock (_gate) _capture = null; }
    }
    public async Task<int> RunAsync()
    {
        lock (_gate)
        {
            ObjectDisposedException.ThrowIf(_disposed, this);
            if (_started) throw new InvalidOperationException("USB capture has already started.");
            _started = true;
        }
        IAsyncDisposable? driver = null;
        var result = 1;
        UsbMirrorStatus? outcome = null;
        try
        {
            _stop.Token.ThrowIfCancellationRequested();
            result = await AttemptAsync();
            _stop.Token.ThrowIfCancellationRequested();
            // A typed native status identifies this condition. Never switch a
            // driver in response to trust, TLS, codec or ordinary I/O failures.
            if (!_stop.IsCancellationRequested && result != 0 && LastStatus?.State == 4)
            {
                Progress("Preparing temporary iPhone USB capture access. Accept the Windows administrator prompt.");
                driver = await acquireDriver(Progress, _stop.Token);
                _stop.Token.ThrowIfCancellationRequested();
                result = await AttemptAsync(); // Exactly one retry, under the lease.
            }
            outcome = LastStatus;
            if (result != 0 && driver is not null && outcome?.State == 4)
                outcome = outcome with { State = 3, Message = "Capture access is still unavailable with the temporary driver. " + outcome.Message };
        }
        catch (OperationCanceledException) when (_stop.IsCancellationRequested)
        { result = 0; outcome = new(0, "USB mirroring stopped.", 0, 0, 0, 0); }
        catch (Exception exception)
        { outcome = new(3, exception.Message, 0, 0, 0, 0); }
        finally
        {
            if (driver is not null)
            {
                Progress("Restoring the Apple USB driver. Keep the iPhone connected.");
                try { await driver.DisposeAsync(); Progress("Apple USB driver restored."); }
                catch (Exception exception)
                {
                    result = 1;
                    outcome = new(3, "Apple USB driver restoration was not verified. " + exception.Message, 0, 0, 0, 0);
                }
            }
            if (outcome is not null) Report(outcome);
        }
        return result;
    }
    public void Stop()
    {
        _stop.Cancel();
        lock (_gate) _capture?.Stop();
    }
    public void Dispose()
    {
        lock (_gate) { if (_disposed) return; _disposed = true; }
        Stop();
    }
}

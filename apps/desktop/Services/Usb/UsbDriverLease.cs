using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Text.Json;

namespace UnkxwynoMirror.Desktop.Services.Usb;

/// <summary>Scopes an elevated driver broker to this desktop process/session.</summary>
public sealed class UsbDriverLease : IAsyncDisposable
{
    private readonly string _directory;
    private readonly Process _broker;
    private bool _released;
    private UsbDriverLease(string directory, Process broker) { _directory = directory; _broker = broker; }
    private static string SessionsRoot => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "UNKXWYNO Mirror", "usb-driver-sessions");
    private sealed record Journal(string State, string Message, string Error, bool RestorationRequired, bool Restored, bool RebootRequired);
    private static void CheckPath(string path)
    {
        for (string? cursor = Path.GetFullPath(path); cursor is not null; cursor = Path.GetDirectoryName(cursor))
            if (Path.Exists(cursor) && (File.GetAttributes(cursor) & FileAttributes.ReparsePoint) != 0)
                throw new IOException("USB session paths must not contain junctions or symbolic links.");
    }
    private static Journal? ReadJournal(string directory)
    {
        var path = Path.Combine(directory, "driver-journal.json"); CheckPath(path);
        if (!File.Exists(path)) return null;
        if (new FileInfo(path).Length > 16384) throw new IOException("USB driver recovery journal exceeds its bound.");
        using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
        return JsonSerializer.Deserialize<Journal>(stream) ?? throw new IOException("USB driver recovery journal is invalid.");
    }
    private static Process Launch(string arguments)
    {
        var script = Path.Combine(AppContext.BaseDirectory, "UsbDriverSession.ps1");
        if (!File.Exists(script)) throw new IOException("The packaged USB driver-session helper is missing.");
        CheckPath(script);
        var powershell = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32", "WindowsPowerShell", "v1.0", "powershell.exe");
        try
        {
            return Process.Start(new ProcessStartInfo(powershell)
            {
                Arguments = "-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \"" + script + "\" " + arguments,
                UseShellExecute = true, Verb = "runas", WindowStyle = ProcessWindowStyle.Hidden,
            }) ?? throw new IOException("Windows did not start the USB driver-session helper.");
        }
        catch (Win32Exception exception) when (exception.NativeErrorCode == 1223)
        { throw new IOException("Windows administrator approval was canceled. No driver was changed.", exception); }
    }
    public static async Task<IAsyncDisposable> AcquireAsync(Action<string> progress, CancellationToken cancel)
    {
        cancel.ThrowIfCancellationRequested(); CheckPath(SessionsRoot);
        var id = Guid.NewGuid().ToString("N"); var directory = Path.Combine(SessionsRoot, id);
        using var owner = Process.GetCurrentProcess();
        var arguments = "-Acquire -SessionId " + id + " -OwnerPid " + Environment.ProcessId.ToString(CultureInfo.InvariantCulture)
            + " -OwnerStartTicks " + owner.StartTime.ToUniversalTime().Ticks.ToString(CultureInfo.InvariantCulture);
        var broker = await Task.Run(() => Launch(arguments));
        var lease = new UsbDriverLease(directory, broker);
        try
        {
            using var deadline = CancellationTokenSource.CreateLinkedTokenSource(cancel);
            deadline.CancelAfter(TimeSpan.FromSeconds(90));
            string? previous = null;
            while (true)
            {
                deadline.Token.ThrowIfCancellationRequested();
                var journal = ReadJournal(directory);
                if (journal is not null)
                {
                    if (journal.Message != previous) { previous = journal.Message; progress(journal.Message); }
                    if (!string.IsNullOrEmpty(journal.Error) || journal.State is "Failed" or "RecoveryRequired")
                        throw new IOException(string.IsNullOrEmpty(journal.Error) ? journal.Message : journal.Error);
                    if (journal.State == "Ready" && !broker.HasExited) return lease;
                }
                if (broker.HasExited) throw new IOException("The USB driver helper stopped before capture access was ready. No capture session was started.");
                await Task.Delay(250, deadline.Token);
            }
        }
        catch (Exception original)
        {
            try { await lease.DisposeAsync(); }
            catch (Exception recovery) { throw new IOException(original.Message + " Driver recovery: " + recovery.Message, recovery); }
            throw;
        }
    }
    public async ValueTask DisposeAsync()
    {
        if (_released) return; _released = true;
        try
        {
            var deadline = Stopwatch.StartNew();
            while (!_broker.HasExited)
            {
                CheckPath(_directory);
                // The elevated helper creates its directory before changing a
                // driver. Repeat this check if cancellation preceded creation.
                if (Directory.Exists(_directory))
                {
                    var stop = Path.Combine(_directory, "stop.request"); CheckPath(stop);
                    if (!File.Exists(stop)) using (File.Create(stop)) { }
                }
                if (deadline.Elapsed > TimeSpan.FromSeconds(90)) throw new IOException("The USB helper is still restoring the driver. Keep the phone connected; its recovery journal is retained.");
                await Task.Delay(250);
            }
            var journal = ReadJournal(_directory);
            if (journal is null) throw new IOException("The USB helper did not produce a driver-status journal.");
            if (journal.RestorationRequired && !journal.Restored)
                throw new IOException(journal.Message + " Close and reopen this app, then choose Mirror iPhone USB to recover after any required Windows restart.");
        }
        finally { _broker.Dispose(); }
    }
    public static async Task RecoverPendingAsync(Action<string> progress, CancellationToken cancel)
    {
        CheckPath(SessionsRoot);
        if (!Directory.Exists(SessionsRoot)) return;
        foreach (var directory in Directory.EnumerateDirectories(SessionsRoot))
        {
            if (!Guid.TryParseExact(Path.GetFileName(directory), "N", out var id)) continue;
            var journal = ReadJournal(directory);
            if (journal is null || !journal.RestorationRequired || journal.Restored) continue;
            cancel.ThrowIfCancellationRequested(); progress("Recovering an unfinished Apple USB driver session. Accept the Windows administrator prompt.");
            using var recovery = await Task.Run(() => Launch("-Recover -SessionId " + id.ToString("N")));
            // Do not abandon an in-progress driver restoration on cancellation.
            var completion = recovery.WaitForExitAsync();
            if (await Task.WhenAny(completion, Task.Delay(TimeSpan.FromSeconds(90))) != completion)
                throw new IOException("Apple USB recovery is still running. Keep the phone connected and wait before starting another capture.");
            await completion;
            var restored = ReadJournal(directory);
            if (recovery.ExitCode != 0 || restored is not { Restored: true })
                throw new IOException("Apple USB driver recovery was not verified. " + restored?.Message);
            progress("Apple USB driver restored.");
        }
    }
}

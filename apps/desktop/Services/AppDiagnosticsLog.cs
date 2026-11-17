using System.Text;
using System.Text.Json;

namespace UnkxwynoMirror.Desktop.Services;

public sealed record DiagnosticsLogEntry(
    DateTimeOffset TimestampUtc,
    string Level,
    string EventName,
    string Message);

public sealed class AppDiagnosticsLog
{
    private static readonly JsonSerializerOptions JsonOptions = new(JsonSerializerDefaults.Web);
    private readonly object _gate = new();

    public AppDiagnosticsLog(string logPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(logPath);
        LogPath = Path.GetFullPath(logPath);
        LogDirectory = Path.GetDirectoryName(LogPath)
            ?? throw new ArgumentException("The log path must include a directory.", nameof(logPath));
    }

    public string LogPath { get; }
    public string LogDirectory { get; }

    public static AppDiagnosticsLog CreateDefault()
    {
        var localData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrWhiteSpace(localData))
        {
            localData = Path.GetTempPath();
        }
        return new(Path.Combine(localData, "UNKXWYNO Mirror", "logs", "desktop.jsonl"));
    }

    public bool TryWrite(string level, string eventName, string message, out string? error)
    {
        error = null;
        var entry = new DiagnosticsLogEntry(
            DateTimeOffset.UtcNow,
            Bound(level, 16, "Info"),
            Bound(eventName, 64, "unknown"),
            Bound(message, 1024, string.Empty));
        try
        {
            lock (_gate)
            {
                Directory.CreateDirectory(LogDirectory);
                var line = JsonSerializer.Serialize(entry, JsonOptions) + Environment.NewLine;
                File.AppendAllText(LogPath, line, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
            }
            return true;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            error = "The local diagnostics log could not be written.";
            return false;
        }
    }

    private static string Bound(string? value, int maximumLength, string fallback)
    {
        var normalized = string.IsNullOrWhiteSpace(value) ? fallback : value.Trim();
        return normalized.Length <= maximumLength ? normalized : normalized[..maximumLength];
    }
}

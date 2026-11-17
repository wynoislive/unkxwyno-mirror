using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using UnkxwynoMirror.Desktop.Models;

namespace UnkxwynoMirror.Desktop.Services;

public sealed record SettingsLoadResult(DesktopSettings Settings, bool Loaded, string? Warning);

public sealed class SettingsStore
{
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true,
        UnmappedMemberHandling = JsonUnmappedMemberHandling.Disallow,
        Converters = { new JsonStringEnumConverter(JsonNamingPolicy.CamelCase, allowIntegerValues: false) },
    };

    public SettingsStore(string settingsPath)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(settingsPath);
        SettingsPath = Path.GetFullPath(settingsPath);
        DataDirectory = Path.GetDirectoryName(SettingsPath)
            ?? throw new ArgumentException("The settings path must include a directory.", nameof(settingsPath));
    }

    public string SettingsPath { get; }
    public string DataDirectory { get; }

    public static SettingsStore CreateDefault()
    {
        var localData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        if (string.IsNullOrWhiteSpace(localData))
        {
            localData = Path.GetTempPath();
        }
        return new(Path.Combine(localData, "UNKXWYNO Mirror", "settings.json"));
    }

    public SettingsLoadResult Load()
    {
        if (!File.Exists(SettingsPath))
        {
            return new(new(), false, null);
        }

        try
        {
            var json = File.ReadAllText(SettingsPath, Encoding.UTF8);
            var settings = JsonSerializer.Deserialize<DesktopSettings>(json, JsonOptions);
            if (settings is null ||
                settings.SchemaVersion != DesktopSettings.CurrentSchemaVersion ||
                !Enum.IsDefined(settings.RequestedResolution) ||
                !Enum.IsDefined(settings.RequestedFrameRate))
            {
                return Invalid("Settings use an unsupported schema or value; safe defaults were loaded.");
            }
            return new(settings, true, null);
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or JsonException)
        {
            return Invalid("Settings could not be read; safe defaults were loaded.");
        }
    }

    public bool TrySave(DesktopSettings settings, out string? error)
    {
        ArgumentNullException.ThrowIfNull(settings);
        error = null;
        if (settings.SchemaVersion != DesktopSettings.CurrentSchemaVersion ||
            !Enum.IsDefined(settings.RequestedResolution) ||
            !Enum.IsDefined(settings.RequestedFrameRate))
        {
            error = "Settings contain an unsupported schema or value.";
            return false;
        }

        var temporaryPath = Path.Combine(DataDirectory, $".{Path.GetFileName(SettingsPath)}.{Guid.NewGuid():N}.tmp");
        try
        {
            Directory.CreateDirectory(DataDirectory);
            var json = JsonSerializer.Serialize(settings, JsonOptions);
            File.WriteAllText(temporaryPath, json, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
            using (var stream = new FileStream(temporaryPath, FileMode.Open, FileAccess.ReadWrite, FileShare.None))
            {
                stream.Flush(flushToDisk: true);
            }
            File.Move(temporaryPath, SettingsPath, overwrite: true);
            return true;
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException)
        {
            error = "Settings could not be saved to local application data.";
            return false;
        }
        finally
        {
            try
            {
                File.Delete(temporaryPath);
            }
            catch (IOException)
            {
                // A failed cleanup must not replace the original save result.
            }
            catch (UnauthorizedAccessException)
            {
                // A failed cleanup must not replace the original save result.
            }
        }
    }

    private static SettingsLoadResult Invalid(string warning) => new(new(), false, warning);
}

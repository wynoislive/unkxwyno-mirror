namespace UnkxwynoMirror.Desktop.Models;

public sealed record DesktopSettings
{
    public const int CurrentSchemaVersion = 1;

    public int SchemaVersion { get; init; } = CurrentSchemaVersion;
    public ResolutionChoice RequestedResolution { get; init; } = ResolutionChoice.Auto;
    public FrameRateChoice RequestedFrameRate { get; init; } = FrameRateChoice.Auto;

    public static DesktopSettings FromSelection(QualitySelection selection)
    {
        ArgumentNullException.ThrowIfNull(selection);
        return new()
        {
            RequestedResolution = selection.RequestedResolution,
            RequestedFrameRate = selection.RequestedFrameRate,
        };
    }
}

namespace UnkxwynoMirror.Desktop.Models;

public enum ResolutionChoice
{
    Auto,
    P480,
    Hd,
    FullHd,
    Qhd,
    Uhd,
    Native,
    Custom,
}

public enum FrameRateChoice
{
    Auto = 0,
    Fps24 = 24,
    Fps30 = 30,
    Fps45 = 45,
    Fps60 = 60,
    Fps90 = 90,
    Fps120 = 120,
}

public sealed record ChoiceItem<T>(T Value, string Label);

public static class QualityCatalog
{
    public static IReadOnlyList<ChoiceItem<ResolutionChoice>> Resolutions { get; } =
    [
        new(ResolutionChoice.Auto, "Auto"),
        new(ResolutionChoice.P480, "480p"),
        new(ResolutionChoice.Hd, "HD · 1280 × 720"),
        new(ResolutionChoice.FullHd, "Full HD · 1920 × 1080"),
        new(ResolutionChoice.Qhd, "2K / QHD · 2560 × 1440"),
        new(ResolutionChoice.Uhd, "4K / UHD · 3840 × 2160"),
        new(ResolutionChoice.Native, "Native"),
        new(ResolutionChoice.Custom, "Custom"),
    ];

    public static IReadOnlyList<ChoiceItem<FrameRateChoice>> FrameRates { get; } =
    [
        new(FrameRateChoice.Auto, "Auto FPS"),
        new(FrameRateChoice.Fps24, "24 FPS"),
        new(FrameRateChoice.Fps30, "30 FPS"),
        new(FrameRateChoice.Fps45, "45 FPS"),
        new(FrameRateChoice.Fps60, "60 FPS"),
        new(FrameRateChoice.Fps90, "90 FPS"),
        new(FrameRateChoice.Fps120, "120 FPS"),
    ];
}

public sealed class QualitySelection
{
    public ResolutionChoice RequestedResolution { get; set; } = ResolutionChoice.Auto;
    public FrameRateChoice RequestedFrameRate { get; set; } = FrameRateChoice.Auto;
    public string RequestedLabel => $"{ResolutionLabel(RequestedResolution)} · {FrameRateLabel(RequestedFrameRate)}";
    public string ActiveLabel { get; private set; } = "—";
    public string Limitation { get; private set; } =
        "No active stream. Native source, encoder, decoder, transport, and display capabilities have not been negotiated.";

    public void ApplyActive(string resolution, int framesPerSecond)
    {
        ArgumentException.ThrowIfNullOrWhiteSpace(resolution);
        ArgumentOutOfRangeException.ThrowIfNegativeOrZero(framesPerSecond);
        ActiveLabel = $"{resolution} · {framesPerSecond} FPS";
        Limitation = string.Empty;
    }

    public void ClearActive(string reason)
    {
        ActiveLabel = "—";
        Limitation = string.IsNullOrWhiteSpace(reason) ? "No active stream." : reason;
    }

    private static string ResolutionLabel(ResolutionChoice value) =>
        QualityCatalog.Resolutions.First(item => item.Value == value).Label;

    private static string FrameRateLabel(FrameRateChoice value) =>
        QualityCatalog.FrameRates.First(item => item.Value == value).Label;
}


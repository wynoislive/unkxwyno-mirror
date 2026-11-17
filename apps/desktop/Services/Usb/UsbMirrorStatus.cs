namespace UnkxwynoMirror.Desktop.Services.Usb;

public sealed record UsbMirrorStatus(int State, string Message, uint Width, uint Height, ulong DisplayedFrames, double DisplayedFps);

public interface IUsbMirrorCapture : IDisposable
{
    UsbMirrorStatus? LastStatus { get; }
    event Action<UsbMirrorStatus>? StatusChanged;
    Task<int> RunAsync();
    void Stop();
}

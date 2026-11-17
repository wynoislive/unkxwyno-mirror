using Microsoft.UI.Xaml;
using UnkxwynoMirror.Desktop.Services;

namespace UnkxwynoMirror.Desktop;

public partial class App : Application
{
    private Window? _window;
    private readonly AppDiagnosticsLog _diagnosticsLog = AppDiagnosticsLog.CreateDefault();

    public App()
    {
        InitializeComponent();
        UnhandledException += OnUnhandledException;
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        _window = new MainWindow();
        _window.Activate();
    }

    private void OnUnhandledException(object sender, Microsoft.UI.Xaml.UnhandledExceptionEventArgs args)
    {
        _diagnosticsLog.TryWrite(
            "Error",
            "app.unhandled_exception",
            $"An unhandled {args.Exception.GetType().Name} reached the UI dispatcher.",
            out _);
        System.Diagnostics.Debug.WriteLine(args.Exception);
    }
}


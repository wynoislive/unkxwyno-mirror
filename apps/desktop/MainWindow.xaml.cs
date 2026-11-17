using System.Collections.ObjectModel;
using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using UnkxwynoMirror.Desktop.Models;
using UnkxwynoMirror.Desktop.Services;
using UnkxwynoMirror.Desktop.Services.Usb;
using Windows.Graphics;
using WinRT.Interop;

namespace UnkxwynoMirror.Desktop;

public sealed partial class MainWindow : Window
{
    private readonly LocalDiscoveryService _discovery = new();
    private readonly ObservableCollection<DiscoveredPeer> _peers = [];
    private readonly QualitySelection _quality = new();
    private readonly SettingsStore _settingsStore = SettingsStore.CreateDefault();
    private readonly AppDiagnosticsLog _diagnosticsLog = AppDiagnosticsLog.CreateDefault();
    private bool _loadingSettings = true;
    private string? _settingsWarning;
    private CoreProbeResult? _lastCoreProbe;
    private string _currentDestination = "Home";
    private DateTime _lastUsbMetricsLog;
    private readonly CancellationTokenSource _windowLifetime = new();
    private bool _windowClosed;
    private UsbMirrorSession? _usbMirror;
    private bool _usbStarting;
    private UsbMirrorStatus? _usbStatus;

    public MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(HeaderDragRegion);
        var displayArea = DisplayArea.GetFromWindowId(AppWindow.Id, DisplayAreaFallback.Primary);
        var workArea = displayArea.WorkArea;
        var width = Math.Min(1280, Math.Max(900, workArea.Width - 32));
        var height = Math.Min(832, Math.Max(640, workArea.Height - 32));
        AppWindow.MoveAndResize(new RectInt32(
            workArea.X + Math.Max(0, (workArea.Width - width) / 2),
            workArea.Y + Math.Max(0, (workArea.Height - height) / 2),
            width,
            height));
        AppWindow.SetPresenter(AppWindowPresenterKind.Overlapped);

        DevicesList.ItemsSource = _peers;
        UpdateDeviceEmptyState();
        ResolutionCombo.ItemsSource = QualityCatalog.Resolutions;
        FrameRateCombo.ItemsSource = QualityCatalog.FrameRates;
        var settingsResult = _settingsStore.Load();
        _settingsWarning = settingsResult.Warning;
        _quality.RequestedResolution = settingsResult.Settings.RequestedResolution;
        _quality.RequestedFrameRate = settingsResult.Settings.RequestedFrameRate;
        ResolutionCombo.SelectedItem = QualityCatalog.Resolutions.First(item =>
            item.Value == settingsResult.Settings.RequestedResolution);
        FrameRateCombo.SelectedItem = QualityCatalog.FrameRates.First(item =>
            item.Value == settingsResult.Settings.RequestedFrameRate);
        _loadingSettings = false;

        _discovery.PeerAdded += OnPeerAdded;
        _discovery.PeerRemoved += OnPeerRemoved;
        _discovery.EnumerationCompleted += OnEnumerationCompleted;
        _discovery.DiscoveryError += OnDiscoveryError;
        Closed += OnClosed;
        WriteLog("Info", "app.started", "Desktop window initialized.");
        if (settingsResult.Loaded)
        {
            WriteLog("Info", "settings.loaded", "Requested quality settings loaded.");
        }
        else if (settingsResult.Warning is not null)
        {
            WriteLog("Warning", "settings.invalid", settingsResult.Warning);
        }
    }

    private void OnLoaded(object sender, RoutedEventArgs args)
    {
        _lastCoreProbe = IndependentCoreProbe.Probe();
        EngineStatusText.Text = _lastCoreProbe.Available
            ? $"Independent core {_lastCoreProbe.Version} · Media setup required"
            : "Independent engine · Setup required";
        ToolTipService.SetToolTip(EngineStatusText, _lastCoreProbe.Explanation);
        FindDevicesButton.Focus(FocusState.Programmatic);
        UpdateQualityStatus();
        UpdateDiagnostics();
        UpdateResponsiveLayout();
        if (_settingsWarning is not null)
        {
            HomeInfoBar.Title = "Settings reset safely";
            HomeInfoBar.Message = _settingsWarning;
            HomeInfoBar.Severity = InfoBarSeverity.Warning;
            HomeInfoBar.IsOpen = true;
        }
    }

    private void OnNavigate(object sender, RoutedEventArgs args)
    {
        if (sender is not Button { Tag: string destination })
        {
            return;
        }

        HomeView.Visibility = destination == "Home" ? Visibility.Visible : Visibility.Collapsed;
        DevicesView.Visibility = destination == "Devices" ? Visibility.Visible : Visibility.Collapsed;
        CapturesView.Visibility = destination == "Captures" ? Visibility.Visible : Visibility.Collapsed;
        SettingsView.Visibility = destination == "Settings" ? Visibility.Visible : Visibility.Collapsed;
        DiagnosticsView.Visibility = destination == "Diagnostics" ? Visibility.Visible : Visibility.Collapsed;
        _currentDestination = destination;
        UpdateResponsiveLayout();
        if (destination == "Diagnostics")
        {
            UpdateDiagnostics(refreshCore: true);
        }
    }

    private void OnFindDevices(object sender, RoutedEventArgs args)
    {
        _peers.Clear();
        HomeInfoBar.Title = "Searching local network";
        HomeInfoBar.Message = "Only UNKXWYNO DNS-SD services are considered. No Internet request is made.";
        HomeInfoBar.Severity = InfoBarSeverity.Informational;
        HomeInfoBar.IsOpen = true;
        try
        {
            _discovery.Stop();
            _discovery.Start();
            WriteLog("Info", "discovery.started", "Local DNS-SD discovery started.");
            UpdateDiagnostics();
        }
        catch (Exception exception)
        {
            ShowDiscoveryError(exception.Message);
        }
    }

    private async void OnPairManually(object sender, RoutedEventArgs args)
    {
        var addressBox = new TextBox
        {
            Header = "Local peer address",
            PlaceholderText = "192.168.1.20:49791",
        };
        var dialogContent = new StackPanel { Spacing = 8 };
        dialogContent.Children.Add(addressBox);
        dialogContent.Children.Add(new TextBlock
        {
            Text = "The address is validated locally. A connection is not trusted until the secure pairing layer is available.",
            TextWrapping = TextWrapping.Wrap,
            Foreground = (Brush)Application.Current.Resources["SecondaryTextBrush"],
        });
        var dialog = new ContentDialog
        {
            XamlRoot = Content.XamlRoot,
            Title = "Pair manually",
            Content = dialogContent,
            PrimaryButtonText = "Save candidate",
            CloseButtonText = "Cancel",
            DefaultButton = ContentDialogButton.Primary,
        };
        var result = await dialog.ShowAsync();
        if (result != ContentDialogResult.Primary)
        {
            return;
        }

        var candidate = addressBox.Text.Trim();
        if (!LocalEndpointValidator.TryValidate(candidate, out var reason))
        {
            WriteLog("Warning", "pairing.address_rejected", "A manual local endpoint candidate was rejected.");
            HomeInfoBar.Title = "Address not accepted";
            HomeInfoBar.Message = reason;
            HomeInfoBar.Severity = InfoBarSeverity.Error;
            HomeInfoBar.IsOpen = true;
            return;
        }

        HomeInfoBar.Title = "Candidate saved for this session";
        HomeInfoBar.Message = "Secure identity exchange is not implemented yet, so the app did not connect or claim the peer as paired.";
        HomeInfoBar.Severity = InfoBarSeverity.Warning;
        HomeInfoBar.IsOpen = true;
        WriteLog("Info", "pairing.candidate_saved", "A validated local endpoint candidate was saved for this session.");
    }

    private void OnTransportChanged(object sender, SelectionChangedEventArgs args)
    {
        if (TransportSelector.SelectedIndex == 1)
        {
            SetupGuideText.Text = "1  Connect your iPhone/iPad with a data cable and unlock it.\n2  Accept Trust This Computer when prompted.\n3  Choose Check iPhone USB. A detected device is not yet a mirroring session.";
            UpdateDiagnostics();
            return;
        }
        SetupGuideText.Text = "1  Connect this PC and your device to the same local network.\n2  Open the UNKXWYNO companion on your device.\n3  Find and approve this PC when it appears.";
        UpdateDiagnostics();
    }

    private async void OnCheckUsb(object sender, RoutedEventArgs args)
    {
        if (_usbMirror is not null)
        {
            _usbMirror.Stop();
            return;
        }
        if (_usbStarting) return;
        _usbStarting = true;
        UsbStatusText.Text = "Starting iPhone USB capture...";
        UsbMirrorSession? mirror = null;
        try
        {
            await UsbDriverLease.RecoverPendingAsync(message => DispatcherQueue.TryEnqueue(() => { if (!_windowClosed) UsbStatusText.Text = message; }), _windowLifetime.Token);
            if (_windowClosed) return;
            // These are display-size negotiation hints, not claims about delivered
            // dimensions or FPS. Live status comes only from displayed frames.
            var dimensions = _quality.RequestedResolution switch
            {
                ResolutionChoice.P480 => (854u, 480u),
                ResolutionChoice.Hd => (1280u, 720u),
                ResolutionChoice.FullHd => (1920u, 1080u),
                ResolutionChoice.Qhd => (2560u, 1440u),
                _ => (3840u, 2160u),
            };
            // NEED is a paced request to keep the phone's video stream moving,
            // not an encoder FPS negotiation. Auto uses the common 60 Hz target;
            // the status window reports the actual device and displayed rates.
            var targetFps = _quality.RequestedFrameRate == FrameRateChoice.Auto
                ? 60u : (uint)_quality.RequestedFrameRate;
            mirror = new UsbMirrorSession(() => new NativeUsbMirror(dimensions.Item1, dimensions.Item2, targetFps: targetFps), UsbDriverLease.AcquireAsync);
            _usbMirror = mirror;
            mirror.StatusChanged += status => DispatcherQueue.TryEnqueue(() =>
            {
                if (_windowClosed || !ReferenceEquals(_usbMirror, mirror)) return;
                _usbStatus = status;
                UsbStatusText.Text = status.State == 2
                    ? $"USB: {status.Width} × {status.Height} · {status.DisplayedFps:F1} displayed FPS · {status.DisplayedFrames} frames. {status.Message}"
                    : status.Message;
                EngineStatusText.Text = status.State == 2 ? "Independent USB mirroring active" : "Independent USB capture";
                if (status.State == 3)
                {
                    HomeInfoBar.Title = "iPhone USB capture could not start";
                    HomeInfoBar.Message = status.Message;
                    HomeInfoBar.Severity = InfoBarSeverity.Error;
                    HomeInfoBar.IsOpen = true;
                    WriteLog("Error", "usb.capture_failed", status.Message);
                }
                else if (status.State == 1) WriteLog("Info", "usb.capture_progress", status.Message);
                else if (status.State == 2 && DateTime.UtcNow - _lastUsbMetricsLog > TimeSpan.FromSeconds(5))
                {
                    _lastUsbMetricsLog = DateTime.UtcNow;
                    WriteLog("Info", "usb.capture_metrics", status.Message);
                }
                UpdateQualityStatus();
            });
            CheckUsbButton.Content = "Stop iPhone USB";
            CompactCheckUsbButton.Content = "Stop iPhone USB";
            HomeInfoBar.IsOpen = false;
            var result = await mirror.RunAsync();
            if (!_windowClosed)
            {
                _usbStatus = mirror.LastStatus;
                if (_usbStatus is not null) UsbStatusText.Text = _usbStatus.Message;
                if (_usbStatus?.State == 3)
                {
                    HomeInfoBar.Title = "iPhone USB capture could not start";
                    HomeInfoBar.Message = _usbStatus.Message;
                    HomeInfoBar.Severity = InfoBarSeverity.Error;
                    HomeInfoBar.IsOpen = true;
                }
                EngineStatusText.Text = "Independent USB capture stopped";
                UpdateQualityStatus();
                WriteLog(result == 0 ? "Info" : "Warning", "usb.capture_ended", $"Native capture exited with result {result}.");
            }
        }
        catch (OperationCanceledException) when (_windowClosed) { }
        catch (Exception exception) when (exception is DllNotFoundException or EntryPointNotFoundException or BadImageFormatException or InvalidOperationException or IOException or System.ComponentModel.Win32Exception or System.Text.Json.JsonException)
        {
            if (!_windowClosed)
            {
                UsbStatusText.Text = exception.Message;
                HomeInfoBar.Title = "iPhone USB setup could not finish";
                HomeInfoBar.Message = exception.Message;
                HomeInfoBar.Severity = InfoBarSeverity.Error;
                HomeInfoBar.IsOpen = true;
                WriteLog("Error", "usb.capture_load_failed", exception.Message);
            }
        }
        finally
        {
            mirror?.Dispose();
            _usbMirror = null;
            _usbStarting = false;
            if (!_windowClosed)
            {
                CheckUsbButton.Content = "Mirror iPhone USB";
                CompactCheckUsbButton.Content = "Mirror iPhone USB";
                UpdateQualityStatus();
            }
        }
    }

    private async void OnViewCompatibility(object sender, RoutedEventArgs args)
    {
        var probe = IndependentCoreProbe.Probe();
        _lastCoreProbe = probe;
        var dialog = new ContentDialog
        {
            XamlRoot = Content.XamlRoot,
            Title = "Current Windows capability",
            Content = new TextBlock
            {
                Text = $"Core: {(probe.Available ? probe.Version : "not loaded")}\nLocal discovery: available\nSecure pairing: unavailable\nWindows capture: unavailable\nHardware encode/decode: unprobed\nUSB: unavailable\n\n{probe.Explanation}",
                TextWrapping = TextWrapping.Wrap,
                MaxWidth = 520,
            },
            CloseButtonText = "Close",
        };
        await dialog.ShowAsync();
    }

    private async void OnCompactConnectionGuide(object sender, RoutedEventArgs args)
    {
        var dialog = new ContentDialog
        {
            XamlRoot = Content.XamlRoot,
            Title = "Connection guide",
            Content = new TextBlock
            {
                Text = "Local Wi-Fi\nConnect this PC and your device to the same local network, open the UNKXWYNO companion, then approve this PC.\n\nUSB\nConnect the iPhone by a data cable, unlock it, and leave the screen awake. Close iTunes and Apple Devices. Choose Mirror iPhone USB and accept the Windows administrator prompt if temporary capture-driver access is needed. The app restores the Apple driver when capture ends. Accept Trust on the iPhone if prompted. Live capture compatibility is still under testing.",
                TextWrapping = TextWrapping.Wrap,
                MaxWidth = 520,
            },
            CloseButtonText = "Close",
        };
        await dialog.ShowAsync();
    }

    private void OnQualityChanged(object sender, SelectionChangedEventArgs args)
    {
        if (ResolutionCombo.SelectedItem is ChoiceItem<ResolutionChoice> resolution)
        {
            _quality.RequestedResolution = resolution.Value;
        }
        if (FrameRateCombo.SelectedItem is ChoiceItem<FrameRateChoice> frameRate)
        {
            _quality.RequestedFrameRate = frameRate.Value;
        }
        _quality.ClearActive("No active stream. The requested profile will be checked against every real endpoint stage before activation.");
        if (!_loadingSettings)
        {
            if (_settingsStore.TrySave(DesktopSettings.FromSelection(_quality), out var saveError))
            {
                WriteLog("Info", "settings.saved", "Requested quality settings saved.");
                _settingsWarning = null;
            }
            else
            {
                _settingsWarning = saveError;
                HomeInfoBar.Title = "Settings not saved";
                HomeInfoBar.Message = saveError;
                HomeInfoBar.Severity = InfoBarSeverity.Warning;
                HomeInfoBar.IsOpen = true;
                WriteLog("Warning", "settings.save_failed", saveError ?? "Settings save failed.");
            }
        }
        UpdateQualityStatus();
    }

    private void UpdateQualityStatus()
    {
        var streaming = _usbStatus?.State == 2;
        var targetFps = _quality.RequestedFrameRate == FrameRateChoice.Auto
            ? 60u : (uint)_quality.RequestedFrameRate;
        MirrorHeadingText.Text = streaming ? "iPhone USB mirroring" : _usbMirror is not null ? "Preparing iPhone USB" : "No active stream";
        MirrorDetailText.Text = streaming ? "Your iPhone screen is open in the USB viewer." : _usbMirror is not null ? "USB connection status appears below." : "Choose a local device to start mirroring.";
        RequestedStatusText.Text = _quality.RequestedLabel;
        ActualStatusText.Text = _usbStatus?.State == 2
            ? $"{_usbStatus.Width} × {_usbStatus.Height} · {_usbStatus.DisplayedFps:F1} displayed FPS"
            : _quality.ActiveLabel;
        if (CapabilityInfoBar is not null)
        {
            CapabilityInfoBar.Message = _usbMirror is not null
                ? $"USB resolution is a source negotiation hint. The {targetFps} FPS NEED cadence requests fresh video; the iPhone may deliver a lower rate. The viewer reports device PTS FPS, displayed FPS, decode+convert time, and sample-to-paint age."
                : _quality.Limitation;
        }
        UpdateDiagnostics();
    }

    private void OnPeerAdded(object? sender, DiscoveredPeer peer) => DispatcherQueue.TryEnqueue(() =>
    {
        if (_peers.All(existing => existing.Id != peer.Id))
        {
            _peers.Add(peer);
        }
        UpdateDeviceEmptyState();
        HomeInfoBar.Title = "Local companion discovered";
        HomeInfoBar.Message = $"{peer.DisplayName} at {peer.Host}:{peer.Port}. It is not paired or trusted yet.";
        HomeInfoBar.Severity = InfoBarSeverity.Success;
        HomeInfoBar.IsOpen = true;
        WriteLog("Info", "discovery.peer_found", "A local untrusted companion candidate was discovered.");
        UpdateDiagnostics();
    });

    private void OnPeerRemoved(object? sender, string id) => DispatcherQueue.TryEnqueue(() =>
    {
        var peer = _peers.FirstOrDefault(item => item.Id == id);
        if (peer is not null)
        {
            _peers.Remove(peer);
        }
        UpdateDeviceEmptyState();
        UpdateDiagnostics();
    });

    private void OnEnumerationCompleted(object? sender, EventArgs args) => DispatcherQueue.TryEnqueue(() =>
    {
        if (_peers.Count == 0)
        {
            HomeInfoBar.Title = "No companion found";
            HomeInfoBar.Message = "Keep both devices on the same local network and open the UNKXWYNO companion. Discovery remains local.";
            HomeInfoBar.Severity = InfoBarSeverity.Informational;
            HomeInfoBar.IsOpen = true;
        }
        UpdateDiagnostics();
    });

    private void OnDiscoveryError(object? sender, string message) =>
        DispatcherQueue.TryEnqueue(() => ShowDiscoveryError(message));

    private void ShowDiscoveryError(string message)
    {
        HomeInfoBar.Title = "Discovery unavailable";
        HomeInfoBar.Message = message;
        HomeInfoBar.Severity = InfoBarSeverity.Error;
        HomeInfoBar.IsOpen = true;
        WriteLog("Error", "discovery.failed", "Local DNS-SD discovery stopped with an error.");
        UpdateDiagnostics();
    }

    private void UpdateDeviceEmptyState()
    {
        EmptyDevicesText.Visibility = _peers.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
    }

    private void OnRootSizeChanged(object sender, SizeChangedEventArgs args) =>
        UpdateResponsiveLayout();

    private void UpdateResponsiveLayout()
    {
        if (ConnectionPanel is null || CompactConnectionButton is null)
        {
            return;
        }
        var showConnectionPanel = _currentDestination == "Home" && RootLayout.ActualWidth >= 1120;
        ConnectionPanel.Visibility = showConnectionPanel ? Visibility.Visible : Visibility.Collapsed;
        ConnectionColumn.Width = showConnectionPanel ? new GridLength(320) : new GridLength(0);
        CompactConnectionButton.Visibility = _currentDestination == "Home" && !showConnectionPanel
            ? Visibility.Visible
            : Visibility.Collapsed;
        CompactCheckUsbButton.Visibility = CompactConnectionButton.Visibility;
    }

    private void OnOpenCaptureFolder(object sender, RoutedEventArgs args)
    {
        var pictures = Environment.GetFolderPath(Environment.SpecialFolder.MyPictures);
        var folder = Path.Combine(pictures, "UNKXWYNO Mirror");
        Directory.CreateDirectory(folder);
        Process.Start(new ProcessStartInfo("explorer.exe", folder) { UseShellExecute = true });
    }

    private void OnRefreshDiagnostics(object sender, RoutedEventArgs args) =>
        UpdateDiagnostics(refreshCore: true);

    private void OnOpenLogFolder(object sender, RoutedEventArgs args)
    {
        try
        {
            Directory.CreateDirectory(_diagnosticsLog.LogDirectory);
            Process.Start(new ProcessStartInfo("explorer.exe", _diagnosticsLog.LogDirectory)
            {
                UseShellExecute = true,
            });
        }
        catch (Exception exception) when (exception is IOException or UnauthorizedAccessException or InvalidOperationException or System.ComponentModel.Win32Exception)
        {
            HomeInfoBar.Title = "Log folder unavailable";
            HomeInfoBar.Message = "Windows could not open the local diagnostics folder.";
            HomeInfoBar.Severity = InfoBarSeverity.Error;
            HomeInfoBar.IsOpen = true;
        }
    }

    private void UpdateDiagnostics(bool refreshCore = false)
    {
        if (DiagnosticsBuildText is null)
        {
            return;
        }
        if (refreshCore || _lastCoreProbe is null)
        {
            _lastCoreProbe = IndependentCoreProbe.Probe();
        }

        var assembly = typeof(MainWindow).Assembly;
        var build = assembly.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion
            ?? assembly.GetName().Version?.ToString()
            ?? "unknown";
        DiagnosticsBuildText.Text = $"{build} · {RuntimeInformation.ProcessArchitecture}";
        DiagnosticsCoreText.Text = _lastCoreProbe.Available
            ? $"Loaded {_lastCoreProbe.Version}, ABI {_lastCoreProbe.AbiVersion}. Media adapters remain capability-gated."
            : $"Not loaded. {_lastCoreProbe.Explanation}";
        DiagnosticsDiscoveryText.Text = _discovery.IsRunning
            ? $"Searching the local network · {_peers.Count} untrusted candidate(s) visible"
            : $"Stopped · {_peers.Count} untrusted candidate(s) retained";
        DiagnosticsQualityText.Text = _quality.ActiveLabel == "—"
            ? $"Requested: {_quality.RequestedLabel}. Actual: no active stream."
            : $"Requested: {_quality.RequestedLabel}. Actual: {_quality.ActiveLabel}.";
        if (_usbStatus?.State == 2)
            DiagnosticsQualityText.Text = $"Requested: {_quality.RequestedLabel}. USB actual: {_usbStatus.Width} × {_usbStatus.Height}, {_usbStatus.DisplayedFps:F1} displayed FPS, {_usbStatus.DisplayedFrames} displayed frames. Windows H.264 decoder with runtime SIMD NV12 conversion; latest-frame software rendering. Host sample-to-paint time appears in the USB status; phone capture-to-display latency, bandwidth and source frame drops remain unmeasured.";
        DiagnosticsSettingsPathText.Text = _settingsStore.SettingsPath;
        DiagnosticsLogPathText.Text = _diagnosticsLog.LogPath;
    }

    private void WriteLog(string level, string eventName, string message) =>
        _diagnosticsLog.TryWrite(level, eventName, message, out _);

    private void OnClosed(object sender, WindowEventArgs args)
    {
        _windowClosed = true;
        _usbMirror?.Stop();
        _windowLifetime.Cancel();
        _windowLifetime.Dispose();
        WriteLog("Info", "app.stopped", "Desktop window closed normally.");
        _discovery.PeerAdded -= OnPeerAdded;
        _discovery.PeerRemoved -= OnPeerRemoved;
        _discovery.EnumerationCompleted -= OnEnumerationCompleted;
        _discovery.DiscoveryError -= OnDiscoveryError;
        _discovery.Dispose();
    }
}


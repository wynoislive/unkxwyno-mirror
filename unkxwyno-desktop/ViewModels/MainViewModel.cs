/**
 * @file MainViewModel.cs
 * @brief ViewModel for unkXwyno Pro Gaming Studio (High-Taste Modern UI).
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Media.Imaging;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Unkxwyno.Desktop.Interop;
using Unkxwyno.Desktop.Models;
using Unkxwyno.Desktop.Services;

namespace Unkxwyno.Desktop.ViewModels
{
    public partial class MainViewModel : ObservableObject
    {
        private readonly MirrorEngineService _engineService;

        // Navigation Tabs: 0: Home, 1: Profiles, 2: Keymapper, 3: Optimization, 4: Settings
        [ObservableProperty]
        private int _selectedTabIndex = 0;

        public bool IsHomeTab => SelectedTabIndex == 0;
        public bool IsProfilesTab => SelectedTabIndex == 1;
        public bool IsKeymapperTab => SelectedTabIndex == 2;
        public bool IsOptimizationTab => SelectedTabIndex == 3;
        public bool IsSettingsTab => SelectedTabIndex == 4;

        partial void OnSelectedTabIndexChanged(int value)
        {
            OnPropertyChanged(nameof(IsHomeTab));
            OnPropertyChanged(nameof(IsProfilesTab));
            OnPropertyChanged(nameof(IsKeymapperTab));
            OnPropertyChanged(nameof(IsOptimizationTab));
            OnPropertyChanged(nameof(IsSettingsTab));
        }

        // Connection Modes: 0: Cable USB, 1: Wireless Wi-Fi, 2: Apple AirPlay
        [ObservableProperty]
        private int _selectedConnectionMode = 0;

        public bool IsWiredMode => SelectedConnectionMode == 0;
        public bool IsWirelessMode => SelectedConnectionMode == 1;
        public bool IsAirPlayMode => SelectedConnectionMode == 2;

        partial void OnSelectedConnectionModeChanged(int value)
        {
            OnPropertyChanged(nameof(IsWiredMode));
            OnPropertyChanged(nameof(IsWirelessMode));
            OnPropertyChanged(nameof(IsAirPlayMode));
        }

        [ObservableProperty]
        private string _statusText = "Ready - Connect device via USB Cable, Wi-Fi, or iOS AirPlay";

        [ObservableProperty]
        private string _deviceBadgeText = "AOA 2.0 / Wi-Fi / AirPlay Ready";

        [ObservableProperty]
        private string _connectedDeviceModel = "Auto-Scanning USB Ports...";

        [ObservableProperty]
        private string _connectedDeviceManufacturer = "Android / Apple Device";

        [ObservableProperty]
        private string _usbConnectionStatus = "AOA 2.0 Plug-and-Play Ready";

        [ObservableProperty]
        private string _localIpAddress = "192.168.1.4";

        [ObservableProperty]
        private int _streamPort = 8099;

        [ObservableProperty]
        private int _airPlayPort = 7000;

        [ObservableProperty]
        private string _copyStatusText = "Copy IP:Port";

        [ObservableProperty]
        private Bitmap? _qrCodeBitmap;

        [ObservableProperty]
        private bool _isStreaming;

        [ObservableProperty]
        private string _streamingBtnText = "START ULTRA-FAST MIRRORING";

        [ObservableProperty]
        private bool _isQrModalOpen = false;

        [ObservableProperty]
        private bool _isScreenOff = false;

        [ObservableProperty]
        private string _screenOffBtnText = "Turn Screen Off";

        [ObservableProperty]
        private string _fpsText = "0.0 FPS";

        [ObservableProperty]
        private string _bitrateText = "0.0 Mbps";

        [ObservableProperty]
        private string _decodeLatencyText = "0.0 ms";

        [ObservableProperty]
        private string _renderLatencyText = "0.0 ms";

        [ObservableProperty]
        private string _audioBufferText = "0.0 ms";

        [ObservableProperty]
        private string _decoderName = "D3D11VA Direct";

        [ObservableProperty]
        private string _framesRenderedText = "0 frames";

        [ObservableProperty]
        private bool _isAudioEnabled = true;

        [ObservableProperty]
        private bool _isTouchInputEnabled = true;

        [ObservableProperty]
        private uint _selectedFps = 60;

        [ObservableProperty]
        private uint _selectedBitrateMbps = 25;

        // Keymapper & FreeLook settings
        [ObservableProperty]
        private string _activeProfileName = "Standard Battle Royale (Free Fire / PUBG)";

        [ObservableProperty]
        private float _aimSensitivityX = 1.0f;

        [ObservableProperty]
        private float _aimSensitivityY = 1.0f;

        [ObservableProperty]
        private string _aimToggleKey = "F1";

        [ObservableProperty]
        private bool _isKeymapHudEnabled = true;

        public event Action? OnRequestOpenKeymapper;
        public event Action? OnRequestShowToolbar;
        public event Action? OnRequestPickBlueStacksConfig;

        public MainViewModel(MirrorEngineService engineService)
        {
            _engineService = engineService;

            // Auto-detect Preferred Local LAN IP address
            try
            {
                LocalIpAddress = QrCodeService.GetPreferredLocalIpAddress();
                QrCodeBitmap = QrCodeService.GenerateQrBitmap($"unkxwyno://connect?ip={LocalIpAddress}&port={StreamPort}");
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[QR] Generation exception: {ex.Message}");
            }

            _engineService.OnDeviceStateChanged += (dev, state) =>
            {
                StatusText = state switch
                {
                    UnkxwynoDeviceState.Connected => $"Connected: {dev.ModelName} ({dev.Manufacturer})",
                    UnkxwynoDeviceState.Streaming => $"Streaming: {dev.ModelName} (Ultra-Low Latency)",
                    UnkxwynoDeviceState.Connecting => "Negotiating AOA 2.0 Accessory Handshake...",
                    UnkxwynoDeviceState.Error => "USB Transmission Error",
                    _ => "Ready - Connect phone via USB Cable, Wi-Fi, or AirPlay"
                };

                ConnectedDeviceModel = state switch
                {
                    UnkxwynoDeviceState.Connected => dev.ModelName,
                    UnkxwynoDeviceState.Streaming => dev.ModelName,
                    _ => "Auto-Scanning USB Ports..."
                };

                ConnectedDeviceManufacturer = state switch
                {
                    UnkxwynoDeviceState.Connected => dev.Manufacturer,
                    UnkxwynoDeviceState.Streaming => dev.Manufacturer,
                    _ => "Android / Apple Device"
                };

                UsbConnectionStatus = state switch
                {
                    UnkxwynoDeviceState.Connected => "Device Attached & Authorized",
                    UnkxwynoDeviceState.Streaming => "Streaming Active (Hardware D3D11)",
                    _ => "AOA 2.0 Plug-and-Play Ready"
                };

                DeviceBadgeText = state switch
                {
                    UnkxwynoDeviceState.Connected => $"{dev.ModelName} [USB AOA]",
                    UnkxwynoDeviceState.Streaming => $"{dev.ModelName} [LIVE]",
                    _ => "Auto-Detecting Devices..."
                };
            };

            _engineService.OnMetricsUpdated += (metrics) =>
            {
                FpsText = $"{metrics.CurrentFps:F1} FPS";
                BitrateText = $"{(metrics.BitrateBps / 1_000_000.0f):F1} Mbps";
                DecodeLatencyText = $"{metrics.DecodeLatencyMs:F1} ms";
                RenderLatencyText = $"{metrics.RenderLatencyMs:F1} ms";
                AudioBufferText = $"{metrics.AudioBufferMs:F1} ms";
                FramesRenderedText = $"{metrics.TotalFramesRendered:N0} frames";
            };

            _engineService.OnLogReceived += (level, msg) =>
            {
                Console.WriteLine($"[Core {level}] {msg}");
            };
        }

        [RelayCommand]
        public void SelectTab(int index)
        {
            SelectedTabIndex = index;
        }

        [RelayCommand]
        public void GoToHome() => SelectedTabIndex = 0;

        [RelayCommand]
        public void GoToProfiles() => SelectedTabIndex = 1;

        [RelayCommand]
        public void GoToKeymapper() => SelectedTabIndex = 2;

        [RelayCommand]
        public void GoToOptimization() => SelectedTabIndex = 3;

        [RelayCommand]
        public void GoToSettings() => SelectedTabIndex = 4;

        [RelayCommand]
        public void SelectWiredMode() => SelectedConnectionMode = 0;

        [RelayCommand]
        public void SelectWirelessMode() => SelectedConnectionMode = 1;

        [RelayCommand]
        public void SelectAirPlayMode() => SelectedConnectionMode = 2;

        [RelayCommand]
        public void ToggleQrModal()
        {
            IsQrModalOpen = !IsQrModalOpen;
        }

        [RelayCommand]
        public void ToggleScreenOff()
        {
            IsScreenOff = !IsScreenOff;
            ScreenOffBtnText = IsScreenOff ? "Screen Off (Active)" : "Turn Screen Off";
            _engineService.SetScreenPower(!IsScreenOff);
        }

        [RelayCommand]
        public void ToggleStreaming()
        {
            if (IsStreaming)
            {
                _engineService.Stop();
                _engineService.ShowViewport(false);
                IsStreaming = false;
                StreamingBtnText = "START ULTRA-FAST MIRRORING";
                StatusText = "Streaming Stopped.";
            }
            else
            {
                var cfg = UnkxwynoConfig.Default;
                cfg.TargetFps = SelectedFps;
                cfg.BitrateBps = SelectedBitrateMbps * 1_000_000;
                cfg.EnableAudio = (byte)(IsAudioEnabled ? 1 : 0);
                cfg.EnableTouchInput = (byte)(IsTouchInputEnabled ? 1 : 0);

                if (_engineService.Initialize(cfg))
                {
                    _engineService.CreateViewport("unkXwyno Mirror - Low Latency Display", 1920, 1080);
                    _engineService.ShowViewport(true);
                    _engineService.Start();
                    IsStreaming = true;
                    StreamingBtnText = "STOP ACTIVE STREAM";
                    StatusText = "Engine Active - Mirroring Display";

                    // Apply in-shader keymap badges if enabled
                    if (IsKeymapHudEnabled)
                    {
                        var profile = KeymapProfile.CreateDefaultFpsProfile();
                        var badges = BlueStacksConfigParser.ToNativeBadges(profile);
                        _engineService.SetKeymapHud(true, badges);
                    }

                    OnRequestShowToolbar?.Invoke();
                }
                else
                {
                    StatusText = "Failed to initialize native core engine.";
                }
            }
        }

        [RelayCommand]
        public void ToggleFullscreen()
        {
            _engineService.ToggleFullscreen();
        }

        [RelayCommand]
        public void OpenKeymapper()
        {
            OnRequestOpenKeymapper?.Invoke();
        }

        [RelayCommand]
        public void RequestImportConfig()
        {
            OnRequestPickBlueStacksConfig?.Invoke();
        }

        public void LoadBlueStacksConfigFile(string path)
        {
            if (File.Exists(path))
            {
                var profile = BlueStacksConfigParser.Parse(path);
                ActiveProfileName = profile.ProfileName;
                AimSensitivityX = profile.AimSensitivityX;
                AimSensitivityY = profile.AimSensitivityY;
                AimToggleKey = profile.AimToggleKey;

                var badges = BlueStacksConfigParser.ToNativeBadges(profile);
                _engineService.SetKeymapHud(true, badges);
            }
        }
    }
}

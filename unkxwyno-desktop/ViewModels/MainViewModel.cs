/**
 * @file MainViewModel.cs
 * @brief ViewModel for unkXwyno Pro Gaming Studio & Telemetry Dashboard.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using System.IO;
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

        [ObservableProperty]
        private int _selectedTabIndex = 0; // 0: Home, 1: Profiles, 2: Keymapper, 3: Optimization, 4: Settings

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

        [ObservableProperty]
        private string _statusText = "Ready - Connect device via USB or Wi-Fi";

        [ObservableProperty]
        private string _deviceBadgeText = "USB / Wi-Fi / AirPlay Ready";

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

            _engineService.OnDeviceStateChanged += (dev, state) =>
            {
                StatusText = state switch
                {
                    UnkxwynoDeviceState.Connected => $"Connected: {dev.ModelName} ({dev.Manufacturer})",
                    UnkxwynoDeviceState.Streaming => $"Streaming: {dev.ModelName} (Ultra-Low Latency)",
                    UnkxwynoDeviceState.Connecting => "Negotiating AOA 2.0 Accessory Handshake...",
                    UnkxwynoDeviceState.Error => "USB Transmission Error",
                    _ => "Ready - Connect phone via USB or Wi-Fi"
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
                    StreamingBtnText = "STOP MIRRORING";
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

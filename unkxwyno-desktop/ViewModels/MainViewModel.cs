/**
 * @file MainViewModel.cs
 * @brief ViewModel for the unkXwyno floating control dock and telemetry HUD.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using CommunityToolkit.Mvvm.ComponentModel;
using CommunityToolkit.Mvvm.Input;
using Unkxwyno.Desktop.Interop;
using Unkxwyno.Desktop.Services;

namespace Unkxwyno.Desktop.ViewModels
{
    public partial class MainViewModel : ObservableObject
    {
        private readonly MirrorEngineService _engineService;

        [ObservableProperty]
        private string _statusText = "Ready - Connect phone via USB";

        [ObservableProperty]
        private bool _isStreaming;

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
        private string _framesRenderedText = "0 frames";

        [ObservableProperty]
        private bool _isAudioEnabled = true;

        [ObservableProperty]
        private bool _isTouchInputEnabled = true;

        [ObservableProperty]
        private uint _selectedFps = 60;

        [ObservableProperty]
        private uint _selectedBitrateMbps = 25;

        public event Action? OnRequestOpenKeymapper;

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
                    _ => "Disconnected - Connect phone via USB"
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
        public void ToggleStreaming()
        {
            if (IsStreaming)
            {
                _engineService.Stop();
                _engineService.ShowViewport(false);
                IsStreaming = false;
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
                    StatusText = "Engine Active - Mirroring Display";
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
    }
}

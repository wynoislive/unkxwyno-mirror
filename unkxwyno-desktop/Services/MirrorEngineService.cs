/**
 * @file MirrorEngineService.cs
 * @brief High-level C# service managing native unkXwyno Core lifecycle and telemetry events.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using System.Timers;
using Unkxwyno.Desktop.Interop;

namespace Unkxwyno.Desktop.Services
{
    public class MirrorEngineService : IDisposable
    {
        private IntPtr _engineHandle = IntPtr.Zero;
        private readonly System.Timers.Timer _metricsTimer;

        // Native callback anchors (prevent GC collection)
        private readonly StateCallback _stateCallback;
        private readonly MetricsCallback _metricsCallback;
        private readonly LogCallback _logCallback;

        public event Action<UnkxwynoDeviceInfo, UnkxwynoDeviceState>? OnDeviceStateChanged;
        public event Action<UnkxwynoMetrics>? OnMetricsUpdated;
        public event Action<UnkxwynoLogLevel, string>? OnLogReceived;

        public bool IsRunning { get; private set; }
        public bool IsViewportVisible { get; private set; }
        public bool IsFullscreen { get; private set; }

        public MirrorEngineService()
        {
            _stateCallback = (in UnkxwynoDeviceInfo dev, UnkxwynoDeviceState state, IntPtr _) =>
            {
                OnDeviceStateChanged?.Invoke(dev, state);
            };

            _metricsCallback = (in UnkxwynoMetrics metrics, IntPtr _) =>
            {
                OnMetricsUpdated?.Invoke(metrics);
            };

            _logCallback = (UnkxwynoLogLevel level, string msg, IntPtr _) =>
            {
                OnLogReceived?.Invoke(level, msg);
            };

            _metricsTimer = new System.Timers.Timer(500); // 2Hz telemetry polling
            _metricsTimer.Elapsed += (s, e) => PollMetrics();
        }

        public bool Initialize(UnkxwynoConfig config)
        {
            Shutdown();

            var status = UnkxwynoNative.unkxwyno_engine_create(in config, out _engineHandle);
            if (status != UnkxwynoStatus.Success || _engineHandle == IntPtr.Zero)
            {
                return false;
            }

            UnkxwynoNative.unkxwyno_set_callbacks(_engineHandle, _stateCallback, _metricsCallback, _logCallback, IntPtr.Zero);
            return true;
        }

        public bool Start()
        {
            if (_engineHandle == IntPtr.Zero) return false;

            var status = UnkxwynoNative.unkxwyno_start(_engineHandle);
            if (status == UnkxwynoStatus.Success)
            {
                IsRunning = true;
                _metricsTimer.Start();
                return true;
            }
            return false;
        }

        public void Stop()
        {
            _metricsTimer.Stop();
            if (_engineHandle != IntPtr.Zero && IsRunning)
            {
                UnkxwynoNative.unkxwyno_stop(_engineHandle);
                IsRunning = false;
            }
        }

        public bool CreateViewport(string title = "unkXwyno Mirror - Low Latency Display", int width = 1920, int height = 1080)
        {
            if (_engineHandle == IntPtr.Zero) return false;
            var status = UnkxwynoNative.unkxwyno_viewport_create(_engineHandle, title, width, height);
            IsViewportVisible = (status == UnkxwynoStatus.Success);
            return IsViewportVisible;
        }

        public void ShowViewport(bool visible)
        {
            if (_engineHandle != IntPtr.Zero)
            {
                UnkxwynoNative.unkxwyno_viewport_show(_engineHandle, visible);
                IsViewportVisible = visible;
            }
        }

        public void ToggleFullscreen()
        {
            if (_engineHandle != IntPtr.Zero)
            {
                IsFullscreen = !IsFullscreen;
                UnkxwynoNative.unkxwyno_viewport_set_fullscreen(_engineHandle, IsFullscreen);
            }
        }

        public IntPtr GetNativeViewportHwnd()
        {
            return _engineHandle != IntPtr.Zero 
                ? UnkxwynoNative.unkxwyno_viewport_get_native_handle(_engineHandle) 
                : IntPtr.Zero;
        }

        public void InjectTouch(byte pointerId, byte action, ushort x, ushort y, ushort pressure = 512)
        {
            if (_engineHandle != IntPtr.Zero)
            {
                UnkxwynoNative.unkxwyno_inject_touch(_engineHandle, pointerId, action, x, y, pressure);
            }
        }

        public void InjectKey(ushort keyCode, bool isDown, byte modifiers = 0)
        {
            if (_engineHandle != IntPtr.Zero)
            {
                UnkxwynoNative.unkxwyno_inject_key(_engineHandle, keyCode, isDown, modifiers);
            }
        }

        private void PollMetrics()
        {
            if (_engineHandle != IntPtr.Zero && IsRunning)
            {
                if (UnkxwynoNative.unkxwyno_get_metrics(_engineHandle, out var metrics) == UnkxwynoStatus.Success)
                {
                    OnMetricsUpdated?.Invoke(metrics);
                }
            }
        }

        public void Shutdown()
        {
            Stop();
            if (_engineHandle != IntPtr.Zero)
            {
                UnkxwynoNative.unkxwyno_engine_destroy(_engineHandle);
                _engineHandle = IntPtr.Zero;
            }
        }

        public void Dispose()
        {
            _metricsTimer.Dispose();
            Shutdown();
            GC.SuppressFinalize(this);
        }
    }
}

/**
 * @file IMirrorEngineService.cs
 * @brief Interface abstraction for unkXwyno native engine lifecycle and telemetry.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using Unkxwyno.Desktop.Interop;

namespace Unkxwyno.Desktop.Services
{
    public interface IMirrorEngineService : IDisposable
    {
        event Action<UnkxwynoDeviceInfo, UnkxwynoDeviceState>? OnDeviceStateChanged;
        event Action<UnkxwynoMetrics>? OnMetricsUpdated;
        event Action<UnkxwynoLogLevel, string>? OnLogReceived;

        bool IsRunning { get; }
        bool IsViewportVisible { get; }
        bool IsFullscreen { get; }

        bool Initialize(UnkxwynoConfig config);
        bool Start();
        void Stop();
        bool CreateViewport(string title = "unkXwyno Mirror - Low Latency Display", int width = 1920, int height = 1080);
        void ShowViewport(bool visible);
        void ToggleFullscreen();
        IntPtr GetNativeViewportHwnd();
        bool GetViewportBounds(out int x, out int y, out int width, out int height);
        bool SetKeymapHud(bool visible, UnkxwynoKeymapBadge[]? badges);
        bool SetScreenPower(bool powerOn);
        void InjectTouch(byte pointerId, byte action, ushort x, ushort y, ushort pressure = 512);
        void InjectKey(ushort keyCode, bool isDown, byte modifiers = 0);
        void InjectMouse(short deltaX, short deltaY, sbyte wheelV = 0, sbyte wheelH = 0, byte buttons = 0);
        bool GetMetrics(out UnkxwynoMetrics metrics);
        void Shutdown();
    }
}

/**
 * @file UnkxwynoCore.cs
 * @brief High-performance C# .NET 9 P/Invoke interop bindings for unkXwyno Core C ABI.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using System.Runtime.InteropServices;

namespace Unkxwyno.Desktop.Interop
{
    public enum UnkxwynoStatus : int
    {
        Success = 0,
        ErrInvalidParam = -1,
        ErrOutOfMemory = -2,
        ErrDeviceNotFound = -3,
        ErrConnectionFailed = -4,
        ErrDecoderInit = -5,
        ErrRendererInit = -6,
        ErrAudioInit = -7,
        ErrAlreadyRunning = -8,
        ErrNotRunning = -9,
        ErrUnsupported = -10
    }

    public enum UnkxwynoConnectionMode : byte
    {
        Auto = 0,
        AndroidAoa2 = 1,
        AndroidAdb = 2,
        IosUsb = 3,
        IosWifi = 4
    }

    public enum UnkxwynoDeviceState : byte
    {
        Disconnected = 0,
        Connecting = 1,
        Connected = 2,
        Streaming = 3,
        Error = 4
    }

    public enum UnkxwynoLogLevel : int
    {
        Debug = 0,
        Info = 1,
        Warn = 2,
        Error = 3
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct UnkxwynoConfig
    {
        public uint MaxWidth;
        public uint MaxHeight;
        public uint TargetFps;
        public uint BitrateBps;
        public byte ConnectionMode;
        public byte EnableAudio;
        public byte EnableTouchInput;
        public byte BorderlessViewport;

        public static UnkxwynoConfig Default => new()
        {
            MaxWidth = 1920,
            MaxHeight = 1080,
            TargetFps = 60,
            BitrateBps = 20_000_000,
            ConnectionMode = (byte)UnkxwynoConnectionMode.Auto,
            EnableAudio = 1,
            EnableTouchInput = 1,
            BorderlessViewport = 1
        };
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    public struct UnkxwynoMetrics
    {
        public float CurrentFps;
        public uint BitrateBps;
        public float DecodeLatencyMs;
        public float RenderLatencyMs;
        public float AudioBufferMs;
        public ulong TotalFramesRendered;
        public ulong TotalFramesDropped;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct UnkxwynoDeviceInfo
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string DeviceId;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string ModelName;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string Manufacturer;

        public byte DeviceType;
        public byte ConnectionState;
        public ushort Reserved;
    }

    [StructLayout(LayoutKind.Sequential, Pack = 1, CharSet = CharSet.Ansi)]
    public struct UnkxwynoKeymapBadge
    {
        public float NormX;
        public float NormY;
        public float Radius;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 8)]
        public string Label;
        public uint ColorRgba;
    }

    // Callback Delegates
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void StateCallback(in UnkxwynoDeviceInfo device, UnkxwynoDeviceState state, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void MetricsCallback(in UnkxwynoMetrics metrics, IntPtr userData);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    public delegate void LogCallback(UnkxwynoLogLevel level, [MarshalAs(UnmanagedType.LPStr)] string message, IntPtr userData);

    public static class UnkxwynoNative
    {
        private const string LibName = "unkxwyno_core";

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "unkxwyno_get_version")]
        private static extern IntPtr unkxwyno_get_version();

        public static string GetVersion()
        {
            IntPtr ptr = unkxwyno_get_version();
            return Marshal.PtrToStringAnsi(ptr) ?? "1.0.0";
        }

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_engine_create(
            in UnkxwynoConfig config,
            out IntPtr outEngine
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void unkxwyno_engine_destroy(IntPtr engine);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_set_callbacks(
            IntPtr engine,
            StateCallback? stateCallback,
            MetricsCallback? metricsCallback,
            LogCallback? logCallback,
            IntPtr userData
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_start(IntPtr engine);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_stop(IntPtr engine);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_get_metrics(IntPtr engine, out UnkxwynoMetrics outMetrics);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_viewport_create(
            IntPtr engine,
            [MarshalAs(UnmanagedType.LPStr)] string title,
            int initialWidth,
            int initialHeight
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_viewport_show(IntPtr engine, [MarshalAs(UnmanagedType.I1)] bool visible);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_viewport_set_fullscreen(IntPtr engine, [MarshalAs(UnmanagedType.I1)] bool fullscreen);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr unkxwyno_viewport_get_native_handle(IntPtr engine);

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_viewport_get_bounds(
            IntPtr engine,
            out int outX,
            out int outY,
            out int outWidth,
            out int outHeight
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_set_keymap_hud(
            IntPtr engine,
            [MarshalAs(UnmanagedType.I1)] bool visible,
            [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 3)] UnkxwynoKeymapBadge[]? badges,
            nuint count
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_set_screen_power(
            IntPtr engine,
            [MarshalAs(UnmanagedType.I1)] bool powerOn
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_inject_touch(
            IntPtr engine,
            byte pointerId,
            byte action,
            ushort x,
            ushort y,
            ushort pressure
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_inject_key(
            IntPtr engine,
            ushort keyCode,
            [MarshalAs(UnmanagedType.I1)] bool isDown,
            byte modifiers
        );

        [DllImport(LibName, CallingConvention = CallingConvention.Cdecl)]
        public static extern UnkxwynoStatus unkxwyno_inject_mouse(
            IntPtr engine,
            short deltaX,
            short deltaY,
            sbyte wheelV,
            sbyte wheelH,
            byte buttons
        );
    }
}

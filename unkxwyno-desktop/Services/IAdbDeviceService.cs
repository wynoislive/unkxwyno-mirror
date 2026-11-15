/**
 * @file IAdbDeviceService.cs
 * @brief Interface abstraction for automated ADB transport and device bridge.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Unkxwyno.Desktop.Services
{
    public interface IAdbDeviceService : IDisposable
    {
        event Action<List<AdbDevice>>? OnDevicesUpdated;
        event Action<string>? OnLog;

        List<AdbDevice> ConnectedDevices { get; }

        Task<List<AdbDevice>> RefreshDevicesAsync();
        Task<bool> SetupPortForwardingAsync(string? serial = null, int port = 8099);
        Task<bool> SetupPortReverseAsync(string? serial = null, int port = 8099);
        Task<bool> LaunchCompanionAppAsync(string? serial = null);
    }
}

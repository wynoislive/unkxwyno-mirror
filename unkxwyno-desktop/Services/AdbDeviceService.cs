/**
 * @file AdbDeviceService.cs
 * @brief Automated ADB transport and port-forwarding bridge for plug-and-play Android USB cable mirroring.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;

namespace Unkxwyno.Desktop.Services
{
    public class AdbDevice
    {
        public string Serial { get; set; } = string.Empty;
        public string Model { get; set; } = "Android Device";
        public string Product { get; set; } = string.Empty;
        public string State { get; set; } = "device";
        public bool IsAuthorized => State.Equals("device", StringComparison.OrdinalIgnoreCase);

        public override string ToString() => $"{Model} ({Serial})";
    }

    public class AdbDeviceService : IDisposable
    {
        private string? _adbPath;
        private readonly System.Timers.Timer _pollTimer;
        private bool _isDisposed;

        public event Action<List<AdbDevice>>? OnDevicesUpdated;
        public event Action<string>? OnLog;

        public List<AdbDevice> ConnectedDevices { get; private set; } = new();

        public AdbDeviceService()
        {
            ResolveAdbBinary();
            _pollTimer = new System.Timers.Timer(1500); // 1.5s device detection loop
            _pollTimer.Elapsed += async (s, e) => await RefreshDevicesAsync();
            _pollTimer.Start();
        }

        private void ResolveAdbBinary()
        {
            try
            {
                var baseDir = AppDomain.CurrentDomain.BaseDirectory;
                var candidates = new[]
                {
                    Path.Combine(baseDir, "adb.exe"),
                    Path.Combine(baseDir, "..", "..", "..", "..", "output", "douwan_extracted", "adb.exe"),
                    Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), @"Android\Sdk\platform-tools\adb.exe")
                };

                foreach (var candidate in candidates)
                {
                    var fullPath = Path.GetFullPath(candidate);
                    if (File.Exists(fullPath))
                    {
                        _adbPath = fullPath;
                        EnsureDllsPresent(Path.GetDirectoryName(fullPath)!, baseDir);
                        OnLog?.Invoke($"[ADB] Found adb engine at: {_adbPath}");
                        return;
                    }
                }

                _adbPath = "adb"; // Fallback to system PATH
            }
            catch (Exception ex)
            {
                OnLog?.Invoke($"[ADB] Error locating adb: {ex.Message}");
                _adbPath = "adb";
            }
        }

        private static void EnsureDllsPresent(string sourceDir, string targetDir)
        {
            try
            {
                foreach (var dll in new[] { "AdbWinApi.dll", "AdbWinUsbApi.dll" })
                {
                    var src = Path.Combine(sourceDir, dll);
                    var dst = Path.Combine(targetDir, dll);
                    if (File.Exists(src) && !File.Exists(dst))
                    {
                        File.Copy(src, dst, true);
                    }
                }
            }
            catch
            {
                // Ignore copy errors
            }
        }

        public async Task<List<AdbDevice>> RefreshDevicesAsync()
        {
            var list = new List<AdbDevice>();
            try
            {
                var output = await RunAdbCommandAsync("devices -l");
                var lines = output.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

                foreach (var line in lines)
                {
                    var trimmed = line.Trim();
                    if (trimmed.StartsWith("List of devices") || string.IsNullOrWhiteSpace(trimmed)) continue;

                    var parts = trimmed.Split(new[] { ' ', '\t' }, StringSplitOptions.RemoveEmptyEntries);
                    if (parts.Length >= 2)
                    {
                        var dev = new AdbDevice
                        {
                            Serial = parts[0],
                            State = parts[1]
                        };

                        for (int i = 2; i < parts.Length; i++)
                        {
                            if (parts[i].StartsWith("model:"))
                            {
                                dev.Model = parts[i].Substring(6).Replace("_", " ");
                            }
                            else if (parts[i].StartsWith("product:"))
                            {
                                dev.Product = parts[i].Substring(8);
                            }
                        }

                        list.Add(dev);
                    }
                }

                ConnectedDevices = list;
                OnDevicesUpdated?.Invoke(list);
            }
            catch (Exception ex)
            {
                OnLog?.Invoke($"[ADB] Failed to enumerate devices: {ex.Message}");
            }
            return list;
        }

        public async Task<bool> SetupPortForwardingAsync(string? serial = null, int port = 8099)
        {
            try
            {
                var target = string.IsNullOrEmpty(serial) ? "" : $"-s {serial} ";
                OnLog?.Invoke($"[ADB] Configuring bidirectional port tunnel: {port} <-> {port}");

                // 1. adb forward tcp:8099 tcp:8099
                var resForward = await RunAdbCommandAsync($"{target}forward tcp:{port} tcp:{port}");
                
                // 2. adb reverse tcp:8099 tcp:8099
                var resReverse = await RunAdbCommandAsync($"{target}reverse tcp:{port} tcp:{port}");

                OnLog?.Invoke($"[ADB] Tunnel active. Forward: {resForward.Trim()}, Reverse: {resReverse.Trim()}");
                return true;
            }
            catch (Exception ex)
            {
                OnLog?.Invoke($"[ADB] Tunnel setup exception: {ex.Message}");
                return false;
            }
        }

        public async Task<bool> LaunchCompanionAppAsync(string? serial = null)
        {
            try
            {
                var target = string.IsNullOrEmpty(serial) ? "" : $"-s {serial} ";
                OnLog?.Invoke("[ADB] Launching unkXwyno MirroringService on Android device...");
                var res = await RunAdbCommandAsync($"{target}shell am start-foreground-service -n com.unkxwyno.mirror/.service.MirroringService");
                OnLog?.Invoke($"[ADB] Launch response: {res.Trim()}");
                return true;
            }
            catch (Exception ex)
            {
                OnLog?.Invoke($"[ADB] Launch companion failed: {ex.Message}");
                return false;
            }
        }

        private async Task<string> RunAdbCommandAsync(string arguments)
        {
            if (string.IsNullOrEmpty(_adbPath)) return string.Empty;

            var psi = new ProcessStartInfo
            {
                FileName = _adbPath,
                Arguments = arguments,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                CreateNoWindow = true
            };

            using var proc = new Process { StartInfo = psi };
            proc.Start();

            var stdoutTask = proc.StandardOutput.ReadToEndAsync();
            var stderrTask = proc.StandardError.ReadToEndAsync();

            await proc.WaitForExitAsync();
            var stdout = await stdoutTask;
            return stdout;
        }

        public void Dispose()
        {
            if (_isDisposed) return;
            _isDisposed = true;
            _pollTimer.Stop();
            _pollTimer.Dispose();
        }
    }
}

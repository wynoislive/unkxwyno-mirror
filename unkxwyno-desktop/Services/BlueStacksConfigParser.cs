/**
 * @file BlueStacksConfigParser.cs
 * @brief High-performance parser importing BlueStacks / MSI App Player keymap (.cfg / .json) schemes.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using Unkxwyno.Desktop.Interop;
using Unkxwyno.Desktop.Models;

namespace Unkxwyno.Desktop.Services
{
    public static class BlueStacksConfigParser
    {
        public static KeymapProfile Parse(string filePath)
        {
            var profile = new KeymapProfile
            {
                ProfileName = Path.GetFileNameWithoutExtension(filePath),
                GamePackageName = "imported.game"
            };

            if (!File.Exists(filePath)) return profile;

            try
            {
                string jsonText = File.ReadAllText(filePath);
                using var doc = JsonDocument.Parse(jsonText);
                var root = doc.RootElement;

                // Check for BlueStacks standard GameControls array
                JsonElement controlsArray;
                if (root.TryGetProperty("ControlSchemes", out var schemes) && schemes.GetArrayLength() > 0)
                {
                    var firstScheme = schemes[0];
                    if (firstScheme.TryGetProperty("Name", out var nameProp))
                    {
                        profile.ProfileName = nameProp.GetString() ?? profile.ProfileName;
                    }
                    if (firstScheme.TryGetProperty("GameControls", out var gc))
                    {
                        controlsArray = gc;
                    }
                    else
                    {
                        return profile;
                    }
                }
                else if (root.TryGetProperty("GameControls", out var gcDirect))
                {
                    controlsArray = gcDirect;
                }
                else
                {
                    return profile;
                }

                foreach (var ctrl in controlsArray.EnumerateArray())
                {
                    string ctrlType = ctrl.TryGetProperty("Type", out var t) ? (t.GetString() ?? "") : "";

                    if (ctrlType.Equals("Tap", StringComparison.OrdinalIgnoreCase))
                    {
                        string key = ctrl.TryGetProperty("Key", out var k) ? (k.GetString() ?? "") : "";
                        double x = ctrl.TryGetProperty("X", out var xp) ? xp.GetDouble() : 0.0;
                        double y = ctrl.TryGetProperty("Y", out var yp) ? yp.GetDouble() : 0.0;

                        if (!string.IsNullOrEmpty(key))
                        {
                            profile.Bindings.Add(new KeyBindingItem
                            {
                                KeyName = key,
                                Type = BindingType.Tap,
                                NormalizedX = (float)(x > 1.0 ? x / 100.0 : x),
                                NormalizedY = (float)(y > 1.0 ? y / 100.0 : y)
                            });
                        }
                    }
                    else if (ctrlType.Equals("Dpad", StringComparison.OrdinalIgnoreCase))
                    {
                        double x = ctrl.TryGetProperty("X", out var xp) ? xp.GetDouble() : 20.0;
                        double y = ctrl.TryGetProperty("Y", out var yp) ? yp.GetDouble() : 70.0;
                        double rad = ctrl.TryGetProperty("Radius", out var rp) ? rp.GetDouble() : 10.0;

                        string up = ctrl.TryGetProperty("KeyUp", out var ku) ? (ku.GetString() ?? "W") : "W";
                        string down = ctrl.TryGetProperty("KeyDown", out var kd) ? (kd.GetString() ?? "S") : "S";
                        string left = ctrl.TryGetProperty("KeyLeft", out var kl) ? (kl.GetString() ?? "A") : "A";
                        string right = ctrl.TryGetProperty("KeyRight", out var kr) ? (kr.GetString() ?? "D") : "D";

                        profile.Bindings.Add(new KeyBindingItem
                        {
                            KeyName = $"{up}/{left}/{down}/{right}",
                            Type = BindingType.Dpad,
                            NormalizedX = (float)(x > 1.0 ? x / 100.0 : x),
                            NormalizedY = (float)(y > 1.0 ? y / 100.0 : y),
                            Radius = (float)(rad > 1.0 ? rad / 100.0 : rad)
                        });
                    }
                    else if (ctrlType.Equals("Pan", StringComparison.OrdinalIgnoreCase) || ctrlType.Equals("FreeLook", StringComparison.OrdinalIgnoreCase))
                    {
                        string toggleKey = ctrl.TryGetProperty("KeyStartStop", out var kss) ? (kss.GetString() ?? "F1") : "F1";
                        double sensX = ctrl.TryGetProperty("Sensitivity", out var sx) ? sx.GetDouble() : 1.0;
                        double sensY = ctrl.TryGetProperty("SensitivityRatioY", out var sy) ? sy.GetDouble() : 1.0;

                        profile.AimToggleKey = toggleKey;
                        profile.AimSensitivityX = (float)sensX;
                        profile.AimSensitivityY = (float)sensY;
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[BlueStacksConfigParser] Error parsing {filePath}: {ex.Message}");
            }

            return profile;
        }

        public static UnkxwynoKeymapBadge[] ToNativeBadges(KeymapProfile profile)
        {
            var list = new List<UnkxwynoKeymapBadge>();
            foreach (var b in profile.Bindings)
            {
                list.Add(new UnkxwynoKeymapBadge
                {
                    NormX = b.NormalizedX,
                    NormY = b.NormalizedY,
                    Radius = b.Radius > 0.001f ? b.Radius : 0.035f,
                    Label = b.KeyName.Length > 7 ? b.KeyName[..7] : b.KeyName,
                    ColorRgba = 0xEE00E5FF // Cyan with high alpha
                });
            }
            return list.ToArray();
        }
    }
}

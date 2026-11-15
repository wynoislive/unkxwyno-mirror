/**
 * @file KeymapProfile.cs
 * @brief High-precision mobile gaming keymapping models for sub-1ms hardware touch emulation.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System.Collections.Generic;

namespace Unkxwyno.Desktop.Models
{
    public enum BindingType
    {
        Tap = 0,
        Dpad = 1,
        FreeLookAim = 2,
        ContinuousFire = 3
    }

    public enum KeymapActionType
    {
        Tap = 0,
        DPad = 1,
        FreeLookAim = 2,
        ContinuousFire = 3
    }

    public class KeyBindingItem
    {
        public string KeyName { get; set; } = "Space";
        public float NormalizedX { get; set; } = 0.5f;
        public float NormalizedY { get; set; } = 0.5f;
        public float Radius { get; set; } = 0.035f;
        public BindingType Type { get; set; } = BindingType.Tap;
        public byte PointerId { get; set; } = 0;
        public string Label { get; set; } = "Action";
    }

    public class KeyMapping
    {
        public string Key { get; set; } = "Space";
        public float NormalizedX { get; set; } = 0.5f;
        public float NormalizedY { get; set; } = 0.5f;
        public KeymapActionType ActionType { get; set; } = KeymapActionType.Tap;
        public byte PointerId { get; set; } = 0;
        public string Label { get; set; } = "Action";
    }

    public class KeymapProfile
    {
        public string ProfileName { get; set; } = "Default FPS / Battle Royale";
        public string GamePackageName { get; set; } = "com.dts.freefireth";
        public string AimToggleKey { get; set; } = "F1";
        public float AimSensitivityX { get; set; } = 1.0f;
        public float AimSensitivityY { get; set; } = 1.0f;

        public List<KeyBindingItem> Bindings { get; set; } = new();
        public List<KeyMapping> Mappings { get; set; } = new();

        public static KeymapProfile CreateDefaultFpsProfile()
        {
            var p = new KeymapProfile
            {
                ProfileName = "Standard Battle Royale (Free Fire / PUBG / COD)",
                GamePackageName = "com.dts.freefireth",
                AimToggleKey = "F1",
                AimSensitivityX = 1.0f,
                AimSensitivityY = 1.0f
            };

            p.Bindings.Add(new() { KeyName = "W", NormalizedX = 0.18f, NormalizedY = 0.70f, Type = BindingType.Tap, PointerId = 1, Label = "Move Up" });
            p.Bindings.Add(new() { KeyName = "A", NormalizedX = 0.12f, NormalizedY = 0.76f, Type = BindingType.Tap, PointerId = 1, Label = "Move Left" });
            p.Bindings.Add(new() { KeyName = "S", NormalizedX = 0.18f, NormalizedY = 0.82f, Type = BindingType.Tap, PointerId = 1, Label = "Move Down" });
            p.Bindings.Add(new() { KeyName = "D", NormalizedX = 0.24f, NormalizedY = 0.76f, Type = BindingType.Tap, PointerId = 1, Label = "Move Right" });
            p.Bindings.Add(new() { KeyName = "Space", NormalizedX = 0.92f, NormalizedY = 0.75f, Type = BindingType.Tap, PointerId = 2, Label = "Jump" });
            p.Bindings.Add(new() { KeyName = "C", NormalizedX = 0.84f, NormalizedY = 0.88f, Type = BindingType.Tap, PointerId = 3, Label = "Crouch" });
            p.Bindings.Add(new() { KeyName = "Z", NormalizedX = 0.76f, NormalizedY = 0.92f, Type = BindingType.Tap, PointerId = 4, Label = "Prone" });
            p.Bindings.Add(new() { KeyName = "R", NormalizedX = 0.80f, NormalizedY = 0.75f, Type = BindingType.Tap, PointerId = 5, Label = "Reload" });
            p.Bindings.Add(new() { KeyName = "LMB", NormalizedX = 0.88f, NormalizedY = 0.60f, Type = BindingType.Tap, PointerId = 6, Label = "Fire" });
            p.Bindings.Add(new() { KeyName = "RMB", NormalizedX = 0.94f, NormalizedY = 0.50f, Type = BindingType.Tap, PointerId = 7, Label = "Aim" });

            return p;
        }
    }
}

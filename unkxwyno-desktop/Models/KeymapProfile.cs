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
    public enum KeymapActionType
    {
        Tap = 0,
        DPad = 1,
        FreeLookAim = 2,
        ContinuousFire = 3
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
        public List<KeyMapping> Mappings { get; set; } = new();

        public static KeymapProfile CreateDefaultFpsProfile()
        {
            return new KeymapProfile
            {
                ProfileName = "Standard Battle Royale (Free Fire / PUBG / COD)",
                Mappings = new List<KeyMapping>
                {
                    // Movement D-Pad
                    new() { Key = "W", NormalizedX = 0.18f, NormalizedY = 0.70f, ActionType = KeymapActionType.DPad, PointerId = 1, Label = "Move Up" },
                    new() { Key = "A", NormalizedX = 0.12f, NormalizedY = 0.76f, ActionType = KeymapActionType.DPad, PointerId = 1, Label = "Move Left" },
                    new() { Key = "S", NormalizedX = 0.18f, NormalizedY = 0.82f, ActionType = KeymapActionType.DPad, PointerId = 1, Label = "Move Down" },
                    new() { Key = "D", NormalizedX = 0.24f, NormalizedY = 0.76f, ActionType = KeymapActionType.DPad, PointerId = 1, Label = "Move Right" },

                    // Combat Actions
                    new() { Key = "Space", NormalizedX = 0.92f, NormalizedY = 0.75f, ActionType = KeymapActionType.Tap, PointerId = 2, Label = "Jump" },
                    new() { Key = "C",     NormalizedX = 0.84f, NormalizedY = 0.88f, ActionType = KeymapActionType.Tap, PointerId = 3, Label = "Crouch" },
                    new() { Key = "Z",     NormalizedX = 0.76f, NormalizedY = 0.92f, ActionType = KeymapActionType.Tap, PointerId = 4, Label = "Prone" },
                    new() { Key = "R",     NormalizedX = 0.80f, NormalizedY = 0.75f, ActionType = KeymapActionType.Tap, PointerId = 5, Label = "Reload" },
                    new() { Key = "LeftButton", NormalizedX = 0.88f, NormalizedY = 0.60f, ActionType = KeymapActionType.Tap, PointerId = 6, Label = "Fire" },
                    new() { Key = "RightButton", NormalizedX = 0.94f, NormalizedY = 0.50f, ActionType = KeymapActionType.Tap, PointerId = 7, Label = "Aim / Scope" },
                }
            };
        }
    }
}

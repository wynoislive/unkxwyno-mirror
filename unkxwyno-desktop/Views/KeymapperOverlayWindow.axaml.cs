using System;
using Avalonia.Controls;
using Avalonia.Input;
using Unkxwyno.Desktop.Models;
using Unkxwyno.Desktop.Services;

namespace Unkxwyno.Desktop.Views
{
    public partial class KeymapperOverlayWindow : Window
    {
        private readonly MirrorEngineService _engineService;
        private readonly KeymapProfile _profile;

        public KeymapperOverlayWindow() : this(new MirrorEngineService())
        {
        }

        public KeymapperOverlayWindow(MirrorEngineService engineService)
        {
            _engineService = engineService;
            _profile = KeymapProfile.CreateDefaultFpsProfile();

            InitializeComponent();

            KeyDown += OnKeyDownHandler;
            KeyUp += OnKeyUpHandler;
        }

        private void OnKeyDownHandler(object? sender, KeyEventArgs e)
        {
            var keyStr = e.Key.ToString();
            foreach (var mapping in _profile.Mappings)
            {
                if (string.Equals(mapping.Key, keyStr, StringComparison.OrdinalIgnoreCase))
                {
                    ushort normX = (ushort)(mapping.NormalizedX * 65535);
                    ushort normY = (ushort)(mapping.NormalizedY * 65535);

                    // Action 0 = Touch Down
                    _engineService.InjectTouch(mapping.PointerId, 0, normX, normY, 800);
                    e.Handled = true;
                    break;
                }
            }
        }

        private void OnKeyUpHandler(object? sender, KeyEventArgs e)
        {
            var keyStr = e.Key.ToString();
            foreach (var mapping in _profile.Mappings)
            {
                if (string.Equals(mapping.Key, keyStr, StringComparison.OrdinalIgnoreCase))
                {
                    ushort normX = (ushort)(mapping.NormalizedX * 65535);
                    ushort normY = (ushort)(mapping.NormalizedY * 65535);

                    // Action 2 = Touch Up
                    _engineService.InjectTouch(mapping.PointerId, 2, normX, normY, 0);
                    e.Handled = true;
                    break;
                }
            }
        }

        private void OnCanvasPointerPressed(object? sender, PointerPressedEventArgs e)
        {
            var point = e.GetCurrentPoint(KeymapCanvas);
            if (point.Properties.IsLeftButtonPressed)
            {
                double normX = point.Position.X / Bounds.Width;
                double normY = point.Position.Y / Bounds.Height;
                _engineService.InjectTouch(0, 0, (ushort)(normX * 65535), (ushort)(normY * 65535), 600);
            }
        }

        private void OnCloseOverlayClick(object? sender, Avalonia.Interactivity.RoutedEventArgs e)
        {
            Close();
        }
    }
}

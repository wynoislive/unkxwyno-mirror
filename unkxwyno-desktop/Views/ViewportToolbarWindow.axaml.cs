/**
 * @file ViewportToolbarWindow.axaml.cs
 * @brief Ultra-slim 32px pill toolbar snapping flush above native Direct3D 11 viewport.
 * @version 1.0.0
 * 
 * Copyright (c) 2026 unkXwyno. Open source under the MIT License.
 * Official Repository & Download: https://unkxwyno-mirror.genzhost.best
 */

using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.Threading;
using Unkxwyno.Desktop.Models;
using Unkxwyno.Desktop.Services;

namespace Unkxwyno.Desktop.Views
{
    public partial class ViewportToolbarWindow : Window
    {
        private readonly IMirrorEngineService _engineService;
        private readonly DispatcherTimer _syncTimer;
        private bool _isScreenOff = false;
        private bool _isHudVisible = true;
        private bool _isMouseLocked = false;
        private bool _isMuted = false;

        public ViewportToolbarWindow()
        {
            InitializeComponent();
            _engineService = null!;
            _syncTimer = null!;
        }

        public ViewportToolbarWindow(IMirrorEngineService engineService)
        {
            InitializeComponent();
            _engineService = engineService;

            // Timer to snap flush above the native HWND position
            _syncTimer = new DispatcherTimer
            {
                Interval = TimeSpan.FromMilliseconds(50)
            };
            _syncTimer.Tick += (s, e) => SnapFlushAboveViewport();
            _syncTimer.Start();
        }

        public void SnapFlushAboveViewport()
        {
            if (_engineService == null || !_engineService.IsViewportVisible)
            {
                Hide();
                return;
            }

            if (_engineService.GetViewportBounds(out int vx, out int vy, out int vw, out int vh))
            {
                if (vw <= 0 || vh <= 0) return;

                int barW = (int)Width;
                int barH = (int)Height;
                int posX = vx + (vw - barW) / 2;
                int posY = vy - barH - 4; // Exactly flush above the window (0px overlap with D3D11 swapchain!)

                if (posY < 0) posY = vy + 6; // If near screen top edge, place inside title area

                Position = new PixelPoint(posX, posY);

                if (!IsVisible)
                {
                    Show();
                }
            }
        }

        private void OnScreenOffClick(object? sender, RoutedEventArgs e)
        {
            _isScreenOff = !_isScreenOff;
            _engineService.SetScreenPower(!_isScreenOff);
            if (ScreenOffBtn != null)
            {
                ScreenOffBtn.Background = _isScreenOff ? Avalonia.Media.Brush.Parse("#00E5FF33") : Avalonia.Media.Brushes.Transparent;
            }
        }

        private void OnToggleKeymapHudClick(object? sender, RoutedEventArgs e)
        {
            _isHudVisible = !_isHudVisible;
            if (_isHudVisible)
            {
                var defaultProfile = KeymapProfile.CreateDefaultFpsProfile();
                var badges = BlueStacksConfigParser.ToNativeBadges(defaultProfile);
                _engineService.SetKeymapHud(true, badges);
            }
            else
            {
                _engineService.SetKeymapHud(false, null);
            }

            if (KeymapHudBtn != null)
            {
                KeymapHudBtn.Background = _isHudVisible ? Avalonia.Media.Brush.Parse("#00E5FF33") : Avalonia.Media.Brushes.Transparent;
            }
        }

        private void OnMouseLockClick(object? sender, RoutedEventArgs e)
        {
            _isMouseLocked = !_isMouseLocked;
            if (MouseLockBtn != null)
            {
                MouseLockBtn.Background = _isMouseLocked ? Avalonia.Media.Brush.Parse("#6366F144") : Avalonia.Media.Brushes.Transparent;
            }
        }

        private void OnAudioClick(object? sender, RoutedEventArgs e)
        {
            _isMuted = !_isMuted;
            if (AudioBtn != null)
            {
                AudioBtn.Background = _isMuted ? Avalonia.Media.Brush.Parse("#FF525233") : Avalonia.Media.Brushes.Transparent;
            }
        }

        private void OnFullscreenClick(object? sender, RoutedEventArgs e)
        {
            _engineService.ToggleFullscreen();
        }

        private void OnCloseClick(object? sender, RoutedEventArgs e)
        {
            _engineService.ShowViewport(false);
            Hide();
        }

        protected override void OnClosed(EventArgs e)
        {
            _syncTimer?.Stop();
            base.OnClosed(e);
        }
    }
}

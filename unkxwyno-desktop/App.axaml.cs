using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Unkxwyno.Desktop.Services;
using Unkxwyno.Desktop.ViewModels;
using Unkxwyno.Desktop.Views;

namespace Unkxwyno.Desktop
{
    public partial class App : Application
    {
        public override void Initialize()
        {
            AvaloniaXamlLoader.Load(this);
        }

        public override void OnFrameworkInitializationCompleted()
        {
            if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
            {
                var engineService = new MirrorEngineService();
                var adbService = new AdbDeviceService();
                var viewModel = new MainViewModel(engineService, adbService);

                var mainWindow = new MainWindow
                {
                    DataContext = viewModel
                };

                ViewportToolbarWindow? toolbarWindow = null;

                viewModel.OnRequestShowToolbar += () =>
                {
                    if (toolbarWindow == null)
                    {
                        toolbarWindow = new ViewportToolbarWindow(engineService);
                    }
                    toolbarWindow.SnapFlushAboveViewport();
                };

                viewModel.OnRequestOpenKeymapper += () =>
                {
                    var keymapperWindow = new KeymapperOverlayWindow(engineService);
                    keymapperWindow.Show();
                };

                viewModel.OnRequestPickBlueStacksConfig += async () =>
                {
                    var topLevel = Avalonia.Controls.TopLevel.GetTopLevel(mainWindow);
                    if (topLevel != null)
                    {
                        var files = await topLevel.StorageProvider.OpenFilePickerAsync(new Avalonia.Platform.Storage.FilePickerOpenOptions
                        {
                            Title = "Select BlueStacks / MSI App Player Keymap (.cfg / .json)",
                            AllowMultiple = false
                        });
                        if (files.Count > 0)
                        {
                            viewModel.LoadBlueStacksConfigFile(files[0].Path.LocalPath);
                        }
                    }
                };

                desktop.MainWindow = mainWindow;
                desktop.Exit += (s, e) =>
                {
                    viewModel.Dispose();
                    engineService.Dispose();
                    adbService.Dispose();
                };
            }

            super_call();
        }

        private void super_call()
        {
            base.OnFrameworkInitializationCompleted();
        }
    }
}

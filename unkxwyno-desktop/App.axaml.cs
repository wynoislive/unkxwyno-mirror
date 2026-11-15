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
                var viewModel = new MainViewModel(engineService);

                var mainWindow = new MainWindow
                {
                    DataContext = viewModel
                };

                viewModel.OnRequestOpenKeymapper += () =>
                {
                    var keymapperWindow = new KeymapperOverlayWindow(engineService);
                    keymapperWindow.Show();
                };

                desktop.MainWindow = mainWindow;
                desktop.Exit += (s, e) => engineService.Dispose();
            }

            super_call();
        }

        private void super_call()
        {
            base.OnFrameworkInitializationCompleted();
        }
    }
}

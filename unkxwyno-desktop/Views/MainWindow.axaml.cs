using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Unkxwyno.Desktop.ViewModels;

namespace Unkxwyno.Desktop.Views
{
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        private void OnDragWindow(object? sender, PointerPressedEventArgs e)
        {
            if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            {
                BeginMoveDrag(e);
            }
        }

        private void OnMinimizeClick(object? sender, RoutedEventArgs e)
        {
            WindowState = WindowState.Minimized;
        }

        private void OnCloseClick(object? sender, RoutedEventArgs e)
        {
            Close();
        }

        private async void OnCopyIpClick(object? sender, RoutedEventArgs e)
        {
            if (DataContext is MainViewModel vm && Clipboard != null)
            {
                string textToCopy = $"{vm.LocalIpAddress}:{vm.StreamPort}";
                await Clipboard.SetTextAsync(textToCopy);
                vm.CopyStatusText = "Copied to Clipboard!";
                await Task.Delay(2500);
                vm.CopyStatusText = "Copy IP:Port";
            }
        }
    }
}

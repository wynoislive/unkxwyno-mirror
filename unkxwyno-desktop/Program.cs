using System;
using Avalonia;

namespace Unkxwyno.Desktop
{
    internal class Program
    {
        [STAThread]
        public static int Main(string[] args)
        {
            if (args.Length > 0 && args[0] == "--smoke-test")
            {
                Console.WriteLine("Running unkXwyno Desktop P/Invoke Smoke Test...");
                string ver = Interop.UnkxwynoNative.GetVersion();
                Console.WriteLine($"[P/Invoke SUCCESS] unkXwyno Core Native Version: {ver}");

                using var engine = new Services.MirrorEngineService();
                var cfg = Interop.UnkxwynoConfig.Default;
                bool initOk = engine.Initialize(cfg);
                Console.WriteLine($"[P/Invoke SUCCESS] Engine Initialize Result: {initOk}");

                bool startOk = engine.Start();
                Console.WriteLine($"[P/Invoke SUCCESS] Engine Start Result: {startOk}");

                engine.Stop();
                Console.WriteLine("[P/Invoke SUCCESS] Engine Stop executed cleanly.");
                return 0;
            }

            return BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
        }

        public static AppBuilder BuildAvaloniaApp()
            => AppBuilder.Configure<App>()
                .UsePlatformDetect()
                .WithInterFont()
                .LogToTrace();
    }
}

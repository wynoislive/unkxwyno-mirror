using System;
using System.IO;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using Avalonia.Media.Imaging;
using QRCoder;

namespace Unkxwyno.Desktop.Services
{
    public static class QrCodeService
    {
        public static string GetPreferredLocalIpAddress()
        {
            try
            {
                foreach (var netInterface in NetworkInterface.GetAllNetworkInterfaces())
                {
                    if (netInterface.OperationalStatus != OperationalStatus.Up)
                        continue;
                    if (netInterface.NetworkInterfaceType == NetworkInterfaceType.Loopback)
                        continue;

                    var ipProps = netInterface.GetIPProperties();
                    foreach (var addr in ipProps.UnicastAddresses)
                    {
                        if (addr.Address.AddressFamily == AddressFamily.InterNetwork)
                        {
                            string ip = addr.Address.ToString();
                            // Skip link-local 169.254, loopback 127, and virtual VPN 26.x
                            if (!ip.StartsWith("169.254.") && !ip.StartsWith("127.") && !ip.StartsWith("26."))
                            {
                                return ip;
                            }
                        }
                    }
                }
            }
            catch
            {
                // Fallback
            }
            return "192.168.1.4";
        }

        public static Bitmap GenerateQrBitmap(string payload, int pixelsPerModule = 10)
        {
            using var generator = new QRCodeGenerator();
            using var data = generator.CreateQrCode(payload, QRCodeGenerator.ECCLevel.M);
            var pngCode = new PngByteQRCode(data);
            byte[] bytes = pngCode.GetGraphic(pixelsPerModule);
            using var ms = new MemoryStream(bytes);
            return new Bitmap(ms);
        }
    }
}

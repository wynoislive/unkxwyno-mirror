#!/usr/bin/env bash
# unkXwyno Mirror - Universal Linux Installer / Runner (.sh / .run)
set -e

echo "============================================================"
echo "  unkXwyno Mirror - Low-Latency Screen Mirroring Studio"
echo "============================================================"

# Check architecture
ARCH=$(uname -m)
if [ "$ARCH" != "x86_64" ] && [ "$ARCH" != "aarch64" ]; then
    echo "[-] Unsupported CPU architecture: $ARCH. Required x86_64 or aarch64."
    exit 1
fi

echo "[*] Checking system dependencies (adb, ffmpeg, libva, vulkan)..."
if command -v apt-get >/dev/null 2>&1; then
    sudo apt-get update -qq
    sudo apt-get install -y -qq adb libva-dev libvulkan-dev ffmpeg avahi-daemon
elif command -v dnf >/dev/null 2>&1; then
    sudo dnf install -y -q android-tools libva-devel vulkan-loader-devel ffmpeg avahi
elif command -v pacman >/dev/null 2>&1; then
    sudo pacman -S --noconfirm android-tools libva vulkan-icd-loader ffmpeg avahi
fi

echo "[*] Configuring ADB port forwarding for USB cable mirroring..."
adb start-server || true
adb forward tcp:8099 tcp:8099 || true
adb reverse tcp:8099 tcp:8099 || true

echo "[*] Ensuring avahi-daemon is running for iOS AirPlay mDNS discovery..."
sudo systemctl start avahi-daemon || true

echo "[+] unkXwyno Mirror Linux environment is ready."
echo "Run: dotnet unkxwyno-desktop.dll or ./unkxwyno-desktop"

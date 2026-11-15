#!/usr/bin/env bash
# unkXwyno Mirror - Universal macOS Setup Script (.sh)
set -e

echo "============================================================"
echo "  unkXwyno Mirror - macOS Setup (usbmuxd, adb, AirPlay)"
echo "============================================================"

if ! command -v brew >/dev/null 2>&1; then
    echo "[!] Homebrew not found. Please install Homebrew from https://brew.sh/"
    exit 1
fi

echo "[*] Installing required tools via Homebrew..."
brew install android-platform-tools libimobiledevice usbmuxd ffmpeg || true

echo "[*] Starting usbmuxd daemon for wired Lightning / USB-C streaming..."
sudo launchctl load -w /System/Library/LaunchDaemons/com.apple.usbmuxd.plist 2>/dev/null || true

echo "[*] Configuring ADB port forwarding for Android USB cable mirroring..."
adb start-server || true
adb forward tcp:8099 tcp:8099 || true
adb reverse tcp:8099 tcp:8099 || true

echo "[+] unkXwyno Mirror macOS environment is ready!"

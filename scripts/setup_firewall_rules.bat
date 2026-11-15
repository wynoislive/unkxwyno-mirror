@echo off
:: unkXwyno Mirror - Automated Windows Firewall Rule Setup
:: Ensures AirPlay (5353 UDP / 7000 TCP) and Android (8099 TCP / 8098 UDP) are allowed
echo ============================================================
echo   unkXwyno Mirror - Configuring Windows Defender Firewall
echo ============================================================
echo.

net session >nul 2>&1
if %errorLevel% neq 0 (
    echo [!] Requesting Administrator privileges to add firewall rules...
    powershell -Command "Start-Process cmd -ArgumentList '/c \"%~f0\"' -Verb RunAs"
    exit /b
)

echo [*] Adding rule for AirPlay RTSP Video (TCP 7000)...
netsh advfirewall firewall delete rule name="unkXwyno AirPlay RTSP" >nul 2>&1
netsh advfirewall firewall add rule name="unkXwyno AirPlay RTSP" dir=in action=allow protocol=TCP localport=7000 profile=any >nul

echo [*] Adding rule for AirPlay mDNS Discovery (UDP 5353)...
netsh advfirewall firewall delete rule name="unkXwyno AirPlay mDNS" >nul 2>&1
netsh advfirewall firewall add rule name="unkXwyno AirPlay mDNS" dir=in action=allow protocol=UDP localport=5353 profile=any >nul

echo [*] Adding rule for Android Wireless Streaming (TCP 8099)...
netsh advfirewall firewall delete rule name="unkXwyno Android Stream" >nul 2>&1
netsh advfirewall firewall add rule name="unkXwyno Android Stream" dir=in action=allow protocol=TCP localport=8099 profile=any >nul

echo [*] Adding rule for Android UDP Auto-Discovery (UDP 8098)...
netsh advfirewall firewall delete rule name="unkXwyno Android Discovery" >nul 2>&1
netsh advfirewall firewall add rule name="unkXwyno Android Discovery" dir=in action=allow protocol=UDP localport=8098 profile=any >nul

echo.
echo [PASS] All Windows Firewall rules configured successfully!
echo You can now mirror from iOS Control Center and Android seamlessly.
echo.
pause

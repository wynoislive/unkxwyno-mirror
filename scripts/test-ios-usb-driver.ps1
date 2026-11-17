[CmdletBinding()]
param(
    [switch]$RunTrial,
    [switch]$Recover,
    [switch]$CloseAppleBackgroundClient,
    [switch]$ObserveOriginal,
    [switch]$ObserveUsbMetadata,
    [switch]$ManagedCapture,
    [int]$OwnerPid,
    [long]$OwnerStartTicks,
    [ValidateRange(10,180)][int]$DurationSeconds = 90,
    [string]$SessionDirectory
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($SessionDirectory)) { throw 'A new trial output directory is required.' }
$SessionDirectory = [IO.Path]::GetFullPath($SessionDirectory)
$allowedRoot = [IO.Path]::GetFullPath((Join-Path $repo 'out')) + [IO.Path]::DirectorySeparatorChar
if (-not $SessionDirectory.StartsWith($allowedRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Trial output must stay under workspace out/.' }
$journalPath = Join-Path $SessionDirectory 'driver-journal.json'
$hardware = 'USB\VID_05AC&PID_12A8'
$originalInf = Join-Path $env:WINDIR 'INF/oem14.inf'
$captureInf = Join-Path $env:WINDIR 'INF/oem15.inf'
$originalHash = '62d18335331cbcadd3e5b7fadcc608d8716fe675088f17366961da4c79001572'
$captureHash = 'e3824a8c963675d615391b25026afec7a3bd0b3f35a74cb3dd07e978e2ffd55c'
function Assert-InfHashes {
    if ((Get-FileHash -LiteralPath $originalInf -Algorithm SHA256).Hash -ine $originalHash -or
        (Get-FileHash -LiteralPath $captureInf -Algorithm SHA256).Hash -ine $captureHash) {
        throw 'An installed driver package changed; no binding operation is allowed.'
    }
}
function Save-Journal($Value) {
    $temporary = Join-Path $SessionDirectory 'driver-journal.pending'
    [IO.File]::WriteAllText($temporary, ($Value | ConvertTo-Json -Depth 5))
    Move-Item -LiteralPath $temporary -Destination $journalPath -Force
}
function Get-Phone {
    $phones = @(Get-PnpDevice -PresentOnly | Where-Object { $_.InstanceId -like ($hardware + '\*') })
    if ($phones.Count -ne 1) { throw 'Keep exactly the original iPhone connected for this driver trial.' }
    return $phones[0]
}
function Get-Binding($Instance) {
    return [ordered]@{
        Service = (Get-PnpDeviceProperty -InstanceId $Instance -KeyName DEVPKEY_Device_Service).Data
        Inf = (Get-PnpDeviceProperty -InstanceId $Instance -KeyName DEVPKEY_Device_DriverInfPath).Data
        Problem = (Get-PnpDeviceProperty -InstanceId $Instance -KeyName DEVPKEY_Device_ProblemCode).Data
    }
}
function Assert-Admin {
    $principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'This driver trial requires Windows administrator elevation.' }
}
function Set-Binding($Inf) {
    Assert-InfHashes
    $phone = Get-Phone
    if ($phone.InstanceId -ine $script:journal.InstanceId) { throw 'The original iPhone is not connected; driver operation stopped.' }
    $restart = $false
    if (-not [IndependentUsbTrialDriver]::UpdateDriverForPlugAndPlayDevicesW([IntPtr]::Zero,$hardware,$Inf,1,[ref]$restart)) {
        throw (New-Object ComponentModel.Win32Exception([Runtime.InteropServices.Marshal]::GetLastWin32Error()))
    }
    if ($restart) { $script:journal.RebootRequired = $true; Save-Journal $script:journal; throw 'Windows requested a restart. Do not repeat the trial before restarting.' }
}
function Restore-Original {
    if ($script:journal.Restored -or -not $script:journal.RestorationRequired) { return }
    Set-Binding $originalInf
    for ($i=0;$i -lt 40;$i++) {
        $binding = Get-Binding $script:journal.InstanceId
        if ($binding.Service -ieq 'usbccgp' -and $binding.Inf -ieq 'oem14.inf' -and $binding.Problem -eq 0) {
            $script:journal.Restored = $true; $script:journal.State = 'AppleDriverRestored'; Save-Journal $script:journal; return
        }
        Start-Sleep -Milliseconds 500
    }
    throw 'Apple driver restoration is not verified. Keep the iPhone connected and recover this journal.'
}
Assert-InfHashes
if (-not $RunTrial -and -not $Recover) {
    $phone = Get-Phone; $binding = Get-Binding $phone.InstanceId
    Write-Host "[preview] Current parent service: $($binding.Service); INF: $($binding.Inf); problem code: $($binding.Problem)."
    Write-Host '[preview] Temporary target: installed libusb0/oem15.inf; restore target: usbccgp/oem14.inf. Hashes verified.'
    Write-Host '[preview] No driver changes performed.'
    exit 0
}
Assert-Admin
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class IndependentUsbTrialDriver {
    [DllImport("newdev.dll", CharSet=CharSet.Unicode, SetLastError=true)]
    [return: MarshalAs(UnmanagedType.Bool)]
    public static extern bool UpdateDriverForPlugAndPlayDevicesW(IntPtr hwnd,string hardware,string inf,uint flags,[MarshalAs(UnmanagedType.Bool)] out bool restart);
}
'@
if ($Recover) {
    if ($OwnerPid -gt 0) {
        do {
            $owner = Get-Process -Id $OwnerPid -ErrorAction SilentlyContinue
            $alive = $owner -and $owner.StartTime.ToUniversalTime().Ticks -eq $OwnerStartTicks
            if ($alive) { Start-Sleep -Milliseconds 500 }
        } while ($alive)
    }
    $script:journal = Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
    if ($script:journal.OriginalHash -ine $originalHash -or $script:journal.CaptureHash -ine $captureHash) { throw 'Unrecognized recovery journal.' }
    $bootTicks=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks
    if ($script:journal.RebootRequired -and $script:journal.BootTicks -eq $bootTicks) { throw 'Windows requested a restart; automatic recovery will not repeat driver operations before that restart.' }
    Restore-Original
    exit 0
}
if ($CloseAppleBackgroundClient) {
    # The user already closed the front-end apps for the driver trial; this
    # background client can restart when the original Apple driver is rebound.
    Get-Process AppleMobileDeviceProcess -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 500
}
if (Get-Process AppleMobileDeviceProcess -ErrorAction SilentlyContinue) { throw 'Close iTunes/Apple Devices and AppleMobileDeviceProcess.exe before the trial. No driver changed.' }
$phone = Get-Phone; $binding = Get-Binding $phone.InstanceId
if ($binding.Service -ine 'usbccgp' -or $binding.Inf -ine 'oem14.inf' -or $binding.Problem -ne 0) { throw 'The expected healthy original Apple parent binding is not present.' }
if (Test-Path -LiteralPath $SessionDirectory) { throw 'Use a new empty trial directory; existing journals are preserved.' }
New-Item -ItemType Directory -Path $SessionDirectory | Out-Null
$nativeExe = Join-Path $repo 'out/core/release/core/unkxwyno_usb_probe.exe'
if (-not (Test-Path -LiteralPath $nativeExe -PathType Leaf)) { throw 'Build the independent native USB probe first.' }
$targetArguments = '--mirror'
if ($ManagedCapture) {
    if ($ObserveOriginal) { throw 'Choose independent managed capture or original observation, not both.' }
    $nativeExe = Join-Path $repo 'tests/usb_capture_managed/bin/Release/net10.0-windows/UnkxwynoMirror.UsbCapture.Hardware.exe'
    if (-not (Test-Path -LiteralPath $nativeExe -PathType Leaf)) { throw 'Build the managed hardware harness before this trial.' }
    $targetArguments = '--hardware'
}
if ($ObserveOriginal) {
    # Analysis-only control, never a product dependency or installer execution.
    $source = Join-Path $repo 'refrence/douwan-static-pass/x64'
    $referenceExe = Join-Path $source 'DouWan.exe'
    $referenceHash = '2ad4abb0d88f8214ca442f009f3ae62129ee108b5ab8085b066652c2871fbdc9'
    if ((Get-FileHash -LiteralPath $referenceExe -Algorithm SHA256).Hash -ine $referenceHash -or
        (Get-AuthenticodeSignature -LiteralPath $referenceExe).Status -ne 'Valid') { throw 'Original observation target identity/signature mismatch.' }
    if (Get-Process DouWan,unkxwyno-receiver -ErrorAction SilentlyContinue) { throw 'An existing original receiver could collide with the control observation.' }
    $referenceCopy = Join-Path $SessionDirectory 'reference-only'
    Copy-Item -LiteralPath $source -Destination $referenceCopy -Recurse
    $nativeExe = Join-Path $referenceCopy 'DouWan.exe'
    if ((Get-FileHash -LiteralPath $nativeExe -Algorithm SHA256).Hash -ine $referenceHash) { throw 'Copied reference target identity mismatch.' }
    $targetArguments = ''
    if ($ObserveUsbMetadata) {
        $observerDll=Join-Path $repo 'out/usb-metadata-observer/UsbMetadataApiTrace.dll'
        $observerInstaller=Join-Path $repo 'src/analysis/Install-UsbMetadataTrace.ps1'
        & $observerInstaller -CompileOnly -ObserverDll $observerDll
        $env:UNKXWYNO_USB_METADATA_TRACE=Join-Path $SessionDirectory 'usb-metadata.log'
    }
}
$script:journal = [ordered]@{
    InstanceId=$phone.InstanceId; OriginalHash=$originalHash; CaptureHash=$captureHash
    NativeExeSha256=(Get-FileHash -LiteralPath $nativeExe -Algorithm SHA256).Hash
    State='Prepared'; RestorationRequired=$false; Restored=$false; RebootRequired=$false
    NativeExitCode=$null; BootTicks=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks
    TargetKind=$(if ($ObserveOriginal) {'OriginalReferenceObservation'} else {'IndependentCapture'})
    UsbMetadataObservation=[bool]$ObserveUsbMetadata
    Error=''; StartedUtc=[DateTime]::UtcNow.ToString('o')
}
Save-Journal $script:journal
$arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -Recover -OwnerPid ' + $PID + ' -OwnerStartTicks ' + (Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks + ' -SessionDirectory "' + $SessionDirectory + '"'
$watchdog = Start-Process -FilePath 'powershell.exe' -WindowStyle Hidden -ArgumentList $arguments -PassThru
$viewer = $null
try {
    $script:journal.RestorationRequired = $true; $script:journal.State='Switching'; Save-Journal $script:journal
    Set-Binding $captureInf
    $ready = $false
    for ($i=0;$i -lt 40;$i++) {
        $binding=Get-Binding $phone.InstanceId
        if ($binding.Service -ieq 'libusb0' -and $binding.Inf -ieq 'oem15.inf' -and $binding.Problem -eq 0) { $ready=$true; break }
        Start-Sleep -Milliseconds 500
    }
    if (-not $ready) { throw 'The installed capture driver did not become ready.' }
    $script:journal.State=$(if ($ObserveOriginal) {'ObservingOriginalReference'} else {'TestingIndependentCapture'}); Save-Journal $script:journal
    $launch = @{FilePath=$nativeExe; WorkingDirectory=(Split-Path -Parent $nativeExe); PassThru=$true; WindowStyle='Hidden'; RedirectStandardOutput=(Join-Path $SessionDirectory 'capture.stdout.log'); RedirectStandardError=(Join-Path $SessionDirectory 'capture.stderr.log')}
    if ($targetArguments) { $launch.ArgumentList=$targetArguments }
    $viewer = Start-Process @launch
    $null = $viewer.Handle
    if ($ObserveOriginal -and $ObserveUsbMetadata) {
        & $observerInstaller -TargetPid $viewer.Id -ExpectedExe $nativeExe -ObserverDll $observerDll
    }
    if (-not $viewer.WaitForExit($DurationSeconds*1000)) {
        $null = $viewer.CloseMainWindow()
        if (-not $viewer.WaitForExit(5000)) { Stop-Process -Id $viewer.Id -Force; $viewer.WaitForExit() }
    }
    $script:journal.NativeExitCode=$viewer.ExitCode
} catch {
    $script:journal.Error=$_.Exception.Message; $script:journal.State='TrialFailed'; Save-Journal $script:journal
    throw
} finally {
    if ($viewer -and -not $viewer.HasExited) { $null=$viewer.CloseMainWindow(); if (-not $viewer.WaitForExit(5000)) { Stop-Process -Id $viewer.Id -Force; $viewer.WaitForExit() } }
    Restore-Original
}

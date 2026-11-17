[CmdletBinding(DefaultParameterSetName='Preview')]
param(
    [Parameter(ParameterSetName='Acquire',Mandatory)][switch]$Acquire,
    [Parameter(ParameterSetName='Watchdog',Mandatory)][switch]$Watchdog,
    [Parameter(ParameterSetName='Recover',Mandatory)][switch]$Recover,
    [Parameter(ParameterSetName='Preview')][switch]$Preview,
    [Parameter(ParameterSetName='Acquire',Mandatory)][int]$OwnerPid,
    [Parameter(ParameterSetName='Acquire',Mandatory)][long]$OwnerStartTicks,
    [Parameter(ParameterSetName='Watchdog',Mandatory)][int]$BrokerPid,
    [Parameter(ParameterSetName='Watchdog',Mandatory)][long]$BrokerStartTicks,
    [Guid]$SessionId=[Guid]::Empty
)
$ErrorActionPreference='Stop'
$hardware='USB\VID_05AC&PID_12A8'
$originalHash='62d18335331cbcadd3e5b7fadcc608d8716fe675088f17366961da4c79001572'
$captureHash='e3824a8c963675d615391b25026afec7a3bd0b3f35a74cb3dd07e978e2ffd55c'
$sessionsRoot=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'UNKXWYNO Mirror/usb-driver-sessions'
$sessionDirectory=Join-Path $sessionsRoot $SessionId.ToString('N')
$journalPath=Join-Path $sessionDirectory 'driver-journal.json'
$stopPath=Join-Path $sessionDirectory 'stop.request'
$script:journal=$null
function Assert-NoReparse([string]$Path) {
    $cursor=[IO.Path]::GetFullPath($Path)
    while($cursor){
        if(Test-Path -LiteralPath $cursor){
            if(([IO.File]::GetAttributes($cursor) -band [IO.FileAttributes]::ReparsePoint) -ne 0){throw 'USB session paths must not contain junctions or symbolic links.'}
        }
        $cursor=[IO.Path]::GetDirectoryName($cursor)
    }
}
function Save-Journal {
    Assert-NoReparse $sessionDirectory
    $temporary=Join-Path $sessionDirectory ([Guid]::NewGuid().ToString('N')+'.pending')
    try {
        [IO.File]::WriteAllText($temporary,($script:journal | ConvertTo-Json -Depth 5))
        Move-Item -LiteralPath $temporary -Destination $journalPath -Force
    } finally {if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}}
}
function Publish([string]$State,[string]$Message) {
    $script:journal.State=$State;$script:journal.Message=$Message;Save-Journal
}
function Get-Phone {
    $phones=@(Get-PnpDevice -PresentOnly | Where-Object {$_.InstanceId -like ($hardware+'\*')})
    if($phones.Count -ne 1){throw 'Connect exactly one supported iPhone by USB and keep it connected through driver restoration.'}
    return $phones[0]
}
function Get-Binding([string]$Instance) {
    return @{
        Service=(Get-PnpDeviceProperty -InstanceId $Instance -KeyName DEVPKEY_Device_Service).Data
        Inf=(Get-PnpDeviceProperty -InstanceId $Instance -KeyName DEVPKEY_Device_DriverInfPath).Data
        Problem=(Get-PnpDeviceProperty -InstanceId $Instance -KeyName DEVPKEY_Device_ProblemCode).Data
    }
}
function Inf-Path([string]$Name,[string]$Hash) {
    if($Name -notmatch '^oem[0-9]+\.inf$'){throw 'Unrecognized installed driver name.'}
    $path=Join-Path $env:WINDIR ('INF/'+$Name)
    if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ine $Hash){throw 'The installed iPhone driver package does not match the verified package. No replacement driver is downloaded.'}
    return $path
}
function Is-Alive([int]$ProcessId,[long]$Ticks) {
    $process=Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    return $process -and $process.StartTime.ToUniversalTime().Ticks -eq $Ticks
}
function Load-Journal {
    Assert-NoReparse $sessionDirectory
    if((Get-Item -LiteralPath $journalPath).Length -gt 16384){throw 'USB driver recovery journal exceeds its bound.'}
    $script:journal=Get-Content -LiteralPath $journalPath -Raw | ConvertFrom-Json
    if($script:journal.SessionId -cne $SessionId.ToString('N') -or
       $script:journal.OriginalHash -ine $originalHash -or $script:journal.CaptureHash -ine $captureHash -or
       $script:journal.InstanceId -notlike ($hardware+'\*')){throw 'Unrecognized USB driver recovery journal.'}
    $null=Inf-Path $script:journal.OriginalInf $originalHash
    $null=Inf-Path $script:journal.CaptureInf $captureHash
}
Assert-NoReparse $sessionsRoot
if($PSCmdlet.ParameterSetName -eq 'Preview'){
    $phone=Get-Phone;$binding=Get-Binding $phone.InstanceId
    [pscustomobject]@{Service=$binding.Service;Inf=$binding.Inf;Problem=$binding.Problem;DriverChanged=$false} | ConvertTo-Json -Compress
    exit 0
}
if($SessionId -eq [Guid]::Empty){throw 'A session identifier is required.'}
$principal=New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if(-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Temporary USB driver access requires Windows administrator approval.'}
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class UnkxwynoUsbDriver {
    [DllImport("newdev.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    public static extern bool UpdateDriverForPlugAndPlayDevicesW(IntPtr window,string hardware,string inf,uint flags,[MarshalAs(UnmanagedType.Bool)] out bool restart);
    [DllImport("setupapi.dll",CharSet=CharSet.Unicode,SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    public static extern bool SetupGetInfDriverStoreLocationW(string inf,IntPtr platform,IntPtr locale,StringBuilder destination,uint capacity,out uint required);
}
'@
function Set-Binding([string]$Name,[string]$Hash) {
    $boot=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks
    if($script:journal.RebootRequired -and $script:journal.BootTicks -eq $boot){throw 'Windows requires a restart before more driver operations. Restart, reconnect the phone, then use driver recovery.'}
    $phone=Get-Phone
    if($phone.InstanceId -ine $script:journal.InstanceId){throw 'The original iPhone is not connected. Reconnect it to restore its Apple driver.'}
    $installed=Inf-Path $Name $Hash
    $store=New-Object Text.StringBuilder 4096;$required=0
    if(-not [UnkxwynoUsbDriver]::SetupGetInfDriverStoreLocationW($installed,[IntPtr]::Zero,[IntPtr]::Zero,$store,4096,[ref]$required)){
        throw (New-Object ComponentModel.Win32Exception([Runtime.InteropServices.Marshal]::GetLastWin32Error()))
    }
    # Use the complete, already installed DriverStore package, not a downloaded
    # INF, a user-selected path, or an INF copied without its signed files.
    if((Get-FileHash -LiteralPath $store.ToString() -Algorithm SHA256).Hash -ine $Hash){throw 'DriverStore package identity mismatch.'}
    $restart=$false
    if(-not [UnkxwynoUsbDriver]::UpdateDriverForPlugAndPlayDevicesW([IntPtr]::Zero,$hardware,$store.ToString(),1,[ref]$restart)){
        throw (New-Object ComponentModel.Win32Exception([Runtime.InteropServices.Marshal]::GetLastWin32Error()))
    }
    if($restart){$script:journal.RebootRequired=$true;Save-Journal;throw 'Windows requested a restart. Do not repeat USB capture until driver recovery after that restart.'}
}
function Restore-Original {
    if(-not $script:journal.RestorationRequired -or $script:journal.Restored){return}
    $binding=Get-Binding $script:journal.InstanceId
    if($binding.Service -ieq 'usbccgp' -and $binding.Inf -ieq $script:journal.OriginalInf -and $binding.Problem -eq 0){
        $script:journal.Restored=$true;Publish 'Restored' 'Apple USB driver is already restored.';return
    }
    Publish 'Restoring' 'Restoring the Apple USB driver. Keep the iPhone connected.'
    Set-Binding $script:journal.OriginalInf $originalHash
    for($i=0;$i -lt 40;$i++){
        $binding=Get-Binding $script:journal.InstanceId
        if($binding.Service -ieq 'usbccgp' -and $binding.Inf -ieq $script:journal.OriginalInf -and $binding.Problem -eq 0){
            $script:journal.Restored=$true;Publish 'Restored' 'Apple USB driver restored.';return
        }
        Start-Sleep -Milliseconds 500
    }
    throw 'Apple USB driver restoration was not verified. Keep the phone connected and run recovery for this session.'
}
if($Watchdog){
    while(Is-Alive $BrokerPid $BrokerStartTicks){Start-Sleep -Milliseconds 500}
    Load-Journal
    if($script:journal.BrokerPid -ne $BrokerPid -or $script:journal.BrokerStartTicks -ne $BrokerStartTicks){throw 'Watchdog owner mismatch.'}
    $recoveryMutex=New-Object Threading.Mutex($false,'Global\UNKXWYNO.iPhone.DriverSession');$recoveryLocked=$false
    try {
        try {$recoveryLocked=$recoveryMutex.WaitOne(30000)} catch [Threading.AbandonedMutexException] {$recoveryLocked=$true}
        if(-not $recoveryLocked){throw 'Another driver operation is active; recovery was not attempted.'}
        Load-Journal;Restore-Original
    } catch {Publish 'RecoveryRequired' $_.Exception.Message;exit 1}
    finally {if($recoveryLocked){$recoveryMutex.ReleaseMutex()};$recoveryMutex.Dispose()}
    exit 0
}
if($Recover){
    Load-Journal
    if((Is-Alive $script:journal.OwnerPid $script:journal.OwnerStartTicks) -or (Is-Alive $script:journal.BrokerPid $script:journal.BrokerStartTicks)){throw 'Close the desktop instance that owned this USB session, then reopen the app for recovery.'}
    $recoveryMutex=New-Object Threading.Mutex($false,'Global\UNKXWYNO.iPhone.DriverSession');$recoveryLocked=$false
    try {
        try {$recoveryLocked=$recoveryMutex.WaitOne(30000)} catch [Threading.AbandonedMutexException] {$recoveryLocked=$true}
        if(-not $recoveryLocked){throw 'Another driver operation is active; recovery was not attempted.'}
        Load-Journal;Restore-Original
    } catch {Publish 'RecoveryRequired' $_.Exception.Message;exit 1}
    finally {if($recoveryLocked){$recoveryMutex.ReleaseMutex()};$recoveryMutex.Dispose()}
    exit 0
}
if(-not (Is-Alive $OwnerPid $OwnerStartTicks)){throw 'The requesting desktop process is no longer running.'}
$owner=Get-Process -Id $OwnerPid
$expected=Join-Path $PSScriptRoot 'UnkxwynoMirror.Desktop.exe'
if($owner.Path -ine $expected){throw 'Only the desktop executable beside this helper may own a driver session.'}
$ownerRecord=Get-CimInstance Win32_Process -Filter ('ProcessId='+$OwnerPid)
$ownerSid=(Invoke-CimMethod -InputObject $ownerRecord -MethodName GetOwnerSid).Sid
if($ownerSid -cne [Security.Principal.WindowsIdentity]::GetCurrent().User.Value){throw 'Approve elevation with the same Windows account that runs this app.'}
if(Test-Path -LiteralPath $sessionDirectory){throw 'A fresh USB session directory is required.'}
New-Item -ItemType Directory -Path $sessionDirectory -Force | Out-Null
$script:journal=[ordered]@{
    SessionId=$SessionId.ToString('N');State='Preparing';Message='Checking installed iPhone USB drivers.'
    OwnerPid=$OwnerPid;OwnerStartTicks=$OwnerStartTicks;BrokerPid=$PID;BrokerStartTicks=(Get-Process -Id $PID).StartTime.ToUniversalTime().Ticks
    OriginalHash=$originalHash;CaptureHash=$captureHash;OriginalInf='';CaptureInf='';InstanceId=''
    RestorationRequired=$false;Restored=$false;RebootRequired=$false;Error=''
    BootTicks=(Get-CimInstance Win32_OperatingSystem).LastBootUpTime.ToUniversalTime().Ticks
}
Save-Journal
$mutex=New-Object Threading.Mutex($false,'Global\UNKXWYNO.iPhone.DriverSession')
$locked=$false
try {
    try {$locked=$mutex.WaitOne(0)} catch [Threading.AbandonedMutexException] {$locked=$true}
    if(-not $locked){throw 'Another iPhone driver session is active. Stop it before starting another.'}
    # Never erase or silently reuse a failed session's recovery journal.
    foreach($directory in Get-ChildItem -LiteralPath $sessionsRoot -Directory){
        if($directory.Name -eq $SessionId.ToString('N')){continue}
        $previous=Join-Path $directory.FullName 'driver-journal.json'
        if(Test-Path -LiteralPath $previous -PathType Leaf){
            Assert-NoReparse $previous
            if((Get-Item -LiteralPath $previous).Length -gt 16384){throw 'An earlier USB journal needs inspection.'}
            $old=Get-Content -LiteralPath $previous -Raw | ConvertFrom-Json
            if($old.RestorationRequired -and -not $old.Restored){throw 'An earlier driver session needs recovery. Close and reopen the app, then choose Mirror iPhone USB.'}
        }
    }
    if(Get-Process iTunes,AppleDevices -ErrorAction SilentlyContinue){throw 'Close iTunes or Apple Devices before USB mirroring, then try again.'}
    foreach($client in Get-Process AppleMobileDeviceProcess -ErrorAction SilentlyContinue){
        $signature=Get-AuthenticodeSignature -LiteralPath $client.Path
        if($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Apple Inc'){throw 'An unrecognized Apple USB client holds the device. Close it before trying again.'}
        Stop-Process -Id $client.Id -Force
    }
    $phone=Get-Phone;$binding=Get-Binding $phone.InstanceId
    if($binding.Service -ine 'usbccgp' -or $binding.Problem -ne 0){throw 'The expected healthy Apple USB driver is not present. No driver was changed.'}
    $null=Inf-Path $binding.Inf $originalHash
    $capture=@(Get-ChildItem -LiteralPath (Join-Path $env:WINDIR 'INF') -File -Filter 'oem*.inf' | Where-Object {(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -ieq $captureHash})
    if($capture.Count -ne 1){throw 'The verified capture driver is not installed. This app does not download or install unverified replacement drivers.'}
    $script:journal.InstanceId=$phone.InstanceId;$script:journal.OriginalInf=$binding.Inf;$script:journal.CaptureInf=$capture[0].Name;Save-Journal
    $powershell=Join-Path $env:WINDIR 'System32/WindowsPowerShell/v1.0/powershell.exe'
    $arguments='-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -Watchdog -SessionId '+$SessionId.ToString('N')+' -BrokerPid '+$PID+' -BrokerStartTicks '+$script:journal.BrokerStartTicks
    $watcher=Start-Process -FilePath $powershell -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(Test-Path -LiteralPath $stopPath){Publish 'Stopped' 'USB capture canceled before driver change.';exit 0}
    $script:journal.RestorationRequired=$true;Publish 'Switching' 'Temporarily preparing the installed iPhone capture driver.'
    Set-Binding $script:journal.CaptureInf $captureHash
    $ready=$false
    for($i=0;$i -lt 40;$i++){
        $binding=Get-Binding $phone.InstanceId
        if($binding.Service -ieq 'libusb0' -and $binding.Inf -ieq $script:journal.CaptureInf -and $binding.Problem -eq 0){$ready=$true;break}
        Start-Sleep -Milliseconds 500
    }
    if(-not $ready){throw 'The installed capture driver did not become ready.'}
    Publish 'Ready' 'Temporary USB capture access is ready.'
    # No streaming time limit. The owning app releases the lease only after
    # its native capture handles have closed. A process crash also releases it.
    while((Is-Alive $OwnerPid $OwnerStartTicks) -and -not (Test-Path -LiteralPath $stopPath)){Start-Sleep -Milliseconds 250}
} catch {
    $script:journal.Error=$_.Exception.Message;Publish 'Failed' $_.Exception.Message
} finally {
    try {Restore-Original} catch {Publish 'RecoveryRequired' $_.Exception.Message}
    if($locked){$mutex.ReleaseMutex()};$mutex.Dispose()
}
if($script:journal.State -in @('Failed','RecoveryRequired')){exit 1}

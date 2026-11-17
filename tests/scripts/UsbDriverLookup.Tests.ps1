# Read-only regression for the production helper's Windows API declaration.
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source=Get-Content -LiteralPath (Join-Path $repo 'apps/desktop/Services/Usb/UsbDriverSession.ps1') -Raw
$tokens=$null;$errors=$null
[Management.Automation.Language.Parser]::ParseInput($source,[ref]$tokens,[ref]$errors) | Out-Null
if($errors.Count){throw 'USB driver helper has PowerShell syntax errors.'}
$declaration=[regex]::Match($source,"(?s)Add-Type -TypeDefinition @'\r?\n(.*?)\r?\n'@").Groups[1].Value
if(-not $declaration){throw 'Production driver API declaration was not found.'}
Add-Type -TypeDefinition $declaration
# Resolve a standard installed Windows INF. This API does not modify drivers.
$inf=Join-Path $env:WINDIR 'INF/usb.inf'
$buffer=New-Object Text.StringBuilder 4096;$required=0
if(-not [UnkxwynoUsbDriver]::SetupGetInfDriverStoreLocationW($inf,[IntPtr]::Zero,[IntPtr]::Zero,$buffer,4096,[ref]$required)){
    throw (New-Object ComponentModel.Win32Exception([Runtime.InteropServices.Marshal]::GetLastWin32Error()))
}
$store=$buffer.ToString()
if(-not (Test-Path -LiteralPath $store -PathType Leaf) -or (Get-FileHash -LiteralPath $store).Hash -cne (Get-FileHash -LiteralPath $inf).Hash){throw 'DriverStore lookup did not return the matching installed INF.'}
Write-Host '[pass] Production Windows driver lookup resolves the installed package. No driver changes performed.'

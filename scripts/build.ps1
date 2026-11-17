[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',

    [ValidateSet('All', 'Desktop', 'Core')]
    [string]$Component = 'All'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Push-Location $repo
try {
    & "$PSScriptRoot/bootstrap.ps1" -Component $Component
    if ($Component -in @('All','Desktop')) { & "$PSScriptRoot/prepare-usb-tls.ps1" }
    if ($Component -in @('All', 'Core')) {
        & "$PSScriptRoot/prepare-usb-dependency.ps1"
        $preset = if ($Configuration -eq 'Release') { 'windows-release' } else { 'windows-debug' }
        cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { throw "Core configure failed: $LASTEXITCODE" }
        cmake --build --preset $preset
        if ($LASTEXITCODE -ne 0) { throw "Core build failed: $LASTEXITCODE" }
    }
    if ($Component -in @('All', 'Desktop')) {
        $desktopProject = 'apps/desktop/UnkxwynoMirror.Desktop.csproj'
        $desktopLock = 'apps/desktop/packages.lock.json'
        if (Test-Path -LiteralPath $desktopLock -PathType Leaf) {
            dotnet restore $desktopProject --locked-mode
            if ($LASTEXITCODE -ne 0) { throw "Desktop restore failed: $LASTEXITCODE" }
        } else {
            Write-Host '[info] Creating the initial desktop dependency lock file.'
            dotnet restore $desktopProject --use-lock-file
            if ($LASTEXITCODE -ne 0) { throw "Desktop restore failed: $LASTEXITCODE" }
            if (-not (Test-Path -LiteralPath $desktopLock -PathType Leaf)) {
                throw "Restore completed without creating $desktopLock."
            }
        }
        dotnet build $desktopProject -c $Configuration --no-restore
        if ($LASTEXITCODE -ne 0) { throw "Desktop build failed: $LASTEXITCODE" }
    }
} finally {
    Pop-Location
}


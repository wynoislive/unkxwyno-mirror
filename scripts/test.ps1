[CmdletBinding()]
param(
    [ValidateSet('All', 'Static', 'Desktop', 'Core', 'Usb')]
    [string]$Component = 'All'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Push-Location $repo
try {
    if ($Component -in @('All', 'Static')) {
        & "$PSScriptRoot/audit-independent-source.ps1"
        & 'tests/scripts/IndependenceAudit.Tests.ps1'
        & 'tests/scripts/PackageAudit.Tests.ps1'
    }
    if ($Component -in @('All', 'Desktop')) {
        & "$PSScriptRoot/bootstrap.ps1" -Component Desktop
        dotnet restore 'tests/desktop/UnkxwynoMirror.Desktop.Tests.csproj'
        if ($LASTEXITCODE -ne 0) { throw "Desktop test restore failed: $LASTEXITCODE" }
        dotnet run --project 'tests/desktop/UnkxwynoMirror.Desktop.Tests.csproj' -c Debug --no-restore
        if ($LASTEXITCODE -ne 0) { throw "Desktop tests failed: $LASTEXITCODE" }
    }
    if ($Component -in @('All', 'Core')) {
        & "$PSScriptRoot/prepare-usb-dependency.ps1"
        & "$PSScriptRoot/bootstrap.ps1" -Component Core
        cmake --preset windows-debug
        if ($LASTEXITCODE -ne 0) { throw "Core configure failed: $LASTEXITCODE" }
        cmake --build --preset windows-debug
        if ($LASTEXITCODE -ne 0) { throw "Core build failed: $LASTEXITCODE" }
        $inventoryText = ctest --preset windows-debug --show-only=json-v1
        if ($LASTEXITCODE -ne 0) { throw "Core test discovery failed: $LASTEXITCODE" }
        $inventory = ($inventoryText -join "`n") | ConvertFrom-Json
        foreach ($requiredTest in @('unkxwyno_core_tests', 'unkxwyno_usb_capture_tests')) {
            $matches = @($inventory.tests | Where-Object { $_.name -eq $requiredTest })
            if ($matches.Count -ne 1) { throw "Required native test must be discovered exactly once: $requiredTest" }
        }
        ctest --preset windows-debug --no-tests=error
        if ($LASTEXITCODE -ne 0) { throw "Core tests failed: $LASTEXITCODE" }
        & 'tests/scripts/NativeFailure.Tests.ps1'
        & 'tests/scripts/TestDiscovery.Tests.ps1'
    }
    if ($Component -in @('All', 'Usb')) {
        & 'tests/scripts/UsbDriverLookup.Tests.ps1'
        & "$PSScriptRoot/prepare-usb-tls.ps1"
        & 'tests/scripts/UsbTlsDependency.Tests.ps1'
        & "$PSScriptRoot/bootstrap.ps1" -Component Desktop
        dotnet run --project 'tests/usb/UnkxwynoMirror.Usb.Tests.csproj' -c Debug
        if ($LASTEXITCODE -ne 0) { throw "USB protocol boundary tests failed: $LASTEXITCODE" }
        dotnet run --project 'tests/usb_pairing/UnkxwynoMirror.UsbPairing.Tests.csproj' -c Debug
        if ($LASTEXITCODE -ne 0) { throw "Trusted USB pairing/TLS tests failed: $LASTEXITCODE" }
    }
} finally {
    Pop-Location
}


[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [ValidateSet('win-x64')]
    [string]$Runtime = 'win-x64',

    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $architecture = $Runtime.Substring('win-'.Length)
    $OutputPath = Join-Path $repo "out/windows-$architecture-publish"
} elseif (-not [IO.Path]::IsPathRooted($OutputPath)) {
    $OutputPath = Join-Path $repo $OutputPath
}
$OutputPath = [IO.Path]::GetFullPath($OutputPath)

if (Test-Path -LiteralPath $OutputPath) {
    $existing = @(Get-ChildItem -LiteralPath $OutputPath -Force)
    if ($existing.Count -gt 0) {
        throw "Publish output must be absent or empty so stale files cannot enter a release: $OutputPath"
    }
} else {
    New-Item -ItemType Directory -Path $OutputPath | Out-Null
}

Push-Location $repo
try {
    & "$PSScriptRoot/bootstrap.ps1" -Component Desktop
    & "$PSScriptRoot/prepare-usb-tls.ps1"
    $project = 'apps/desktop/UnkxwynoMirror.Desktop.csproj'
    dotnet restore $project --locked-mode
    if ($LASTEXITCODE -ne 0) {
        throw "Locked desktop restore failed with exit code $LASTEXITCODE."
    }

    # Do not add --no-build here. WinUI needs its publish target to produce the
    # publish-specific XBF and PRI resources copied by the project target.
    dotnet publish $project -c $Configuration -r $Runtime --no-restore -o $OutputPath
    if ($LASTEXITCODE -ne 0) {
        throw "Windows publish failed with exit code $LASTEXITCODE."
    }

    foreach ($required in @('UnkxwynoMirror.Desktop.exe', 'App.xbf', 'MainWindow.xbf', 'UnkxwynoMirror.Desktop.pri', 'UsbDriverSession.ps1')) {
        if (-not (Test-Path -LiteralPath (Join-Path $OutputPath $required) -PathType Leaf)) {
            throw "Published WinUI artifact is incomplete; missing $required."
        }
    }

    & "$PSScriptRoot/verify-independent-package.ps1" -PackagePath $OutputPath
    Write-Host "[pass] Verified Windows development artifact: $OutputPath"
} finally {
    Pop-Location
}

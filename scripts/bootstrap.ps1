[CmdletBinding()]
param(
    [ValidateSet('All', 'Desktop', 'Core')]
    [string]$Component = 'All'
)

$ErrorActionPreference = 'Stop'

function Add-PortableToolToPath {
    param([Parameter(Mandatory)][string]$ExecutableName)

    if (Get-Command $ExecutableName -ErrorAction SilentlyContinue) {
        return
    }

    $roots = @(
        (Join-Path $env:LOCALAPPDATA 'unkxwyno-toolchains'),
        (Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages')
    ) | Where-Object { Test-Path -LiteralPath $_ -PathType Container }

    $candidate = $roots | ForEach-Object {
        Get-ChildItem -LiteralPath $_ -Filter $ExecutableName -File -Recurse -ErrorAction SilentlyContinue
    } | Where-Object { $_.FullName -notmatch '[\\/]downloads[\\/]' } |
        Sort-Object FullName |
        Select-Object -First 1
    if ($candidate) {
        $directory = $candidate.Directory.FullName
        $env:PATH = "$directory$([IO.Path]::PathSeparator)$env:PATH"
        Write-Host "[info] Using portable $ExecutableName from $directory"
    }
}

if ($Component -in @('All', 'Core')) {
    foreach ($executable in @('cmake.exe', 'ninja.exe', 'clang-cl.exe', 'cl.exe', 'clang++.exe', 'g++.exe')) {
        Add-PortableToolToPath $executable
    }
}

$required = @()
if ($Component -in @('All', 'Desktop')) {
    $required += @{
        Name = '.NET SDK 10'
        Probe = {
            (Get-Command dotnet -ErrorAction SilentlyContinue) -and
            ((dotnet --list-sdks) -match '^10\.0\.')
        }
    }
}
if ($Component -in @('All', 'Core')) {
    $required += @(
        @{ Name = 'CMake'; Probe = { Get-Command cmake -ErrorAction SilentlyContinue } },
        @{ Name = 'Ninja'; Probe = { Get-Command ninja -ErrorAction SilentlyContinue } },
        @{
            Name = 'C++ compiler'
            Probe = {
                (Get-Command clang-cl -ErrorAction SilentlyContinue) -or
                (Get-Command cl -ErrorAction SilentlyContinue) -or
                (Get-Command clang++ -ErrorAction SilentlyContinue) -or
                (Get-Command g++ -ErrorAction SilentlyContinue)
            }
        }
    )
}

$missing = @()
foreach ($item in $required) {
    $available = & $item.Probe
    if ($available) {
        Write-Host "[ok] $($item.Name)"
    } else {
        Write-Host "[missing] $($item.Name)"
        $missing += $item.Name
    }
}

if ($missing.Count -gt 0) {
    throw "Missing $Component prerequisites: $($missing -join ', '). See docs/BUILD_STATUS.md."
}

Write-Host "[ok] Independent $Component prerequisites are available."


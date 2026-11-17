[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$audit = Join-Path $repo 'scripts/verify-independent-package.ps1'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("unkxwyno-package-audit-" + [Guid]::NewGuid().ToString('N'))

function Assert-Throws {
    param(
        [Parameter(Mandatory)][scriptblock]$Action,
        [Parameter(Mandatory)][string]$ExpectedText
    )

    try {
        & $Action
    } catch {
        if ($_.Exception.Message -notlike "*$ExpectedText*") {
            throw "Expected failure containing '$ExpectedText' but received: $($_.Exception.Message)"
        }
        return
    }
    throw "Expected failure containing '$ExpectedText', but the command passed."
}

try {
    [IO.Directory]::CreateDirectory($testRoot) | Out-Null
    Assert-Throws { & $audit -PackagePath $testRoot } 'package is empty'

    $clean = Join-Path $testRoot 'unkxwyno-mirror.exe'
    [IO.File]::WriteAllBytes($clean, [byte[]](1, 2, 3, 4))
    & $audit -PackagePath $testRoot

    $legacy = Join-Path $testRoot 'renamed-douwan-component.dll'
    [IO.File]::WriteAllBytes($legacy, [byte[]](5, 6, 7, 8))
    Assert-Throws { & $audit -PackagePath $testRoot } 'forbidden legacy material'
    Remove-Item -LiteralPath $legacy

    $partialTls=Join-Path $testRoot 'libcrypto-3-x64.dll'
    [IO.File]::WriteAllBytes($partialTls,[byte[]](1,2,3,4))
    Assert-Throws { & $audit -PackagePath $testRoot } 'Published USB TLS dependency identity mismatch'
    Remove-Item -LiteralPath $partialTls
    $desktop=Join-Path $testRoot 'UnkxwynoMirror.Desktop.exe'
    [IO.File]::WriteAllBytes($desktop,[byte[]](1,2,3,4))
    Assert-Throws { & $audit -PackagePath $testRoot } 'Published USB TLS dependency identity mismatch'
    Write-Host '[pass] Package audit rejects empty/legacy artifacts, partial TLS dependencies and a desktop package missing TLS.'
} finally {
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $resolvedTest = [IO.Path]::GetFullPath($testRoot)
    if ($resolvedTest.StartsWith($resolvedTemp + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolvedTest -Recurse -Force -ErrorAction SilentlyContinue
    }
}

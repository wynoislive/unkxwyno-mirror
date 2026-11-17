[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$audit = Join-Path $repo 'scripts/audit-independent-source.ps1'
$testRoot = Join-Path ([IO.Path]::GetTempPath()) ("unkxwyno-source-audit-" + [Guid]::NewGuid().ToString('N'))
$appsRoot = Join-Path $testRoot 'apps'

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
    [IO.Directory]::CreateDirectory($appsRoot) | Out-Null
    $sourcePath = Join-Path $appsRoot 'Receiver.cs'
    [IO.File]::WriteAllText($sourcePath, 'internal sealed class Receiver {}')
    & $audit -RootPath $testRoot -ScanRelativePaths @('apps')

    [IO.File]::WriteAllText($sourcePath, 'internal const string Legacy = "DouWan";')
    Assert-Throws { & $audit -RootPath $testRoot -ScanRelativePaths @('apps') } 'legacy DouWan dependency'

    [IO.File]::WriteAllText($sourcePath, 'internal const string Billing = "subscription";')
    Assert-Throws { & $audit -RootPath $testRoot -ScanRelativePaths @('apps') } 'paid-feature mechanism'
    Write-Host '[pass] Independence source audit detects controlled legacy and monetization regressions.'
} finally {
    $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $resolvedTest = [IO.Path]::GetFullPath($testRoot)
    if ($resolvedTest.StartsWith($resolvedTemp + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolvedTest -Recurse -Force -ErrorAction SilentlyContinue
    }
}

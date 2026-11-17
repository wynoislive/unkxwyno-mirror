$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script:ctestInvoked = $false

# Inject a deterministic configure failure. The real orchestration must throw
# immediately and must never launch CTest against a missing or stale executable.
function global:cmake {
    $global:LASTEXITCODE = 17
}
function global:ctest {
    $script:ctestInvoked = $true
    $global:LASTEXITCODE = 0
}
try {
    foreach ($entryPoint in @('test.ps1', 'build.ps1')) {
        $rejected = $false
        try {
            & (Join-Path $repo "scripts/$entryPoint") -Component Core
        } catch {
            if ($_.Exception.Message -notlike 'Core configure failed: 17') {
                throw
            }
            $rejected = $true
        }
        if (-not $rejected) {
            throw "$entryPoint returned success after a native configure failure."
        }
    }
    if ($script:ctestInvoked) {
        throw 'CTest ran after configuration failed.'
    }
    Write-Host '[pass] Build/test orchestration rejects native failures before running downstream commands.'
} finally {
    Remove-Item -LiteralPath Function:\cmake -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath Function:\ctest -ErrorAction SilentlyContinue
}

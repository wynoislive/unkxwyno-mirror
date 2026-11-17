$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$script:executed = $false
function global:cmake { $global:LASTEXITCODE = 0 }
function global:ctest {
    if ($args -contains '--show-only=json-v1') {
        '{"tests":[{"name":"unkxwyno_core_tests"}]}'
    } else { $script:executed = $true }
    $global:LASTEXITCODE = 0
}
try {
    $rejected = $false
    try { & (Join-Path $repo 'scripts/test.ps1') -Component Core }
    catch {
        if ($_.Exception.Message -ne 'Required native test must be discovered exactly once: unkxwyno_usb_capture_tests') { throw }
        $rejected = $true
    }
    if (-not $rejected -or $script:executed) {
        throw 'Native lane did not reject a missing USB regression executable before running CTest.'
    }
    Write-Host '[pass] Missing USB test discovery cannot turn the native lane green.'
} finally {
    Remove-Item -LiteralPath Function:\cmake -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath Function:\ctest -ErrorAction SilentlyContinue
}

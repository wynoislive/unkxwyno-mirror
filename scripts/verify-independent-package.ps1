[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$PackagePath
)

$ErrorActionPreference = 'Stop'
$resolved = (Resolve-Path -LiteralPath $PackagePath).Path
if (-not (Test-Path -LiteralPath $resolved -PathType Container)) {
    throw 'PackagePath must be a directory.'
}

$forbiddenNamePattern = '(?i)(douwan|libairplay|airplayinput|airplayreceiver|scrcpy|(^|[^a-z])adb([^a-z]|$))'
$forbiddenHashes = @(
    '81a43b30558e34af6cb9c3056b07e8bc93d88ecb23e68257bb246da07f552f02',
    '9d012d7452a4b2d65bf5046e3e97ad747e38110036c7aa86666ea23553b98476',
    '8a827624296b38ab0f025ee539e71b7d8bee75e69f9c0e412fb83ca1b2246988'
)

$entries = @(Get-ChildItem -LiteralPath $resolved -Recurse -Force)
$links = @($entries | Where-Object { ($_.Attributes -band [IO.FileAttributes]::ReparsePoint) -ne 0 })
if ($links.Count -gt 0) {
    throw "Independent package contains reparse points that escape complete inspection:`n$($links.FullName -join "`n")"
}

$files = @($entries | Where-Object { -not $_.PSIsContainer })
if ($files.Count -eq 0) {
    throw 'Independent package is empty; there is no artifact to verify.'
}

$nameHits = @($files | Where-Object { $_.Name -match $forbiddenNamePattern })
$hashHits = foreach ($file in $files) {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    if ($forbiddenHashes -contains $hash) { $file.FullName }
}

if ($nameHits -or $hashHits) {
    $bad = @(@($nameHits.FullName) + @($hashHits) | Sort-Object -Unique)
    throw "Independent package contains forbidden legacy material:`n$($bad -join "`n")"
}

$usbLibrary = Join-Path $resolved 'libusb-1.0.dll'
if (Test-Path -LiteralPath $usbLibrary -PathType Leaf) {
    if ((Get-FileHash -LiteralPath $usbLibrary -Algorithm SHA256).Hash -ine '5bd409849825009b6fe25861a6147f76d256aab248f07049d78387e3bff12d94') {
        throw 'Published libusb runtime does not match the pinned upstream build.'
    }
    foreach ($notice in @('libusb-LICENSE.txt', 'THIRD_PARTY_NOTICES.md')) {
        $noticePath = Join-Path $resolved $notice
        if (-not (Test-Path -LiteralPath $noticePath -PathType Leaf) -or (Get-Item -LiteralPath $noticePath).Length -eq 0) {
            throw "Published USB runtime is missing its license/notice: $notice"
        }
    }
}
$tlsPins = @{
    'libssl-3-x64.dll' = '6af55a4d522e2cc29ff0833cad4f6f7560f8ba2d107b97990d9415c590a4224a'
    'libcrypto-3-x64.dll' = 'f8839fd7d5b5e7b1763c88705538c5762a2f97df17f37092e37de3c663f30b1e'
    'OpenSSL-LICENSE.txt' = '7d5450cb2d142651b8afa315b5f238efc805dad827d91ba367d8516bc9d49e7a'
}
$hasTlsFiles=@($tlsPins.Keys | Where-Object {Test-Path -LiteralPath (Join-Path $resolved $_) -PathType Leaf}).Count -gt 0
if ($hasTlsFiles -or (Test-Path -LiteralPath (Join-Path $resolved 'UnkxwynoMirror.Desktop.exe') -PathType Leaf)) {
    foreach ($name in $tlsPins.Keys) {
        $library = Join-Path $resolved $name
        if (-not (Test-Path -LiteralPath $library -PathType Leaf) -or (Get-FileHash -LiteralPath $library -Algorithm SHA256).Hash -ine $tlsPins[$name]) { throw "Published USB TLS dependency identity mismatch: $name" }
    }
}

Write-Host "[pass] $($files.Count) files contain no known legacy names or primary artifact hashes."


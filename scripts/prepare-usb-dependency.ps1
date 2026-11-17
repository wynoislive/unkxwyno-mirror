[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$root = Join-Path $repo 'out/deps/libusb-1.0.30'
$archive = Join-Path $repo 'out/deps/libusb-1.0.30.7z'
$source = Join-Path $repo 'out/deps/libusb-1.0.30.tar.bz2'
$binaryHash = '7fb1dfec805b97983763d7d0ae244320da12add1003d4249c96cc4d586398c79'
$sourceHash = 'fea36f34f9156400209595e300840767ab1a385ede1dc7ee893015aea9c6dbaf'
New-Item -ItemType Directory -Path $root -Force | Out-Null
foreach ($item in @(
    @{ Path=$archive; Name='libusb-1.0.30.7z'; Hash=$binaryHash },
    @{ Path=$source; Name='libusb-1.0.30.tar.bz2'; Hash=$sourceHash }
)) {
    if (-not (Test-Path -LiteralPath $item.Path)) {
        Invoke-WebRequest -Uri ('https://github.com/libusb/libusb/releases/download/v1.0.30/' + $item.Name) -OutFile $item.Path
    }
    if ((Get-FileHash -LiteralPath $item.Path -Algorithm SHA256).Hash -ine $item.Hash) {
        throw "Pinned libusb archive digest mismatch: $($item.Path)"
    }
}
# Extract only named members from the digest-verified upstream archives.
tar -xf $archive -C $root 'MinGW64/dll/libusb-1.0.dll' 'include/libusb.h'
if ($LASTEXITCODE -ne 0) { throw 'libusb binary extraction failed.' }
tar -xf $source -C $root 'libusb-1.0.30/COPYING'
if ($LASTEXITCODE -ne 0) { throw 'libusb license extraction failed.' }
Write-Host '[ok] libusb 1.0.30 x64 dependency and LGPL license verified.'
$directHeader = Join-Path $root 'include/lusb0_usb.h'
if (-not (Test-Path -LiteralPath $directHeader)) {
    Invoke-WebRequest -Uri 'https://raw.githubusercontent.com/mcuee/libusb-win32/2e8aaf2f1fa0b98a115015419c0689c16be3ec55/libusb/src/lusb0_usb.h' -OutFile $directHeader
}
if ((Get-FileHash -LiteralPath $directHeader -Algorithm SHA256).Hash -ine 'e76f3ee6f230f7e4d28baaf989bb44eda17d93877a30147ca508651201f89c6c') {
    throw 'Pinned libusb-win32 public header digest mismatch.'
}

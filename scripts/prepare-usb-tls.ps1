[CmdletBinding()]
param([string]$ArchivePath)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$target=Join-Path $repo 'out/deps/openssl-3.5.9'
# Independent Apache-2.0 libraries from the official portable Git distribution.
# No installed Git, reference receiver or archive executable is used.
$archiveHash='4a92850b6e917da4fd010669b614b55dccf5b0ec333a534cc4be4700dfefdafd'
$archiveUrl='https://github.com/git-for-windows/git/releases/download/v2.56.1.windows.1/MinGit-2.56.1-64-bit.zip'
$pins=@{
    'libssl-3-x64.dll'='6af55a4d522e2cc29ff0833cad4f6f7560f8ba2d107b97990d9415c590a4224a'
    'libcrypto-3-x64.dll'='f8839fd7d5b5e7b1763c88705538c5762a2f97df17f37092e37de3c663f30b1e'
    'OpenSSL-LICENSE.txt'='7d5450cb2d142651b8afa315b5f238efc805dad827d91ba367d8516bc9d49e7a'
}
New-Item -ItemType Directory -Path $target -Force | Out-Null
$cached=$true
foreach($name in $pins.Keys){
    $file=Join-Path $target $name
    if(-not (Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ine $pins[$name]){$cached=$false}
}
if($cached -and [string]::IsNullOrWhiteSpace($ArchivePath)){
    Write-Host '[ok] Cached independent OpenSSL libraries and license verified.'
    return
}
if([string]::IsNullOrWhiteSpace($ArchivePath)){
    $ArchivePath=Join-Path $repo 'out/deps/MinGit-2.56.1-64-bit.zip'
    if(-not (Test-Path -LiteralPath $ArchivePath -PathType Leaf)){
        $download=$ArchivePath+'.download'
        try {
            Invoke-WebRequest -Uri $archiveUrl -OutFile $download -UseBasicParsing
            if((Get-FileHash -LiteralPath $download -Algorithm SHA256).Hash -ine $archiveHash){throw 'Official TLS dependency archive digest mismatch.'}
            Move-Item -LiteralPath $download -Destination $ArchivePath
        } finally {if(Test-Path -LiteralPath $download -PathType Leaf){Remove-Item -LiteralPath $download}}
    }
}
if((Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash -ine $archiveHash){throw 'Official TLS dependency archive digest mismatch.'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($ArchivePath)
try {
    # Exact members only: never expand an archive-controlled path into the tree.
    $verified=@{}
    foreach($name in $pins.Keys){
        $member=if($name -eq 'OpenSSL-LICENSE.txt'){'ucrt64/share/licenses/openssl/LICENSE'}else{'ucrt64/bin/'+$name}
        $entries=@($archive.Entries | Where-Object {$_.FullName -ceq $member})
        if($entries.Count -ne 1 -or $entries[0].Length -gt 8MB){throw "Invalid TLS dependency archive member: $name"}
        $stream=$entries[0].Open();$memory=New-Object IO.MemoryStream;$sha=[Security.Cryptography.SHA256]::Create()
        try {
            $stream.CopyTo($memory);$bytes=$memory.ToArray()
            $digest=([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-','')
            if($digest -ine $pins[$name]){throw "Pinned independent OpenSSL member mismatch: $name"}
            $verified[$name]=$bytes
        } finally {$stream.Dispose();$memory.Dispose();$sha.Dispose()}
    }
    foreach($name in $verified.Keys){[IO.File]::WriteAllBytes((Join-Path $target $name),$verified[$name])}
} finally {$archive.Dispose()}
Write-Host '[ok] Independent OpenSSL 3.5.9 x64 libraries and license verified.'

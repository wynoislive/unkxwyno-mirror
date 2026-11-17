$ErrorActionPreference='Stop'
$repo=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$prepare=Join-Path $repo 'scripts/prepare-usb-tls.ps1'
$dependency=Join-Path $repo 'out/deps/openssl-3.5.9'
$names=@('libssl-3-x64.dll','libcrypto-3-x64.dll','OpenSSL-LICENSE.txt')
$before=@{}
foreach($name in $names){$before[$name]=(Get-FileHash -LiteralPath (Join-Path $dependency $name) -Algorithm SHA256).Hash}
$temporary=Join-Path ([IO.Path]::GetTempPath()) ('unkxwyno-tls-dependency-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    $untrusted=Join-Path $temporary 'untrusted.zip'
    [IO.File]::WriteAllBytes($untrusted,[byte[]]@(0,1,2,3))
    $rejected=$false
    try { & $prepare -ArchivePath $untrusted }
    catch {
        if($_.Exception.Message -ne 'Official TLS dependency archive digest mismatch.'){throw}
        $rejected=$true
    }
    if(-not $rejected){throw 'An explicitly supplied untrusted TLS archive was accepted.'}
    foreach($name in $names){
        if((Get-FileHash -LiteralPath (Join-Path $dependency $name) -Algorithm SHA256).Hash -cne $before[$name]){throw "Untrusted archive changed the prepared dependency: $name"}
    }
    & $prepare
    Write-Host '[pass] TLS cache verification and explicit untrusted-archive rejection preserve prepared libraries.'
} finally {
    # Delete only the exact files and directory created by this test.
    if(Test-Path -LiteralPath (Join-Path $temporary 'untrusted.zip')){Remove-Item -LiteralPath (Join-Path $temporary 'untrusted.zip')}
    Remove-Item -LiteralPath $temporary
}

[CmdletBinding()]
param(
    [string]$RootPath = (Split-Path -Parent $PSScriptRoot),
    [string[]]$ScanRelativePaths = @('apps', 'core', 'src/backend/independent')
)

$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $RootPath).Path
$textExtensions = @(
    '.c', '.cc', '.cmake', '.cpp', '.cs', '.csproj', '.entitlements', '.gradle',
    '.h', '.hpp', '.json', '.kts', '.plist', '.props', '.ps1', '.swift', '.toml', '.xml', '.xaml'
)
$textNames = @('CMakeLists.txt', 'Package.swift', 'Podfile', 'gradlew')
$generatedDirectoryPattern = '(?i)[\\/](bin|obj|build|out|\.gradle|\.swiftpm|DerivedData)[\\/]'
$rules = [ordered]@{
    'legacy DouWan dependency' = '(?i)\bdouwan\b'
    'AirPlay dependency or protocol' = '(?i)\bairplay\b|libairplay|airplayinput|airplayreceiver'
    'scrcpy dependency' = '(?i)\bscrcpy\b'
    'Miracast dependency or protocol' = '(?i)\bmiracast\b'
    'paid-feature mechanism' = '(?i)\b(paywall|subscription|premium|watermark|advertisement|admob|billingclient)\b|in-app purchase'
}

$files = @()
foreach ($relativePath in $ScanRelativePaths) {
    $candidate = Join-Path $resolvedRoot $relativePath
    if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
        throw "Required source root is missing: $candidate"
    }
    $files += Get-ChildItem -LiteralPath $candidate -Recurse -File |
        Where-Object {
            $_.FullName -notmatch $generatedDirectoryPattern -and
            (($textExtensions -contains $_.Extension.ToLowerInvariant()) -or ($textNames -contains $_.Name))
        }
}

if ($files.Count -eq 0) {
    throw 'No independent runtime source files were discovered.'
}

$hits = [Collections.Generic.List[string]]::new()
foreach ($file in $files | Sort-Object FullName -Unique) {
    $content = [IO.File]::ReadAllText($file.FullName)
    foreach ($rule in $rules.GetEnumerator()) {
        if ($content -match $rule.Value) {
            $relative = $file.FullName.Substring($resolvedRoot.Length).TrimStart([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
            $hits.Add("$relative : $($rule.Key)")
        }
    }
}

if ($hits.Count -gt 0) {
    throw "Independent runtime source audit failed:`n$($hits -join "`n")"
}

Write-Host "[pass] $($files.Count) independent runtime source files contain no prohibited legacy, protocol, or monetization dependencies."

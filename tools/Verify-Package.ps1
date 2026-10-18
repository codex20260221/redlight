param(
    [string]$Package = 'build/packages/RedLight-0.6.0-rc.1-windows-x64.zip',
    [string]$Binary = 'build/Release/RedLight.exe',
    [string]$ExpectedRoot = 'RedLight-0.6.0-rc.1-windows-x64'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PackagePaths.psm1') -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repoRoot = Split-Path -Parent $PSScriptRoot
$expectedFiles = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::Ordinal)
foreach ($relative in @('RedLight.exe', 'RedLight.exe.sha256', 'README.md', 'CHANGELOG.md', 'LICENSE')) {
    $expectedFiles.Add($relative, (Join-Path $repoRoot $relative))
}
$documentationRoot = Join-Path $repoRoot 'docs'
foreach ($document in Get-ChildItem -LiteralPath $documentationRoot -File -Recurse) {
    $relative = 'docs/' + $document.FullName.Substring($documentationRoot.Length + 1).Replace('\', '/')
    $expectedFiles.Add($relative, $document.FullName)
}
$allowedDirectories = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$null = $allowedDirectories.Add('')
$null = $allowedDirectories.Add('docs')
foreach ($directory in Get-ChildItem -LiteralPath $documentationRoot -Directory -Recurse) {
    $null = $allowedDirectories.Add('docs/' + $directory.FullName.Substring($documentationRoot.Length + 1).Replace('\', '/'))
}
$packageItem = Get-Item -LiteralPath $Package
$binaryItem = Get-Item -LiteralPath $Binary
$binaryHash = (Get-FileHash -LiteralPath $binaryItem.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
$zip = [System.IO.Compression.ZipFile]::OpenRead($packageItem.FullName)
$found = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
try {
    $paths = @(Read-PackageEntryPaths -Names @($zip.Entries | ForEach-Object { $_.FullName }) -ExpectedRoot $ExpectedRoot)
    for ($index = 0; $index -lt $paths.Count; ++$index) {
        $entry = $zip.Entries[$index]
        $relative = $paths[$index].RelativePath
        if ($paths[$index].IsDirectory) {
            if ($entry.Length -ne 0 -or -not $allowedDirectories.Contains($relative)) { throw "Unexpected package directory content: $relative" }
            continue
        }
        if (-not $expectedFiles.ContainsKey($relative)) { throw "Unexpected package content: $relative" }
        if ($relative -eq 'RedLight.exe.sha256') {
            if ($entry.Length -gt 128) { throw 'Packaged executable checksum is oversized.' }
        } else {
            $sourcePath = if ($relative -eq 'RedLight.exe') { $binaryItem.FullName } else { $expectedFiles[$relative] }
            if ($entry.Length -ne (Get-Item -LiteralPath $sourcePath).Length) { throw "Incorrect package content length: $relative" }
        }
        $null = $found.Add($relative)
        $entryStream = $entry.Open()
        try {
            if ($relative -eq 'RedLight.exe.sha256') {
                $reader = [System.IO.StreamReader]::new($entryStream)
                try { $checksum = $reader.ReadToEnd().Trim() } finally { $reader.Dispose() }
                if ($checksum -ne "$binaryHash  RedLight.exe") { throw 'Packaged executable checksum mismatch.' }
            } else {
                $sha = [System.Security.Cryptography.SHA256]::Create()
                try { $entryHash = ([BitConverter]::ToString($sha.ComputeHash($entryStream))).Replace('-', '').ToLowerInvariant() }
                finally { $sha.Dispose() }
                if ($relative -eq 'RedLight.exe') {
                    $expectedHash = $binaryHash
                } else {
                    $expectedHash = (Get-FileHash -LiteralPath $expectedFiles[$relative] -Algorithm SHA256).Hash.ToLowerInvariant()
                }
                if ($entryHash -ne $expectedHash) { throw "Stale or incorrect package content: $relative" }
            }
        } finally { $entryStream.Dispose() }
    }
    foreach ($required in $expectedFiles.Keys) {
        if (-not $found.Contains($required)) { throw "Missing package content: $required" }
    }
} finally { $zip.Dispose() }
$hash = Get-FileHash -LiteralPath $packageItem.FullName -Algorithm SHA256
"$($hash.Hash.ToLowerInvariant())  $($packageItem.Name)" | Set-Content -LiteralPath "$($packageItem.FullName).sha256" -Encoding ascii
Write-Output "Verified portable package: $($found.Count) files match current sources/binary; SHA-256 written."

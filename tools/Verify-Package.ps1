param(
    [string]$Package = 'build/packages/RedLight-0.6.0-rc.1-windows-x64.zip',
    [string]$Binary = 'build/Release/RedLight.exe'
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$repoRoot = Split-Path -Parent $PSScriptRoot
$packageItem = Get-Item -LiteralPath $Package
$binaryHash = (Get-FileHash -LiteralPath $Binary -Algorithm SHA256).Hash.ToLowerInvariant()
$zip = [System.IO.Compression.ZipFile]::OpenRead($packageItem.FullName)
$found = @{}
try {
    foreach ($entry in $zip.Entries) {
        if ($entry.Name -eq '') { continue }
        # CPack places files under one named top-level directory.
        $relative = ($entry.FullName -split '/', 2)[1]
        if (-not $relative -or $relative -match '(^/|(^|/)\.\.(/|$))') { throw 'Invalid package path.' }
        if ($found.ContainsKey($relative)) { throw "Duplicate package entry: $relative" }
        $found[$relative] = $true
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
                } elseif ($relative -in @('README.md', 'CHANGELOG.md', 'LICENSE') -or $relative.StartsWith('docs/')) {
                    $expectedHash = (Get-FileHash -LiteralPath (Join-Path $repoRoot $relative) -Algorithm SHA256).Hash.ToLowerInvariant()
                } else { throw "Unexpected package content: $relative" }
                if ($entryHash -ne $expectedHash) { throw "Stale or incorrect package content: $relative" }
            }
        } finally { $entryStream.Dispose() }
    }
    foreach ($required in @('RedLight.exe', 'RedLight.exe.sha256', 'README.md', 'CHANGELOG.md', 'LICENSE', 'docs/release-validation.md')) {
        if (-not $found.ContainsKey($required)) { throw "Missing package content: $required" }
    }
} finally { $zip.Dispose() }
$hash = Get-FileHash -LiteralPath $packageItem.FullName -Algorithm SHA256
"$($hash.Hash.ToLowerInvariant())  $($packageItem.Name)" | Set-Content -LiteralPath "$($packageItem.FullName).sha256" -Encoding ascii
Write-Output "Verified portable package: $($found.Count) files match current sources/binary; SHA-256 written."

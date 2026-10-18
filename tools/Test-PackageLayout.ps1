$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.IO.Compression
# Real checker, synthetic executable bytes, existing documentation as data.
# No inspected image is executed, and no archive is extracted.
$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDirectory = [IO.Path]::GetFullPath((Join-Path $repoRoot 'build'))
$fixtureDirectory = [IO.Path]::GetFullPath((Join-Path $buildDirectory ('package-code-fixture-' + [Guid]::NewGuid().ToString('N'))))
if (-not $fixtureDirectory.StartsWith($buildDirectory + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Fixture directory must remain inside the build directory.'
}
$binary = Join-Path $fixtureDirectory 'binary.data'
$fixture = Join-Path $fixtureDirectory 'package.zip'
$binaryBytes = [byte[]]@(0, 1, 2, 3, 255)
$files = [Collections.Generic.Dictionary[string, string]]::new([StringComparer]::Ordinal)
foreach ($name in @('README.md', 'CHANGELOG.md', 'LICENSE')) { $files.Add($name, (Join-Path $repoRoot $name)) }
$documentationRoot = Join-Path $repoRoot 'docs'
foreach ($document in Get-ChildItem -LiteralPath $documentationRoot -File -Recurse) {
    $files.Add('docs/' + $document.FullName.Substring($documentationRoot.Length + 1).Replace('\', '/'), $document.FullName)
}

function Write-FixtureEntry($Archive, [string]$Name, [byte[]]$Bytes) {
    $entry = $Archive.CreateEntry($Name)
    $stream = $entry.Open()
    try { $stream.Write($Bytes, 0, $Bytes.Length) } finally { $stream.Dispose() }
}
function New-FixtureArchive([string]$Case) {
    foreach ($path in @($fixture, "$fixture.sha256")) { if ([IO.File]::Exists($path)) { [IO.File]::Delete($path) } }
    $archive = [IO.Compression.ZipFile]::Open($fixture, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($pair in $files.GetEnumerator()) {
            if ($Case -eq 'MissingDoc' -and $pair.Key -eq 'docs/magnification-probe.md') { continue }
            $name = 'DataRoot/' + $pair.Key
            if ($Case -eq 'MultipleRoots' -and $pair.Key -eq 'LICENSE') { $name = 'OtherRoot/LICENSE' }
            if ($Case -eq 'CaseAlias' -and $pair.Key -eq 'README.md') { $name = 'DataRoot/readme.md' }
            $bytes = [IO.File]::ReadAllBytes($pair.Value)
            if ($Case -eq 'StaleDoc' -and $pair.Key -eq 'README.md') { $bytes[0] = $bytes[0] -bxor 1 }
            Write-FixtureEntry $archive $name $bytes
            if ($Case -eq 'Duplicate' -and $pair.Key -eq 'README.md') { Write-FixtureEntry $archive $name $bytes }
        }
        $image = [byte[]]$binaryBytes.Clone()
        if ($Case -eq 'StaleBinary') { $image[0] = 9 }
        if ($Case -eq 'Length') { $image = [byte[]]@(0, 1) }
        Write-FixtureEntry $archive 'DataRoot/RedLight.exe' $image
        $checksum = "$binaryHash  RedLight.exe`r`n"
        if ($Case -eq 'Checksum') { $checksum = ('0' * 64) + '  RedLight.exe' }
        if ($Case -eq 'OversizedChecksum') { $checksum = '0' * 1024 }
        Write-FixtureEntry $archive 'DataRoot/RedLight.exe.sha256' ([Text.Encoding]::ASCII.GetBytes($checksum))
        if ($Case -eq 'UnexpectedDirectory') { Write-FixtureEntry $archive 'DataRoot/extra/' ([byte[]]@()) }
        if ($Case -eq 'DirectoryPayload') { Write-FixtureEntry $archive 'DataRoot/docs/' ([byte[]]@(1)) }
        if ($Case -eq 'TraversalDirectory') { Write-FixtureEntry $archive 'DataRoot/docs/../' ([byte[]]@()) }
        if ($Case -eq 'ParentFile') { Write-FixtureEntry $archive 'DataRoot/docs' ([byte[]]@()) }
    } finally { $archive.Dispose() }
}

$cases = @(
    @{ Name = 'Canonical synthetic package'; Case = 'Valid'; Error = '' },
    @{ Name = 'Entries split across roots'; Case = 'MultipleRoots'; Error = 'Package entry must be beneath the expected root:' },
    @{ Name = 'Duplicate entry'; Case = 'Duplicate'; Error = 'Duplicate or aliased package entry:' },
    @{ Name = 'Unexpected empty directory'; Case = 'UnexpectedDirectory'; Error = 'Unexpected package directory content:' },
    @{ Name = 'Nonempty directory payload'; Case = 'DirectoryPayload'; Error = 'Unexpected package directory content:' },
    @{ Name = 'Traversal in directory entry'; Case = 'TraversalDirectory'; Error = 'Invalid package path:' },
    @{ Name = 'File used as implicit parent'; Case = 'ParentFile'; Error = 'Package file/directory collision:' },
    @{ Name = 'Missing previously optional documentation'; Case = 'MissingDoc'; Error = 'Missing package content: docs/magnification-probe.md' },
    @{ Name = 'Incorrect source spelling'; Case = 'CaseAlias'; Error = 'Unexpected package content: readme.md' },
    @{ Name = 'Stale same-length executable data'; Case = 'StaleBinary'; Error = 'Stale or incorrect package content: RedLight.exe' },
    @{ Name = 'Stale same-length documentation'; Case = 'StaleDoc'; Error = 'Stale or incorrect package content: README.md' },
    @{ Name = 'Mismatched executable checksum'; Case = 'Checksum'; Error = 'Packaged executable checksum mismatch.' },
    @{ Name = 'Oversized checksum before text read'; Case = 'OversizedChecksum'; Error = 'Packaged executable checksum is oversized.' },
    @{ Name = 'Incorrect executable data length'; Case = 'Length'; Error = 'Incorrect package content length: RedLight.exe' }
)
try {
    $null = [IO.Directory]::CreateDirectory($fixtureDirectory)
    [IO.File]::WriteAllBytes($binary, $binaryBytes)
    $binaryHash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    foreach ($case in $cases) {
        New-FixtureArchive $case.Case
        $message = ''
        try { & (Join-Path $PSScriptRoot 'Verify-Package.ps1') -Package $fixture -Binary $binary -ExpectedRoot 'DataRoot' | Out-Null }
        catch { $message = $_.Exception.Message }
        if ($case.Error -eq '') {
            if ($message -or -not [IO.File]::Exists("$fixture.sha256")) { throw "Valid synthetic package failed: $message" }
        } elseif (-not $message.StartsWith($case.Error, [StringComparison]::Ordinal) -or [IO.File]::Exists("$fixture.sha256")) {
            throw "Expected package rejection '$($case.Error)' without checksum publication; got '$message'."
        }
        Write-Output "PASS $($case.Name)"
    }
    if ((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $binaryHash) { throw 'Synthetic binary source changed.' }
} finally {
    foreach ($path in @($fixture, "$fixture.sha256", $binary)) { if ([IO.File]::Exists($path)) { [IO.File]::Delete($path) } }
    if ([IO.Directory]::Exists($fixtureDirectory)) { [IO.Directory]::Delete($fixtureDirectory, $false) }
}
Write-Output "Verified $($cases.Count) synthetic archive regressions; no archive was extracted or image executed."

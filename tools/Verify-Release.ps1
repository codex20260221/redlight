param(
    [string]$Binary = "build/Release/RedLight.exe",
    [string]$ExpectedVersion = "0.6.0-rc.1"
)
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_REF_TYPE -eq 'tag' -and $env:GITHUB_REF_NAME -ne "v$ExpectedVersion") {
    throw "Release tag does not match candidate version $ExpectedVersion."
}
# Read PE headers and file metadata only. Never execute the application.
$item = Get-Item -LiteralPath $Binary
if ($item.VersionInfo.ProductVersion -ne $ExpectedVersion -or $item.VersionInfo.FileVersion -ne $ExpectedVersion) {
    throw "Version mismatch: expected $ExpectedVersion; got $($item.VersionInfo.ProductVersion)"
}
if (-not $item.VersionInfo.IsPreRelease) { throw 'RC binary must have prerelease metadata.' }
$stream = [System.IO.File]::OpenRead($item.FullName)
$reader = [System.IO.BinaryReader]::new($stream)
try {
    if ($reader.ReadUInt16() -ne 0x5A4D) { throw 'Missing DOS header.' }
    $stream.Position = 0x3C
    $peOffset = $reader.ReadInt32()
    $stream.Position = $peOffset
    if ($reader.ReadUInt32() -ne 0x4550) { throw 'Invalid PE signature.' }
    if ($reader.ReadUInt16() -ne 0x8664) { throw 'Release must be native x64.' }
    $stream.Position = $peOffset + 22
    $characteristics = $reader.ReadUInt16()
    if (($characteristics -band 0x0002) -eq 0 -or ($characteristics -band 0x2000) -ne 0) {
        throw 'Release must be an executable image without the DLL flag.'
    }
    $stream.Position = $peOffset + 24
    if ($reader.ReadUInt16() -ne 0x20B) { throw 'Release must be PE32+.' }
    $stream.Position = $peOffset + 24 + 68
    if ($reader.ReadUInt16() -ne 2) { throw 'Release must use the Windows GUI subsystem.' }
    $flags = $reader.ReadUInt16()
    if (($flags -band 0x140) -ne 0x140) { throw 'ASLR and DEP must be enabled.' }
} finally { $reader.Dispose() }
Import-Module (Join-Path $PSScriptRoot 'BinaryImports.psm1') -Force
$dlls = @(Get-BinaryDllImports -Binary $item.FullName)
Test-BinaryDllImports -Dlls $dlls -Policy PortableRelease
Write-Output "Portable Windows imports verified: $($dlls -join ', ')"
# Inspect the embedded manifest as data. An .rc file can compile a mistakenly
# named resource type, so source presence alone does not establish embedding.
$sdkBin = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Windows Kits/10/bin'
$manifestTool = Get-ChildItem -Path (Join-Path $sdkBin '*/x64/mt.exe') -File |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $manifestTool) { throw 'Windows SDK mt.exe is required to verify the manifest.' }
$manifestPath = Join-Path $item.DirectoryName 'RedLight.embedded.manifest'
& $manifestTool.FullName -nologo "-inputresource:$($item.FullName);#1" "-out:$manifestPath"
if ($LASTEXITCODE -ne 0) { throw 'Executable manifest is missing or invalid.' }
[xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
$privileges = $manifest.SelectSingleNode("//*[local-name()='requestedExecutionLevel']")
if (-not $privileges -or $privileges.level -ne 'asInvoker' -or $privileges.uiAccess -ne 'false') {
    throw 'Executable must use normal user privileges with uiAccess disabled.'
}
$identity = $manifest.SelectSingleNode("/*[local-name()='assembly']/*[local-name()='assemblyIdentity']")
$numericVersion = ($ExpectedVersion -split '-')[0] + '.0'
if (-not $identity -or $identity.version -ne $numericVersion -or $identity.processorArchitecture -ne 'amd64') {
    throw 'Manifest identity does not match release version/architecture.'
}
$hash = Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256
"$($hash.Hash.ToLowerInvariant())  $($item.Name)" | Set-Content -LiteralPath "$($item.FullName).sha256" -Encoding ascii
Write-Output "Verified $ExpectedVersion, Windows x64 executable, GUI, ASLR/DEP, embedded asInvoker manifest; SHA-256 written."

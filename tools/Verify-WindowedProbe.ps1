param([string]$Binary = 'build-windowed/Release/WindowedRedProbe.exe')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# Static PE/manifest inspection only. This must never launch the probe.
Import-Module (Join-Path $PSScriptRoot 'BinaryImports.psm1') -Force
$item = Get-Item -LiteralPath $Binary
$stream = [IO.File]::OpenRead($item.FullName)
$reader = [IO.BinaryReader]::new($stream)
try {
    if ($reader.ReadUInt16() -ne 0x5a4d) { throw 'Missing DOS header.' }
    $stream.Position = 0x3c
    $offset = $reader.ReadInt32()
    $stream.Position = $offset
    if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) { throw 'Probe must be a Windows x64 PE.' }
    $stream.Position = $offset + 22
    $kind = $reader.ReadUInt16()
    if (($kind -band 2) -eq 0 -or ($kind -band 0x2000) -ne 0) { throw 'Probe must be an executable image.' }
    $stream.Position = $offset + 24
    if ($reader.ReadUInt16() -ne 0x20b) { throw 'Probe must be PE32+.' }
    $stream.Position = $offset + 24 + 68
    if ($reader.ReadUInt16() -ne 2) { throw 'Probe must be a GUI executable.' }
    if (($reader.ReadUInt16() -band 0x140) -ne 0x140) { throw 'Probe must enable ASLR/DEP.' }
} finally { $reader.Dispose() }
$imports = @(Get-BinaryDllImports -Binary $item.FullName)
Test-BinaryDllImports -Dlls $imports -Policy PortableRelease
$sdk = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Windows Kits/10/bin'
$mt = Get-ChildItem -Path (Join-Path $sdk '*/x64/mt.exe') -File | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $mt) { throw 'Windows SDK mt.exe is required for static manifest extraction.' }
$manifestPath = Join-Path $item.DirectoryName 'WindowedProbe.embedded.manifest'
& $mt.FullName -nologo "-inputresource:$($item.FullName);#1" "-out:$manifestPath"
if ($LASTEXITCODE -ne 0) { throw 'Missing/invalid embedded probe manifest.' }
[xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
$identity = $manifest.SelectSingleNode("/*[local-name()='assembly']/*[local-name()='assemblyIdentity']")
$privileges = $manifest.SelectSingleNode("//*[local-name()='requestedExecutionLevel']")
$dpi = $manifest.SelectSingleNode("//*[local-name()='dpiAwareness' and namespace-uri()='http://schemas.microsoft.com/SMI/2016/WindowsSettings']")
if (-not $identity -or $identity.name -ne 'RedLight.WindowedProbe' -or $identity.processorArchitecture -ne 'amd64') { throw 'Not the isolated windowed probe identity.' }
if (-not $privileges -or $privileges.level -ne 'asInvoker' -or $privileges.uiAccess -ne 'false') { throw 'Probe must keep normal user privileges and UIAccess disabled.' }
if (-not $dpi -or $dpi.InnerText -ne 'PerMonitorV2') { throw 'Probe must declare its own PerMonitorV2 context.' }
Write-Output "Verified isolated x64 GUI probe, ASLR/DEP, asInvoker/uiAccess=false, PMv2, and Windows imports ($($imports -join ', ')); no image executed."

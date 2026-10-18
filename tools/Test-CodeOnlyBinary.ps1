param([string]$Binary = "build/Release/RedLightCodeTests.exe")
$ErrorActionPreference = 'Stop'
# Inspect imports before running tests. Do not launch RedLight or MagRedProbe.
$vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio/Installer/vswhere.exe'
$dumpbin = & $vswhere -latest -products '*' -find 'VC/Tools/MSVC/**/bin/Hostx64/x64/dumpbin.exe' | Select-Object -First 1
if (-not $dumpbin) { throw 'dumpbin.exe is required to verify test isolation.' }
$imports = & $dumpbin /imports $Binary
if ($LASTEXITCODE -ne 0) { throw 'Could not inspect code test imports.' }
$dlls = @($imports | Select-String '^\s+([A-Za-z0-9_.-]+\.dll)\s*$' | ForEach-Object { $_.Matches[0].Groups[1].Value })
if ($dlls.Count -eq 0) { throw 'No imports found; isolation is unproven.' }
foreach ($dll in $dlls) {
    if ($dll -ine 'KERNEL32.dll') { throw "Unexpected code-test import: $dll" }
}
Write-Output "Code-only test imports verified: $($dlls -join ', ')"

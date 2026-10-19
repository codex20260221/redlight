param([string[]]$Binary = @("build/Release/RedLightCodeTests.exe", "build/Release/RedLightDiagnosticsCodeTests.exe", "build/Release/RedLightWindowedControlCodeTests.exe"))
$ErrorActionPreference = 'Stop'
# Inspect imports before running tests. Do not launch RedLight or MagRedProbe.
Import-Module (Join-Path $PSScriptRoot 'BinaryImports.psm1') -Force
foreach ($testBinary in $Binary) {
    $dlls = @(Get-BinaryDllImports -Binary $testBinary)
    Test-BinaryDllImports -Dlls $dlls -Policy CodeOnly
    Write-Output "Code-only imports verified for $testBinary : $($dlls -join ', ')"
}

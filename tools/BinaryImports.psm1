Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:dumpbinPath = $null

function Read-DumpbinDllImports {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][string[]]$DependencyOutput,
        [Parameter(Mandatory)][AllowEmptyCollection()][AllowEmptyString()][string[]]$ImportOutput
    )
    foreach ($output in @($DependencyOutput, $ImportOutput)) {
        if (@($output | Where-Object { $_ -match '^Dump of file .+' }).Count -ne 1) {
            throw 'Expected exactly one dumped image; imports are unproven.'
        }
    }
    $names = [Collections.Generic.List[string]]::new()
    $dependencies = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $inSection = $false
    $sectionHasName = $false
    foreach ($line in $DependencyOutput) {
        if ($line -match '^\s*Image has the following (?:delay load )?dependencies:\s*$') {
            if ($inSection -and -not $sectionHasName) { throw 'Empty dependency section.' }
            $inSection = $true
            $sectionHasName = $false
            continue
        }
        if ($inSection) {
            if ([string]::IsNullOrWhiteSpace($line)) {
                if ($sectionHasName) { $inSection = $false }
                continue
            }
            $entry = [regex]::Match($line, '^ {4}(\S(?:.*\S)?)$')
            if (-not $entry.Success) { throw 'Unrecognized dependency-list formatting; imports are unproven.' }
            $name = $entry.Groups[1].Value
            if ($dependencies.Add($name)) { $names.Add($name) }
            $sectionHasName = $true
        } elseif ($line -match '^\s*Image.*dependenc') {
            throw 'Unrecognized dependency section; imports are unproven.'
        }
    }
    if (($inSection -and -not $sectionHasName) -or $names.Count -eq 0) { throw 'No dependencies found; imports are unproven.' }
    # /IMPORTS documents ordinary and delay-loaded DLLs. Its four-space DLL
    # headers must agree with /DEPENDENTS; individual symbols are more indented.
    $headers = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($line in $ImportOutput) {
        $entry = [regex]::Match($line, '^ {4}(\S(?:.*\S)?)$')
        if ($entry.Success) { $null = $headers.Add($entry.Groups[1].Value) }
    }
    if (-not $headers.SetEquals($dependencies)) { throw 'Dependency and import-table DLL names disagree; imports are unproven.' }
    return $names.ToArray()
}

function Get-BinaryDllImports {
    param([Parameter(Mandatory)][string]$Binary)
    $item = Get-Item -LiteralPath $Binary
    if ($item.PSIsContainer) { throw 'A binary file is required for import inspection.' }
    if (-not $script:dumpbinPath) {
        $locator = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio/Installer/vswhere.exe'
        $candidates = @(& $locator -latest -products '*' -find 'VC/Tools/MSVC/**/bin/Hostx64/x64/dumpbin.exe')
        if ($LASTEXITCODE -ne 0) { throw 'Visual Studio discovery failed during static import verification.' }
        $script:dumpbinPath = $candidates | Select-Object -First 1
        if (-not $script:dumpbinPath) { throw 'dumpbin.exe is required for static import verification.' }
    }
    $dependencies = @(& $script:dumpbinPath /nologo /dependents $item.FullName)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect binary dependencies.' }
    $imports = @(& $script:dumpbinPath /nologo /imports $item.FullName)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect binary import tables.' }
    return Read-DumpbinDllImports -DependencyOutput $dependencies -ImportOutput $imports
}

function Test-BinaryDllImports {
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][string[]]$Dlls,
        [Parameter(Mandatory)][ValidateSet('CodeOnly', 'PortableRelease')][string]$Policy
    )
    if ($Dlls.Count -eq 0) { throw 'No imports found; policy is unproven.' }
    $allowed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $null = $allowed.Add('KERNEL32.dll')
    if ($Policy -eq 'PortableRelease') {
        foreach ($name in @('USER32.dll', 'GDI32.dll', 'SHELL32.dll', 'MAGNIFICATION.dll', 'WTSAPI32.dll', 'ADVAPI32.dll')) {
            $null = $allowed.Add($name)
        }
    }
    foreach ($dll in $Dlls) {
        if (-not $allowed.Contains($dll)) { throw "Unexpected $Policy import: $dll" }
    }
}

Export-ModuleMember -Function Read-DumpbinDllImports, Get-BinaryDllImports, Test-BinaryDllImports

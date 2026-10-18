$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'BinaryImports.psm1') -Force

function Assert-Condition([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Rejected([scriptblock]$Action) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert-Condition $rejected 'Unproven or disallowed import data was accepted.'
}
function New-ImportFixture([string[]]$Normal = @(), [string[]]$Delayed = @()) {
    $dependencies = [Collections.Generic.List[string]]::new()
    $imports = [Collections.Generic.List[string]]::new()
    foreach ($list in @($dependencies, $imports)) {
        $list.Add('Dump of file synthetic.exe')
        $list.Add('')
        $list.Add('File Type: EXECUTABLE IMAGE')
        $list.Add('')
    }
    foreach ($section in @(@{ Names = $Normal; Label = '' }, @{ Names = $Delayed; Label = 'delay load ' })) {
        if ($section.Names.Count -eq 0) { continue }
        $dependencies.Add("  Image has the following $($section.Label)dependencies:")
        $dependencies.Add('')
        $imports.Add("  Section contains the following $($section.Label)imports:")
        $imports.Add('')
        foreach ($name in $section.Names) {
            $dependencies.Add("    $name")
            $imports.Add("    $name")
            $imports.Add('             140000000 Import Address Table')
            $imports.Add('             140000100 Import Name Table')
            $imports.Add('')
            $imports.Add('                          10 SyntheticFunction')
            $imports.Add('')
        }
        $dependencies.Add('')
    }
    $dependencies.Add('  Summary')
    $dependencies.Add('        1000 .text')
    $imports.Add('  Summary')
    $imports.Add('        1000 .text')
    return [pscustomobject]@{ Dependencies = $dependencies.ToArray(); Imports = $imports.ToArray() }
}
function Read-Fixture($Fixture) {
    return @(Read-DumpbinDllImports -DependencyOutput $Fixture.Dependencies -ImportOutput $Fixture.Imports)
}

$cases = @(
    @{ Name = 'Kernel-only ordinary imports'; Run = {
        $dlls = @(Read-Fixture (New-ImportFixture @('KERNEL32.dll')))
        Assert-Condition ($dlls.Count -eq 1 -and $dlls[0] -ceq 'KERNEL32.dll') 'Kernel-only list changed.'
        Test-BinaryDllImports -Dlls $dlls -Policy CodeOnly
    } },
    @{ Name = 'Reviewed portable imports and strict test policy'; Run = {
        $expected = @('USER32.dll', 'GDI32.dll', 'SHELL32.dll', 'MAGNIFICATION.dll', 'WTSAPI32.dll', 'ADVAPI32.dll', 'KERNEL32.dll')
        $dlls = @(Read-Fixture (New-ImportFixture $expected))
        Assert-Condition (($dlls -join ',') -ceq ($expected -join ',')) 'Stable dependency ordering changed.'
        Test-BinaryDllImports -Dlls $dlls -Policy PortableRelease
        Assert-Rejected { Test-BinaryDllImports -Dlls $dlls -Policy CodeOnly }
    } },
    @{ Name = 'Delay-only forbidden imports'; Run = {
        $dlls = @(Read-Fixture (New-ImportFixture -Delayed @('USER32.dll')))
        Assert-Rejected { Test-BinaryDllImports -Dlls $dlls -Policy CodeOnly }
    } },
    @{ Name = 'Mixed delayed imports'; Run = {
        $dlls = @(Read-Fixture (New-ImportFixture @('KERNEL32.dll') @('USER32.dll')))
        Assert-Condition ($dlls.Count -eq 2) 'Delay-loaded name was omitted.'
        Assert-Rejected { Test-BinaryDllImports -Dlls $dlls -Policy CodeOnly }
    } },
    @{ Name = 'Dependency/import delayed-name disagreement'; Run = {
        $normal = New-ImportFixture @('KERNEL32.dll')
        $delayed = New-ImportFixture @('KERNEL32.dll') @('USER32.dll')
        Assert-Rejected { Read-DumpbinDllImports -DependencyOutput $normal.Dependencies -ImportOutput $delayed.Imports }
    } },
    @{ Name = 'Redistributable/debug/private dependency refusal'; Run = {
        foreach ($name in @('VCRUNTIME140.dll', 'MSVCP140.dll', 'ucrtbased.dll', 'private-helper.dll')) {
            $dlls = @(Read-Fixture (New-ImportFixture @('KERNEL32.dll', $name)))
            Assert-Rejected { Test-BinaryDllImports -Dlls $dlls -Policy PortableRelease }
        }
    } },
    @{ Name = 'Names without DLL suffix remain visible'; Run = {
        $dlls = @(Read-Fixture (New-ImportFixture @('KERNEL32.dll', 'USER32')))
        Assert-Condition ($dlls.Count -eq 2 -and $dlls[1] -ceq 'USER32') 'Suffixless dependency was dropped.'
        Assert-Rejected { Test-BinaryDllImports -Dlls $dlls -Policy CodeOnly }
        Assert-Rejected { Test-BinaryDllImports -Dlls $dlls -Policy PortableRelease }
    } },
    @{ Name = 'Path/space/Unicode names remain visible'; Run = {
        $unicode = ([string][char]0x5de5) + [char]0x5177 + '.dll'
        foreach ($name in @('C:\private\KERNEL32.dll', 'custom library.dll', $unicode)) {
            $dlls = @(Read-Fixture (New-ImportFixture @('KERNEL32.dll', $name)))
            Assert-Condition ($dlls.Count -eq 2 -and $dlls[1] -ceq $name) 'Nonstandard dependency was omitted or normalized.'
            Assert-Rejected { Test-BinaryDllImports -Dlls $dlls -Policy PortableRelease }
        }
    } },
    @{ Name = 'Case-insensitive duplicate agreement'; Run = {
        $dlls = @(Read-Fixture (New-ImportFixture @('KERNEL32.dll', 'kernel32.DLL')))
        Assert-Condition ($dlls.Count -eq 1) 'Duplicate import names were not consolidated.'
        Test-BinaryDllImports -Dlls $dlls -Policy CodeOnly
    } },
    @{ Name = 'Missing dependency section'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        Assert-Rejected { Read-DumpbinDllImports -DependencyOutput @('Dump of file synthetic.exe', '  Summary') -ImportOutput $fixture.Imports }
    } },
    @{ Name = 'Empty native output'; Run = {
        Assert-Rejected { Read-DumpbinDllImports -DependencyOutput @() -ImportOutput @() }
    } },
    @{ Name = 'Multiple dumped images'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        Assert-Rejected { Read-DumpbinDllImports -DependencyOutput @($fixture.Dependencies + 'Dump of file second.exe') -ImportOutput $fixture.Imports }
    } },
    @{ Name = 'Unexpected dependency indentation'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        $fixture.Dependencies = @($fixture.Dependencies | ForEach-Object { $_ -replace '^    KERNEL32', '     KERNEL32' })
        Assert-Rejected { Read-Fixture $fixture }
    } },
    @{ Name = 'Trailing dependency whitespace'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        $fixture.Dependencies = @($fixture.Dependencies | ForEach-Object { $_ -replace '^    KERNEL32.dll$', '    KERNEL32.dll ' })
        Assert-Rejected { Read-Fixture $fixture }
    } },
    @{ Name = 'Unrecognized dependency section'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        $fixture.Dependencies = @($fixture.Dependencies | ForEach-Object { $_ -replace 'following dependencies:', 'following unusual dependencies:' })
        Assert-Rejected { Read-Fixture $fixture }
    } },
    @{ Name = 'Missing import-table headers'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        Assert-Rejected { Read-DumpbinDllImports -DependencyOutput $fixture.Dependencies -ImportOutput @('Dump of file synthetic.exe', '  Summary') }
    } },
    @{ Name = 'Extra import-table library'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        Assert-Rejected { Read-DumpbinDllImports -DependencyOutput $fixture.Dependencies -ImportOutput @($fixture.Imports + '    USER32.dll') }
    } },
    @{ Name = 'Empty policy input'; Run = {
        Assert-Rejected { Test-BinaryDllImports -Dlls @() -Policy CodeOnly }
        Assert-Rejected { Test-BinaryDllImports -Dlls @() -Policy PortableRelease }
    } },
    @{ Name = 'Unknown policy cannot bypass verification'; Run = {
        Assert-Rejected { Test-BinaryDllImports -Dlls @('USER32.dll') -Policy 'Any' }
    } },
    @{ Name = 'Truncated later dependency section'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        $fixture.Dependencies += @('  Image has the following delay load dependencies:', '')
        Assert-Rejected { Read-Fixture $fixture }
    } },
    @{ Name = 'Individual imported symbols are not DLL headers'; Run = {
        $fixture = New-ImportFixture @('KERNEL32.dll')
        $fixture.Imports += '                         USER32.dll'
        Test-BinaryDllImports -Dlls @(Read-Fixture $fixture) -Policy CodeOnly
    } }
)
foreach ($case in $cases) {
    & $case.Run
    Write-Output "PASS $($case.Name)"
}
Write-Output "Verified $($cases.Count) synthetic import/parser policy scenarios; no inspected binary was executed."

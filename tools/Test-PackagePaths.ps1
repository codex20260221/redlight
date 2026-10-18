$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'PackagePaths.psm1') -Force

function Assert-Condition([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Assert-Rejected([string[]]$Names, [string]$Root = 'RedLight') {
    $rejected = $false
    try { Read-PackageEntryPaths -Names $Names -ExpectedRoot $Root | Out-Null } catch { $rejected = $true }
    Assert-Condition $rejected 'Invalid or ambiguous package paths were accepted.'
}
$cases = @(
    @{ Name = 'Canonical files and explicit directories'; Run = {
        $paths = @(Read-PackageEntryPaths @('RedLight/', 'RedLight/RedLight.exe', 'RedLight/docs/', 'RedLight/docs/readme.md') 'RedLight')
        Assert-Condition ($paths.Count -eq 4 -and $paths[0].RelativePath -eq '' -and $paths[0].IsDirectory -and
            $paths[1].RelativePath -eq 'RedLight.exe' -and -not $paths[1].IsDirectory -and $paths[2].IsDirectory -and
            $paths[3].RelativePath -eq 'docs/readme.md') 'Canonical entry metadata/order changed.'
    } },
    @{ Name = 'Implicit directories and Unicode names'; Run = {
        $unicode = 'RedLight/docs/' + [char]0x03a9 + '.md'
        $paths = @(Read-PackageEntryPaths @($unicode) 'RedLight')
        Assert-Condition ($paths.Count -eq 1 -and $paths[0].FullName -ceq $unicode -and -not $paths[0].IsDirectory) 'Unicode/implicit path was normalized.'
    } },
    @{ Name = 'Empty entry lists and root names'; Run = { Assert-Rejected @(); Assert-Rejected @('RedLight/a') ''; Assert-Rejected @('RedLight/a') '../RedLight' } },
    @{ Name = 'Missing and unexpected roots'; Run = { foreach ($name in @('RedLight', 'Other/a', 'redlight/a', 'a')) { Assert-Rejected @($name) } } },
    @{ Name = 'Multiple roots'; Run = { Assert-Rejected @('RedLight/a', 'Other/b') } },
    @{ Name = 'Absolute drive UNC and device paths'; Run = { foreach ($name in @('/RedLight/a', 'C:/RedLight/a', '\\server\RedLight\a', '\\?\C:\RedLight\a')) { Assert-Rejected @($name) } } },
    @{ Name = 'Backslash traversal and mixed separators'; Run = { foreach ($name in @('RedLight/docs\a', 'RedLight/docs\..\a', 'RedLight\a')) { Assert-Rejected @($name) } } },
    @{ Name = 'Dot and parent traversal'; Run = { foreach ($name in @('RedLight/./a', 'RedLight/../a', 'RedLight/docs/../../a', 'RedLight/docs/..')) { Assert-Rejected @($name) } } },
    @{ Name = 'Empty components and repeated separators'; Run = { foreach ($name in @('', 'RedLight//a', 'RedLight/docs//', 'RedLight//')) { Assert-Rejected @($name) } } },
    @{ Name = 'Alternate stream and reserved characters'; Run = { foreach ($character in @(':', '<', '>', '"', '|', '?', '*')) { Assert-Rejected @('RedLight/a' + $character + 'b') } } },
    @{ Name = 'Control and NUL characters'; Run = { foreach ($number in @(0, 1, 9, 10, 13, 31, 127)) { Assert-Rejected @('RedLight/a' + [char]$number + 'b') } } },
    @{ Name = 'Trailing dot and space aliases'; Run = { foreach ($name in @('RedLight/a.', 'RedLight/a ', 'RedLight/docs./a', 'RedLight/docs /a', 'RedLight/docs. /')) { Assert-Rejected @($name) } } },
    @{ Name = 'Windows device names and extensions'; Run = { foreach ($name in @('CON', 'prn.md', 'AUX.tar.gz', 'nul', 'COM1.txt', 'lpt9.md')) { Assert-Rejected @('RedLight/docs/' + $name) } } },
    @{ Name = 'Superscript device aliases'; Run = { foreach ($number in @(0x00b9, 0x00b2, 0x00b3)) { Assert-Rejected @('RedLight/docs/COM' + [char]$number + '.md'); Assert-Rejected @('RedLight/LPT' + [char]$number + '/') } } },
    @{ Name = 'Device-like ordinary names remain valid'; Run = { $paths = @(Read-PackageEntryPaths @('RedLight/CONtext', 'RedLight/COM10.md', 'RedLight/.hidden', 'RedLight/my file.md') 'RedLight'); Assert-Condition ($paths.Count -eq 4) 'Valid ordinary names were over-restricted.' } },
    @{ Name = 'Duplicate files and ordinal case aliases'; Run = { Assert-Rejected @('RedLight/a', 'RedLight/a'); Assert-Rejected @('RedLight/docs/a', 'RedLight/DOCS/A') } },
    @{ Name = 'Duplicate directories and root aliases'; Run = { Assert-Rejected @('RedLight/docs/', 'RedLight/DOCS/'); Assert-Rejected @('RedLight/', 'RedLight/') } },
    @{ Name = 'Explicit file and directory collision'; Run = { Assert-Rejected @('RedLight/docs', 'RedLight/docs/'); Assert-Rejected @('RedLight/docs/', 'RedLight/docs') } },
    @{ Name = 'Implicit ancestor collision in either order'; Run = { Assert-Rejected @('RedLight/docs', 'RedLight/docs/a'); Assert-Rejected @('RedLight/docs/a', 'RedLight/DOCS') } }
)
foreach ($case in $cases) {
    & $case.Run
    Write-Output "PASS $($case.Name)"
}
Write-Output "Verified $($cases.Count) synthetic package-path scenarios; no archive was extracted or image executed."

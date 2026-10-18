param(
    [string]$Binary = 'build/Release/RedLight.exe',
    [string]$ExpectedVersion = '0.6.0-rc.1'
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
# These are data copies for static verification. Never execute either image.
$source = Get-Item -LiteralPath $Binary
$sourceHash = (Get-FileHash -LiteralPath $source.FullName -Algorithm SHA256).Hash
$bytes = [IO.File]::ReadAllBytes($source.FullName)
if ($bytes.Length -lt 64 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5a4d) { throw 'A complete PE source image is required.' }
$peOffset = [BitConverter]::ToInt32($bytes, 0x3c)
if ($peOffset -lt 0 -or $peOffset -gt $bytes.Length - 24 -or [BitConverter]::ToUInt32($bytes, $peOffset) -ne 0x4550) {
    throw 'A complete PE header is required for the static fixture.'
}
$characteristics = [BitConverter]::ToUInt16($bytes, $peOffset + 22)
if (($characteristics -band 2) -eq 0 -or ($characteristics -band 0x2000) -ne 0) { throw 'The baseline source must be an executable image.' }
$repoRoot = Split-Path -Parent $PSScriptRoot
$fixtureDirectory = Join-Path (Join-Path $repoRoot 'build') ('metadata-code-fixture-' + [Guid]::NewGuid().ToString('N'))
$fixture = Join-Path $fixtureDirectory 'modified-image.pe-data'
if ([StringComparer]::OrdinalIgnoreCase.Equals($source.FullName, [IO.Path]::GetFullPath($fixture))) { throw 'Fixture must not replace its source.' }
$null = [IO.Directory]::CreateDirectory($fixtureDirectory)
try {
    foreach ($case in @(
        @{ Name = 'DLL-marked release data'; Flags = [uint16]($characteristics -bor 0x2000) },
        @{ Name = 'Non-executable release data'; Flags = [uint16]($characteristics -band 0xfffd) }
    )) {
        $copy = [byte[]]$bytes.Clone()
        [BitConverter]::GetBytes($case.Flags).CopyTo($copy, $peOffset + 22)
        [IO.File]::WriteAllBytes($fixture, $copy)
        $rejected = $false
        try { & (Join-Path $PSScriptRoot 'Verify-Release.ps1') -Binary $fixture -ExpectedVersion $ExpectedVersion | Out-Null }
        catch {
            if ($_.Exception.Message -ne 'Release must be an executable image without the DLL flag.') { throw }
            $rejected = $true
        }
        if (-not $rejected) { throw 'Static image-kind verification accepted invalid release data.' }
        Write-Output "PASS $($case.Name)"
    }
} finally {
    foreach ($path in @($fixture, "$fixture.sha256", (Join-Path $fixtureDirectory 'RedLight.embedded.manifest'))) {
        if ([IO.File]::Exists($path)) { [IO.File]::Delete($path) }
    }
    [IO.Directory]::Delete($fixtureDirectory, $false)
}
if ((Get-FileHash -LiteralPath $source.FullName -Algorithm SHA256).Hash -ne $sourceHash) { throw 'Source image changed during static metadata checks.' }
Write-Output 'Verified 2 static image-kind regressions; source image is unchanged and no image was executed.'

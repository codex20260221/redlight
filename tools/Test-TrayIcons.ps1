$ErrorActionPreference = 'Stop'
& "$PSScriptRoot/Generate-TrayIcons.ps1" -Verify
function Assert([bool]$condition, [string]$message) { if (-not $condition) { throw $message } }
$images = @{}
foreach ($state in @('off', 'strict', 'luma', 'attention')) {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot "../resources/tray-$state.ico"))
    Assert ([BitConverter]::ToUInt16($bytes, 0) -eq 0 -and [BitConverter]::ToUInt16($bytes, 2) -eq 1) 'ICO header'
    Assert ([BitConverter]::ToUInt16($bytes, 4) -eq 7) 'Seven explicit icon sizes'
    $next = 6 + 7 * 16
    foreach ($index in 0..6) {
        $directory = 6 + $index * 16
        $size = @(16, 20, 24, 32, 40, 48, 64)[$index]
        $stride = [int]([Math]::Ceiling($size / 32.0) * 4)
        $length = 40 + $size * $size * 4 + $stride * $size
        Assert ($bytes[$directory] -eq $size -and $bytes[$directory + 1] -eq $size) 'Directory dimensions'
        Assert ([BitConverter]::ToUInt16($bytes, $directory + 4) -eq 1 -and [BitConverter]::ToUInt16($bytes, $directory + 6) -eq 32) 'Directory depth'
        Assert ([BitConverter]::ToUInt32($bytes, $directory + 8) -eq $length) 'Directory length'
        Assert ([BitConverter]::ToUInt32($bytes, $directory + 12) -eq $next) 'Contiguous bounded image offsets'
        Assert ($next + $length -le $bytes.Length) 'Image bound'
        Assert ([BitConverter]::ToUInt32($bytes, $next) -eq 40 -and [BitConverter]::ToInt32($bytes, $next + 4) -eq $size -and
            [BitConverter]::ToInt32($bytes, $next + 8) -eq 2 * $size) 'DIB dimensions'
        Assert ([BitConverter]::ToUInt16($bytes, $next + 12) -eq 1 -and [BitConverter]::ToUInt16($bytes, $next + 14) -eq 32 -and
            [BitConverter]::ToUInt32($bytes, $next + 16) -eq 0) 'Uncompressed 32-bit DIB'
        $white = 0; $black = 0; $transparent = 0; $pixels = [byte[]]::new($size * $size)
        for ($y = 0; $y -lt $size; ++$y) {
            for ($x = 0; $x -lt $size; ++$x) {
                $pixel = $next + 40 + (($size - 1 - $y) * $size + $x) * 4
                $b = $bytes[$pixel]; $g = $bytes[$pixel + 1]; $r = $bytes[$pixel + 2]; $alpha = $bytes[$pixel + 3]
                Assert ($r -eq $g -and $g -eq $b -and ($r -eq 0 -or $r -eq 255)) 'Artwork uses black and full-luminance white only'
                Assert ($alpha -eq 0 -or $alpha -eq 255) 'Exact opaque or transparent alpha'
                $mask = $bytes[$next + 40 + $size * $size * 4 + ($size - 1 - $y) * $stride + [int][Math]::Floor($x / 8.0)]
                Assert ((($mask -band (128 -shr ($x % 8))) -ne 0) -eq ($alpha -eq 0)) 'Legacy transparency agrees with alpha'
                if ($alpha -eq 0) { Assert ($r -eq 0) 'Transparent RGB zero'; ++$transparent }
                elseif ($r -eq 255) { ++$white; $pixels[$y * $size + $x] = 2 }
                else { ++$black; $pixels[$y * $size + $x] = 1 }
                # Strict and every Luma preset map monochrome endpoints to the same red contrast.
                foreach ($weights in @(@(1.0, 0.0, 0.0), @(0.299, 0.587, 0.114), @(0.2126, 0.7152, 0.0722), @((1.0/3), (1.0/3), (1.0/3)))) {
                    $red = $weights[0] * $r + $weights[1] * $g + $weights[2] * $b
                    Assert ([Math]::Abs($red - $r) -lt 0.000001) 'Black/white contrast survives Strict and Luma filtering'
                }
            }
        }
        Assert ($white -ge $size * $size / 10 -and $black -ge $size -and $transparent -ge $size) 'Bright body, dark outline, and transparent margin'
        $images["$state-$size"] = $pixels
        $next += $length
    }
    Assert ($next -eq $bytes.Length) 'No trailing or hidden image data'
}
foreach ($size in @(16, 20, 24, 32, 40, 48, 64)) {
    $states = @('off', 'strict', 'luma', 'attention')
    foreach ($a in 0..2) { foreach ($b in ($a + 1)..3) {
        $differences = 0
        for ($pixel = 0; $pixel -lt $size * $size; ++$pixel) {
            if ($images["$($states[$a])-$size"][$pixel] -ne $images["$($states[$b])-$size"][$pixel]) { ++$differences }
        }
        Assert ($differences -ge $size) 'Every state differs by shape/coverage, independent of hue'
    } }
    $center = [int]($size / 2) * $size + [int]($size / 2)
    Assert ($images["off-$size"][$center] -eq 0 -and $images["strict-$size"][$center] -eq 2 -and
        $images["luma-$size"][$center] -eq 2 -and $images["attention-$size"][$center] -eq 1) 'Hollow off, filled active, and dark warning mark'
}
Write-Output 'PASS tray resources: seven sizes, four distinct monochrome states, transparency, contrast, and reproducible artwork (no native APIs).'

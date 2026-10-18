param([switch]$Verify)
$ErrorActionPreference = 'Stop'

# Original pixel artwork. '.' is transparent, '#' opaque black, 'W' opaque white.
# No image libraries, GDI, shell, or display APIs are used to generate the resources.
$artwork = [ordered]@{
    off = @(
        '................', '.....######.....', '...##WWWWWW##...', '..#WW######WW#..',
        '..#W##....##W#..', '.#W##......##W#.', '.#W#........#W#.', '.#W#........#W#.',
        '.#W#........#W#.', '.#W#........#W#.', '.#W##......##W#.', '..#W##....##W#..',
        '..#WW######WW#..', '...##WWWWWW##...', '.....######.....', '................')
    strict = @(
        '................', '.....######.....', '...##WWWWWW##...', '..#WWWWWWWWWW#..',
        '..#WWWWWWWWWW#..', '.#WWWWWWWWWWWW#.', '.#WWWWWWWWWWWW#.', '.#WWWWWWWWWWWW#.',
        '.#WWWWWWWWWWWW#.', '.#WWWWWWWWWWWW#.', '.#WWWWWWWWWWWW#.', '..#WWWWWWWWWW#..',
        '..#WWWWWWWWWW#..', '...##WWWWWW##...', '.....######.....', '................')
    luma = @(
        '................', '.......##.......', '......#WW#......', '.....#WWWW#.....',
        '....#WWWWWW#....', '...#WWWWWWWW#...', '..#WWWWWWWWWW#..', '.#WWWWWWWWWWWW#.',
        '.#WWWWWWWWWWWW#.', '..#WWWWWWWWWW#..', '...#WWWWWWWW#...', '....#WWWWWW#....',
        '.....#WWWW#.....', '......#WW#......', '.......##.......', '................')
    attention = @(
        '................', '.......##.......', '......#WW#......', '......#WW#......',
        '.....#W##W#.....', '.....#W##W#.....', '....#WW##WW#....', '....#WW##WW#....',
        '...#WWW##WWW#...', '...#WWWWWWWW#...', '..#WWWW##WWWW#..', '..#WWWW##WWWW#..',
        '.#WWWWWWWWWWWW#.', '.##############.', '................', '................')
}
$sizes = @(16, 20, 24, 32, 40, 48, 64)
foreach ($entry in $artwork.GetEnumerator()) {
    $rows = $entry.Value
    if ($rows.Count -ne 16 -or @($rows | Where-Object { $_ -notmatch '^[.#W]{16}$' }).Count) {
        throw "Invalid pixel artwork: $($entry.Key)"
    }
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]$sizes.Count)
        $offset = 6 + 16 * $sizes.Count
        foreach ($size in $sizes) {
            $maskStride = [int]([Math]::Ceiling($size / 32.0) * 4)
            $bytes = 40 + $size * $size * 4 + $maskStride * $size
            $writer.Write([byte]$size); $writer.Write([byte]$size)
            $writer.Write([uint16]0); $writer.Write([uint16]1); $writer.Write([uint16]32)
            $writer.Write([uint32]$bytes); $writer.Write([uint32]$offset)
            $offset += $bytes
        }
        foreach ($size in $sizes) {
            $maskStride = [int]([Math]::Ceiling($size / 32.0) * 4)
            $writer.Write([uint32]40); $writer.Write([int32]$size); $writer.Write([int32]($size * 2))
            $writer.Write([uint16]1); $writer.Write([uint16]32); $writer.Write([uint32]0)
            $writer.Write([uint32]($size * $size * 4 + $maskStride * $size))
            1..4 | ForEach-Object { $writer.Write([uint32]0) }
            for ($y = $size - 1; $y -ge 0; --$y) {
                for ($x = 0; $x -lt $size; ++$x) {
                    $pixel = $rows[[int][Math]::Floor($y * 16.0 / $size)][[int][Math]::Floor($x * 16.0 / $size)]
                    $value = if ($pixel -eq 'W') { [byte]255 } else { [byte]0 }
                    $writer.Write($value); $writer.Write($value); $writer.Write($value)
                    $writer.Write([byte]$(if ($pixel -eq '.') { 0 } else { 255 }))
                }
            }
            for ($y = $size - 1; $y -ge 0; --$y) {
                $mask = [byte[]]::new($maskStride)
                for ($x = 0; $x -lt $size; ++$x) {
                    if ($rows[[int][Math]::Floor($y * 16.0 / $size)][[int][Math]::Floor($x * 16.0 / $size)] -eq '.') {
                        $index = [int][Math]::Floor($x / 8.0)
                        $mask[$index] = $mask[$index] -bor (128 -shr ($x % 8))
                    }
                }
                $writer.Write($mask)
            }
        }
        $writer.Flush()
        $bytes = $stream.ToArray()
        $path = Join-Path $PSScriptRoot "../resources/tray-$($entry.Key).ico"
        if ($Verify) {
            if (-not [IO.File]::Exists($path) -or
                [Convert]::ToBase64String([IO.File]::ReadAllBytes($path)) -cne [Convert]::ToBase64String($bytes)) {
                throw "Tray resource differs from original artwork: $path"
            }
        } else { [IO.File]::WriteAllBytes($path, $bytes) }
    } finally { $writer.Dispose(); $stream.Dispose() }
}
Write-Output $(if ($Verify) { 'Tray icon artwork matches all four resources.' } else { 'Generated four tray icon resources.' })

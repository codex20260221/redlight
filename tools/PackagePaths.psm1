Set-StrictMode -Version Latest

function Test-PackagePathComponent([string]$Name) {
    if ([string]::IsNullOrEmpty($Name) -or $Name -in @('.', '..') -or
        $Name -match '[\\/:<>"|?*\x00-\x1f\x7f]' -or $Name.EndsWith(' ') -or $Name.EndsWith('.')) { return $false }
    # Windows device names remain reserved with extensions and in subdirectories.
    if ($Name -match '^(CON|PRN|AUX|NUL|COM[1-9\u00b9\u00b2\u00b3]|LPT[1-9\u00b9\u00b2\u00b3])($|\.)') { return $false }
    return $true
}

function Read-PackageEntryPaths {
    param([string[]]$Names, [string]$ExpectedRoot)
    if (-not (Test-PackagePathComponent $ExpectedRoot) -or -not $Names -or $Names.Count -eq 0) {
        throw 'A valid expected package root and nonempty entry list are required.'
    }
    $seen = [Collections.Generic.Dictionary[string, bool]]::new([StringComparer]::OrdinalIgnoreCase)
    $entries = [Collections.Generic.List[object]]::new()
    foreach ($name in $Names) {
        if ([string]::IsNullOrEmpty($name)) { throw 'Invalid package path.' }
        $directory = $name.EndsWith('/', [StringComparison]::Ordinal)
        $path = if ($directory) { $name.Substring(0, $name.Length - 1) } else { $name }
        $parts = $path.Split([char]'/')
        foreach ($part in $parts) {
            if (-not (Test-PackagePathComponent $part)) { throw "Invalid package path: $name" }
        }
        if (-not [StringComparer]::Ordinal.Equals($parts[0], $ExpectedRoot) -or ($parts.Count -eq 1 -and -not $directory)) {
            throw "Package entry must be beneath the expected root: $name"
        }
        if ($seen.ContainsKey($path)) { throw "Duplicate or aliased package entry: $name" }
        $seen.Add($path, $directory)
        $relative = if ($parts.Count -eq 1) { '' } else { $path.Substring($ExpectedRoot.Length + 1) }
        $entries.Add([pscustomobject]@{ FullName = $name; RelativePath = $relative; IsDirectory = $directory; Path = $path })
    }
    # Detect a file used as a parent even when no explicit directory entry exists,
    # and regardless of archive order. Never normalize or extract an entry.
    foreach ($entry in $entries) {
        $parent = $entry.Path
        while (($slash = $parent.LastIndexOf('/')) -ge 0) {
            $parent = $parent.Substring(0, $slash)
            if ($seen.ContainsKey($parent) -and -not $seen[$parent]) { throw "Package file/directory collision: $($entry.FullName)" }
        }
    }
    return $entries.ToArray()
}

Export-ModuleMember -Function Read-PackageEntryPaths

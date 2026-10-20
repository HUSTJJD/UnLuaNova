# Artifact reuse for Tools/ReleasePipeline.ps1.
#
# Why this exists: one release run compiles both Lua backends, cooks and packages three
# configurations and rebuilds both analyzers from source. None of that changes when its
# inputs did not change, and paying for it again is what made a second run over the same
# tree cost the same hour as the first one.
#
# How: each expensive stage gets a fingerprint of the inputs that decide its output, plus
# the files it produces. A hit means "these inputs already produced these files", and the
# stage is then recorded as `cached` and skipped. Recorded files are re-checked on every
# hit (size, modification time and content hash), so a file that was rebuilt, deleted or
# replaced by the other backend's build is a miss, never a silent reuse.
#
# The single-slot rule: the backend is a compile-time choice and UE keeps exactly one build
# tree, so a run that builds both backends leaves only the *second* one on disk. Nothing
# survives a backend switch, which is why the pipeline orders the backends by whatever the
# tree currently holds: the first backend of a run reuses the previous run's build, the
# second one rebuilds and leaves the tree for the next run.
#
# Test stages are never cached: they are the evidence, not an input.

$script:ReleaseCacheDisabled = $true
$script:ReleaseCacheDirectory = $null
$script:ReleaseCacheIndex = $null
$script:ReleaseCacheDirty = $false

function Initialize-ReleaseCache {
    param([Parameter(Mandatory)] [string] $Directory, [switch] $Disabled)

    $script:ReleaseCacheDisabled = [bool]$Disabled
    $script:ReleaseCacheDirectory = [IO.Path]::GetFullPath($Directory)
    $script:ReleaseCacheIndex = $null
    $script:ReleaseCacheDirty = $false
    if (-not $Disabled) { New-Item -ItemType Directory -Path $script:ReleaseCacheDirectory -Force | Out-Null }
}

function Get-ReleaseCacheArtifactDirectory { Join-Path $script:ReleaseCacheDirectory 'artifacts' }
function Get-ReleaseCacheIndexFile { Join-Path $script:ReleaseCacheDirectory 'index.json' }
function Get-ReleaseCacheFile([string] $Key) { Join-Path (Get-ReleaseCacheArtifactDirectory) "$Key.json" }

function ConvertTo-ReleaseSha256 {
    param([Parameter(Mandatory)] [AllowEmptyString()] [string] $Text)

    $Sha = [Security.Cryptography.SHA256]::Create()
    try {
        $Bytes = [Text.Encoding]::UTF8.GetBytes($Text)
        return (($Sha.ComputeHash($Bytes) | ForEach-Object { $_.ToString('x2') }) -join '')
    } finally { $Sha.Dispose() }
}

function Get-ReleaseFileSha256 {
    param([Parameter(Mandatory)] [string] $Path)

    $Sha = [Security.Cryptography.SHA256]::Create()
    # ReadWrite share: a build has just written this file and an indexer may still hold it.
    $Stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
    try { return (($Sha.ComputeHash($Stream) | ForEach-Object { $_.ToString('x2') }) -join '') }
    finally { $Stream.Dispose(); $Sha.Dispose() }
}

function Get-ReleaseFileEntry {
    param([Parameter(Mandatory)] [IO.FileInfo] $File, [string] $Relative,
        [Parameter(Mandatory)] $Sha, [bool] $MetadataOnly)

    # Metadata-only mode trades accuracy for speed on trees that are too large to hash
    # (assets): path, size and modification time still change when the content changes,
    # and a false miss only costs a rebuild.
    if ($MetadataOnly) { return "$Relative|$($File.Length)|$($File.LastWriteTimeUtc.Ticks)" }

    $Stream = $null
    try {
        $Stream = [IO.File]::Open($File.FullName, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::ReadWrite)
        $Hash = (($Sha.ComputeHash($Stream) | ForEach-Object { $_.ToString('x2') }) -join '')
    } catch {
        $Hash = 'unreadable'
    } finally { if ($Stream) { $Stream.Dispose() } }
    return "$Relative|$($File.Length)|$Hash"
}

function Get-ReleaseTreeFingerprint {
    param(
        [Parameter(Mandatory)] [string[]] $Roots,
        # Build output and generated sources are excluded: they are what a fingerprinted
        # stage produces, so letting them back in would make every run differ from the last.
        [string[]] $ExcludeDirectories = @('Binaries', 'Intermediate', 'Generated', 'Saved', '.git'),
        [switch] $MetadataOnly
    )

    $Entries = [Collections.Generic.List[string]]::new()
    $Sha = [Security.Cryptography.SHA256]::Create()
    try {
        foreach ($Root in $Roots) {
            $Full = [IO.Path]::GetFullPath($Root)
            if (Test-Path -LiteralPath $Full -PathType Leaf) {
                $Entries.Add((Get-ReleaseFileEntry (Get-Item -LiteralPath $Full) ([IO.Path]::GetFileName($Full)) $Sha $MetadataOnly))
                continue
            }
            if (-not (Test-Path -LiteralPath $Full -PathType Container)) {
                $Entries.Add('absent|' + [IO.Path]::GetFileName($Full))
                continue
            }
            foreach ($File in (Get-ChildItem -LiteralPath $Full -Recurse -File -Force -ErrorAction SilentlyContinue)) {
                $Relative = $File.FullName.Substring($Full.Length).TrimStart('\', '/')
                $Parents = @($Relative -split '[\\/]' | Select-Object -SkipLast 1)
                if (@($Parents | Where-Object { $ExcludeDirectories -contains $_ })) { continue }
                $Entries.Add((Get-ReleaseFileEntry $File $Relative $Sha $MetadataOnly))
            }
        }
    } finally { $Sha.Dispose() }

    return (ConvertTo-ReleaseSha256 (($Entries | Sort-Object) -join "`n"))
}

function ConvertTo-ReleaseFingerprint {
    param([Parameter(Mandatory)] [string[]] $Parts)
    return (ConvertTo-ReleaseSha256 (($Parts | Where-Object { $_ }) -join "`n"))
}

function Get-ReleaseCacheIndex {
    if ($script:ReleaseCacheIndex) { return $script:ReleaseCacheIndex }

    $Index = $null
    $File = Get-ReleaseCacheIndexFile
    if (Test-Path -LiteralPath $File) {
        try { $Index = Get-Content -LiteralPath $File -Raw -Encoding UTF8 | ConvertFrom-Json -AsHashtable } catch { $Index = $null }
    }
    # A damaged index is not worth failing a release over: it only means one rebuild. The
    # check is "does it have entries", not "is it exactly a [hashtable]": -AsHashtable returns
    # an ordered dictionary that is not a [hashtable], and rejecting it silently threw away
    # every recorded build, so no stage could ever be reused.
    $Entries = if ($Index) { $Index['entries'] } else { $null }
    if ($null -eq $Entries -or $null -eq $Entries.Keys) { $Index = @{ schema_version = 1; entries = @{} }; $Entries = $Index['entries'] }
    # Entries written before the switch to ticks carry an ISO timestamp only. Fill the ticks
    # in rather than declaring every earlier build unknown: that would cost a full rebuild.
    foreach ($Key in @($Entries.Keys)) {
        foreach ($Record in @($Entries[$Key]['artifacts'])) {
            if ($null -eq $Record -or $Record['modified_ticks']) { continue }
            $Stamp = $Record['modified_utc']
            if (-not $Stamp) { continue }
            $Record['modified_ticks'] = if ($Stamp -is [DateTime]) { $Stamp.Ticks } else {
                [DateTime]::Parse([string]$Stamp, [Globalization.CultureInfo]::InvariantCulture,
                    [Globalization.DateTimeStyles]::AssumeUniversal -bor [Globalization.DateTimeStyles]::AdjustToUniversal).Ticks
            }
        }
    }
    $script:ReleaseCacheIndex = $Index
    return $Index
}

function Write-ReleaseCacheIndex {
    if ($script:ReleaseCacheDisabled -or -not $script:ReleaseCacheDirty) { return }
    $Index = Get-ReleaseCacheIndex
    $Index['updated_utc'] = [DateTime]::UtcNow.ToString('o')
    ($Index | ConvertTo-Json -Depth 12) | Set-Content -LiteralPath (Get-ReleaseCacheIndexFile) -Encoding UTF8
    $script:ReleaseCacheDirty = $false
}

function Get-ReleaseArtifactRecord {
    param([string[]] $Paths)

    $Records = [Collections.Generic.List[object]]::new()
    foreach ($Path in @($Paths | Where-Object { $_ })) {
        # A stage whose output is missing was never a build: recording the fingerprint
        # would let the next run claim a build that produced nothing.
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
        $Item = Get-Item -LiteralPath $Path
        $Records.Add([ordered]@{ path = $Item.FullName; bytes = $Item.Length;
            # Ticks are what is compared: an ISO timestamp survives JSON as a DateTime, and
            # turning that back into a string for comparison gives a culture-specific form
            # that never matches, which made every recorded build look like a different one.
            # The readable stamp is kept for anyone opening the file.
            modified_ticks = $Item.LastWriteTimeUtc.Ticks
            modified_utc   = $Item.LastWriteTimeUtc.ToString('o')
            # Hashing is skipped for very large files; size and mtime still catch a rebuild.
            sha256      = if ($Item.Length -le 512MB) { Get-ReleaseFileSha256 $Item.FullName } else { '' } })
    }
    return @($Records)
}

function Test-ReleaseCacheHit {
    param([Parameter(Mandatory)] [string] $Key, [Parameter(Mandatory)] [string] $Fingerprint, [string[]] $Paths = @())

    if ($script:ReleaseCacheDisabled -or -not $Fingerprint) { return $false }
    $Entry = (Get-ReleaseCacheIndex)['entries'][$Key]
    if (-not $Entry) { return $false }
    if ([string]$Entry['fingerprint'] -ne $Fingerprint) { return $false }

    $Wanted = @($Paths | Where-Object { $_ })
    $Recorded = @($Entry['artifacts'])
    # Nothing recorded means nothing was produced, which is never a hit.
    if ($Recorded.Count -eq 0) { return $false }

    # Every recorded artifact is re-checked. The requested list is allowed to be a different
    # length: a stage that started tracking an extra file (or stopped) must not invalidate
    # every earlier entry, as long as the files it now asks for are there.
    foreach ($Record in $Recorded) {
        # Property access, not an indexer: depending on how the index was read the record is
        # a hashtable or a custom object, and indexing the latter returns nothing at all.
        $Path = [string]$Record.path
        if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
        $Item = Get-Item -LiteralPath $Path
        if ($Item.Length -ne [long]$Record.bytes) { return $false }
        if ($Item.LastWriteTimeUtc.Ticks -ne [long]$Record.modified_ticks) { return $false }
        $Hash = [string]$Record.sha256
        if ($Hash -and (Get-ReleaseFileSha256 $Path) -ne $Hash) { return $false }
    }
    foreach ($Path in $Wanted) {
        if ($Recorded.path -notcontains [IO.Path]::GetFullPath($Path) -and
            -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    }
    return $true
}

function Set-ReleaseCacheEntry {
    param([Parameter(Mandatory)] [string] $Key, [Parameter(Mandatory)] [string] $Fingerprint, [string[]] $Paths = @())

    if ($script:ReleaseCacheDisabled -or -not $Fingerprint) { return }
    $Artifacts = Get-ReleaseArtifactRecord -Paths $Paths
    if ($null -eq $Artifacts) { return }

    $Index = Get-ReleaseCacheIndex
    $Index['entries'][$Key] = [ordered]@{ fingerprint = $Fingerprint;
        recorded_utc = [DateTime]::UtcNow.ToString('o'); artifacts = $Artifacts }
    $script:ReleaseCacheDirty = $true
    # Written immediately: a run that is killed must not leave "not built" behind.
    Write-ReleaseCacheIndex
}

function Restore-ReleaseCacheFile {
    param([Parameter(Mandatory)] [string] $Key, [Parameter(Mandatory)] [string] $Destination)

    $Cached = Get-ReleaseCacheFile $Key
    if ($script:ReleaseCacheDisabled -or -not (Test-Path -LiteralPath $Cached)) { return $false }
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    Copy-Item -LiteralPath $Cached -Destination $Destination -Force
    return $true
}

function Save-ReleaseCacheFile {
    param([Parameter(Mandatory)] [string] $Key, [Parameter(Mandatory)] [string] $Source)

    if ($script:ReleaseCacheDisabled -or -not (Test-Path -LiteralPath $Source)) { return }
    $Cached = Get-ReleaseCacheFile $Key
    New-Item -ItemType Directory -Path (Split-Path -Parent $Cached) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Cached -Force
}

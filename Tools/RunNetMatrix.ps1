<#
.SYNOPSIS
    Connected network delivery matrix: Legacy / PushOff / Iris, plus late join, reconnect,
    dormancy and relevancy. Opt-in and separate from the ordinary Release matrix.
.DESCRIPTION
    Every cell launches real Server + Client1 + Client2 processes. The driving process gets
    -UnLuaNovaNetScenarios for the four scenarios and UNLUANOVA_TEST_NET_VARIANT for the
    delivery variant; FNetSession turns the latter into real engine switches on each peer
    (-UseIrisReplication=1 or a ConsoleVariables ini override), and AppendWorldFacts reads the
    resulting cvar state back. A variant therefore cannot pass by renaming a directory.

    Evidence is copied into one release-shaped directory per backend and judged by
    ValidateRelease.py --net-matrix. That gate fails closed: a missing topology.json, a
    missing peer log, a reused PID, a faked Iris flag or a scenario that never ran all fail.

    This is a source + gate delivery. Running it needs freshly built Editor/Game/Server
    binaries and a real Dedicated Server launch; none of those are implied by this file.
.EXAMPLE
    .\Tools\RunNetMatrix.ps1 -Backend Lua -SkipBuild
#>
[CmdletBinding()]
param(
    [string] $EnginePath = '',
    [string] $ProjectPath = '',
    [string] $OutputDir = '',
    [ValidateSet('Lua', 'Luau', 'Both')] [string] $Backend = 'Both',
    [string] $ConnectedFilter = 'UnLuaNova.API.Net.DedicatedServer',
    [string] $ScenarioFilter = 'UnLuaNova.Slow.Net.Scenarios',
    [ValidateRange(1, 1440)] [int] $TimeoutMinutes = 60,
    [switch] $SkipBuild,
    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDir) { $OutputDir = Join-Path $RepoRoot 'Saved/ReleaseTests/net-matrix' }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$RunTests = Join-Path $PSScriptRoot 'RunTests.ps1'
$Variants = @('Legacy', 'PushOff', 'Iris')
$Scenarios = @('LateJoin', 'Reconnect', 'Dormancy', 'Relevancy')
$Backends = if ($Backend -eq 'Both') { @('Lua', 'Luau') } else { @($Backend) }

function Copy-Tree([string] $Source, [string] $Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing connected evidence at $Source - the peer run did not produce a topology."
    }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Copy-Item -Path (Join-Path $Source '*') -Destination $Destination -Recurse -Force
}

function Invoke-Cell([string] $Label, [string] $BackendName, [string] $Filter, [string] $ExtraArgs,
    [string] $Variant, [string] $Scratch) {
    # A cell must not leak its variant or net root into the next cell or the caller's shell:
    # a stale UNLUANOVA_TEST_NET_VARIANT would make a Legacy run report itself as Iris.
    $SavedVariant = [Environment]::GetEnvironmentVariable('UNLUANOVA_TEST_NET_VARIANT', 'Process')
    $SavedNetRoot = [Environment]::GetEnvironmentVariable('UNLUANOVA_TEST_NET_ROOT', 'Process')
    try {
        if (Test-Path -LiteralPath $Scratch) { Remove-Item -LiteralPath $Scratch -Recurse -Force }
        New-Item -ItemType Directory -Path $Scratch -Force | Out-Null
        $env:UNLUANOVA_TEST_NET_VARIANT = $Variant
        $env:UNLUANOVA_TEST_NET_ROOT = Join-Path $Scratch 'net-root'
        Write-Host "NetMatrix: $Label ($Variant) -> $Scratch" -ForegroundColor Cyan
        if ($DryRun) { return }
        & $RunTests -Profile Development -Backend $BackendName -Carrier Game -Tier All -Filter $Filter `
            -OutputDir $Scratch -ExtraArgs $ExtraArgs -SkipBuild:$SkipBuild `
            -TimeoutMinutes $TimeoutMinutes -EnginePath $EnginePath -ProjectPath $ProjectPath
        if ($LASTEXITCODE -ne 0) { throw "NetMatrix cell failed: $Label ($LASTEXITCODE)" }
    } finally {
        [Environment]::SetEnvironmentVariable('UNLUANOVA_TEST_NET_VARIANT', $SavedVariant, 'Process')
        [Environment]::SetEnvironmentVariable('UNLUANOVA_TEST_NET_ROOT', $SavedNetRoot, 'Process')
    }
}

if (-not $DryRun) { New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null }

foreach ($B in $Backends) {
    $RunDir = Join-Path $OutputDir $B
    if (-not $DryRun) {
        if (Test-Path -LiteralPath $RunDir) { Remove-Item -LiteralPath $RunDir -Recurse -Force }
        New-Item -ItemType Directory -Path (Join-Path $RunDir 'metadata') -Force | Out-Null
    }

    foreach ($Variant in $Variants) {
        $Flags = switch ($Variant) {
            'Iris' { '-UseIrisReplication=1' }
            'PushOff' { '-ini:Engine:[ConsoleVariables]:Net.IsPushModelEnabled=0' }
            default { '' }
        }
        $CellArgs = (@("-UnLuaNovaNetVariant=$Variant") + @($Flags | Where-Object { $_ })) -join ' '
        $Scratch = Join-Path $RunDir "scratch/variant-$Variant"
        Invoke-Cell "variant/$B/$Variant" $B $ConnectedFilter $CellArgs $Variant $Scratch
        if (-not $DryRun) {
            Copy-Tree (Join-Path $Scratch 'net/DedicatedServer') `
                (Join-Path $RunDir "net/variants/$Variant/DedicatedServer")
        }
    }

    $ScenarioArgs = "-UnLuaNovaNetScenarios -UnLuaNovaNetVariant=Legacy"
    $Scratch = Join-Path $RunDir 'scratch/scenarios'
    Invoke-Cell "scenarios/$B" $B $ScenarioFilter $ScenarioArgs 'Legacy' $Scratch
    if (-not $DryRun) {
        foreach ($Scenario in $Scenarios) {
            Copy-Tree (Join-Path $Scratch "net/$Scenario") (Join-Path $RunDir "net/scenarios/$Scenario")
        }
    }

    if ($DryRun) { continue }
    $Manifest = Join-Path $RunDir 'metadata/net-variants.json'
    [IO.File]::WriteAllText($Manifest, (@{ variants = $Variants } | ConvertTo-Json -Compress))
    $Python = if ($env:UNLUANOVA_PYTHON) { $env:UNLUANOVA_PYTHON } else { 'python' }
    & $Python (Join-Path $PSScriptRoot 'ValidateRelease.py') --net-matrix `
        --run $RunDir --backend $B --carrier game
    if ($LASTEXITCODE -ne 0) { throw "Network matrix gate failed for backend $B" }
}

Write-Host "Network matrix evidence: $OutputDir" -ForegroundColor Green

<#
.SYNOPSIS
    Connected network delivery matrix: Legacy / PushOff / Iris / Cooked, plus eight connected
    scenarios. Opt-in and separate from the ordinary Release matrix.
.DESCRIPTION
    Every cell launches real Server + Client1 + Client2 processes. The driving process gets
    -UnLuaNovaNetScenarios and UNLUANOVA_TEST_NET_VARIANT; FNetSession forwards the scenario
    flag, the packet-loss percentage and the real reconnect URL to every peer, and turns the
    variant into real engine switches (-UseIrisReplication=1 or a ConsoleVariables ini
    override). AppendWorldFacts reads the resulting cvar state, WITH_EDITOR and the engine's
    connection byte counters back, so a variant cannot pass by renaming a directory.

    Eight scenarios: LateJoin, Reconnect (process replacement), Dormancy, Relevancy,
    JoinOrderSwap, PacketLoss (NetEmulation), SeamlessReconnect (UNetConnection::Close +
    in-process reopen) and SchemaMismatch (cross-peer declaration fingerprints).
    The Cooked variant launches staged WindowsServer / Windows binaries as the peers, so
    packaged-peer acceptance is its own cell rather than a claim about the Editor build.

    Evidence is copied into one release-shaped directory per backend and judged by
    ValidateRelease.py --net-matrix. That gate fails closed: a missing topology.json, a
    missing peer log, a reused PID, a faked Iris/Cooked flag, a mismatched declaration
    fingerprint or a scenario that never ran all fail.

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
$Variants = @('Legacy', 'PushOff', 'Iris', 'Cooked')
$Scenarios = @('LateJoin', 'Reconnect', 'Dormancy', 'Relevancy',
    'JoinOrderSwap', 'PacketLoss', 'SeamlessReconnect', 'SchemaMismatch')
$Backends = if ($Backend -eq 'Both') { @('Lua', 'Luau') } else { @($Backend) }

function Copy-Tree([string] $Source, [string] $Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing connected evidence at $Source - the peer run did not produce a topology."
    }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Copy-Item -Path (Join-Path $Source '*') -Destination $Destination -Recurse -Force
}

function Resolve-StagedExe([string] $Stage, [string] $ExeName) {
    $Found = @(Get-ChildItem -LiteralPath $Stage -Recurse -File -Filter $ExeName -ErrorAction SilentlyContinue |
            Where-Object { $_.Directory -and $_.Directory.Name -eq 'Win64' -and
                $_.Directory.Parent -and $_.Directory.Parent.Name -eq 'Binaries' })
    if ($Found.Count -ne 1) {
        throw "Expected one packaged $ExeName under $Stage/Binaries/Win64, found $($Found.Count). Run a Release build first."
    }
    return $Found[0].FullName
}

function Invoke-Cell([string] $Label, [string] $BackendName, [string] $Filter, [string] $ExtraArgs,
    [string] $Variant, [string] $Scratch, [string] $PeerExe = '', [string] $ServerExe = '') {
    # A cell must not leak its variant, net root or packaged binaries into the next cell or
    # the caller's shell: a stale UNLUANOVA_TEST_NET_VARIANT would make a Legacy run report
    # itself as Iris, and a stale PEER_EXE would silently turn Cooked into an Editor run.
    $Names = @('UNLUANOVA_TEST_NET_VARIANT', 'UNLUANOVA_TEST_NET_ROOT',
        'UNLUANOVA_TEST_PEER_EXE', 'UNLUANOVA_TEST_SERVER_EXE')
    $Saved = @{}
    foreach ($Name in $Names) { $Saved[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process') }
    try {
        if (Test-Path -LiteralPath $Scratch) { Remove-Item -LiteralPath $Scratch -Recurse -Force }
        New-Item -ItemType Directory -Path $Scratch -Force | Out-Null
        $env:UNLUANOVA_TEST_NET_VARIANT = $Variant
        $env:UNLUANOVA_TEST_NET_ROOT = Join-Path $Scratch 'net-root'
        [Environment]::SetEnvironmentVariable('UNLUANOVA_TEST_PEER_EXE', $PeerExe, 'Process')
        [Environment]::SetEnvironmentVariable('UNLUANOVA_TEST_SERVER_EXE', $ServerExe, 'Process')
        Write-Host "NetMatrix: $Label ($Variant) -> $Scratch" -ForegroundColor Cyan
        if ($DryRun) { return }
        & $RunTests -Profile Development -Backend $BackendName -Carrier Game -Tier All -Filter $Filter `
            -OutputDir $Scratch -ExtraArgs $ExtraArgs -SkipBuild:$SkipBuild `
            -TimeoutMinutes $TimeoutMinutes -EnginePath $EnginePath -ProjectPath $ProjectPath
        if ($LASTEXITCODE -ne 0) { throw "NetMatrix cell failed: $Label ($LASTEXITCODE)" }
    } finally {
        foreach ($Name in $Names) {
            [Environment]::SetEnvironmentVariable($Name, $Saved[$Name], 'Process')
        }
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
        $PeerExe = ''
        $ServerExe = ''
        if ($Variant -eq 'Cooked' -and -not $DryRun) {
            $ProjectFile = if ($ProjectPath) { $ProjectPath } else { Join-Path $RepoRoot 'UnLuaNovaDemo.uproject' }
            $ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProjectFile)
            $PeerExe = Resolve-StagedExe (Join-Path $RepoRoot 'Saved/StagedBuilds/Windows') "$ProjectName.exe"
            $ServerExe = Resolve-StagedExe (Join-Path $RepoRoot 'Saved/StagedBuilds/WindowsServer') "$ProjectName`Server.exe"
        }
        $Scratch = Join-Path $RunDir "scratch/variant-$Variant"
        Invoke-Cell "variant/$B/$Variant" $B $ConnectedFilter $CellArgs $Variant $Scratch $PeerExe $ServerExe
        if (-not $DryRun) {
            Copy-Tree (Join-Path $Scratch 'net/DedicatedServer') `
                (Join-Path $RunDir "net/variants/$Variant/DedicatedServer")
        }
    }

    # Forwarded to every peer by FNetSession; only the PacketLoss scenario's peer command
    # actually issues the NetEmulation.* console commands.
    $ScenarioArgs = "-UnLuaNovaNetScenarios -UnLuaNovaNetVariant=Legacy -UnLuaNovaNetPktLoss=25"
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

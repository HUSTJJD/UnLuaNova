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
    The Cooked variant launches staged WindowsServer / Windows Development binaries as the
    peers; the other three variants and all eight scenarios use the Editor carrier. The gate
    reads compile-time WITH_EDITOR back as the "Cooked" field, so Editor peers are required
    for those cells and a real packaged peer only for Cooked. Running everything in -game
    made every peer report Cooked=true and the gate could never pass.

    Evidence is copied into one release-shaped directory per backend and judged by
    ValidateRelease.py --net-matrix. That gate fails closed: a missing topology.json, a
    missing peer log, a reused PID, a faked Iris/Cooked flag, a mismatched declaration
    fingerprint or a scenario that never ran all fail.

    This is a source + gate delivery. Running it needs freshly built Editor/Game/Server
    binaries and a real Dedicated Server launch; none of those are implied by this file.
.EXAMPLE
    .\Tools\RunNetMatrix.ps1 -Backend Lua -SkipBuild
.EXAMPLE
    .\Tools\RunDevelopmentTests.ps1 -NetMatrix -SkipBuild
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
    # Three editor peers per cell can spend several minutes in module/shader/DDC startup
    # on a loaded machine. This widens the driver's wait for peer reports without
    # weakening the fail-fast default used by ordinary automation runs.
    [ValidateRange(60, 1800)] [int] $ReportTimeoutSeconds = 600,
    [switch] $SkipBuild,
    # Every cell runs against the prebuilt backend-isolated Game/Server targets; there is
    # no ordinary-target path.
    [switch] $DryRun,
    # Set by ReleasePipeline.ps1: the parent pipeline already owns RunTests.lock, so
    # every cell must verify the release token instead of trying to take the lock again.
    [switch] $ReleaseManaged,
    # Debug-only: run a subset of cells (variant names such as "Iris", or "scenarios"),
    # optionally with -Diagnostic to turn on the engine's Iris RPC logging. A partial run
    # skips ValidateRelease.py and is never a release verdict; it exists so one cell can
    # be investigated in minutes instead of re-running a two-hour Release.
    [string[]] $OnlyCells = @(),
    [switch] $SkipGate,
    [switch] $Diagnostic
)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDir) { $OutputDir = Join-Path $RepoRoot 'Saved/ReleaseTests/net-matrix' }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
$RunDevTests = Join-Path $PSScriptRoot 'RunDevelopmentTests.ps1'
$Variants = @('Legacy', 'PushOff', 'Iris', 'Cooked')
$Scenarios = @('LateJoin', 'Reconnect', 'Dormancy', 'Relevancy',
    'JoinOrderSwap', 'PacketLoss', 'SeamlessReconnect', 'SchemaMismatch')
$Backends = if ($Backend -eq 'Both') { @('Lua', 'Luau') } else { @($Backend) }
$RunToken = [DateTime]::UtcNow.ToString('yyyyMMddHHmmss')

function Test-CellSelected([string] $Label, [string] $ShortName, [string] $BackendName) {
    if ($OnlyCells.Count -eq 0) { return $true }
    foreach ($Wanted in $OnlyCells) {
        if ($Wanted -eq $ShortName -or $Wanted -eq $Label -or
            $Wanted -eq "variant/$BackendName/$ShortName" -or
            ($ShortName -eq 'scenarios' -and $Wanted -eq "scenarios/$BackendName")) {
            return $true
        }
    }
    return $false
}

function Copy-Tree([string] $Source, [string] $Destination) {
    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Missing connected evidence at $Source - the peer run did not produce a topology."
    }
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
    Copy-Item -Path (Join-Path $Source '*') -Destination $Destination -Recurse -Force
}

function Resolve-StagedExe([string] $Stage, [string] $ExeName) {
    # Only a *Development* packaged binary still contains the automation peer test. A
    # Shipping package strips it, so accepting "-Win64-Shipping.exe" here would produce a
    # Cooked cell whose peers never write a report - and the gate would then correctly fail.
    $Found = @(Get-ChildItem -LiteralPath $Stage -Recurse -File -Filter $ExeName -ErrorAction SilentlyContinue |
            Where-Object { $_.Directory -and $_.Directory.Name -eq 'Win64' -and
                $_.Directory.Parent -and $_.Directory.Parent.Name -eq 'Binaries' })
    if ($Found.Count -ne 1) {
        throw "Expected one Development staged $ExeName under $Stage/Binaries/Win64, found $($Found.Count). " +
              'A Shipping package cleans that staging directory; re-run the Development package ' +
              'stage (ReleasePipeline.ps1 <backend>-package-development) before the Cooked variant.'
    }
    return $Found[0].FullName
}

function Invoke-Cell([string] $Label, [string] $BackendName, [string] $Filter, [string] $ExtraArgs,
    [string] $Variant, [string] $Scratch, [string] $PeerExe = '', [string] $ServerExe = '',
    [string] $Carrier = 'Editor') {
    # A cell must not leak its variant, net root or packaged binaries into the next cell or
    # the caller's shell: a stale UNLUANOVA_TEST_NET_VARIANT would make a Legacy run report
    # itself as Iris, and a stale PEER_EXE would silently turn Cooked into an Editor run.
    $Names = @('UNLUANOVA_TEST_NET_VARIANT', 'UNLUANOVA_TEST_NET_ROOT',
        'UNLUANOVA_TEST_PEER_EXE', 'UNLUANOVA_TEST_SERVER_EXE',
        'UNLUANOVA_TEST_NET_REPORT_TIMEOUT')
    # The driver is another UE process on the same automation message bus. Give it its own
    # session id too: otherwise a peer's worker can answer the driver's FindWorkers and the
    # driver's filtered test executes in the wrong process. Peer ids are generated in
    # FNetSession::AddPeer so topology.json and the launched command line always agree.
    $DriverSessionId = [Guid]::NewGuid().ToString()
    # Build the token list explicitly. A pipeline result on the left of '+' can collapse to
    # a scalar string, in which case PowerShell concatenates instead of appending to the
    # array and -join never inserts the separator: the driver used to receive
    # "-UnLuaNovaNetVariant=Legacy-SessionId=<guid>" as ONE malformed argument.
    $DriverTokens = @()
    $TrimmedExtra = "$ExtraArgs".Trim()
    if ($TrimmedExtra) { $DriverTokens += $TrimmedExtra }
    $DriverTokens += "-SessionId=$DriverSessionId"
    $DriverArgs = $DriverTokens -join ' '
    $Saved = @{}
    foreach ($Name in $Names) { $Saved[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process') }
    try {
        $env:UNLUANOVA_TEST_NET_REPORT_TIMEOUT = [string]$ReportTimeoutSeconds
        # Scratch holds only this cell's generated reports. Use the .NET API so a host-level
        # Remove-Item wrapper cannot refuse to clear it between cells.
        if (Test-Path -LiteralPath $Scratch) { [IO.Directory]::Delete($Scratch, $true) }
        New-Item -ItemType Directory -Path $Scratch -Force | Out-Null
        $env:UNLUANOVA_TEST_NET_VARIANT = $Variant
        # A Development -game process remaps absolute paths under the project into its own
        # cooked root. The Windows client and WindowsServer server targets use different
        # roots, so peer reports used to land in two separate trees and the connected
        # session timed out reading them. A net root outside the project is not remapped,
        # so all three peers and the driver share one directory; the per-run token keeps
        # stale reports from a previous matrix out of this one.
        $script:CellNetRoot = Join-Path ([IO.Path]::GetTempPath()) "UnLuaNovaNetMatrix/$RunToken/$($Label -replace '[\\/]', '_')"
        $env:UNLUANOVA_TEST_NET_ROOT = $script:CellNetRoot
        [Environment]::SetEnvironmentVariable('UNLUANOVA_TEST_PEER_EXE', $PeerExe, 'Process')
        [Environment]::SetEnvironmentVariable('UNLUANOVA_TEST_SERVER_EXE', $ServerExe, 'Process')
        Write-Host "NetMatrix: $Label ($Variant, carrier $Carrier) -> $Scratch" -ForegroundColor Cyan
        if ($DryRun) { return }
        # -Carrier matters to the gate, not just to speed. AppendWorldFacts records
        # WITH_EDITOR at build time as the "Cooked" field: Editor binaries report false,
        # packaged binaries report true. ValidateRelease.py --net-matrix therefore requires
        # Editor peers for Legacy / PushOff / Iris and the eight scenarios, and a real
        # packaged peer only for Cooked. Running every cell with -Carrier Game made all of
        # them report Cooked=true and the gate failed nine times by construction.
        #
        # -NoReport: a cell runs one filtered automation test whose verdict is the net/
        # topology, peer reports and peer logs. The full-suite case-set baseline and the
        # per-cell IntelliSense/typecheck pass do not apply to a filtered connected run;
        # the UE process exit code and the fail-closed Copy-Tree/gate below still do.
        & $RunDevTests -Backend $BackendName -Carrier $Carrier -Tier All -Filter $Filter `
            -OutputDir $Scratch -ExtraArgs $DriverArgs -SkipBuild:$SkipBuild `
            -TimeoutMinutes $TimeoutMinutes -EnginePath $EnginePath -ProjectPath $ProjectPath -NoReport `
            -ReleaseManaged:$ReleaseManaged
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
        if (Test-Path -LiteralPath $RunDir) { [IO.Directory]::Delete($RunDir, $true) }
        New-Item -ItemType Directory -Path (Join-Path $RunDir 'metadata') -Force | Out-Null
    }

    # One failing cell must not abort the whole backend. These runs are expensive (three
    # real UE processes per cell, eight scenarios in one driver) and the gate is fail-closed:
    # a cell that produced no topology is reported as a missing-evidence error anyway.
    # Previously a missing staged Cooked exe threw before the scenario cell ever ran, so a
    # 40-minute matrix lost all eight scenarios to one packaging prerequisite.
    $CellErrors = [Collections.Generic.List[string]]::new()

    foreach ($Variant in $Variants) {
        if (-not (Test-CellSelected "variant/$B/$Variant" $Variant $B)) { continue }
        try {
            # UE 6.0 Iris requires registered subobject lists; the engine raises an ensure
            # otherwise and never assigns NetRefHandles to replicated components.
            $IrisFlags = @('-UseIrisReplication=1',
                '-ini:Engine:[SystemSettings]:net.SubObjects.DefaultUseSubObjectReplicationList=1',
                # FNetBlobManager::SendRPC returns false silently when this cvar is off or
                # the RPC handler was not created; UNetDriver then refuses to fall back to
                # legacy replication, so every Lua-declared RPC disappears without a log.
                '-ini:Engine:[SystemSettings]:net.Iris.EnableRPCs=1')
            if ($Diagnostic) {
                # Verbose engine logging is diagnostic-only: LogIrisRpc reports rejected
                # RPCs and LogIris reports "Unable to create RPC for function ...".
                $IrisFlags += @('-ini:Engine:[Core.Log]:LogIrisRpc=Verbose',
                    '-ini:Engine:[Core.Log]:LogIris=Verbose',
                    '-ini:Engine:[Core.Log]:LogNet=Verbose',
                    '-ini:Engine:[ConsoleVariables]:net.Iris.ThrottleRPCWarnings=0')
            }
            $Flags = switch ($Variant) {
                'Iris' { $IrisFlags }
                'PushOff' { @('-ini:Engine:[ConsoleVariables]:Net.IsPushModelEnabled=0') }
                default { @() }
            }
            # The Cooked cell's driver is an Editor process; its three *peers* are the staged
            # Development Game/Server binaries selected below. Driving the cell from a -game
            # binary under Binaries/Win64 aborted with "No COOKED content was found" before
            # any peer started, because only the staged tree contains cooked content. The
            # gate reads WITH_EDITOR from the peer reports, so an Editor driver does not
            # weaken the "this really is a packaged peer" assertion.
            $CellArgs = (@("-UnLuaNovaNetVariant=$Variant") + @($Flags | Where-Object { $_ })) -join ' '
            $PeerExe = ''
            $ServerExe = ''
            if ($Variant -eq 'Cooked' -and -not $DryRun) {
                $ProjectFile = if ($ProjectPath) { $ProjectPath } else { Join-Path $RepoRoot 'UnLuaNovaDemo.uproject' }
                $ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProjectFile)
                # An isolated Game/Server target is staged under its own target name (AppName),
                # not the ordinary project name. Looking for the old .exe would either fail or
                # pick up a build from the other backend's Release run.
                $GameTarget = "UnLuaNovaDemo$B"
                $ServerTarget = "UnLuaNovaDemo${B}Server"
                # ReleasePipeline stages each backend into its own directory
                # (UAT cleans the staging directory on every -stage), so look there as well.
                $DevStage = Join-Path $RepoRoot "Saved/StagedBuilds/Windows/Backends/$B"
                $ServerStage = Join-Path $RepoRoot "Saved/StagedBuilds/WindowsServer/Backends/$B"
                $PeerExe = Resolve-StagedExe $DevStage "$GameTarget.exe"
                $ServerExe = Resolve-StagedExe $ServerStage "$ServerTarget.exe"
            }
            $Scratch = Join-Path $RunDir "scratch/variant-$Variant"
            # Every cell is driven from the Editor build. Only the Cooked cell's three peers
            # are packaged binaries; passing the staged exes is what makes it a genuinely
            # packaged cell instead of a renamed Editor run. WITH_EDITOR=0 in those peer
            # reports is what ValidateRelease.py reads as Cooked=true.
            $CellCarrier = 'Editor'
            Invoke-Cell "variant/$B/$Variant" $B $ConnectedFilter $CellArgs $Variant $Scratch `
                $PeerExe $ServerExe -Carrier $CellCarrier
            if (-not $DryRun) {
                Copy-Tree (Join-Path $script:CellNetRoot 'DedicatedServer') `
                    (Join-Path $RunDir "net/variants/$Variant/DedicatedServer")
            }
        } catch {
            $CellErrors.Add("variant/$B/$Variant : $($_.Exception.Message)")
            Write-Warning "NetMatrix cell 'variant/$B/$Variant' failed: $($_.Exception.Message)"
        }
    }

    # Forwarded to every peer by FNetSession; only the PacketLoss scenario's peer command
    # actually issues the NetEmulation.* console commands.
    $RunScenarios = Test-CellSelected "scenarios/$B" 'scenarios' $B
    if ($RunScenarios) {
        try {
            $ScenarioArgs = "-UnLuaNovaNetScenarios -UnLuaNovaNetVariant=Legacy -UnLuaNovaNetPktLoss=25"
            $Scratch = Join-Path $RunDir 'scratch/scenarios'
            Invoke-Cell "scenarios/$B" $B $ScenarioFilter $ScenarioArgs 'Legacy' $Scratch
        } catch {
            $CellErrors.Add("scenarios/$B : $($_.Exception.Message)")
            Write-Warning "NetMatrix cell 'scenarios/$B' failed: $($_.Exception.Message)"
        }
    }
    if (-not $DryRun -and $RunScenarios) {
        # Copy each scenario independently: one missing topology is a gate error, not a
        # reason to discard the seven other scenarios the driver did produce. In a partial
        # debug run the other scenarios were never requested, so their absence is expected.
        $PartialRun = ($OnlyCells.Count -gt 0) -or $SkipGate
        foreach ($Scenario in $Scenarios) {
            try {
                Copy-Tree (Join-Path $script:CellNetRoot $Scenario) (Join-Path $RunDir "net/scenarios/$Scenario")
            } catch {
                if ($PartialRun) { continue }
                $CellErrors.Add("scenarios/$B/$Scenario : $($_.Exception.Message)")
                Write-Warning "NetMatrix scenario '$Scenario' has no evidence: $($_.Exception.Message)"
            }
        }
    }
    if ($DryRun) { continue }
    $Partial = ($OnlyCells.Count -gt 0) -or $SkipGate
    $GateExit = 0
    if ($Partial) {
        Write-Host 'Partial matrix: skipping ValidateRelease.py --net-matrix (not a release verdict).' -ForegroundColor Yellow
    } else {
        $Manifest = Join-Path $RunDir 'metadata/net-variants.json'
        [IO.File]::WriteAllText($Manifest, (@{ variants = $Variants } | ConvertTo-Json -Compress))
        $Python = if ($env:UNLUANOVA_PYTHON) { $env:UNLUANOVA_PYTHON } else { 'python' }
        & $Python (Join-Path $PSScriptRoot 'ValidateRelease.py') --net-matrix `
            --run $RunDir --backend $B --carrier game
        $GateExit = $LASTEXITCODE
    }
    if ($CellErrors.Count -gt 0) {
        foreach ($Line in $CellErrors) { Write-Host "CELL FAILURE: $Line" -ForegroundColor Red }
        throw "Network matrix for backend $B had $($CellErrors.Count) failed cell(s); the gate above lists the missing evidence."
    }
    if ($GateExit -ne 0) { throw "Network matrix gate failed for backend $B" }
}

Write-Host "Network matrix evidence: $OutputDir" -ForegroundColor Green

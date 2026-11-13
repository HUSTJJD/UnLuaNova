[CmdletBinding()]
param([string] $EnginePath = '', [string] $ProjectPath = '', [string] $OutputDir = '',
    [ValidateRange(1, 1440)] [int] $TimeoutMinutes = 60,
    # Write this run to Saved/ReleaseTests/baseline instead of Saved/ReleaseTests/test.
    # The two directories are the whole history: the run being judged, and the run it is
    # judged against.
    [switch] $Baseline,
    # Rebuild, re-cook and re-package everything even when the inputs did not change.
    [switch] $NoCache,
    # Delete the per-timestamp result directories the pipeline used to create
    # (Saved/ReleaseTests/<yyyyMMdd-HHmmss-fff>).
    [switch] $PruneLegacy,
    # Backend isolation is unconditional: only the backend-isolated Editor/Game/Server
    # targets are built, into Binaries/Win64/Backends/<Backend>. Both backends' products
    # stay on disk, so no stage overwrites the other one's build.
    [switch] $DryRun)

$ErrorActionPreference = 'Stop'

# RunTests.ps1 deliberately takes no parameters, so the one knob a cold engine
# upgrade needs is an environment variable rather than a switch. UE 6.0 performs a
# FULL COOK on a cold DDC and a single shader permutation can exceed 100 seconds;
# the 60-minute default is not enough for that first run. Example:
#   $env:UNLUANOVA_RELEASE_TIMEOUT_MINUTES = 180; .\Tools\RunTests.ps1
# The override applies to every external stage through Run below. It is deliberately
# NOT a partial-run switch: the complete Release matrix is still always executed.
if ($env:UNLUANOVA_RELEASE_TIMEOUT_MINUTES) {
    $OverrideMinutes = 0
    if (-not [int]::TryParse($env:UNLUANOVA_RELEASE_TIMEOUT_MINUTES, [ref]$OverrideMinutes) -or
        $OverrideMinutes -lt 1 -or $OverrideMinutes -gt 1440) {
        throw "UNLUANOVA_RELEASE_TIMEOUT_MINUTES must be an integer in 1..1440 (got '$($env:UNLUANOVA_RELEASE_TIMEOUT_MINUTES)')."
    }
    if ($OverrideMinutes -ne $TimeoutMinutes) {
        Write-Host "Timeout   : $OverrideMinutes min (UNLUANOVA_RELEASE_TIMEOUT_MINUTES)" -ForegroundColor DarkGray
        $TimeoutMinutes = $OverrideMinutes
    }
}

$RepoRoot = Split-Path -Parent $PSScriptRoot
$PowerShellExe = (Get-Process -Id $PID).Path
. (Join-Path $PSScriptRoot 'ReleaseSupport.ps1')
. (Join-Path $PSScriptRoot 'ReleaseCache.ps1')

if ($DryRun) {
    Write-Host @'
Release / Win64 / Lua + Luau (no operations performed):
  1. Lock project; snapshot config/environment and source provenance.
  2. Initialize pinned submodules; prepare isolated Python tools and MSVC environment;
     archive stale ordinary-layout DLLs/manifests so only one backend's UCLASS layout
     is visible to the engine.
  3. Repository audit, Python/PowerShell tooling regressions, both analyzers, and a
     strict documentation site build.
  4. Per backend: enable features; build Editor/generator; generate this backend's bindings; rebuild.
  5. Build/cook/stage Development game and Dedicated Server targets into per-backend staging/cook directories.
  6. Full Editor automation including Slow, connected cooked peers, screenshots and benchmark.
  7. Fresh declaration export and mandatory backend type checking.
  8. Full applicable cooked Development automation with runtime manifest verification.
  9. Build/cook/stage Shipping game (packaging gate, not Shipping runtime certification).
 10. Connected delivery matrix: eight scenarios and Legacy / PushOff / Iris / Cooked
     variants per backend, three peer processes per cell, judged by ValidateRelease.py
     --net-matrix inside the same run directory.
 11. Record execution evidence for the four carriers; enforce provenance, promotion and
     release-claim gates; write release-summary.json and release-summary.md.
 12. Restore original config/environment and backend layout.

Every release check in this repository is a stage of this one command. There is no second
command to remember: docs site, connected matrix, evidence, provenance, promotion and the
eight published release markers all run here. Build stages are reused when their inputs
did not change (Saved/ReleaseCache); test stages always run. Results land in
Saved/ReleaseTests/test (or /baseline with -Baseline).
Missing/skipped evidence, timeouts, failed/blocked stages and absent performance baselines fail.
'@
    Write-Host @'
Backend isolation (default, not a switch): steps 4/5/8 build
UnLuaNovaDemo<Backend>{Editor,Server,''} into Binaries/Win64/Backends/<Backend> and
activate that backend with Tools/SwitchBackend.ps1. Cooked assets go to
Saved/Cooked/Backends/<Backend>/<Configuration>/<CookPlatform> and generated static binding
sources to Source/UnLuaNova/Generated/Backends/<Backend>/, so no stage overwrites another
one's output and no shared binding source needs backup/restore. Stage caches are keyed
per isolated target.
'@
    exit 0
}

# Two result directories, not one per run: a timestamp turns every run into a new pile of
# evidence, and after a dozen runs nobody can tell which one is the answer or what the
# previous one said. `baseline` holds the run being compared against, `test` the run being
# judged, and each run overwrites its own.
$ReleaseRoot = Join-Path $RepoRoot 'Saved/ReleaseTests'
if ($PruneLegacy) {
    # Housekeeping only, and it exits: pruning is not a release run, and silently starting a
    # 70-minute matrix because someone asked to delete old directories is not a surprise
    # anyone wants. Only the directories the old scheme produced: <yyyyMMdd-HHmmss-fff>.
    $Pruned = @(Get-ChildItem -LiteralPath $ReleaseRoot -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^\d{8}-\d{6}-\d{3}$' })
    foreach ($Directory in $Pruned) {
        Write-Host "Prune   : $($Directory.FullName)"
        try { [IO.Directory]::Delete($Directory.FullName, $true) } catch { }
    }
    Write-Host "Pruned $($Pruned.Count) legacy result directory/directories from $ReleaseRoot."
    exit 0
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $ReleaseRoot $(if ($Baseline) { 'baseline' } else { 'test' })
    # A failed run can leave cache entries for a PARTIAL build: UBT may have written a
    # DLL and a definitions header but never produced the *.modules manifest, cooked
    # assets or staged peers. On the next run those stages would be reported as
    # "cached" and automation would publish zero cases. Force one full rebuild after
    # any failed run so the single-command workflow heals itself without -NoCache.
    $PreviousSummary = Join-Path $OutputDir 'release-summary.json'
    if (-not $NoCache -and (Test-Path -LiteralPath $PreviousSummary)) {
        try {
            $Previous = Get-Content -LiteralPath $PreviousSummary -Raw | ConvertFrom-Json
            if ($Previous.complete -eq $true -and $Previous.passed -ne $true) {
                Write-Host 'Cache    : disabled - previous Release failed, forcing a full rebuild.' -ForegroundColor DarkYellow
                $NoCache = $true
            }
        } catch { }
    }
    # A fixed directory has to be emptied: leftover files from the previous run would be
    # published as this run's evidence, and a stale summary is worse than none.
    if (Test-Path -LiteralPath $OutputDir) {
        # Delete through the .NET API rather than Remove-Item: some PowerShell hosts wrap
        # Remove-Item in a recycle-bin hook that refuses tool-owned generated files and
        # throws through -ErrorAction SilentlyContinue, which would abort the run before
        # its first stage. A leftover that cannot be removed is caught by the check below.
        foreach ($Entry in (Get-ChildItem -LiteralPath $OutputDir -Force)) {
            try {
                if ($Entry.PSIsContainer) { [IO.Directory]::Delete($Entry.FullName, $true) }
                else { [IO.File]::Delete($Entry.FullName) }
            } catch { }
        }
    }
}
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if ((Test-Path $OutputDir) -and @(Get-ChildItem -LiteralPath $OutputDir -Force).Count) {
    throw "Release output must be empty to exclude stale evidence: $OutputDir"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
Initialize-ReleaseCache -Directory (Join-Path $RepoRoot 'Saved/ReleaseCache') -Disabled:$NoCache
$Stages = [Collections.Generic.List[object]]::new()
$Statuses = @{}
$RequiredStages = @('tool-environment', 'legacy-layout-cleanup', 'repository-audit',
    'tooling-tests', 'analyzer-LuaLS', 'analyzer-LuauLsp', 'docs-site')
foreach ($BackendName in @('Lua', 'Luau')) {
    foreach ($Step in @('config', 'editor-build', 'static-bindings', 'package-development', 'server-build', 'package-server',
        'editor-tests', 'declarations', 'typecheck', 'editor-evidence', 'game-tests', 'game-evidence', 'reports', 'package-shipping')) {
        $RequiredStages += "$BackendName-$Step"
    }
    # The connected matrix is a Release stage now: every cell launches three peer
    # processes, which is why it used to be an opt-in second command. It is judged
    # inside RunNetMatrix.ps1 by ValidateRelease.py --net-matrix.
    $RequiredStages += "net-matrix-$BackendName"
}
$RequiredStages += @('evidence-index', 'release-provenance', 'promotion-gate', 'release-claims')
# Connected-matrix evidence lives under this run's directory, so a fresh Release
# can never publish topology.json files left behind by an older run.
$NetMatrixDir = Join-Path $OutputDir 'net-matrix'
$Started = [DateTime]::UtcNow
$script:CacheHits = 0
$LockStream = $null
$ConfigCaptured = $false
$SavedEnvironment = @{}
$EnvironmentNames = @('PATH', 'LUA_LANGUAGE_SERVER', 'LUAU_LSP', 'UNLUANOVA_RELEASE_TOKEN',
    'UNLUANOVA_TEST_MANIFEST', 'UNLUANOVA_TEST_PEER_EXE', 'UNLUANOVA_TEST_SERVER_EXE', 'UNLUANOVA_TEST_NET_ROOT', 'UNLUANOVA_RECORD_FRAMES')
foreach ($Name in $EnvironmentNames) { $SavedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process') }
$Provenance = [ordered]@{ platform = 'Win64'; backends = @('Lua', 'Luau');
    isolated_backends = $true; machine = $env:COMPUTERNAME }

function Test-ReleaseStageSucceeded([string] $Name) { $Statuses[$Name] -in @('passed', 'cached') }

function Resolve-ReleaseStageArtifacts([scriptblock] $Script, [string[]] $Paths) {
    # Named parameters on purpose: `&` with an array argument would splat it positionally
    # and only the first path would survive.
    if ($Script) { return @(& $Script | Where-Object { $_ }) }
    return @($Paths | Where-Object { $_ })
}

function Test-ReleaseCacheVerify([scriptblock] $Verify) {
    # A verification that cannot run (unreadable header, missing tree) is a miss, not a
    # failed release: the stage just runs again.
    try { return [bool](& $Verify) } catch { return $false }
}

function Invoke-Stage([string] $Name, [scriptblock] $Action, [string[]] $Needs = @(),
    # -CacheKey/-Fingerprint/-Artifacts: reuse what a previous run already produced for the
    # same inputs. -Verify decides whether the files on disk really are this stage's output
    # (each backend has its own tree, so another backend's build must not be reused as
    # this one's). -ReuseFile carries a small result (a type check summary) across runs, because
    # it lives inside the run directory and would otherwise never exist on a hit. -After
    # runs on passed *and* cached, for the bookkeeping a skipped stage still owes.
    [string] $CacheKey = '', [string] $Fingerprint = '', [string[]] $Artifacts = @(),
    # -ArtifactsOf is the same list as a scriptblock, for outputs whose path is only known
    # once they exist (a packaged executable is found by searching the staging directory).
    # It is evaluated before the lookup and again after the stage, so the recorded artifact
    # is the one this run produced.
    [scriptblock] $ArtifactsOf = $null,
    [scriptblock] $Verify = $null, [string] $ReuseFile = '', [scriptblock] $After = $null) {
    $Watch = [Diagnostics.Stopwatch]::StartNew()
    $State = 'passed'
    $Detail = ''
    $Missing = @($Needs | Where-Object { -not (Test-ReleaseStageSucceeded $_) })
    $Hit = if (-not $Missing.Count -and $CacheKey) {
        (Test-ReleaseCacheHit -Key $CacheKey -Fingerprint $Fingerprint `
            -Paths (Resolve-ReleaseStageArtifacts -Script $ArtifactsOf -Paths $Artifacts)) -and
        (-not $Verify -or (Test-ReleaseCacheVerify $Verify))
    } else { $false }
    Write-Host "[$Name]" -ForegroundColor Cyan
    if ($Missing.Count) { $State = 'blocked'; $Detail = 'Dependencies: ' + ($Missing -join ', ') }
    elseif ($Hit) {
        $State = 'cached'
        $Detail = 'Reused the previous run: inputs unchanged, artifacts verified.'
        $script:CacheHits++
        Write-Host "  cached - skipping ($CacheKey)" -ForegroundColor DarkGray
        if ($ReuseFile -and -not (Restore-ReleaseCacheFile -Key $CacheKey -Destination $ReuseFile)) {
            # The summary the evidence gate reads has to come from somewhere; without the
            # cached copy there is nothing to reuse, so the stage has to run after all.
            $State = 'passed'
            $script:CacheHits--
        }
    }
    if ($State -eq 'passed') {
        try {
            & $Action | Out-Host
            if ($CacheKey) {
                Set-ReleaseCacheEntry -Key $CacheKey -Fingerprint $Fingerprint -Paths (Resolve-ReleaseStageArtifacts -Script $ArtifactsOf -Paths $Artifacts)
                if ($ReuseFile) { Save-ReleaseCacheFile -Key $CacheKey -Source $ReuseFile }
            }
        } catch { $State = 'failed'; $Detail = $_.Exception.Message; Write-Warning $Detail }
    }
    if (($State -in @('passed', 'cached')) -and $After) {
        try { & $After | Out-Host }
        catch { $State = 'failed'; $Detail = $_.Exception.Message; Write-Warning $Detail }
    }
    $Statuses[$Name] = $State
    $Stages.Add([ordered]@{ name = $Name; status = $State; seconds = $Watch.Elapsed.TotalSeconds; detail = $Detail })
    Write-ReleaseSummary
}

function Get-ReleaseAutomationRows([string] $Root) {
    # The automation table lives in the markdown only, which makes it invisible to anything
    # that compares two runs. Read the same per-run summaries and return rows instead.
    $Rows = @()
    foreach ($BackendName in @('Lua', 'Luau')) {
        $Tag = $BackendName.ToLowerInvariant()
        foreach ($CarrierName in @('editor', 'game')) {
            $Relative = if ($CarrierName -eq 'editor') { "$Tag/editor/$Tag" } else { "$Tag/game" }
            $SummaryPath = Join-Path $Root "$Relative/summary.json"
            $Row = [ordered]@{ backend = $BackendName; carrier = $CarrierName }
            if (Test-Path -LiteralPath $SummaryPath) {
                try {
                    $RunSummary = Get-Content -LiteralPath $SummaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
                    $Row.total = [int]$RunSummary.total
                    $Row.passed = [int]$RunSummary.passed
                    $Row.failed = [int]$RunSummary.failed
                    $Row.skipped = [int]$RunSummary.skipped
                    $Row.screenshots = [int]$RunSummary.screenshots
                    $Row.report = "$Relative/report.md"
                } catch { $Row.error = 'Invalid summary' }
            } else { $Row.error = 'Not available' }
            $Rows += $Row
        }
    }
    return @($Rows)
}

function Compare-ReleaseBaseline([string] $Root, [object[]] $Automation) {
    # Two result directories exist so that "worse than last time" is a question with an
    # answer. Regressions are reported, not enforced: this run's own stages decide the
    # verdict, and a machine that legitimately ran fewer tests is not a failed release.
    $BaselineFile = Join-Path $Root 'release-summary.json'
    if (-not (Test-Path -LiteralPath $BaselineFile)) { return $null }
    try { $Baseline = Get-Content -LiteralPath $BaselineFile -Raw -Encoding UTF8 | ConvertFrom-Json } catch { return $null }

    $Regressions = [Collections.Generic.List[string]]::new()
    foreach ($Stage in $Stages) {
        $Before = @($Baseline.stages | Where-Object { $_.name -eq $Stage.name } | Select-Object -First 1)
        if (-not $Before) { continue }
        if ($Before[0].status -in @('passed', 'cached') -and $Stage.status -notin @('passed', 'cached')) {
            $Regressions.Add("$($Stage.name): $($Before[0].status) -> $($Stage.status)")
        }
    }
    $BeforeRows = if ($Baseline.automation) { @($Baseline.automation) } else { @() }
    foreach ($Row in $Automation) {
        $Previous = @($BeforeRows | Where-Object { $_.backend -eq $Row.backend -and $_.carrier -eq $Row.carrier } | Select-Object -First 1)
        if (-not $Previous -or $Previous[0].error -or $Row.error) { continue }
        if ([int]$Row.failed -gt [int]$Previous[0].failed) {
            $Regressions.Add("$($Row.backend)/$($Row.carrier): failed $($Previous[0].failed) -> $($Row.failed)")
        } elseif ([int]$Row.total -lt [int]$Previous[0].total) {
            $Regressions.Add("$($Row.backend)/$($Row.carrier): total $($Previous[0].total) -> $($Row.total)")
        }
    }
    return [ordered]@{ path = $BaselineFile; compared = $true;
        baseline_started_utc = $Baseline.started_utc; baseline_passed = $Baseline.passed;
        regressions = @($Regressions) }
}

function Write-ReleaseSummary {
    $Complete = $script:Finished -eq $true
    $Passed = $Complete -and $Stages.Count -gt 0 -and @($Stages | Where-Object { $_.status -notin @('passed', 'cached') }).Count -eq 0
    $Automation = Get-ReleaseAutomationRows $OutputDir
    $BaselineRoot = Join-Path $ReleaseRoot 'baseline'
    # This run *is* the baseline when it is writing into the baseline directory; otherwise
    # whatever is in there is what it gets compared against.
    $IsBaselineRun = [IO.Path]::GetFullPath($OutputDir) -eq [IO.Path]::GetFullPath($BaselineRoot)
    $Baseline = if ($IsBaselineRun) { $null } else { Compare-ReleaseBaseline $BaselineRoot $Automation }
    $Result = [ordered]@{ schema_version = 1; complete = $Complete; passed = $Passed;
        started_utc = $Started.ToString('o'); updated_utc = [DateTime]::UtcNow.ToString('o');
        output = [ordered]@{ directory = $OutputDir; kind = $(if ($IsBaselineRun) { 'baseline' } else { 'test' });
            baseline = $BaselineRoot };
        reuse = [ordered]@{ enabled = -not $NoCache; reused_stages = $script:CacheHits };
        scope = 'Win64 Lua+Luau: repository audit, tooling regressions, strict docs site, Editor and cooked Development automation, connected delivery matrix (8 scenarios + 4 variants per backend), Dedicated Server peers, Shipping packaging, evidence/provenance/promotion gates and published release claims';
        provenance = $Provenance; required_stages = @($RequiredStages);
        stages = @($Stages.ToArray()); automation = $Automation; baseline_comparison = $Baseline }
    $Result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputDir 'release-summary.json') -Encoding UTF8
    $Lines = @('# Release Validation', '', "Complete: $Complete. Passed: $Passed.", '',
        $Result.scope, '', "Result directory: ``$OutputDir`` ($($Result.output.kind)).",
        "Build reuse: $(if ($NoCache) { 'disabled (-NoCache)' } else { "$script:CacheHits stage(s) reused" }).", '')
    $Lines += @('| Stage | Status | Detail |', '| --- | --- | --- |')
    foreach ($Stage in $Stages) {
        $Detail = $Stage.detail.Replace('|', '/').Replace("`r", ' ').Replace("`n", ' ')
        $Lines += "| $($Stage.name) | $($Stage.status) | $Detail |"
    }
    $Lines += @('', '## Automation Reports', '',
        '| Backend | Carrier | Total | Passed | Failed | Skipped | Screenshots | Report |',
        '| --- | --- | --- | --- | --- | --- | --- | --- |')
    foreach ($Row in $Automation) {
        if ($Row.error) { $Lines += "| $($Row.backend) | $($Row.carrier) | - | - | - | - | - | $($Row.error) |" }
        else { $Lines += "| $($Row.backend) | $($Row.carrier) | $($Row.total) | $($Row.passed) | $($Row.failed) | $($Row.skipped) | $($Row.screenshots) | [Details]($($Row.report)) |" }
    }
    if ($Baseline) {
        $Lines += @('', '## Compared With Baseline', '', "Baseline: ``$($Baseline.path)`` (started $($Baseline.baseline_started_utc)).", '')
        if (@($Baseline.regressions).Count) {
            $Lines += '| Regression |', '| --- |'
            foreach ($Item in $Baseline.regressions) { $Lines += "| $Item |" }
        } else {
            $Lines += 'No regression: every stage that passed in the baseline still passes, and no carrier lost tests.'
        }
    } elseif (-not $IsBaselineRun) {
        $Lines += @('', '## Compared With Baseline', '', 'No baseline yet - run `.\Tools\ReleasePipeline.ps1 -Baseline` once to create one.')
    }
    $Lines += @('', 'The stage verdict also requires runtime manifests, memory and network evidence, type checks, and independent performance references. Test totals alone do not establish release acceptance.', '',
        'Source provenance and file hashes are recorded in [release-summary.json](release-summary.json).')
    $Lines | Set-Content -LiteralPath (Join-Path $OutputDir 'release-summary.md') -Encoding UTF8
}

function Run([string] $File, [string[]] $Arguments, [string] $Label, [int] $Minutes = $TimeoutMinutes) {
    Invoke-ReleaseProcess -File $File -Arguments $Arguments -Log (Join-Path $OutputDir "logs/$Label.log") -Minutes $Minutes
}
function Run-Script([string] $Script, [string[]] $Arguments, [string] $Label) {
    Run $PowerShellExe (@('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot $Script)) + $Arguments) $Label
}
function Get-ReleaseDeclarationDigest([string] $Root) {
    # One stable digest over both declaration dialects. The cache entry hashes a single
    # small manifest instead of tens of thousands of generated files, and -Verify still
    # detects a truncated, stale or hand-edited tree when the stage would be reused.
    $Normalized = [IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $Sha = [Security.Cryptography.SHA256]::Create()
    try {
        $Files = @(Get-ChildItem -LiteralPath $Normalized -Recurse -Filter '*.lua' -File -ErrorAction SilentlyContinue |
            Sort-Object -Property { $_.FullName.Substring($Normalized.Length) })
        foreach ($File in $Files) {
            $Relative = $File.FullName.Substring($Normalized.Length).TrimStart('\', '/').Replace('\', '/')
            $Header = [Text.Encoding]::UTF8.GetBytes("$Relative|$($File.Length)|")
            # Passing the input array as the output buffer keeps this valid on both
            # .NET Framework (PowerShell 5.1) and .NET (PowerShell 7).
            $null = $Sha.TransformBlock($Header, 0, $Header.Length, $Header, 0)
            $Bytes = [IO.File]::ReadAllBytes($File.FullName)
            $null = $Sha.TransformBlock($Bytes, 0, $Bytes.Length, $Bytes, 0)
        }
        $null = $Sha.TransformFinalBlock([byte[]]@(), 0, 0)
        return (([BitConverter]::ToString($Sha.Hash)).Replace('-', '').ToLowerInvariant() + " files=$($Files.Count)")
    } finally { $Sha.Dispose() }
}
function Build-Target([string] $Target, [string] $Label) {
    Run $BuildBat @($Target, 'Win64', 'Development', "-Project=$ProjectPath", '-WaitMutex') $Label
}
function Record-Binary([string] $Exe, [string] $Name) {
    $File = Get-Item -LiteralPath $Exe
    $Destination = Join-Path $BackendDir "metadata/$Name.json"
    # The directory is created here too: this runs on a cache hit as well, and the stage that
    # would otherwise have created it was the one that got skipped.
    New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
    [ordered]@{ path = $File.FullName; bytes = $File.Length;
        sha256 = (Get-FileHash -LiteralPath $Exe -Algorithm SHA256).Hash;
        backend = $B; recorded_utc = [DateTime]::UtcNow.ToString('o') } |
        ConvertTo-Json | Set-Content -LiteralPath $Destination -Encoding UTF8
}
function Package-Game([string] $Configuration, [string] $Label, [string] $StageDir, [string] $CookDir) {
    $UatArgs = @('BuildCookRun', "-project=$ProjectPath", '-noP4', '-unattended',
        '-platform=Win64', "-clientconfig=$Configuration", "-target=$script:ReleaseGameTarget", '-build',
        '-skipbuildeditor', '-cook', '-stage', '-pak', '-iostore', '-package')
    # UAT calls CleanStagingDirectory on every -stage, so two backends cannot share one
    # staging directory: the second package would delete the first one's .exe. Isolated
    # builds stage each backend into its own Backends/<Backend> subdirectory.
    if ($StageDir) { $UatArgs += "-stagingdirectory=$StageDir" }
    # CookCommandlet writes cooked assets into -CookOutputDir verbatim (it only appends
    # <CookPlatform> itself when the path contains a [Platform] token), while
    # CopyBuildToStagingDirectory APPENDS SC.CookPlatform unless the last path segment
    # already equals it. Pass the exact platform directory so both sides agree, and keep
    # it under Backends/<Backend>/<Configuration> so no two runs ever cook over each
    # other's Saved/Cooked/Windows* tree.
    if (-not $CookDir) { throw 'Package-Game requires an isolated cook directory.' }
    $UatArgs += "-CookOutputDir=$CookDir"
    Run $Uat $UatArgs $Label
    # Record-Binary is not called here but by the stage's -After block, so that a reused
    # (cached) package still records the binary it is shipping.
}

try {
    if (-not $ProjectPath) {
        $Projects = @(Get-ChildItem -LiteralPath $RepoRoot -Filter '*.uproject' -File)
        if ($Projects.Count -ne 1) { throw 'Expected exactly one .uproject in the repository.' }
        $ProjectPath = $Projects[0].FullName
    }
    $ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
    if ((Split-Path -Parent $ProjectPath) -ne $RepoRoot) { throw 'Release uses the repository demo project and its test assets.' }
    $ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProjectPath)
    $EnginePath = Find-ReleaseEngine $EnginePath $ProjectPath
    $Editor = Join-Path $EnginePath 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $BuildBat = Join-Path $EnginePath 'Engine/Build/BatchFiles/Build.bat'
    $Uat = Join-Path $EnginePath 'Engine/Build/BatchFiles/RunUAT.bat'
    foreach ($Required in @($Editor, $BuildBat, $Uat)) {
        if (-not (Test-Path -LiteralPath $Required)) { throw "Missing prerequisite: $Required" }
    }
    $LockPath = Join-Path $RepoRoot 'Saved/RunTests.lock'
    New-Item -ItemType Directory -Path (Split-Path -Parent $LockPath) -Force | Out-Null
    # Read sharing lets our child verify ownership; a second writer still cannot acquire it.
    $LockStream = [IO.File]::Open($LockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Read)
    $env:UNLUANOVA_RELEASE_TOKEN = [guid]::NewGuid().ToString('N')
    $Stamp = [Text.Encoding]::UTF8.GetBytes("pid=$PID token=$env:UNLUANOVA_RELEASE_TOKEN")
    $LockStream.SetLength(0)
    $LockStream.Write($Stamp, 0, $Stamp.Length)
    $LockStream.Flush()
    $IniPath = Join-Path $RepoRoot 'Config/DefaultUnLuaNova.ini'
    $IniExisted = Test-Path -LiteralPath $IniPath
    $IniBytes = if ($IniExisted) { [IO.File]::ReadAllBytes($IniPath) } else { $null }
    $ConfigCaptured = $true
    $Provenance.engine = Get-Content (Join-Path $EnginePath 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
    $Provenance.commit = (& git -C $RepoRoot rev-parse HEAD) -join ''
    $Provenance.worktree = @(& git -C $RepoRoot status --short)
    Run 'git' @('-C', $RepoRoot, 'diff', 'HEAD', '--binary', '--no-ext-diff') 'source-diff'
    $Provenance.source_diff_sha256 = (Get-FileHash (Join-Path $OutputDir 'logs/source-diff.log') -Algorithm SHA256).Hash
    $Provenance.untracked_files = @(
        foreach ($Relative in (& git -C $RepoRoot -c core.quotepath=false ls-files --others --exclude-standard -- Source Plugins Tools Config Docs)) {
            $Source = Join-Path $RepoRoot $Relative
            $Destination = Join-Path $OutputDir ('metadata/untracked-source/' + $Relative)
            New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
            Copy-Item -LiteralPath $Source -Destination $Destination
            [ordered]@{ path = $Relative; sha256 = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash }
        }
    )
    $Provenance.project_sha256 = (Get-FileHash -LiteralPath $ProjectPath -Algorithm SHA256).Hash

    Invoke-Stage 'tool-environment' {
        # UNLUANOVA_PYTHON wins over PATH: the common failure is a machine whose default
        # `python` is old, and reordering PATH to fix it is more invasive than naming one.
        $Python = if ($env:UNLUANOVA_PYTHON) { $env:UNLUANOVA_PYTHON }
                  else { (Get-Command python -ErrorAction Stop).Source }
        if (-not (Test-Path -LiteralPath $Python)) { throw "UNLUANOVA_PYTHON points at a missing file: $Python" }

        # Check the version before touching venv. `python -m venv` on 3.7-3.9 shells out to
        # `ensurepip --default-pip`, which does not exist there, so the stage dies with an
        # opaque subprocess error and every later stage reports "blocked". Say what is wrong.
        $PythonVersion = (& $Python -c 'import sys; print("%d.%d.%d" % sys.version_info[:3])' | Select-Object -Last 1)
        $PythonVersion = if ($PythonVersion) { $PythonVersion.Trim() } else { '' }
        if (-not ($PythonVersion -match '^(\d+)\.(\d+)\.')) { throw "Cannot read a Python version from $Python" }
        $Major = [int]$Matches[1]; $Minor = [int]$Matches[2]
        if ($Major -lt 3 -or ($Major -eq 3 -and $Minor -lt 10)) {
            throw "Python 3.10+ is required (venv's ensurepip --default-pip is 3.10+); found $PythonVersion at $Python. Install a newer Python, or set UNLUANOVA_PYTHON to one."
        }
        $Provenance.python = [ordered]@{ path = $Python; version = $PythonVersion }

        $Venv = Join-Path $RepoRoot 'Saved/ReleaseTools/python'
        $script:PythonExe = Join-Path $Venv 'Scripts/python.exe'
        # Creating a venv over an existing one is allowed but rewrites it; skipping is both
        # faster and one less thing that can break a working environment.
        if (-not (Test-Path -LiteralPath $PythonExe)) { Run $Python @('-m', 'venv', $Venv) 'python-venv' }
        # The interpreter's presence is not proof of a healthy venv. An interrupted
        # `python -m venv` leaves pyvenv.cfg and python.exe behind but stops before
        # ensurepip and the activation scripts; the executable-only check above then
        # skips recreation and `pip install` dies with "No module named pip", which fails
        # tool-environment and blocks every downstream stage. Probe pip and bootstrap it
        # from the interpreter's bundled wheel (offline) before trusting this venv.
        $VenvRepaired = $false
        & $PythonExe @('-B', '-m', 'pip', '--version') 2>$null | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Run $PythonExe @('-m', 'ensurepip', '--default-pip') 'python-ensurepip'
            & $PythonExe @('-B', '-m', 'pip', '--version') 2>$null | Out-Null
            if ($LASTEXITCODE -ne 0) {
                throw "pip is unavailable in $Venv even after ensurepip; delete Saved/ReleaseTools/python and rerun."
            }
            $VenvRepaired = $true
        }
        # pip resolves the whole dependency set on every call, and pinned or not that is a
        # network round trip per package. The venv itself is the cache: same interpreter and
        # same requirements means the installed set is already there.
        # Both requirement files are part of the fingerprint: the docs toolchain is
        # installed here because `mkdocs build --strict` is a Release stage, not a
        # separate command the maintainer has to remember.
        $DependencyFingerprint = ConvertTo-ReleaseFingerprint @("python=$PythonVersion",
            (Get-ReleaseTreeFingerprint -Roots @(
                (Join-Path $PSScriptRoot 'requirements.txt'),
                (Join-Path $RepoRoot 'requirements-docs.txt'))))
        $DependencyKey = 'python-dependencies'
        # A repaired venv has pip but not the third-party set, so a surviving cache entry
        # must not skip the install: cmake/ninja/mkdocs would be missing downstream.
        if (-not $VenvRepaired -and (Test-ReleaseCacheHit -Key $DependencyKey -Fingerprint $DependencyFingerprint -Paths @($PythonExe))) {
            Write-Host "  cached - python dependencies already installed ($DependencyKey)" -ForegroundColor DarkGray
            $script:CacheHits++
        } else {
            Run $PythonExe @('-m', 'pip', 'install',
                '-r', (Join-Path $PSScriptRoot 'requirements.txt'),
                '-r', (Join-Path $RepoRoot 'requirements-docs.txt'),
                'cmake==3.31.6', 'ninja==1.11.1.3') 'python-dependencies'
            Set-ReleaseCacheEntry -Key $DependencyKey -Fingerprint $DependencyFingerprint -Paths @($PythonExe)
        }
        $env:PATH = (Join-Path $Venv 'Scripts') + ';' + $env:PATH
        $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $VS = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $VS) { throw 'Visual Studio C++ tools are required by Unreal and the analyzers.' }
        $Provenance.compiler_installation = @(& $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json) -join "`n" | ConvertFrom-Json
        # Configure only the child shell; do not leak VS environment changes into the caller.
        $script:DevShell = Join-Path $VS 'Common7/Tools/Launch-VsDevShell.ps1'
        if (-not (Test-Path $DevShell)) { throw "Missing $DevShell" }
        Run 'git' @('-C', $RepoRoot, 'submodule', 'update', '--init', '--recursive') 'submodules'
        $Provenance.submodules = @(& git -C $RepoRoot submodule status --recursive)
    }
    # UE's FModuleManager falls back to scanning UnrealEditor-<Module>.dll when a
    # *.modules manifest is disabled. The backend-isolation refactor left the
    # ordinary-layout DLLs in the TOP-LEVEL Binaries/Win64/ (built from the old UCLASS
    # layout), while isolated targets write fresh DLLs under Backends/<Backend>/.
    # Both get registered in one process; because their UHT reload versions differ the
    # engine aborts with "Trying to recreate changed class ... outside of hot reload".
    # Archive the ordinary-layout products once - they are reproducible build
    # artifacts, this project only uses isolated targets, and nothing is deleted.
    Invoke-Stage 'legacy-layout-cleanup' {
        $ArchiveRoot = Join-Path $RepoRoot 'Saved/LegacyLayout'
        $Stamp = $Started.ToString('yyyyMMdd-HHmmss')
        $LayoutRoots = @('Binaries/Win64', 'Plugins/UnLuaNova/Binaries/Win64',
            'Plugins/UnLuaNovaTestSuite/Binaries/Win64')
        foreach ($Ext in @('LuaProtobuf', 'LuaRapidjson', 'LuaSocket')) {
            $LayoutRoots += "Plugins/UnLuaNovaExtensions/$Ext/Binaries/Win64"
        }
        $Moved = 0
        foreach ($Rel in $LayoutRoots) {
            $Root = Join-Path $RepoRoot $Rel
            if (-not (Test-Path -LiteralPath $Root)) { continue }
            # Top level only: Backends/<Backend>/ subdirectories hold the live DLLs.
            $Files = @(Get-ChildItem -LiteralPath $Root -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like 'UnrealEditor-*.dll' -or $_.Name -like '*.modules*' })
            if ($Files.Count -eq 0) { continue }
            $Destination = Join-Path $ArchiveRoot "$Stamp/$Rel"
            New-Item -ItemType Directory -Path $Destination -Force | Out-Null
            foreach ($F in $Files) {
                $Target = Join-Path $Destination $F.Name
                if (Test-Path -LiteralPath $Target) { [System.IO.File]::Delete($Target) }
                [System.IO.File]::Move($F.FullName, $Target)
                $Moved++
            }
        }
        Write-Host "Archived $Moved ordinary-layout file(s) under $ArchiveRoot/$Stamp"
    } @('tool-environment')
    Invoke-Stage 'repository-audit' { Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'AuditRepository.py')) 'audit' } @('tool-environment')
    # Strict docs build used to be a separate command (and the only GitHub workflow
    # job). It belongs in this run: a broken nav or a dangling link is a release
    # regression, not a docs-only chore. -f makes it independent of the caller's cwd,
    # and the site lands in this run's directory so stale output cannot be published.
    Invoke-Stage 'docs-site' {
        # A cached python-dependencies entry can outlive a venv that was recreated,
        # partially cleaned, or written by an older fingerprint. Probe the toolchain
        # instead of failing a long Release on a missing module: install only when
        # mkdocs-material is genuinely absent.
        & $PythonExe @('-B', '-c', 'import mkdocs, material') 2>$null | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Run $PythonExe @('-m', 'pip', 'install', '-r',
                (Join-Path $RepoRoot 'requirements-docs.txt')) 'docs-dependencies'
        }
        Run $PythonExe @('-B', '-m', 'mkdocs', 'build', '--strict',
            '-f', (Join-Path $RepoRoot 'mkdocs.yml'),
            '--site-dir', (Join-Path $OutputDir 'site')) 'docs-site'
    } @('tool-environment')
    Invoke-Stage 'tooling-tests' {
        Run $PythonExe @('-B', '-m', 'unittest', 'discover', '-s', (Join-Path $PSScriptRoot 'tests'), '-p', 'test_*.py') 'python-tests'
        Run-Script 'tests/TestReleaseSupport.ps1' @() 'powershell-tests'
    } @('tool-environment')
    $LspBuild = Join-Path $RepoRoot 'Saved/ReleaseTools/luau-lsp-ninja'
    foreach ($Tool in @('LuaLS', 'LuauLsp')) {
        # Both analyzers are compiled from their submodule, and -Force used to rebuild them
        # on every release run - several minutes of C++ for a tool whose source has not
        # moved. The submodule commits and the build script decide the binary, so they are
        # the fingerprint.
        $ToolDir = Join-Path $PSScriptRoot $(if ($Tool -eq 'LuaLS') { 'lua-language-server' } else { 'luau-lsp' })
        $BuildScript = Join-Path $PSScriptRoot "Build-$Tool.ps1"
        $ToolExe = if ($Tool -eq 'LuaLS') { Join-Path $ToolDir 'bin/lua-language-server.exe' } else {
            $Found = Get-ChildItem -LiteralPath $LspBuild -Recurse -Filter 'luau-lsp.exe' -File -ErrorAction SilentlyContinue |
                Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
            if ($Found) { $Found.FullName } else { '' }
        }
        $ToolFingerprint = ConvertTo-ReleaseFingerprint @("tool=$Tool",
            "head=$((& git -C $ToolDir rev-parse HEAD) -join '')",
            "submodules=$((@(& git -C $ToolDir submodule status --recursive) -join "`n"))",
            (Get-ReleaseTreeFingerprint -Roots @($BuildScript)))
        Invoke-Stage "analyzer-$Tool" {
            $ToolArguments = if ($Tool -eq 'LuauLsp') {
                " -Generator Ninja -BuildDir '" + $LspBuild.Replace("'", "''") + "'"
            } else { '' }
            $Command = "& '" + $DevShell.Replace("'", "''") + "' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation; & '" +
                $BuildScript.Replace("'", "''") + "' -Force -NoSubmodules" + $ToolArguments + "; exit `$LASTEXITCODE"
            Run $PowerShellExe @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-EncodedCommand',
                [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($Command))) "build-$Tool"
        } -Needs @('tool-environment') -CacheKey "analyzer-$Tool" -Fingerprint $ToolFingerprint -Artifacts @($ToolExe)
    }
    $env:LUA_LANGUAGE_SERVER = Join-Path $PSScriptRoot 'lua-language-server/bin/lua-language-server.exe'
    $Lsp = Get-ChildItem $LspBuild -Recurse -Filter 'luau-lsp.exe' -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($Lsp) { $env:LUAU_LSP = $Lsp.FullName }

    # --- what decides whether a stage has to run again -------------------------------
    # Source, assets and project config, hashed once per run. Generated output is excluded
    # (it is what the stages produce), which is what keeps "the same tree" stable across
    # runs instead of differing because the last run left files behind.
    $EngineStamp = (Get-FileHash -LiteralPath (Join-Path $EnginePath 'Engine/Build/Build.version') -Algorithm SHA256).Hash
    # IntelliSense is generated by the declarations stage, so it is excluded here and
    # fingerprinted on its own for the type check that reads it.
    $CodeTree = Get-ReleaseTreeFingerprint -Roots @((Join-Path $RepoRoot 'Source'), (Join-Path $RepoRoot 'Plugins'),
        (Join-Path $RepoRoot 'Config'), $ProjectPath) -ExcludeDirectories @('Binaries', 'Intermediate', 'Generated', 'Saved', '.git', 'Content', 'IntelliSense')
    $ContentRoots = @((Join-Path $RepoRoot 'Content')) + @(Get-ChildItem -LiteralPath (Join-Path $RepoRoot 'Plugins') -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Join-Path $_.FullName 'Content' } | Where-Object { Test-Path -LiteralPath $_ })
    $ContentTree = Get-ReleaseTreeFingerprint -Roots $ContentRoots
    # Lua and Luau scripts are assets, so a script change must not rebuild the C++ - but it
    # has to invalidate the cook and the type check.
    $ScriptTree = Get-ReleaseTreeFingerprint -Roots @((Join-Path $RepoRoot 'Content/Script'))
    $DeclarationRoot = Join-Path $RepoRoot 'Plugins/UnLuaNova/IntelliSense'
    $DeclarationFile = Join-Path $DeclarationRoot 'Luau/UnLuaNova.d.lua'
    # Written by the declarations stage; hashing this one file makes both dialects part
    # of the cache decision without recording 20k generated .lua files in index.json.
    $DeclarationManifest = Join-Path $DeclarationRoot 'declarations.sha256'
    $DeclarationFingerprint = ConvertTo-ReleaseFingerprint @("engine=$EngineStamp", "code=$CodeTree", "content=$ContentTree")

    # --- backend order ---------------------------------------------------------------
    # Both backends live in separate Backends/<Backend> trees, so there is no single slot
    # to preserve: a fixed order is fine and neither build invalidates the other.
    $PluginRoot = Join-Path $RepoRoot 'Plugins/UnLuaNova'
    $BackendOrder = @('Lua', 'Luau')
    Write-Host "Backend order : $($BackendOrder -join ' -> ') - isolated Backends/<Backend> trees, no single-slot reuse order"

    foreach ($B in $BackendOrder) {
        $EditorTarget = "UnLuaNovaDemo${B}Editor"
        $GameTarget = "UnLuaNovaDemo${B}"
        $ServerTarget = "UnLuaNovaDemo${B}Server"
        # Package-Game / package-server run in their own function scopes, so publish the
        # names instead of relying on dynamic scope lookup.
        $script:ReleaseGameTarget = $GameTarget
        $script:ReleaseServerTarget = $ServerTarget
        $script:ReleaseDefinitionsSubFolder = "Backends/$B"
        Write-Host "Targets  : $B -> $EditorTarget / $GameTarget / $ServerTarget"
        $script:EditorStarted = $null
        $Lower = $B.ToLowerInvariant()
        $BackendDir = Join-Path $OutputDir $Lower
        $EditorDir = Join-Path $BackendDir "editor/$Lower"
        $GameDir = Join-Path $BackendDir 'game'
        $DevStage = Join-Path $RepoRoot "Saved/StagedBuilds/Windows/Backends/$B"
        $ServerStage = Join-Path $RepoRoot "Saved/StagedBuilds/WindowsServer/Backends/$B"
        $StageArg = $DevStage
        $ServerStageArg = $ServerStage
        # Cook outputs are isolated on backend AND configuration/carrier. The leaf name
        # has to be exactly UAT's SC.CookPlatform ("Windows" / "WindowsServer"): the
        # cooker writes into this directory verbatim and the stager reads it verbatim.
        $script:ReleaseCookRoot = Join-Path $RepoRoot "Saved/Cooked/Backends/$B"
        $DevCookDir = Join-Path $script:ReleaseCookRoot 'Development/Windows'
        $ShippingCookDir = Join-Path $script:ReleaseCookRoot 'Shipping/Windows'
        $ServerCookDir = Join-Path $script:ReleaseCookRoot 'Development/WindowsServer'
        # UAT calls CleanStagingDirectory on every -stage. Development and Shipping used to
        # share $DevStage, so the LAST Release stage deleted the Development client exe that
        # binary-Development.json records and that the Cooked connected cell must launch.
        # Keep the two configurations in separate roots: a completed Release then leaves a
        # usable Development staged peer behind instead of only the Shipping binary.
        $ShippingStage = Join-Path $RepoRoot "Saved/StagedBuilds/WindowsShipping/Backends/$B"
        # metadata/ is created here and not by the stages that write into it: those stages can
        # be skipped as cache hits, and their -After bookkeeping (binary provenance) still
        # runs - a directory that only exists on the "ran" path turns a reused package into a
        # failed stage.
        New-Item -ItemType Directory -Path $EditorDir, $GameDir, (Join-Path $BackendDir 'metadata') -Force | Out-Null
        Invoke-Stage "$B-config" {
            Set-ReleaseIniValue $IniPath '[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]' 'LuaBackend' $B
            foreach ($Feature in @('bTimer', 'bEvent', 'bOOP')) {
                Set-ReleaseIniValue $IniPath '[/Script/UnLuaNova.UnLuaNovaSettings]' $Feature 'True'
            }
            Set-ReleaseIniValue $IniPath '[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]' 'bStaticBinding' 'True'
            Set-ReleaseIniValue $IniPath '[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]' 'bAutoGenerateStaticBinding' 'False'
        }
        # The ini is fingerprinted only after the config stage wrote it: it is restored to
        # its original bytes when the run ends, so reading it earlier would hash the wrong
        # file and every run would look like a different build.
        $IniStamp = if (Test-Path -LiteralPath $IniPath) { (Get-FileHash -LiteralPath $IniPath -Algorithm SHA256).Hash } else { 'absent' }
        # The isolated targets produce different DLL/exe names and different stage paths,
        # so their caches must never be interchangeable with an ordinary run's cache.
        $BuildFingerprint = ConvertTo-ReleaseFingerprint @("backend=$B", "isolated=True",
            "targets=$EditorTarget/$GameTarget/$ServerTarget", 'platform=Win64',
            "engine=$EngineStamp", "ini=$IniStamp", "code=$CodeTree", "binding=isolated")
        # Packaging also cooks assets, so it moves when content moves, not only when code does.
        $PackageFingerprint = ConvertTo-ReleaseFingerprint @($BuildFingerprint, "content=$ContentTree", "cook=isolated")
        $BinariesRel = "Binaries/Win64/Backends/$B"
        $DefinitionsRel = 'Intermediate/Build/Win64/x64/UnrealEditor/Development'
        if ($script:ReleaseDefinitionsSubFolder) { $DefinitionsRel = "$DefinitionsRel/$script:ReleaseDefinitionsSubFolder" }
        $BackendDll = Join-Path $PluginRoot "$BinariesRel/UnrealEditor-$B.dll"
        $DefinitionsHeader = Join-Path $PluginRoot "$DefinitionsRel/UnLuaNova/Definitions.UnLuaNova.h"
        $BindingSource = Get-ReleaseStaticBindingSource $ProjectPath $B
        $ServerExe = Join-Path $RepoRoot "Binaries/Win64/$ServerTarget.exe"

        Invoke-Stage "$B-editor-build" {
            Build-Target $EditorTarget "$B-editor-build"
        } -Needs @('tool-environment', "$B-config") -CacheKey "$B-editor-build" -Fingerprint $BuildFingerprint `
            -Artifacts @($BackendDll, $DefinitionsHeader) `
            -Verify { (Get-ReleaseBuiltBackend $PluginRoot $script:ReleaseDefinitionsSubFolder) -eq $B } `
            -After {
                # Activation has to follow the build: SwitchBackend.ps1 fails closed unless
                # the VM DLL and the manifest UBT just wrote are present. -After also runs
                # on a cache hit, so a reused build still activates the right backend before
                # the static binding generator / editor commandlets start loading modules.
                Run-Script 'SwitchBackend.ps1' @('-Backend', $B, '-ProjectPath', $ProjectPath, '-Platform', 'Win64') "$B-activate-backend"
            }
        Invoke-Stage "$B-static-bindings" {
            # The generator writes only into this backend's Generated/Backends/<Backend>/,
            # so there is no shared source to archive, stub or restore. The other backend's
            # file stays untouched and is compiled as an empty translation unit (its whole
            # body is guarded by UNLUANOVA_LUA_BACKEND).
            Run $Editor @($ProjectPath, '-run=UnLuaNovaStaticBindingGenerator', '-unattended', '-NoSplash', '-stdout') "$B-generate-bindings"
            if (-not (Test-ReleaseBindingGenerated -Path $BindingSource -Backend $B)) {
                throw "Static binding generator produced no source for $B : $BindingSource"
            }
            Build-Target $EditorTarget "$B-rebuild-bindings"
            Build-Target 'ShaderCompileWorker' "$B-shader-worker-build"
        } -Needs @("$B-editor-build") -CacheKey "$B-static-bindings" -Fingerprint $BuildFingerprint `
            -Artifacts @($BindingSource) -Verify {
            # Reuse only a file the generator owns: a hand-edited, truncated or still-bootstrapped
            # binding source would otherwise be compiled into a "cached" build.
            Test-ReleaseBindingGenerated -Path $BindingSource -Backend $B
        }
        Invoke-Stage "$B-package-development" { Package-Game 'Development' "$B-package-development" $StageArg $DevCookDir } -Needs @("$B-static-bindings") `
            -CacheKey "$B-package-development" -Fingerprint $PackageFingerprint `
            -ArtifactsOf { Resolve-ReleaseGameExe $DevStage $script:ReleaseGameTarget 'Development' } `
            -After { Record-Binary (Get-ReleaseGameExe $DevStage $script:ReleaseGameTarget 'Development') 'binary-Development' }
        Invoke-Stage "$B-server-build" { Build-Target $ServerTarget "$B-server-build" } -Needs @("$B-static-bindings") `
            -CacheKey "$B-server-build" -Fingerprint $BuildFingerprint -Artifacts @($ServerExe)
        Invoke-Stage "$B-package-server" {
            $UatServerArgs = @('BuildCookRun', "-project=$ProjectPath", '-noP4', '-unattended',
                '-server', '-noclient', '-serverplatform=Win64', '-serverconfig=Development',
                "-target=$script:ReleaseServerTarget", '-build', '-skipbuildeditor', '-cook', '-stage', '-pak', '-iostore', '-package')
            if ($ServerStageArg) { $UatServerArgs += "-stagingdirectory=$ServerStageArg" }
            $UatServerArgs += "-CookOutputDir=$ServerCookDir"
            Run $Uat $UatServerArgs "$B-package-server"
        } -Needs @("$B-server-build") -CacheKey "$B-package-server" -Fingerprint $PackageFingerprint `
            -ArtifactsOf { Resolve-ReleaseGameExe $ServerStage $script:ReleaseServerTarget 'Development' } `
            -After {
                # The editor tests launch these peers, so the variable has to be set whether
                # the package was rebuilt or reused.
                $env:UNLUANOVA_TEST_SERVER_EXE = Get-ReleaseGameExe $ServerStage $script:ReleaseServerTarget 'Development'
                Record-Binary $env:UNLUANOVA_TEST_SERVER_EXE 'binary-Server'
            }
        Invoke-Stage "$B-editor-tests" {
            $env:UNLUANOVA_TEST_PEER_EXE = Get-ReleaseGameExe $DevStage $script:ReleaseGameTarget 'Development'
            $env:UNLUANOVA_TEST_NET_ROOT = Join-Path $EditorDir 'net'
            $env:UNLUANOVA_TEST_MANIFEST = Join-Path $EditorDir 'metadata/manifest.json'
            $script:EditorStarted = [DateTime]::UtcNow
            $env:UNLUANOVA_RECORD_FRAMES = '0'
            try {
                $TestArgs = @('-ReleaseManaged', '-EnginePath', $EnginePath,
                    '-ProjectPath', $ProjectPath, '-Backend', $B, '-Carrier', 'Editor', '-SkipBuild', '-RenderOffScreen',
                    '-OutputDir', (Join-Path $BackendDir 'editor'), '-TimeoutMinutes', "$TimeoutMinutes")
                # The Release is the one place where a changed test inventory must not be
                # a warning: "all green" after silently dropping half the cases is the
                # failure mode the baseline comparison exists to catch.
                $TestArgs += '-FailOnDrift'
                Run-Script 'RunDevelopmentTests.ps1' $TestArgs "$B-editor-tests"
                Assert-ReleaseAutomationLog (Join-Path $OutputDir "logs/$B-editor-tests.log")
            } finally { Stop-ReleasePeers $env:UNLUANOVA_TEST_NET_ROOT $EditorStarted }
        } @("$B-package-development", "$B-package-server")
        Invoke-Stage "$B-declarations" {
            $script:DeclarationStarted = [DateTime]::UtcNow
            $DeclRoot = $DeclarationRoot
            $BackupRoot = Join-Path $BackendDir 'previous-declarations'
            New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
            $Moved = [Collections.Generic.List[object]]::new()
            try {
                foreach ($Dialect in @('Luau', 'EmmyLua')) {
                    $Source = [IO.Path]::GetFullPath((Join-Path $DeclRoot $Dialect))
                    $Destination = [IO.Path]::GetFullPath((Join-Path $BackupRoot $Dialect))
                    if (-not $Source.StartsWith($RepoRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
                        -not $Destination.StartsWith($OutputDir + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                        throw 'Declaration backup paths escaped their expected roots.'
                    }
                    if (Test-Path -LiteralPath $Source) {
                        Move-Item -LiteralPath $Source -Destination $Destination -ErrorAction Stop
                        $Moved.Add([pscustomobject]@{ Source = $Source; Destination = $Destination })
                    }
                }
                Run $Editor @($ProjectPath, '-run=UnLuaNovaIntelliSense', '-BP=1', '-unattended', '-NoSplash', '-stdout') "$B-declarations"
                $Files = @((Get-Item "$DeclRoot/Luau/UnLuaNova.d.lua")) + @(Get-ChildItem "$DeclRoot/EmmyLua" -Recurse -Filter '*.lua' -File)
                if ($Files.Count -lt 2 -or @($Files | Where-Object { $_.Length -eq 0 }).Count) {
                    throw 'Declaration export is empty or contains stale files.'
                }
                [IO.File]::WriteAllText($DeclarationManifest,
                    (Get-ReleaseDeclarationDigest $DeclRoot), [Text.UTF8Encoding]::new($false))
            } catch {
                # A failed export used to leave the shared IntelliSense tree emptied by the
                # Move-Item above, with no restore anywhere in the pipeline's finally block.
                # The commandlet can also leave a partial tree behind, so drop that before
                # moving the archived dialect back; otherwise Move-Item nests inside it.
                foreach ($Entry in $Moved) {
                    try {
                        if (Test-Path -LiteralPath $Entry.Source) {
                            [IO.Directory]::Delete($Entry.Source, $true)
                        }
                        if (Test-Path -LiteralPath $Entry.Destination) {
                            Move-Item -LiteralPath $Entry.Destination -Destination $Entry.Source -ErrorAction Stop
                        }
                    } catch {
                        Write-Warning "Declaration rollback failed for $($Entry.Source): $($_.Exception.Message)"
                    }
                }
                throw
            }
            # Backend-independent: the commandlet writes both dialects, so the second
            # backend used to pay another editor launch for files it already had.
        } -Needs @("$B-static-bindings") -CacheKey 'declarations-both' -Fingerprint $DeclarationFingerprint `
            -Artifacts @($DeclarationFile, $DeclarationManifest) -Verify {
            (Test-Path -LiteralPath $DeclarationManifest -PathType Leaf) -and
            ([IO.File]::ReadAllText($DeclarationManifest) -ceq (Get-ReleaseDeclarationDigest $DeclarationRoot))
        }
        $Analyzer = if ($B -eq 'Lua') { 'LuaLS' } else { 'LuauLsp' }
        $AnalyzerExe = if ($B -eq 'Lua') { $env:LUA_LANGUAGE_SERVER } else { $env:LUAU_LSP }
        $CheckSummary = Join-Path $EditorDir $(if ($B -eq 'Lua') { 'lua-analyze.json' } else { 'luau-analyze.json' })
        $CheckFingerprint = ConvertTo-ReleaseFingerprint @("backend=$B",
            "analyzer=$(if ($AnalyzerExe -and (Test-Path -LiteralPath $AnalyzerExe)) { Get-ReleaseFileSha256 $AnalyzerExe } else { 'absent' })",
            # Metadata for the declarations: they are tens of thousands of generated files,
            # and an export that rewrites them always changes their modification times, so
            # path + size + time already answers "were they re-exported?".
            "scripts=$ScriptTree", "declarations=$(Get-ReleaseTreeFingerprint -Roots @($DeclarationRoot) -MetadataOnly)")
        Invoke-Stage "$B-typecheck" {
            if ($B -eq 'Lua') {
                Run-Script 'CheckLuaTypes.ps1' @('-Require', '-LuaLS', $env:LUA_LANGUAGE_SERVER, '-SummaryPath', $CheckSummary) "$B-typecheck"
            } else {
                Run-Script 'CheckLuauTypes.ps1' @('-Require', '-LuauLsp', $env:LUAU_LSP, '-Path', 'Content/Script/Luau', '-Mode', 'strict', '-SummaryPath', $CheckSummary) "$B-typecheck"
            }
            # The summary is written into this run's directory, so a cache hit has to put it
            # back: the evidence gate reads it and "no file" is a failed gate, not a skip.
        } -Needs @("$B-declarations", "analyzer-$Analyzer") -CacheKey "$B-typecheck" -Fingerprint $CheckFingerprint -ReuseFile $CheckSummary
        Invoke-Stage "$B-editor-evidence" {
            $Perf = Join-Path $EditorDir 'metadata/perf.json'
            if (-not $EditorStarted -or -not (Test-Path $Perf) -or (Get-Item $Perf).LastWriteTimeUtc -lt $EditorStarted) {
                throw 'Missing fresh benchmark artifact.'
            }
            Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'ValidateRelease.py'), '--run', $EditorDir, '--backend', $B, '--carrier', 'editor') "$B-editor-evidence"
        } @('tool-environment', "$B-editor-tests")
        Invoke-Stage "$B-game-tests" {
            $Exe = Get-ReleaseGameExe $DevStage $script:ReleaseGameTarget 'Development'
            $env:UNLUANOVA_TEST_PEER_EXE = $Exe
            $env:UNLUANOVA_TEST_NET_ROOT = Join-Path $GameDir 'net'
            $env:UNLUANOVA_TEST_MANIFEST = Join-Path $GameDir 'metadata/manifest.json'
            $GameStarted = [DateTime]::UtcNow
            try {
                Run $Exe @('/UnLuaNovaTestSuite/Tests/Misc/Empty', '-game', '-unattended', '-NoSplash', '-nosound', '-stdout', '-RenderOffScreen',
                    '-ExecCmds=UnLuaNovaTest.ExportManifest, Automation RunTests UnLuaNova; Quit',
                    "-ReportExportPath=$GameDir", "-abslog=$GameDir/Game.log") "$B-game-tests"
                Assert-ReleaseAutomationLog (Join-Path $OutputDir "logs/$B-game-tests.log")
            } finally {
                Stop-ReleasePeers $env:UNLUANOVA_TEST_NET_ROOT $GameStarted
                $PackagedProject = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $Exe) '../..'))
                $ShotDir = Join-Path $PackagedProject 'Saved/Screenshots/Windows'
                $Shots = @(Get-ChildItem -LiteralPath $ShotDir -Filter '*.png' -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.LastWriteTimeUtc -ge $GameStarted })
                if ($Shots.Count) {
                    $ShotDestination = Join-Path $GameDir 'screenshots'
                    New-Item -ItemType Directory -Path $ShotDestination -Force | Out-Null
                    $Shots | Copy-Item -Destination $ShotDestination -Force
                }
            }
        } @("$B-package-development", "$B-package-server")
        Invoke-Stage "$B-game-evidence" {
            Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'ValidateRelease.py'), '--run', $GameDir, '--backend', $B, '--carrier', 'game') "$B-game-evidence"
        } @('tool-environment', "$B-game-tests")
        Invoke-Stage "$B-reports" {
            foreach ($Dir in @($EditorDir, $GameDir)) {
                # Editor and cooked carriers run different case inventories, so each has
                # its own committed baseline. Both are checked for drift here; before this
                # the game carrier had no case-set guardrail at all.
                $CarrierBaseline = if ($Dir -eq $GameDir) {
                    Join-Path $RepoRoot "Tools/baseline/$($B.ToLower())-game.json"
                } else {
                    Join-Path $RepoRoot "Tools/baseline/$($B.ToLower()).json"
                }
                Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'GenerateTestReport.py'), '--reports', $Dir, '--out', $Dir,
                    '--backend', $B, '--baseline', $CarrierBaseline, '--fail-on-drift') "$B-report-$(Split-Path $Dir -Leaf)"
            }
        } @('tool-environment')
        Invoke-Stage "$B-package-shipping" {
            Package-Game 'Shipping' "$B-package-shipping" $ShippingStage $ShippingCookDir
        } -Needs @("$B-static-bindings") -CacheKey "$B-package-shipping" -Fingerprint $PackageFingerprint `
            -ArtifactsOf { Resolve-ReleaseGameExe $ShippingStage $script:ReleaseGameTarget 'Shipping' } `
            -After { Record-Binary (Get-ReleaseGameExe $ShippingStage $script:ReleaseGameTarget 'Shipping') 'binary-Shipping' }
    }

    # The connected delivery matrix used to be a separate opt-in command. It is a
    # Release stage now: eight scenarios + four variants per backend, three peer
    # processes per cell, judged inside RunNetMatrix.ps1 by
    # ValidateRelease.py --net-matrix. Both backends' isolated Game/Server targets
    # were built above, so -SkipBuild never recompiles them.
    foreach ($B in @('Lua', 'Luau')) {
        Invoke-Stage "net-matrix-$B" {
            $NetArgs = @('-NetMatrix', '-EnginePath', $EnginePath, '-ProjectPath', $ProjectPath,
                '-Backend', $B, '-SkipBuild', '-OutputDir', $NetMatrixDir,
                '-TimeoutMinutes', "$TimeoutMinutes")
            Run-Script 'RunDevelopmentTests.ps1' $NetArgs "net-matrix-$B"
        } @('tool-environment', 'Lua-package-development', 'Lua-package-server',
            'Luau-package-development', 'Luau-package-server')
    }

    # Record execution evidence for the four carriers. This closes the gap between
    # "the pipeline collected evidence" and "the provenance gate can prove this run
    # belongs to HEAD": ValidateProvenance.py --require-current needs at least one
    # non-historical record in Tools/evidence/index.json for the current commit.
    Invoke-Stage 'evidence-index' {
        foreach ($EvidenceBackend in @('Lua', 'Luau')) {
            $EvidenceLower = $EvidenceBackend.ToLowerInvariant()
            $Reports = @{
                Editor = Join-Path $OutputDir "$EvidenceLower/editor/$EvidenceLower"
                Game   = Join-Path $OutputDir "$EvidenceLower/game"
            }
            foreach ($Carrier in @('Editor', 'Game')) {
                $Label = "evidence-$EvidenceLower-$($Carrier.ToLowerInvariant())"
                Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'RecordEvidence.py'),
                    '--report', $Reports[$Carrier], '--backend', $EvidenceBackend,
                    '--carrier', $Carrier, '--run-id', "release-$EvidenceLower-$($Carrier.ToLowerInvariant())") $Label
            }
        }
    } @('tool-environment', 'Lua-editor-evidence', 'Lua-game-evidence',
        'Luau-editor-evidence', 'Luau-game-evidence')
} catch {
    $Stages.Add([ordered]@{ name = 'pipeline'; status = 'failed'; seconds = 0; detail = $_.Exception.Message })
    Write-Warning $_.Exception.Message
} finally {
    # Stop any build still running before releasing the run lock: orphaned Build.bat/UBT/cl
    # processes hold UBT's mutex, and the next run would wait on it for most of its timeout.
    Stop-ReleaseChildProcesses
    if ($ConfigCaptured) {
        # The run leaves one backend's isolated manifests live. Its ini is restored to the
        # original bytes below, so the manifest layout has to go back to the ordinary
        # single-target one too - otherwise the next doctor run sees an active backend that
        # disagrees with LuaBackend. SwitchBackend -Backend Default never writes the ini.
        try {
            Run-Script 'SwitchBackend.ps1' @('-Backend', 'Default', '-ProjectPath', $ProjectPath, '-Platform', 'Win64') 'restore-backend-layout'
        } catch {
            $Stages.Add([ordered]@{ name = 'restore-backend-layout'; status = 'failed'; seconds = 0; detail = $_.Exception.Message })
        }
    }
    try {
        if ($ConfigCaptured) {
            if ($IniExisted) { [IO.File]::WriteAllBytes($IniPath, $IniBytes) }
            elseif (Test-Path -LiteralPath $IniPath) { [IO.File]::Delete($IniPath) }
        }
        foreach ($Name in $EnvironmentNames) { [Environment]::SetEnvironmentVariable($Name, $SavedEnvironment[$Name], 'Process') }
    } catch {
        $Stages.Add([ordered]@{ name = 'restore'; status = 'failed'; seconds = 0; detail = $_.Exception.Message })
    } finally {
        if ($LockStream) {
            $LockStream.Dispose()
            try { [IO.File]::Delete($LockPath) } catch { }
        }
    }
    # A stage that never ran is a blocked release stage, not a silently omitted
    # check. The three final gates run below, so they must not be pre-marked here.
    $FinalStageNames = @('release-provenance', 'promotion-gate', 'release-claims')
    foreach ($Name in $RequiredStages) {
        if ($Name -notin $FinalStageNames -and -not $Statuses.ContainsKey($Name)) {
            $Stages.Add([ordered]@{ name = $Name; status = 'blocked'; seconds = 0; detail = 'Pipeline ended before this stage.' })
        }
    }
    $script:Finished = $true

    # The notices-compliance stage was removed on 2026-11-13 by maintainer decision:
    # external legal provenance questions are out of scope for this repository's
    # release gate. Tools/GenerateNotices.py remains available as a standalone
    # manifest generator; it is not a Release stage.
    Write-ReleaseSummary

    # These gates run AFTER a complete summary exists: ValidateProvenance needs the
    # four gates this run just wrote, and CheckPromotion --release-gate needs
    # complete=true. They are ordinary Invoke-Stage stages, so a failure is recorded
    # in release-summary.json - the old hand-written release-claims block only
    # appended a failed stage without rewriting the summary, a real hole in the verdict.
    Invoke-Stage 'release-provenance' {
        Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'ValidateProvenance.py'),
            '--run', $OutputDir, '--require-current',
            '--report', (Join-Path $OutputDir 'metadata/provenance-gate.json')) 'release-provenance'
    } @('tool-environment', 'evidence-index', 'Lua-editor-evidence', 'Lua-game-evidence',
        'Luau-editor-evidence', 'Luau-game-evidence')

    Invoke-Stage 'promotion-gate' {
        Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'CheckPromotion.py'),
            '--release-gate', '--strict', '--run', $OutputDir,
            '--net-root', $NetMatrixDir) 'promotion-gate'
        # Full L0-L4 report, including the two human decisions (published marker
        # and release tag), printed for the maintainer without gating the Release.
        Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'CheckPromotion.py'),
            '--run', $OutputDir, '--net-root', $NetMatrixDir) 'promotion-report'
    } @('release-provenance', 'net-matrix-Lua', 'net-matrix-Luau')

    # The eight published release-status / release-provenance comments are generated
    # from the summary that just landed, never hand-edited. --check immediately proves
    # the generator agreed with its own output.
    Invoke-Stage 'release-claims' {
        Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'UpdateReleaseStatus.py')) 'release-claims'
        Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'UpdateReleaseStatus.py'), '--check') 'release-claims-check'
    } @('tool-environment')
}
Write-Host "Release summary: $(Join-Path $OutputDir 'release-summary.md')"
if (-not $NoCache -and $script:CacheHits) {
    Write-Host "Build reuse: $script:CacheHits stage(s) reused from Saved/ReleaseCache (-NoCache to force everything)."
}
if (@($Stages | Where-Object { $_.status -notin @('passed', 'cached') }).Count) { exit 1 }
exit 0

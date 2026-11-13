<#
.SYNOPSIS
    One-command Win64 release validation for Lua and Luau. No parameters.
.DESCRIPTION
    Run this file with no arguments. It is the only user-facing entry point and it always
    performs the COMPLETE Release. Every check in this repository is a stage of this one
    command: repository audit, Python/PowerShell tooling regressions, building both
    analyzers, a strict documentation site build, both backend builds, generated static
    bindings, Editor and cooked Development automation, connected delivery matrix
    (8 scenarios + 4 variants per backend), declaration/type checks, memory leak and
    performance evidence, a Development Server build, Shipping packaging, execution
    evidence capture, provenance and promotion gates, and the eight published release
    markers. There is no second command to remember. Missing mandatory evidence fails.
    See Docs/CN/Testing.md and Docs/EN/Testing.md for the current scope and remaining gaps.

    The engine and the project are discovered automatically (UNREAL_ENGINE_PATH, the
    .uproject's registered engine build, or a neighbouring source build); results always
    land in Saved/ReleaseTests/test. Build stages whose inputs did not change are reused
    from Saved/ReleaseCache; test, evidence and report stages always run.

    This entry point stays parameterless by design. The only supported adjustment is
    the environment variable UNLUANOVA_RELEASE_TIMEOUT_MINUTES (1..1440, default 60):
    a cold UE 6.0 cook compiles shaders from scratch and can exceed the default.
    It changes the timeout of every external stage, never the scope of the Release.

    Backend isolation is unconditional: every carrier uses
    UnLuaNovaDemo<Backend>{Editor,Server,''} and Backends/<Backend> outputs for DLLs,
    intermediates, staged packages, cooked assets and generated static bindings.

    Selective or partial runs are not a release verdict. Developers debugging a single
    filter, carrier, tier or -BuildOnly may use .\Tools\RunDevelopmentTests.ps1, which
    ReleasePipeline.ps1 also calls internally for the per-backend automation stages.
    The connected -NetMatrix is no longer a separate opt-in command: this pipeline runs
    it for both backends into Saved/ReleaseTests/test/net-matrix and judges it inline.
    Any stage that fails is recorded in release-summary.json and fails the whole run.
.EXAMPLE
    .\Tools\RunTests.ps1
#>

$ErrorActionPreference = 'Stop'

# Deliberately no parameter block: a complete Release has nothing to configure, and a
# forgotten switch must never turn the verdict into a partial run. Anything passed on the
# command line lands in $args - reject it with a pointer to the right tool instead of
# silently ignoring it.
if ($args.Count -gt 0) {
    throw @"
.\Tools\RunTests.ps1 takes no parameters - it always runs the complete Release matrix.
For a partial/debug run use .\Tools\RunDevelopmentTests.ps1 (for example -Filter,
-Backend, -BuildOnly or -NetMatrix). Release maintenance switches such as -DryRun,
-Baseline and -NoCache belong to .\Tools\ReleasePipeline.ps1.
"@
}

$LASTEXITCODE = 0
& (Join-Path $PSScriptRoot 'ReleasePipeline.ps1')
exit $LASTEXITCODE

<#
.SYNOPSIS
    One-command Win64 release validation for Lua and Luau. No parameters.
.DESCRIPTION
    Run this file with no arguments. It is the only user-facing entry point and it always
    performs the complete Release: tool bootstrap and regressions, both backend builds,
    generated static bindings, Editor and cooked Development automation, connected peers,
    declaration/type checks, memory leak and performance evidence, a Development Server
    build and Shipping packaging. Missing mandatory evidence fails. See Docs/CN/Testing.md
    and Docs/EN/Testing.md for the current scope and remaining gaps.

    The engine and the project are discovered automatically (UNREAL_ENGINE_PATH, the
    .uproject's registered engine build, or a neighbouring source build); results always
    land in Saved/ReleaseTests/test. Build stages whose inputs did not change are reused
    from Saved/ReleaseCache; test, evidence and report stages always run.

    Backend isolation is unconditional: every carrier uses
    UnLuaNovaDemo<Backend>{Editor,Server,''} and Backends/<Backend> outputs for DLLs,
    intermediates, staged packages, cooked assets and generated static bindings.

    Selective or partial runs are not a release verdict. Developers who need a filter, a
    carrier, a tier, -BuildOnly or the connected -NetMatrix use
    .\Tools\RunDevelopmentTests.ps1 instead; that script is also what ReleasePipeline.ps1
    calls internally for the per-backend automation stages.
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

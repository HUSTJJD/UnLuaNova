<#
.SYNOPSIS
    Runs the UnLuaNova automation tests and writes a readable report.

.DESCRIPTION
    Thin wrapper around the UE commandlet that everyone on the team can call the same
    way, so "did it pass?" never depends on how somebody spelled -ExecCmds:

        UnrealEditor-Cmd.exe <uproject> -unattended -NoSplash
            -ExecCmds="Automation RunTests <filter>; Quit"
            -ReportExportPath=<dir> -abslog=<dir>\Editor.log

    After the run it collects the JSON the automation framework exported, grabs any
    screenshots the tests took, and hands both to Tools/GenerateTestReport.py, which
    writes the actual conclusions (report.md + summary.json).

    The tests need a real UE build, so this always runs locally - see
    Docs/CN/Testing.md. Use -Publish when you want the result on the docs site; CI only
    renders what has been committed.

.PARAMETER EnginePath
    UE root (the directory containing Engine/). Left empty it is discovered: the
    UNREAL_ENGINE_PATH environment variable, then a source build in this repository or one
    of its parents, then any build registered by UnrealVersionSelector. Nothing here is
    machine specific, so the script works on a fresh clone.

.PARAMETER ProjectPath
    Path to the .uproject. Left empty, the first .uproject found in the repository root is
    used - so this does not have to be edited per checkout either.

.PARAMETER Filter
    Automation test filter, matched as a prefix. Default "UnLuaNova" runs everything the
    plugin registers. Use "UnLuaNova.API" etc. to narrow it down.

.PARAMETER Map
    Map to open before running. Most UnLuaNova tests build their own world, so this is
    only needed for the scene/showcase tests that expect a specific level.

.PARAMETER ExtraCommands
    Console commands appended after the suite, separated by ';'. Use it for screenshots in
    a run that is not driven by a test, e.g.
    -ExtraCommands "UnLuaNovaTest.Screenshot Showcase_Binding".

.PARAMETER Screenshots
    Keep rendering enabled and copy Saved/Screenshots into the report. Off by default
    because it forces a real viewport, which is much slower than -NullRHI.

.PARAMETER Publish
    Copy the generated report.md and its screenshots into Docs/ so the GitHub Pages
    build picks them up.

.PARAMETER NoReport
    Skip report generation; just run the tests and leave the raw JSON behind.

.PARAMETER DryRun
    Print what would run - resolved engine, project, output dir and the full command line -
    and exit without starting the editor. Use it to check the discovered paths.

.EXAMPLE
    .\Tools\RunTests.ps1

.EXAMPLE
    .\Tools\RunTests.ps1 -Filter UnLuaNova.API -Publish
#>

[CmdletBinding()]
param(
    [string] $EnginePath = "",
    [string] $ProjectPath = "",
    [string] $Filter = "UnLuaNova",
    [string] $Map = "",
    [string] $OutputDir = "",
    [string] $ExtraCommands = "",
    [switch] $Screenshots,
    [switch] $Publish,
    [switch] $NoReport,
    [switch] $DryRun
)

$ErrorActionPreference = "Stop"

# Everything is derived from where this script lives, so no path in here belongs to one
# particular machine.
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ToolsDir = Join-Path $RepoRoot "Tools"

function Test-UnrealEngineRoot {
    param([string] $Path)
    return ($Path -and (Test-Path (Join-Path $Path "Engine/Binaries/Win64/UnrealEditor-Cmd.exe")))
}

function Get-RegisteredEngineBuilds {
    # UnrealVersionSelector keeps GUID -> engine path. Reading it keeps this script free of
    # any machine specific path: the mapping lives on the machine, not in the repository.
    $RegKey = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
    if (-not (Test-Path $RegKey)) { return @() }

    $Builds = @()
    foreach ($Prop in (Get-ItemProperty -Path $RegKey).PSObject.Properties) {
        if ($Prop.Name -like "PS*") { continue }
        $Builds += [pscustomobject]@{ Id = $Prop.Name; Path = $Prop.Value }
    }
    return $Builds
}

function Find-UnrealEngine {
    param([string] $Hint, [string] $UProjectPath)

    if ($Hint) { return $Hint }
    if ($env:UNREAL_ENGINE_PATH) { return $env:UNREAL_ENGINE_PATH }

    # A .uproject names its engine by GUID, and the registry maps that GUID to a path -
    # the authoritative answer wherever the project has been associated with a build.
    if ($UProjectPath -and (Test-Path $UProjectPath)) {
        $Association = $null
        try { $Association = (Get-Content $UProjectPath -Raw | ConvertFrom-Json).EngineAssociation } catch { }
        if ($Association) {
            foreach ($Build in (Get-RegisteredEngineBuilds)) {
                if ($Build.Id -eq $Association -and (Test-UnrealEngineRoot $Build.Path)) {
                    return $Build.Path
                }
            }
        }
    }

    # A source build often sits next to the project or a few levels above it.
    $Dir = $RepoRoot
    for ($i = 0; $i -lt 5; $i++) {
        if (Test-UnrealEngineRoot $Dir) { return $Dir }
        $Parent = Split-Path -Parent $Dir
        if (-not $Parent -or $Parent -eq $Dir) { break }
        $Dir = $Parent
    }

    # Last resort: any registered build that actually has the commandlet.
    foreach ($Build in (Get-RegisteredEngineBuilds)) {
        if (Test-UnrealEngineRoot $Build.Path) { return $Build.Path }
    }

    return $null
}

if (-not $ProjectPath) {
    $FoundProject = Get-ChildItem -Path $RepoRoot -Filter "*.uproject" -File | Select-Object -First 1
    if (-not $FoundProject) {
        throw "No .uproject found under '$RepoRoot'. Pass -ProjectPath <path to .uproject>."
    }
    $ProjectPath = $FoundProject.FullName
}

$EnginePath = Find-UnrealEngine -Hint $EnginePath -UProjectPath $ProjectPath
if (-not $EnginePath) {
    throw @"
Could not locate a UE build. Do one of:
  - pass -EnginePath <UE root>
  - set the UNREAL_ENGINE_PATH environment variable
  - associate the project with a build (right click the .uproject -> Switch Unreal Engine version)
"@
}

if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot ("Saved/TestReports/" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
# UE resolves -ReportExportPath against its own working directory (the engine's
# Binaries/Win64), not ours, so a relative -OutputDir silently wrote the JSON next to
# UnrealEditor-Cmd.exe and the run looked like it produced nothing.
if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path (Get-Location) $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$EditorCmd = Join-Path $EnginePath "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
if (-not (Test-Path $EditorCmd)) {
    throw "UnrealEditor-Cmd.exe not found under '$EnginePath'. Pass -EnginePath <UE root>."
}
if (-not (Test-Path $ProjectPath)) {
    throw "Project not found at '$ProjectPath'. Pass -ProjectPath <path to .uproject>."
}

# Automation exports one JSON per test into -ReportExportPath; the log goes next to it so
# a failed run can be read without hunting through Saved/Logs.
$LogFile = Join-Path $OutputDir "Editor.log"

# ExtraCommands run after the suite, which is where screenshots belong - they need the
# world the tests left behind, not the one at startup.
$Cmds = @("Automation RunTests $Filter")
if ($ExtraCommands) { $Cmds += $ExtraCommands }
$Cmds += "Quit"
$ExecCmds = $Cmds -join "; "

$Args = @(
    $ProjectPath
)
if ($Map) { $Args += $Map }
$Args += @(
    "-unattended"
    "-NoSplash"
    "-stdout"
    "-ExecCmds=$ExecCmds"
    "-ReportExportPath=$OutputDir"
    "-abslog=$LogFile"
)
# Screenshots need a real viewport; without them the run is headless and much faster.
if (-not $Screenshots) { $Args += "-NullRHI" }

Write-Host "Engine : $EnginePath"
Write-Host "Filter : $Filter"
Write-Host "Output : $OutputDir"
Write-Host "Cmd    : $EditorCmd $($Args -join ' ')"
Write-Host ""

if ($DryRun)
{
    Write-Host "Dry run - nothing was executed."
    exit 0
}

$sw = [System.Diagnostics.Stopwatch]::StartNew()
& $EditorCmd @Args
$EditorExit = $LASTEXITCODE
$sw.Stop()

Write-Host ""
Write-Host "Editor exited with $EditorExit in $('{0:N1}' -f $sw.Elapsed.TotalSeconds)s"

# Screenshots land in the project's Saved dir, not next to the JSON; pull them in so the
# report is self-contained.
if ($Screenshots) {
    $ShotDir = Join-Path $RepoRoot "Saved/Screenshots/WindowsEditor"
    if (Test-Path $ShotDir) {
        $Dest = Join-Path $OutputDir "screenshots"
        New-Item -ItemType Directory -Path $Dest -Force | Out-Null
        Copy-Item (Join-Path $ShotDir "*.png") -Destination $Dest -Force -ErrorAction SilentlyContinue
    }
}

$JsonCount = @(Get-ChildItem -Path $OutputDir -Filter "*.json" -File -ErrorAction SilentlyContinue).Count
if ($JsonCount -eq 0) {
    Write-Host "No automation JSON in '$OutputDir' - check '$LogFile'." -ForegroundColor Yellow
}

$ExitCode = $EditorExit

if (-not $NoReport) {
    $Generator = Join-Path $ToolsDir "GenerateTestReport.py"
    if (-not (Test-Path $Generator)) {
        throw "Report generator not found at '$Generator'."
    }

    $GenArgs = @($Generator, "--reports", $OutputDir, "--out", $OutputDir)
    if ($Publish) { $GenArgs += "--publish" }

    & python @GenArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Report generation failed (exit $LASTEXITCODE)." -ForegroundColor Red
        if ($ExitCode -eq 0) { $ExitCode = $LASTEXITCODE }
    }

    $Summary = Join-Path $OutputDir "summary.json"
    if (Test-Path $Summary) {
        $S = Get-Content $Summary -Raw | ConvertFrom-Json
        Write-Host ""
        Write-Host ("Result : {0} passed, {1} failed, {2} total" -f $S.passed, $S.failed, $S.total)
        Write-Host ("Report : {0}" -f (Join-Path $OutputDir "report.md"))
        if ($S.failed -gt 0) { $ExitCode = 1 }
    }
}

exit $ExitCode

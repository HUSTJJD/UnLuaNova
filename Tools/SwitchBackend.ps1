<#
.SYNOPSIS
    Activate one prebuilt Lua backend for the example project without recompiling.
.DESCRIPTION
    UnLuaNovaDemoLuaEditor / UnLuaNovaDemoLuauEditor build into their own
    Binaries/<Platform>/Backends/<Backend> and Intermediate/.../Backends/<Backend>
    directories (ModuleRules.BinariesSubFolder). Both sets of DLLs can therefore sit on
    disk at the same time, but Unreal's FModuleManager scans Binaries/ recursively for
    UnrealEditor.modules manifests (ModuleManager.cpp FindModulePathsInDirectory), so two
    live manifests would make module resolution ambiguous: the editor could load a Lua
    UnLuaNova.dll next to a Luau Luau.dll.

    This script makes exactly one manifest live per binaries root by renaming the others
    to *.disabled (a name the engine never reads), and pins Config/DefaultUnLuaNova.ini's
    LuaBackend to the activated backend. No build, no copy, no recompile.

    Build each target once first:
      Build.bat UnLuaNovaDemoLuaEditor  Win64 Development -Project=<...>.uproject
      Build.bat UnLuaNovaDemoLuauEditor Win64 Development -Project=<...>.uproject

    -Backend Default restores the ordinary single-target layout (root manifests live,
    isolated manifests disabled); it does not rewrite the ini.
.PARAMETER Backend
    Lua, Luau, or Default. Required unless -Status is used.
.PARAMETER Status
    Read-only: print which backend currently has live manifests and the ini value.
.PARAMETER ProjectPath
    Optional .uproject path. Defaults to the one next to this repository root.
.PARAMETER Platform
    Binaries subdirectory, default Win64.
.PARAMETER DryRun
    Print the planned manifest renames and ini change without writing anything.
.EXAMPLE
    .\Tools\SwitchBackend.ps1 -Backend Lua
.EXAMPLE
    .\Tools\SwitchBackend.ps1 -Backend Luau -DryRun
.EXAMPLE
    .\Tools\SwitchBackend.ps1 -Status
#>

[CmdletBinding()]
param(
    [ValidateSet("", "Lua", "Luau", "Default")]
    [string] $Backend = "",
    [switch] $Status,
    [string] $ProjectPath = "",
    [string] $Platform = "Win64",
    [switch] $DryRun
)

$ErrorActionPreference = "Stop"

if (-not $Status -and -not $Backend) {
    throw "Pass -Backend Lua|Luau|Default, or -Status to inspect the current state."
}
if ($Status -and $Backend) {
    throw "-Status is read-only; do not combine it with -Backend."
}

$RepoRoot = Split-Path -Parent $PSScriptRoot
if (-not $ProjectPath) {
    $FoundProject = Get-ChildItem -Path $RepoRoot -Filter "*.uproject" -File | Select-Object -First 1
    if (-not $FoundProject) { throw "No .uproject found under '$RepoRoot'. Pass -ProjectPath." }
    $ProjectPath = $FoundProject.FullName
}
if (-not (Test-Path $ProjectPath)) { throw "Project not found: '$ProjectPath'." }
$ProjectDir = Split-Path -Parent $ProjectPath
$IniPath = Join-Path $ProjectDir "Config/DefaultUnLuaNova.ini"
$IniSection = "[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]"

# Every module whose Build.cs calls UnLuaNovaBackendLayout.Apply writes its DLL into one
# of these roots. Keep this list in sync when adding a plugin with Lua-facing modules;
# Tools/tests/test_backend_isolation.py cross-checks it against the Build.cs set.
$PluginCandidates = @(
    (Join-Path $ProjectDir "Plugins/UnLuaNova"),
    (Join-Path $ProjectDir "Plugins/UnLuaNova/Plugins/UnLuaNova")
)
$PluginRoot = $PluginCandidates | Where-Object { Test-Path (Join-Path $_ "UnLuaNova.uplugin") } | Select-Object -First 1
if (-not $PluginRoot) { throw "UnLuaNova.uplugin not found under '$ProjectDir/Plugins'." }

$BinariesRoots = @(
    (Join-Path $ProjectDir "Binaries/$Platform"),
    (Join-Path $PluginRoot "Binaries/$Platform"),
    (Join-Path $ProjectDir "Plugins/UnLuaNovaTestSuite/Binaries/$Platform")
)
$ExtensionsRoot = Join-Path $ProjectDir "Plugins/UnLuaNovaExtensions"
if (Test-Path $ExtensionsRoot) {
    foreach ($ExtDir in (Get-ChildItem -Path $ExtensionsRoot -Directory)) {
        $BinariesRoots += Join-Path $ExtDir.FullName "Binaries/$Platform"
    }
}

$ManifestName = "UnrealEditor.modules"
$DisabledSuffix = ".disabled"
$Backends = @("Lua", "Luau")

function Get-IniBackend {
    if (-not (Test-Path $IniPath)) { return $null }
    $InSection = $false
    foreach ($Line in [System.IO.File]::ReadAllLines($IniPath, [System.Text.Encoding]::UTF8)) {
        $Trimmed = $Line.Trim()
        if ($Trimmed.StartsWith("[")) { $InSection = ($Trimmed -ieq $IniSection); continue }
        if ($InSection -and $Trimmed -match "^LuaBackend\s*=\s*(.*)$") { return $Matches[1].Trim() }
    }
    return $null
}

function Set-IniBackend {
    param([string] $Value)

    $Lines = if (Test-Path $IniPath) {
        @([System.IO.File]::ReadAllLines($IniPath, [System.Text.Encoding]::UTF8))
    } else { @() }
    $Out = New-Object System.Collections.Generic.List[string]
    $InSection = $false
    $Written = $false
    foreach ($Line in $Lines) {
        $Trimmed = $Line.Trim()
        if ($Trimmed.StartsWith("[")) {
            if ($InSection -and -not $Written) { $Out.Add("LuaBackend=$Value"); $Written = $true }
            $InSection = ($Trimmed -ieq $IniSection)
            $Out.Add($Line)
            continue
        }
        if ($InSection -and $Trimmed -match "^LuaBackend\s*=") {
            if (-not $Written) { $Out.Add("LuaBackend=$Value"); $Written = $true }
            continue
        }
        $Out.Add($Line)
    }
    if (-not $Written) {
        if (-not $InSection) {
            if ($Out.Count -gt 0) { $Out.Add("") }
            $Out.Add($IniSection)
        }
        $Out.Add("LuaBackend=$Value")
    }
    if ($DryRun) {
        Write-Host "DryRun   : would write LuaBackend=$Value to $IniPath"
        return
    }
    New-Item -ItemType Directory -Path (Split-Path -Parent $IniPath) -Force | Out-Null
    # No BOM: UBT parses this file and a BOM in front of a section header breaks it.
    [System.IO.File]::WriteAllLines($IniPath, $Out, (New-Object System.Text.UTF8Encoding($false)))
    Write-Host "Ini      : LuaBackend=$Value -> $IniPath"
}

function Get-ManifestState {
    param([string] $Root)

    $Live = Join-Path $Root $ManifestName
    $state = [pscustomobject]@{ Root = $Root; RootLive = (Test-Path $Live); Backends = @{} }
    foreach ($B in $Backends) {
        $BackendLive = Join-Path (Join-Path (Join-Path $Root "Backends") $B) $ManifestName
        $state.Backends[$B] = (Test-Path $BackendLive)
    }
    return $state
}

if ($Status) {
    Write-Host "Ini      : LuaBackend=$(Get-IniBackend)"
    foreach ($Root in $BinariesRoots) {
        if (-not (Test-Path $Root)) { continue }
        $S = Get-ManifestState -Root $Root
        $Active = @()
        if ($S.RootLive) { $Active += "Default" }
        foreach ($B in $Backends) { if ($S.Backends[$B]) { $Active += $B } }
        if ($Active.Count -eq 0) { $Active = @("(none)") }
        Write-Host ("Root     : {0}`n           active: {1}" -f $Root, ($Active -join ", "))
    }
    exit 0
}

# A disabled manifest is a plain rename: the engine only ever reads the exact
# UBT_MODULE_MANIFEST filename, so *.disabled is invisible to module discovery.
function Set-ManifestEnabled {
    param([string] $Path, [bool] $Enabled, [string] $Label)

    $Disabled = "$Path$DisabledSuffix"
    $LiveExists = Test-Path $Path
    $DisabledExists = Test-Path $Disabled
    if ($Enabled) {
        if ($LiveExists) { return }
        if (-not $DisabledExists) { return }
        if ($DryRun) { Write-Host "DryRun   : would enable $Label ($Disabled -> $Path)"; return }
        [System.IO.File]::Move($Disabled, $Path)
        Write-Host "Enable   : $Label"
    } else {
        if (-not $LiveExists) { return }
        if ($DisabledExists) {
            throw "Both '$Path' and '$Disabled' exist. Resolve by hand before switching (refusing to delete a manifest)."
        }
        if ($DryRun) { Write-Host "DryRun   : would disable $Label ($Path -> $Disabled)"; return }
        [System.IO.File]::Move($Path, $Disabled)
        Write-Host "Disable  : $Label"
    }
}

$WantedBackends = if ($Backend -eq "Default") { @() } else { @($Backend) }
foreach ($Root in $BinariesRoots) {
    if (-not (Test-Path $Root)) { continue }
    Set-ManifestEnabled -Path (Join-Path $Root $ManifestName) `
        -Enabled ($Backend -eq "Default") -Label "$Root (ordinary target)"
    foreach ($B in $Backends) {
        $Path = Join-Path (Join-Path (Join-Path $Root "Backends") $B) $ManifestName
        Set-ManifestEnabled -Path $Path -Enabled ($WantedBackends -contains $B) -Label "$Root Backends/$B"
    }
}

# Fail closed: a switch that leaves the editor with no (or an unbuilt) backend is worse
# than a switch that refuses. Check the VM DLL and at least one live isolated manifest.
if ($Backend -ne "Default") {
    $VmDll = Join-Path (Join-Path (Join-Path $PluginRoot "Binaries/$Platform") "Backends/$Backend") "UnrealEditor-$Backend.dll"
    if (-not $DryRun -and -not (Test-Path $VmDll)) {
        throw @"
'$VmDll' is missing - the $Backend target has not been built yet.
Build it once, then switch again:
  Build.bat UnLuaNovaDemo${Backend}Editor $Platform Development -Project=$ProjectPath
Switching backends never compiles; it only changes which prebuilt manifest is live.
"@
    }

    $AnyLive = $false
    foreach ($Root in $BinariesRoots) {
        $Candidate = Join-Path (Join-Path (Join-Path $Root "Backends") $Backend) $ManifestName
        if ((-not $DryRun) -and (Test-Path $Candidate)) { $AnyLive = $true }
        if ($DryRun) { $AnyLive = $true }
    }
    if (-not $AnyLive) {
        throw "No live Backends/$Backend/UnrealEditor.modules found. Rebuild the $Backend Editor target (UBT writes the manifest) and try again."
    }
}

if ($Backend -ne "Default") { Set-IniBackend -Value $Backend }

Write-Host ("Active   : {0} (no compile, no overwrite)" -f $(if ($Backend -eq "Default") { "ordinary single-target layout" } else { $Backend }))

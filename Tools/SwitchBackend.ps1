<#
.SYNOPSIS
    Activate one prebuilt Lua backend across every carrier without recompiling.
.DESCRIPTION
    UnLuaNovaDemo<Backend>{Editor,Server,''} build into their own
    Binaries/<Platform>/Backends/<Backend> and Intermediate/.../Backends/<Backend>
    directories (ModuleRules.BinariesSubFolder). Both backends' DLLs can therefore sit on
    disk at the same time, but FModuleManager scans Binaries/ recursively for module
    manifests (ModuleManager.cpp FindModulePathsInDirectory), so two live manifests would
    make module resolution ambiguous: a process could load a Lua UnLuaNova.dll next to a
    Luau Luau.dll.

    Editor targets share AppName "UnrealEditor" (UnrealEditor.modules), while Game/Server
    targets use their own target name as AppName (UnLuaNovaDemoLua.modules,
    UnLuaNovaDemoLuauServer.modules, ...). The manifest names therefore vary by carrier and
    configuration, so this script discovers every *.modules instead of hardcoding one name.
    It makes exactly one zone live per binaries root by renaming the others to *.disabled
    (a name the engine never reads), and pins Config/DefaultUnLuaNova.ini's LuaBackend to
    the activated backend. No build, no copy, no recompile.

    A root that has no Backends/<Backend>/ manifests keeps its ordinary root manifest live:
    not every module is built by every carrier, and disabling the only manifest a root has
    would make those modules undiscoverable rather than just inactive.

    Build each carrier once first (Development / Win64):
      Build.bat UnLuaNovaDemoLuaEditor  Win64 Development -Project=<...>.uproject
      Build.bat UnLuaNovaDemoLua        Win64 Development -Project=<...>.uproject
      Build.bat UnLuaNovaDemoLuaServer Win64 Development -Project=<...>.uproject
      Build.bat UnLuaNovaDemoLuauEditor  ... (and the other two Luau targets)

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

# Every module whose Build.cs carries [UAK BACKEND LAYOUT] writes its DLL into one of
# these roots. Keep this list in sync when adding a plugin with Lua-facing modules;
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

$DisabledSuffix = ".disabled"
$ManifestPattern = "*.modules"
$IsolatedDir = "Backends"
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

function Get-ManifestEntries {
    # One entry per *potential* live manifest, with the zone it belongs to and whether the
    # live and/or disabled form is present. Zone is "Default" for a root-level manifest and
    # the backend name for anything under Backends/<Backend>/.
    param([string] $Root)

    $Entries = @{}
    if (-not (Test-Path -LiteralPath $Root)) { return @() }
    foreach ($F in @(Get-ChildItem -LiteralPath $Root -File -ErrorAction SilentlyContinue)) {
        $Name = $F.Name
        $IsDisabled = $Name.EndsWith($DisabledSuffix)
        if ($IsDisabled) { $Name = $Name.Substring(0, $Name.Length - $DisabledSuffix.Length) }
        if ($Name -notlike $ManifestPattern) { continue }

        $LivePath = Join-Path $Root $Name
        if (-not $Entries.ContainsKey($LivePath)) {
            $Rel = $LivePath.Substring($Root.Length).TrimStart('\', '/')
            $Parts = $Rel -split '[\\/]'
            $Zone = "Default"
            if ($Parts.Count -ge 3 -and $Parts[0] -eq $IsolatedDir -and ($Backends -contains $Parts[1])) {
                $Zone = $Parts[1]
            }
            $Entries[$LivePath] = [pscustomobject]@{
                Path = $LivePath; Zone = $Zone; Live = $false; Disabled = $false
            }
        }
        if ($IsDisabled) { $Entries[$LivePath].Disabled = $true }
        else { $Entries[$LivePath].Live = $true }
    }
    return @($Entries.Values)
}

# A disabled manifest is a plain rename: the engine only ever reads the exact
# UBT_MODULE_MANIFEST filename, so *.disabled is invisible to module discovery.
function Set-ManifestEnabled {
    param([string] $Path, [bool] $Enabled, [string] $Label)

    $Disabled = "$Path$DisabledSuffix"
    $LiveExists = Test-Path -LiteralPath $Path
    $DisabledExists = Test-Path -LiteralPath $Disabled
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

if ($Status) {
    Write-Host "Ini      : LuaBackend=$(Get-IniBackend)"
    $LiveZones = @{}
    foreach ($Root in $BinariesRoots) {
        $Entries = @(Get-ManifestEntries -Root $Root)
        if ($Entries.Count -eq 0) { continue }
        $LiveNames = @($Entries | Where-Object { $_.Live } |
            ForEach-Object { "$(Split-Path -Leaf $_.Path) [$($_.Zone)]" })
        foreach ($E in $Entries) { if ($E.Live) { $LiveZones[$E.Zone] = $true } }
        if ($LiveNames.Count -eq 0) { $LiveNames = @("(none)") }
        Write-Host ("Root     : {0}`n           live: {1}" -f $Root, ($LiveNames -join ", "))
    }
    $IsolatedLive = @($Backends | Where-Object { $LiveZones.ContainsKey($_) })
    if ($IsolatedLive.Count -gt 1) {
        $Active = "AMBIGUOUS: " + ($IsolatedLive -join ', ')
    } elseif ($IsolatedLive.Count -eq 1) {
        $Active = $IsolatedLive[0]
    } elseif ($LiveZones.ContainsKey("Default")) {
        $Active = "Default (ordinary single-target layout)"
    } else {
        $Active = "(none)"
    }
    Write-Host "Active   : $Active"
    exit 0
}

foreach ($Root in $BinariesRoots) {
    $Entries = @(Get-ManifestEntries -Root $Root)
    if ($Entries.Count -eq 0) { continue }

    # Only suppress a root's ordinary manifests when this root actually has an isolated
    # manifest for the requested backend. Otherwise the ordinary build stays usable.
    $HasTargetIsolated = $Backend -ne "Default" -and
        @($Entries | Where-Object { $_.Zone -eq $Backend }).Count -gt 0

    foreach ($E in $Entries) {
        if ($E.Live -and $E.Disabled) {
            throw "Both '$($E.Path)' and '$($E.Path)$DisabledSuffix' exist. Resolve by hand before switching (refusing to delete a manifest)."
        }
        $Enable = $false
        if ($E.Zone -eq "Default") {
            $Enable = ($Backend -eq "Default") -or (-not $HasTargetIsolated)
        } elseif ($Backend -ne "Default" -and $E.Zone -eq $Backend) {
            $Enable = $true
        }
        $Label = "$(Split-Path -Leaf $E.Path) [$($E.Zone)] @ $Root"
        Set-ManifestEnabled -Path $E.Path -Enabled $Enable -Label $Label
    }
}

# Fail closed: a switch that leaves the process with no (or an unbuilt) backend is worse
# than a switch that refuses. Every carrier's VM DLL is named *-<Backend>.dll, but its
# prefix follows AppName (UnrealEditor for Editor, the target name for Game/Server), so
# look for the suffix rather than one hardcoded name.
if ($Backend -ne "Default") {
    $VmRoot = Join-Path (Join-Path $PluginRoot "Binaries/$Platform") "$IsolatedDir/$Backend"
    $VmDlls = @(Get-ChildItem -LiteralPath $VmRoot -File -Filter "*-${Backend}.dll" -ErrorAction SilentlyContinue)
    if (-not $DryRun -and $VmDlls.Count -eq 0) {
        throw @"
'$VmRoot/*-$Backend.dll' is missing - the $Backend targets have not been built yet.
Build each carrier once, then switch again:
  Build.bat UnLuaNovaDemo${Backend}Editor $Platform Development -Project=$ProjectPath
  Build.bat UnLuaNovaDemo${Backend}       $Platform Development -Project=$ProjectPath
  Build.bat UnLuaNovaDemo${Backend}Server $Platform Development -Project=$ProjectPath
Switching backends never compiles; it only changes which prebuilt manifest is live.
"@
    }

    $AnyLive = $false
    foreach ($Root in $BinariesRoots) {
        if ($DryRun) { $AnyLive = $true; break }
        foreach ($E in @(Get-ManifestEntries -Root $Root)) {
            if ($E.Zone -eq $Backend -and $E.Live -and -not $E.Disabled) { $AnyLive = $true }
        }
    }
    if (-not $AnyLive) {
        throw "No live Backends/$Backend/*.modules found. Rebuild the $Backend targets (UBT writes the manifests) and try again."
    }
}

if ($Backend -ne "Default") { Set-IniBackend -Value $Backend }

if ($DryRun -and $Backend -ne "Default") {
    Write-Host "DryRun   : nothing was renamed or written; if Backends/$Backend/*.modules is missing, build the $Backend targets once first."
}
Write-Host ("Active   : {0} (no compile, no overwrite)" -f $(if ($Backend -eq "Default") { "ordinary single-target layout" } else { $Backend }))

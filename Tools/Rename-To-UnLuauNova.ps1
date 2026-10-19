<#
.SYNOPSIS
    Renames the vendored UnLua plugin to UnLuauNova (permanent fork).

.DESCRIPTION
    Since the project is a permanent fork, this performs the full rename that was
    deliberately postponed while upstream syncing was still a goal.

    It is intentionally NOT executed automatically: a rename touches module names,
    API macros and UCLASS names, so it must be followed by a full compile and by
    resaving assets / config sections. Run it on a clean tree, build, then commit.

    What it changes
      - Plugins/UnLua                     -> Plugins/UnLuauNova
      - Source/UnLua* module directories  -> Source/UnLuauNova*
      - module name "UnLua"               -> "UnLuauNova"      (Build.cs, .uplugin, .uproject)
      - UNLUA_API                         -> UNLUANOVA_API
      - LogUnLua                          -> LogUnLuauNova
      - namespace UnLua                   -> namespace UnLuauNova
      - FriendlyName / CreatedBy in .uplugin

    What it deliberately does NOT change (needs asset + config migration)
      - UCLASS names: UUnLuaSettings / UUnLuaInterface / ULuaEnvLocator / ULuaModuleLocator
      - ini section [/Script/UnLuaEditor.UnLuaEditorSettings]
      - the Lua-side global `UnLua` (kept so existing scripts keep working)

.EXAMPLE
    pwsh Tools/Rename-To-UnLuauNova.ps1 -WhatIf
    pwsh Tools/Rename-To-UnLuauNova.ps1
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string] $RepoRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'

$Old = 'UnLua'
$New = 'UnLuauNova'

function Rename-InFiles {
    param([string[]] $Path, [string[]] $Include)

    Get-ChildItem -Path $Path -Recurse -File -Include $Include |
        Where-Object { $_.FullName -notmatch '[\\/](Binaries|Intermediate|\.git|\.codebuddy)[\\/]' } |
        ForEach-Object {
            $text = [IO.File]::ReadAllText($_.FullName)
            $original = $text

            # Order matters: longest / most specific first.
            $text = $text -replace "`"$Old`"", "`"$New`""                       # module name strings
            $text = $text -replace "${Old}_API", "$($New.ToUpper())_API"        # UNLUA_API
            $text = $text -replace "Log$Old", "Log$New"                          # LogUnLua
            $text = $text -replace "namespace\s+$Old\b", "namespace $New"        # namespace UnLua

            if ($text -ne $original) {
                if ($PSCmdlet.ShouldProcess($_.FullName, 'rewrite')) {
                    [IO.File]::WriteAllText($_.FullName, $text)
                }
            }
        }
}

# 1. file contents
Rename-InFiles -Path $RepoRoot -Include @('*.h', '*.cpp', '*.cs', '*.uplugin', '*.uproject', '*.ini', '*.md')

# 2. module directories
$pluginRoot = Join-Path $RepoRoot 'Plugins/UnLua'
if (Test-Path $pluginRoot) {
    Get-ChildItem -Path (Join-Path $pluginRoot 'Source') -Directory |
        Where-Object { $_.Name -like "$Old*" } |
        ForEach-Object {
            $target = Join-Path $_.Parent.FullName ($_.Name -replace "^$Old", $New)
            if ($PSCmdlet.ShouldProcess($_.FullName, "rename dir -> $target")) {
                Move-Item -LiteralPath $_.FullName -Destination $target
            }
        }

    $target = Join-Path (Split-Path $pluginRoot -Parent) $New
    if ($PSCmdlet.ShouldProcess($pluginRoot, "rename plugin dir -> $target")) {
        Move-Item -LiteralPath $pluginRoot -Destination $target
    }
}

Write-Host "`nRename done. Now:" -ForegroundColor Yellow
Write-Host "  1. Build the project and fix anything the script missed."
Write-Host "  2. Resave assets referencing UUnLuaSettings / UUnLuaInterface."
Write-Host "  3. Rename the ini section [/Script/UnLuaEditor.UnLuaEditorSettings]."
Write-Host "  4. Update README / LICENSE / CHANGELOG branding."

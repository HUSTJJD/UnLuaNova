<#
.SYNOPSIS
    Renames the vendored UnLua plugin to UnLuauNova (permanent fork).

.DESCRIPTION
    Scope is deliberately "module + brand identity", not "every file name":
    renaming individual .h/.cpp files would break #include directives for no
    benefit, while module directories MUST match the module name for UBT.

    Renamed
      - Plugins/UnLua -> Plugins/UnLuauNova, UnLua.uplugin -> UnLuauNova.uplugin
      - Source/UnLua* module directories and their *.Build.cs
      - module name "UnLua" in .uplugin / *.Build.cs / IMPLEMENT_MODULE / LoadModuleChecked
      - UNLUA_API -> UNLUANOVA_API, LogUnLua -> LogUnLuauNova, namespace UnLua -> UnLuauNova
      - "UnLua" string literals in C++/C# (plugin lookups, settings section, stat group, UI labels)
      - "Source/UnLua/" path literals

    Preserved on purpose
      - the Lua-visible global `UnLua` (UnLuaLib.cpp / UnLuaManager.cpp) so existing scripts keep working
      - UCLASS names (UUnLuaSettings / UUnLuaInterface / ULuaEnvLocator / ULuaModuleLocator) - they need
        an asset resave + config section migration first
      - Config=UnLuaSettings (renaming it means renaming Config/DefaultUnLuaSettings.ini)
      - every *.lua file (module names like `UnLua.Class` are part of the Lua API)

    Produces a single revertible commit. Build afterwards.
#>
[CmdletBinding(SupportsShouldProcess = $true)]
param(
    [string] $RepoRoot = (Split-Path -Parent $PSScriptRoot)
)

$ErrorActionPreference = 'Stop'
$Old = 'UnLua'
$New = 'UnLuauNova'

# Lua-visible global must survive; UCLASS/config names need a separate migration.
$ContentExcluded = @('UnLuaLib.cpp', 'UnLuaManager.cpp')

function Edit-TextFile {
    param([System.IO.FileInfo] $File, [scriptblock] $Transform)

    $text = [IO.File]::ReadAllText($File.FullName)
    $updated = & $Transform $text
    if ($updated -ne $text) {
        if ($PSCmdlet.ShouldProcess($File.FullName, 'rewrite')) {
            [IO.File]::WriteAllText($File.FullName, $updated)
        }
    }
}

$codeFiles = Get-ChildItem -Path $RepoRoot -Recurse -File -Include '*.h', '*.cpp', '*.cs', '*.uplugin', '*.uproject' |
    Where-Object { $_.FullName -notmatch '[\\/](Binaries|Intermediate|\.git|\.codebuddy)[\\/]' }

foreach ($file in $codeFiles) {
    if ($ContentExcluded -contains $file.Name) { continue }

    Edit-TextFile $file {
        param($t)
        $t = $t -replace "${Old}_API", "$($New.ToUpper())_API"       # UNLUA_API
        $t = $t -replace "\bLog$Old\b", "Log$New"                     # LogUnLua
        $t = $t -replace "namespace\s+$Old\b", "namespace $New"       # namespace UnLua
        $t = $t -replace "Source/$Old/", "Source/$New/"               # path literals
        $t = $t -replace '"UnLua"', '"UnLuauNova"'                    # module/plugin/settings strings
        return $t
    }
}

# Module directories (must match module name for UBT) + their Build.cs
$pluginRoot = Join-Path $RepoRoot "Plugins/$Old"
if (Test-Path $pluginRoot) {
    Get-ChildItem -Path (Join-Path $pluginRoot 'Source') -Directory |
        Where-Object { $_.Name -like "$Old*" } |
        ForEach-Object {
            $target = Join-Path $_.Parent.FullName ($_.Name -replace "^$Old", $New)
            if ($PSCmdlet.ShouldProcess($_.FullName, "rename module dir -> $target")) {
                Move-Item -LiteralPath $_.FullName -Destination $target
            }
        }

    $target = Join-Path (Split-Path $pluginRoot -Parent) $New
    if ($PSCmdlet.ShouldProcess($pluginRoot, "rename plugin dir -> $target")) {
        Move-Item -LiteralPath $pluginRoot -Destination $target
    }
}

Get-ChildItem -Path $RepoRoot -Recurse -File -Filter "$Old.Build.cs" |
    Where-Object { $_.FullName -notmatch '[\\/](Binaries|Intermediate|\.git)[\\/]' } |
    ForEach-Object {
        $target = Join-Path $_.DirectoryName "$New.Build.cs"
        if ($PSCmdlet.ShouldProcess($_.FullName, "rename -> $target")) {
            Move-Item -LiteralPath $_.FullName -Destination $target
        }
    }

$uplugin = Join-Path $RepoRoot "Plugins/$New/$Old.uplugin"
if (Test-Path $uplugin) {
    $target = Join-Path (Split-Path $uplugin -Parent) "$New.uplugin"
    if ($PSCmdlet.ShouldProcess($uplugin, "rename -> $target")) {
        Move-Item -LiteralPath $uplugin -Destination $target
    }
}

Write-Host "`nRename applied." -ForegroundColor Green
Write-Host "Next: build, then separately migrate UCLASS names + Config=UnLuaSettings."

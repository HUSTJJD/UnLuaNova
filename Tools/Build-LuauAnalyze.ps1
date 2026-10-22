<#
.SYNOPSIS
    从 Luau 子模块构建 luau-analyze（Luau 后端的静态检查器）。

.DESCRIPTION
    luau-analyze 不需要单独引入依赖：它是现有 Luau 子模块的一部分
    （Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau/CLI/src/Analyze.cpp）。
    UBT 用 .ubtignore 把 CLI/ 排除了（插件只需要 VM 和 Compiler），
    所以这个可执行文件从来没被构建出来 —— 本脚本补上这一步。

    只构建 Luau.Analyze.CLI 这一个 target（cmake 会带上它依赖的 Analysis / Compiler），
    不构建 tests、bench、CodeGen 这些跟检查无关的东西。

    构建产物落在 Luau 子模块的 build/ 里，由 .gitmodules 的 ignore = untracked 兜住，
    不会污染父仓库。

.EXAMPLE
    .\Tools\Build-LuauAnalyze.ps1

.EXAMPLE
    # 构建完立刻用它检查一次
    .\Tools\Build-LuauAnalyze.ps1; .\Tools\CheckLuauTypes.ps1
#>

[CmdletBinding()]
param(
    # cmake 生成/构建目录；默认在 Luau 子模块里
    [string] $BuildDir = "Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau/build",

    # 构建配置
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string] $Configuration = "Release"
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-InputPath([string] $Candidate)
{
    if ([System.IO.Path]::IsPathRooted($Candidate)) { return $Candidate }
    return Join-Path $RepoRoot $Candidate
}

$LuauDir = Resolve-InputPath "Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau"
if (-not (Test-Path -LiteralPath (Join-Path $LuauDir "CMakeLists.txt")))
{
    throw @"
Luau 子模块是空的：$LuauDir
先把它拉下来：

    git submodule update --init Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau
"@
}

$CMake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $CMake)
{
    throw @"
找不到 cmake。装一个（scoop install cmake / winget install Kitware.CMake），
或把它放进 PATH。luau-analyze 是 C++，只能从源码构建 —— 它没有官方预编译二进制。
"@
}

$Build = Resolve-InputPath $BuildDir

Write-Host "Luau   : $LuauDir"
Write-Host "cmake  : $($CMake.Source)"
Write-Host "build  : $Build ($Configuration)"

& cmake -S $LuauDir -B $Build -DCMAKE_BUILD_TYPE=$Configuration
if ($LASTEXITCODE -ne 0) { throw "cmake 配置失败（exit $LASTEXITCODE）。" }

& cmake --build $Build --target Luau.Analyze.CLI --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "构建 Luau.Analyze.CLI 失败（exit $LASTEXITCODE）。" }

# 多配置生成器（MSVC）会把产物放进 build/<Configuration>/，单配置的直接放 build/
$Exe = Get-ChildItem -LiteralPath $Build -Recurse -Filter "luau-analyze.exe" -File -ErrorAction SilentlyContinue |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1

if (-not $Exe)
{
    throw "构建结束了但没找到 luau-analyze.exe（在 $Build 下）。"
}

Write-Host ""
Write-Host "luau-analyze : $($Exe.FullName)" -ForegroundColor Green
Write-Host "Tools/CheckLuauTypes.ps1 会自动找到它；也可以显式指定："
Write-Host "    .\Tools\CheckLuauTypes.ps1 -LuauAnalyze `"$($Exe.FullName)`""
Write-Host "或设环境变量：`$env:LUAU_ANALYZE = `"$($Exe.FullName)`""

<#
.SYNOPSIS
    从子模块构建 luau-lsp，给 Luau 后端做静态检查用。

.DESCRIPTION
    luau-lsp 以子模块引入在 Tools/luau-lsp，跟 main（见 .gitmodules）。它自带一个嵌套
    子模块（luau/，就是分析器本体），第一次构建前要拉下来，本脚本会代劳。

    为什么需要它，而不是接着用 luau-analyze：上游把 luau-analyze 的 --defs 选项删了
    （CLI/src/Analyze.cpp 里已经没有这个分支，传进去只会被当成 "Unrecognized option"），
    而编辑器导出的 UnLuaNova.d.lua 只有喂进去才有意义 —— 不喂的话每个 UE.xxx 都是
    Unknown global，报告全是假错。luau-lsp analyze 有 --definitions，且读的就是同一份
    .d.lua 声明文件。

    构建走 cmake：需要一个 C++17 编译器（MSVC）。cl.exe 只在 VS 的开发者环境里，
    所以**请在 Developer PowerShell / x64 Native Tools Command Prompt 里跑这个脚本**，
    否则 cmake 找不到编译器，报的是一行看不懂的配置错误。

    产物：Tools/luau-lsp/build/<配置>/luau-lsp.exe
    （build/ 是未跟踪内容，由 .gitmodules 的 ignore = untracked 兜住，不会污染父仓库。）

.EXAMPLE
    .\Tools\Build-LuauLsp.ps1

.EXAMPLE
    # 构建完立刻用它检查一次
    .\Tools\Build-LuauLsp.ps1; .\Tools\CheckLuauTypes.ps1
#>

[CmdletBinding()]
param(
    # cmake 生成/构建目录；默认在 luau-lsp 子模块里
    [string] $BuildDir = "Tools/luau-lsp/build",

    # 构建配置
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string] $Configuration = "Release",

    # Optional explicit generator; use a separate BuildDir when changing generators.
    [string] $Generator = '',

    # 已构建过也重新构建一次
    [switch] $Force,

    # 不自动拉嵌套子模块（离线时用，前提是之前拉过）
    [switch] $NoSubmodules
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$SubDir = Join-Path $RepoRoot "Tools/luau-lsp"

function Resolve-InputPath([string] $Candidate)
{
    if ([System.IO.Path]::IsPathRooted($Candidate)) { return $Candidate }
    return Join-Path $RepoRoot $Candidate
}

function Find-LuauLsp([string] $Root)
{
    # MSVC 是多配置生成器，产物落在 build/<Configuration>/ 下；别的生成器直接放 build/
    return Get-ChildItem -LiteralPath $Root -Recurse -Filter "luau-lsp.exe" -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1
}

$Build = Resolve-InputPath $BuildDir

if (-not $Force)
{
    $Existing = Find-LuauLsp -Root $Build
    if ($Existing)
    {
        Write-Host "已经有产物了：$($Existing.FullName)"
        Write-Host "（加 -Force 重新构建）"
        exit 0
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $SubDir "CMakeLists.txt")))
{
    throw @"
luau-lsp 子模块是空的：$SubDir
先把它拉下来：

    git submodule update --init Tools/luau-lsp
"@
}

# --- 嵌套子模块 ---------------------------------------------------------------
# luau/ 是 luau-lsp 自带的 Luau 源码（分析器本体）。cmake 第一步就是
# add_subdirectory(luau)，目录空着配置阶段就失败。
if (-not $NoSubmodules)
{
    $Need = Join-Path $SubDir "luau"
    $Empty = (-not (Test-Path -LiteralPath $Need)) -or
             ((Get-ChildItem -LiteralPath $Need -Force -ErrorAction SilentlyContinue | Measure-Object).Count -eq 0)

    if ($Empty)
    {
        Write-Host "拉取 luau-lsp 的嵌套子模块（luau/）…"
        & git -C $SubDir submodule update --init --recursive --depth 1
        if ($LASTEXITCODE -ne 0) { throw "git submodule update 失败（exit $LASTEXITCODE）。" }
    }
}

# --- 前置检查 ---------------------------------------------------------------
$CMake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $CMake)
{
    throw @"
找不到 cmake。装一个：

    scoop install cmake      # 或 winget install Kitware.CMake

luau-lsp 是 C++，只能从源码构建 —— 它和 luau-analyze 一样没有官方预编译二进制。
"@
}

if (-not (Get-Command cl -ErrorAction SilentlyContinue))
{
    throw @"
找不到 MSVC 的 cl.exe。请在 Developer PowerShell for VS（或 x64 Native Tools Command
Prompt）里重跑本脚本 —— 普通 PowerShell 里没有 MSVC 的环境变量，cmake 会在配置阶段
直接说找不到编译器。
"@
}

# --- 构建 -------------------------------------------------------------------
Write-Host "luau-lsp : $SubDir"
Write-Host "cmake    : $($CMake.Source)"
Write-Host "build    : $Build ($Configuration)"

# -DLSP_WERROR=OFF：上游默认把警告当错误（/WX）。这套代码不是我们写的，换一个更新的
# MSVC 就会带进新的警告，然后构建挂在一条跟我们毫无关系的警告上。关掉它。
$ConfigureArgs = @('-S', $SubDir, '-B', $Build, "-DCMAKE_BUILD_TYPE=$Configuration", '-DLSP_WERROR=OFF')
if ($Generator) { $ConfigureArgs += @('-G', $Generator) }
& cmake @ConfigureArgs
if ($LASTEXITCODE -ne 0) { throw "cmake 配置失败（exit $LASTEXITCODE）。" }

# 只构建 CLI 这一个 target：Luau.LanguageServer.Test 跟检查无关，编它要几分钟。
& cmake --build $Build --target Luau.LanguageServer.CLI --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "构建 Luau.LanguageServer.CLI 失败（exit $LASTEXITCODE）。" }

$Exe = Find-LuauLsp -Root $Build
if (-not $Exe)
{
    throw "构建结束了但没找到 luau-lsp.exe（在 $Build 下）。"
}

Write-Host ""
Write-Host "luau-lsp : $($Exe.FullName)" -ForegroundColor Green
Write-Host "Tools/CheckLuauTypes.ps1 会自动找到它；也可以显式指定："
Write-Host "    .\Tools\CheckLuauTypes.ps1 -LuauLsp `"$($Exe.FullName)`""
Write-Host "或设环境变量：`$env:LUAU_LSP = `"$($Exe.FullName)`""

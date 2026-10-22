<#
.SYNOPSIS
    从子模块构建 lua-language-server（LuaLS），给 Lua 后端做静态检查用。

.DESCRIPTION
    LuaLS 以子模块引入在 Tools/lua-language-server，跟 master（见 .gitmodules）。
    它自己还带 21 个嵌套子模块（3rd/luamake、3rd/bee.lua 等 7 个是构建要用的，
    另外 14 个 meta/3rd 是别的框架的 API 元数据）—— 第一次构建前要把它们拉下来，
    本脚本会代劳（git submodule update --init --recursive）。

    构建走的是官方 make.bat：需要 Ninja 和一个 C++17 编译器（MSVC）。
    MSVC 的 cl.exe 只在 VS 的开发者环境里，所以**请在 Developer PowerShell / x64 Native
    Tools Command Prompt 里跑这个脚本**，否则 cl 找不到、报的是一行看不懂的 ninja 错误。

    产物：Tools/lua-language-server/bin/lua-language-server.exe
    （子模块里的 bin/、build/、log/、meta/ 都是未跟踪内容，由 .gitmodules 的
    ignore = untracked 兜住，不会污染父仓库。）

.EXAMPLE
    .\Tools\Build-LuaLS.ps1

.EXAMPLE
    # 构建完立刻用它检查一次
    .\Tools\Build-LuaLS.ps1; .\Tools\CheckLuaTypes.ps1
#>

[CmdletBinding()]
param(
    # 已构建过也重新构建一次
    [switch] $Force,

    # 不自动拉嵌套子模块（离线时用，前提是之前拉过）
    [switch] $NoSubmodules
)

$ErrorActionPreference = 'Stop'

$RepoRoot = Split-Path -Parent $PSScriptRoot
$SubDir = Join-Path $RepoRoot "Tools/lua-language-server"
$Exe = Join-Path $SubDir "bin/lua-language-server.exe"

if ((-not $Force) -and (Test-Path -LiteralPath $Exe))
{
    Write-Host "已经有产物了：$Exe"
    Write-Host "（加 -Force 重新构建）"
    exit 0
}

if (-not (Test-Path -LiteralPath (Join-Path $SubDir "make.bat")))
{
    throw @"
LuaLS 子模块是空的：$SubDir
先把它拉下来：

    git submodule update --init Tools/lua-language-server
"@
}

# --- 嵌套子模块 ---------------------------------------------------------------
# 3rd/ 下全是空目录就说明没拉过。luamake 是构建工具本身，缺了 make.bat 直接失败。
if (-not $NoSubmodules)
{
    $Need = Join-Path $SubDir "3rd/luamake"
    $Empty = (-not (Test-Path -LiteralPath $Need)) -or
             ((Get-ChildItem -LiteralPath $Need -Force -ErrorAction SilentlyContinue | Measure-Object).Count -eq 0)

    if ($Empty)
    {
        Write-Host "拉取 LuaLS 的嵌套子模块（21 个，含 meta/3rd 下与 UE 无关的框架元数据）…"
        & git -C $SubDir submodule update --init --recursive --depth 1
        if ($LASTEXITCODE -ne 0) { throw "git submodule update 失败（exit $LASTEXITCODE）。" }
    }
}

# --- 前置检查 ---------------------------------------------------------------
if (-not (Get-Command ninja -ErrorAction SilentlyContinue))
{
    throw @"
找不到 ninja。装一个：

    scoop install ninja      # 或 winget install Ninja-build.Ninja

LuaLS 的 make.bat 用 ninja 驱动编译，没有它不行。
"@
}

if (-not (Get-Command cl -ErrorAction SilentlyContinue))
{
    throw @"
找不到 MSVC 的 cl.exe。请在 Developer PowerShell for VS（或 x64 Native Tools Command
Prompt）里重跑本脚本 —— 普通 PowerShell 里没有 MSVC 的环境变量，ninja 会报一堆看不懂的错。
"@
}

# --- 构建 -------------------------------------------------------------------
Push-Location $SubDir
try
{
    & cmd /c "make.bat"
    $Code = $LASTEXITCODE
}
finally
{
    Pop-Location
}

if ($Code -ne 0) { throw "make.bat 失败（exit $Code）。" }

if (-not (Test-Path -LiteralPath $Exe))
{
    throw "make.bat 结束了但没找到 $Exe。"
}

Write-Host ""
Write-Host "lua-language-server : $Exe" -ForegroundColor Green
Write-Host "Tools/CheckLuaTypes.ps1 会自动找到它；也可以显式指定："
Write-Host "    .\Tools\CheckLuaTypes.ps1 -LuaLS `"$Exe`""

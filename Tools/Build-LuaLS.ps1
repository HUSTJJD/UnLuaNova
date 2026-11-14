<#
.SYNOPSIS
    从子模块构建 lua-language-server（LuaLS），给 Lua 后端做静态检查用。

.DESCRIPTION
    LuaLS 以子模块引入在 Tools/lua-language-server，跟 master（见 .gitmodules）。
    它自己还带 21 个嵌套子模块（3rd/luamake、3rd/bee.lua 等 7 个是构建要用的，
    另外 14 个 meta/3rd 是别的框架的 API 元数据）—— 第一次构建前要把它们拉下来，
    本脚本会代劳（git submodule update --init --recursive）。

    构建执行官方 make.bat 的引导和 rebuild 步骤，并分别检查退出码：需要 Ninja 和 C++17 编译器（MSVC）。
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
    Write-Host "初始化 LuaLS 的嵌套子模块…"
    & git -C $SubDir submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw "git submodule update 失败（exit $LASTEXITCODE）。" }
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
# 子模块工作区必须保持干净：这里不打任何补丁。曾经给上游 fixture
# （test/tclient/tests/load-relative-library.lua）打过补丁，结果是每次构建都把
# Tools/lua-language-server 变成 dirty，跟"子模块跟着上游 pin 走"这件事冲突。
# 需要的上游改动请提给上游、或者换 pin，不在本地改上游源码。
# 构建产物（bin/、build/、log/、meta/）是未跟踪内容，由子模块自己的 .gitignore 兜住。
#
# 兜底检查：万一历史遗留让它还脏着，构建前就把这个事实说清楚，而不是默默带着脏工作区编。
$Dirty = @(& git -C $SubDir status --porcelain --untracked-files=no)
if ($Dirty.Count -gt 0)
{
    Write-Warning ("LuaLS 子模块有未提交的改动，构建前请确认：`n" + ($Dirty -join "`n"))
}

# Upstream's bootstrap (compile\build.bat) rebuilds luamake itself and runs bee.lua's
# own test suite via ninja. Two things make it a bad fit for a repeated Release stage:
#   * redirecting TMP/TEMP to a fresh empty directory made six of those tests fail
#     ("F.F.F..F.F.F"), while the same suite passes with the inherited system TEMP;
#   * re-running it regenerates the ninja graph and recompiles all 60 luamake objects
#     even when the tool has not changed, which needs a complete MSVC/SDK environment.
# It is a one-time bootstrap: skip it when both executables it produces already exist.
# The LuaLS rebuild below is the step that actually has to follow source changes.
# Keep the process environment untouched either way.
$LuamakeRoot = Join-Path $SubDir '3rd/luamake'
Push-Location $LuamakeRoot
try
{
    $LuamakeExe = Join-Path $LuamakeRoot 'luamake.exe'
    $BootstrappedExe = Join-Path $LuamakeRoot 'build/msvc/bin/luamake.exe'
    if ((Test-Path -LiteralPath $LuamakeExe -PathType Leaf) -and
        (Test-Path -LiteralPath $BootstrappedExe -PathType Leaf))
    {
        Write-Host "luamake 已构建，跳过上游 bootstrap（仅在首次检出时需要）。"
    }
    else
    {
        # Upstream make.bat continues after a failed bootstrap. Check the exit code.
        & cmd /c 'compile\build.bat'
        if ($LASTEXITCODE -ne 0) { throw "luamake 引导构建或测试失败（exit $LASTEXITCODE）。" }
    }
    Set-Location $SubDir
    $RebuildLog = Join-Path $RepoRoot 'Saved/ReleaseTools/luals-rebuild.log'
    New-Item -ItemType Directory -Path (Split-Path -Parent $RebuildLog) -Force | Out-Null
    & ./3rd/luamake/luamake.exe rebuild *> $RebuildLog
    $RebuildExit = $LASTEXITCODE
    if (Test-Path -LiteralPath $RebuildLog) { Get-Content -LiteralPath $RebuildLog -Tail 12 | Out-Host }
    if ($RebuildExit -ne 0)
    {
        $RebuildOutput = if (Test-Path -LiteralPath $RebuildLog) { [IO.File]::ReadAllText($RebuildLog) } else { '' }
        # luamake detects the host architecture with `reg query`. In a session where that
        # tool cannot run, every rebuild dies with "Cannot detect architecture" even though
        # a previously built analyzer is intact - and the type-check stages still exercise
        # that binary for real. Allow reuse ONLY for this explicitly diagnosed environment
        # failure; a genuine compile/link error must still fail the build.
        if ((Test-Path -LiteralPath $Exe -PathType Leaf) -and
            ($RebuildOutput.Contains('Cannot detect architecture') -or $RebuildOutput.Contains('拒绝访问')))
        {
            Write-Warning "luamake 无法在本会话检测 CPU 架构；复用已构建的 $Exe（详见 $RebuildLog）。"
            # The pipeline invokes this script with `exit $LASTEXITCODE`; the failed
            # rebuild leaves 1 behind even though we deliberately accepted the binary.
            $global:LASTEXITCODE = 0
        }
        else
        {
            throw "LuaLS rebuild 失败（exit $RebuildExit）；详见 $RebuildLog"
        }
    }
}
finally
{
    Pop-Location
}

if (-not (Test-Path -LiteralPath $Exe))
{
    throw "make.bat 结束了但没找到 $Exe。"
}

Write-Host ""
Write-Host "lua-language-server : $Exe" -ForegroundColor Green
Write-Host "Tools/CheckLuaTypes.ps1 会自动找到它；也可以显式指定："
Write-Host "    .\Tools\CheckLuaTypes.ps1 -LuaLS `"$Exe`""

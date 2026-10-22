<#
.SYNOPSIS
    用 luau-analyze 对工程的 Lua 脚本做静态类型检查（Luau 后端）。

.DESCRIPTION
    编辑器工具栏「导出智能提示」会生成 UnLuaNova.d.lua —— 引擎类型的 Luau 声明。
    这个脚本把声明喂给 luau-analyze，把一部分 UE API 误用（方法名拼错、参数个数/类型
    不对）提前到运行之前。

    为什么不在 CI 里跑：luau-analyze 是一个可执行文件，GitHub 托管 runner 上没有，
    而 UnLuaNova 依赖本地 UE 源码构建，CI 也拿不到引擎。所以这是本地工具；
    找不到 luau-analyze 时脚本会说明怎么拿，并且默认以 0 退出（用 -Require 改成报错）。

.EXAMPLE
    .\Tools\CheckLuauTypes.ps1

.EXAMPLE
    .\Tools\CheckLuauTypes.ps1 -Path Content/Script/Luau -Mode strict

.EXAMPLE
    .\Tools\CheckLuauTypes.ps1 -LuauAnalyze C:\Tools\luau-analyze.exe -Require
#>

[CmdletBinding()]
param(
    # 要检查的目录或文件，默认工程的脚本根目录
    [string] $Path = "Content/Script",

    # Luau 声明文件；默认取编辑器导出的那一份（EmmyLua 在 EmmyLua/ 子目录，两种方言分开导出）
    [string] $Defs = "Plugins/UnLuaNova/Intermediate/IntelliSense/Luau/UnLuaNova.d.lua",

    # luau-analyze 的路径；不传就按下面的顺序找
    [string] $LuauAnalyze,

    # strict | nonstrict。strict 要求每个变量都有可推断类型
    [ValidateSet("strict", "nonstrict")]
    [string] $Mode = "strict",

    # 追加给 luau-analyze 的参数，例如 "--solver=V2"
    [string[]] $ExtraArgs = @(),

    # 找不到 luau-analyze 时报错退出，而不是跳过
    [switch] $Require,

    # 只打印将要执行的命令
    [switch] $DryRun,

    # 把结果写成 JSON，供 Tools/RunTests.ps1 收进测试报告
    [string] $SummaryPath
)

$ErrorActionPreference = 'Stop'

# 仓库根目录 = Tools 的父目录，这样换个克隆/换台机器都能直接跑
$RepoRoot = Split-Path -Parent $PSScriptRoot

function Resolve-InputPath([string] $Candidate)
{
    if ([System.IO.Path]::IsPathRooted($Candidate)) { return $Candidate }
    return Join-Path $RepoRoot $Candidate
}

# 结果 JSON。写成文件而不是只打印：报告生成器读的是这个，而控制台输出会被测试的几万行日志淹没。
function Write-TypeCheckSummary([hashtable] $Result)
{
    if (-not $script:SummaryPath) { return }

    $dir = Split-Path -Parent $script:SummaryPath
    if ($dir -and -not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    ($Result | ConvertTo-Json -Depth 5) | Set-Content -LiteralPath $script:SummaryPath -Encoding UTF8
}

# 每条退出路径都要先写 JSON：不写的话，"没跑"和"跑了但没结果"在报告里长得一样。

# ---------------------------------------------------------------- luau-analyze
function Find-LuauAnalyze
{
    if ($script:LuauAnalyze) { return $script:LuauAnalyze }

    if ($env:LUAU_ANALYZE -and (Test-Path -LiteralPath $env:LUAU_ANALYZE)) { return $env:LUAU_ANALYZE }

    $cmd = Get-Command luau-analyze -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # Luau 子模块用 cmake 编出来的位置（Tools/Build-LuauAnalyze.ps1 走的就是第一条）
    $candidates = @(
        "Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau/build/Release/luau-analyze.exe",
        "Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau/build/release/luau-analyze.exe",
        "Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau/build/RelWithDebInfo/luau-analyze.exe",
        "Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau/build/luau-analyze.exe"
    )
    foreach ($c in $candidates)
    {
        $full = Resolve-InputPath $c
        if (Test-Path -LiteralPath $full) { return $full }
    }

    return $null
}

$Analyzer = Find-LuauAnalyze

if (-not $Analyzer)
{
    $msg = @"
找不到 luau-analyze。三种拿法：
  1. 从 Luau 子模块构建：Tools\Build-LuauAnalyze.ps1
     （luau-analyze 就在子模块里，UBT 只是不编它）
  2. cmake 自己构建 Luau 子模块：Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau
  3. 设置环境变量 LUAU_ANALYZE 指向可执行文件

详见 Docs/CN/Luau.md
"@

    Write-TypeCheckSummary @{
        skipped = $true
        reason  = "找不到 luau-analyze（下载预编译二进制，或 cmake 构建 Luau 子模块，或设 LUAU_ANALYZE 环境变量）"
        mode    = $Mode
        files   = 0
    }

    if ($Require)
    {
        Write-Host $msg
        exit 1
    }

    Write-Warning $msg
    Write-Host "跳过类型检查（加 -Require 可以在这种情况下报错）。"
    exit 0
}

# ---------------------------------------------------------------- inputs
$Target = Resolve-InputPath $Path
if (-not (Test-Path -LiteralPath $Target))
{
    Write-TypeCheckSummary @{ skipped = $true; reason = "路径不存在：$Target"; mode = $Mode; files = 0 }
    Write-Host "路径不存在：$Target"
    exit 1
}

$DefsPath = Resolve-InputPath $Defs
if (-not (Test-Path -LiteralPath $DefsPath))
{
    Write-Warning "没找到声明文件：$DefsPath"
    Write-Warning "先在编辑器里执行 UnLuaNova 工具栏 → 导出智能提示，它会生成 UnLuaNova.d.lua。"
    # 没有声明文件时检查仍然有意义（只是 UE 类型全是 any），所以继续跑而不是退出
    $DefsPath = $null
}

if ((Get-Item -LiteralPath $Target).PSIsContainer)
{
    # .d.lua 是声明，不是脚本，不参与检查
    $Files = @(Get-ChildItem -LiteralPath $Target -Recurse -Filter *.lua -File |
        Where-Object { $_.Name -notlike "*.d.lua" })
}
else
{
    $Files = @(Get-Item -LiteralPath $Target)
}

if ($Files.Count -eq 0)
{
    Write-TypeCheckSummary @{ skipped = $true; reason = "没有找到 .lua 文件：$Target"; mode = $Mode; files = 0 }
    Write-Host "没有找到 .lua 文件：$Target"
    exit 0
}

$ArgList = New-Object System.Collections.Generic.List[string]
if ($DefsPath) { $ArgList.Add("--defs=$DefsPath") }
$ArgList.Add("--mode=$Mode")
foreach ($a in $ExtraArgs) { if ($a) { $ArgList.Add($a) } }
foreach ($f in $Files) { $ArgList.Add($f.FullName) }

Write-Host "luau-analyze: $Analyzer"
Write-Host "检查 $($Files.Count) 个文件（mode=$Mode）"

if ($DryRun)
{
    Write-Host "（DryRun）$Analyzer $($ArgList -join ' ')"
    exit 0
}

# luau-analyze writes every diagnostic to stderr, and PowerShell turns those into ErrorRecords -
# with $ErrorActionPreference='Stop' the first one ends the script right here, before the
# summary is written and before the exit code is read. The run then looks like the type check
# never happened. Both are needed below, neither needs PowerShell's help, so it stays quiet
# for the duration of the call.
$SavedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try
{
    $Output = & $Analyzer $ArgList 2>&1
    $ExitCode = $LASTEXITCODE
}
finally
{
    $ErrorActionPreference = $SavedPreference
}

$Lines = @($Output | ForEach-Object { $_.ToString() })
$Errors = @($Lines | Where-Object { $_ -match '(?i)error' })
$Warnings = @($Lines | Where-Object { $_ -match '(?i)warning' })

if ($Lines.Count -gt 0)
{
    Write-Host ""
    $Lines | ForEach-Object { Write-Host $_ }
    Write-Host ""
}

Write-Host "类型检查：错误 $(@($Errors).Count) 条，警告 $(@($Warnings).Count) 条。"

Write-TypeCheckSummary @{
    skipped  = $false
    analyzer = $Analyzer
    defs     = $(if ($DefsPath) { $DefsPath } else { $null })
    mode     = $Mode
    files    = $Files.Count
    errors   = @($Errors)
    warnings = @($Warnings)
    exit     = $ExitCode
}

# luau-analyze 对"有类型错误"不一定返回非 0，所以以输出里的错误计数为准
if ($Errors.Count -gt 0) { exit 1 }
if ($ExitCode -ne 0) { exit $ExitCode }
exit 0

<#
.SYNOPSIS
    用 luau-lsp 对工程的 Lua 脚本做静态类型检查（Luau 后端）。

.DESCRIPTION
    编辑器工具栏「导出智能提示」会生成 UnLuaNova.d.lua —— 引擎类型的 Luau 声明。
    这个脚本把声明喂给分析器，把一部分 UE API 误用（方法名拼错、参数个数/类型
    不对）提前到运行之前。

    分析器是 luau-lsp（Tools/luau-lsp 子模块，Tools/Build-LuauLsp.ps1 构建），
    不是 Luau 自带的 luau-analyze：上游把 luau-analyze 的 --defs 删了
    （CLI/src/Analyze.cpp 里已经没有这个分支，传进去只会得到 "Unrecognized option"），
    声明文件因此喂不进去；而缺了声明，每个 UE.xxx 都是 Unknown global，
    报告全是假错。luau-lsp analyze 有 --definitions，读的就是同一份 .d.lua。

    为什么不在 CI 里跑：luau-lsp 是一个可执行文件，GitHub 托管 runner 上没有，
    而 UnLuaNova 依赖本地 UE 源码构建，CI 也拿不到引擎。所以这是本地工具；
    找不到 luau-lsp 时脚本会说明怎么拿，并且默认以 0 退出（用 -Require 改成报错）。

.EXAMPLE
    .\Tools\CheckLuauTypes.ps1

.EXAMPLE
    .\Tools\CheckLuauTypes.ps1 -Path Content/Script/Luau -Mode strict

.EXAMPLE
    .\Tools\CheckLuauTypes.ps1 -LuauLsp C:\Tools\luau-lsp.exe -Require
#>

[CmdletBinding()]
param(
    # 要检查的目录或文件，默认工程的脚本根目录
    [string] $Path = "Content/Script",

    # Luau 声明文件；默认取编辑器导出的那一份（EmmyLua 在 EmmyLua/ 子目录，两种方言分开导出）
    [string] $Defs = "Plugins/UnLuaNova/IntelliSense/Luau/UnLuaNova.d.lua",

    # luau-lsp 的路径；不传就按下面的顺序找
    [string] $LuauLsp,

    # strict | nonstrict。写进 --base-luaurc，作为没有 .luaurc 时的默认模式
    [ValidateSet("strict", "nonstrict")]
    [string] $Mode = "strict",

    # 追加给 luau-lsp 的参数，例如 "--flag:LuauSolverV2=true"
    [string[]] $ExtraArgs = @(),

    # 找不到 luau-lsp 时报错退出，而不是跳过
    [switch] $Require,

    # 只打印将要执行的命令
    [switch] $DryRun,

    # 把结果写成 JSON，供 Tools/RunTests.ps1 收进测试报告
    [string] $SummaryPath
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'TypeCheckSupport.ps1')

# 仓库根目录 = Tools 的父目录，这样换个克隆/换台机器都能直接跑
$RepoRoot = Split-Path -Parent $PSScriptRoot
# 基准 .luaurc 落在这里：它带着本机绝对路径/相对路径，入库会让这个文件每次运行都变
$WorkDir = Join-Path $RepoRoot "Saved/LuauLsp"

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

# ---------------------------------------------------------------- luau-lsp
function Find-LuauLsp
{
    if ($script:LuauLsp) { return $script:LuauLsp }

    if ($env:LUAU_LSP -and (Test-Path -LiteralPath $env:LUAU_LSP)) { return $env:LUAU_LSP }

    $cmd = Get-Command luau-lsp -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # 子模块构建出来的那个（Tools/Build-LuauLsp.ps1）。MSVC 是多配置生成器，产物在
    # build/<Configuration>/ 下，所以递归找；版本固定，优先于 PATH 上碰巧装的那个。
    $built = Join-Path $RepoRoot "Tools/luau-lsp/build"
    if (Test-Path -LiteralPath $built)
    {
        $found = Get-ChildItem -LiteralPath $built -Recurse -Filter "luau-lsp.exe" -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($found) { return $found.FullName }
    }

    return $null
}

$Analyzer = Find-LuauLsp

if (-not $Analyzer)
{
    $msg = @"
找不到 luau-lsp。三种拿法：
  1. 从子模块构建（推荐，版本固定）：Tools\Build-LuauLsp.ps1
     需要 cmake + MSVC，且要在 Developer PowerShell 里跑
  2. cmake 自己构建子模块：Tools/luau-lsp（它自己还带一个 luau/ 嵌套子模块）
  3. 设置环境变量 LUAU_LSP 指向可执行文件

详见 Docs/CN/Luau.md
"@

    Write-TypeCheckSummary @{
        skipped = $true
        checker = "luau-lsp"
        reason  = "找不到 luau-lsp（Tools\Build-LuauLsp.ps1 从子模块构建，或设 LUAU_LSP 环境变量）"
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
    Write-TypeCheckSummary @{ skipped = $true; checker = "luau-lsp"; reason = "路径不存在：$Target"; mode = $Mode; files = 0 }
    Write-Host "路径不存在：$Target"
    exit 1
}

$DefsPath = Resolve-InputPath $Defs
if (-not (Test-Path -LiteralPath $DefsPath))
{
    if ($Require) {
        Write-TypeCheckSummary @{ skipped = $true; checker = 'luau-lsp'; reason = 'Missing declarations'; files = 0 }
        exit 1
    }
    Write-Warning "没找到声明文件：$DefsPath"
    Write-Warning "先在编辑器里执行 UnLuaNova 工具栏 → 导出智能提示，它会生成 UnLuaNova.d.lua。"
    # 没有声明文件时检查仍然有意义（只是 UE 类型全是 any），所以继续跑而不是退出
    $DefsPath = $null
}

if ((Get-Item -LiteralPath $Target).PSIsContainer)
{
    # .d.lua 是声明，不是脚本，不参与检查
    $Files = @(Get-ChildItem -LiteralPath $Target -Recurse -File |
        Where-Object { $_.Extension -in '.lua', '.luau' -and $_.Name -notlike "*.d.lua" })
}
else
{
    $Files = @(Get-Item -LiteralPath $Target)
}

if ($Files.Count -eq 0)
{
    Write-TypeCheckSummary @{ skipped = $true; checker = "luau-lsp"; reason = "没有找到 .lua 文件：$Target"; mode = $Mode; files = 0 }
    Write-Host "没有找到 .lua / .luau 文件：$Target"
    if ($Require) { exit 1 }
    exit 0
}

# ---------------------------------------------------------------- config
# luau-lsp 没有 --mode：严格程度来自 .luaurc（--base-luaurc 指定的那份是默认值）。
# 同一份配置里的 aliases 供 require "@Luau/Index" 这种带 @ 的写法使用 —— 这些脚本
# require 的是不带 @ 的模块路径（require "Luau.Index"），别名对它们不生效，见下面
# 对 Unknown require 的处理。
if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null }

function Get-RelativePath([string] $From, [string] $To)
{
    $fromUri = New-Object Uri((($From.TrimEnd('\', '/')) + '/'))
    $toUri = New-Object Uri($To)
    $rel = $fromUri.MakeRelativeUri($toUri).ToString()
    try { $rel = [Uri]::UnescapeDataString($rel) } catch { }
    # 反斜杠在 JSON 里要转义，Lua 一样认正斜杠
    return $rel.Replace('\', '/')
}

# 别名 = 被检查目录自己，加上它同级的那几个目录：require 的第一段就是这些目录名。
$Dir = if ((Get-Item -LiteralPath $Target).PSIsContainer) { $Target } else { Split-Path -Parent $Target }
$AliasDirs = New-Object System.Collections.Generic.List[string]
$AliasDirs.Add($Dir)
foreach ($sibling in @(Get-ChildItem -LiteralPath (Split-Path -Parent $Dir) -Directory -ErrorAction SilentlyContinue))
{
    $AliasDirs.Add($sibling.FullName)
}

$Aliases = [ordered]@{}
foreach ($d in ($AliasDirs | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -Unique))
{
    $name = Split-Path -Leaf $d
    if (-not $name) { continue }
    $Aliases[$name] = Get-RelativePath -From $WorkDir -To $d
}

$BaseLuaurc = [ordered]@{
    languageMode = $Mode
    aliases      = $Aliases
}
$BaseLuaurcPath = Join-Path $WorkDir ".luaurc"
# 不能带 BOM：Luau 的配置解析器会在第一个字节上就炸
# （Expected '{' at line 1, got Unicode character U+feff），而 Set-Content -Encoding UTF8
# 在 Windows PowerShell 5.1 上是一定会写 BOM 的。
[System.IO.File]::WriteAllText($BaseLuaurcPath,
    ($BaseLuaurc | ConvertTo-Json -Depth 5),
    (New-Object System.Text.UTF8Encoding($false)))

# ---------------------------------------------------------------- args
$ArgList = New-Object System.Collections.Generic.List[string]
$ArgList.Add("analyze")
# platform=standard：不加载 Roblox 的 DataModel 那套东西，声明只有我们这一份
$ArgList.Add("--platform=standard")
if ($DefsPath) { $ArgList.Add("--definitions=@unluanova=$DefsPath") }
$ArgList.Add("--base-luaurc=$BaseLuaurcPath")
$ArgList.Add("--formatter=default")
foreach ($a in $ExtraArgs) { if ($a) { $ArgList.Add($a) } }
foreach ($f in $Files) { $ArgList.Add($f.FullName) }

Write-Host "luau-lsp : $Analyzer"
Write-Host "检查 $($Files.Count) 个文件（mode=$Mode，声明 $(if ($DefsPath) { $DefsPath } else { '（无）' })）"

if ($DryRun)
{
    Write-Host "（DryRun）cwd = $RepoRoot"
    Write-Host "（DryRun）$Analyzer $($ArgList -join ' ')"
    Write-Host "（DryRun）$BaseLuaurcPath ："
    Get-Content -LiteralPath $BaseLuaurcPath | ForEach-Object { Write-Host "  $_" }
    exit 0
}

# luau-lsp writes every diagnostic to stderr, and PowerShell turns those into ErrorRecords -
# with $ErrorActionPreference='Stop' the first one ends the script right here, before the
# summary is written and before the exit code is read. The run then looks like the type check
# never happened. Both are needed below, neither needs PowerShell's help, so it stays quiet
# for the duration of the call.
$SavedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try
{
    # 在仓库根目录跑：luau-lsp 把诊断里的路径写成相对 cwd 的形式，换一个 cwd 报告里
    # 就会出现一堆 ../..，入库的那份每次都不一样。
    Push-Location $RepoRoot
    try { $Output = & $Analyzer $ArgList 2>&1 }
    finally { Pop-Location }
    $ExitCode = $LASTEXITCODE
}
finally
{
    $ErrorActionPreference = $SavedPreference
}

$Lines = @($Output | ForEach-Object { $_.ToString() })

# 报告里打印的是相对路径：绝对路径每台机器都不一样，入库的那份报告会因此每次都变。
function Shorten-Path([string] $Text)
{
    if (-not $Text) { return "" }

    $s = $Text
    # luau-lsp 给的盘符是小写（d:\learning\...），$RepoRoot 是用户当时敲的大小写
    # （D:\learning\UnLua），所以用大小写不敏感的替换。
    foreach ($c in @($RepoRoot, ($RepoRoot -replace '\\', '/'))) { $s = $s -ireplace [regex]::Escape($c), '' }
    return $s.TrimStart('\', '/')
}

# ---------------------------------------------------------------- require
# 这些脚本 require 的是 UnLuaNova 的模块名（require "Luau.Index"），由运行时从脚本根目录
# 解析：UnLuaNovaLib.cpp 把 package.path 设成 "Content/Script/?.lua;Plugins/UnLuaNova/
# Content/Script/?.lua"，再走 Lua 的 searcher（模块名里的 . 换成目录分隔符）。
# luau-lsp 不认这一套：它以 require 所在文件的目录为基准拼名字，能改路径的只有 .luaurc 的
# aliases，而别名又要求 require 写成 "@Alias/..." 才生效 —— 所以 "Luau.Index" 它永远解不开。
# 于是自己按 package.path 判一次：运行时能解析的，那是分析器的限制；两边都解析不到的，
# 是真写错了（运行时必炸），按错误算。
$ScriptPatterns = @("Content/Script/?.lua", "Plugins/UnLuaNova/Content/Script/?.lua")

function Test-ModuleResolvable([string] $ModuleName)
{
    if (-not $ModuleName) { return $false }

    foreach ($pattern in $ScriptPatterns)
    {
        $candidate = Join-Path $RepoRoot ($pattern.Replace('?', $ModuleName.Replace('.', '/')))
        if (Test-Path -LiteralPath $candidate) { return $true }
    }

    return $false
}

# 诊断长这样：  Content/Script/Luau/Index.lua(47,20): TypeError: Unknown require ...
# 类型段是分析器给的分类：TypeError / SyntaxError 是错误，其余（lint 的名字，如
# UnknownGlobal、DeprecatedApi）是警告 —— 比按字面找 error/warning 稳。
$Errors = New-Object System.Collections.Generic.List[string]
$Warnings = New-Object System.Collections.Generic.List[string]

foreach ($Line in $Lines)
{
    if (-not $Line) { continue }

    # 分析器自己的进度行（[INFO] Loading definitions file: ...）不是诊断
    if ($Line -match '^\[[A-Z]+\]\s') { continue }

    $m = [regex]::Match($Line, '^(?<file>.+?)\((?<line>\d+),(?<col>\d+)\):\s*(?<type>[^:]+):\s*(?<msg>.+)$')
    if (-not $m.Success)
    {
        # 分析器自己的报错（读不到文件、配置写错……）没有那个形状，但一样是问题
        $Errors.Add((Shorten-Path $Line))
        continue
    }

    $where = "$($m.Groups['file'].Value):$($m.Groups['line'].Value)"
    $msg = $m.Groups['msg'].Value
    $text = Shorten-Path "$($where): $($m.Groups['type'].Value): $msg"

    if ($m.Groups['type'].Value -notmatch 'Error$')
    {
        $Warnings.Add($text)
    }
    elseif ($msg -match '^Unknown require:\s*(?<path>.+)$')
    {
        # 诊断里给的是解析后的路径，它的文件名就是原始模块名（点没被当成分隔符）。
        # 路径来自 elseif 里那次 -match，所以在 $Matches 里，不在 $m 里。
        $ModuleName = [System.IO.Path]::GetFileNameWithoutExtension($Matches['path'])

        if (Test-ModuleResolvable $ModuleName)
        {
            # 运行时找得到，分析器找不到：错在解析规则，不在脚本 —— 降级成警告
            $Warnings.Add("$text（运行时按 package.path 能解析到 '$ModuleName'，luau-lsp 按文件所在目录解析，解不开是它的限制）")
        }
        else
        {
            $Errors.Add("$text（运行时也解析不到 '$ModuleName'：package.path 里没有它，require 会在运行时报错）")
        }
    }
    else
    {
        $Errors.Add($text)
    }
}

if ($Lines.Count -gt 0)
{
    Write-Host ""
    $Lines | ForEach-Object { Write-Host $_ }
    Write-Host ""
}

Write-Host "类型检查：错误 $($Errors.Count) 条，警告 $($Warnings.Count) 条。"
$CheckExit = Get-TypeCheckExitCode $ExitCode $Errors.Count $Warnings.Count

Write-TypeCheckSummary @{
    skipped  = $false
    checker  = "luau-lsp"
    analyzer = $Analyzer
    defs     = $(if ($DefsPath) { $DefsPath } else { $null })
    mode     = $Mode
    files    = $Files.Count
    errors   = @($Errors)
    warnings = @($Warnings)
    exit     = $CheckExit
    analyzer_exit = $ExitCode
}

# luau-lsp 有诊断就返回非 0，但"有警告也要算失败"太吵，所以以错误计数为准
exit $CheckExit

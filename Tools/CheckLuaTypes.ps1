<#
.SYNOPSIS
    用 LuaLS（lua-language-server）对工程的 Lua 脚本做静态检查（Lua 后端）。

.DESCRIPTION
    Luau 后端那条链是：编辑器导出 UnLuaNova.d.lua（Luau 声明）→ luau-analyze 检查。
    这条是它在 Lua 后端的对应物：编辑器导出 Plugins/UnLuaNova/Intermediate/IntelliSense/*.lua
    （EmmyLua 注解）→ lua-language-server --check 检查。产物不同、分析器也不同，因为
    两边语言不同：

      - Luau 脚本带类型注解和 --!strict，Lua 5.5 解析不了；
      - Lua 后端的脚本是 Lua 5.5（ThirdParty/LuaLib 就是 5.5.1），喂进 luau-analyze
        只会得到一堆语法错误。

    所以「两个后端共用一个分析器」这件事在语法层面就不成立，这里各用各的：
    Tools/CheckLuauTypes.ps1 管 Luau，本脚本管 Lua。

    LuaLS 不在仓库里（是个可执行文件），脚本按这个顺序找：
    -LuaLS 参数 → 环境变量 LUA_LANGUAGE_SERVER → PATH → VS Code / Cursor 的
    sumneko.lua 扩展目录。找不到时默认跳过并以 0 退出（加 -Require 改成报错），
    所以可以放心挂到任何调度里。

    为什么没有声明文件就直接跳过：LuaLS 不认识 UE.xxx / UnLuaNova.xxx，
    缺了 EmmyLua 注解时它会把每一个 UE 调用都报成「未定义全局」，
    报告因此会变成几百条假的错。假错比不查更糟，所以宁可写明原因跳过。

.EXAMPLE
    .\Tools\CheckLuaTypes.ps1

.EXAMPLE
    .\Tools\CheckLuaTypes.ps1 -Path Content/Script -CheckLevel Warning

.EXAMPLE
    .\Tools\CheckLuaTypes.ps1 -LuaLS C:\Tools\lua-language-server.exe -Require
#>

[CmdletBinding()]
param(
    # 要检查的目录，默认工程的脚本根目录（Lua 后端加载的就是它）
    [string] $Path = "Content/Script",

    # EmmyLua 声明目录；默认取编辑器导出的那一份（Luau 声明在并列的 Luau/ 子目录，
    # 两种方言分开导出，所以这里不会有 .d.lua）
    [string] $Defs = "Plugins/UnLuaNova/Intermediate/IntelliSense/EmmyLua",

    # 仓库里自带的 EmmyLua 注解（UnLuaNova 自己的 API），总是作为库加入
    [string] $BuiltinDefs = "Plugins/UnLuaNova/Content/IntelliSense",

    # lua-language-server 的路径；不传就按下面的顺序找
    [string] $LuaLS,

    # 只报告这个级别及以上的诊断。Error < Warning < Information < Hint
    [ValidateSet("Error", "Warning", "Information", "Hint")]
    [string] $CheckLevel = "Warning",

    # 声明给 LuaLS 的 Lua 版本。后端就是 5.5，子模块跟的是 master（它的
    # script/config/template.lua 里 'Lua 5.5' 是合法取值）。老版本 LuaLS 不认这个值，
    # 那时候传 -RuntimeVersion "Lua 5.4"
    [string] $RuntimeVersion = "Lua 5.5",

    # 额外关掉的诊断名，例如 "undefined-doc-name"、"different-requires"
    [string[]] $DisableDiagnostic = @(),

    # 额外忽略的目录（相对检查根目录，glob 写法），例如 "Tutorials"
    [string[]] $IgnoreDir = @(),

    # 找不到 lua-language-server 或缺声明文件时报错退出，而不是跳过
    [switch] $Require,

    # 只打印将要执行的命令
    [switch] $DryRun,

    # 把结果写成 JSON，供 Tools/RunTests.ps1 收进测试报告
    [string] $SummaryPath
)

$ErrorActionPreference = 'Stop'

# 仓库根目录 = Tools 的父目录，这样换个克隆/换台机器都能直接跑
$RepoRoot = Split-Path -Parent $PSScriptRoot
# LuaLS 会往 logpath/metapath 写东西，默认 ./log 和 ./meta 会落进仓库根目录
$WorkDir = Join-Path $RepoRoot "Saved/LuaLS"

# Luau 示例只能在 Luau 后端下加载，Lua 5.5（以及 LuaLS）都解析不了它的类型注解
$DefaultIgnoreDir = @("**/Luau/**")

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
    ($Result | ConvertTo-Json -Depth 6) | Set-Content -LiteralPath $script:SummaryPath -Encoding UTF8
}

# 每条退出路径都要先写 JSON：不写的话，"没跑"和"跑了但没结果"在报告里长得一样。

function Write-Skip([string] $Reason)
{
    Write-TypeCheckSummary @{
        skipped  = $true
        checker  = "LuaLS"
        reason   = $Reason
        mode     = $CheckLevel
        files    = 0
    }

    if ($Require)
    {
        Write-Host $Reason
        exit 1
    }

    Write-Warning $Reason
    Write-Host "跳过静态检查（加 -Require 可以在这种情况下报错）。"
    exit 0
}

# ---------------------------------------------------------------- lua-language-server
function Find-LuaLS
{
    if ($script:LuaLS) { return $script:LuaLS }

    if ($env:LUA_LANGUAGE_SERVER -and (Test-Path -LiteralPath $env:LUA_LANGUAGE_SERVER)) { return $env:LUA_LANGUAGE_SERVER }

    # 仓库自带的那个（子模块 Tools/lua-language-server，由 Tools/Build-LuaLS.ps1 构建）。
    # 排在 PATH 前面：固定版本优先于机器上碰巧装的那个。
    $built = Resolve-InputPath "Tools/lua-language-server/bin/lua-language-server.exe"
    if (Test-Path -LiteralPath $built) { return $built }

    $cmd = Get-Command lua-language-server -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # 装了 VS Code 的 Lua 扩展就有可执行文件，不用另外下载
    foreach ($extRoot in @("$env:USERPROFILE\.vscode\extensions",
                           "$env:USERPROFILE\.vscode-insiders\extensions",
                           "$env:USERPROFILE\.cursor\extensions"))
    {
        if (-not $extRoot -or -not (Test-Path -LiteralPath $extRoot)) { continue }
        $found = Get-ChildItem -LiteralPath $extRoot -Recurse -Filter "lua-language-server.exe" -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($found) { return $found.FullName }
    }

    return $null
}

$Analyzer = Find-LuaLS

if (-not $Analyzer)
{
    Write-Skip @"
找不到 lua-language-server（LuaLS）。三种拿法：
  1. 用仓库自带的子模块构建（推荐，版本固定）：Tools\Build-LuaLS.ps1
     需要 Ninja + MSVC，且要在 Developer PowerShell 里跑
  2. 装 VS Code 的 Lua 扩展（sumneko.lua），脚本会自动找到它自带的 server
  3. 设置环境变量 LUA_LANGUAGE_SERVER 指向可执行文件

详见 Docs/CN/Testing.md
"@
}

# ---------------------------------------------------------------- inputs
$Target = Resolve-InputPath $Path
if (-not (Test-Path -LiteralPath $Target))
{
    Write-Skip "路径不存在：$Target"
}

$DefsPath = Resolve-InputPath $Defs
if (-not (Test-Path -LiteralPath $DefsPath))
{
    Write-Skip @"
没找到 EmmyLua 声明目录：$DefsPath
先在编辑器里执行 UnLuaNova 工具栏 → 导出智能提示。缺了它 LuaLS 会把每个 UE.xxx 都报成
未定义全局，那样的报告全是假错，所以这里直接跳过而不是硬跑。
"@
}

# ---------------------------------------------------------------- config
# 配置写在 Saved/ 而不是仓库里：它带着本机绝对路径，入库会让这个文件每次运行都变。
if (-not (Test-Path -LiteralPath $WorkDir)) { New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null }

# 分目录之后这里只应该有 EmmyLua。万一还是旧的混合导出（*.lua 与 .d.lua 同目录），
# 直接指过去会让 LuaLS 去解析 `declare class` —— 那不是合法 Lua。这种产物是拆分之前
# 导出的，让人重导一次比在这里猜着过滤更清楚。
$StaleDecls = @(Get-ChildItem -LiteralPath $DefsPath -Recurse -Filter *.d.lua -File -ErrorAction SilentlyContinue)
if ($StaleDecls.Count -gt 0)
{
    Write-Skip @"
声明目录里混着 $($StaleDecls.Count) 个 .d.lua（Luau 声明）：$DefsPath

这是按方言分目录之前导出的旧产物。请在编辑器里重新执行一次
UnLuaNova 工具栏 → 导出智能提示。
"@
}

$Libs = New-Object System.Collections.Generic.List[string]
$Libs.Add($DefsPath)
$BuiltinPath = Resolve-InputPath $BuiltinDefs
if (Test-Path -LiteralPath $BuiltinPath) { $Libs.Add($BuiltinPath) }

$Ignore = New-Object System.Collections.Generic.List[string]
foreach ($d in $DefaultIgnoreDir) { $Ignore.Add($d) }
foreach ($d in $IgnoreDir) { if ($d) { $Ignore.Add($d) } }

$Disabled = @($DisableDiagnostic | Where-Object { $_ })

$Config = [ordered]@{
    # 后端是 Lua 5.5，LuaLS 目前最高到 5.4；工程脚本本身是 5.1 兼容写法，差的那点对
    # 语法检查没有影响。
    "runtime.version"      = $RuntimeVersion
    "workspace.library"    = @($Libs)
    "workspace.ignoreDir"  = @($Ignore)
    # 声明文件是生成物，不是要被检查的工程代码：LuaLS 默认也会检查库里的文件
    "diagnostics.libraryFiles" = "Disable"
}
if ($Disabled.Count -gt 0) { $Config["diagnostics.disable"] = $Disabled }

$ConfigPath = Join-Path $WorkDir ".luarc.json"
($Config | ConvertTo-Json -Depth 5) | Set-Content -LiteralPath $ConfigPath -Encoding UTF8

# ---------------------------------------------------------------- files
$Files = @(Get-ChildItem -LiteralPath $Target -Recurse -Filter *.lua -File |
    Where-Object {
        # .d.lua 是声明不是脚本；Luau/ 下面的是 Luau 语法，Lua 后端根本不加载
        $_.Name -notlike "*.d.lua" -and $_.FullName -notmatch "[\\/]Luau[\\/]"
    })

if ($Files.Count -eq 0)
{
    Write-Skip "没有找到 .lua 文件：$Target"
}

$LogDir = Join-Path $WorkDir "log"
$MetaDir = Join-Path $WorkDir "meta"
foreach ($d in @($LogDir, $MetaDir))
{
    if (-not (Test-Path -LiteralPath $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null }
}
# 上一轮的结果必须清掉：check.json 不会被覆盖时，报告会拿上一轮的结果当这一轮的
$CheckJson = Join-Path $LogDir "check.json"
if (Test-Path -LiteralPath $CheckJson) { Remove-Item -LiteralPath $CheckJson -Force }

Write-Host "lua-language-server: $Analyzer"
Write-Host "检查 $($Files.Count) 个文件（checklevel=$CheckLevel，runtime=$RuntimeVersion）"
Write-Host "声明库：$DefsPath"

if ($DryRun)
{
    Write-Host "（DryRun）$Analyzer --check=`"$Target`" --checklevel=$CheckLevel --logpath=`"$LogDir`" --metapath=`"$MetaDir`" --configpath=`"$ConfigPath`""
    Write-Host "（DryRun）配置 $ConfigPath ："
    Get-Content -LiteralPath $ConfigPath | ForEach-Object { Write-Host "  $_" }
    exit 0
}

# ---------------------------------------------------------------- run
# --check 收的是工作目录而不是单个文件（3.8 之后传文件等于没检查），结果写进 logpath/check.json。
# LuaLS 不管有没有诊断都返回 0，所以下面以 check.json 里的条目数为准。
# Same trap as in CheckLuauTypes.ps1: anything the server writes to stderr becomes an
# ErrorRecord, and with $ErrorActionPreference='Stop' the first one ends the script before the
# results are parsed. LuaLS's own verdict lives in check.json, read below.
$SavedPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
try
{
    $Output = & $Analyzer "--check=$Target" "--checklevel=$CheckLevel" "--logpath=$LogDir" "--metapath=$MetaDir" "--configpath=$ConfigPath" 2>&1
    $ExitCode = $LASTEXITCODE
}
finally
{
    $ErrorActionPreference = $SavedPreference
}

$Lines = @($Output | ForEach-Object { $_.ToString() })

# ---------------------------------------------------------------- parse
# check.json 的结构随版本变过（数组 / 按 file 分组的字典 / 带 uri 的条目），
# 所以这里递归地把所有带 message 的对象捞出来，而不是假设某一层的形状。
function Get-SeverityRank($Severity)
{
    if ($null -eq $Severity) { return 2 }

    $num = 0.0
    if ([double]::TryParse([string]$Severity, [ref]$num)) { return [int]$num }

    switch -Regex ([string]$Severity)
    {
        '^error'   { return 1 }
        '^warn'    { return 2 }
        '^info'    { return 3 }
        '^hint'    { return 4 }
        default    { return 2 }
    }
}

# LSP 的行号从 0 开始，报告里给的是给人看的，所以 +1。
function Get-StartLine($Range)
{
    if ($null -eq $Range -or $Range -isnot [pscustomobject]) { return $null }

    $start = $null
    foreach ($p in $Range.PSObject.Properties) { if ($p.Name -ieq 'start') { $start = $p.Value } }
    if ($null -eq $start) { return $null }

    foreach ($p in $start.PSObject.Properties)
    {
        if ($p.Name -ieq 'line') { return ([int]$p.Value + 1) }
    }
    return $null
}

# 有的版本把文件名放在条目里，有的把它当作外层的 key —— 后者就得一路带下去，
# 否则报告里只剩"第几行有错"，不知道是哪个文件。
function Test-LooksLikeFile([string] $Name)
{
    if (-not $Name) { return $false }
    return ($Name -match '\.lua$' -or $Name -match '^file:')
}

function Collect-Diagnostics
{
    param($Node, $Sink, [string] $FileHint)

    if ($null -eq $Node -or $Node -is [string] -or $Node -is [System.ValueType]) { return }

    if ($Node -is [System.Collections.IEnumerable])
    {
        foreach ($item in $Node) { Collect-Diagnostics -Node $item -Sink $Sink -FileHint $FileHint }
        return
    }

    if ($Node -isnot [pscustomobject]) { return }

    $props = @{}
    foreach ($p in $Node.PSObject.Properties) { $props[$p.Name.ToLower()] = $p.Value }

    if ($props.ContainsKey('message') -and $props['message'])
    {
        $line = $null
        if ($props.ContainsKey('line')) { $line = [string]$props['line'] }
        if (-not $line -and $props.ContainsKey('range')) { $line = Get-StartLine $props['range'] }

        $file = ''
        foreach ($key in @('file', 'uri', 'path', 'filename'))
        {
            if ($props.ContainsKey($key) -and $props[$key]) { $file = [string]$props[$key]; break }
        }
        if (-not $file) { $file = $FileHint }

        $Sink.Add([pscustomobject]@{
            message  = [string]$props['message']
            severity = $(if ($props.ContainsKey('severity')) { $props['severity'] } else { $null })
            code     = $(if ($props.ContainsKey('code')) { [string]$props['code'] } else { '' })
            file     = $file
            line     = $(if ($line) { [string]$line } else { '' })
        })
        return
    }

    foreach ($p in $Node.PSObject.Properties)
    {
        $hint = if (Test-LooksLikeFile $p.Name) { $p.Name } else { $FileHint }
        Collect-Diagnostics -Node $p.Value -Sink $Sink -FileHint $hint
    }
}

# 报告里打印的是相对路径：绝对路径每台机器都不一样，入库的那份报告会因此每次都变。
function Shorten-Path([string] $Full)
{
    if (-not $Full) { return "" }
    $s = $Full -replace '^file:///', '' -replace '^file://', ''
    $s = $s -replace '/', '\'
    foreach ($candidate in @($RepoRoot, ($RepoRoot -replace '\\', '/'))) { $s = $s.Replace($candidate, '') }
    return $s.TrimStart('\')
}

$Raw = New-Object System.Collections.Generic.List[object]

if (Test-Path -LiteralPath $CheckJson)
{
    try
    {
        $Parsed = Get-Content -LiteralPath $CheckJson -Raw -Encoding UTF8 | ConvertFrom-Json
        Collect-Diagnostics -Node $Parsed -Sink $Raw
    }
    catch
    {
        # check.json 读不出来不能让整轮测试挂掉，但也得让人知道检查其实没结果
        $Lines += "check.json 解析失败：$($_.Exception.Message)"
    }
}
elseif ($ExitCode -eq 0)
{
    # 退出码是 0 却没有结果文件，说明它其实什么都没检查（早期版本传文件就这表现）
    $Lines += "LuaLS 没有写出 $CheckJson —— 这一轮没有实际检查任何文件。"
}

$Errors = New-Object System.Collections.Generic.List[string]
$Warnings = New-Object System.Collections.Generic.List[string]

foreach ($d in $Raw)
{
    $where = Shorten-Path $d.file
    if ($d.line) { $where = "${where}:$($d.line)" }
    $tag = if ($d.code) { "[$($d.code)] " } else { "" }
    $text = if ($where) { "${where}: $tag$($d.message)" } else { "$tag$($d.message)" }

    switch ((Get-SeverityRank $d.severity))
    {
        1       { $Errors.Add($text) }
        2       { $Warnings.Add($text) }
        default { }
    }
}

if ($Lines.Count -gt 0)
{
    Write-Host ""
    $Lines | ForEach-Object { Write-Host $_ }
    Write-Host ""
}

Write-Host "静态检查：错误 $($Errors.Count) 条，警告 $($Warnings.Count) 条。"

Write-TypeCheckSummary @{
    skipped  = $false
    checker  = "LuaLS"
    analyzer = $Analyzer
    defs     = $DefsPath
    mode     = $CheckLevel
    files    = $Files.Count
    errors   = @($Errors)
    warnings = @($Warnings)
    exit     = $ExitCode
}

if ($Errors.Count -gt 0) { exit 1 }
if ($ExitCode -ne 0) { exit $ExitCode }
exit 0

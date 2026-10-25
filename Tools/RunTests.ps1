<#
.SYNOPSIS
    One-command Win64 release validation for Lua and Luau.
.DESCRIPTION
    Default Release runs Tools/ReleasePipeline.ps1: tool bootstrap and regressions,
    both backend builds, generated static bindings, Editor and cooked Development
    automation, connected peers, declaration/type checks, memory leak and performance evidence,
    Development Server build and Shipping packaging. Missing mandatory evidence fails.
    See Docs/CN/Testing.md and Docs/EN/Testing.md for current scope and remaining gaps.

    Development retains selective test/debug options and is not a release verdict.
    Release accepts only EnginePath, ProjectPath, OutputDir, TimeoutMinutes and DryRun.
.PARAMETER Profile
    Release by default. Development permits partial runs, SkipBuild, BuildOnly,
    custom filters/carriers, NullRHI, media/report switches and baseline maintenance.
.PARAMETER EnginePath
    Optional UE root. Otherwise uses UNREAL_ENGINE_PATH or the project's registered engine.
.PARAMETER ProjectPath
    Optional repository demo .uproject path.
.PARAMETER OutputDir
    Release output directory; must be empty. Default Saved/ReleaseTests/<timestamp>.
.PARAMETER TimeoutMinutes
    Per external release stage timeout, default 60 minutes.
.PARAMETER DryRun
    Release prints its full plan without file writes, installation, compilation or UE.
.EXAMPLE
    .\Tools\RunTests.ps1
.EXAMPLE
    .\Tools\RunTests.ps1 -DryRun
.EXAMPLE
    .\Tools\RunTests.ps1 -Profile Development -Backend Luau -Filter UnLuaNova.API.Timer
.EXAMPLE
    .\Tools\RunTests.ps1 -Profile Development -Backend Luau -BuildOnly
#>

[CmdletBinding()]
param(
    [ValidateSet("Release", "Development")]
    [string] $Profile = "Release",
    [ValidateRange(1, 1440)]
    [int] $TimeoutMinutes = 60,
    [switch] $ReleaseManaged,
    [string] $EnginePath = "",
    [string] $ProjectPath = "",
    [ValidateSet("Lua", "Luau", "Both")]
    [string] $Backend = "Both",
    [string] $Platform = "Win64",
    [string] $Configuration = "Development",
    [string] $Filter = "UnLuaNova",
    [string] $Map = "",
    [string] $OutputDir = "",
    [string] $ExtraCommands = "",
    # 测试载体。Auto：按静态分类挑 —— filter 里没有需要视口/截图的用例就跑 -game，
    # 否则回编辑器。见上面的 .PARAMETER Carrier。
    [ValidateSet("Auto", "Editor", "Game")]
    [string] $Carrier = "Auto",
    # 分层。Fast = 跑掉慢用例之外的所有用例（见 -SlowPrefix），给"改完想马上知道有没有
    # 弄坏"用；All（默认）= 全跑，给 nightly 与发版前用。
    [ValidateSet("All", "Fast")]
    [string] $Tier = "All",
    # Fast 层要跳过的用例名前缀。慢用例必须自己站到一个独立段里（默认
    # UnLuaNova.Slow.*）：UE 的自动化只有 filter，没有 exclude，所以"除了慢的之外全跑"
    # 只能表示成"跑这些段"。
    [string[]] $SlowPrefix = @("UnLuaNova.Slow."),
    [switch] $NoScreenshots,
    [switch] $NullRHI,
    [switch] $RenderOffScreen,
    [switch] $Sound,
    [switch] $Publish,
    [switch] $NoReport,
    [switch] $SkipBuild,
    # 只编译不跑：编完每个后端就退出，不启动编辑器、不生成报告。给 nightly 与"改完 spec
    # 想先确认能编过"这类场景用；与 -SkipBuild 互斥。
    [switch] $BuildOnly,
    # 静态检查报出错误时让脚本以非 0 退出（Luau 后端是 luau-lsp，Lua 后端是 LuaLS）。
    # 默认只记录到报告里，不当作测试失败：它是静态检查，不是自动化用例，而且本机未必装了
    # 对应的分析器。
    [switch] $FailOnTypeErrors,
    # 用例集护栏：把这一轮的用例清单与 Tools/baseline/<后端>.json 比对，消失/新增会打出来。
    # 默认只告警不失败（改用例名是正当操作）；加上 -FailOnDrift 才让脚本非 0 退出。
    [switch] $FailOnDrift,
    # 人工确认后跑一次：把这一轮的用例清单写成新的基线并提交。
    [switch] $UpdateBaseline,
    [switch] $DryRun
)

$ErrorActionPreference = "Stop"

# Dispatch before discovery, config writes or classification.
if ($Profile -eq "Release") {
    $Allowed = @('Profile', 'EnginePath', 'ProjectPath', 'OutputDir', 'TimeoutMinutes', 'DryRun')
    foreach ($Key in $PSBoundParameters.Keys) {
        if ($Key -notin $Allowed) {
            throw "-$Key is a Development option. Release always runs the complete Win64 matrix. Use -Profile Development for a partial run."
        }
    }
    & (Join-Path $PSScriptRoot 'ReleasePipeline.ps1') -EnginePath $EnginePath -ProjectPath $ProjectPath `
        -OutputDir $OutputDir -TimeoutMinutes $TimeoutMinutes -DryRun:$DryRun
    exit $LASTEXITCODE
}
Write-Host 'Development profile: this result is not a release verdict.' -ForegroundColor Yellow

# Everything is derived from where this script lives, so no path in here belongs to one
# particular machine.
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ToolsDir = Join-Path $RepoRoot "Tools"

function Publish-PerfBaseline {
    # Benchmark.spec.cpp writes Saved/Benchmark/perf-<Backend>.json on every run; this is
    # what turns one machine's numbers into a baseline everybody is measured against.
    #
    # Before this existed the performance figures in the docs were a number someone read off
    # a screen once: no VM, no machine, no engine version attached, and nothing to diff
    # against. The spec warns (does not fail) while the file is missing, so the first person
    # to run -UpdateBaseline is what makes the gate real.
    param([string] $Backend)

    if (-not $ProjectPath) { return }
    $ProjectDir = Split-Path -Parent $ProjectPath
    $Source = Join-Path $ProjectDir "Saved/Benchmark/perf-$Backend.json"
    if (-not (Test-Path $Source)) {
        Write-Host "PerfBase  : 没找到 $Source —— 这次没有跑 Benchmark 用例，性能基线未更新。" -ForegroundColor Yellow
        return
    }

    $Target = Join-Path $RepoRoot "Tools/baseline/perf-$($Backend.ToLower()).json"
    Copy-Item $Source $Target -Force
    Write-Host "PerfBase  : $Target" -ForegroundColor Green
    Write-Host "            提交它之后，Perf.Benchmark 用例会把每次耗时与它比对（容差 25%）。" -ForegroundColor DarkGray
}

function Test-UnrealEngineRoot {
    param([string] $Path)
    return ($Path -and (Test-Path (Join-Path $Path "Engine/Binaries/Win64/UnrealEditor-Cmd.exe")))
}

function Get-RegisteredEngineBuilds {
    # UnrealVersionSelector keeps GUID -> engine path. Reading it keeps this script free of
    # any machine specific path: the mapping lives on the machine, not in the repository.
    $RegKey = "HKCU:\Software\Epic Games\Unreal Engine\Builds"
    if (-not (Test-Path $RegKey)) { return @() }

    $Builds = @()
    foreach ($Prop in (Get-ItemProperty -Path $RegKey).PSObject.Properties) {
        if ($Prop.Name -like "PS*") { continue }
        $Builds += [pscustomobject]@{ Id = $Prop.Name; Path = $Prop.Value }
    }
    return $Builds
}

function Find-UnrealEngine {
    param([string] $Hint, [string] $UProjectPath)

    if ($Hint) { return $Hint }
    if ($env:UNREAL_ENGINE_PATH) { return $env:UNREAL_ENGINE_PATH }

    # A .uproject names its engine by GUID, and the registry maps that GUID to a path -
    # the authoritative answer wherever the project has been associated with a build.
    if ($UProjectPath -and (Test-Path $UProjectPath)) {
        $Association = $null
        try { $Association = (Get-Content $UProjectPath -Raw | ConvertFrom-Json).EngineAssociation } catch { }
        if ($Association) {
            foreach ($Build in (Get-RegisteredEngineBuilds)) {
                if ($Build.Id -eq $Association -and (Test-UnrealEngineRoot $Build.Path)) {
                    return $Build.Path
                }
            }
        }
    }

    # A source build often sits next to the project or a few levels above it.
    $Dir = $RepoRoot
    for ($i = 0; $i -lt 5; $i++) {
        if (Test-UnrealEngineRoot $Dir) { return $Dir }
        $Parent = Split-Path -Parent $Dir
        if (-not $Parent -or $Parent -eq $Dir) { break }
        $Dir = $Parent
    }

    # Last resort: any registered build that actually has the commandlet.
    foreach ($Build in (Get-RegisteredEngineBuilds)) {
        if (Test-UnrealEngineRoot $Build.Path) { return $Build.Path }
    }

    return $null
}

if (-not $ProjectPath) {
    $FoundProject = Get-ChildItem -Path $RepoRoot -Filter "*.uproject" -File | Select-Object -First 1
    if (-not $FoundProject) {
        throw "No .uproject found under '$RepoRoot'. Pass -ProjectPath <path to .uproject>."
    }
    $ProjectPath = $FoundProject.FullName
}

$EnginePath = Find-UnrealEngine -Hint $EnginePath -UProjectPath $ProjectPath
if (-not $EnginePath) {
    throw @"
Could not locate a UE build. Do one of:
  - pass -EnginePath <UE root>
  - set the UNREAL_ENGINE_PATH environment variable
  - associate the project with a build (right click the .uproject -> Switch Unreal Engine version)
"@
}

if (-not $OutputDir) {
    $OutputDir = Join-Path $RepoRoot ("Saved/TestReports/" + (Get-Date -Format "yyyyMMdd-HHmmss"))
}
# UE resolves -ReportExportPath against its own working directory (the engine's
# Binaries/Win64), not ours, so a relative -OutputDir silently wrote the JSON next to
# UnrealEditor-Cmd.exe and the run looked like it produced nothing.
if (-not [System.IO.Path]::IsPathRooted($OutputDir)) {
    $OutputDir = Join-Path (Get-Location) $OutputDir
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null

$EditorCmd = Join-Path $EnginePath "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
if (-not (Test-Path $EditorCmd)) {
    throw "UnrealEditor-Cmd.exe not found under '$EnginePath'. Pass -EnginePath <UE root>."
}
if (-not (Test-Path $ProjectPath)) {
    throw "Project not found at '$ProjectPath'. Pass -ProjectPath <path to .uproject>."
}
$BuildBat = Join-Path $EnginePath "Engine/Build/BatchFiles/Build.bat"
if (-not $SkipBuild -and -not (Test-Path $BuildBat)) {
    throw "Build.bat not found under '$EnginePath'. Pass -EnginePath <UE root> or add -SkipBuild."
}

# --- 载体：这一轮测试跑在哪个进程里 ---------------------------------------------
# 从来都是"全部塞进一个 UnrealEditor-Cmd"，于是不管用例要不要，每轮都得付一次编辑器
# 启动 + 编辑器 tick 的账，而这笔账是摊在每个用例头上的，看不出来。实际上只有两类
# 用例真的离不开编辑器：要真实视口/截图的（D/E），和要 PIE 联网的（C）。
# A（纯逻辑）和 B（只要一个 UWorld）在项目自己的 <Project>.exe -game 里一样跑。
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectPath)
$ClassificationPath = Join-Path $OutputDir "classification.json"
$script:Classification = $null

function Get-TestClassification {
    # 纯静态分析（扫 Specs/ 与 Tests/ 的源码），跑一次几毫秒；失败就当没分类。
    if ($script:Classification) { return $script:Classification }

    $Classifier = Join-Path $ToolsDir "ClassifyTests.py"
    if (-not (Test-Path $Classifier)) { return $null }
    # python 不在 PATH 上时不该把整轮跑崩：分类只是"挑载体 / 挑层"的依据，拿不到就走保守
    # 路径（编辑器 + 全跑），脚本别的地方要用 python 会自己报错。
    if (-not (Get-Command python -ErrorAction SilentlyContinue)) { return $null }

    try {
        & python $Classifier --out $ClassificationPath 2>&1 | Out-Null
    } catch {
        return $null
    }
    if (-not (Test-Path $ClassificationPath)) { return $null }

    try {
        $Json = Get-Content $ClassificationPath -Raw -Encoding UTF8 | ConvertFrom-Json
    } catch {
        return $null
    }
    $Map = @{}
    if ($Json.tests) {
        foreach ($P in $Json.tests.PSObject.Properties) { $Map[$P.Name] = [string]$P.Value }
    }
    $script:Classification = $Map
    return $Map
}

function Resolve-Carrier {
    param([Parameter(Mandatory)] [string] $Requested)

    if ($Requested -ne "Auto") {
        return @{ Carrier = $Requested; Reason = "-Carrier $Requested（手动指定）" }
    }

    $Map = Get-TestClassification
    if (-not $Map -or $Map.Count -eq 0) {
        return @{ Carrier = "Editor"; Reason = "拿不到静态分类（Tools/ClassifyTests.py），保守起见用编辑器" }
    }

    # 只有 D/E 真的必须在编辑器里：D 要 NonNullRHI 的视口，E 要截像素。
    # C（PIE 联网）现在也留在编辑器里 —— 它要 PIE 的 server+client，见 P2。
    # 判断用的是这一轮真正会跑的用例（$RunFilters，Fast 层已经把慢段剔掉了）。
    $Heavy = @()
    foreach ($Key in $Map.Keys) {
        $Selected = $false
        foreach ($F in $RunFilters) {
            if ($Key -like "$F*") { $Selected = $true; break }
        }
        if (-not $Selected) { continue }

        $Cls = $Map[$Key]
        if ($Cls -eq "D" -or $Cls -eq "E" -or $Cls -eq "C") { $Heavy += "$Key($Cls)" }
    }
    if ($Heavy.Count -gt 0) {
        return @{ Carrier = "Editor"; Reason = "filter 里有只能在编辑器里跑的用例：" + ($Heavy -join ', ') }
    }
    return @{ Carrier = "Game"; Reason = "filter 里没有 D/E/C 类用例 —— 可以离开编辑器进程" }
}

function Resolve-RunFilters {
    # 一次 run 可以排多个 filter（每条 -ExecCmds 一个 "Automation RunTests"），这就是本脚本
    # 的"分层"：UE 的自动化没有 exclude，所以 Fast 层不跑的用例，只能靠不把它们所在的
    # 段排进去来表达。
    #
    # 段是从静态分类里算出来的（Tools/ClassifyTests.py 扫出来的用例名），所以分类里没有
    # 的用例也不会被 Fast 层跑到 —— 报告顶部的"未分类"计数必须是 0，这不是排印错误。
    if ($Tier -ne "Fast") { return @($Filter) }

    $Map = Get-TestClassification
    if (-not $Map -or $Map.Count -eq 0) {
        Write-Host "Tier      : 拿不到静态分类，Fast 层退化为全跑。" -ForegroundColor Yellow
        return @($Filter)
    }

    # 段切到哪一层，跟着 -Filter 走：传了 UnLuaNova.API.TArray 还按"前两段"归并，等于
    # 把整个 UnLuaNova.API 都排进来 —— "我只想跑 TArray" 变成"跑了 API 全段"，而这是
    # Fast 层最不该发生的事（它本来就是为了少跑）。
    # 但最少两层：默认 Filter 就是 "UnLuaNova"，只切一层等于原样全跑，慢段也跟着回来。
    $FilterDepth = @($Filter -split '\.' | Where-Object { $_ }).Count
    $Depth = [Math]::Max(2, $FilterDepth)

    $Segments = @{}
    foreach ($Key in $Map.Keys) {
        if ($Key -notlike "$Filter*") { continue }

        $Slow = $false
        foreach ($Prefix in $SlowPrefix) {
            if ($Key -like "$Prefix*") { $Slow = $true; break }
        }
        if ($Slow) { continue }

        $Parts = $Key -split '\.'
        $Take = [Math]::Min($Depth, $Parts.Count)
        $Segments[(($Parts | Select-Object -First $Take) -join '.')] = $true
    }

    if ($Segments.Count -eq 0) {
        Write-Host "Tier      : Fast 层没有剩下任何段，全跑。" -ForegroundColor Yellow
        return @($Filter)
    }

    return @($Segments.Keys | Sort-Object)
}

# 先算这一轮跑哪些，再决定用什么载体：Fast 层剔掉的用例不该把载体拖回编辑器。
$RunFilters = Resolve-RunFilters
$CarrierChoice = Resolve-Carrier -Requested $Carrier
$IsGameCarrier = ($CarrierChoice.Carrier -eq "Game")

function Resolve-GameExe {
    # -game 用项目自己编出来的 exe。源码版引擎把游戏目标的 exe 放在项目的
    # Binaries/Win64 下，安装版/少数布局下在 Engine/Binaries/Win64，两个都找。
    $Candidates = @(
        (Join-Path (Split-Path -Parent $ProjectPath) "Binaries/Win64/$ProjectName.exe")
        (Join-Path $EnginePath "Engine/Binaries/Win64/$ProjectName.exe")
    )
    foreach ($C in $Candidates) {
        if (Test-Path $C) { return $C }
    }
    return $Candidates[0]
}

# --- one run at a time -------------------------------------------------------------
# The backend is global state: an ini value plus whatever the last build linked. Two runs
# overlap at their peril - the second Build.bat only waits on UBT's mutex and then uses
# whatever the first one produced, which is exactly how a run filed under 'lua' once ran
# Luau binaries. This lock makes that impossible, and it records the owning PID so a run
# that was killed does not block the next one forever.
$LockPath = Join-Path $RepoRoot "Saved/RunTests.lock"
$LockStream = $null

function Acquire-RunLock {
    if ($ReleaseManaged) {
        $Owner = Get-Content $LockPath -Raw -ErrorAction Stop
        if (-not $env:UNLUANOVA_RELEASE_TOKEN -or $Owner -notlike "*token=$env:UNLUANOVA_RELEASE_TOKEN*") {
            throw 'ReleaseManaged requires the owning release pipeline lock.'
        }
        return
    }
    if (Test-Path $LockPath) {
        $Held = Get-Content $LockPath -Raw -ErrorAction SilentlyContinue
        $HeldPid = if ($Held -match "pid=(\d+)") { $Matches[1] } else { $null }
        if ($HeldPid -and (Get-Process -Id ([int]$HeldPid) -ErrorAction SilentlyContinue)) {
            throw @"
Another RunTests.ps1 is already running (pid $HeldPid - see $LockPath).
The Lua backend is global state: an ini value plus the built binaries. Two runs at once
would hand one of them the other one's build. Wait for it to finish, or delete the lock
if you are certain it is not running.
"@
        }
        Write-Host "Removing stale lock from a run that is no longer alive ($LockPath)" -ForegroundColor Yellow
        Remove-Item $LockPath -Force -ErrorAction SilentlyContinue
    }

    New-Item -ItemType Directory -Path (Split-Path -Parent $LockPath) -Force | Out-Null
    # FileShare::None is the actual mutex: a second process cannot even open the file.
    #
    # Retried, because Remove-Item above returns while Windows may still be finishing the
    # delete (a scanner or an indexer holding the file briefly is enough), and because two
    # runs starting in the same second can both get past the stale check. Without this the
    # failure is an IOException that looks like a broken script rather than a busy moment.
    $Attempt = 0
    while ($true) {
        $Attempt++
        try {
            $script:LockStream = [System.IO.File]::Open($LockPath, [System.IO.FileMode]::OpenOrCreate,
                [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
            break
        } catch {
            if ($Attempt -ge 5) {
                throw "Could not take the run lock at '$LockPath' after $Attempt tries: $($_.Exception.Message)"
            }
            Start-Sleep -Milliseconds 400
        }
    }
    $Stamp = [System.Text.Encoding]::UTF8.GetBytes("pid=$PID started=$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
    $script:LockStream.Write($Stamp, 0, $Stamp.Length)
    $script:LockStream.Flush()
}

function Release-RunLock {
    if ($ReleaseManaged) { return }
    if ($script:LockStream) {
        $script:LockStream.Dispose()
        $script:LockStream = $null
    }
    Remove-Item $LockPath -Force -ErrorAction SilentlyContinue
}

# --- which backend is actually in these binaries? -----------------------------------
# The ini and the run directory name are only intentions. The authoritative answer is the
# header UBT generated for the plugin module when it compiled: it carries
# UNLUANOVA_LUA_BACKEND (0 = Lua, 1 = Luau) and it only exists for a build that ran.
$PluginRoot = Join-Path $RepoRoot "Plugins/UnLuaNova"

function Get-BuiltBackend {
    # 中间目录按目标名分层：编辑器目标是 UnrealEditor，游戏目标是 <Project>。
    # 只接受当前目标的定义，不能借另一个目标的旧头文件通过后端校验。
    $Folders = if ($IsGameCarrier) { @($ProjectName) } else { @('UnrealEditor') }
    foreach ($Folder in $Folders) {
        $Definitions = Join-Path $PluginRoot "Intermediate/Build/$Platform/x64/$Folder/$Configuration/UnLuaNova/Definitions.UnLuaNova.h"
        if (Test-Path $Definitions) { break }
        $Definitions = $null
    }
    if (-not $Definitions) { return $null }

    $Text = Get-Content $Definitions -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
    if (-not $Text) { return $null }
    if ($Text -match "UNLUANOVA_LUA_BACKEND\s+1") { return "Luau" }
    if ($Text -match "UNLUANOVA_LUA_BACKEND\s+0") { return "Lua" }
    return $null
}

function Assert-BackendBuilt {
    param([string] $Value)

    # 模块 DLL 的名字带着目标名：编辑器目标是 UnrealEditor-Lua.dll，游戏目标是
    # <Project>-Lua.dll。跑 -game 时编的是后者，照着编辑器的名字找会永远找不到。
    $DllPrefix = if ($IsGameCarrier) { $ProjectName } else { "UnrealEditor" }
    $Dll = Join-Path $PluginRoot ("Binaries/Win64/$DllPrefix-" + $Value + ".dll")
    if (-not (Test-Path $Dll)) {
        throw "'$Dll' is missing - the $Value backend has never been built here. Drop -SkipBuild."
    }

    $Built = Get-BuiltBackend
    if ($Built -ne $Value) {
        throw @"
This run is for the $Value backend, but the binaries on disk are the $Built build.
The backend is a compile-time choice (UnLuaNova.Build.cs reads LuaBackend from $IniPath);
switching it without a build that actually finishes - or a build that was interrupted -
leaves the ini, the run directory name and the DLLs disagreeing, and the tests get filed
under the wrong backend. Rebuild (drop -SkipBuild) and run again.
"@
    }
}

# A crash exports no JSON, so a crashed run used to look like "0 tests" and the only clue
# was a 500 KB log nobody opened. Pull the answer out of the log and print it here.
function Show-CrashSummary {
    param([string] $LogPath)

    if (-not (Test-Path $LogPath)) { return }
    $Lines = Get-Content $LogPath -Encoding UTF8 -ErrorAction SilentlyContinue
    if (-not $Lines) { return }

    $CrashAt = -1
    for ($i = 0; $i -lt $Lines.Count; $i++) {
        if ($Lines[$i] -match "Critical error|Fatal error|Unhandled Exception|Assertion failed|Ensure failed") {
            $CrashAt = $i
            break
        }
    }
    if ($CrashAt -lt 0) { return }

    # The test that was running: the last "Test Started" with no "Test Completed" to match.
    # (-notMatch would not fill $Matches, hence the nested if.)
    $Running = $null
    for ($i = 0; $i -lt $CrashAt; $i++) {
        if ($Lines[$i] -match "Test Started\. Name=\{([^}]*)\}") {
            $Name = $Matches[1]
            $Done = $false
            for ($j = $i + 1; $j -lt $CrashAt; $j++) {
                if ($Lines[$j] -match "Test Started\.") { break }
                if ($Lines[$j] -match "Test Completed\." -and $Lines[$j] -match [regex]::Escape("Name={$Name}")) {
                    $Done = $true
                    break
                }
            }
            if (-not $Done) { $Running = $Name }
        }
    }

    Write-Host ""
    Write-Host "CRASH: the editor died - no results were exported." -ForegroundColor Red
    if ($Running) { Write-Host "  running at the time : $Running" -ForegroundColor Red }
    $Lines[$CrashAt..([Math]::Min($CrashAt + 5, $Lines.Count - 1))] | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    $Lines | Where-Object { $_ -match "\[Callstack\]" } | Select-Object -First 5 |
        ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
    Write-Host ""
}

# The other way a run produces nothing: the editor comes up, announces it is ready, and then
# never queues a test. It looks exactly like a hang - the process is alive, ticking, logging
# nothing - and it is what an unquoted -ExecCmds does, because "Automation RunTests X" then
# reaches the editor as the bare command "Automation". Say it out loud and echo the command
# line the editor actually saw, which is where the missing quotes are plain to see.
function Show-NoTestsQueuedHint {
    param([string] $LogPath)

    if (-not (Test-Path $LogPath)) { return }
    $Lines = Get-Content $LogPath -Encoding UTF8 -ErrorAction SilentlyContinue
    if (-not $Lines) { return }
    if (-not ($Lines | Select-String -SimpleMatch "Ready to start automation" -Quiet)) { return }
    if ($Lines | Select-String -SimpleMatch "Automation: RunTests=" -Quiet) { return }

    Write-Host "Hint      : 编辑器到了 'Ready to start automation' 却一条测试都没排队 —— 大概率是 -ExecCmds 的引号没传进去。" -ForegroundColor Yellow
    $Lines | Select-String -SimpleMatch "LogInit: Command Line:" | Select-Object -First 1 |
        ForEach-Object { Write-Host "           $($_.Line)" -ForegroundColor Yellow }
}

# --- screenshots ---------------------------------------------------------------------
# Screenshots go to the project's Saved/Screenshots, not next to the JSON the automation
# framework exports, so they have to be pulled into the run directory for the report to be
# self-contained.
#
#   - The directory is not cleaned by the editor. A run that takes no picture at all would
#     otherwise publish last week's PNGs as this run's gallery, which is worse than an empty
#     one: nobody can tell the pictures are not from this run. So it is emptied first.

function Get-ScreenshotDir {
    # 截图落在项目 Saved/Screenshots/<平台名> 下，而平台名带着载体：编辑器是
    # WindowsEditor，游戏是 Windows。-game 的跑动若照旧去 WindowsEditor 找，一张
    # 都找不到（还会把上一轮编辑器的图当成自己的）。
    $Leaf = if ($IsGameCarrier) { "Windows" } else { "WindowsEditor" }
    return (Join-Path $RepoRoot "Saved/Screenshots/$Leaf")
}

function Clear-StaleScreenshots {
    $ShotDir = Get-ScreenshotDir
    if (-not (Test-Path $ShotDir)) { return }
    Get-ChildItem -Path $ShotDir -Filter "*.png" -File -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

function Get-NetTopologyFiles {
    param([Parameter(Mandatory)] [datetime] $Since)

    $Root = if ($env:UNLUANOVA_TEST_NET_ROOT) { $env:UNLUANOVA_TEST_NET_ROOT } else { Join-Path $RepoRoot "Saved/UnLuaNovaNet" }
    if (-not (Test-Path $Root)) { return @() }

    return @(Get-ChildItem -Path $Root -Filter "topology.json" -Recurse -File -ErrorAction SilentlyContinue |
        Where-Object { $_.LastWriteTime -ge $Since })
}

function Stop-NetPeers {
    # 用例自己会清理 peer（FNetTerminatePeersCommand），但那是"用例走到了最后一步"才成立：
    # 崩了、被超时打断、主进程被 Ctrl-C 掉，peer 就留下了 —— 每个都是一个还在吃 CPU 的
    # UE 进程，占着端口，下一次跑还会撞上。所以主进程退出后由脚本按 topology 再收一次尾。
    param([Parameter(Mandatory)] [datetime] $Since)

    $Killed = 0
    foreach ($Topology in (Get-NetTopologyFiles -Since $Since)) {
        try {
            $Json = Get-Content $Topology.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        } catch {
            continue
        }

        $ExeName = [System.IO.Path]::GetFileNameWithoutExtension([string]$Json.exe)
        foreach ($Peer in @($Json.peers)) {
            $PeerProcessId = [int]$Peer.pid
            if ($PeerProcessId -le 0) { continue }

            $Proc = Get-Process -Id $PeerProcessId -ErrorAction SilentlyContinue
            if (-not $Proc) { continue }

            # pid 会被复用：确认这个 pid 现在装的还是那个 exe，别把别人的进程杀掉。
            if ($ExeName -and ($Proc.ProcessName -ne $ExeName)) { continue }
            # 还要确认它是**这一轮**起的：peer 跑完就退出，pid 完全可能已经归了另一次启动
            # （甚至是用户自己开的那一份）。进程名一样，只有启动时间能分辨。
            # 拿不到启动时间就不杀 —— 宁可留下一个孤儿，也不杀一个来路不明的进程。
            try {
                if ($Proc.StartTime -lt $Since) { continue }
            } catch {
                continue
            }

            Stop-Process -Id $PeerProcessId -Force -ErrorAction SilentlyContinue
            $Killed++
        }
    }

    if ($Killed -gt 0) {
        Write-Host ("Net       : {0} 个残留 peer 进程已结束（用例没走到自己清理那一步）。" -f $Killed) -ForegroundColor Yellow
    }
}

function Format-DataSize {
    param([long] $Bytes)
    if ($Bytes -ge 1MB) { return "{0:N1} MB" -f ($Bytes / 1MB) }
    return "{0:N0} KB" -f ($Bytes / 1KB)
}

function Export-IntelliSense {
    # The type check is only as good as the declarations it reads, and those are produced by
    # the editor - UnLuaNova.d.lua for luau-lsp, the EmmyLua annotations for LuaLS. They
    # used to be reachable only from the editor toolbar, so a machine where nobody ever
    # clicked that button ran the check against nothing: luau-lsp turned every UE type
    # into "Unknown global 'UnLuaNova'", and LuaLS skipped the run outright. Both still
    # looked like a passing run.
    #
    # So the run exports them itself, with the commandlet the toolbar button calls. One extra
    # editor launch (~40s), and only when the type check is actually going to run.
    $LuauDir = Join-Path $RepoRoot "Plugins/UnLuaNova/IntelliSense/Luau"
    $DeclsFile = Join-Path $LuauDir "UnLuaNova.d.lua"

    Write-Host "Decl      : 导出智能提示（$EditorCmd -run=UnLuaNovaIntelliSense -BP=1）"

    $Psi = New-Object System.Diagnostics.ProcessStartInfo
    $Psi.FileName = $EditorCmd
    # Quoted like the test run's own arguments: an unquoted project path splits at the first
    # space and the commandlet is asked to open a project that does not exist.
    $Psi.Arguments = '"' + $ProjectPath + '" -run=UnLuaNovaIntelliSense -BP=1 -unattended -NoSplash -stdout'
    $Psi.UseShellExecute = $false
    $Proc = [System.Diagnostics.Process]::Start($Psi)
    $Proc.WaitForExit()

    if ($Proc.ExitCode -ne 0) {
        Write-Host "Decl      : commandlet 退出码 $($Proc.ExitCode)，声明文件可能没写出来。" -ForegroundColor Yellow
    }

    if (-not (Test-Path -LiteralPath $DeclsFile) -or (Get-Item -LiteralPath $DeclsFile).Length -eq 0) {
        Write-Host "Decl      : 没生成 $DeclsFile —— luau-lsp 会把每个 UE 类型都报成 unknown global。" -ForegroundColor Yellow
        return $false
    }

    $Size = (Get-Item -LiteralPath $DeclsFile).Length
    $EmmyLuaCount = @(Get-ChildItem -Path (Join-Path $RepoRoot "Plugins/UnLuaNova/IntelliSense/EmmyLua") `
        -Filter "*.lua" -File -ErrorAction SilentlyContinue).Count
    Write-Host ("Decl      : {0}（{1}），EmmyLua 注解 {2} 个" -f $DeclsFile, (Format-DataSize $Size), $EmmyLuaCount)
    return $true
}

function Ensure-Utf8Bom {
    param([Parameter(Mandatory)] [string] $Path)

    # Windows PowerShell 5.1 decodes a .ps1 with the machine's ANSI code page unless the file
    # opens with a UTF-8 BOM. These scripts are full of Chinese, so a BOM-less file is not a
    # cosmetic problem: the bytes are decoded as GBK, the resulting garbage lands inside string
    # literals, and the script dies with a ParseException before it runs a single line. That is
    # how a run once reported a failed type check without ever saying what was wrong.
    #
    # Anything can drop a BOM - an editor, a save, a scripted rewrite - so do not trust it: put
    # it back when it is missing. Returns $true when it had to write. A BOM is only prepended,
    # never to a file that already has one: that would corrupt the first line.
    $Bytes = [System.IO.File]::ReadAllBytes($Path)
    if ($Bytes.Length -ge 3 -and $Bytes[0] -eq 0xEF -and $Bytes[1] -eq 0xBB -and $Bytes[2] -eq 0xBF) {
        return $false
    }

    $Text = [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding($true)))
    return $true
}

function Invoke-TypeCheck {
    param(
        [Parameter(Mandatory)] [string] $RunDir,
        [Parameter(Mandatory)] [string] $Backend
    )

    # Two backends, two analyzers, and that is not a preference - it is what each language
    # can parse. Luau scripts carry type annotations and --!strict, which Lua 5.5 chokes on;
    # the Lua backend's scripts are Lua 5.5, which a Luau analyzer chokes on. So each backend
    # runs the checker that reads its language, against the declarations the editor exported
    # for it (.d.lua for Luau, EmmyLua for Lua), and each writes its own summary next to the
    # automation JSON - the report generator looks for these exact names.
    # The Luau side is luau-lsp, not Luau's own luau-analyze: upstream deleted --defs from
    # luau-analyze, so the .d.lua could not reach it and every UE.xxx came back as an
    # Unknown global. luau-lsp analyze takes --definitions and reads the same file.
    if ($Backend -eq "Luau") {
        $SummaryPath = Join-Path $RunDir "luau-analyze.json"
        $CheckerName = "CheckLuauTypes.ps1"
        $Checker = Join-Path $ToolsDir $CheckerName
        $CheckerLabel = "luau-lsp"
        # Only the annotated Luau scripts: the rest of Content/Script is written to Lua 5.1
        # so it loads on both backends, and strict mode would light up on it for reasons
        # that have nothing to do with this run.
        # A hashtable, not an array: splatting an array of "-Name", "Value" pairs binds
        # positionally against these scripts (both take [CmdletBinding()]), which made
        # -CheckLevel swallow "-SummaryPath" and fail the ValidateSet - the type check then
        # aborted before writing its summary, with a parameter-binding error rather than
        # anything about the scripts.
        $CheckerArgs = @{ Path = "Content/Script/Luau"; Mode = "strict"; SummaryPath = $SummaryPath }
    } else {
        $SummaryPath = Join-Path $RunDir "lua-analyze.json"
        $CheckerName = "CheckLuaTypes.ps1"
        $Checker = Join-Path $ToolsDir $CheckerName
        $CheckerLabel = "LuaLS"
        # The whole script root minus Content/Script/Luau (Luau syntax, never loaded by the
        # Lua backend) - Warning and above, so type hints do not bury real errors.
        $CheckerArgs = @{ Path = "Content/Script"; CheckLevel = "Warning"; SummaryPath = $SummaryPath }
    }

    $Skip = {
        param([string] $Reason)
        @{
            skipped = $true
            checker = $CheckerLabel
            reason  = $Reason
            mode    = $(if ($Backend -eq "Luau") { "strict" } else { "Warning" })
            files   = 0
        } | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $SummaryPath -Encoding UTF8
    }

    if (-not (Test-Path $Checker)) {
        & $Skip "没找到 Tools/$CheckerName。"
        Write-Host ("TypeCheck : skipped - 没找到 Tools/{0}" -f $CheckerName) -ForegroundColor Yellow
        return
    }

    if (Ensure-Utf8Bom -Path $Checker) {
        Write-Host "TypeCheck : $CheckerName 缺 UTF-8 BOM，已补上（见函数注释）。" -ForegroundColor Yellow
    }

    & $Checker @CheckerArgs

    if (-not (Test-Path $SummaryPath)) {
        & $Skip "类型检查脚本没有写出结果文件（退出码 $LASTEXITCODE）。"
        return
    }

    $S = Get-Content $SummaryPath -Raw | ConvertFrom-Json
    if ($S.skipped) {
        Write-Host ("TypeCheck : skipped - {0}" -f $S.reason) -ForegroundColor Yellow
        return
    }

    $Errors = @($S.errors).Count
    Write-Host ("TypeCheck ({0}) : {1} file(s), {2} error(s), {3} warning(s)" -f $CheckerLabel, $S.files, $Errors, @($S.warnings).Count)
    if ($Errors -gt 0) {
        Write-Host "TypeCheck : 有静态检查错误，见报告「静态类型检查」一节。" -ForegroundColor Yellow
        if ($FailOnTypeErrors) { $script:ExitCode = 1 }
    }
}

function Copy-Screenshots {
    param([string] $Dest)

    $ShotDir = Get-ScreenshotDir
    $Files = @(Get-ChildItem -Path $ShotDir -Filter "*.png" -File -ErrorAction SilentlyContinue)
    if ($Files.Count -eq 0) { return 0 }

    New-Item -ItemType Directory -Path $Dest -Force | Out-Null
    foreach ($F in $Files) { Copy-Item $F.FullName -Destination $Dest -Force -ErrorAction SilentlyContinue }
    return $Files.Count
}

function Copy-NetArtifacts {
    # 联网用例的产物：每个 peer 一份报告 JSON、一份 -abslog 日志，加一份 topology.json。
    # headless 的独立服务器没有窗口也没录屏，它唯一的痕迹就是这份日志 —— 不归档的话，
    # "Server peer 没写出报告"只能靠猜。
    #
    # 只归档本轮：这个目录没人清理，把上一轮的会话目录一起拷进来，报告里就会同时出现
    # 两个 "Server.json"，而"哪一份是这轮的"得靠人去猜 mtime。
    param(
        [Parameter(Mandatory)] [string] $RunDir,
        [Parameter(Mandatory)] [datetime] $Since
    )

    $Root = if ($env:UNLUANOVA_TEST_NET_ROOT) { $env:UNLUANOVA_TEST_NET_ROOT } else { Join-Path $RepoRoot "Saved/UnLuaNovaNet" }
    if (-not (Test-Path $Root)) { return 0 }

    $Sessions = @(Get-NetTopologyFiles -Since $Since)
    if ($Sessions.Count -eq 0) { return 0 }

    $Dest = Join-Path $RunDir "net"
    if ([IO.Path]::GetFullPath($Root) -eq [IO.Path]::GetFullPath($Dest)) { return $Sessions.Count }
    $Copied = 0
    foreach ($Topology in $Sessions) {
        $SessionDir = Split-Path -Parent $Topology.FullName
        foreach ($F in @(Get-ChildItem -Path $SessionDir -Recurse -File -ErrorAction SilentlyContinue)) {
            $Relative = $F.FullName.Substring($Root.Length).TrimStart('\', '/')
            $Target = Join-Path $Dest $Relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $Target) -Force | Out-Null
            Copy-Item $F.FullName -Destination $Target -Force -ErrorAction SilentlyContinue
            $Copied++
        }
    }

    $Logs = @(Get-ChildItem -Path $Dest -Filter "*.log" -Recurse -File -ErrorAction SilentlyContinue)
    Write-Host ("Net       : {0} 个 peer 日志 + {1} 份报告 -> {2}" -f
        $Logs.Count,
        @(Get-ChildItem -Path $Dest -Filter "*.json" -Recurse -File -ErrorAction SilentlyContinue).Count,
        $Dest)
    return $Copied
}

# --- the Lua backend is a build-time setting ------------------------------------------
# Both backends export the same lua_* symbols, so exactly one of them is in a build and it
# is picked by UnLuaNova.Build.cs reading LuaBackend out of this ini. Running both backends
# therefore means editing it, rebuilding, testing, and putting it back afterwards.

# 全插件只有一个配置文件；旧的 DefaultUnLuaNova.ini 仍被 Build.cs 当兜底读，
# 但脚本一律写新文件，避免"改了一个、另一个还是旧值"。
$IniPath = Join-Path (Split-Path -Parent $ProjectPath) "Config/DefaultUnLuaNova.ini"
$IniSection = "[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]"
$IniExisted = Test-Path $IniPath
$IniBackup = $null

function Get-IniBackend {
    if (-not (Test-Path $IniPath)) { return $null }
    $InSection = $false
    # UTF8 on the way in to match the UTF8 (no BOM) on the way out - a round trip through
    # the shell's default encoding would re-encode any non-ASCII byte in the file.
    foreach ($Line in ([System.IO.File]::ReadAllLines($IniPath, [System.Text.Encoding]::UTF8))) {
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
            # Leaving the section: if it never had the key, add it on the way out.
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

    New-Item -ItemType Directory -Path (Split-Path -Parent $IniPath) -Force | Out-Null
    # No BOM: UBT parses this file and a BOM in front of the first section header turns it
    # into a line that is not a section at all.
    [System.IO.File]::WriteAllLines($IniPath, $Out, (New-Object System.Text.UTF8Encoding($false)))
}

function Restore-IniBackend {
    try {
        if ($IniExisted) {
            [System.IO.File]::WriteAllBytes($IniPath, $IniBackup)
        } elseif (Test-Path $IniPath) {
            Remove-Item $IniPath -Force
        }
    } catch {
        $script:ExitCode = 1
        Write-Warning "Could not restore '$IniPath' - put LuaBackend back by hand. $_"
    }
}

$Backends = if ($Backend -eq "Both") { @("Lua", "Luau") } else { @($Backend) }
# 编辑器载体编 <Project>Editor 目标，-game 编 <Project> 目标：后者不带 UnrealEd，
# 也不带只在编辑器里才需要的模块（UnLuaNovaTestSuite.Build.cs 已经是条件依赖）。
$TargetName = if ($IsGameCarrier) { $ProjectName } else { $ProjectName + "Editor" }

function Format-EditorArg {
    param([string] $Arg)

    # Start-Process hands -ArgumentList to the child as one command line and quotes nothing
    # itself, unlike `&`, which used to do it for us when the editor was launched inline.
    # Anything holding a space has to arrive quoted, or the child reads it as two arguments.
    #
    # -ExecCmds is the bad one: unquoted, "Automation RunTests UnLuaNova; Quit" reaches the
    # editor as the exec command "Automation" - which queues no test - plus a stray "Quit"
    # argument. The editor then sits there ticking forever: no crash, no error, no test, and
    # a log that stops at "Ready to start automation".
    if ($Arg.Contains('"')) { return $Arg } # already quoted - see -ExecCmds below
    if ($Arg -match '\s') { return '"' + $Arg + '"' }
    return $Arg
}

function Get-TestArgs {
    param([string] $ReportDir)

    # Automation exports one JSON per test into -ReportExportPath; the log goes next to it
    # so a failed run can be read without hunting through Saved/Logs. The name follows the
    # carrier: a -game run writing Editor.log is how a wrong-carrier report goes unnoticed.
    $LogName = if ($IsGameCarrier) { "Game.log" } else { "Editor.log" }
    $LogFile = Join-Path $ReportDir $LogName

    # ExtraCommands run after the suite, which is where screenshots belong - they need the
    # world the tests left behind, not the one at startup.
    # One RunTests per filter: -Tier Fast 是"跑了 N 个段"而不是"跑了一个段再排除"。
    $Cmds = @()
    foreach ($F in $RunFilters) { $Cmds += "Automation RunTests $F" }
    if ($ExtraCommands) { $Cmds += $ExtraCommands }
    $Cmds += "Quit"
    $ExecCommands = $Cmds -join '; '
    # UE separates console commands with commas; Automation owns the semicolon chain.
    if ($ReleaseManaged) { $ExecCommands = 'UnLuaNovaTest.ExportManifest, ' + $ExecCommands }

    $A = @($ProjectPath)
    if ($Map) { $A += $Map }
    # -game: 用打包前的独立游戏进程跑，不起编辑器。少了编辑器启动与后台 tick，
    # 少了一整套编辑器模块 —— 也是唯一能验证"这些用例其实不依赖编辑器"的办法。
    if ($IsGameCarrier) { $A += "-game" }
    $A += @(
        "-unattended"
        "-NoSplash"
        "-stdout"
        # Quoted on purpose: the value is one argument holding spaces and a semicolon. UE
        # reads -ExecCmds out of the raw command line, so the quotes have to be part of the
        # string the child sees (Format-EditorArg leaves an already-quoted argument alone).
        "-ExecCmds=`"$ExecCommands`""
        "-ReportExportPath=$ReportDir"
        "-abslog=$LogFile"
    )
    # Rendering stays on unless it is explicitly turned off: with -NullRHI the framework
    # drops the tests that need a viewport, so the run is a different (smaller) suite rather
    # than the same one done faster. -NullRHI is opt-in for that reason.
    if ($NullRHI) { $A += "-NullRHI" }
    if ($RenderOffScreen) { $A += "-RenderOffScreen" }
    # Audio is the other way round: off unless asked for. Opening a map ends the previous
    # test's PIE session, which tears an audio device down, and a device that is slow to
    # stop takes the editor's main thread with it - a run once froze inside
    # FAudioMixerWasapi::StopAudioStream() and never ticked again.
    if (-not $Sound) { $A += "-nosound" }
    return $A
}

Write-Host "Engine       : $EnginePath"
Write-Host "Project      : $ProjectPath"
Write-Host "Carrier      : $($CarrierChoice.Carrier) - $($CarrierChoice.Reason)"
Write-Host "Tier         : $Tier$(if ($Tier -eq 'Fast') { " (跳过 $($SlowPrefix -join ', '))" })"
Write-Host "Filter       : $($RunFilters -join ' | ')"
Write-Host "Backend(s)   : $($Backends -join ', ')"
Write-Host "Output       : $OutputDir"
Write-Host "Media        : $(if ($NoScreenshots) { 'off' } else { 'screenshots' })"
if ($NullRHI -and -not $NoScreenshots) {
    # Not an error: the suite still runs, it just has nothing to photograph.
    Write-Host "               -NullRHI: no viewport, so no picture will come out of this run." -ForegroundColor Yellow
}
Write-Host "Audio        : $(if ($Sound) { 'on (-Sound)' } else { 'off (-nosound)' })"
if ($RenderOffScreen) {
    Write-Host 'Rendering    : offscreen RHI; test screenshots enabled.'
}

if ($DryRun)
{
    foreach ($B in $Backends) {
        $RunDir = Join-Path $OutputDir $B.ToLower()
        Write-Host ""
        Write-Host "--- $B -> $RunDir"
        Write-Host "Build  : $BuildBat $TargetName $Platform $Configuration -Project=`"$ProjectPath`""
        $ArgLine = ((Get-TestArgs -ReportDir $RunDir) | ForEach-Object { Format-EditorArg $_ }) -join ' '
        Write-Host "Cmd    : $(if ($IsGameCarrier) { Resolve-GameExe } else { $EditorCmd }) $ArgLine"
    }
    Write-Host ""
    Write-Host "Dry run - nothing was executed."
    exit 0
}

if ($BuildOnly -and $SkipBuild) {
    throw "-BuildOnly and -SkipBuild are mutually exclusive: one asks for a build, the other skips it."
}

$ExitCode = 0
$CurrentBackend = Get-IniBackend
$IniOriginal = $CurrentBackend
# The declarations are backend-independent, so the run exports them once and both backends
# read the same files (see the call site next to Invoke-TypeCheck).
$IntelliSenseExported = $false

# Taken only now: -DryRun exits above without touching anything, and must not leave a lock
# behind for the next run to trip over.
Acquire-RunLock
$IniExisted = Test-Path $IniPath
$IniBackup = if ($IniExisted) { [System.IO.File]::ReadAllBytes($IniPath) } else { $null }
$OriginalRecordFrames = $env:UNLUANOVA_RECORD_FRAMES

try {
    foreach ($B in $Backends) {
        Write-Host ""
        Write-Host "=== Backend $B ===" -ForegroundColor Cyan

        if ($CurrentBackend -ne $B) {
            # Switching the backend without rebuilding is the one thing this script must
            # never do: the ini would name one backend while the DLLs are the other, and
            # every result would be filed - and read - as the wrong backend.
            if ($SkipBuild) {
                throw @"
-Backend $B together with -SkipBuild, while the project is set to '$CurrentBackend'.
-SkipBuild means "test whatever is already built", and what is built here is
$CurrentBackend - editing the ini would only relabel it. Drop -SkipBuild, or run
-Backend $CurrentBackend.
"@
            }

            Write-Host "Setting LuaBackend=$B in $IniPath"
            Set-IniBackend -Value $B
            $CurrentBackend = $B

            # Nothing to delete here on purpose. There used to be a wipe of
            # Plugins/UnLuaNova/Intermediate: UBT does not watch Config/DefaultUnLuaNova.ini,
            # so a backend switch left the previous backend's objects in place and the only way
            # to force the issue was to throw the directory away - 75 MB of build output, and a
            # full recompile whether or not anything relevant changed.
            #
            # The ini is now listed in UnLuaNova.Build.cs' ExternalDependencies, so touching it
            # invalidates the makefile by itself. Assert-BackendBuilt after the build is what
            # catches the case where it did not: it compares the ini to the macro UBT actually
            # compiled in, and refuses to run rather than file results under the wrong backend.
        }

        if (-not $SkipBuild) {
            $BuildArgs = @($TargetName, $Platform, $Configuration, "-Project=$ProjectPath", "-WaitMutex")
            Write-Host "Build  : $BuildBat $($BuildArgs -join ' ')"
            $Bsw = [System.Diagnostics.Stopwatch]::StartNew()
            & $BuildBat @BuildArgs
            $BuildExit = $LASTEXITCODE
            $Bsw.Stop()
            Write-Host "Build exited with $BuildExit in $('{0:N1}' -f $Bsw.Elapsed.TotalSeconds)s"
            if ($BuildExit -ne 0) {
                Write-Host "Build failed for the $B backend - its tests were not run." -ForegroundColor Red
                # Put the ini back *now*, not only at the end of the script: it currently
                # names a backend that was never built, and the next -SkipBuild run would
                # silently test the old binaries under the new name.
                Restore-IniBackend
                $CurrentBackend = $IniOriginal
                $ExitCode = 1
                continue
            }
        }

        # Only now is it safe to name this run's backend: the build produced it and the
        # artifacts agree with the ini. Everything downstream reads it from here.
        Assert-BackendBuilt -Value $B

        if ($BuildOnly) {
            Write-Host "BuildOnly  : $B 编译通过，跳过测试运行。" -ForegroundColor Green
            continue
        }

        $RunDir = Join-Path $OutputDir $B.ToLower()
        New-Item -ItemType Directory -Path $RunDir -Force | Out-Null
        [System.IO.File]::WriteAllText((Join-Path $RunDir "backend.txt"), $B,
            (New-Object System.Text.UTF8Encoding($false)))
        $RunArgLine = ((Get-TestArgs -ReportDir $RunDir) | ForEach-Object { Format-EditorArg $_ }) -join ' '
        $RunExe = if ($IsGameCarrier) { Resolve-GameExe } else { $EditorCmd }
        # $(...) and not just (...): a bare `if` in an argument list is parsed as a command
        # name, which fails at run time with "The term 'if' is not recognized" - inside a
        # subexpression it is an expression, which is what is meant here.
        $RunLog = Join-Path $RunDir $(if ($IsGameCarrier) { "Game.log" } else { "Editor.log" })
        if (-not (Test-Path $RunExe)) {
            # Only reachable for -game: the editor cmd is checked at startup. The usual
            # cause is a project whose Game target has never been built here.
            throw "'$RunExe' not found - build the '$ProjectName' target first (this run builds it unless -SkipBuild was passed)."
        }
        Write-Host "Cmd    : $RunExe $RunArgLine"

        # The editor does not empty Saved/Screenshots, so a run that takes no picture would
        # publish the previous run's PNGs as its own.
        if (-not $NoScreenshots) { Clear-StaleScreenshots }

        # Suppress frame capture even when running an older prebuilt test binary.
        $env:UNLUANOVA_RECORD_FRAMES = '0'

        # 本轮开始时间：联网 peer 的产物（topology / 报告 / 日志）都按它区分"这轮的"和
        # "以前留下的" —— 那个目录不会自己清理，不区分就会一直翻旧账。
        $RunStart = Get-Date

        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        # ProcessInfo + Process.Start, not Start-Process:
        #  - the arguments are one pre-joined, pre-quoted string, because Start-Process joins
        #    an array with spaces and adds no quoting of its own - that is how -ExecCmds lost
        #    its quotes once and the editor came up without a single test queued;
        #  - UseShellExecute=false, because a process started through ShellExecute does not
        #    report its exit code. Start-Process reported none, so "Editor exited with  in
        #    234.0s" and a failing run could pass as success.
        # It is also what -NoNewWindow did: the child keeps this console.
        $EditorPsi = New-Object System.Diagnostics.ProcessStartInfo
        $EditorPsi.FileName = $RunExe
        $EditorPsi.Arguments = $RunArgLine
        $EditorPsi.UseShellExecute = $false
        $EditorProc = [System.Diagnostics.Process]::Start($EditorPsi)
        try {
            if ($ReleaseManaged) {
                $EditorProc.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::AboveNormal
                $MetadataDir = Join-Path $RunDir 'metadata'
                New-Item -ItemType Directory -Path $MetadataDir -Force | Out-Null
                [ordered]@{ pid = $EditorProc.Id; executable = $RunExe;
                    priority_class = $EditorProc.PriorityClass.ToString(); render_offscreen = [bool]$RenderOffScreen;
                    recorded_utc = [DateTime]::UtcNow.ToString('o') } |
                    ConvertTo-Json | Set-Content (Join-Path $MetadataDir 'process.json') -Encoding UTF8
                Write-Host 'Scheduling   : AboveNormal for this release test process.'
            }
            while (-not $EditorProc.WaitForExit(3000)) {
                if ($sw.Elapsed.TotalMinutes -ge $TimeoutMinutes) {
                    & taskkill.exe /PID $EditorProc.Id /T /F | Out-Null
                    throw "Automation exceeded $TimeoutMinutes minutes. See $RunLog"
                }
            }
        } finally {
            if (-not $EditorProc.HasExited) { & taskkill.exe /PID $EditorProc.Id /T /F | Out-Null }
            Stop-NetPeers -Since $RunStart
        }
        $EditorExit = $EditorProc.ExitCode
        $sw.Stop()

        Write-Host "$($CarrierChoice.Carrier) exited with $EditorExit in $('{0:N1}' -f $sw.Elapsed.TotalSeconds)s"
        if ($EditorExit -ne 0) { $ExitCode = $EditorExit }

        # Screenshots land in the project's Saved dir, not next to the JSON; pull them in so
        # the report is self-contained.
        if (-not $NoScreenshots) {
            $Dest = Join-Path $RunDir "screenshots"
            $Copied = Copy-Screenshots -Dest $Dest
            if ($Copied -gt 0) {
                Write-Host "Media     : $Copied screenshot(s) -> $Dest"
            } else {
                Write-Host "Media     : no screenshots were taken." -ForegroundColor Yellow
            }

        }

        $JsonCount = @(Get-ChildItem -Path $RunDir -Filter "*.json" -File -ErrorAction SilentlyContinue).Count
        if ($JsonCount -eq 0) {
            Write-Host "No automation JSON in '$RunDir' - check '$RunLog'." -ForegroundColor Yellow
            Show-NoTestsQueuedHint -LogPath $RunLog
        }

        # A crash exports nothing, so without this the run looks like "0 tests" and the only
        # clue is a half-megabyte log. Show-CrashSummary says nothing unless the log really
        # contains a crash, so calling it on a merely failed run is harmless.
        Show-CrashSummary -LogPath $RunLog

        # 联网 peer 的报告与日志（headless 的服务器只有日志）—— 归到这一轮的目录里，
        # 于是"某个 peer 没写报告"这句话后面能直接跟上一个可打开的文件。
        Copy-NetArtifacts -RunDir $RunDir -Since $RunStart | Out-Null

        # Static type checking is not an automation test, so it never shows up in the JSON the
        # engine exports - run it here and leave its summary next to the JSON, which is the
        # only place the report generator will find it.
        if (-not $NoReport -and -not $ReleaseManaged) {
            # 声明导出走的是编辑器 commandlet。-game 这一轮只编了游戏目标，编辑器那份
            # DLL 可能根本不存在（或者属于另一个后端）—— 与其拿一份来路不明的二进制去
            # 导出声明，不如明说这一步没做。
            $EditorBackendDll = Join-Path $PluginRoot ("Binaries/Win64/UnrealEditor-" + $B + ".dll")
            if ($IsGameCarrier -and -not (Test-Path $EditorBackendDll)) {
                Write-Host "TypeCheck : 跳过 —— 本次跑在 -game 上，没有 $B 的编辑器二进制（未编 <Project>Editor 目标）。" -ForegroundColor Yellow
            } else {
                # Declarations first, and only once per run: the commandlet writes both dialects
                # whatever the backend is, so the second backend would pay another editor launch
                # for the same files.
                if (-not $IntelliSenseExported) {
                    $IntelliSenseExported = Export-IntelliSense
                }

                Invoke-TypeCheck -RunDir $RunDir -Backend $B
            }
        }

        if (-not $NoReport) {
            $Generator = Join-Path $ToolsDir "GenerateTestReport.py"
            if (-not (Test-Path $Generator)) {
                throw "Report generator not found at '$Generator'."
            }

            $GenArgs = @($Generator, "--reports", $RunDir, "--out", $RunDir, "--backend", $B)
            if ($Publish) { $GenArgs += "--publish" }
            # 用例被改名/误删/被 filter 漏掉时，"全绿"是最坏的结果 —— 少跑一半和全过
            # 看起来一模一样。基线比对让报告自己说出这件事。
            $GenArgs += @("--baseline", (Join-Path $RepoRoot "Tools/baseline/$($B.ToLower()).json"))
            if ($UpdateBaseline) { $GenArgs += "--update-baseline" }
            if ($FailOnDrift) { $GenArgs += "--fail-on-drift" }

            # 每个用例"需要什么环境"的分类，报告里用它算出 A–E 各占多少秒。
            # 没有这一节，"把某一类挪出编辑器进程能省多少"只能靠估 —— 而这是决定要不要
            # 动测试框架的依据。分类是纯静态分析源码，跑一次几毫秒；失败也不该拖垮报告。
            $Classifier = Join-Path $ToolsDir "ClassifyTests.py"
            $ClassFile = $ClassificationPath
            if (Test-Path $Classifier) {
                & python $Classifier --out $ClassFile 2>&1 | Out-Null
                if (Test-Path $ClassFile) { $GenArgs += @("--classification", $ClassFile) }
            }

            & python @GenArgs
            if ($LASTEXITCODE -ne 0) {
                Write-Host "Report generation failed (exit $LASTEXITCODE)." -ForegroundColor Red
                if ($ExitCode -eq 0) { $ExitCode = $LASTEXITCODE }
            }

            # Same "confirm it by hand" moment as the test-name baseline, and for the same
            # reason: a machine that happened to be busy would otherwise enshrine its slow
            # run as the thing every later run is judged against.
            if ($UpdateBaseline) { Publish-PerfBaseline -Backend $B }

            $Summary = Join-Path $RunDir "summary.json"
            if (Test-Path $Summary) {
                # -Encoding UTF8: the summary carries test names in Chinese, and the
                # default encoding mangles them badly enough that ConvertFrom-Json throws
                # before the totals are ever read - a crash report then loses its verdict.
                $S = Get-Content $Summary -Raw -Encoding UTF8 | ConvertFrom-Json
                Write-Host ""
                Write-Host ("Result ($B) : {0} passed, {1} failed, {2} total" -f $S.passed, $S.failed, $S.total)
                Write-Host ("Report ($B) : {0}" -f (Join-Path $RunDir "report.md"))
                if ($S.failed -gt 0) { $ExitCode = 1 }
                if ($S.crashed) {
                    # A crash exports no JSON, so "0 tests" would otherwise leave this run
                    # looking green to anything that only checks the exit code.
                    Write-Host ("CRASHED ($B) : {0}" -f $S.crash_test) -ForegroundColor Red
                    $ExitCode = 1
                }
            }
        }
    }
}
finally {
    # The ini is global state and this script edits it, so it goes back the way it was
    # found - including after a failed build, which restores it above and again here.
    Restore-IniBackend
    $env:UNLUANOVA_RECORD_FRAMES = $OriginalRecordFrames
    Release-RunLock
}

exit $ExitCode

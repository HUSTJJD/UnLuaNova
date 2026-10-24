<#
.SYNOPSIS
    Runs the UnLuaNova automation tests and writes a readable report.

.DESCRIPTION
    Thin wrapper around the UE commandlet that everyone on the team can call the same
    way, so "did it pass?" never depends on how somebody spelled -ExecCmds:

        UnrealEditor-Cmd.exe <uproject> -unattended -NoSplash
            -ExecCmds="Automation RunTests <filter>; Quit"
            -ReportExportPath=<dir> -abslog=<dir>\Editor.log

    The Lua backend is a compile-time choice (Config/DefaultUnLuaNova.ini ->
    LuaBackend, read by UnLuaNova.Build.cs), so this builds before it tests: otherwise the
    binaries silently come from whichever backend was built last and the report labels the
    run with the wrong one. -Backend Both (the default) therefore switches the setting,
    rebuilds, runs, then does the same for the other backend - one report directory per
    backend, and two pages when publishing.

    That setting is global state, so switching it is treated as a transaction: an exclusive
    lock keeps two runs from interleaving (a second Build.bat would otherwise just wait on
    UBT's mutex and then run the first run's binaries), the ini is restored the moment a
    build fails rather than only at the end, and before the editor starts the run checks
    the artifacts UBT left behind - not the directory name - to confirm which backend is
    actually on disk. -SkipBuild still switches nothing.

    After each run it collects the JSON the automation framework exported, grabs any
    screenshots the tests took, and hands both to Tools/GenerateTestReport.py, which
    writes the actual conclusions (report.md + summary.json).

    The tests need a real UE build, so this always runs locally - see
    Docs/CN/Testing.md. Use -Publish when you want the result on the docs site; CI only
    renders what has been committed.

.PARAMETER EnginePath
    UE root (the directory containing Engine/). Left empty it is discovered: the
    UNREAL_ENGINE_PATH environment variable, then a source build in this repository or one
    of its parents, then any build registered by UnrealVersionSelector. Nothing here is
    machine specific, so the script works on a fresh clone.

.PARAMETER ProjectPath
    Path to the .uproject. Left empty, the first .uproject found in the repository root is
    used - so this does not have to be edited per checkout either.

.PARAMETER Backend
    Which Lua backend to build and test: Lua, Luau, or Both (default). Both runs the whole
    thing twice - switch the setting, rebuild, test - and reports each backend separately.
    The project config is put back the way it was afterwards.

.PARAMETER Platform
    Platform handed to UBT. Default Win64.

.PARAMETER Configuration
    Configuration handed to UBT. Default Development.

.PARAMETER SkipBuild
    Skip the build and test whatever binaries are already there. Refuses to run if the
    backend being asked for is not the one the project is currently set to: switching the
    setting without rebuilding would only relabel the existing binaries, and every result
    would be filed - and read - as the wrong backend.

.PARAMETER Filter
    Automation test filter, matched as a prefix. Default "UnLuaNova" runs everything the
    plugin registers. Use "UnLuaNova.API" etc. to narrow it down.

.PARAMETER Map
    Map to open before running. Most UnLuaNova tests build their own world, so this is
    only needed for the scene/showcase tests that expect a specific level.

.PARAMETER ExtraCommands
    Console commands appended after the suite, separated by ';'. Use it for screenshots in
    a run that is not driven by a test, e.g.
    -ExtraCommands "UnLuaNovaTest.Screenshot Showcase_Binding".

.PARAMETER NoScreenshots
    Do not collect the pictures the tests took. They are collected by default: the docs site
    shows them (Docs/CN/Showcase.md and the gallery in each report), and a run that leaves
    them behind is a run whose report has an empty gallery for no reason anybody can see
    afterwards.

.PARAMETER NoVideo
    Skip recording entirely - both the whole-run screen recording and the showcase clips.
    Recording is on by default, and it is two different things: the run records the editor
    window from start to finish (see -NoSessionVideo), and the showcase tests additionally
    capture a burst of frames that the report generator encodes into one .mp4 each. No
    system-wide ffmpeg is needed - it comes from the pip package in Tools/requirements.txt -
    and a burst that cannot be encoded fails the run instead of quietly leaving a pile of PNGs.

.PARAMETER NoSessionVideo
    Skip only the whole-run screen recording and keep the showcase clips. The showcase clips
    are recorded by the tests themselves, so they stop where the showcase tests stop: a hang
    in Issue430, a crash in a spec, the 40 seconds of editor startup - none of it is in any
    clip. The session recording is what covers the run end to end.

.PARAMETER NullRHI
    Run headless (-NullRHI). Faster, but it is not the same suite: Slate does not tick UMG
    without a viewport, so tests that need one - Issue434 waits for a widget's Tick - are
    skipped by the framework rather than run. Use it when you want a quick answer, not when
    you want the whole suite.

.PARAMETER Sound
    Keep audio on. It is off by default (-nosound): every map test ends the previous test's
    PIE session, and shutting that session down means shutting its audio device down. On a
    machine whose device is slow to stop, that teardown can block the editor's main thread
    for good - one run sat on a single frame for 40 minutes and never moved again. Nothing
    in this suite listens to audio, so the device is simply not created.

.PARAMETER Publish
    Copy the generated report.md and its screenshots into Docs/ so the GitHub Pages
    build picks them up.

.PARAMETER NoReport
    Skip report generation; just run the tests and leave the raw JSON behind.

.PARAMETER DryRun
    Print what would run - resolved engine, project, output dir and the full command line -
    and exit without starting the editor. Use it to check the discovered paths.

.PARAMETER BuildOnly
    Build and stop: compile each requested backend, verify the binaries really are that
    backend, and exit without starting the editor or generating a report. This is what a
    nightly scheduled task wants - it catches a spec that no longer compiles within a day,
    without paying for a full test run. Mutually exclusive with -SkipBuild.

.EXAMPLE
    .\Tools\RunTests.ps1

.EXAMPLE
    .\Tools\RunTests.ps1 -Filter UnLuaNova.API -Publish

.EXAMPLE
    .\Tools\RunTests.ps1 -Backend Lua -Publish

    Builds the plain-Lua backend, runs the suite and publishes Docs/test-lua-report.md only.

.EXAMPLE
    .\Tools\RunTests.ps1 -Backend Luau -BuildOnly

    Compiles the Luau backend and exits - no editor, no report. Exits non-zero if it does
    not compile, which is the point.
#>

[CmdletBinding()]
param(
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
    [switch] $NoScreenshots,
    [switch] $NoVideo,
    # 只关掉"整场录屏"，保留 showcase 剪辑：剪辑是用例自己录的，卡死/崩溃/启动那几十秒都不在里面。
    [switch] $NoSessionVideo,
    # 录屏帧率。整场录屏动辄几分钟，15fps 足够看清"卡在哪一帧"，文件也只有一半大。
    [int] $SessionFps = 15,
    [switch] $NullRHI,
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

# Everything is derived from where this script lives, so no path in here belongs to one
# particular machine.
$RepoRoot = Split-Path -Parent $PSScriptRoot
$ToolsDir = Join-Path $RepoRoot "Tools"

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

# --- one run at a time -------------------------------------------------------------
# The backend is global state: an ini value plus whatever the last build linked. Two runs
# overlap at their peril - the second Build.bat only waits on UBT's mutex and then uses
# whatever the first one produced, which is exactly how a run filed under 'lua' once ran
# Luau binaries. This lock makes that impossible, and it records the owning PID so a run
# that was killed does not block the next one forever.
$LockPath = Join-Path $RepoRoot "Saved/RunTests.lock"
$LockStream = $null

function Acquire-RunLock {
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
    $Definitions = Join-Path $PluginRoot "Intermediate/Build/Win64/x64/UnrealEditor/Development/UnLuaNova/Definitions.UnLuaNova.h"
    if (-not (Test-Path $Definitions)) { return $null }

    $Text = Get-Content $Definitions -Raw -Encoding UTF8 -ErrorAction SilentlyContinue
    if (-not $Text) { return $null }
    if ($Text -match "UNLUANOVA_LUA_BACKEND\s+1") { return "Luau" }
    if ($Text -match "UNLUANOVA_LUA_BACKEND\s+0") { return "Lua" }
    return $null
}

function Assert-BackendBuilt {
    param([string] $Value)

    $Dll = Join-Path $PluginRoot ("Binaries/Win64/UnrealEditor-" + $Value + ".dll")
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

# --- screenshots and clips -----------------------------------------------------------
# Screenshots go to the project's Saved/Screenshots, not next to the JSON the automation
# framework exports, so they have to be pulled into the run directory for the report to be
# self-contained. Two things make that more than a copy:
#
#   - The directory is not cleaned by the editor. A run that takes no picture at all would
#     otherwise publish last week's PNGs as this run's gallery, which is worse than an empty
#     one: nobody can tell the pictures are not from this run. So it is emptied first.
#   - A clip is a frame sequence (Name_0001.png, Name_0002.png, ...) that becomes a video in
#     the report generator. Encoding is not optional and does not need a system ffmpeg: it
#     comes from the pip package in Tools/requirements.txt. A burst that cannot be encoded
#     fails the run rather than passing as a pile of stills.

function Clear-StaleScreenshots {
    $ShotDir = Join-Path $RepoRoot "Saved/Screenshots/WindowsEditor"
    if (-not (Test-Path $ShotDir)) { return }
    Get-ChildItem -Path $ShotDir -Filter "*.png" -File -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
}

# --- whole-run screen recording ---------------------------------------------------------
# The showcase clips are recorded by the tests themselves, so they cover exactly the showcase
# tests and nothing else: not the 40 seconds of editor startup, not the PIE teardown between
# two regression tests, and above all not a hang - a run that freezes produces no clip at all,
# which is the one run everybody wants to watch afterwards.
#
# So the run records the editor window end to end: the recorder starts with the editor and is
# stopped when the editor exits, and the .mp4 lands in <RunDir>/videos/session.mp4, where the
# report generator already looks for video.

function Resolve-FfmpegExe {
    $cmd = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    # Same source the report generator uses for its clips, so a machine that can produce a
    # report can record the run - no system-wide install, nothing on PATH.
    try {
        $out = & python -c "import imageio_ffmpeg;print(imageio_ffmpeg.get_ffmpeg_exe())" 2>$null
        if ($LASTEXITCODE -eq 0 -and $out) {
            $exe = ($out | Select-Object -Last 1).Trim()
            if ($exe -and (Test-Path -LiteralPath $exe)) { return $exe }
        }
    } catch { }
    return $null
}

function Wait-EditorWindow {
    param([string] $ProcessName, [int] $TimeoutSeconds = 90)

    # The window does not exist the moment the process does: the editor spends its first
    # seconds loading, and gdigrab cannot capture a title that is not there yet. Wait for it
    # rather than recording forty seconds of desktop.
    $Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $Deadline) {
        $Proc = Get-Process -Name $ProcessName -ErrorAction SilentlyContinue |
            Where-Object { $_.MainWindowTitle } | Select-Object -First 1
        if ($Proc) { return $Proc }

        # Nothing left to wait for - the editor died on startup. Sitting out the rest of the
        # timeout only delays the desktop fallback.
        if (-not (Get-Process -Name $ProcessName -ErrorAction SilentlyContinue)) { return $null }

        Start-Sleep -Milliseconds 500
    }
    return $null
}

if (-not ("UnLuaNova.WindowTools" -as [type])) {
    Add-Type -Namespace UnLuaNova -Name WindowTools -MemberDefinition @'
        public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
        [DllImport("user32.dll")] public static extern bool IsIconic(System.IntPtr hWnd);
        [DllImport("user32.dll")] public static extern bool IsWindowVisible(System.IntPtr hWnd);
        [DllImport("user32.dll")] public static extern bool ShowWindow(System.IntPtr hWnd, int nCmdShow);
        [DllImport("user32.dll")] public static extern bool GetWindowRect(System.IntPtr hWnd, out RECT r);
        [DllImport("user32.dll")] public static extern int GetSystemMetrics(int index);
        [DllImport("user32.dll")] public static extern System.IntPtr SetThreadDpiAwarenessContext(System.IntPtr context);
'@
}

function Restore-EditorWindow {
    param($Process)

    # A minimized window is the one case that has to be fixed before recording: gdigrab exits
    # with -5 and writes no file at all, while MainWindowTitle stays non-empty, so "the window
    # exists" is not enough to go on. Restore it - no focus stealing, no foreground business,
    # nobody's typing is interrupted.
    $Handle = $Process.MainWindowHandle
    if (-not $Handle -or $Handle -eq [System.IntPtr]::Zero) { return $false }

    if ([UnLuaNova.WindowTools]::IsIconic($Handle)) {
        [void] [UnLuaNova.WindowTools]::ShowWindow($Handle, 9) # SW_RESTORE
        Start-Sleep -Milliseconds 500
        if ([UnLuaNova.WindowTools]::IsIconic($Handle)) { return $false }
    }

    return [UnLuaNova.WindowTools]::IsWindowVisible($Handle)
}

function Get-WindowCaptureRegion {
    param($Process)

    # The editor window's rectangle in *physical* pixels, clamped to the virtual screen.
    #
    # Physical, because gdigrab works in real device pixels while PowerShell - DPI-unaware by
    # default - reports scaled ones: on a 150% display the rect would come back 2/3 of its
    # real size and the capture would be of the wrong part of the screen. Per-monitor-v2
    # awareness for this thread fixes the numbers without touching the rest of the session.
    $Handle = $Process.MainWindowHandle
    if (-not $Handle -or $Handle -eq [System.IntPtr]::Zero) { return $null }

    $Saved = [UnLuaNova.WindowTools]::SetThreadDpiAwarenessContext([System.IntPtr](-4))
    try {
        $Rect = New-Object UnLuaNova.WindowTools+RECT
        if (-not [UnLuaNova.WindowTools]::GetWindowRect($Handle, [ref] $Rect)) { return $null }

        # gdigrab's desktop source starts at (0,0) whatever the virtual screen's origin is, so
        # a monitor sitting left of / above the primary one would give negative offsets.
        $OriginX = [UnLuaNova.WindowTools]::GetSystemMetrics(76) # SM_XVIRTUALSCREEN
        $OriginY = [UnLuaNova.WindowTools]::GetSystemMetrics(77) # SM_YVIRTUALSCREEN
        $ScreenW = [UnLuaNova.WindowTools]::GetSystemMetrics(78) # SM_CXVIRTUALSCREEN
        $ScreenH = [UnLuaNova.WindowTools]::GetSystemMetrics(79) # SM_CYVIRTUALSCREEN

        $X = [Math]::Max($Rect.Left - $OriginX, 0)
        $Y = [Math]::Max($Rect.Top - $OriginY, 0)
        $W = [Math]::Min($Rect.Right - $OriginX, $ScreenW) - $X
        $H = [Math]::Min($Rect.Bottom - $OriginY, $ScreenH) - $Y

        # A maximized window hangs a few pixels over the screen edge; yuv420p has no odd
        # dimensions either. Both are cheaper to fix here than to let gdigrab refuse.
        if ($W -lt 16 -or $H -lt 16) { return $null }
        if ($W % 2) { $W-- }
        if ($H % 2) { $H-- }

        return [pscustomobject]@{ X = $X; Y = $Y; Width = $W; Height = $H }
    } finally {
        [void] [UnLuaNova.WindowTools]::SetThreadDpiAwarenessContext($Saved)
    }
}

function Start-SessionRecording {
    param(
        [Parameter(Mandatory)] [string] $OutFile,
        [Parameter(Mandatory)] [string] $ProcessName,
        [int] $Fps = 15
    )

    $Ffmpeg = Resolve-FfmpegExe
    if (-not $Ffmpeg) {
        Write-Host "Record    : skipped - 没找到 ffmpeg（pip install -r Tools/requirements.txt，或放进 PATH）" -ForegroundColor Yellow
        return $null
    }

    $OutDir = Split-Path -Parent $OutFile
    if ($OutDir) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }

    # Recording starts when the window is up - the forty seconds of editor startup are not
    # worth the megabytes, and the run is only interesting from the first test on.
    $Proc = Wait-EditorWindow -ProcessName $ProcessName
    $Region = if ($Proc -and (Restore-EditorWindow -Process $Proc)) { Get-WindowCaptureRegion -Process $Proc } else { $null }
    if ($Region) {
        # NOT "-i title=<window>": gdigrab reads the window's own surface, and for a
        # D3D-composited window that surface is black. Measured on this machine, capturing a
        # 3840x2088 editor window that way: 8 KB of black frames per 2 seconds, with ffmpeg
        # repeating the last good frame for the rest of the run ("Failed to capture image
        # (error 8)"), i.e. a 333 KB "video" of one still frame. The same window area read off
        # the composed desktop is 1.2 MB per 2 seconds of real picture.
        #
        # So: capture the desktop and crop it to the window. Same pixels, minus the failure.
        # The trade-off is that another window dragged over the editor is now in the video -
        # unavoidable, since the desktop is what the compositor hands out.
        $Source = "desktop"
        $Crop = " -offset_x $($Region.X) -offset_y $($Region.Y) -video_size $($Region.Width)x$($Region.Height)"
        $What = "编辑器窗口区域 $($Region.Width)x$($Region.Height) @ $($Region.X),$($Region.Y)（整屏裁切）"
    } else {
        $Crop = ""
        # No window to crop to: a headless run, a window that never came up, or one that
        # stayed minimized. Record the whole screen instead - a run with no picture at all is
        # worse than one with the desktop in it. Mind the gap: nothing before this point is on
        # tape, which is why the wait is not longer.
        $What = "整个屏幕（没等到可用的编辑器窗口：无头运行 / 窗口没起来 / 最小化后没还原）—— 开头的几十秒不在录像里"
    }

    # Two things the scale filter buys: it caps the width, because the source is the whole
    # virtual screen (7680x2160 on a two-monitor box - recording that at 15fps loses more
    # frames than it keeps), and it forces even dimensions, because yuv420p has no odd width
    # and an editor window is whatever size it was last dragged to.
    $Args = "-hide_banner -loglevel error -y -f gdigrab -framerate $Fps -draw_mouse 0$Crop " +
            "-i `"$Source`" -vf `"scale=2*trunc(min(1920\,iw)/2):-2`" " +
            "-c:v libx264 -preset veryfast -crf 28 -pix_fmt yuv420p -movflags +faststart " +
            "`"$OutFile`""

    $Psi = New-Object System.Diagnostics.ProcessStartInfo
    $Psi.FileName = $Ffmpeg
    $Psi.Arguments = $Args
    $Psi.UseShellExecute = $false
    # stdin is how gdigrab is told to stop: 'q' lets ffmpeg finish the file properly, where
    # killing it leaves an mp4 without a moov atom - i.e. unplayable, and exactly the file
    # somebody opens after a hang.
    $Psi.RedirectStandardInput = $true

    $Proc = [System.Diagnostics.Process]::Start($Psi)
    if (-not $Proc) { return $null }

    Write-Host "Record    : $What -> $OutFile"
    return [pscustomobject]@{ Process = $Proc; OutFile = $OutFile; Started = (Get-Date) }
}

function Stop-SessionRecording {
    param($Recording)

    if (-not $Recording) { return }

    $Proc = $Recording.Process
    try {
        if (-not $Proc.HasExited) {
            $Proc.StandardInput.Write("q")
            $Proc.StandardInput.Flush()
            # Encoding a few minutes of 1280x720 is not instant; give it room rather than
            # killing a file that is one second from being finished.
            if (-not $Proc.WaitForExit(30000)) {
                $Proc.Kill()
                $Proc.WaitForExit(5000)
            }
        }
    } catch {
        # The recorder dying on its own is not this run's failure - say so and move on.
    }

    $Size = if (Test-Path -LiteralPath $Recording.OutFile) {
        (Get-Item -LiteralPath $Recording.OutFile).Length
    } else { 0 }

    if ($Size -gt 0) {
        Write-Host ("Record    : 整场录屏 {0}（{1}）" -f $Recording.OutFile, (Format-DataSize $Size))
    } else {
        Write-Host "Record    : 录屏没有产出文件（ffmpeg 退出码 $($Proc.ExitCode)）。" -ForegroundColor Yellow
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

    $ShotDir = Join-Path $RepoRoot "Saved/Screenshots/WindowsEditor"
    $Files = @(Get-ChildItem -Path $ShotDir -Filter "*.png" -File -ErrorAction SilentlyContinue)
    if ($Files.Count -eq 0) { return 0 }

    New-Item -ItemType Directory -Path $Dest -Force | Out-Null
    foreach ($F in $Files) { Copy-Item $F.FullName -Destination $Dest -Force -ErrorAction SilentlyContinue }
    return $Files.Count
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
$IniBackup = if ($IniExisted) { [System.IO.File]::ReadAllText($IniPath) } else { $null }

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
            [System.IO.File]::WriteAllText($IniPath, $IniBackup, (New-Object System.Text.UTF8Encoding($false)))
        } elseif (Test-Path $IniPath) {
            Remove-Item $IniPath -Force
        }
    } catch {
        Write-Warning "Could not restore '$IniPath' - put LuaBackend back by hand. $_"
    }
}

$Backends = if ($Backend -eq "Both") { @("Lua", "Luau") } else { @($Backend) }
$TargetName = ([System.IO.Path]::GetFileNameWithoutExtension($ProjectPath)) + "Editor"

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

function Get-EditorArgs {
    param([string] $ReportDir)

    # Automation exports one JSON per test into -ReportExportPath; the log goes next to it
    # so a failed run can be read without hunting through Saved/Logs.
    $LogFile = Join-Path $ReportDir "Editor.log"

    # ExtraCommands run after the suite, which is where screenshots belong - they need the
    # world the tests left behind, not the one at startup.
    $Cmds = @("Automation RunTests $Filter")
    if ($ExtraCommands) { $Cmds += $ExtraCommands }
    $Cmds += "Quit"

    $A = @($ProjectPath)
    if ($Map) { $A += $Map }
    $A += @(
        "-unattended"
        "-NoSplash"
        "-stdout"
        # Quoted on purpose: the value is one argument holding spaces and a semicolon. UE
        # reads -ExecCmds out of the raw command line, so the quotes have to be part of the
        # string the child sees (Format-EditorArg leaves an already-quoted argument alone).
        "-ExecCmds=`"$($Cmds -join '; ')`""
        "-ReportExportPath=$ReportDir"
        "-abslog=$LogFile"
    )
    # Rendering stays on unless it is explicitly turned off: with -NullRHI the framework
    # drops the tests that need a viewport, so the run is a different (smaller) suite rather
    # than the same one done faster. -NullRHI is opt-in for that reason.
    if ($NullRHI) { $A += "-NullRHI" }
    # Audio is the other way round: off unless asked for. Opening a map ends the previous
    # test's PIE session, which tears an audio device down, and a device that is slow to
    # stop takes the editor's main thread with it - a run once froze inside
    # FAudioMixerWasapi::StopAudioStream() and never ticked again.
    if (-not $Sound) { $A += "-nosound" }
    return $A
}

Write-Host "Engine       : $EnginePath"
Write-Host "Project      : $ProjectPath"
Write-Host "Filter       : $Filter"
Write-Host "Backend(s)   : $($Backends -join ', ')"
Write-Host "Output       : $OutputDir"
Write-Host "Media        : $(if ($NoScreenshots) { 'off' } else { 'screenshots' })$(if ($NoVideo -or $NoScreenshots) { '' } else { ' + clips (encoded by the report generator)' })"
if ($NullRHI -and -not $NoScreenshots) {
    # Not an error: the suite still runs, it just has nothing to photograph.
    Write-Host "               -NullRHI: no viewport, so no picture will come out of this run." -ForegroundColor Yellow
}
Write-Host "Audio        : $(if ($Sound) { 'on (-Sound)' } else { 'off (-nosound)' })"

if ($DryRun)
{
    foreach ($B in $Backends) {
        $RunDir = Join-Path $OutputDir $B.ToLower()
        Write-Host ""
        Write-Host "--- $B -> $RunDir"
        Write-Host "Build  : $BuildBat $TargetName $Platform $Configuration -Project=`"$ProjectPath`""
        $ArgLine = ((Get-EditorArgs -ReportDir $RunDir) | ForEach-Object { Format-EditorArg $_ }) -join ' '
        Write-Host "Cmd    : $EditorCmd $ArgLine"
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
        $RunArgLine = ((Get-EditorArgs -ReportDir $RunDir) | ForEach-Object { Format-EditorArg $_ }) -join ' '
        Write-Host "Cmd    : $EditorCmd $RunArgLine"

        # The editor does not empty Saved/Screenshots, so a run that takes no picture would
        # publish the previous run's PNGs as its own.
        if (-not $NoScreenshots) { Clear-StaleScreenshots }

        # Read by the showcase tests: they capture a burst of frames while it is set.
        # Recording is on unless -NoVideo; the frames are encoded by the report generator.
        if ($NoVideo) {
            $env:UNLUANOVA_RECORD_FRAMES = $null
        } elseif ($env:UNLUANOVA_RECORD_FRAMES) {
            # already set by the caller - leave it alone
        } else {
            # 帧数，不是帧率：showcase 用例每个 tick 抓一张截图，抓满这么多张为止。
            # 每一张都是一次完整的视口回读 + PNG 编码，是跑测试时最主要的卡顿来源
            # （4 个 showcase 各 60 帧时，它们占了整轮 183s 里的 77s）。
            # 30 张 + 报告生成器按 15fps 编码 = 仍是 2 秒的剪辑，截图数少一半。
            $env:UNLUANOVA_RECORD_FRAMES = "30"
        }

        # Launched with Start-Process and not with `&` on purpose: `&` blocks until the editor
        # exits, and by then there is nothing left to record. Handing the recorder the live
        # process is what makes "from the first second to the last" possible.
        $EditorProcessName = [System.IO.Path]::GetFileNameWithoutExtension($EditorCmd)
        $WantSessionVideo = -not $NoVideo -and -not $NoSessionVideo -and -not $NullRHI
        $Recording = $null

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
        $EditorPsi.FileName = $EditorCmd
        $EditorPsi.Arguments = $RunArgLine
        $EditorPsi.UseShellExecute = $false
        $EditorProc = [System.Diagnostics.Process]::Start($EditorPsi)
        try {
            if ($WantSessionVideo) {
                $Recording = Start-SessionRecording -OutFile (Join-Path $RunDir "videos/session.mp4") `
                    -ProcessName $EditorProcessName -Fps $SessionFps
            } elseif ($NullRHI -and -not $NoVideo -and -not $NoSessionVideo) {
                Write-Host "Record    : skipped - -NullRHI 没有画面可录。" -ForegroundColor Yellow
            }

            $EditorProc.WaitForExit()
        } finally {
            # Always stop it: a hung editor is exactly the run that has to end up on disk, and
            # it is stopped from here - not by the editor, which is the thing that hung.
            Stop-SessionRecording -Recording $Recording
        }
        $EditorExit = $EditorProc.ExitCode
        $sw.Stop()

        Write-Host "Editor exited with $EditorExit in $('{0:N1}' -f $sw.Elapsed.TotalSeconds)s"
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
            Write-Host "No automation JSON in '$RunDir' - check '$(Join-Path $RunDir 'Editor.log')'." -ForegroundColor Yellow
            Show-NoTestsQueuedHint -LogPath (Join-Path $RunDir "Editor.log")
        }

        # A crash exports nothing, so without this the run looks like "0 tests" and the only
        # clue is a half-megabyte log. Show-CrashSummary says nothing unless the log really
        # contains a crash, so calling it on a merely failed run is harmless.
        Show-CrashSummary -LogPath (Join-Path $RunDir "Editor.log")

        # Static type checking is not an automation test, so it never shows up in the JSON the
        # engine exports - run it here and leave its summary next to the JSON, which is the
        # only place the report generator will find it.
        if (-not $NoReport) {
            # Declarations first, and only once per run: the commandlet writes both dialects
            # whatever the backend is, so the second backend would pay another editor launch
            # for the same files.
            if (-not $IntelliSenseExported) {
                $IntelliSenseExported = Export-IntelliSense
            }

            Invoke-TypeCheck -RunDir $RunDir -Backend $B
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

            & python @GenArgs
            if ($LASTEXITCODE -ne 0) {
                Write-Host "Report generation failed (exit $LASTEXITCODE)." -ForegroundColor Red
                if ($ExitCode -eq 0) { $ExitCode = $LASTEXITCODE }
            }

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
    Release-RunLock
}

exit $ExitCode

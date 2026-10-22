---
title: "Testing and Automation"
description: "Running the suite on UE's Automation framework, generating reports and screenshots"
---

# Testing and Automation

UnLuaNova tests with the **native UE Automation framework** - no third-party framework. Every
test lives in the `UnLuaNovaTestSuite` plugin, and one run gives you the pass rate, the failure
reasons and the screenshots.

> This project (`UnLuaNovaDemo`) enables it in its `.uproject`, so the tests are always ready to
> run. It is a **development tool, not a runtime dependency**, so the plugin keeps
> `EnabledByDefault: false` - using UnLuaNova in another project does not drag it along; enable
> it when you want to run the tests.

## Layout

```
Plugins/UnLuaNovaTestSuite/
├── Source/UnLuaNovaTestSuite/
│   ├── Public/                    test infrastructure (latent commands, fixtures, assertion macros)
│   │   ├── TestCommands.h/.cpp        latent commands: open a level / take a screenshot
│   │   ├── UnLuaNovaTestCommon.h      test base classes and macros
│   │   └── UnLuaNovaTestHelpers.h     UObject stubs, assertion macros, static exports
│   ├── Private/Specs/             unit tests (BDD style, BEGIN_DEFINE_SPEC)
│   └── Private/Tests/             regression and instance tests (Issue###Test)
└── Content/Tests/                 levels and assets for the scenario tests
```

The companion scripts live in `Tools/` and are invoked from the repository root:

| Script | What it does |
|---|---|
| `Tools/RunTests.ps1` | Builds, runs the suite, collects screenshots/recordings, generates the report — the subject of this page |
| `Tools/GenerateTestReport.py` | Turns the JSON the automation framework exported into the Markdown/HTML report (`RunTests.ps1` calls it) |
| `Tools/CheckLuauTypes.ps1` | Luau backend: static type check via `luau-analyze` plus the generated `UnLuaNova.d.lua` |
| `Tools/CheckLuaTypes.ps1` | Lua backend: static check via LuaLS (`lua-language-server`) plus the generated EmmyLua annotations |
| `Tools/Build-LuauAnalyze.ps1` | cmake-builds `luau-analyze` from the Luau submodule (it is in there; UBT just skips `CLI/`) |
| `Tools/Build-LuaLS.ps1` | Builds LuaLS from the `Tools/lua-language-server` submodule; pulls its 21 nested submodules on first run |
| `Tools/Rename-To-UnLuaNova.ps1` | Migration script: bulk-renames an UnLua project/codebase to `UnLuaNova` (`UnLua` → `UnLuaNova`, `UUnLuaSettings` → `UUnLuaNovaSettings`, …). Run it once when migrating; not part of running tests |
| `Tools/Install-NightlyTask.ps1` | Registers / removes the Windows scheduled task that compiles the test code once a night (`-BuildOnly`), see "Compile at least once a day" below |

## One command

```powershell
# every UnLuaNova test: build first, then run both backends, one report each
.\Tools\RunTests.ps1

# one area only
.\Tools\RunTests.ps1 -Filter UnLuaNova.API

# just the three optional extension plugins (LuaRapidjson / LuaProtobuf / LuaSocket)
.\Tools\RunTests.ps1 -Filter UnLuaNova.Extensions

# one backend only
.\Tools\RunTests.ps1 -Backend Luau

# refresh the sample images only (fast) and publish them to the docs site
.\Tools\RunTests.ps1 -Filter UnLuaNova.Showcase -Backend Luau -Publish

# recording is on by default - this just publishes the result
.\Tools\RunTests.ps1 -Publish

# stills only
.\Tools\RunTests.ps1 -NoVideo
```

What the script wraps - so nobody has to type `-ExecCmds` by hand:

```
UnrealEditor-Cmd.exe <uproject> -unattended -NoSplash -stdout
    -ExecCmds="Automation RunTests <filter>; Quit"
    -ReportExportPath=<dir> -abslog=<dir>\Editor.log
```

`-Backend` defaults to `Both`, so "run everything" above is really **two rounds**: switch the
setting, build, run, switch back, build, run - then two independent reports. See "One report per
backend" below.

### Common switches

| Switch | Default | Meaning |
|---|---|---|
| `-EnginePath` | auto-discovered | UE source root, see "How paths are resolved" |
| `-ProjectPath` | auto-discovered | project file: the first `.uproject` under the repo root |
| `-DryRun` | off | print the commands, do not launch the editor or build |
| `-Backend` | `Both` | which Lua backend to run: `Lua`, `Luau` or `Both` |
| `-Platform` / `-Configuration` | `Win64` / `Development` | passed to UBT |
| `-SkipBuild` | off | skip building and run the existing binaries; **never combined with a backend switch** |
| `-BuildOnly` | off | build and stop: compile each backend and exit without launching the editor or generating a report. For nightlies and for "does this spec still compile?"; mutually exclusive with `-SkipBuild` |
| `-Filter` | `UnLuaNova` | test filter, prefix matched |
| `-Map` | empty | which level the scenario tests should open; most tests build their own World |
| `-OutputDir` | `Saved/TestReports/<timestamp>` | report root, one sub-directory per backend |
| `-ExtraCommands` | empty | extra console commands to run afterwards (used for screenshots) |
| `-NoScreenshots` | off | do not collect screenshots. **They are collected by default**: the docs site and the report gallery run on them |
| `-NoVideo` | off | do not record. **Recording is on by default**: the showcase tests record a frame sequence and the report generator encodes it into an `.mp4` |
| `-FailOnTypeErrors` | off | exit non-zero when the static checker reports errors (by default they are only recorded). `luau-analyze` on the Luau backend, LuaLS on the Lua backend |
| `-FailOnDrift` | off | exit non-zero when the test list no longer matches the baseline (by default it only warns), see "Test list baseline" below |
| `-UpdateBaseline` | off | write this run's test list as the new baseline and commit it - a reviewed, manual act |
| `-NullRHI` | off | headless: faster, but **not the same set of tests**, and no frames to capture - see below |
| `-Publish` | off | copy the report and screenshots into `Docs/` for Pages |
| `-NoReport` | off | run the tests only, generate no report |

The script exits `1` when anything failed, so it can be hooked into any scheduler.

### Tests that need a real viewport

By default the suite runs **with a real viewport** (no `-NullRHI`), which means the whole set
runs. Under `-NullRHI`, UMG widgets are simply **not ticked** - Slate has nothing to draw, so
`SObjectWidget::Tick` never runs, and any test that waits for a widget to tick waits forever. It
looks exactly like "the thing under test is broken"; Issue434 is such a test, because it relies on
the widget's tick to `Broadcast` a multicast delegate.

Those tests register with `BEGIN_VIEWPORT_TESTSUITE` (which carries
`EAutomationTestFlags::NonNullRHI`): with `-NullRHI` on the command line the framework drops them
from that round entirely instead of recording a failure - they did not run, so they are neither
passed nor failed. `-NullRHI` is therefore the switch for "I want a fast answer", and the price
is a report whose total is a couple of tests short:

```powershell
# the full set (default)
.\Tools\RunTests.ps1

# a quick pass: minus the tests that need a viewport
.\Tools\RunTests.ps1 -NullRHI
```

The tests that were not run are not counted as passed - calling "cannot run headless" a pass
would be a false result.

## Test list

The first column is exactly what `-Filter` matches, so `.\Tools\RunTests.ps1 -Filter
UnLuaNova.API.TArray` runs that one row. The list comes straight from the registration macros
under `Private/` (`BEGIN_DEFINE_SPEC` / `IMPLEMENT_SIMPLE_AUTOMATION_TEST` /
`BEGIN_VIEWPORT_TESTSUITE`) - **when tests are added or removed, the code wins**, and
`Tools/baseline/*.json` is the drift reference.

| Registered name | What it covers | Notes |
|---|---|---|
| `UnLuaNova.API.Base` | `Push` / `RunChunk` / `Call` / `CallTableFunc`, including by-ref parameters | |
| `UnLuaNova.API.Binding.Dynamic` | dynamic binding: script path given at SpawnActor | editor only |
| `UnLuaNova.API.Binding.Multiple` | multiple binding: one script bound to several classes | editor only |
| `UnLuaNova.API.Binding.Overridden` | overriding: one script bound to several classes | editor only |
| `UnLuaNova.API.Binding.Priority` | binding conflict: static binding wins over dynamic | editor only |
| `UnLuaNova.API.Binding.Static` | static binding: script path from `GetModuleName` | editor only |
| `UnLuaNova.API.CallUEFast` | the fast static-binding call path and its parameter cache | |
| `UnLuaNova.API.DataTable` | `GetTableDataRowFromName` | |
| `UnLuaNova.API.DefaultParamCollector` | default-parameter collection and fill-in at call time | |
| `UnLuaNova.API.EKeys` | reaching `EKeys`, comparing `FKey` | |
| `UnLuaNova.API.Enum` | `GetNameStringByValue` / `GetDisplayNameTextByValue` / `GetMaxValue` | |
| `UnLuaNova.API.ErrorHandling` | compile errors, runtime errors, engine-side errors, messages free of machine paths | |
| `UnLuaNova.API.FColor` | construct / `Set` / `Add` / `tostring()` | |
| `UnLuaNova.API.FIntPoint` | construct / `Set` / `Add` / `Sub` / `Mul` / `Div` / `unm` / `tostring()` | |
| `UnLuaNova.API.FIntVector` | same as above, plus `SizeSquared` | |
| `UnLuaNova.API.FLinearColor` | construct / `Set` / `Add` / `Sub` / `Mul` / `Div` / `tostring()` | |
| `UnLuaNova.API.FLuaEnv` | creating an env, `DoString`, `GC` and hot reload, module loading, env lifetime | |
| `UnLuaNova.API.FMulticastScriptDelegate` | `Add` / `Remove` / `Clear` / `Broadcast` | |
| `UnLuaNova.API.FQuat` | construct / `Set` / `Normalize` / `Mul` / `tostring()` | |
| `UnLuaNova.API.FRotator` | construct / `Set` / `tostring()` | |
| `UnLuaNova.API.FScriptDelegate` | `Bind` / `Unbind` / `Execute` | |
| `UnLuaNova.API.FTransform` | construct / `BlendWith` / `tostring()` | single-argument constructors not supported yet |
| `UnLuaNova.API.FVector` | construct / `Set` / `Normalize` / arithmetic / `unm` / `tostring()` | |
| `UnLuaNova.API.FVector2D` | same as above | |
| `UnLuaNova.API.FVector4` | same as above | |
| `UnLuaNova.API.Lifetime` | release tag, collection of script-held objects, manual references, env teardown | |
| `UnLuaNova.API.LuaBackend` | the backend shim, capability parity, Luau-only features, integer types, coroutines, profiler and debug | backend specific |
| `UnLuaNova.API.LuaScriptCompat` | `collectgarbage` / `load` and other script-level compatibility | backend specific |
| `UnLuaNova.API.Observability` | CPU profiling, memory stats, debug service | |
| `UnLuaNova.API.OutParam.Full` | returning every out parameter | editor only |
| `UnLuaNova.API.OutParam.Partial` | returning some out parameters, rest filled from the inputs | editor only |
| `UnLuaNova.API.RuntimeModules` | Lua modules shipped with the plugin: `Vector` / `Buffer` | backend specific |
| `UnLuaNova.API.StaticBinding` | the static-binding registry, resolution, property type classification | |
| `UnLuaNova.API.StringEx` | `string` extensions: split / strip / startswith·endswith / count / join | |
| `UnLuaNova.API.TArray` | full container surface: `Add` / `Insert` / `Remove` / `Swap` / `Shuffle` / `ToTable` / `pairs` etc. | |
| `UnLuaNova.API.TMap` | full container surface: `Add` / `Remove` / `Find` / `FindRef` / `Keys` / `Values` / `pairs` etc. | |
| `UnLuaNova.API.TSet` | full container surface: `Add` / `Remove` / `Contains` / `ToArray` / `pairs` etc. | |
| `UnLuaNova.API.UClass` | `Load` / `IsChildOf` / `GetDefaultObject` | |
| `UnLuaNova.API.UE` | the global `UE` namespace | |
| `UnLuaNova.API.UObject` | `Load` / `IsValid` / `GetName` / `GetOuter` / `GetClass` / `GetWorld` / `IsA` / `Release` | |
| `UnLuaNova.API.World` | `SpawnActor` | |
| `UnLuaNova.Binding` | scoped enums and enum classes | |
| `UnLuaNova.Editor.Commands` | console commands, Lua templates, editor commands | editor only |
| `UnLuaNova.Editor.StaticBindingGenerator` | the static binding generator | editor only |
| `UnLuaNova.Extensions` | LuaRapidjson / LuaProtobuf / LuaSocket | needs the optional extension plugins |
| `UnLuaNova.Regression.Issue276` | `coroutine delay` stops working once the game world is paused | |
| `UnLuaNova.Regression.Issue286` | wrong Blueprint `TMap FindRef` | |
| `UnLuaNova.Regression.Issue295` | dynamically bound UMG object crashes in Destruct on map change | |
| `UnLuaNova.Regression.Issue300` | wrong value returned to Blueprint from an overridden function | |
| `UnLuaNova.Regression.Issue372` | Lua registry leftovers after `K2_DestroyActor` | |
| `UnLuaNova.Regression.Issue401` | array arguments corrupted by Lua overriding | |
| `UnLuaNova.Regression.Issue430` | crash calling an overridden function from an anim notify | |
| `UnLuaNova.Regression.Issue434` | multicast delegate handler bound to `self` | **needs a real viewport** |
| `UnLuaNova.Regression.Issue443` | Blueprint functions unreachable from some code blocks | |
| `UnLuaNova.Regression.Issue446` | crash calling an overridden method from `FTickableGameObject::Tick` | |
| `UnLuaNova.Regression.Issue453` | by-ref `TArray` arrives in Blueprint as an empty array | |
| `UnLuaNova.Regression.Issue455` | crash caused by `CacheScriptContainer` | |
| `UnLuaNova.Regression.Issue471` | `FScriptDelegate` unbinding does nothing | |
| `UnLuaNova.Regression.Issue603` | crash re-firing an event from inside its own Lua listener | |
| `UnLuaNova.Regression.Issue634` | crash overriding a C++ struct return that is not the ReturnValue | |
| `UnLuaNova.Regression.Issue660` | crash binding one UnLua function to several delegates | |
| `UnLuaNova.Regression.IssueOverrides` | problems caused by overriding | |
| `UnLuaNova.Settings` | actual behaviour of settings such as the dead-loop watchdog | mutates global settings, restored in `AfterEach` |
| `UnLuaNova.Showcase.Binding` | static binding sample screenshot | **needs a real viewport** |
| `UnLuaNova.Showcase.Container` | container access sample screenshot | **needs a real viewport** |
| `UnLuaNova.Showcase.Override` | Blueprint event override sample screenshot | **needs a real viewport** |
| `UnLuaNova.Showcase.UMG` | UMG and Lua lifetime sample screenshot | **needs a real viewport** |

## One report per backend

The Lua backend is a **build-time choice** (see "Lua backend" in [Settings](Settings.md)): both
backends export the same `lua_*` symbols and cannot coexist in one build. So "test both backends"
is not a flag, it is:

```powershell
.\Tools\RunTests.ps1 -Backend Both -Publish
```

and for each backend, in order:

1. write `LuaBackend=<backend>` into `Config/DefaultUnLuaNovaEditor.ini`;
2. clear `Plugins/UnLuaNova/Intermediate/` (UBT does not watch that ini, so without this it may
   treat the previous backend's object files as up to date);
3. `Engine/Build/BatchFiles/Build.bat <Project>Editor <platform> <config> -Project=<uproject>`;
4. run the tests, results into `Saved/TestReports/<timestamp>/lua` and `.../luau`;
5. generate the report; restore the ini afterwards (`try/finally`, so an interrupt restores it
   too).

### Switching backends is global state, so the whole sequence is atomic

The ini is repo-level mutable state and the binaries are build-time output; once the two diverge,
results get filed under the wrong backend (this has happened: a directory called `lua` holding a
Luau build). Four guards prevent it:

- **Only one `RunTests.ps1` at a time**: `Saved/RunTests.lock` records its holder's PID and a
  second process exits with an error; a leftover lock whose PID no longer exists is cleared
  automatically. Without this, the second run's `Build.bat` just waits on UBT's mutex and then
  runs whatever the first build produced.
- **A failed build restores the ini immediately**, not at the end of the script: otherwise the
  ini points at a backend that never built successfully, and the next `-SkipBuild` would test old
  binaries under that name.
- **The artefacts are verified before the editor starts**: the script reads
  `Definitions.UnLuaNova.h` (UBT generates it for the plugin module) and checks
  `UNLUANOVA_LUA_BACKEND` - the binary on disk, not the directory name or the ini.
- **`-SkipBuild` may not switch backends**: it would relabel existing binaries without rebuilding,
  so it is refused outright.

`-SkipBuild` is therefore only useful for rerunning the backend you just built - for example when
report generation failed and you want to try again.

### A crash still produces a report

When the editor crashes, the automation framework exports no JSON, and the report used to say
nothing but "0 tests" with no clue where it died. Now: `RunTests.ps1` prints the crash stack to
the console, `GenerateTestReport.py` digs the **test that was running** and the call stack out of
`Editor.log` and writes them into a "crash" section, and `summary.json` carries `crashed` /
`crash_test` (the script exits 1 on it, so CI cannot mistake it for a pass).

Artefacts:

| Backend | Local report | Published to the docs site |
|---|---|---|
| `Lua` | `Saved/TestReports/<timestamp>/lua/report.md` | `Docs/test-lua-report.md` |
| `Luau` | `Saved/TestReports/<timestamp>/luau/report.md` | `Docs/test-luau-report.md` |

Both reports are tagged with their backend (title, and a "Lua backend" row in the verdict table),
so you cannot lose track of which one you are reading. Use `-Backend Lua` or `-Backend Luau` to
run just one.

## Test assets

The rename (UnLua -> UnLuaNova) could only rewrite text, not `.uasset`, and it left a few kinds
of damage behind: Blueprints saved without being compiled (no `SKEL_*` / `Default__*` /
`BlueprintGeneratedClass` in the package, so `Class ..._C` resolved to nothing), DataTables that
had lost their RowStruct (the engine logs `Missing RowStruct` at **Error** level), and actors
that disappeared from levels along with their class.

All of that is **fixed and committed** - no repair step is needed any more, and the one-off
`UnLuaNova.Editor.RepairTestAssets` test is gone: the test was one-off, the assets are not.

If the same damage ever reappears (say someone edits test assets by hand), the manual procedure
is still the obvious one: right-click `/UnLuaNovaTestSuite/` in the Content Browser -> Resave All,
or open each Blueprint and Compile. The command-line equivalent is
`UnrealEditor-Cmd.exe <uproject> -run=ResavePackages -packagefolder=/UnLuaNovaTestSuite/`
(not guaranteed to trigger a Blueprint recompile).

## How paths are resolved

There is **no machine specific path** in the script; everything is derived, so a fresh clone on
another machine just runs:

1. repo root = the parent of the directory holding the script (the parent of `Tools/`);
2. project file = the first `*.uproject` under the repo root;
3. UE root, tried in this order:
   - `-EnginePath` passed explicitly;
   - the `UNREAL_ENGINE_PATH` environment variable;
   - **the `EngineAssociation` GUID from the `.uproject` -> the registry key
     `HKCU:\Software\Epic Games\Unreal Engine\Builds`** (the mapping UnrealVersionSelector
     maintains, the most authoritative source);
   - any directory at the repo root or up to 5 levels above that contains
     `Engine/Binaries/Win64/UnrealEditor-Cmd.exe` (source builds often sit next to the project);
   - any engine in the registry that has that exe.

If none of that works it fails with the three ways to fix it. Add `-DryRun` to check the
resolution first.

## Reports and verdicts

After a run, each backend's directory `Saved/TestReports/<timestamp>/<backend>/` holds:

| File | Contents |
|---|---|
| `report.md` | the verdict: pass rate, total/passed/failed/skipped, the **passing list**, failure details (message and source location), slowest tests, screenshot gallery |
| `summary.json` | machine readable totals (including `backend`); the script decides its exit code from it |
| `luau-analyze.json` | static type check result, rendered into the report as its own section |
| `Editor.log` | the full log of this run |
| `screenshots/` | screenshots taken during the run (including the recorded frame sequences `name_0001.png` ...) |
| `videos/` | `.mp4` encoded from the frames (a failed encode goes into the report and exits non-zero) |
| `*.json` | the raw results the engine exported |

The two copies `-Publish` writes to `Docs/test-lua-report.md` / `Docs/test-luau-report.md` are
**committed to git**, so they carry no machine specific path and no per-run directory name (both
change on every run and would produce meaningless diffs for identical results). The local
`report.md` still points at the raw JSON directory, for tracing back.

Failure details list the message and `file:line` directly, so you can go straight to it.
A test marked success but carrying an error record is treated as a failure - that is a failure
pretending to be a pass.

**Passing tests are listed too**, not just failures: after a change to shared code, "what still
works" matters as much as "what broke", and a percentage cannot answer that. The list is sorted by
test name, so an unchanged result produces a clean git diff.

## Screenshots and recordings

The sample images on the docs site (`Docs/CN/Showcase.md`) and the gallery at the bottom of every
report are **produced by the tests**, not taken by hand. Hand-taken pictures go stale: the code
moves on and the picture still claims to be current.

### Where the screenshots come from

`UnLuaNova.Showcase.*` (`Private/Tests/ShowcaseTest.cpp`) - four specs, each opening one tutorial
level (`Content/Tutorials/`), waiting for PIE and taking one shot:

| Spec | Level | Produces |
|---|---|---|
| `UnLuaNova.Showcase.Binding` | `01_HelloWorld/HelloWorld` | `Showcase_Binding.png` |
| `UnLuaNova.Showcase.Override` | `02_OverrideBlueprintEvents/...` | `Showcase_Override.png` |
| `UnLuaNova.Showcase.Container` | `06_NativeContainers/...` | `Showcase_Container.png` |
| `UnLuaNova.Showcase.UMG` | `11_ReleaseUMG/ReleaseUMG` | `Showcase_UMG.png` |

They register through `BEGIN_VIEWPORT_TESTSUITE`: with no viewport there is no frame to capture,
so a `-NullRHI` round drops them entirely rather than recording "screenshot failed". **Do not pass
`-NullRHI` when you want images.**

Screenshots are collected into the report directory by default (`-NoScreenshots` opts out), and
the script empties `Saved/Screenshots/WindowsEditor/` first - the editor does not, and stale PNGs
would be published as this run's images. That is worse than having no image, because nobody can
tell.

### What the shot captures: what the player sees

`FScreenshotLatentCommand` captures the **game viewport with UI**
(`RequestScreenshot(Name, true, ..., bRestrictToGameViewport=true)`). The tutorial levels' visible
content is `Screen.Print` (PrintString on-screen text) and UMG widgets - all in the UI layer. The
old `bShowUI=false` request captured the bare scene render target - and the engine also disables
on-screen debug messages for that path - while the levels themselves have no lights or meshes, so
all four showcase images came out pure black. After the fix, the shot is exactly what pressing
Play shows.

Each shot is followed by `FVerifyScreenshotNotBlankLatentCommand`: it decodes the PNG and fails
the test when the whole image is a single color. A showcase image that shows nothing is worse
than a failure - a failure gets looked at, a black image quietly ships to the docs site.

### When the level itself is empty: the dressing commandlet

Fixing capture flags only gets UI-layer content into the picture; it cannot change the level.
The four tutorial levels **are empty worlds** - no light, no geometry, no PlayerStart - and their
demos print to the screen and the log, so the scene itself has nothing to photograph. Dressing
them is all this commandlet does:

```powershell
UnrealEditor-Cmd.exe "D:\learning\UnLua\UnLuaNovaDemo.uproject" -run=UnLuaNovaShowcaseDressing -unattended -NoSplash
# one level only: ... -run=UnLuaNovaShowcaseDressing -Map=/Game/Tutorials/01_HelloWorld/HelloWorld
# undo it:        ... -run=UnLuaNovaShowcaseDressing -Undo
```

Per level it adds: a movable sun, a movable sky light, a floor to catch the light, three cubes so
the shot reads as a 3D space, and a PlayerStart so PIE has a defined camera. Everything is
Movable on purpose - static lights need a lighting build, a commandlet cannot run one, and static
lights would still come out black.

Idempotent: an already dressed level is skipped. `-Undo` removes actors tagged
`UnLuaNovaShowcaseDressing` and nothing else, so anything placed by hand is untouched. Commit the
resulting `.umap` changes afterwards.

### Adding a shot to a test

```cpp
ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FScreenshotLatentCommand(TEXT("Showcase_Binding")));
```

The latent command **waits for the PNG to actually land** (screenshots are written
asynchronously) and has an upper bound: under `-NullRHI` there will never be a PNG, and hanging
the whole suite is far worse than a missing picture.

### A screenshot without a test

```powershell
.\Tools\RunTests.ps1 -Map /Game/Tutorials/01_HelloWorld/HelloWorld `
    -ExtraCommands "UnLuaNovaTest.Screenshot Showcase_Binding"
```

### Recording video

Besides the still, the Showcase specs record a **frame sequence**
(`Showcase_Binding_Clip_0001.png` ...), which the report generator encodes into
`videos/<name>.mp4` and embeds as a player. **Recording is on by default, not an option** -
"no video in the report" and "one still image in the report" look similar, but the first one
means this round did not record.

```powershell
# refresh just the sample images and clips (fast) and publish them
.\Tools\RunTests.ps1 -Filter UnLuaNova.Showcase -Backend Luau -Publish

# no recording
.\Tools\RunTests.ps1 -NoVideo
```

Change the length with `UNLUANOVA_RECORD_FRAMES` (default 60 frames).

### Where the encoder comes from

The engine has no video encoder (see below), so encoding lives in the report generator, which
looks in this order:

1. the `--ffmpeg` argument;
2. the `FFMPEG` environment variable;
3. `ffmpeg` on PATH;
4. **the static ffmpeg `imageio-ffmpeg` ships** - an ordinary pip package, so there is nothing
   to install system-wide and nothing to put on PATH:
   `python -m pip install -r Tools/requirements.txt`.

If none of the four exists, the failure is written into the report's video section (with the
reason) and the script exits non-zero - **not** "keep the frames and make do". Making do looks
exactly like "this round recorded nothing".

!!! note "Why not the engine's own recording"
    What UE produces natively is an **image sequence** (HighResShot, Movie Render's PNG/EXR
    output), not a video file; Epic's own Movie Render Pipeline has a "Command Line Encoder"
    setting that asks *you* for an **external ffmpeg path** - which is Epic saying there is no
    encoder in the engine. This engine build ships no ffmpeg executable either (only AMD AMF
    headers). And what the tests need is "the few seconds this spec ran", not an offline
    sequence render, so capturing one frame per rendered frame from the test is both simpler
    and more controllable.

## Test list baseline

"All green" hides the worst kind of failure: a test that was renamed, filtered out or deleted
by accident leaves a report that is **still all green** - half the suite missing looks exactly
like everything passing. So every run carries the **full list of test names** (`names` in
`summary.json`) and diffs it against the previous baseline:

```powershell
# A normal run already compares - RunTests.ps1 passes --baseline itself
.\Tools\RunTests.ps1 -Backend Luau

# Make "the set of tests changed" turn CI red
.\Tools\RunTests.ps1 -Backend Luau -FailOnDrift

# Once the change is confirmed intentional, refresh the baseline and commit it
.\Tools\RunTests.ps1 -Backend Both -UpdateBaseline
git add Tools/baseline
```

Three things are compared: the total, the names that disappeared or appeared, and the total
runtime (only flagged past ±50% - noise from a loaded machine is not a regression). The
baselines are `Tools/baseline/lua.json` and `Tools/baseline/luau.json`, and they are
**committed**.

It **warns but does not fail** by default: renaming a test is legitimate, and a CI that goes
red every time someone does is a CI people turn off.

## Why not on GitHub Actions

UnLuaNova depends on a **local UE source build** (UE 5.8.3). GitHub-hosted runners have no such
engine and cannot build one. So:

- tests run locally;
- add `-Publish` when you want them on the site, then commit the generated
  `Docs/test-lua-report.md`, `Docs/test-luau-report.md` and the screenshots;
- Actions only renders and publishes `Docs/` to GitHub Pages.

If you have a self-hosted Windows runner, wire `Tools/RunTests.ps1` into it - its exit code is
already what a CI wants.

### Compile at least once a day

The test code is compiled code too: rename an engine field, move a helper, and the whole spec
directory stops compiling. A full build takes 20-odd minutes, so nobody runs one by hand every
day. Make the compile happen daily instead:

```powershell
# Register (or update) a task that runs nightly at 02:00: build only, no editor, no tests,
# no report
.\Tools\Install-NightlyTask.ps1

# The same thing by hand; it exits non-zero on failure and logs to Saved/nightly-build.log
pwsh -File D:\learning\UnLua\Tools\RunTests.ps1 -Backend Luau -BuildOnly -NoReport
```

`-BuildOnly` compiles and exits, and RunTests.ps1 exits non-zero when the build fails - which
is the whole feedback loop: "the test code does not compile" is caught **within a day** rather
than at the next manual full build. `-Backend Both` compiles both backends, at twice the cost.

!!! note "You get this loop without a runner too"
    The scheduled task above is the degraded form of it: compile only, no test run.
    With a self-hosted Windows runner, swap it for a GitHub Actions `schedule` running the
    same command.

## Conventions for new tests

- **Naming**: `UnLuaNova.<area>.<topic>`, for example `UnLuaNova.API.FLuaEnv`. Issue regressions
  keep their `Issue###` number. Use exactly this casing — **no all-caps** (`UNLUANOVA.API.Base`
  can slip past `-Filter` and will not line up with `Automation List` output).
- **Unit tests** go in `Private/Specs/` using `BEGIN_DEFINE_SPEC`; **scenario / instance tests** go
  in `Private/Tests/`.
- Everything is wrapped in `#if WITH_DEV_AUTOMATION_TESTS`, so packaged builds contain no test
  code.
- Tests that need a World use `FOpenMapLatentCommand` rather than calling `LoadMap` themselves.

---

## Further reading

- [Test report (Lua backend)](../test-lua-report.md) - the latest Lua backend run
- [Test report (Luau backend)](../test-luau-report.md) - the latest Luau backend run
- [Showcase samples](Showcase.md) - the sample screenshots the tests produce
- [Debugging](Debugging.md) - debugging Lua scripts

---
title: "Testing and Automation"
description: "One release command with mandatory evidence and failure reporting"
---

# Testing and Automation

<!-- release-status: passed -->
<!-- release-provenance: 347bb8c5 -->

Run from the repository root:

```powershell
.\Tools\RunTests.ps1
```

The command takes **no parameters** and orchestrates the current **Win64** Release matrix. Operators do not need separate backend builds, analyzer builds, declaration exports, network peers or test filters. It uses the project's associated UE installation, installed Visual Studio C++ tools and Python. On first use it initializes pinned submodules and installs isolated Python tools under `Saved/ReleaseTools`; it does not install UE or Visual Studio.

> **Python 3.10 or newer is required.** On 3.7–3.9, `python -m venv` shells out to
> `ensurepip --default-pip`, which did not exist yet, so the stage dies with an opaque
> subprocess error and every later stage is recorded as `blocked`. The pipeline now checks
> the version first and says so. If the default `python` on PATH is too old, name one
> instead of reordering PATH:

```powershell
$env:UNLUANOVA_PYTHON = 'C:\Users\<you>\scoop\apps\python311\current\python.exe'
.\Tools\RunTests.ps1
```

> Run it with **PowerShell 7 (`pwsh`)**, not Windows PowerShell 5.1 (`powershell.exe`).

## Backend-isolated builds

Backend isolation is the **default and only** build path, not an option: `RunTests.ps1`,
`ReleasePipeline.ps1`, `RunDevelopmentTests.ps1` and `RunNetMatrix.ps1` always use the six
`UnLuaNovaDemo<backend>{Editor,Server,''}` targets. **Each backend's Editor / Game /
Server targets are built once into separate `Backends/<backend>/` trees, and switching
afterwards only changes module manifests - no compile, no overwrite**. This applies to
Development, Release, `-Carrier Game` and `-NetMatrix`. A normal project can still pick
the backend at compile time in `Config/DefaultUnLuaNova.ini` and build the ordinary
targets by hand; the pipeline no longer offers a single-slot path.

### One-time build

```powershell
# Editor
Build.bat UnLuaNovaDemoLuaEditor  Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
Build.bat UnLuaNovaDemoLuauEditor Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
# Game
Build.bat UnLuaNovaDemoLua        Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
Build.bat UnLuaNovaDemoLuau       Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
# Dedicated Server
Build.bat UnLuaNovaDemoLuaServer  Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
Build.bat UnLuaNovaDemoLuauServer Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
```

Editor-only automation needs just the first two; `-Carrier Game`, NetMatrix and Release also
need the Game/Server pairs.

```powershell
# From then on (manifest rename + ini label only, no compile, no overwrite)
.\Tools\SwitchBackend.ps1 -Backend Lua
.\Tools\SwitchBackend.ps1 -Backend Luau
.\Tools\SwitchBackend.ps1 -Status                 # which one is live?
.\Tools\SwitchBackend.ps1 -Backend Default       # back to the ordinary single target

# Partial/debug runs: Editor or Game carrier (not a release verdict)
.\Tools\RunDevelopmentTests.ps1 -Backend Both
.\Tools\RunDevelopmentTests.ps1 -Backend Both -Carrier Game

# Connected network matrix (each cell launches real server/client processes)
.\Tools\RunDevelopmentTests.ps1 -NetMatrix -Backend Both -SkipBuild

# Full Win64 Release matrix - no parameters, everything is a default
.\Tools\RunTests.ps1
```

### Why six targets

UBT gives every Editor target the same `AppName` (`UnrealEditor`), so a second `Target.cs`
alone would emit the same `UnrealEditor-UnLuaNova.dll` and the same object files. A second
`.uproject` alone does not help either: plugin build products live in the plugin's own
`Binaries/` and `Intermediate/`. The byte-identical `[UAK BACKEND LAYOUT]` block in every
`*.Build.cs` sets `ModuleRules.BinariesSubFolder`, which redirects **both** the DLL and the
intermediate directory into `Binaries/Win64/Backends/<backend>/` and
`Intermediate/.../Backends/<backend>/`.

The naming contract is `UnLuaNovaDemo<Backend>{Editor,Server,''}`. Editor targets use a
shared build environment and always keep `AppName = UnrealEditor`; Game/Server targets do
not, so their `AppName` is the target name and their exe, module DLL prefix, intermediate
folder and `*.modules` manifest name all change with it (for example
`UnLuaNovaDemoLua.exe`, `UnLuaNovaDemoLuaServer.modules`). `RunTests.ps1` and
`ReleasePipeline.ps1` use the matching name for the current carrier so they never pick up
the other backend's binary.

The two extra isolated Editor targets leave the project with three `*Editor.Target.cs` files.
UAT's `BuildCookRun` refuses to run when it cannot resolve a default editor target, even
with `-skipbuildeditor`, so `Config/DefaultEngine.ini` must declare
`[/Script/BuildSettings.BuildSettings] DefaultEditorTarget=UnLuaNovaDemoEditor`.
`Tools/tests/test_backend_isolation.py` guards this as a static regression. It does not
change the default isolated build, where the Editor target is selected per backend by
`RunTests.ps1` / `ReleasePipeline.ps1`.

### Why SwitchBackend is needed

The engine scans `Binaries/**` recursively for `*.modules`
(`FModuleManager::FindModulePathsInDirectory`). With two build trees both manifests are
found, module names collide and which backend loads is undefined. The script **discovers
all `*.modules` dynamically** (no more hardcoded `UnrealEditor.modules`), renames the
inactive ones to `*.disabled` (a name the engine never reads) and pins `LuaBackend` in
`Config/DefaultUnLuaNova.ini`. An isolated target takes its backend from the target name,
not the ini, so the Lua target can be built while Luau is active. A Binaries root that has
no isolated products for that backend keeps its root manifest live, so its modules stay
discoverable.

| Constraint | Detail |
|---|---|
| First-run cost | Six full builds and roughly twice the disk space; switching is free afterwards |
| Carrier | Editor / Game / Server are all isolated, including `-Carrier Game`, `-NetMatrix` and the parameterless full Release |
| Release | Each backend uses isolated Editor/Game/Server targets, UAT `-stagingdirectory` under `Saved/StagedBuilds/{Windows,WindowsServer}/Backends/<backend>`, `-CookOutputDir` under `Saved/Cooked/Backends/<backend>/{Development,Shipping}/{Windows,WindowsServer}`, and static binding sources under `Generated/Backends/<backend>/`; cache fingerprints include `isolated`/`targets`, and the run restores the Default manifest layout and the original ini at the end |
| Manifests | Building an isolated target rewrites that backend's manifests. Building an ordinary target afterwards recreates root manifests - run `SwitchBackend.ps1` again |
| Concurrency | Switching is still global state; `Saved/RunTests.lock` remains in force. Do not build or open the editor during a switch |
| Host projects | `Tools/unluanova.ps1 doctor` now understands `Backends/<backend>/`, live/disabled manifests and the demo project's ini label: two DLL sets coexisting is no longer an error; two backends' manifests live at once is |

`Tools/tests/test_backend_isolation.py` guards the source contract: byte-identical layout
blocks, all six targets, the end-to-end call chain and the parameterless entry point,
plus functional tests of the outer doctor against synthetic Binaries trees.

## Automatic Stages

| Stage | Work |
|---|---|
| Repository/tools | Capability and link audit; Python regressions; PowerShell syntax, argument, process and timeout tests |
| Analyzers | Initialize the MSVC environment and build pinned LuaLS and luau-lsp |
| Backends | Run both Lua and Luau with Timer/Event/OOP and static binding enabled |
| Editor | Build Editor/generator, generate static binding source into this backend's `Generated/Backends/<backend>/` and rebuild. No shared-source backup/substitution/restore |
| Development | Build/cook/package Game and Dedicated Server targets into per-backend `Backends/<backend>` staging directories |
| Editor automation | All registered UnLuaNova tests, including Slow, lifetime, extensions, showcase media and benchmarks |
| Connected peers | Use this backend's current staged Server/Game for one DS and two clients; verify two server-spawned actors, two rounds of Lua field replication, bidirectional RPCs, owner-targeted delivery and multicast with raw per-peer observations |
| Types | Back up old generated declarations, export fresh declarations, require successful backend type checks |
| Cooked automation | Run all tests registered for the Development game's actual UE context |
| Memory | Require `UnLuaNova.Memory` evidence in both backend/carrier combinations: VM allocation balance, resource release and warmed heap growth |
| Shipping | Build/cook/package into `WindowsShipping/Backends/<backend>` and verify the executable; this is not Shipping runtime certification |
| Cleanup | Restore original config bytes and changed environment variables, clean up owned processes, write an aggregate verdict |

`Saved/RunTests.lock` prevents concurrent runner backend changes. Avoid editing/building the same project in another process during validation. Configuration is restored; both backends' isolated binaries stay on disk, Release resets the manifest layout to Default and Development leaves the last activated backend live.

Release sets `UNLUANOVA_TEST_PEER_EXE` and `UNLUANOVA_TEST_SERVER_EXE` to the Game and Server executables in that backend's `Backends/<backend>` staging paths. Compilation reuses each backend's `Binaries/Win64/Backends/<backend>`; the Development package uses `Saved/StagedBuilds/{Windows,WindowsServer}/Backends/<backend>`, while **Shipping stages separately into `Saved/StagedBuilds/WindowsShipping/Backends/<backend>`**. UAT calls `CleanStagingDirectory` on every `-stage`, so sharing one root let the final Shipping stage delete the Development client exe that `metadata/binary-Development.json` and the connected matrix's Cooked cell still need. With separate roots a completed Release leaves a usable Development staged peer. The two backends never overwrite each other's outputs, and no per-run executables are copied. Each backend's `metadata/binary-*.json` retains the path, size and SHA-256 at the time of use. A UE 5.8 Game binary cannot become a dedicated server through `-server`. Topology records each peer's executable; cleanup checks its PID, path and start time.

Release Editor, Game and non-headless peers use `-RenderOffScreen`, retaining real RHI, viewport tests, single game-viewport screenshots independently of desktop window visibility. Desktop recording, frame sequences and video encoding have been removed; DS still uses `-NullRHI`. Development can explicitly select `-RenderOffScreen`.

The Release test Editor uses `AboveNormal` process priority, recorded with its PID in `metadata/process.json`, to reduce scheduling interference from normal-priority background tasks. Only the owned test process changes. Original references used Normal priority; this environmental difference cannot establish a runtime optimization. Workload, sampling, references and limits remain unchanged, and frequency, memory and other workloads can still affect results.

LuaLS builds use a unique temporary directory under `Saved/ReleaseTools/luals-temp/` and restore TMP/TEMP on exit. Bootstrap and main analyzer builds/tests have separate exit-code checks; either failure blocks the analyzer stage.

Cook outputs are isolated on backend *and* configuration: all three `BuildCookRun`
invocations pass `-CookOutputDir` explicitly, to
`Saved/Cooked/Backends/<backend>/Development/Windows`, `.../Shipping/Windows` and
`.../Development/WindowsServer`. The last segment must be UAT's `SC.CookPlatform`: the
CookCommandlet uses `-outputdir` verbatim as its sandbox root (single-platform cooks do not
append a platform directory when the path has no `[Platform]` token), while
`CopyBuildToStagingDirectory` appends `CookPlatform` unless the final segment already equals it.
Separating configurations also stops Development and Shipping from overwriting each other's
cooked assets.

Static binding sources are isolated per backend. The generator reads the compile-time
`UNLUANOVA_LUA_BACKEND` of the Editor binary hosting it and writes its `.cpp` into
`Generated/Backends/<backend>/`, so the two backends never overwrite each other. UBT compiles
every `.cpp` under the module directory, so the whole body of each generated file is guarded by
that same macro: only the file matching the current target registers tables, the other one is an
empty translation unit. The Release therefore needs no backup / stub / restore choreography and
there is no shared file to corrupt. Besides the generator's ownership marker, the gate also
checks the `// backend:` label and the macro guard, so a file from the wrong backend fails
closed. The first generation removes the legacy global single file (only if it carries the
generator's marker). Fingerprints include a generator format version to invalidate old output.

DAP session tests use OS-assigned loopback ports and nonblocking game-thread polling with persistent TCP and UTF-8 byte buffers. Requests time out after 5 seconds and sessions after 20 seconds; cleanup removes polling before closing sockets. Full Editor runs for both backends include session, unknown-command, fragmented-request, frame-decoding, concurrent-stop and restart regressions.

## Verdict

Read `Saved/ReleaseTests/test/release-summary.md` or `release-summary.json` (`Saved/ReleaseTests/baseline/` with `-Baseline`). Those two directories are the entire history: the run being judged, and the run it is judged against, each overwritten by its own next run. A `test` run adds a "Compared With Baseline" section listing stages that passed in the baseline but not now, and carriers that lost tests or gained failures; the comparison is reported, not enforced.

Stages report `passed`, `cached`, `failed` or `blocked`. `cached` means the stage's inputs did not change and the previous run's artifacts were reused (see below). Any failure or blocked dependency returns nonzero. An in-progress summary has `complete=false`. Each external stage has a default 60-minute timeout, adjustable with `-TimeoutMinutes`.

## Build artifact reuse

Compiling, cooking, packaging, exporting declarations and type checking only need to happen when their inputs change, so `Tools/ReleaseCache.ps1` fingerprints them: each stage hashes the inputs that decide its output (source, assets, project config, engine version, backend, analyzer) and records them in `Saved/ReleaseCache/index.json` together with its artifacts' path, size, modification time and SHA-256. A stage is recorded as `cached` and skipped when the fingerprint matches and the artifacts still match the record; a deleted artifact, one replaced by another build, or any changed input is a miss and the stage runs.

- Test, evidence and report stages are never cached. They are this run's evidence, not an input.
- Each backend has its own build tree: isolated targets write DLLs, intermediates, staged packages, cooked assets and static binding sources into separate `Backends/<backend>/` directories, so neither build overwrites the other. Cache fingerprints always include `isolated=True`, the per-backend target names and `cook=isolated` / `binding=isolated`; the backend order is fixed at Lua then Luau.
- Declarations are exported once for both backends. Type check summaries (`*-analyze.json`) are copied back from the cache into the run directory on a hit, because the evidence gate reads them and "no file" is a failed gate rather than a skip.
- `-NoCache` forces everything to run again; deleting `Saved/ReleaseCache` does the same. `-PruneLegacy` deletes the old per-timestamp result directories (`Saved/ReleaseTests/<yyyyMMdd-HHmmss-fff>`).

Backend outputs include `editor/<backend>/`, `game/`, binary metadata and logs. The actual process exports `metadata/manifest.json` with backend, feature flags, Editor/cooked state and full leaf test paths. `ValidateRelease.py` compares this against UE's `index.json`. Empty, missing, duplicate, skipped, unknown, error-bearing and wrong-backend results fail. Editor source suites must also be registered. Historical display-name/count baselines no longer decide release completeness.

The Editor evidence gate requires all four Showcase PNGs. Cooked reports also collect newly generated single screenshots. Network evidence requires Server/Client1/Client2 logs, backend/role identity, matching RPCIds, distinct PIDs, open connections and matching server endpoints. DedicatedServer schema 2 additionally requires a shared server-generated SessionId, two distinct owners, two rounds of Lua HP observations and exact per-peer RPC logs. Standalone or declaration-only historical reports cannot pass. A peer-report helper's no-op does not prove transmission.

Editor diagnostics use the current Editor executable for all peers by default, avoiding stale Game binaries. Release explicitly selects its current staged Server/Game through environment variables. Clients never spawn the test actors or assign ownership locally.

Performance requires `Tools/baseline/perf-lua.json` and `perf-luau.json`, matching backend, engine, machine, case inventory and iteration counts, with a 25% regression limit. Missing comparable references fail the gate; the runner never promotes its own measurements automatically.

Sampling warms each case for at least 250 ms, then records five samples of at least 100 ms each and reports the median of the five per-operation averages. The gate rejects missing, short or inconsistent samples.

## Memory Leak Gate

No extra command or manual `lua.memstats` toggle is required. Nine reports under `metadata/memory/` cover environment cycles, warmed heap stability, actual module hot reload, a deliberately retained graph, native smart-pointer payloads, delegates, UMG image/texture ownership, and both World/environment teardown orders. Reports must match the manifest's run ID, backend and carrier. Missing reports, unregistered cases, insufficient work or failed assertions fail release validation.

- Audited environments install a pointer ledger before VM creation and retain it through `lua_close`. Every closed VM must have zero live bytes, live blocks and accounting errors. Ordinary environments do not maintain this map.
- Lifecycle cases run 24 cycles; each World teardown order runs 12. Weak pointers verify native payload and UObject release; Timer cases check callbacks and captured owners.
- Heap stability and hot reload warm up for 8 rounds, then sample 32 full-GC rounds. Growth is limited to 8 KiB of VM live heap and 16 allocator blocks. Schema 2 records GC-reported `live_bytes` separately from ledger `bytes` and `blocks`: Luau allocates pages, whose normal unused capacity can vary by about 32 KiB. Every allocation must still be released at VM close. Limits are fixed in code and the validator, not provided by reports.
- A control retains a graph across full GC, requires growth above 64 KiB, then releases it and verifies recovery. The validator checks measurements independently of `passed=true`.

This is scenario-based leak regression, not proof of process-wide memory safety. Native ownership checks do not cover every C++ allocation; GPU/RHI resources, Luau CodeGen executable pages, third-party libraries and long production sessions still need dedicated coverage. Process working-set retention alone is not evidence of a leak. `FullFix-Release-22` passed all four backend/carrier runs and evidence gates: 36 memory reports, with zero residue and accounting errors after closure.

## Static release gates (no UE required)

These check files and JSON only - no compiler, no editor - so they can run before the
worktree is frozen:

```powershell
python -B Tools/AuditRepository.py --write-matrix
python -B -m unittest discover -s Tools/tests -p "test_*.py"
python -B Tools/ValidateProvenance.py --run Saved\ReleaseTests\baseline --summary Saved\ReleaseTests\baseline\release-summary.json
python -B Tools/GenerateNotices.py --check Saved\StagedBuilds\Windows\NOTICES.txt Saved\StagedBuilds\WindowsServer\NOTICES.txt
```

These checks validate file structure, local links, Markdown anchors and evidence consistency
without a UE build. Runtime behaviour and platform acceptance are covered by the full Release run.

## Development

Partial runs use the separate `Tools/RunDevelopmentTests.ps1` and are not release evidence.
`Tools/RunTests.ps1` takes no parameters, so a forgotten switch can never turn a release
verdict into a half-covered run:

```powershell
.\Tools\RunDevelopmentTests.ps1 -Backend Luau -Filter UnLuaNova.API.Timer
.\Tools\RunDevelopmentTests.ps1 -Tier Fast
.\Tools\RunDevelopmentTests.ps1 -Filter UnLuaNova.Showcase -Backend Luau -Publish
```

`SkipBuild`, `BuildOnly`, `NullRHI`, `NoReport`, individual backends and custom filters
belong to that debug driver. The nightly build installer uses it too and only verifies
compilation.

### Connected network matrix (separate opt-in)

Eight connected scenarios (late join, process-level reconnect, dormancy, relevancy, join-order swap, packet loss / reordering, `UNetConnection`-level reconnect, cross-peer declaration mismatch) and the Legacy / PushOff / Iris / Cooked variants each launch three real processes, so they are not part of the ordinary test matrix or the standard Release stages:

```powershell
.\Tools\RunDevelopmentTests.ps1 -NetMatrix -Backend Lua -SkipBuild
python Tools\ValidateRelease.py --net-matrix --run Saved\ReleaseTests\net-matrix\Lua --backend Lua --carrier game
```

`Tools/RunNetMatrix.ps1` is the underlying script. `-ExtraArgs` puts `-UnLuaNovaNetScenarios`, `-UnLuaNovaNetPktLoss=25` and the variant's real engine switch (`-UseIrisReplication=1` or a push-model cvar ini override) on the process command line; `FNetSession` forwards the scenario flag, loss percentage, exact reconnect URL and `UNLUANOVA_TEST_NET_VARIANT` to every peer.

**The carrier is per variant.** `AppendWorldFacts` writes compile-time `WITH_EDITOR` into each peer report as `Cooked`, and the gate therefore requires **Editor carriers** (`Cooked=false`) for Legacy / PushOff / Iris and all eight scenarios, with a **Game carrier plus staged packaged peers** (`Cooked=true`) only for Cooked. Driving every cell with `-Carrier Game` made the first three variants report `Cooked=true` too and the gate failed by construction. The Cooked cell needs the **Development** client exe in `Saved/StagedBuilds/Windows` (a Shipping package strips the automation peer test and cannot stand in) and the Development server exe in `WindowsServer`; a completed Release now preserves both.

Each cell runs one filtered connected test and is judged by topology, three peer reports and peer logs - not by the full case inventory - so cells use `-NoReport` and are not compared with `Tools/baseline/*.json`. The UE process exit code, `Copy-Tree`'s topology check and the gate stay fail-closed. `UNLUANOVA_TEST_NET_ROOT` points at a shared directory outside the project so a Development `-game` process cannot remap project-absolute paths into its own cooked root.

`AppendWorldFacts` reads cvar state, `WITH_EDITOR` and `UNetConnection` counters back, while `BuildNetDeclarationReport()` exports FProperty-derived parameter specs and `RepSchemaVersion` / `VariantRepSchemaVersion`, so neither a renamed directory nor a disagreeing declaration can pass. Artifacts land in `net/scenarios/<Name>/` and `net/variants/<Name>/DedicatedServer/`, judged into `metadata/net-matrix-gate.json`. This is **not** UE automation evidence and does not touch `Tools/evidence/index.json`. The gate fails closed on a missing topology, a missing peer log, a PID mismatch, a faked Iris/push-model/Cooked flag, a mismatched or non-discriminating declaration fingerprint, a missing Generic field / UStruct parameter, zero connection byte counters or a scenario that never ran.

These are maintainer operations on `Tools/ReleasePipeline.ps1`. Day-to-day acceptance is
just the parameterless `.\Tools\RunTests.ps1` - none of these switches are needed.

Preview the default plan without writing config, installing tools, compiling or launching UE:

```powershell
.\Tools\ReleasePipeline.ps1 -DryRun
```

Write this run as the baseline later runs are compared against (`Saved/ReleaseTests/baseline`):

```powershell
.\Tools\ReleasePipeline.ps1 -Baseline
```

Rebuild and re-package everything, ignoring the cache:

```powershell
.\Tools\ReleasePipeline.ps1 -NoCache
```

## Scope

Register new `UnLuaNova.*` suites in the test plugin's `Private/Specs` or `Private/Tests`. Source classification tracks suites; the runtime manifest tracks actual context-specific leaves. New registration macros require classifier support, not silent exclusions. Multiple Lua assertions may still aggregate into one UE test.

This entry covers the defined Win64 test matrix. Feature-off combinations, other platforms and
devices, Shipping runtime smoke, Iris / push-model modes and external benchmark adapters require
separate validation.

Type-check summaries retain every warning and the raw `analyzer_exit`. The final `exit` follows error-level diagnostics: warnings alone do not block, while type errors, abnormal analyzer exits and missing results fail.

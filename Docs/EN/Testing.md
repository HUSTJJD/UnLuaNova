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

The default `Release` profile orchestrates the current **Win64** matrix. Operators do not need separate backend builds, analyzer builds, declaration exports, network peers or test filters. It uses the project's associated UE installation, installed Visual Studio C++ tools and Python. On first use it initializes pinned submodules and installs isolated Python tools under `Saved/ReleaseTools`; it does not install UE or Visual Studio.

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

## Backend-isolated builds (-BackendTargets)

Without the switch, both Development and Release behave exactly as before: edit the ini,
rebuild, run, switch backends, rebuild again. With `-BackendTargets`, **each backend's
Editor / Game / Server targets are built once into separate `Backends/<backend>/` trees,
and switching afterwards only changes module manifests - no compile, no overwrite**. The
switch works for Development, Release, `-Carrier Game` and `-NetMatrix`.

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

# Development: Editor or Game carrier
.\Tools\RunTests.ps1 -Profile Development -BackendTargets -Backend Both
.\Tools\RunTests.ps1 -Profile Development -BackendTargets -Backend Both -Carrier Game

# Connected network matrix (each cell launches real server/client processes)
.\Tools\RunTests.ps1 -Profile Development -NetMatrix -BackendTargets -Backend Both -SkipBuild

# Full Win64 Release matrix (isolated targets + per-backend staging directories)
.\Tools\RunTests.ps1 -Profile Release -BackendTargets
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
change isolated mode, where the Editor target is selected explicitly per backend by
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
| Carrier | Editor / Game / Server are all isolated. `-BackendTargets` combines with `-Carrier Game`, `-NetMatrix` and `-Profile Release` |
| Release | Each backend uses isolated Editor/Game/Server targets and UAT `-stagingdirectory` pointing at `Saved/StagedBuilds/{Windows,WindowsServer}/Backends/<backend>` (UAT cleans the staging directory on every `-stage`, so a shared one would delete the other backend's exe); cache fingerprints include `isolated`/`targets`, and the run restores the Default manifest layout and the original ini at the end |
| Manifests | Building an isolated target rewrites that backend's manifests. Building an ordinary target afterwards recreates root manifests - run `SwitchBackend.ps1` again |
| Concurrency | Switching is still global state; `Saved/RunTests.lock` remains in force. Do not build or open the editor during a switch |
| Host projects | `Tools/unluanova.ps1 doctor` now understands `Backends/<backend>/`, live/disabled manifests and the demo project's ini label: two DLL sets coexisting is no longer an error; two backends' manifests live at once is |

`Tools/tests/test_backend_isolation.py` guards the source contract: byte-identical layout
blocks, all six targets, end-to-end script plumbing, plus functional tests of the outer
doctor against synthetic Binaries trees.

## Automatic Stages

| Stage | Work |
|---|---|
| Repository/tools | Capability and link audit; Python regressions; PowerShell syntax, argument, process and timeout tests |
| Analyzers | Initialize the MSVC environment and build pinned LuaLS and luau-lsp |
| Backends | Run both Lua and Luau with Timer/Event/OOP and static binding enabled |
| Editor | Archive old static binding source, build Editor/generator, regenerate and rebuild the generated code |
| Development | Build/cook/package Game and Dedicated Server targets into default output paths |
| Editor automation | All registered UnLuaNova tests, including Slow, lifetime, extensions, showcase media and benchmarks |
| Connected peers | Use this backend's current staged Server/Game for one DS and two clients; verify two server-spawned actors, two rounds of Lua field replication, bidirectional RPCs, owner-targeted delivery and multicast with raw per-peer observations |
| Types | Back up old generated declarations, export fresh declarations, require successful backend type checks |
| Cooked automation | Run all tests registered for the Development game's actual UE context |
| Memory | Require `UnLuaNova.Memory` evidence in both backend/carrier combinations: VM allocation balance, resource release and warmed heap growth |
| Shipping | Build/cook/package into default output paths and verify the executable; this is not Shipping runtime certification |
| Cleanup | Restore original config bytes and changed environment variables, clean up owned processes, write an aggregate verdict |

`Saved/RunTests.lock` prevents concurrent runner backend changes. Avoid editing/building the same project in another process during validation. Configuration is restored; the last built binaries still belong to the last backend.

Release sets `UNLUANOVA_TEST_PEER_EXE` and `UNLUANOVA_TEST_SERVER_EXE` to the current Game and Server executables in default staging paths. Compilation reuses `Binaries/Win64`; packaging reuses `Saved/StagedBuilds/Windows` and `WindowsServer`. Subsequent backends and Shipping overwrite the corresponding outputs without copying per-run executables. Each backend's `metadata/binary-*.json` retains the path, size and SHA-256 at the time of use. A UE 5.8 Game binary cannot become a dedicated server through `-server`. Topology records each peer's executable; cleanup checks its PID, path and start time.

Release Editor, Game and non-headless peers use `-RenderOffScreen`, retaining real RHI, viewport tests, single game-viewport screenshots independently of desktop window visibility. Desktop recording, frame sequences and video encoding have been removed; DS still uses `-NullRHI`. Development can explicitly select `-RenderOffScreen`.

The Release test Editor uses `AboveNormal` process priority, recorded with its PID in `metadata/process.json`, to reduce scheduling interference from normal-priority background tasks. Only the owned test process changes. Original references used Normal priority; this environmental difference cannot establish a runtime optimization. Workload, sampling, references and limits remain unchanged, and frequency, memory and other workloads can still affect results.

LuaLS builds use a unique temporary directory under `Saved/ReleaseTools/luals-temp/` and restore TMP/TEMP on exit. Bootstrap and main analyzer builds/tests have separate exit-code checks; either failure blocks the analyzer stage.

Static binding backups live in each backend's `metadata/static-binding-backup/`. Only the fixed output file bearing the generator's ownership marker is archived. If no replacement exists when the pipeline finishes, the original is restored and the backup retained. Old generated code therefore cannot prevent the updated generator from building, and operators need no manual source cleanup. Fingerprints include a generator format version to invalidate obsolete output.

DAP session tests use OS-assigned loopback ports and nonblocking game-thread polling with persistent TCP and UTF-8 byte buffers. Requests time out after 5 seconds and sessions after 20 seconds; cleanup removes polling before closing sockets. Full Editor runs for both backends include session, unknown-command, fragmented-request, frame-decoding, concurrent-stop and restart regressions.

## Verdict

Read `Saved/ReleaseTests/test/release-summary.md` or `release-summary.json` (`Saved/ReleaseTests/baseline/` with `-Baseline`). Those two directories are the entire history: the run being judged, and the run it is judged against, each overwritten by its own next run. A `test` run adds a "Compared With Baseline" section listing stages that passed in the baseline but not now, and carriers that lost tests or gained failures; the comparison is reported, not enforced.

Stages report `passed`, `cached`, `failed` or `blocked`. `cached` means the stage's inputs did not change and the previous run's artifacts were reused (see below). Any failure or blocked dependency returns nonzero. An in-progress summary has `complete=false`. Each external stage has a default 60-minute timeout, adjustable with `-TimeoutMinutes`.

## Build artifact reuse

Compiling, cooking, packaging, exporting declarations and type checking only need to happen when their inputs change, so `Tools/ReleaseCache.ps1` fingerprints them: each stage hashes the inputs that decide its output (source, assets, project config, engine version, backend, analyzer) and records them in `Saved/ReleaseCache/index.json` together with its artifacts' path, size, modification time and SHA-256. A stage is recorded as `cached` and skipped when the fingerprint matches and the artifacts still match the record; a deleted artifact, one replaced by another build, or any changed input is a miss and the stage runs.

- Test, evidence and report stages are never cached. They are this run's evidence, not an input.
- The build tree is single-slot: the backend is a compile-time choice, so building the second backend overwrites the first one's binaries. The pipeline therefore starts with whichever backend the tree currently holds (read from `Definitions.UnLuaNova.h`): that backend reuses the previous run's compile, cook and package, and the other one rebuilds and leaves the tree for the next run. A fixed order would rebuild both every time.
- Declarations are exported once for both backends. Type check summaries (`*-analyze.json`) are copied back from the cache into the run directory on a hit, because the evidence gate reads them and "no file" is a failed gate rather than a skip.
- `-NoCache` forces everything to run again; deleting `Saved/ReleaseCache` does the same. `-PruneLegacy` deletes the old per-timestamp result directories (`Saved/ReleaseTests/<yyyyMMdd-HHmmss-fff>`).

Backend outputs include `editor/<backend>/`, `game/`, binary metadata and logs. The actual process exports `metadata/manifest.json` with backend, feature flags, Editor/cooked state and full leaf test paths. `ValidateRelease.py` compares this against UE's `index.json`. Empty, missing, duplicate, skipped, unknown, error-bearing and wrong-backend results fail. Editor source suites must also be registered. Historical display-name/count baselines no longer decide release completeness.

The Editor evidence gate requires all four Showcase PNGs. Cooked reports also collect newly generated single screenshots. Network evidence requires Server/Client1/Client2 logs, backend/role identity, matching RPCIds, distinct PIDs, open connections and matching server endpoints. DedicatedServer schema 2 additionally requires a shared server-generated SessionId, two distinct owners, two rounds of Lua HP observations and exact per-peer RPC logs. Standalone or declaration-only historical reports cannot pass. A peer-report helper's no-op does not prove transmission.

Editor diagnostics use the current Editor executable for all peers by default, avoiding stale Game binaries. Release explicitly selects its current staged Server/Game through environment variables. Clients never spawn the test actors or assign ownership locally.

Performance requires `Tools/baseline/perf-lua.json` and `perf-luau.json`, matching backend, engine, machine, case inventory and iteration counts, with a 25% regression limit. `perf-lua.json` was refreshed in `1455c38d` from an independent 2026-11-05 median-of-5 reference run; `perf-luau.json` still comes from the 2026-10-24 `FullFix-Complete-11` diagnostic run. See the baseline README. Neither is a pre-fix measurement, so neither establishes a speedup caused by the fixes. Missing comparable references fail the gate; the runner never promotes its own measurements automatically. External performance claims still require [controlled acceptance](Performance_Parity_slua.md).

Performance sampling warms each case for at least 250 ms, then records five samples of at least 100 ms each. Each sample repeats the same iteration batch; the result is the median of the five per-operation averages. `metadata/perf.json` retains raw elapsed times and batch counts. The gate recomputes the result and rejects missing, short or inconsistent samples. `perf-lua.json` now uses the current median-of-5 method; `perf-luau.json` still carries single-batch samples. Workload, per-batch iterations, units and thresholds remain comparable, but the sampling-method change cannot establish a runtime speedup.

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

* `AuditRepository.py` also validates inline/HTML local links, Markdown anchors (using
  MkDocs slug rules) and that remote links use plain https. Its output separates historical
  from current-tree evidence: a record with `historical: true` is a real run of an earlier
  tree and is **not proof of the current worktree**.
* `ValidateProvenance.py` makes that distinction fail-closed: it checks the evidence index,
  that all four release gates (two backends x two carriers) exist and belong to one run, and
  that `release-summary.json` agrees with them. `--require-current` fails unless at least one
  non-historical record matches the current HEAD.
* `GenerateNotices.py` deterministically assembles the shipped `NOTICES.txt` from license
  information already present in the repository; `--check` compares staged artifacts.
  Unresolvable rights-holder information is listed as `UNRESOLVED` and fails the gate rather
  than being invented.
* `ValidateRelease.py`'s showcase gate now requires a PNG signature and an IHDR header, so a
  renamed text file can no longer satisfy it.

These are **static structure gates**. They prove evidence files are complete, consistent and
reproducible; they do not prove UE actually ran. Runtime behaviour, real coverage and remote
reachability still belong to the full Release and the specialised acceptance runs.

`GenerateNotices.py --check` still **honestly fails**: 11 `UNRESOLVED` items and two staged
`NOTICES.txt` files that disagree with the generated list. `ValidateProvenance.py` is different:
checked against the tested commit
(`python Tools/ValidateProvenance.py --require-current --head 1455c38d`) it is PASS with four
current-tree records, while running it against the current HEAD fails by fail-closed design
because the newest evidence is bound to `1455c38d` and HEAD has since moved to documentation
and tooling commits. Neither result is a broken script: the first is incomplete legal data, the
second means "no execution is bound to the new HEAD". Do not edit the evidence index or invent
notice text to turn those commands green; making HEAD itself pass requires another full Release.

**New fail-closed checks in this pass** (tooling level, covered by static regressions): each of
the four carriers has its own committed case-set baseline (`Tools/baseline/lua.json` /
`luau.json` for Editor, `lua-game.json` / `luau-game.json` for cooked), and both the Release
editor tests and the report stage pass `--fail-on-drift`. In cooked logs, any
`LogAutomationController: Error` other than the known missing UE HTML report template fails
the stage. A failed declaration export rolls the archived IntelliSense tree back, the
declaration cache verifies both Luau and EmmyLua dialects through `declarations.sha256`, and a
failed static-binding stub write restores the shared generated source. When a case set changes
intentionally, review it first, then refresh the matching baseline with `-UpdateBaseline`
(Editor) or `GenerateTestReport.py --update-baseline` (Game).

## Development

Partial runs explicitly select `Development` and are not release evidence:

```powershell
.\Tools\RunTests.ps1 -Profile Development -Backend Luau -Filter UnLuaNova.API.Timer
.\Tools\RunTests.ps1 -Profile Development -Tier Fast
.\Tools\RunTests.ps1 -Profile Development -Filter UnLuaNova.Showcase -Backend Luau -Publish
```

`SkipBuild`, `BuildOnly`, `NullRHI`, `NoReport`, individual backends and custom filters are Development options. The release entry rejects reduced coverage. The nightly build installer also uses Development and only verifies compilation.

### Connected network matrix (separate opt-in)

Eight connected scenarios (late join, process-level reconnect, dormancy, relevancy, join-order swap, packet loss / reordering, `UNetConnection`-level reconnect, cross-peer declaration mismatch) and the Legacy / PushOff / Iris / Cooked variants each launch three real processes, so they are not part of the ordinary test matrix or the 33 normal Release stages:

```powershell
.\Tools\RunTests.ps1 -Profile Development -NetMatrix -Backend Lua -SkipBuild
python Tools\ValidateRelease.py --net-matrix --run Saved\ReleaseTests\net-matrix\Lua --backend Lua --carrier game
```

`Tools/RunNetMatrix.ps1` is the underlying script. `-ExtraArgs` puts `-UnLuaNovaNetScenarios`, `-UnLuaNovaNetPktLoss=25` and the variant's real engine switch (`-UseIrisReplication=1` or a push-model cvar ini override) on the process command line; `FNetSession` forwards the scenario flag, loss percentage, exact reconnect URL and `UNLUANOVA_TEST_NET_VARIANT` to every peer. The Cooked variant points the peer executables at staged `Windows` / `WindowsServer` binaries. `AppendWorldFacts` reads cvar state, `WITH_EDITOR` and `UNetConnection` counters back, while `BuildNetDeclarationReport()` exports FProperty-derived parameter specs and `RepSchemaVersion` / `VariantRepSchemaVersion`, so neither a renamed directory nor a disagreeing declaration can pass. Artifacts land in `net/scenarios/<Name>/` and `net/variants/<Name>/DedicatedServer/`, judged into `metadata/net-matrix-gate.json`. This is **not** UE automation evidence and does not touch `Tools/evidence/index.json`. The gate fails closed on a missing topology, a missing peer log, a PID mismatch, a faked Iris/push-model/Cooked flag, a mismatched or non-discriminating declaration fingerprint, a missing Generic field / UStruct parameter, zero connection byte counters or a scenario that never ran. **At present all eight scenarios and four variants have source and gates only, with no execution evidence.**

Preview the default plan without writing config, installing tools, compiling or launching UE:

```powershell
.\Tools\RunTests.ps1 -DryRun
```

Write this run as the baseline later runs are compared against (`Saved/ReleaseTests/baseline`):

```powershell
.\Tools\RunTests.ps1 -Baseline
```

Rebuild and re-package everything, ignoring the cache:

```powershell
.\Tools\RunTests.ps1 -NoCache
```

## Scope

Register new `UnLuaNova.*` suites in the test plugin's `Private/Specs` or `Private/Tests`. Source classification tracks suites; the runtime manifest tracks actual context-specific leaves. New registration macros require classifier support, not silent exclusions. Multiple Lua assertions may still aggregate into one UE test.

This entry covers the defined Win64 matrix, not every unfinished product requirement or 100% code coverage. Feature-off combinations, other platforms/devices, Shipping runtime smoke, Iris/push-model modes and external benchmark adapters remain [audit work](Benchmark_Review.md). License scope and third-party notices also require separate maintenance.

Validation status (only for the 2026-11-03 baseline run: `-Baseline`, `Saved/ReleaseTests/baseline`, commit `347bb8c5`): 33/33 stages passed with 2482/2482 UE executions, zero failures and zero skips — Lua Editor 626, Lua cooked 600, Luau Editor 641, Luau cooked 615. All four real DS + two-client gates (DedicatedServer and Multicast) passed, and both backends packaged Development, Server and Shipping. All 41 Python tooling tests and PowerShell support checks passed. Static analysis had zero errors, with 298 Lua warnings and seven Luau warnings retained — both are a known analyzer limitation on `package.path`-style requires and do not affect loading. Both LuaLS and luau-lsp run the binary built from their submodule, and the build does not modify submodule sources; LuaLS's bee dependency retains two upstream-disabled tests. Release22's old DedicatedServer case does not establish transport. Historical failures and acceptance limits are in the [regression record](Regression_20261024.md).

**Current release status: passed** (source: `Saved/ReleaseTests/test/release-summary.json`).
The 2026-11-07 full Release produced `complete=true`, `passed=true`: 33/33 stages passed, 2752/2752 UE executions, 0 failed, 0 skipped - Lua Editor 690, Lua cooked 661, Luau Editor 715, Luau cooked 686, with all four
release gates green. Every earlier failure was diagnosed and fixed (measurement-environment
interference, UBT mutex / interrupted child compilers, the Net regression, duplicate UFunction
overrides, the Luau safe-point sentinel, a cooked-shutdown use-after-free and a stale VM-hook
slot). The 25% gate was not relaxed; `perf-lua.json` was refreshed in `1455c38d` from an independent median-of-5 reference run matching the current sampler, while `perf-luau.json` and the 2026-11-03 baseline run are unchanged. New regressions cover idempotent
instance binding, GC safe-point semantics, hook-slot cleanup on Env destruction and a bounded
safe-point cancellation check.

Type-check summaries retain every warning and the raw `analyzer_exit`. The final `exit` follows error-level diagnostics: warnings alone do not block, while type errors, abnormal analyzer exits and missing results fail.

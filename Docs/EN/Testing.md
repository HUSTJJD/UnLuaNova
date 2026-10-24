---
title: "Testing and Automation"
description: "One release command with mandatory evidence and failure reporting"
---

# Testing and Automation

Run from the repository root:

```powershell
.\Tools\RunTests.ps1
```

The default `Release` profile orchestrates the current **Win64** matrix. Operators do not need separate backend builds, analyzer builds, declaration exports, network peers or test filters. It uses the project's associated UE installation, installed Visual Studio C++ tools and Python. On first use it initializes pinned submodules and installs isolated Python tools under `Saved/ReleaseTools`; it does not install UE or Visual Studio.

## Automatic Stages

| Stage | Work |
|---|---|
| Repository/tools | Capability and link audit; Python regressions; PowerShell syntax, argument, process and timeout tests |
| Analyzers | Initialize the MSVC environment and build pinned LuaLS and luau-lsp |
| Backends | Run both Lua and Luau with Timer/Event/OOP and static binding enabled |
| Editor | Archive old static binding source, build Editor/generator, regenerate and rebuild the generated code |
| Development | Build/cook/package/archive the game and build the Server target |
| Editor automation | All registered UnLuaNova tests, including Slow, lifetime, extensions, showcase media and benchmarks |
| Connected peers | Launch the current backend's cooked game, retaining topology, reports and logs; verify multicast reaches two clients |
| Types | Back up old generated declarations, export fresh declarations, require successful backend type checks |
| Cooked automation | Run all tests registered for the Development game's actual UE context |
| Memory | Require `UnLuaNova.Memory` evidence in both backend/carrier combinations: VM allocation balance, resource release and warmed heap growth |
| Shipping | Build/cook/package and verify the archived executable; this is not Shipping runtime certification |
| Cleanup | Restore original config bytes and changed environment variables, clean up owned processes, write an aggregate verdict |

`Saved/RunTests.lock` prevents concurrent runner backend changes. Avoid editing/building the same project in another process during validation. Configuration is restored; the last built binaries still belong to the last backend.

Static binding backups live in each backend's `metadata/static-binding-backup/`. Only the fixed output file bearing the generator's ownership marker is archived. If no replacement exists when the pipeline finishes, the original is restored and the backup retained. Old generated code therefore cannot prevent the updated generator from building, and operators need no manual source cleanup. Fingerprints include a generator format version to invalidate obsolete output.

DAP session tests use OS-assigned loopback ports and nonblocking game-thread polling with persistent TCP and UTF-8 byte buffers. Requests time out after 5 seconds and sessions after 20 seconds; cleanup removes polling before closing sockets. Full Editor runs for both backends include session, unknown-command, fragmented-request, frame-decoding, concurrent-stop and restart regressions.

## Verdict

Read `Saved/ReleaseTests/<timestamp>/release-summary.md` or `release-summary.json`. Stages report `passed`, `failed` or `blocked`; any failure or blocked dependency returns nonzero. An in-progress summary has `complete=false`. Each external stage has a default 60-minute timeout, adjustable with `-TimeoutMinutes`.

Backend outputs include `editor/<backend>/`, `game/`, packaged Development/Shipping archives and logs. The actual process exports `metadata/manifest.json` with backend, feature flags, Editor/cooked state and full leaf test paths. `ValidateRelease.py` compares this against UE's `index.json`. Empty, missing, duplicate, skipped, unknown, error-bearing and wrong-backend results fail. Editor source suites must also be registered. Historical display-name/count baselines no longer decide release completeness.

All four Showcase PNGs and encoded clips are required. Connected network evidence must contain Server/Client1/Client2 logs, backend/role identity, matching RPCIds and actual multicast reception. A peer-report helper's no-op does not prove transmission.

Performance requires reviewed `Tools/baseline/perf-lua.json` and `perf-luau.json`, matching backend, engine, machine, case inventory and iteration counts, with a 25% regression limit. These baselines are currently absent. The first run retains `metadata/perf.json` and fails this gate; it does not automatically approve its own measurements. Baseline maintenance does not require operators to run extra test commands. External performance claims still require [controlled acceptance](Performance_Parity_slua.md).

## Memory Leak Gate

No extra command or manual `lua.memstats` toggle is required. Nine reports under `metadata/memory/` cover environment cycles, warmed heap stability, actual module hot reload, a deliberately retained graph, native smart-pointer payloads, delegates, UMG image/texture ownership, and both World/environment teardown orders. Reports must match the manifest's run ID, backend and carrier. Missing reports, unregistered cases, insufficient work or failed assertions fail release validation.

- Audited environments install a pointer ledger before VM creation and retain it through `lua_close`. Every closed VM must have zero live bytes, live blocks and accounting errors. Ordinary environments do not maintain this map.
- Lifecycle cases run 24 cycles; each World teardown order runs 12. Weak pointers verify native payload and UObject release; Timer cases check callbacks and captured owners.
- Heap stability and hot reload warm up for 8 rounds, then sample 32 full-GC rounds. Growth is limited to 8 KiB and 16 live blocks. Limits are fixed in code and the validator, not provided by reports.
- A control retains a graph across full GC, requires growth above 64 KiB, then releases it and verifies recovery. The validator checks measurements independently of `passed=true`.

This is scenario-based leak regression, not proof of process-wide memory safety. Native ownership checks do not cover every C++ allocation; GPU/RHI resources, Luau CodeGen executable pages, third-party libraries and long production sessions still need dedicated coverage. Process working-set retention alone is not evidence of a leak. New C++ cases and budgets await actual runs on both backends.

## Development

Partial runs explicitly select `Development` and are not release evidence:

```powershell
.\Tools\RunTests.ps1 -Profile Development -Backend Luau -Filter UnLuaNova.API.Timer
.\Tools\RunTests.ps1 -Profile Development -Tier Fast
.\Tools\RunTests.ps1 -Profile Development -Filter UnLuaNova.Showcase -Backend Luau -Publish
```

`SkipBuild`, `BuildOnly`, `NullRHI`, `NoReport`, individual backends and custom filters are Development options. The release entry rejects reduced coverage. The nightly build installer also uses Development and only verifies compilation.

Preview the default plan without writing config, installing tools, compiling or launching UE:

```powershell
.\Tools\RunTests.ps1 -DryRun
```

## Scope

Register new `UnLuaNova.*` suites in the test plugin's `Private/Specs` or `Private/Tests`. Source classification tracks suites; the runtime manifest tracks actual context-specific leaves. New registration macros require classifier support, not silent exclusions. Multiple Lua assertions may still aggregate into one UE test.

This entry covers the defined Win64 matrix, not every unfinished product requirement or 100% code coverage. Feature-off combinations, other platforms/devices, Shipping runtime smoke, Iris/push-model modes and external benchmark adapters remain [audit work](Benchmark_Review.md). License scope and third-party notices also require separate maintenance.

Validation status: tool regressions, script parsing and safe DryRun have passed. The full release pipeline and new C++ regressions still await validation through the single command above. Script checks do not establish successful compilation or UE runtime behavior.

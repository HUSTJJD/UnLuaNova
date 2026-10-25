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

Release Editor, Game and non-headless peers use `-RenderOffScreen`, retaining real RHI, viewport tests, screenshots and test-frame clips independently of desktop window visibility. Desktop recording is disabled; DS still uses `-NullRHI`. Development can explicitly select `-RenderOffScreen`.

The Release test Editor uses `AboveNormal` process priority, recorded with its PID in `metadata/process.json`, to reduce scheduling interference from normal-priority background tasks. Only the owned test process changes. Original references used Normal priority; this environmental difference cannot establish a runtime optimization. Workload, sampling, references and limits remain unchanged, and frequency, memory and other workloads can still affect results.

LuaLS builds use a unique temporary directory under `Saved/ReleaseTools/luals-temp/` and restore TMP/TEMP on exit. Bootstrap and main analyzer builds/tests have separate exit-code checks; either failure blocks the analyzer stage.

Static binding backups live in each backend's `metadata/static-binding-backup/`. Only the fixed output file bearing the generator's ownership marker is archived. If no replacement exists when the pipeline finishes, the original is restored and the backup retained. Old generated code therefore cannot prevent the updated generator from building, and operators need no manual source cleanup. Fingerprints include a generator format version to invalidate obsolete output.

DAP session tests use OS-assigned loopback ports and nonblocking game-thread polling with persistent TCP and UTF-8 byte buffers. Requests time out after 5 seconds and sessions after 20 seconds; cleanup removes polling before closing sockets. Full Editor runs for both backends include session, unknown-command, fragmented-request, frame-decoding, concurrent-stop and restart regressions.

## Verdict

Read `Saved/ReleaseTests/<timestamp>/release-summary.md` or `release-summary.json`. Stages report `passed`, `failed` or `blocked`; any failure or blocked dependency returns nonzero. An in-progress summary has `complete=false`. Each external stage has a default 60-minute timeout, adjustable with `-TimeoutMinutes`.

Backend outputs include `editor/<backend>/`, `game/`, binary metadata and logs. The actual process exports `metadata/manifest.json` with backend, feature flags, Editor/cooked state and full leaf test paths. `ValidateRelease.py` compares this against UE's `index.json`. Empty, missing, duplicate, skipped, unknown, error-bearing and wrong-backend results fail. Editor source suites must also be registered. Historical display-name/count baselines no longer decide release completeness.

The Editor evidence gate requires all four Showcase PNGs and encoded clips. Cooked reports also collect newly generated screenshots and encode clips. Network evidence requires Server/Client1/Client2 logs, backend/role identity, matching RPCIds, distinct PIDs, open connections and matching server endpoints. DedicatedServer schema 2 additionally requires a shared server-generated SessionId, two distinct owners, two rounds of Lua HP observations and exact per-peer RPC logs. Standalone or declaration-only historical reports cannot pass. A peer-report helper's no-op does not prove transmission.

Editor diagnostics use the current Editor executable for all peers by default, avoiding stale Game binaries. Release explicitly selects its current staged Server/Game through environment variables. Clients never spawn the test actors or assign ownership locally.

Performance requires `Tools/baseline/perf-lua.json` and `perf-luau.json`, matching backend, engine, machine, case inventory and iteration counts, with a 25% regression limit. Current references come from the independent `FullFix-Complete-11` diagnostic run, documented in the baseline README. They are not pre-fix measurements and cannot establish a performance improvement from these fixes. Missing comparable references fail the gate; the runner never promotes its own measurements automatically. External performance claims still require [controlled acceptance](Performance_Parity_slua.md).

Performance sampling warms each case for at least 250 ms, then records five samples of at least 100 ms each. Each sample repeats the same iteration batch; the result is the median of the five per-operation averages. `metadata/perf.json` retains raw elapsed times and batch counts. The gate recomputes the result and rejects missing, short or inconsistent samples. The original reference files remain unchanged and used single-batch samples. Workload, per-batch iterations, units and thresholds remain comparable, but the sampling-method change cannot establish a runtime speedup.

## Memory Leak Gate

No extra command or manual `lua.memstats` toggle is required. Nine reports under `metadata/memory/` cover environment cycles, warmed heap stability, actual module hot reload, a deliberately retained graph, native smart-pointer payloads, delegates, UMG image/texture ownership, and both World/environment teardown orders. Reports must match the manifest's run ID, backend and carrier. Missing reports, unregistered cases, insufficient work or failed assertions fail release validation.

- Audited environments install a pointer ledger before VM creation and retain it through `lua_close`. Every closed VM must have zero live bytes, live blocks and accounting errors. Ordinary environments do not maintain this map.
- Lifecycle cases run 24 cycles; each World teardown order runs 12. Weak pointers verify native payload and UObject release; Timer cases check callbacks and captured owners.
- Heap stability and hot reload warm up for 8 rounds, then sample 32 full-GC rounds. Growth is limited to 8 KiB of VM live heap and 16 allocator blocks. Schema 2 records GC-reported `live_bytes` separately from ledger `bytes` and `blocks`: Luau allocates pages, whose normal unused capacity can vary by about 32 KiB. Every allocation must still be released at VM close. Limits are fixed in code and the validator, not provided by reports.
- A control retains a graph across full GC, requires growth above 64 KiB, then releases it and verifies recovery. The validator checks measurements independently of `passed=true`.

This is scenario-based leak regression, not proof of process-wide memory safety. Native ownership checks do not cover every C++ allocation; GPU/RHI resources, Luau CodeGen executable pages, third-party libraries and long production sessions still need dedicated coverage. Process working-set retention alone is not evidence of a leak. `FullFix-Release-22` passed all four backend/carrier runs and evidence gates: 36 memory reports, with zero residue and accounting errors after closure.

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

Validation status: `ConnectedNet-Release-28` passed on 2026-10-25: 33/33 stages, 2482/2482 UE executions, zero failures and skips, all four real DS/two-client evidence gates, and both Shipping packages. All 40 Python tooling tests and PowerShell support checks passed. Static analysis had zero errors, with 298 Lua warnings and seven Luau warnings retained. LuaLS uses the documented initialization fixture patch; its bee dependency retains two upstream-disabled tests. Default outputs now contain Luau and are reused across runs. Release22's old DedicatedServer case does not establish transport. See the [complete regression record](Regression_20261024.md) for historical failures and acceptance limits.

Type-check summaries retain every warning and the raw `analyzer_exit`. The final `exit` follows error-level diagnostics: warnings alone do not block, while type errors, abnormal analyzer exits and missing results fail.

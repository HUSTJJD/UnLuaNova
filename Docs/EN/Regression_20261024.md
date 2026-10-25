---
title: "2026-10-24 Regression Fixes and Validation"
description: "Static binding crash, backend compatibility and release validation evidence"
---

# 2026-10-24 Regression Fixes and Validation

## Acceptance Status

The complete `FullFix-Release-22` run passed on 2026-10-25: `release-summary.json` records `complete=true`, `passed=true`, all 33 stages passed, and process exit code 0. The four UE automation runs passed **2478/2478 test executions with zero failures and skips**. All entries below come from this single complete run.

| Backend | Editor | Cooked Development |
|---|---:|---:|
| Lua | 625/625 | 599/599 |
| Luau | 640/640 | 614/614 |

UE counts 14 Lua Editor, six Lua cooked, 18 Luau Editor and ten Luau cooked cases as succeeded with warnings, 48 total. Their states are all `Success`; passing does not mean warning-free logs.

Environment: Windows 11, Win64, Unreal Engine 5.8.3 source build, Lua 5.5 and Luau. UE targets use MSVC 14.50; analyzers use MSVC 14.51.

The run lasted from 00:07:51 to 01:06:45 UTC on 2026-10-25. Development Game/Server builds, packages and archives, Shipping packages, and all four strict evidence gates passed. Published reports are available for [Lua Editor](../test-lua-report.md) and [Luau Editor](../test-luau-report.md); complete cooked reports remain in the local run directory.

## Final Evidence

| Check | Release22 result |
|---|---|
| Memory and resources | Nine reports per run, 36 total; zero remaining bytes, allocation blocks, resources and accounting errors after environment closure. Stability checks and the retained-reference control passed |
| Connected networking | DedicatedServer and multicast passed in all four runs. Each multicast reached both clients, with matching RPC IDs 1/2/3 and 14 NetFields |
| Static types | Lua: 92 files, zero errors, 298 warnings. Luau: seven files, zero errors, seven warnings. Both raw analyzer exits were 1; both parsed checker verdicts were 0, with warnings retained |
| Tool regressions | All 34 Python tests and PowerShell support checks passed |
| Documentation and delivery | Strict MkDocs build, the 40-group capability/local-link audit and `git diff --check` passed. All ten clips currently in the published directories decoded, including this run's eight Editor Showcase clips |
| Screenshots and clips | Four main screenshots and four Showcase clips per run, 16 of each total. All clips fully decoded; representative screenshots from each run were visually inspected |
| Performance | All checks passed against unchanged independent references and the unchanged 25% limit. Lua Editor `VM.Call`: 0.0273270003; Luau Editor `VM.TableGrow`: 0.0143017428 and `VM.TableRead`: 0.00807557255 microseconds per operation |

Evidence root: `Saved/ReleaseTests/FullFix-Release-22/`.

- `release-summary.md` and `release-summary.json`: all 33 stage verdicts, four report links, source diff hash, untracked file hashes, toolchain and submodule versions. `metadata/untracked-source/` retains the untracked source snapshot.
- `lua/editor/lua/`, `lua/game/`, `luau/editor/luau/` and `luau/game/`: each contains `report.md`, UE `index.json`, `metadata/manifest.json` and `metadata/release-gate.json`.
- Each run's `metadata/perf.json`, `metadata/memory/`, `net/`, `screenshots/` and `videos/`: raw performance samples, memory evidence, network peer reports/logs and media.
- Each backend's `packages/Development/`, `packages/Server/` and `packages/Shipping/`: this run's archives. Root `logs/` retains build, packaging, analyzer and tool logs; Python test output is in `logs/python-tests.log.stderr`.

Report publication and documentation finalization followed the completed tests; provenance retains the source state at run startup. Configuration was restored, and the last built binaries belong to Luau.

## Changes

| Problem | Cause and resulting behavior |
|---|---|
| Crash in the first Base test | Static binding resolution writes descriptors that were generated as const; descriptors now have a writable contract and are regenerated before compilation |
| Console registration conflict | CodeGen command and variable shared a name; the variable is now `lua.codegen.enabled` |
| Hot reload | Preserve loader data, locate `_ENV` correctly, use Luau environments, traverse stack frames iteratively, reject primitive visited keys and update the module cache |
| Incremental GC stalls | Account for the existing heap when changing modes and preserve minimum work while a collection remains incomplete |
| Class-name binding | Bare names use `ClassName`; full object paths continue to use `Class` |
| Lua-declared RPCs | Preserve declaration templates and function identity, register root RPCs in NetFields, and invoke Lua handlers by their declared names |
| Cross-process network decoding | Register RPCs before NetDriver caches the class; configured prebinding is checked again after engine initialization |
| Multicast test lifecycle | Wait for both replicated actors, snapshot server topology at send time, then wait for client receipt reports |
| Intermittent peer report reads | File existence can precede completed JSON writes. Write reports and topology to sibling temporary files and publish after closing; readiness requires successful parsing. `UnLuaNova.API.Net.ReportReadiness` covers temporary files, incomplete JSON and complete reports |
| Static complex properties | Delegate/container properties use their specialized reflection descriptors rather than an unsupported static descriptor |
| Test-order dependency | Static binding tests restore the original generated table after each case |
| Missing Blueprint declarations | Compare full asset class paths on UE 5.1 and later |
| Blueprint cook failures | An earlier module rename and asset save lost native references; recover the complete historical graphs for three Benchmark Blueprints, the Issue494 Blueprint/map and the Issue500 map, then migrate with precise redirects and UE resaving |
| Cooked map tests | OpenLevel now travels to the requested map and waits for its new world to begin play, with the Lua environment activated for map tests |
| Cooked allocator/template assumptions | Reserve checks minimum capacity and insertion without reallocation; editor template files are checked only in the editor |
| Struct-array destruction after map unload | A live property owner does not guarantee a live UScriptStruct; descriptors retain the element type and destroy values through it. A Lua API test forces UE GC before reading/finalizing a struct array, then verifies type release after the environment closes |
| Packaged server role | UE 5.8 Game binaries cannot select dedicated mode with -server; package a Server target and select peer executables by role |
| Shipping compilation | Guard DanglingCheck and cross-process network test implementations with `WITH_DEV_AUTOMATION_TESTS`. Guard profiler Insights calls with `CPUPROFILERTRACE_ENABLED`, preserving statistics and sampling that do not depend on CPU Trace |
| Release automation startup | Separate engine ExecCmds with commas while retaining semicolons inside the Automation command chain |
| Benchmark validity and short-sample variability | Prevent native-loop elimination; warm for at least 250 ms, then take five samples of at least 100 ms each and the median of normalized batch means. The gate independently checks raw samples |
| Report provenance | Store benchmark data in each run directory; display memory/resource evidence from the same run |

## Struct Container Lifetime

A Lua-owned struct array can outlive a map unload. Previously, destruction reached the element type through its property descriptor, but a valid property owner did not establish that the referenced `UScriptStruct` was still alive. The diagnostic stack was `lua_close -> FLuaArray::~FLuaArray -> FPropertyDesc::Destruct -> FStructProperty::DestroyValueInternal`.

`FScriptStructPropertyDesc` now retains the type through `FObjectReferencer` and saves its pointer and `ArrayDim`. Destruction calls that type's `DestroyStruct` directly, independent of the original declaring property. Retention lasts for the descriptor lifetime: clearing one array does not promise immediate type collection because the environment's descriptor cache may still need it.

`UnLuaNova.Regression.StructContainerLifetime` creates a transient struct with a string field, constructs an array through public Lua APIs, removes the Lua type variable, and forces UE GC before reading and clearing the array. It then closes the independent Lua environment, forces UE GC again, and checks that the weak type pointer has expired. This covers both valid destruction and eventual release of the retained type.

## Memory and Static Analysis

Memory schema 2 records both physical allocator bytes/blocks and VM live bytes. Luau page capacity can fluctuate without retaining live objects, so steady-state growth uses VM live bytes. Closing the VM must still leave zero physical allocations and resources. A retained-reference positive control checks that the instrumentation can detect retention.

Type errors, abnormal analyzer exits and missing valid results fail. Parsed warnings remain nonblocking; summaries retain both `analyzer_exit` and the checker verdict `exit`. Luau runtime module names can resolve through package paths that luau-lsp does not understand. These warnings mean inference across those require boundaries is incomplete.

## Diagnostic History

Diagnostic run `FullFix-Luau-02` ran 638 Editor cases: 621 passed and 17 failed. Subsequent focused runs isolated and fixed additional failures; they do not establish release acceptance. The original crash and Issue562 have focused results for both backends in `Saved/TestReports/StaticBindingCrash-Fix`.

The final tooling check passed 34 Python tests and the PowerShell support tests. The LuaLS bootstrap dependency previously reported seven filesystem/Unix socket failures under the system temporary directory. With an isolated workspace temporary directory, the same suite passed 110 cases with zero failures and two skips. Those skips are upstream-disabled Lua stack overflow tests; their test list was not changed. The build script now isolates and restores TMP/TEMP and checks bootstrap and main-build exit codes separately, preventing a later success from masking bootstrap failures.

Full Development diagnostic run `FullFix-Complete-11`: Lua passed 622/623 cases. Its sole failure was a stack fixture whose tail calls removed expected frames; the fixture now stores return values explicitly. Luau passed 638/638, including the corrected fixture. Both type checks reported zero errors, with 298 and seven warnings respectively. Connected multicast reached both clients on both backends. This Editor-only run does not establish cooked or packaging acceptance.

Release builds luau-lsp with an explicit Ninja generator in `Saved/ReleaseTools/luau-lsp-ninja`, avoiding a conflict between pinned CMake and existing Visual Studio generator caches.

After asset recovery, an independent full cook completed 732/732 packages with zero errors and warnings. Lua DebugService passed a focused 8/8 rerun. `FullFix-LuaCooked-16/regression` passed 56/57; the sole failure was an object-path/package-path comparison timeout. Struct destruction and the new type-lifetime case passed. These are diagnostic results, not full Release acceptance.

After the fix, `FullFix-LuauCooked-18/regression` passed 59/59. Its full preliminary run passed 611/613; two network cases failed because the Server package was still being built. Four screenshots and four clips were generated. This manual invocation omitted the Release token, so strict memory provenance validation failed. It also overlapped compilation and cannot establish final memory or performance acceptance.

The official Luau cooked stage in `FullFix-Release-17` passed **613/613 with no skips**. Runtime inventory, all nine memory reports, and real DedicatedServer/Multicast evidence passed validation; four screenshots and four clips were added to its report. Luau Editor passed 638/639. Its only failure was `VM.TableGrow`: 0.021013 microseconds per operation versus the independent 0.015414 reference, a 36.3% increase exceeding the 25% limit. The reference remains unchanged pending repeat measurements. Lua did not execute because of the test linkage issue that had not yet been fixed at that stage. Some fixes also postdate this run's source snapshot, so this run cannot establish final acceptance.

The first Shipping source build exposed missing compilation guards around test implementations and CPU Trace calls, addressed as described above. After compilation exited, three independent Editor performance diagnostics (`FullFix-Perf-18b/18c/18d`) measured `TableGrow` at 0.020612, 0.015199 and 0.014035 microseconds per operation. The first exceeded the limit; all performance cases passed in the latter two. This demonstrates measurement variability, not a stable regression or speedup. References, measurement logic and thresholds remain unchanged; final acceptance still requires a passing complete Release run.

`FullFix-Shipping-18` completed the corrected Luau Shipping incremental build, link, cook, package and archive with UAT exit code 0. In `FullFix-Release-19`, Lua Editor passed 624/624, Lua cooked passed 598/598, and Luau Editor passed 639/639. Luau `TableGrow` measured 0.017375 microseconds per operation, within the original 25% limit. Luau cooked passed 612/613: DedicatedServer could not read the server report during comparison, while connected multicast passed. This exposed the readiness race between file existence and readable JSON, addressed above with an additional regression. The failed run is preserved; the corrected complete rerun is `FullFix-Release-20`.

`FullFix-Release-20` completed all 33 stages: Lua Editor passed 624/625, Lua cooked 599/599, Luau Editor 640/640, and Luau cooked 614/614, with no skips. Both Shipping packages succeeded. The sole failure was Lua Editor `VM.Call`: 0.033788 microseconds per operation versus the independent 0.026266 reference, a 28.6% increase exceeding the 25% limit. Both the automation stage and its evidence gate correctly failed, leaving `complete=true`, `passed=false`. Report readiness, DedicatedServer and connected multicast passed in all four runs. Raw results remain intact pending focused performance diagnostics and another complete acceptance run.

Three subsequent independent Lua Editor diagnostics (`FullFix-Perf-21a/21b/21c`) each passed 2/2 cases. `VM.Call` measured 0.025297, 0.024287 and 0.025425 microseconds per operation, all below the original reference. References, iteration counts, measurement logic and thresholds were unchanged. Unrelated background applications were consuming CPU on the machine; measurement variability remains a limitation, without attributing the exceedance to a specific process or claiming a performance improvement. The next complete acceptance run is `FullFix-Release-21`.

`FullFix-Release-21` passed Lua Editor 625/625, Lua cooked 599/599, Luau Editor 639/640 and Luau cooked 614/614, with no skips. Both Shipping packages passed. Lua `VM.Call` measured 0.027748 microseconds per operation, within the limit. The sole failed case contained Luau `VM.TableGrow` (0.021962) and `VM.TableRead` (0.010876), each approximately 42.5% above its reference. This run remains failed; successful subsets from different runs cannot be combined into one complete verdict.

Following repeated short-sample variability, each case now warms for at least 250 ms and records five samples of at least 100 ms each. Samples repeat the same iteration batch, normalize elapsed time per batch and take the median. Batch contents, per-batch iterations, original references and the 25% limit are unchanged; total sampling time and raw evidence increase. The strict gate recomputes the median and rejects short, missing or inconsistent samples. Tool regression coverage increased to 34 passing tests. Older references used single-batch samples, so the method difference remains a limitation and cannot establish a runtime speedup. Release22 subsequently validated the complete matrix with this change.

With the new sampler, Luau diagnostics `FullFix-Perf-22a/22b/22c` each passed 2/2 cases. `TableGrow` measured 0.015982, 0.014371 and 0.016551; `TableRead` measured 0.008285, 0.007716 and 0.008346 microseconds per operation. Lua diagnostic `FullFix-Perf-22d` passed 2/2 with `VM.Call` at 0.026046. Independent validation of raw samples and original references passed for all four diagnostics. Reference SHA-256 hashes match their provenance record. The complete rerun is `FullFix-Release-22`.

Following these diagnostics, `FullFix-Release-22` completed and passed the entire matrix, as recorded at the start of this document. Historical failures remain preserved. Performance references came from an independent run and were not replaced with measurements under test.

## Acceptance Limits

Full-session desktop capture failed: Windows gdigrab returned error 5 and ffmpeg exited with -22. No full-session recording is available. All 16 Showcase clips generated from test frames fully decoded, but they do not constitute a full-session recording.

The timed multiple-batch median sampler differs from the original single-batch reference method, and unrelated background CPU activity introduces variability. Passing establishes regression acceptance under the original thresholds, not a runtime speedup. Memory evidence covers the defined scenarios, not process-wide memory safety.

See [Testing](Testing.md) and [Lua Networking](LuaNetworking.md). Shipping packaging is not Shipping runtime certification. Other platforms, every feature-off combination, Iris and the broader networking scenarios listed in the networking guide remain outside this acceptance scope.

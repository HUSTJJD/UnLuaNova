# Current Source Audit and Roadmap

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

Audited 2026-10-24; fixes and a verification run added 2026-11-02, and a complete Release validation completed 2026-11-03 (both at the end of this page). **Full slua-unreal/FUnLua parity, superior performance and complete test coverage are not established.** Source implementation, test existence, execution success and platform acceptance are distinct evidence levels. See the [capability matrix](../feature-matrix.md).

## Baselines

slua-unreal: `7970ab72fd59118e1b3f86aac2d016828b54b748`; FUnLua: `7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3`; upstream Luau 0.744: `32c28089305f488f901aaf1dbf3fd1cb65fa3d11`; integrated Luau: `6d36d483cb88973276af611198169f76f3ce05b0`. No dependency upgrade was made. Public VM headers match between the Luau pins, but 18 VM implementation files differ. UnLua was checked as the inherited source, not exhaustively re-audited.

## Findings and Acceptance

| ID / Priority | Current issue | Required acceptance |
|---|---|---|
| NET-01 / P0 | Fixed None layout now rejects conditional declarations; old non-None component settings disable replication. **Replication declarations are now owned per Env** (fixed 2026-11-02): `FUnLuaNovaNet::Cleanup(Env)` drops only that Env's entries plus dead classes, so closing one Env no longer wipes another's schema. Full conditional replication is still missing | Stable conditional layout; multiple actors, owner/nonowner, mixed conditions, late join, dormancy; legacy/Iris/push model separately; multi-Env declaration and teardown order still needs execution |
| NET-02 / P0, specialised acceptance outstanding | Strict int32/int64 bucket conversion, atomic property validation, immutable cached schemas and whole-RPC parameter rejection implemented | Added real bucket round-trip/bad-input tests; run both backends and connected-peer/schema mismatch cases |
| LIFE-01 / P0, specialised acceptance outstanding | Main-state callbacks, Env/World close cleanup, explicit World, isolated handles, remaining-delay pause and reentry protection implemented | Added coroutine GC, teardown-order, multi-World and cross-Env tests; world pause/time dilation, multi-PIE and packaged specialised acceptance outstanding |
| LIFE-02 / P0, fixed, specialised acceptance outstanding | Manual proxies and known smart-pointer paths now use native destructors; obsolete proxies cannot release replacements; unconstructed/released payloads skip destruction. **UScriptStruct copies now get a finalizer** (fixed 2026-11-02): `ClassRegistry.cpp` registered `__gc` without an `UNLUANOVA_HAS_GC_METAMETHOD` guard, and Luau has no `TM_GC`, so struct copies never ran `DestroyStruct`. `NewScriptStructUserdata()` now stores the type pointer inside the block and attaches a native destructor | Added forced-GC/Env-close tests; finish allocation audit and run both collectors/backends; no Lua stack work in native destructors; struct arrays and DataTable row copies need dedicated memory acceptance |
| COMPAT-01 / P1 | Upvalue identity, registry/uservalue and load mode are approximate or placeholders | Explicit supported subset; shared scalar closure aliases, reload rollback, source/bytecode modes |
| BIND-01 / P1 | UClass-keyed binding is not independent per-instance module binding | Same-class A/B modules, inheritance, unbind and hot reload isolation |
| EVENT-01 / P1 | Tokens hold owners strongly; weak-key indexing does not release them automatically | Explicit unsubscribe contract; recursive Once fixed this round, teardown and closure ownership still need execution |
| SAFE-01 / P1 | Read-only globals do not restrict UE/debug/io/socket capabilities; io is not an OS sandbox | Trusted/untrusted policies, path/link/quota/error/timeout checks, debug service exposure policy |
| TOOL-01 / P1 | Coverage export/gates incomplete; CodeGen counts requests. Default release now requires checkers, declarations, runtime manifests and provenance; end-to-end specialised acceptance outstanding | Execute the single entry; define coverage denominators and exclusions |
| QA-01 / P1 | Old reports cannot certify current edits; some network tests compare unconnected processes | Real server/clients, lifecycle/error cases, packaged/platform runs, feature-based acceptance |
| PERF-01 / P1, self-baseline committed, cross-project comparison still missing | `Tools/baseline/perf-lua.json` and `perf-luau.json` **are committed** (2026-10-24, Win64 Editor Development, UE 5.8.3, Ryzen 7 4700G) and gate Release at 25% regression. On the same machine and build Luau beats Lua 5.5.1, by **2.08×** on the container workload. Thunks still call `ProcessEvent`. **No same-machine equivalent-adapter comparison against slua-unreal / FUnLua exists**, so no cross-project speed conclusion is established | Write equivalent adapters for all three projects and run them on one machine; record distributions, allocations, GC, cold start and code size |
| LEGAL-01 / closed | Root license changed to **MIT** on 2026-11-02, matching the MIT headers of inherited files; the size threshold, additional terms and commercial licensing are gone (`COMMERCIAL.md` and `CLA.md` deleted). The earlier root-versus-file scope conflict no longer exists | Remaining work is independent of the license: complete version, provenance and shipped-notice inventory for extensions, tools, binaries and sample assets; new third-party imports need URL/revision, retained notices and modification notes |

The [Chinese audit](../CN/Benchmark_Review.md) contains the expanded implementation and acceptance detail. Both use the same IDs.

## Order and Innovation

First fix network/lifetime correctness; then complete feature acceptance, examples and traceable reports; finally run controlled benchmarks, packaged/platform validation and license review. Timer now uses nonreused Env-scoped handles and a close barrier; registration-site diagnostics remain missing. Shared binding/type metadata and complete network schema negotiation remain proposals.

Timer ownership, manual/smart reference finalization, exact networking integers and strict declarations are implemented with regression sources and updated examples/docs. CodeGen VM ownership, io validation/CRLF, RPC ordering/schema fingerprints and Event.Once reentrancy also have fixes. All of it was built from source and executed by the complete Release validation of 2026-11-03 (see the end of this page).

Maintainer validation uses only `.\Tools\RunTests.ps1`. Its default Release pipeline orchestrates both backend builds, Timer flags, full automation, connected cooked peers, type checks and packaging, failing on missing evidence. No extra test commands are required. That pipeline ran end to end on 2026-11-03 (33/33 stages); see [testing](Testing.md) for scope.

## 2026-11-02 addendum

This round compiled and executed, not just read. Environment: Windows 11, UE 5.8.3 source engine, Win64, MSVC 14.50.

Two fixes, both reflected in the table above:

| Item | Fix | Where |
|---|---|---|
| LIFE-02 | `UScriptStruct` copies were never destroyed on Luau. `ClassRegistry.cpp` registered `__gc` with no `UNLUANOVA_HAS_GC_METAMETHOD` guard, and Luau has no `TM_GC`, so the registration was dead code; the allocation sites passed no destructor either. A copy holding an `FString` or `TArray` member therefore never ran `DestroyStruct`. `NewScriptStructUserdata()` now writes the type pointer into the block (the 8 bytes ahead of the padding, so the reader's address formula is unchanged) and attaches a native destructor | `LuaCore.h/.cpp`, `PropertyDesc.cpp`, `LuaLib_DataTable.cpp`, `ClassRegistry.cpp` |
| NET-01 | `FUnLuaNovaNet::RepProps` is a process-level map, `Cleanup()` emptied it wholesale, and it was triggered per Env from the Manager destructor — closing any one Env cleared every Env's schema. Ownership is now recorded per Env; `Cleanup(Env)` drops only that Env's entries plus entries whose class is gone | `UnLuaNovaNet.h/.cpp`, `UnLuaNovaManager.cpp` |

Result (both backends built from source, no new compile errors; Editor carrier only):

| Backend | Tests | Result |
|---|---:|---|
| Luau (default) | 527 | 527 passed / 0 failed / 0 skipped |
| Lua 5.5.1 | 502 | 502 passed / 0 failed / 0 skipped |

Covers `UnLuaNova.API.*`, `UnLuaNova.Regression.*`, `UnLuaNova.Binding`, `UnLuaNova.Memory`, `UnLuaNova.Editor`, `UnLuaNova.Settings`, `UnLuaNova.Lua` and `UnLuaNova.Extensions`.

**This is not full Release acceptance**: no cooked / packaged / Shipping run, no real dedicated-server plus multi-client networking, no performance gate or type check, and no `UnLuaNova.Perf` or `UnLuaNova.Net.*` segments. Those were covered the next day — see below.

## 2026-11-03 baseline Release validation (validated)

`.\Tools\RunTests.ps1 -Baseline` ran the full matrix and passed; artifacts are in
`Saved/ReleaseTests/baseline`. That run's provenance records commit `347bb8c5` with 8 uncommitted
worktree changes, so the numbers below describe only that run. Environment: Windows 11, UE 5.8.3
source build, Win64, Visual Studio 2026 18.10 (MSVC 14.50).

| Backend | Carrier | Tests | Result |
|---|---|---:|---|
| Luau (default) | Editor | 641 | 641 passed / 0 failed / 0 skipped |
| Luau (default) | cooked `-game` | 615 | 615 passed / 0 failed / 0 skipped |
| Lua 5.5.1 | Editor | 626 | 626 passed / 0 failed / 0 skipped |
| Lua 5.5.1 | cooked `-game` | 600 | 600 passed / 0 failed / 0 skipped |

All 33 stages passed, covering cooked runs, real dedicated-server plus two-client networking
(both DedicatedServer and multicast sessions), each backend's type check (0 errors), the 25%
performance gate, and Development / Server / Shipping packaging — including the
`UnLuaNova.Perf` and `UnLuaNova.Net.*` segments.

**What this run closes is the "fixed but never executed" layer**: the LIFE-02 and NET-01 fixes above
now have execution evidence. **What it does not close is each row's specialised gap** — conditional
replication, multi-PIE and world pause, Iris/push model, dedicated memory acceptance for struct
arrays and DataTable row copies, and a cross-project same-machine comparison. Those remain scope
statements, not "executed but unrecorded".

## Latest regular validation (not passed)

`Saved/ReleaseTests/test` (finished 2026-11-03 15:23 local time) reports `complete=true`,
`passed=false`: five Luau Editor benchmark cases exceeded the 25% gate, `Luau-editor-tests` failed
and `Luau-editor-evidence` was blocked. Product sources, manifests and process priority are
unchanged between the runs, so measurement-environment interference is the leading explanation.
The baseline is not replaced and the gate is not relaxed. The latest run decides current status;
see [Current Version Status](ReleaseNotes.md).

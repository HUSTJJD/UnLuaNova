# Current Source Audit and Roadmap

Audited 2026-10-24. **Full slua-unreal/FUnLua parity, superior performance and complete test coverage are not established.** No compilation or UE automation was started. Source implementation, test existence, execution success and platform acceptance are distinct evidence levels. See the [capability matrix](../feature-matrix.md).

## Baselines

slua-unreal: `7970ab72fd59118e1b3f86aac2d016828b54b748`; FUnLua: `7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3`; upstream Luau 0.744: `32c28089305f488f901aaf1dbf3fd1cb65fa3d11`; integrated Luau: `6d36d483cb88973276af611198169f76f3ce05b0`. No dependency upgrade was made. Public VM headers match between the Luau pins, but 18 VM implementation files differ. UnLua was checked as the inherited source, not exhaustively re-audited.

## Findings and Acceptance

| ID / Priority | Current issue | Required acceptance |
|---|---|---|
| NET-01 / P0 | Fixed None layout now rejects conditional declarations; old non-None component settings disable replication. Full conditions and multi-Env ownership of global schema cleanup remain incomplete | Stable conditional layout; multiple actors, owner/nonowner, mixed conditions, late join, dormancy; legacy/Iris/push model separately |
| NET-02 / P0, execution pending | Strict int32/int64 bucket conversion, atomic property validation, immutable cached schemas and whole-RPC parameter rejection implemented | Added real bucket round-trip/bad-input tests; run both backends and connected-peer/schema mismatch cases |
| LIFE-01 / P0, execution pending | Main-state callbacks, Env/World close cleanup, explicit World, isolated handles, remaining-delay pause and reentry protection implemented | Added coroutine GC, teardown-order, multi-World and cross-Env tests; world pause/time dilation, multi-PIE and packaged execution pending |
| LIFE-02 / P0, execution pending | Manual proxies and known smart-pointer paths now use native destructors; obsolete proxies cannot release replacements; unconstructed/released payloads skip destruction | Added forced-GC/Env-close tests; finish allocation audit and run both collectors/backends; no Lua stack work in native destructors |
| COMPAT-01 / P1 | Upvalue identity, registry/uservalue and load mode are approximate or placeholders | Explicit supported subset; shared scalar closure aliases, reload rollback, source/bytecode modes |
| BIND-01 / P1 | UClass-keyed binding is not independent per-instance module binding | Same-class A/B modules, inheritance, unbind and hot reload isolation |
| EVENT-01 / P1 | Tokens hold owners strongly; weak-key indexing does not release them automatically | Explicit unsubscribe contract; recursive Once fixed this round, teardown and closure ownership still need execution |
| SAFE-01 / P1 | Read-only globals do not restrict UE/debug/io/socket capabilities; io is not an OS sandbox | Trusted/untrusted policies, path/link/quota/error/timeout checks, debug service exposure policy |
| TOOL-01 / P1 | Coverage export/gates incomplete; CodeGen counts requests. Default release now requires checkers, declarations, runtime manifests and provenance; end-to-end execution pending | Execute the single entry; define coverage denominators and exclusions |
| QA-01 / P1 | Old reports cannot certify current edits; some network tests compare unconnected processes | Real server/clients, lifecycle/error cases, packaged/platform runs, feature-based acceptance |
| PERF-01 / P1 | Benchmark thresholds exist but committed performance baselines are absent; thunks still call ProcessEvent | Same-machine controlled comparison and raw distributions |
| LEGAL-01 / release P0 | Root/file license scope unresolved; dependency inventory incomplete; FUnLua license file absent | Rights-holder scope decision and complete notices before distribution |

The [Chinese audit](../CN/Benchmark_Review.md) contains the expanded implementation and acceptance detail. Both use the same IDs.

## Order and Innovation

First fix network/lifetime correctness; then complete feature acceptance, examples and traceable reports; finally run controlled benchmarks, packaged/platform validation and license review. Timer now uses nonreused Env-scoped handles and a close barrier; registration-site diagnostics remain missing. Shared binding/type metadata and complete network schema negotiation remain proposals.

Timer ownership, manual/smart reference finalization, exact networking integers and strict declarations are implemented with regression sources and updated examples/docs. CodeGen VM ownership, io validation/CRLF, RPC ordering/schema fingerprints and Event.Once reentrancy also have fixes. Runtime changes are **unbuilt and unexecuted**.

Maintainer validation uses only `.\Tools\RunTests.ps1`. Its default Release pipeline orchestrates both backend builds, Timer flags, full automation, connected cooked peers, type checks and packaging, failing on missing evidence. No extra test commands are required. The pipeline and new C++ manifest await actual build/runtime validation; see [testing](Testing.md) for scope.

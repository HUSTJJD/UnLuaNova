# Current Source Audit and Roadmap

Audited 2026-10-24. **Full slua-unreal/FUnLua parity, superior performance and complete test coverage are not established.** No compilation or UE automation was started. Source implementation, test existence, execution success and platform acceptance are distinct evidence levels. See the [capability matrix](../feature-matrix.md).

## Baselines

slua-unreal: `7970ab72fd59118e1b3f86aac2d016828b54b748`; FUnLua: `7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3`; upstream Luau 0.744: `32c28089305f488f901aaf1dbf3fd1cb65fa3d11`; integrated Luau: `6d36d483cb88973276af611198169f76f3ce05b0`. No dependency upgrade was made. Public VM headers match between the Luau pins, but 18 VM implementation files differ. UnLua was checked as the inherited source, not exhaustively re-audited.

## Findings and Acceptance

| ID / Priority | Current issue | Required acceptance |
|---|---|---|
| NET-01 / P0 | Replication conditions depend on an instance owner although UE layouts are class-cached. Typed buckets cannot express independent field conditions | Stable class schema; multiple actors, owner/nonowner, mixed conditions, late join, dormancy; legacy/Iris/push model separately |
| NET-02 / P0 | int64 passes through Luau int32 API; unknown conditions fall back to COND_None | Exact numeric contract, reject invalid declarations; boundary and schema mismatch cases |
| LIFE-01 / P0 | Timer stores a raw coroutine state without keepalive or Env teardown; selects first game world | Explicit Env/World ownership; coroutine GC, both teardown orders, multi-PIE, cancellation/reentrancy |
| LIFE-02 / P0 | Container/FText/exported constructors use VM destructors, but AddManualRef still relies on __gc | Audit all allocations and borrowed/manual/smart references; force Lua and UE GC; no Lua stack work in VM destructors |
| COMPAT-01 / P1 | Upvalue identity, registry/uservalue and load mode are approximate or placeholders | Explicit supported subset; shared scalar closure aliases, reload rollback, source/bytecode modes |
| BIND-01 / P1 | UClass-keyed binding is not independent per-instance module binding | Same-class A/B modules, inheritance, unbind and hot reload isolation |
| EVENT-01 / P1 | Tokens hold owners strongly; weak-key indexing does not release them automatically | Explicit unsubscribe contract; recursive Once fixed this round, teardown and closure ownership still need execution |
| SAFE-01 / P1 | Read-only globals do not restrict UE/debug/io/socket capabilities; io is not an OS sandbox | Trusted/untrusted policies, path/link/quota/error/timeout checks, debug service exposure policy |
| TOOL-01 / P1 | Coverage export/gates incomplete; CodeGen counts requests; typecheck can skip | Build provenance, success/skip/failure distinction, mandatory checker in acceptance jobs |
| QA-01 / P1 | Old reports cannot certify current edits; some network tests compare unconnected processes | Real server/clients, lifecycle/error cases, packaged/platform runs, feature-based acceptance |
| PERF-01 / P1 | Benchmark thresholds exist but committed performance baselines are absent; thunks still call ProcessEvent | Same-machine controlled comparison and raw distributions |
| LEGAL-01 / release P0 | Root/file license scope unresolved; dependency inventory incomplete; FUnLua license file absent | Rights-holder scope decision and complete notices before distribution |

The [Chinese audit](../CN/Benchmark_Review.md) contains the expanded implementation and acceptance detail. Both use the same IDs.

## Order and Innovation

First fix network/lifetime correctness; then complete feature acceptance, examples and traceable reports; finally run controlled benchmarks, packaged/platform validation and license review. Proposed innovations are generation-aware Env/World callback handles, shared binding/type metadata, deterministic network schema negotiation and a capability manifest. These remain proposals.

This round changes CodeGen VM ownership, io validation/CRLF handling, RPC ordering/schema fingerprints and Event.Once reentrancy, and adds regression source/examples. These runtime changes are **unbuilt and unexecuted**. No claim of all-feature completion follows from this audit.

# slua-unreal and FUnLua Source Comparison

Use the pinned revisions in the [audit](Benchmark_Review.md). Upstream marketing benchmarks are not local measurements.

## slua-unreal

[Audited source](https://github.com/Tencent/sluaunreal/tree/7970ab72fd59118e1b3f86aac2d016828b54b748/Plugins/slua_unreal/Source)

Read LuaObject/LuaVar/LuaState, delegates, overrides, profiler and CppBinding together with the export tools. UnLuaNova inherits reflection, bidirectional calls, overrides, out/ref, containers and delegates from UnLua. It also has template exports; claiming no CppBinding-like path would be wrong. Its generated reflected thunks are not arbitrary C++ libclang exports and still call ProcessEvent.

Multi-state, hot reload, actor/component/widget binding, RPC, profiling, watchdog and remote debugging must be assessed as workflows. Current gaps include per-instance modules, callback ownership, network conditions, packaged remote debugging and performance evidence. Budgeted incremental GC is not equivalent to slua's multithreaded GC.

## FUnLua

[Audited source](https://github.com/fcscript/FUnLua/tree/7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3/Plugins/FCScript/Source/FCScript)

| Entry | Source-backed lesson | Local requirement |
|---|---|---|
| FCGetObj.h | Typed FCObjRef IDs and parent/child ownership | Explicit owned/borrowed references and invalidation tests |
| FCSafeProperty.h | Weak property metadata | Blueprint recompile/reinstance invalidation |
| FCObjectManager.h/.cpp | Per-object binding and (Object, Function) identity | Independent modules on same-class instances |
| FCDynamicDelegateManager.h | Object/table plus function identity | Deduplication and owner tokens while preserving UE singlecast semantics |
| FCRefObjCache.cpp | Caching and budgeted release | Isolate by Env; do not import global-context assumptions |

The README's performance multipliers are unverified here. Source includes manual FGCObject references, so ownership claims must be qualified by path.

slua-unreal uses BSD-3-Clause. FUnLua's README links LICENSE.TXT but that file was absent from the audited tree. No implementation was copied from either project. See [license status](License.md) and the [capability matrix](../feature-matrix.md).

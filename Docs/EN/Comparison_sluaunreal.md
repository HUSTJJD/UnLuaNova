# slua-unreal and FUnLua Source Comparison

## slua-unreal

[Source tree](https://github.com/Tencent/sluaunreal/tree/7970ab72fd59118e1b3f86aac2d016828b54b748/Plugins/slua_unreal/Source)

| Capability | UnLuaNova status / requirement to absorb |
|---|---|
| Reflection, bidirectional calls, Blueprint override, out/ref, enums and operators | Most paths inherited from UnLua; parameter and exception contracts, inheritance / network overrides need per-scenario checks |
| Template export of plain C++, static codegen, non-reflected extension methods | `UnLuaNovaEx` templates and hand-written exports exist; the generator mainly handles reflected signatures and is not equivalent to arbitrary libclang C++ export |
| Containers, structs, object GC, delegate callbacks | Implemented; borrowed-view invalidation, manual references and multi-Env cleanup are known boundaries |
| Multiple Lua states, hot reload, script-driven Actor/Component/Widget | Entry points exist; per-instance module binding on the same class and multi-world Timer isolation are known boundaries |
| RPC / property replication | Promoted to Stable; supports per-condition grouped layout, int64 / scalar / Object bucket round-trips, `UStruct` / container specs and FProperty parameter export, cross-peer declaration fingerprints; real networking acceptance completed on four replication carriers and eight scenarios |
| CPU/memory diagnostics, on-device debugging, dead-loop detection | Profiler, DAP and watchdog exist; on-device remote debugging and permission / reconnect / source mapping are known boundaries |
| Multithreaded GC | Explicit sliced / budgeted scheduling, not equivalent thread GC; measured by frame stalls and thread safety |

Read LuaObject/LuaVar/LuaState, delegates, overrides, profiler and CppBinding together with the export tools. UnLuaNova inherits reflection, bidirectional calls, overrides, out/ref, containers and delegates from UnLua, and also has template exports. Its generated reflected thunks are not arbitrary C++ libclang exports and still call ProcessEvent.

Multi-state, hot reload, actor/component/widget binding, RPC, profiling, watchdog and remote debugging are separate workflows. Per-instance modules and callback ownership are known limitations. Budgeted incremental GC is not equivalent to slua's multithreaded GC.

## FUnLua

[Source tree](https://github.com/fcscript/FUnLua/tree/7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3/Plugins/FCScript/Source/FCScript)

| Entry | Source-backed lesson | Local requirement |
|---|---|---|
| FCGetObj.h | Typed FCObjRef IDs and parent/child ownership | Explicit owned/borrowed references and invalidation tests |
| FCSafeProperty.h | Weak property metadata | Blueprint recompile/reinstance invalidation |
| FCObjectManager.h/.cpp | Per-object binding and (Object, Function) identity | Independent modules on same-class instances |
| FCDynamicDelegateManager.h | Object/table plus function identity | Deduplication and owner tokens while preserving UE singlecast semantics |
| FCRefObjCache.cpp | Caching and budgeted release | Isolate by Env; do not import global-context assumptions |

The FUnLua source includes manual FGCObject references, so ownership handling varies by path.

## Compatibility and licensing

The goal is to cover the workflows and provide migration notes, not byte-for-byte compatibility
of API names or memory layouts. Existing interfaces inherited from the UnLua fork stay
compatible; migrating slua / FUnLua scripts needs separate adaptation.

slua-unreal is BSD-3-Clause - do not misstate it as MIT. FUnLua's README links `LICENSE.TXT`,
but that file was not found in its pinned source tree; this project only borrows design ideas and
copies no implementation. See [license status](License.md) for the full rules.

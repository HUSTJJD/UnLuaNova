# slua-unreal and FUnLua Source Comparison

## slua-unreal

[Source](https://github.com/Tencent/sluaunreal)

Read LuaObject/LuaVar/LuaState, delegates, overrides, profiler and CppBinding together with the export tools. UnLuaNova inherits reflection, bidirectional calls, overrides, out/ref, containers and delegates from UnLua, and also has template exports. Its generated reflected thunks are not arbitrary C++ libclang exports and still call ProcessEvent.

Multi-state, hot reload, actor/component/widget binding, RPC, profiling, watchdog and remote debugging are separate workflows. Per-instance modules and callback ownership are known limitations. Budgeted incremental GC is not equivalent to slua's multithreaded GC.

## FUnLua

[Source](https://github.com/fcscript/FUnLua)

| Entry | Source-backed lesson | Local requirement |
|---|---|---|
| FCGetObj.h | Typed FCObjRef IDs and parent/child ownership | Explicit owned/borrowed references and invalidation tests |
| FCSafeProperty.h | Weak property metadata | Blueprint recompile/reinstance invalidation |
| FCObjectManager.h/.cpp | Per-object binding and (Object, Function) identity | Independent modules on same-class instances |
| FCDynamicDelegateManager.h | Object/table plus function identity | Deduplication and owner tokens while preserving UE singlecast semantics |
| FCRefObjCache.cpp | Caching and budgeted release | Isolate by Env; do not import global-context assumptions |

The FUnLua source includes manual FGCObject references, so ownership handling varies by path.

slua-unreal uses BSD-3-Clause; FUnLua states MIT in its README. No implementation was copied from either project. See [license status](License.md).

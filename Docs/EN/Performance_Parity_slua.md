---
title: "Performance parity with slua: why it is fast, and how we pass it"
description: "Breaking down slua-unreal's seven sources of speed, with the corresponding UnLuaNova implementation and acceptance criteria for each"
---

# Performance parity with slua: why it is fast, and how we pass it

This page answers exactly one question: **what does slua-unreal skip on every
cross-language call, and where is the equivalent place in UnLuaNova - is it skipped there
yet?**

Conclusion first, evidence second:

* slua's performance edge is **not "better C++" - it is "marshalling moved from runtime to
  compile time"**. Of its three binding paths only reflection does type dispatch at
  runtime; CppBinding and static code generation turn "what type is this parameter" into a
  template argument, so marshalling collapses into a few stores. The 0.541 s (reflection)
  vs 0.090 s (CppBinding) in its README comes from that one difference - **same machine,
  same million calls, 6x apart.**
* UnLuaNova used to be worse off than "no CppBinding": **our marshalling layer was thicker
  than slua's even on the reflection path**. slua's 0.54 µs for reflection is roughly the
  bare `ProcessEvent` cost, so its marshalling layer charges almost nothing; our own
  numbers are 0.459 µs for `ProcessEvent` and 1.439 µs to call a UE function from Lua, so
  **the 0.98 µs difference is entirely our marshalling and validation.**
* This round lands everything slua has, plus three places where we are more conservative
  or stronger (see [section four](#four-beyond-slua-three-things-it-cannot-give)). The core
  is giving static binding a **real generated thunk** instead of only a description table -
  exactly the G5 item marked "to do" in the comparison page.

> ⚠️ About the numbers: slua's table is "seconds per 100k calls", measured on Win10 /
> Ryzen 7 4700G / UE 4; ours come from `UnLuaNova.Perf.BenchmarkParity` and
> `UnLuaNova.Perf.Benchmark`. **Two machines, two engine versions - absolute values are not
> comparable.** Every "N times faster" on this page is either **the ratio of one path
> before vs after our own optimisation**, or explicitly flagged as the other project's own
> control data.

---

## One. Seven sources of slua's speed

### 1. Marshalling fixed at compile time (CppBinding / static codegen) - the only order-of-magnitude gap

slua offers three bindings: blueprint reflection, **libclang static codegen**, and
**CppBinding template expansion**. It says of the latter two that they "do not differ in
principle, one is generated and one is hand-written". The difference is entirely at compile
time:

```cpp
// CppBinding marshalling (schematic): the type is a template argument
int32 Arg0 = LuaObject::checkValue<int32>(L, 2);
FVector Arg1 = LuaObject::checkValue<FVector>(L, 3);
```

| Path | 1M empty function calls | Relative to reflection |
|:---|---:|---:|
| Blueprint reflection | 0.541507 s | 1.0x |
| CppBinding | 0.090571 s | **6.0x** |
| FString in, int out (reflection) | 0.930042 s | — |
| FString in, int out (CppBinding) | 0.223207 s | **4.2x** |

**What is skipped**: `FProperty` virtual calls, per-parameter initialisation of the
`UFunction` parameter frame, parameter lookup by name, runtime type dispatch. Only
`ProcessEvent` remains.

### 2. Function index cache - the "10x" of 2.0 over 1.x

slua 2.0's release note: *"function indexing is 10x faster than 1.x"*. That maps to the
`TestIndex*` group in its README table:

| Case | slua | UnLua | Ratio |
|:---|---:|---:|---:|
| `TestIndexStaticCall` | 0.00125 | 0.00535 | 4.28 |
| `TestIndexEmptyCall` | 0.00126 | 0.00559 | 4.44 |
| `TestIndexGetBoolCall` | 0.00142 | 0.00572 | 4.03 |

In essence: write the result of "name -> callable" back into the metatable, then a single
`rawget` retrieves it. UnLuaNova **already has this** (`GetFieldInternal` in `LuaCore.cpp`
does a `lua_rawset` back into the metatable), so this is not a gap - what is worth writing
down is: **1.x did not have it, we were born with the advantage, and there is no reason to
throw it away somewhere else.**

### 3. Reference access to structs

slua 2.0 changed structs from copy to reference:

```lua
-- old code (read-modify-write, two marshalling passes)
local Position = Actor.Position
Position.X = 1
Actor.Position = Position
-- new code
Actor.Position.X = 1
```

UnLuaNova **has the same thing**: `FPropertyDesc` with `bCreateCopy=false` goes through
`PushPointer`, hitting the weak `StructMap` by lightuserdata with zero copy and zero
allocation. Even.

### 4. Cached override dispatch - the 29x, the biggest number in the table

| Case | slua | UnLua | Ratio |
|:---|---:|---:|---:|
| `TestOverrideReceiveBeginPlay` | 0.00135 | 0.03956 | **29.30** |
| `TestOverrideTestFunc` | 0.01313 | 0.09633 | 7.34 |

`ReceiveBeginPlay` is a **parameterless event**. UnLua runs the full `FFrame` parameter
unpack for it (a `Stack.Step` loop plus building the `FOutParmRec` chain), while slua
dispatches an empty signature directly. **This is "parameter frame preparation" taken to an
extreme** - a parameterless call should cost about one `lua_pcall`, and 29x says UnLua pays
the parametered price when there are no parameters.

### 5. The reflection path itself is thinner

`TestEmptyCall` slua 0.0183 / UnLua 0.0335 (1.83x), `TestGetIntCall` 0.02085 / 0.04239
(2.03x). Same reflection path, 2x apart. Against slua's 0.54 µs reflection ≈ bare
`ProcessEvent`, the conclusion is: **its reflection path has no "extra validation done on
every call"**, whereas the UnLua family does (`CheckObject`, `CheckPropertyType`,
`GetFunctionCallspace`).

### 6. Property access does not re-validate

`TestGetIntVar` 0.01396 / 0.01553, `TestGetVectorVar` 0.01109 / 0.02358 (2.13x),
`TestGetStructVar` 0.01085 / 0.02408 (2.22x). Note that **Get is much slower than Set**
(GetVector 2.13x vs SetVector 0.91x) - the gap is concentrated in the "read out" step's
type conversion and allocation, not in "writing in".

### 7. Multithreaded GC (affects frame rate, not single-call cost)

slua claims multithreaded GC support. The comparison page already argues that **our premise
does not hold** (`FLuaEnv::GC()` has exactly one caller in the whole repo, Lua 5.5 runs
generational and Luau runs incremental, so there is no periodic spike to amortise in the
first place). Not adopted.

---

## Two. Where we are slow: the per-item cost list

Every item below comes from actual code in this repo, with a location. This is what the
work was based on.

| # | Cost | Location | Happens |
|:--|:---|:---|:--|
| 1 | `FPropertyDesc` two virtual calls: `ReadValue_InContainer` -> `GetValueInternal` | `ReflectionUtils/PropertyDesc.h:149` | every property read/write |
| 2 | `TWeakFieldPtr::IsValid()` - weak handle resolution, not a null check | `PropertyDesc.h:151,189` | every property read/write (**twice**) |
| 3 | `LowLevel::CheckPropertyOwner` -> `UObject::IsA()` walking the class chain | `Public/LowLevel.cpp` (`ENABLE_TYPE_CHECK=1` by default) | every property read/write |
| 4 | `FFunctionDesc::CheckObject` -> `IsA` + `ULuaFunction::Get` | `ReflectionUtils/FunctionDesc.cpp:677` | every function call (WITH_EDITOR) |
| 5 | `PreCall` per-parameter `InitializeValue` + `CheckPropertyType` virtual call | `FunctionDesc.cpp:387,415` | every call x parameter count |
| 6 | `Object->GetFunctionCallspace()` | `FunctionDesc.cpp:228` | every call (when not statically bound) |
| 7 | `FClassRegistry::Find(const char*)` constructing an `FName` (UTF8->TCHAR + hash) | `Registries/ClassRegistry.cpp:50` | every UObject push |
| 8 | `PushObjectCore` rebuilding the metatable name: `FString::Printf` or `GetPathName()` | `Private/LuaCore.cpp:467` | every object-typed property read |
| 9 | Object-typed properties create fresh userdata every time (`PushObjectCore` bypasses the `ObjectMap` cache) | `LuaCore.cpp:551` | every object-typed property read |
| 10 | `FStaticPropBinding` / `FStaticFuncBinding` are generated, and **the property side is never consumed at all** | the only consumer of StaticBinding in the repo is `FunctionDesc.cpp` | permanent |

Item 10 is the most absurd: `Offset` and `EStaticPropKind` have been generated for ages and
nothing reads them at runtime.

---

## Three. What was landed

### S1 - static binding generates a thunk (slua's CppBinding equivalent, the core of G5)

**Approach**: `FStaticFuncBinding` gains a `lua_CFunction Thunk` field. The Commandlet
generates template instantiations for functions whose signature can be marshalled
statically, the metatable hangs that C function directly, and **the whole
`FFunctionDesc` / `FPropertyDesc` chain is never entered**.

```cpp
// generated output (schematic)
{ TEXT("K2_SetActorLocation"), 0, MAX_uint16, NumFStaticParamBinding_Actor_...,
  FStaticFuncFlags(), FStaticParamBinding_..., nullptr,
  EStaticPropKind::Bool, nullptr,
  &UnLuaNova::StaticFuncThunk<EStaticPropKind::Bool, EStaticPropKind::Float, EStaticPropKind::Bool> },
```

The key design: **types are compile-time constants, offsets are read at runtime.**

```cpp
template <EStaticPropKind Ret, EStaticPropKind... Args>
int32 StaticFuncThunk(lua_State* L);
```

slua bakes the offsets into the generated code too, which is a little faster, but **adding
a UPROPERTY without regenerating silently shifts everything**. Our offsets come from
`FStaticPropBinding::Offset` (filled from `FProperty::GetOffset_ForGC()` at registration),
so they can never go stale; the price is one indirect load per parameter - bought with
"stale generated files are still safe".

Three fallbacks, any of which drops back to the original path:

1. **Generation time**: `CollectThunkKinds()` only admits non-static / non-net /
   non-interface / non-latent / no out params / **no C++ default arguments**
   (`CPP_Default_*` metadata) / `ParmsSize <= 256` / at most 8 parameters / every parameter
   kind present in `IsThunkableParamKind()`.

   The default-argument rule deserves its own sentence: the reflection path fills the slot
   with the collected default when a script omits the argument, whereas a thunk receives a
   zeroed frame. Better to leave such functions on the reflection path than to be "a bit
   faster while the meaning of `obj:Foo()` quietly changes".
2. **Registration time**: `FStaticBindingRegistry::Resolve` recomputes the kind from the
   real `FProperty` and nulls the `Thunk` on mismatch (the signature changed); a parameter
   that does not resolve nulls it too (so two unresolved slots cannot both write to offset 0).
3. **Runtime**: if `Func->Thunk` is null, `TryPushStaticBindingField` returns false and
   `FFunctionDesc` takes over as before, still through `CallUEFast`.

**Acceptance** - the Commandlet prints thunk coverage at the end:

```
[StaticBindingGen] thunk coverage: 8123/11044 functions (73.6%)
```

### S2 - property access consumes static binding

New `FStaticPropDesc` (`Private/ReflectionUtils/StaticPropDesc.{h,cpp}`). It **inherits
`FPropertyDesc`**, so `Class_Index` / `Class_NewIndex` and the container paths do not
change by a line, and kinds it does not handle fall back to the base implementation -
silently changing behaviour is impossible.

What is saved is items 1 and 2 above: reads become "add offset + push by type", and
`IsValid()` drops from "switch plus several `IsUObjectValid`" to a single
`PropertyPtr.IsValid()` (a live `FProperty` implies its `PropertyClass` / `Struct` /
`SignatureFunction` is live).

Where it lands: `GetFieldInternal` consults static binding **before**
`ClassDesc->RegisterField()`, and on a hit pushes the value and writes it back into the
metatable cache - so no `FFieldDesc` is even created.

### S3 - removing allocations and repeated validation from hot paths

| Change | Location | Saves |
|:---|:---|:---|
| On a metatable hit, take the `ClassDesc` lightuserdata straight from the metatable | `ClassRegistry.cpp:PushMetatable` | item 7: constructing an `FName` every time |
| Memoise the `IsA` in `CheckPropertyOwner` by (class, property) | `LowLevel.cpp` | item 3: class chain walk |
| Object-typed properties go through `PushUObject` (the `ObjectMap` identity cache) | `LuaCore.cpp:PushObjectElement` / `PushInterfaceElement` | items 8 and 9; also fixes "reading the same object twice yields two different Lua values" |

The memo is `thread_local`, not `static`: the result of `IsA` is a pure function of
(object class, property), but under multiple envs a torn read of a `static` could report an
unvalidated combination as true.

`PushObjectElement` keeps the old behaviour for **already-freed** objects (still
`PushObjectCore`), because that is an error path and should not be swapped out in passing.

### S4 - not landed yet

* `FFunctionDesc::CheckObject`'s editor-only `IsA` + `ULuaFunction::Get` (item 4): it can be
  memoised the same way, but it lives in a `WITH_EDITOR` branch, so a benchmark run in the
  editor would amplify it - confirm the measurement environment first.
* `PreCall` batch-writing by kind (item 5): static binding already has
  `FStaticFuncBinding::Params`, so a `CallUEStatic` is possible, but **the types that have a
  thunk no longer take this path**, leaving only "projects that never ran the Commandlet" as
  the benefit surface.
* Direct dispatch for empty override signatures (slua's 29x): short-circuit
  `bUnpackParams` on `NumParms == 0` in `FunctionDesc::CallLua`.

---

## Four. Beyond slua: three things it cannot give

Parity is not the point. These three are ours alone:

1. **Thunks cover properties, not just functions.** slua's CppBinding is hand-registered
   functions, and no real project hand-writes a few thousand UPROPERTYs - so its speedup
   naturally covers only "the hot functions somebody picked". Our thunks are generated in
   bulk by the Commandlet from reflection data, **properties and functions covered in the
   same batch**, and it is opt-out (`meta=(UnLuaNovaNoExport)`) with everything on by
   default.
2. **Stale generated output is safe.** As above: offsets read at runtime, kinds recomputed
   at registration, mismatch falls back. slua's static codegen is forward libclang parsing
   with baked offsets, so a header change without regeneration is a misaligned crash. We do
   not maintain a clang version or UE macro expansion (this was already on the "do not
   adopt" list in the comparison page).
3. **Native vector on the Luau backend.** `FPropertyDesc` has a direct-write fast path for
   `FVector` (Luau's `LUA_TVECTOR` lands its three components straight into
   `Dest->X/Y/Z`, with no userdata). slua has one backend, Lua 5.4, and no such layer.

Plus the parts we already led on (see the comparison page): dual language backends, Luau
static type checking, sandboxing, hot reload, 518 automated cases, UE 5.8.

---

## Five. Acceptance criteria

What has to come out of this is not "feels faster", it is these three numbers:

1. **Re-measure on the same machine**: run `UnLuaNova.Perf.BenchmarkParity` (100k
   iterations, case names aligned with slua's `TestPerf`), producing
   `Saved/Benchmark/parity-latest-<Backend>.csv`. The baseline is the bare `ProcessEvent`
   cost from the `Ref.TestEmptyCall` / `Ref.TestGetIntCall` rows of the same run - the goal
   is for **"call a UE function from Lua" to converge towards the bare cost**, not towards
   slua's absolute number.
2. **Thunk coverage**: the Commandlet's `thunk coverage: X/Y`. Below 60% means the
   generator's eligibility test is too strict and is worth loosening (start with the out
   parameter class).
3. **Property side**: `TestGetIntVar` / `TestGetVectorVar` / `TestGetStrVar` before vs
   after. These are exactly the rows where UnLua loses worst in slua's table
   (2.03x / 2.13x / 2.36x), and exactly what this round's changes hit.

**The gain is on by default**: `bStaticBinding` defaults to `True` and the generated output
is maintained automatically on editor startup (see [Static Export and
Binding](StaticExportBinding.md)), so nobody has to remember to run a command. The only
remaining manual step is **one recompile** - the output is C++, which is the inherent cost
of compile-time binding.

---

## Further reading

- [Comparison with slua-unreal](Comparison_sluaunreal.md) - capability matrix and G1~G7
- [Architecture](Architecture.md) - the dual-backend abstraction, override and static binding paths
- [Static Export and Binding](StaticExportBinding.md) - Commandlet usage and opt-out
- [Console Commands](ConsoleCommand.md) - `lua.profiler` / `lua.memstats`
- slua-unreal: <https://github.com/Tencent/sluaunreal>

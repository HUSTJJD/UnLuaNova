---
title: "Comparison with slua-unreal: gaps and adoptions"
description: "Item-by-item comparison of Tencent/sluaunreal's capabilities and implementation paths: what to adopt, how it maps onto UnLuaNova's existing architecture, and what not to copy"
---

# Comparison with slua-unreal: gaps and adoptions

This page answers one question: **what does slua-unreal get right, what is UnLuaNova
missing, and how do we close it.**

Conclusion first:

* **There is exactly one real capability gap** - Lua-declared networking (RPC / replicated
  properties).
* **"Lua-side first-class UObject" (G1) is not a gap** - I misjudged it while copying slua
  too literally. After checking `ULuaFunction::IsOverridable` the conclusion flips: it
  accepts `FUNC_BlueprintEvent`, so **native classes can already be overridden**, and the
  "no blueprint" need is already served three ways in the project: ini `ClassModuleBindings`,
  `IUnLuaNovaInterface`, and `UUnLuaNovaBindingComponent`. Building a set of parallel
  `ALua*` base classes would only lose `self.Overridden.Xxx(self)` and add a second,
  concurrent lifecycle. See [Lua-driven Objects](LuaDrivenObjects.md).
* Also landed in this round: typed calls from C++ into Lua member functions (that one really
  was missing), DAP `evaluate` / `pause` (G7), per-function profiler aggregation (G4), and a
  cross-project performance control suite using the same case names as slua's `TestPerf` (P0).
* The GC item (G3) **was over-engineered on a false premise** - there was no periodic spike
  to fix. See G3 below.
* **One suspicious performance signal** - in our own benchmark "call a UE function from Lua"
  is 1.44 µs, while slua's **blueprint reflection** path is 0.54 µs (comparable machines,
  both reflection), so our marshalling layer spends about 1 µs on top of `ProcessEvent`
  itself. This needs a same-machine re-measurement, but it points at one clear direction:
  static binding should generate a **thunk**, not only a **description table**.
* **Three things slua does should not be copied** - adding UFunctions to a UClass at runtime,
  a home-grown profiler front end, and forward libclang parsing. We have a better foundation
  for each.

> This page asks "what does the competition have that we lack". **The second round of
> comparison changed direction** and asked "how good is what we already claim to have" - and
> the answer was that the shortfall is no longer feature richness but behaviour under default
> configuration and engineering discipline (userdata never collected on Luau, performance
> numbers with no artifacts, no CI running tests). See [Benchmark Review: Current Health and
> Next Steps](Benchmark_Review.md).

---

## One. The shape of slua-unreal

### Three bindings side by side

| | Mechanism | slua's approach | UnLuaNova's counterpart |
|:---|:---|:---|:---|
| 1 | Blueprint reflection | `LuaObject.cpp` over UE reflection | same (`ReflectionUtils/`) |
| 2 | Static codegen | libclang parses C++ headers and emits glue | `UnLuaNovaStaticBindingGeneratorCommandlet` (reflection-based, not libclang) |
| 3 | CppBinding | `LuaCppBinding.h` template expansion, marshalling fixed at compile time | **no counterpart** (this is the key gap, see G5) |

slua's README gives a 1M-call comparison: reflection 0.54 s, CppBinding 0.09 s - **template
expansion is an order of magnitude faster**. Our current fastest path (`CallUEFast`) is still
"reflection plus caching", not "fixed at compile time".

### The core of 2.0: Lua classes become first-class UObjects

This is the biggest split between slua and the UnLua family. Under
`Plugins/slua_unreal/Source/slua_unreal/Public/` there is a set of preset base classes:

```
LuaActor.h  LuaPawn.h  LuaCharacter.h  LuaPlayerController.h
LuaGameMode.h  LuaGameState.h  LuaPlayerState.h  LuaUserWidget.h
LuaActorComponent.h  LuaLevelScriptActor.h
```

`ALuaActor`'s declaration (verified against source):

```cpp
UCLASS(BlueprintType, Blueprintable)
class SLUA_UNREAL_API ALuaActor : public AActor, public ILuaOverriderInterface
{
    GENERATED_UCLASS_BODY()
    virtual FString GetLuaFilePath_Implementation() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated) FLuaNetSerialization LuaNetSerialization;
    ...
};
```

`LuaOverriderInterface.h` supplies the typed entry point for C++ calling into Lua:

```cpp
template<typename RET, typename... ARGS>
RET CallLuaFunction(const FString& FunctionName, ARGS&& ...Args);
// plus four variants: CallLuaFunctionIfExist / WithContext / IfExistWithContext
// internally GetSelfTable() fetches the Lua table, TMap<FString, LuaVar> FuncMap caches functions
```

**What that means:** in slua a Lua class is not attached to any blueprint - it is a genuine
`ALuaActor` subclass with RPCs, replicated properties, and the ability to be called from C++
through a strongly typed signature.

UnLuaNova cannot do that today - see G1 / G2.

### Networking: enhancing the UClass at runtime

`LuaNet.h` (verified) exposes:

```cpp
static void addLuaRPCType(const FString& rpcType, EFunctionFlags netFlag);
static ClassLuaReplicated* addClassReplicatedProps(lua_State* L, UObject* obj, const LuaVar& luaModule);
```

Together with the custom `NetDeltaSerialize` in `LuaNetSerialization.h/cpp`, this flattens
the fields declared in the Lua table and replicates them incrementally. The Lua side (README):

```lua
LuaActor.ServerRPC.TestServerRPC = { Reliable = true, Params = { EPropertyClass.Int, ... } }
function LuaActor:GetLifetimeReplicatedProps()
    return { { "HP", ELifetimeCondition.COND_OwnerOnly, EPropertyClass.Float }, ... }
end
```

---

## Two. Capability matrix

Legend: ✅ have it ｜ ⚠️ partial ｜ ❌ missing

| Dimension | slua-unreal | UnLuaNova | Verdict |
|:---|:---|:---|:---|
| Blueprint reflection binding | ✅ | ✅ | even |
| Static codegen | ✅ libclang | ✅ Commandlet + opt-out | even (different route) |
| CppBinding / compile-time thunk | ✅ | ❌ | **behind** (G5) |
| Lua class as first-class UObject | ✅ `ILuaOverriderInterface` + 9 base classes | ✅ **already there**: ini `ClassModuleBindings` / `IUnLuaNovaInterface` / `UnLuaNovaBindingComponent` + native classes are overridable | **nothing to add** (see G1) |
| Typed C++ call into Lua | ✅ `CallLuaFunction<RET>` | ✅ `CallLuaFunction<RET>` (added this round, built on the existing `PushFunction`) | **closed** |
| Lua-declared RPC | ✅ | ❌ **`IsOverridable` explicitly excludes `FUNC_Net`** | **behind** (G2) |
| Lua-declared replicated properties | ✅ incl. conditional replication / arrays / structs | ❌ | **behind** (G2) |
| Overriding blueprint events | ✅ | ✅ opcode injection + `self.Overridden` | we are stronger |
| Lua class inheritance / super | ✅ `Class(base)` + `__super` + `Super` | ✅ `Class(super_name)` + `Super` | even |
| Reference access to structs | ✅ | ✅ `PropertyDesc.cpp` `bCreateCopy=false` | even |
| Operator overloading | ✅ | ✅ `MathLib/LuaLib_F*` | even |
| Out params / non-const refs | ✅ | ✅ `IsOutParameter` + `CopyBack` | even |
| Delegates (single / multicast) | ✅ | ✅ `FDelegateRegistry` | even |
| Containers TArray/TMap/TSet | ✅ | ✅ reference wrapper + element cache | even |
| Extension methods | ✅ `LuaExtensionMethod.cpp` | ⚠️ needs C++ (`UnLuaNovaEx.h`) | small gap (G6) |
| Multiple Lua VMs | ✅ | ✅ `FLuaEnv` + `ULuaEnvLocator_ByGameInstance` | even |
| **Multiple Lua language backends** | ❌ Lua 5.4 only | ✅ **Lua 5.5 + Luau** | **we are stronger** |
| Dead loop detection | ✅ | ✅ shared hook dispatcher slot | even |
| CPU profiler | ✅ home-grown + on-device | ✅ writes to Unreal Insights | even (different shape, G4) |
| Memory profiling | ✅ `LuaMemoryProfile` | ✅ `lua.memstats` categorised | even |
| Multithreaded GC | ✅ (claimed in README) | ⚠️ explicit `GC()` can be sliced (default is still VM-managed, see G3) | **nothing to add** (see G3) |
| Debugger | ✅ dedicated VSCode plugin | ⚠️ standard DAP, missing `pause` / `evaluate` | **closed** (G7) |
| Static type checking | ⚠️ exported through the debugger | ✅ Luau `.d.lua` + LuaLS annotations | **we are stronger** |
| Sandbox / read-only globals | ❌ | ✅ Luau-only | **we are stronger** |
| Hot reload | ❌ | ✅ Yunfeng hotfix scheme | **we are stronger** |
| Automated tests | ❌ | ✅ 518 cases on both backends | **we are stronger** |
| Engine version | UE 4.18 / 4.26 / 5.1 | **UE 5.8.3** | **we are stronger** |
| protobuf | ✅ built in | ✅ `UnLuaNovaExtensions/LuaProtobuf` | even |
| Editor C/S simulation | ✅ `LuaSimulate` | ✅ | even |

**Net conclusion:** after verification slua leads in only 2 items (CppBinding / compile-time
thunk, declarative networking) while we lead in 9 (dual backends, type checking, sandbox, hot
reload, tests, engine version, override mechanism, profiler aggregation, binding without
parallel base classes). Only one remaining gap really matters: **the ability to declare
network behaviour**.

One lesson worth recording: G1 and G3 both started as gaps in my notes, and after checking
`LuaFunction.cpp:74` (accepts `FUNC_BlueprintEvent`) and "`GC()` has exactly one caller"
neither holds. **When comparing, translate the other project's motivation into a fact in our
own code before deciding to act** - otherwise you copy somebody else's answer onto a problem
we do not have.

---

## Three. Gap details and landing design

### G1 - Lua-side first-class UObject: **not a gap; this section records why**

This section originally concluded "missing, copy slua and add a set of `ALua*` base
classes". After verification the conclusion flipped; the reasoning is kept because it is
worth more than the conclusion.

**The premise that was falsified**

I claimed "native classes cannot be overridden, so forwarding has to be explicit". In fact:

```cpp
// Private/LuaFunction.cpp:74
return Function->HasAnyFunctionFlags(FUNC_BlueprintEvent)
    || (Function->FunctionFlags & (FUNC_Native|FUNC_Event|FUNC_Net)) == (FUNC_Native|FUNC_Event);
```

It accepts `FUNC_BlueprintEvent`, **regardless of whether the class is a blueprint**.
`AActor::ReceiveBeginPlay` is a `BlueprintImplementableEvent` and is overridable on a plain
C++ subclass just the same (`LuaOverrides.cpp:36` copies it into `ULuaOverridesClass` and
swaps the thunk).

I claimed "we need an interface for the Lua file path". In fact
`IUnLuaNovaInterface::GetModuleName` already does exactly that, down to the same comment
sentence. I had built a near-duplicate second interface.

I claimed "we need a C++ callback after binding". In fact `UUnLuaNovaManager::Bind` already
calls `Initialize` in the module (`UnLuaNovaManager.cpp:97`). I had copied slua's
`PostLuaHook`.

**What copying would cost**

Building parallel `ALua*` base classes has three net losses, so it is not done:

1. **Losing `self.Overridden.Xxx(self)`** - UnLuaNova's signature capability, which explicit
   forwarding bypasses;
2. A second, parallel lifecycle path whose trigger timing and tick order would have to be
   re-aligned with the override system;
3. "No blueprint" is already served by ini `ClassModuleBindings` /
   `IUnLuaNovaInterface` / `UUnLuaNovaBindingComponent` - a fourth route is duplication.

**The half that really was missing**

From C++ you could only call **global** functions (`Call` / `CallTableFunc`); there was no
entry point that "calls a Lua method on an object and returns the value typed". That one
was genuinely missing and is now added:

```cpp
bool bEnabled = UnLuaNova::CallLuaFunction<bool>(MyActor, TEXT("IsTestEnable"), 1, 2);
```

The implementation is deliberately thin: lookup reuses `PushFunction()` - the same function
the override path uses - so it shares the self table, the Super chain and
`ReportLuaCallError` by construction. "Callable from C++" and "callable from Lua" are
therefore equivalent by construction. The first version hand-walked the metatable and
immediately began to drift from `PushFunction`'s semantics; it was changed to reuse.

**Risk** - calling `CallLuaFunction` from an engine callback that is not the game thread
violates the threading constraints of UObject and of Lua.

---

### G2 - Lua-declared networking (RPC + replicated properties)

**Evidence of the current state**

* `Private/ReflectionUtils/FunctionDesc.cpp:43`:
  `if (InFunction->HasAnyFunctionFlags(FUNC_Net)) LuaFunctionName = FString::Printf(TEXT("%s_RPC"), ...)`
  - it only handles Net functions that **already exist** in the engine; writing `Xxx_RPC` on
  the Lua side just renames the override.
* Nothing in the module mentions `GetLifetimeReplicatedProps` / `LifetimeCondition` / `COND_*`.
  Lua **cannot declare** new replicated properties, nor specify a replication condition.

**Landing design (the point: do not copy slua's runtime UClass mutation)**

slua adds UFunctions to a UClass at runtime through `LuaNet::addClassRPC`, paying for a whole
cleanup apparatus (`onObjectDeleted` / `addedRPCClasses` / `removeObjectTable`). We already
have two better foundations:

* `ULuaOverridesClass` - already the container that "carries ULuaFunctions created at
  runtime", and `ULuaFunction::RestoreOverrides` restores automatically on PIE exit.
  **Reuse it directly.**
* `UnLuaNovaStaticBindingGeneratorCommandlet` - generates at build time. **Anything that can
  be done at build time should not be left to runtime.**

In three phases:

| Phase | Scope | Approach |
|:---|:---|:---|
| P1a | RPC: Server / Client / Multicast × Reliable / Unreliable | during `TryBind`, read the `M.ServerRPC / ClientRPC / MulticastRPC` tables -> build a `ULuaFunction` -> set `FUNC_Net｜FUNC_NetServer｜FUNC_NetClient｜FUNC_NetMulticast｜FUNC_NetReliable` -> attach to `ULuaOverridesClass`. Parameter types map from an `EPropertyClass`-style declaration to `FProperty` subclasses |
| P1b | Replicated properties: scalar / FVector / FString / UObject ref | new `FLuaNetSerialization` UStruct (`TStructOpsTypeTraits` with `WithNetDeltaSerializer`); Lua's `GetLifetimeReplicatedProps()` returns the declaration table, which is flattened into an index table at bind time and serialized incrementally by index |
| P1c | Conditional replication (`ELifetimeCondition`), `TArray`, nested structs | add `Condition` and a nested `FlatArrayPropInfo` to P1b's flattened table |

The Lua API is deliberately kept **identical to slua's** (`ServerRPC` /
`GetLifetimeReplicatedProps`, same names and shapes) to lower migration cost, and `.d.lua`
generation gets the matching type declarations.

**Acceptance** - `UnLuaNovaTestSuite/Private/Tests/Net/`: 2 clients + 1 server PIE, asserting
that Server RPCs execute on the server, Client RPCs only on the owning client, Multicast on
every end, and that a `COND_OwnerOnly` property keeps its default on a non-owning client.

**Risk** - the largest item in the whole table (P1a alone is a medium feature, P1b/c each add
another multiple). And a dynamically generated UFunction participating in replication must be
**baked at build time into cooked packages**, or RPCs disappear after packaging. That needs a
spike before rolling out broadly.

---

### G3 - GC hitches: **the premise does not hold; this section records why**

My original phrasing was "spread a 40 ms spike every few seconds into fractions of a
millisecond per frame". That is slua's motivation copied over, not a fact about this project:

* **`FLuaEnv::GC()` has exactly one caller in the entire repo: the `lua.gc` console command**
  (a full-text search for `->GC()` produces one hit). Nothing calls it automatically, so no
  periodic spike exists.
* `UNLUANOVA_SETUP_GC` is already running: Lua 5.5 uses `LUA_GCGEN` **generational**, Luau
  uses **incremental** with a step size and a goal. Both already schedule themselves by
  allocation rate.

So "frame-slicing" was overrated by me. It now does exactly one thing: **make the one explicit
`GC()` sliceable**, for projects that want to call GC deliberately during a cutscene or a
loading screen.

**Therefore the default stays `Automatic`, and staying there is recommended.** Switching to
`slice` calls `LUA_GCSTOP`, which on Lua 5.5 means giving up the generational mode that
`UNLUANOVA_SETUP_GC` deliberately enables - trading a tuned collector for a hand-written
stepper. This is spelled out in the `ELuaGcMode` comment, in `ConsoleCommand.md` and in
[Architecture](Architecture.md), so it is not mistaken for "better by default".

**No real multithreaded GC**: the GC state machine of Lua/Luau is tightly bound to
`lua_State`, and UObject reachability has to be decided on the game thread, so real
parallelism would mean retrofitting the VM with a locking GC - uncontrollable maintenance
cost and crash risk. slua claims support, but its source layout shows no separate GC thread
module either.

**Acceptance** - what is needed is "a slice costs far less per frame than a full cycle", not
"the original spike is gone" (there never was one).

---

### G4 - Profiler usability (not capability)

Our data already reaches Unreal Insights, and Insights itself can attach to a device - so
"can it attach to a device" is not the gap. The real gap is **aggregation**: Insights' timing
view is unfriendly to a few hundred Lua functions, while slua's home-grown tool does
bottom-up hotspot aggregation.

Recommendation (low cost):

* add bottom-up (self-time) aggregated export to `lua.profiler`: `lua.profiler dump <file>`
  emitting CSV/JSON;
* add `lua.profiler record <file>` so CI can compare across commits (with `Tools/RunTests.ps1`).

**Do not** build our own visualisation front end - that road has no end and we will not do it
better than Insights.

---

### G5 - Static binding generates a thunk (answering the 1.44 µs)

**Locating the problem**

Our own benchmark (README):

| Path | µs/call |
|---|---:|
| UE reflection `ProcessEvent` | 0.4588 |
| Calling a UE function from Lua | 1.4388 |

**The 0.98 µs difference is entirely our marshalling and lookup layer.** slua's reflection
path at 0.54 µs is roughly bare `ProcessEvent`, so its marshalling layer adds almost nothing;
and its CppBinding is 0.09 µs (marshalling fixed at compile time).

> ⚠️ Two machines, two engine versions, two case sets - **the numbers are not directly
> comparable**. The next step is a same-machine re-measurement: port slua's `TestPerf.lua`
> cases into `UnLuaNova.Perf.Benchmark` and put them in the same table as ours. Until that
> data exists, the item below is handled as "clear optimisation headroom", not as "2.6x slower".

**Landing design**

Today the Commandlet generates a **description table** (`FStaticPropBinding` /
`FStaticFuncBinding` in `StaticBindingTypes.h`), and the read path is still "look up in the
table -> branch on `EStaticPropKind` -> generic marshalling". Change it to **also generate a
thunk**:

```cpp
// generated output (schematic)
static int32 LuaThunk_AActor_K2_SetActorLocation(lua_State* L)
{
    AActor* Self = (AActor*)UnLuaNova::GetUObject(L, 1);
    FVector  Arg0 = UnLuaNova::GetValue<FVector>(L, 2);   // fixed at compile time
    bool     Arg1 = UnLuaNova::GetValue<bool>(L, 3);
    // ... call directly, no FFunctionDesc
}
static const FStaticFuncBinding FuncBinding_AActor[] = {
    UNLUANOVA_STATIC_FUNC_BEGIN("K2_SetActorLocation", &LuaThunk_AActor_K2_SetActorLocation),
};
```

`FStaticFuncBinding` gains a `lua_CFunction Thunk` field:

* non-null -> the metatable hangs that C function directly, skipping the whole reflection path;
* null -> falls back to the existing generic path (out params, variadics, blueprint-implemented
  functions, latent functions).

**Cost** - generated code size and compile time go up, so `UnLuaNovaNoExport` needs a
same-level opt-out (e.g. `meta=(UnLuaNovaNoThunk)`), and the generator has to be able to judge
"can this signature be marshalled statically".

**Acceptance** - on a same-machine re-measurement, "call a UE function from Lua" converges
towards the bare `ProcessEvent` cost; `UnLuaNova.Perf.Benchmark` reports the thunk hit rate
(the share of functions that can be marshalled statically).

---

### G6 - Extension methods (low priority)

slua's `LuaExtensionMethod.cpp` attaches non-blueprint functions to a UClass (the README
example is `UUserWidget::GetWidgetFromName`). We can do the same through `UnLuaNovaEx.h`'s
`ADD_FUNCTION_EX` / `ADD_NAMED_FUNCTION` - **it just requires writing C++**.

If it is worth closing, what is missing is the "without writing C++" route:
`UnLuaNova.AddExtensionMethod(UClass, Name, Fn)`, putting a Lua function into that class's
metatable. The risk is a conflict with the static binding metatable cache ordering
(`GetFieldInternal` **caches into the metatable**, see `Architecture.md`), so registration has
to happen before the first access. Lowest priority.

---

### G7 - Two debugger requests

`Docs/EN/Debugging.md` states plainly that `pause` and `evaluate` are missing. Those two are
exactly what separates usable breakpoint debugging from unusable, and neither is hard:

* **`evaluate`** - when a breakpoint hits, the game thread is blocked spinning inside
  `Continue()`, **which is precisely the only safe moment to evaluate**; queueing the
  evaluation into that spin is enough (`FLuaDebugAdapter` already has a thread).
* **`pause`** - on the Luau backend use `lua_breakpoint` to plant a "stop at the next line"
  trap; on the Lua 5.5 backend degrade to hook count=1 (accept the brief performance hit,
  since pausing is a debugging state anyway).

One more low-cost migration-friendly item: provide `import('Button')` as a global function
that is sugar for `UE.UClass.Load`, so scripts written for slua run with fewer edits.
**Type declarations still come from `.d.lua`** - that is our advantage and it does not yield
to compatibility.

---

## Four. Three things deliberately not adopted

| slua's approach | Why not to copy | What we do |
|:---|:---|:---|
| `LuaNet::addClassRPC` adding UFunctions to a UClass at runtime | it pollutes the UClass and needs a whole cleanup apparatus (`onObjectDeleted` / `addedRPCClasses`); hot reload and cooked packages both need special handling | Commandlet generation at build time + attaching at runtime to the existing `ULuaOverridesClass` (`RestoreOverrides` reverts automatically) |
| Home-grown profiler front end | visualisation is a separate product and we will not beat Insights | keep writing data into Insights; only add aggregated export and recording (G4) |
| Forward libclang parsing of C++ headers | requires maintaining a clang version and UE macro expansion; poor return | use UBT reflection data + a build-time Commandlet (`UnLuaNovaDefaultParamCollector` already does something similar on the C# side) |

---

## Five. Adoption plan

Ordered by "can be delivered independently and verified independently". Every item must
bring test cases into `UnLuaNovaTestSuite` or it does not count as done.

| Phase | Item | Effort | Depends on | Acceptance | Status |
|:---|:---|:---|:---|:---|:---|
| **P0** | Same-machine benchmark re-measurement (aligned with slua `TestPerf` case names and iteration counts) | small | none | `parity-latest-<Backend>.csv` | ✅ `UnLuaNova.Perf.BenchmarkParity` |
| **P0** | DAP: add `evaluate` / `pause` | small | none | can evaluate and pause in VSCode | ✅ |
| **P0** | Explicit `GC()` sliceable + `lua.gc slice/budget/mult/limit/stats` | small | none | a slice costs far less per frame than a full cycle | ✅ `UnLuaNova.API.GcScheduler` (default is still `Automatic`) |
| **P1** | ~~G1 first-class UObject: `ALua*` parallel base classes~~ | — | — | — | ❌ **not done**: premise falsified, see G1 |
| **P1** | Typed C++ call into Lua: `CallLuaFunction<RET>` | medium | none | call per object and get the typed return value back | ✅ `UnLuaNova.API.Overrider` |
| **P2** | G2a Lua-declared RPC | large | none | the three RPC kinds behave correctly in C/S PIE | ✅ landed, with an automatic C/S test `UnLuaNova.API.Net.ClientServer` (listen server + 1 client PIE, asserting each RPC kind executes on the right end). **Still uncovered: cross-process `RPCId` agreement** - in a single process both ends share one UClass, so it proves nothing |
| **P2** | G2b/c replicated properties + conditional replication | large | G2a | `COND_OwnerOnly` and friends take effect | ⚠️ landed; the payload moved from "one string blob" to **bucket-by-type + push model**, and conditions are **per type** rather than per field. See [Lua-declared Networking](LuaNetworking.md) |
| **P2** | G5 static binding generates a thunk | medium | P0 benchmark | thunk hit rate + call cost converges | ⬜ to do (re-measure first) |
| **P3** | G4 profiler aggregated export and recording | small | none | `lua.profiler dump` emits CSV | ✅ |
| **P3** | G6 extension methods / `import()` sugar | small | none | one case each | ⬜ to do |

**The prerequisite for G2 was already there**: RPC functions can go through `ULuaFunction`
(it is already a `UFunction` subclass and already has `execScriptCallLua`), attached to the
existing `ULuaOverridesClass` and reverted by `RestoreOverrides` on PIE exit - no need to add
functions to a UClass at runtime and then write cleanup logic, as slua does. The only thing
to get past is that one `!FUNC_Net` in `IsOverridable`.

G2a / G2b are implemented; see [Lua-declared Networking](LuaNetworking.md).

The spike I had planned - "can a build-time generated RPC UFunction keep its Net flag in a
cooked package" - **is no longer needed**: once thought through, it asked the wrong question.
We do not serialize the function into the package; we build it at runtime on every startup,
which is the same thing blueprint class loading does when it creates UFunctions, and has
nothing to do with cooking.

Exactly one unknown remains: **whether the two ends agree on `RPCId`**. If the engine already
assigned one, use the engine's; if it is still 0, `EnsureRpcId()` takes "largest existing id on
the class + 1" - both ends running the same module and adding in the same order should land on
the same id in theory. That has to be confirmed in C/S PIE, and it is the only thing that could
overturn the whole design.

---

## Six. Neither side has it - where we can move first

Comparison is not about catching up; it is about finding open ground. slua does not have these
either:

* **Hot-patch packages and version management** - we have `HotReload.lua` (the Yunfeng scheme),
  but not the "patch set -> version -> verification -> rollback" layer.
* **Attributing Lua memory by source** - `lua.memstats` has categorised statistics but still
  cannot answer "which function of which module allocated this", and that is the answer you
  need against a leak.
* **Coroutine leak detection** - latent functions create coroutines
  (`Class_CallLatentFunction`), and there is no way to detect a coroutine left hanging.
* **Multi-env debugging** - `ULuaEnvLocator_ByGameInstance` can produce several envs, but
  `lua.dbg` / `lua.dap` only work on one of them.

---

## Further reading

- [Architecture](Architecture.md) - directory layout, the dual-backend abstraction, override and static binding paths
- [C++ API Reference](CxxApi.md) - `FLuaEnv`, export macros, the static binding registry, override types
- [Why UnLuaNova](WhyUnLuaNova.md) - the trade-offs behind every addition over UnLua
- [Console Commands](ConsoleCommand.md) - `lua.profiler` / `lua.memstats` / `lua.dbg` / `lua.dap`
- [Testing and Automation](Testing.md) - case conventions (a new capability must bring cases)
- slua-unreal: <https://github.com/Tencent/sluaunreal>

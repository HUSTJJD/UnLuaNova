---
title: "Architecture"
description: "UnLuaNova's directory layout, dual-backend abstraction, binding and overriding, the static binding chain, and how to add a type or a test"
---

# Architecture

This page is for people **changing UnLuaNova itself**: after it you should know how the code is
split, how one Lua call reaches the engine, and what a new type has to touch. If you only want to
write scripts against UnLuaNova, the [Programming Guide](UnLuaNova_Programming_Guide.md) is the
better read.

---

## Layout

```
Plugins/UnLuaNova/
├── Source/
│   ├── UnLuaNova/              runtime module (ships with the game)
│   │   ├── Public/             27 headers plus StaticBinding/
│   │   ├── Private/
│   │   │   ├── ReflectionUtils/  UE reflection wrappers: ClassDesc / FunctionDesc / PropertyDesc / EnumDesc …
│   │   │   ├── Registries/       one registry per kind: Class / Object / Function / Property / Enum / Delegate / Container
│   │   │   ├── BaseLib/          13 LuaLib_*: Array/Set/Map/Class/Object/Enum/Delegate/DataTable/Text/Key/File/World…
│   │   │   ├── MathLib/          10 LuaLib_F* (FVector/FQuat/FTransform…) plus the shared LuaLib_Math.h
│   │   │   ├── Containers/       container headers (LuaArray/LuaMap/LuaSet)
│   │   │   └── StaticBinding/    the static binding registry implementation
│   │   └── UnLuaNova.Build.cs  backend selection lives here (reads LuaBackend from Config/DefaultUnLuaNovaEditor.ini)
│   ├── UnLuaNovaEditor/        editor module: toolbars, IntelliSense, 3 commandlets, settings
│   ├── UnLuaNovaDefaultParamCollector/  C# UBT plugin (not a module) collecting UFunction default parameters
│   └── ThirdParty/             the Lua and Luau git submodules plus each one's Build.cs and generator
├── UnLuaNovaExtensions/        LuaProtobuf / LuaRapidjson / LuaSocket, three independent plugins (optional)
└── UnLuaNovaTestSuite/         tests: Private/Specs/ (unit) + Private/Tests/ (regression, scenario)
```

Both backends are **compiled from source by UBT** - there is no prebuilt step. Lua goes through one
aggregated `LuaUnity.c`; Luau goes through `LuauLib/Generated/*.cpp`, where each wrapper includes
exactly one Luau source file (three file-level `static` symbols collide, so `bUseUnity = false` is
required).

---

## The dual-backend abstraction

`Public/LuaBackend.h` is **the only** place that absorbs VM differences. Its own comment is blunt
about it:

> everything else must go through the `UNLUANOVA_*` macros instead of calling the VM directly, and
> **only this file** includes a backend's headers - no other file should include `lua.h`,
> `lualib.h`, `lauxlib.h` or `luacode.h`, because both backends own those names.

**Do not write `if (UNLUANOVA_LUA_BACKEND == ...)` in feature code.** The backend id only decides
*which VM gets compiled*; what actually changes behaviour is capability, so branches belong on the
capability macros.

| Capability macro | Lua | Luau | Meaning |
|---|:---:|:---:|---|
| `UNLUANOVA_HAS_HOOK_EVENT` | yes | no | `lua_Debug.event` exists, letting the profiler pair calls and returns; Luau uses interrupt instead |
| `UNLUANOVA_HAS_NATIVE_VECTOR` | no | yes | three/four floats inline in a TValue, no allocation |
| `UNLUANOVA_HAS_NATIVE_BUFFER` | no | yes | Luau's buffer type |
| `UNLUANOVA_HAS_READONLY_TABLES` | no | yes | read-only tables (the basis of `UE.<Type>` write protection) |
| `UNLUANOVA_HAS_SANDBOX` | no | yes | `luaL_sandbox` / `luaL_sandboxthread` |
| `UNLUANOVA_HAS_BREAKPOINT` | no | yes | breakpoint and single-step callbacks |
| `UNLUANOVA_HAS_ALLOC_CALLBACKS` | no | yes | typed/tagged allocation callbacks (what memory stats uses) |
| `UNLUANOVA_HAS_USERDATA_GC` | yes | no | **Luau has no `__gc` at all**; the dtor is not wired up yet |
| `UNLUANOVA_HAS_ITER_METAMETHOD` | no | yes | Luau uses `__iter` (Lua uses `__pairs`), registered via `UNLUANOVA_ITER_ENTRY` |

The rest are "both have it, but named or signed differently" shims: `UNLUANOVA_NEWSTATE`,
`UNLUANOVA_LOAD`, `UNLUANOVA_NEWDATA`, `UNLUANOVA_UDATA_LEN`, `UNLUANOVA_SETUP_GC`,
`UNLUANOVA_RESUME`, `UNLUANOVA_IS_YIELDABLE`, `UNLUANOVA_PUSHINT64/TOINT64`, and so on.

The header also carries three compile-time guards: both backends enabled → `#error`; the VM actually
compiled in does not match the selected backend → `#error`; wrong headers (`LUA_VECTOR_SIZE`,
`sizeof(lua_Integer)`) → `#error` / `static_assert`. This is the last line of defence against
backend differences leaking outward.

---

## Binding and overriding

### Binding: attaching a Lua module to a UObject / UClass

1. `FLuaEnv::TryBind(UObject*)` (`Private/LuaEnv.cpp`) is the entry point;
2. `UUnLuaNovaManager::Bind()` (`Private/UnLuaNovaManager.cpp`) `require`s the module, then
   `BindClass()` copies the module table per class, records `BindInfo{Class, ModuleName, TableRef}`,
   lists the Lua-side function names via `LowLevel::GetFunctionNames` and the overridable UFunctions
   via `ULuaFunction::GetOverridableFunctions`, and overrides every name that matches;
3. `FObjectRegistry::Bind()` finishes the object-side registration.

When a script reads `self.SomeProperty`, the call goes through `LuaCore.cpp`'s `GetField` →
`GetFieldInternal`, which reads `__name`, registers the `FClassDesc`/`FFieldDesc` and **caches the
result into the metatable**, so the second access never touches reflection again. The other
direction (engine calling into Lua) is `PushFunction` / `CallFunction`.

### Overriding: thunk replacement plus opcode injection

`FLuaOverrides::Override(UFunction*, UClass*, FName)` (`Private/LuaOverrides.cpp`):

1. clears `FUNC_Native` on the source function;
2. **duplicates** the original into a `ULuaOverridesClass` (which carries every `ULuaFunction`
   created at runtime) - which is exactly why `self.Overridden.Xxx(self)` can still reach the
   original implementation;
3. `ULuaFunction::Override()` (`Private/LuaFunction.cpp`) does the critical step: it swaps the
   original's native thunk for `execScriptCallLua` and overwrites `Function->Script` with a
   recognisable header `{EX_StringConst, 'L','U','A','\0', EX_UInt64Const}` followed by the
   `ULuaFunction*` pointer.

That last step is the "opcode injection": `ULuaFunction::Get(UFunction*)` reads those bytes back to
tell whether a UFunction has been overridden from Lua. Both thunks, `execCallLua` and
`execScriptCallLua`, end up in `FFunctionRegistry::Invoke`.

Restore / suspend / resume: `ULuaFunction::RestoreOverrides` (on leaving PIE), `SuspendOverrides`
(before PIE saves a UPackage, so `ULuaFunction`s are not serialised to disk), `ResumeOverrides`
(after the save completes).

### Latent functions

`Class_CallLatentFunction` in `LuaCore.cpp` is the closure installed for UFunctions whose
`FFunctionDesc::IsLatentFunction()` is true. It fetches or creates a coroutine, and then:

- **called on the main thread** → reports a clear error;
- **entered from an engine C callback** → checks `UNLUANOVA_IS_YIELDABLE(L)` first and raises a
  readable error instead of letting Luau emit the rather opaque *attempt to yield across
  metamethod/C-call boundary*;
- normal path → `Function->CallUE(L, NumParams, &ThreadRef)` followed by `lua_yield`.

---

## Static binding: generate → register → `CallUEFast`

| Stage | Where |
|---|---|
| **1. Generate** | `UnLuaNovaEditor/Private/Commandlets/UnLuaNovaStaticBindingGeneratorCommandlet.cpp`. Output defaults to `Source/<Project>/Generated/UnLuaNovaStaticBinding_Generated.cpp`. Selection is **opt-out**: it skips `SKEL_/REINST_/TRASHCLASS_/HOTRELOADED_/PLACEHOLDER-CLASS`, deprecated/transient types, and anything carrying `meta=(UnLuaNovaNoExport)`; `meta=(UnLuaNovaExport)` on an ancestor force-includes it |
| **2. Register** | each generated type carries `UNLUANOVA_REGISTER_STATIC_BINDING(TypeName)`, which declares a static `FStaticBindingAutoRegistrar` whose constructor calls `FStaticBindingRegistry::Get().Register(Type)`. Registration touches no UObject, so **running it at DLL load time is safe** |
| **3. Resolve** | `Register()` only enqueues; `ResolvePending()` resolves the actual `FProperty`/`UFunction` pointers and is triggered **lazily by readers** - so resolution always happens after the UObject system is up. The read path uses an `FRWLock` plus a `PendingCount` atomic fast check, so `FindProp`/`FindFunc` do not re-run resolution every time |
| **4. Consume** | `FFunctionDesc`'s constructor calls `FStaticBindingRegistry::Get().Find(Owner)` then `FindFunc(...)`, caching the result in `FFunctionDesc::StaticBinding` |
| **5. Dispatch** | `FFunctionDesc::CallUE`: if a static binding was found and the function is **not** a network function, not latent and not an interface function → `CallUEFast`; everything else keeps the reflective path |
| **6. Fast path** | `CallUEFast` still reuses `PreCall`/`PostCall`/`Buffer`; it only drops what the pre-resolved descriptor makes unnecessary - no call-space query, and a direct `Object->UObject::ProcessEvent`. Override redirection happens only when `Flags.bOverridable` is set. **Observable behaviour is identical to the reflective path** |

Each of the three exclusions has a reason: network functions need `GetFunctionCallspace` /
`CallRemoteFunction`, latent functions need coroutine bookkeeping, and interface functions need a
per-object `FindFunctionByName`.

---

## Adding a LuaLib type

Copy `Private/MathLib/LuaLib_FIntPoint.cpp`:

1. **Create** `Private/MathLib/LuaLib_FMyType.cpp`, including `"UnLuaNovaEx.h"` and
   `"LuaLib_Math.h"`.
2. **Constructor** `FMyType_New(lua_State*)`: `NewTypedUserdata(L, FMyType)` plus placement new,
   dispatching on `lua_gettop`.
3. **`luaL_Reg` table**: `Set`, `Add/Sub/Mul/Div`, `__add/__sub/__mul/__div`, `__unm`,
   `__tostring` (`TMathUtils<T>::ToString`), `__call` → `_New`.
4. **Export block**: `BEGIN_EXPORT_REFLECTED_CLASS(FMyType)` … `ADD_FUNCTION(...)` …
   `ADD_LIB(FMyTypeLib)` … `END_EXPORT_CLASS()` … `IMPLEMENT_EXPORTED_CLASS(FMyType)`.
   `IMPLEMENT_EXPORTED_CLASS` creates the static helper instance whose constructor registers itself
   with `UnLuaNova::ExportClass` (`Binding.cpp`).
5. **Traits**: add `template<> struct TMathTypeTraits<FMyType>` (`FieldType` / `ScalarType` /
   `NUM_FIELDS`) to `LuaLib_Math.h`, plus a `TMul<FMyType, float>`-style specialization if an
   operator is not uniform.
6. **When it is registered**: reflected classes come from `ClassRegistry.cpp` via
   `FindExportedReflectedClass(...)->Register(L)`, right next to `UELib::SetTableForClass`;
   non-reflected ones from `LuaEnv.cpp`. The metatable is built by
   `TExportedClassBase<bIsReflected>::Register` (`UnLuaNovaEx.inl`).
7. **Building**: nothing to change. `UnLuaNova.Build.cs` lists no source files and UBT globs
   `Private/**`.
8. **Testing**: `UnLuaNovaTestSuite/Private/Specs/LuaLib_FMyType.spec.cpp`, registered as
   `UnLuaNova.API.FMyType`, mirroring `LuaLib_FIntPoint.spec.cpp` (`BeforeEach` does
   `UnLuaNova::Startup(); L = UnLuaNova::GetState();`, then `Describe`/`It` blocks run `RunChunk`).
   Every `MathLib/LuaLib_F*.cpp` has a matching spec.

---

## Further reading

- [Testing and Automation](Testing.md) - how tests are written, run and read
- [C++ API Reference](CxxApi.md) - every public C++ signature
- [How To Implement Overriding](How_To_Implement_Overriding.md) - the override mechanism step by step
- [Static Export and Binding](StaticExportBinding.md) - hand-written exports (the macro system above)
- [Luau Backend](Luau.md) - semantic differences between the two backends
- [Settings](Settings.md) - backend, sandbox, watchdog and other switches

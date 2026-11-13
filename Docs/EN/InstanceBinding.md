---
title: Per-instance Module Binding (BIND-INSTANCE)
description: Bind different Lua modules to individual UObject instances of the same UClass, including semantics, constraints and the blueprint entry points.
---

# Per-instance Module Binding (BIND-INSTANCE)

Different `UObject` instances of the same `UClass` can be bound to different Lua
modules. Class-level binding remains the default: an instance without an explicit
override keeps using the module that `ULuaModuleLocator` resolves for its class.

## API

```cpp
UUnLuaNovaManager* Manager = Env->GetManager();

// Class-level binding must be established first (interface / ini / TryBind),
// otherwise the instance override is rejected:
Env->TryBind(First);

FString Error;
bool bOk = Manager->BindInstance(Second, TEXT("Weapons.Rifle"), LUA_NOREF, &Error);

// Query and revoke
Manager->HasInstanceBinding(Second);
Manager->GetInstanceBoundRef(Second);          // instance module table ref, LUA_NOREF when unbound
Manager->GetInstanceBoundModuleName(Second);
Manager->UnbindInstance(Second);               // metatable falls back to the class default module
```

Blueprint entry points: `BindObjectToModule(Object, ModuleName)` /
`UnbindObjectFromModule(Object)`. Both return whether they succeeded; on failure the
reason is written to `LogUnLuaNova`.

```lua
-- An override module is an ordinary Lua module; functions are named after overridable UFunctions
local M = {}
function M:ReceiveBeginPlay() print("rifle instance") end
function M:Initialize(Initializer) end   -- optional, same contract as class-level binding
return M
```

## Semantics and implementation notes

- `BindInstance` `require`s the module and makes a **shallow copy** as that
  instance's private `REQUIRED_MODULE`. The class module table cannot be shared:
  `FObjectRegistry::Bind` sets the metatable in place, so sharing would mutate the
  class default.
- Engine overrides live on the `UClass`, not on the object. When an instance module
  introduces new overridable function names, those names are merged into that class's
  override set (`ULuaFunction::Override` short-circuits already-overridden functions,
  so repeated calls after a blueprint recompile are idempotent). If a particular
  instance does not define the function, `FFunctionRegistry::Invoke` cannot find a Lua
  function and falls back to the original `UFunction`, so calls never cross instances.
- `FFunctionRegistry`'s `FuncRef` cache is keyed by `ULuaFunction*` (class level).
  Instance-overridden objects instead **resolve against their own `INSTANCE` metatable
  on every call**; the temporary ref is released immediately after the call. Objects
  without an override keep the original cached path, so default behaviour and
  performance are unchanged.
- Already-bound objects swap their metatable in place via
  `FObjectRegistry::RebindModule`, without `Unbind`: userdata and `INSTANCE` tables
  already pushed into Lua stay valid. The new module table inherits the old UObject
  metatable, so the metatable chain stays intact.
- `BindInstance` calls the module's `Initialize` under the same contract as
  class-level binding and can pass an `InitializerTableRef`.
- Cleanup: `NotifyUObjectDeleted` and `Cleanup` `luaL_unref` each instance table to
  avoid leaks when multiple environments are opened and closed repeatedly. An
  instance table lives in the Manager of the `FLuaEnv` that created it (bind to the
  same environment that the dispatch thunk uses).

## Constraints

- Class-level binding must be established before an override; a `UClass` itself
  cannot be an instance-binding target, and the module name cannot be empty.
- `require` must return a table; instance modules are shallow copies, so module-level
  state is not shared.
- This is opt-in; the default path is unchanged. `BindInstance` does not modify
  `ULuaModuleLocator`'s class-level cache, nor the existing priority of static
  binding over dynamic binding.
- Overrides added to a class are class-level resources; `ULuaFunction::RestoreOverrides`
  removes them too, and re-binding an instance adds them back.

## Further reading

[Console Commands](ConsoleCommand.md) · [Test Architecture and Acceptance](Testing_Architecture.md)

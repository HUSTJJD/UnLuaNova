---
title: "Lua-driven Objects and Calling Lua from C++"
description: "Making a Lua module drive a game object with UnLuaNova's existing binding and override machinery (no parallel base classes), calling Lua member functions from C++ in a typed way, and the gap that really remains: RPCs cannot be overridden"
---

# Lua-driven Objects and Calling Lua from C++

## Conclusion first: no parallel base classes are needed

A natural idea is "follow slua and provide a set of base classes - `ALuaActor` /
`ULuaUserWidget` - so a Lua module can become a game object without depending on a
blueprint". **In this project that is wrong**, and the reason is
`ULuaFunction::IsOverridable`:

```cpp
// Private/LuaFunction.cpp:74
static constexpr uint32 FlagMask   = FUNC_Native | FUNC_Event | FUNC_Net;
static constexpr uint32 FlagResult = FUNC_Native | FUNC_Event;
return Function->HasAnyFunctionFlags(FUNC_BlueprintEvent) || (Function->FunctionFlags & FlagMask) == FlagResult;
```

It accepts `FUNC_BlueprintEvent`, **regardless of whether the class is a blueprint**.
`AActor::ReceiveBeginPlay` is a `BlueprintImplementableEvent`, so it is overridable even on a
plain C++ subclass - the override system copies the UFunction into `ULuaOverridesClass` and
swaps the thunk (`LuaOverrides.cpp:36`).

That is: **native classes have always been overridable from Lua.** Adding a set of base
classes to "forward" the lifecycle explicitly brings only three losses:

1. **Losing `self.Overridden.Xxx(self)`** - UnLuaNova's signature capability, which explicit
   forwarding bypasses;
2. **A second, parallel lifecycle path**, which would have to be re-aligned with the override
   system on trigger timing and tick order;
3. **Module location already has three routes** (table below); a fourth is pure duplication.

## Three existing ways to let a Lua module drive an object

| Method | Granularity | When to use it |
|:---|:---|:---|
| `IUnLuaNovaInterface::GetModuleName()` | class (reads the CDO) | declared by the blueprint / C++ class itself; the most common usage |
| ini `ClassModuleBindings` | class | **touches no code at all** - one config line binds an existing class, including a native one, to a module |
| `UUnLuaNovaBindingComponent` | instance | different instances of the same class pointing at different modules |

```lua
-- Content/Script/Gameplay/Pickup.lua
local M = UnLuaNova.Class()

function M:Initialize()          -- called by UUnLuaNovaManager::Bind once binding completes
    self.Score = 10
end

function M:ReceiveBeginPlay()    -- ordinary override, works on native classes too
    print("spawned")
end

function M:ReceiveTick(DeltaSeconds)
    self:AddActorLocalRotation(UE.FRotator(0, 90 * DeltaSeconds, 0))
end

return M
```

One line in `/Script/UnLuaNova.UnLuaNovaSettings` is enough - no blueprint, no C++, and no new
base class:

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+ClassModuleBindings=(Class="/Game/Blueprints/BP_Pickup.BP_Pickup_C",Module="Gameplay.Pickup")
```

**The post-binding hook already exists**: `UUnLuaNovaManager::Bind` calls `Initialize` in the
module (`UnLuaNovaManager.cpp:97`), so no extra C++ callback interface is needed.

## Calling Lua member functions from C++

This half was what was actually missing. The engine side used to be able to call only
**global** functions (`UnLuaNova::Call` / `CallTableFunc`); there was no entry point for
"call this Lua method on this object and get the result back as a bool".

```cpp
#include "UnLuaNovaOverrider.h"

bool   bEnabled = UnLuaNova::CallLuaFunction<bool>(MyActor, TEXT("IsTestEnable"), 1, 2);
double Total    = UnLuaNova::CallLuaFunction<double>(MyActor, TEXT("Sum"), 1.5, 2.5);

UnLuaNova::CallLuaFunctionVoid(MyActor, TEXT("Reset"));
UnLuaNova::CallLuaFunctionIfExist<void>(MyActor, TEXT("OnPickedUp"), Picker);
```

Points:

* `self` is the **first parameter**, with the rest following it (standard Lua method call
  semantics).
* The return value is the first result, converted through `FLuaValue::Value<Ret>()` by the
  declared type.
* An unbound object is `TryHookLua`'d first. `NotifyUObjectCreated` does try every object
  once, but it runs **before** instance properties are copied from the prototype, so a module
  located through instance properties is not readable yet at that point.
* The env is taken per object (`ULuaEnvLocator::Locate`), **not** the main env - under
  `ByGameInstance`, getting this wrong hands you another world's self table.
* The entry point installs `FDeadLoopCheck` and `FDanglingCheck` guards, same as
  `FLuaEnv::DoString`.

**The implementation is deliberately thin**: lookup reuses `PushFunction()` - the same
function the override path uses - so it shares one self table, Super chain and
`ReportLuaCallError` by construction. "Callable from C++" and "callable from Lua" are
therefore equivalent by construction and cannot drift apart. An earlier version hand-walked
the metatable and immediately began to diverge from `PushFunction`'s semantics.

Underlying two-step entry (for when you want to take over argument marshalling yourself):

```cpp
UnLuaNova::FLuaMemberCall Call = UnLuaNova::BeginLuaMemberCall(Obj, TEXT("EchoSelf"), false);
if (Call.IsValid())
{
    // stack is [message handler][function][self]; PushArgs goes on top
    auto Values = UnLuaNova::EndLuaMemberCall(Call, /*NumArgs=*/1);   // NumArgs includes self
}
```

## The gap that really remains: RPCs cannot be overridden

Look at the second branch of `IsOverridable` again:

```cpp
(Function->FunctionFlags & (FUNC_Native | FUNC_Event | FUNC_Net)) == (FUNC_Native | FUNC_Event)
```

**`FUNC_Net` is explicitly excluded.** That is not a judgement copied from slua; it is a fact
in our own code:

* `FunctionDesc.cpp:43` only renames Net functions that already exist to `Xxx_RPC`;
* the override system never touches a function carrying `FUNC_Net`.

The consequence: **Lua can only override RPCs the engine already declared; it cannot declare a
new RPC, nor declare replicated properties.** That is the thing worth doing, and it matters far
more than "adding ten more base classes" - because it decides whether Lua can express network
behaviour on its own.

Direction (not implemented when this page was written): RPC functions go through
`ULuaFunction` (already a `UFunction` subclass, already has `execScriptCallLua`), attached to
the existing `ULuaOverridesClass` and reverted by `RestoreOverrides` on PIE exit - **no need** to
add functions to a UClass at runtime and then write a cleanup apparatus, as slua does. One
unknown to verify before starting: whether a build-time generated RPC UFunction keeps its Net
flag in a cooked package.

> This gap has since been closed - see [Lua-declared Networking](LuaNetworking.md). The
> cross-process `RPCId` question raised above is covered by
> `UnLuaNova.API.Net.DedicatedServer`.

---

## Further reading

- [How To Implement Overriding](How_To_Implement_Overriding.md) - the override system and `self.Overridden`
- [C++ API Reference](CxxApi.md) - `FLuaEnv`, override types, `UnLuaNovaOverrider`
- [Architecture](Architecture.md) - the full binding, override and static binding path
- [Comparison with slua-unreal](Comparison_sluaunreal.md) - how the conclusions above were reached

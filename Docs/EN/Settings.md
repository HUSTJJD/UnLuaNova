---
title: "Settings"
description: "Every runtime and editor setting, including the Lua backend switch"
---

# Settings

Open them from **UnLuaNova toolbar -> Settings**, or under **Project Settings -> Plugins**.

## Runtime settings

### Startup module name

A script module executed as soon as the Lua environment starts. `Main` runs `Main.lua`.
Empty means nothing is executed.

### Dead loop check

A timeout in seconds that stops Lua code from hanging the game. `0` disables it.

Note: it is built on the Lua hook API, so it only catches loops running Lua bytecode - a loop
inside C++ still hangs.

### Dangling pointer check

Forbids Lua from caching references to structs and containers; they are invalidated once a
C++-to-Lua call completes.

### Print Lua stack on system error

Dumps the stack of every Lua environment to the log when a crash is caught. On by default.

### Lua environment locator

By default every `UObject` lands in one environment. Choose `ULuaEnvLocator_ByGameInstance`
if you want per-`GameInstance` isolation (for example a UI framework that was not written for
several game instances in one process), or subclass `ULuaEnvLocator` for your own rule.
Isolation is not a sandbox: objects in other environments are still reachable through UE.

### Lua module locator

By default, binding an object to Lua needs `GetModuleName` filled in by hand (or `Alt` to
fill it from the Blueprint path). Subclass `ULuaModuleLocator` to derive it automatically -
for example stripping a `BP_` prefix - so it is always clear which script binds which asset.

### Pre-bind class list

Types such as `UBlueprintFunctionLibrary` and `UAnimNotifyState` survive leaving PIE, so
UnLuaNova never sees them constructed again on the second run and their binding is lost. List
such resident types here and their subclasses are bound right after the environment starts.

## Editor settings

### Hot reload mode

* Automatic: reload as soon as code changes
* Manual: `Alt+L` or the toolbar entry
* Never: disabled

### Generate IntelliSense

Whether to generate hint files for Lua - see [IntelliSense](../CN/IntelliSense.md).

### Auto startup

UnLuaNova starts with the game by default so it can catch every bindable object. Turn it off
if you have your own startup flow and call `UnLuaNova::Startup()` yourself.

*Requires a recompile.*

### Enable UnLuaNova debug code

More verbose UnLuaNova logging. Off by default. *Requires a recompile.*

### Persistent UFunction parameter buffer

Reuses one buffer per `UFunction` instead of allocating on every call. On by default.
*Requires a recompile.*

### Enable type checking

Checks argument types on every Lua-to-UE call. Keep it on while developing, turn it off for
shipping. On by default. *Requires a recompile.*

### Enable calling overridden functions

Lets Lua reach the original implementation through `self.Overridden`. On by default.
*Requires a recompile.*

### Enable FText support

Off by default, in which case `FText` crosses the boundary as a Lua string and loses some
localization data. On passes it as a C++ object instead. *Requires a recompile.*

### Compile the Lua environment as C++

Off by default. Turning it on gives better cross-language exception handling, but you then
have to deal with C-compiled third-party Lua sources yourself. *Requires a recompile.*

### Lua backend

Set in `Config/DefaultUnLuaNovaEditor.ini` (a **recompile** is required - the backend is a
build-time choice):

```ini
[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]
LuaBackend=Luau
```

| Value | Meaning |
|---|---|
| `Lua` | Official Lua (`github.com/lua/lua`), matching upstream UnLua behaviour |
| `Luau` | Luau (`github.com/luau-lang/luau`): gradual typing, native sandbox, native breakpoints |

When the key is absent the default is **`Luau`** (`UnLuaNova.Build.cs` falls back to it).
The checkout's ini may currently say `Lua` because `Tools/RunTests.ps1` restores whatever it
found after switching backends - do not read that as "this project chose Lua". Delete the line
or set it explicitly if you want it pinned.

Both backends are git submodules under `Plugins/UnLuaNova/Source/ThirdParty/`:

```
ThirdParty/
├── LuaLib/            this project: Lua.Build.cs + LuaUnity.c
│   └── Lua/           submodule: github.com/lua/lua
└── LuauLib/           this project: Luau.Build.cs + generated wrappers
    └── Luau/          submodule: github.com/luau-lang/luau
```

After cloning:

```bash
git submodule update --init --recursive
```

*The old "custom Lua version" setting (pointing at a `lua-5.4.x` source tree) is gone.*

### UE4 namespace

Compatibility for older code that reaches the namespace as `UE4` (it is `UE` now).
*Requires a recompile.*

### Return value order / Blueprint path suffix / UTF-8 BOM / argument passing

Legacy compatibility switches for older UnLua projects; each is documented in the UI and
*requires a recompile*.

---

## Further reading

- [Console Commands](ConsoleCommand.md) - runtime debug commands
- [Features](Features.md) - what the two backends differ on

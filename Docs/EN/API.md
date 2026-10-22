---
title: "API Reference"
description: "C++ API, Lua API and build-time macros of UnLuaNova"
---

# UnLuaNova API

## C++ API

### Global Variables
* `UnLuaNova::GLuaSrcRelativePath` Relative path of Lua script source files based on Content directory

* `UnLuaNova::GLuaSrcFullPath` Absolute path of Lua script source files

* `UnLuaNova::FLuaEnv` One isolated Lua environment (multi-env builds have one instance per
  environment); the primary environment is reached via `IUnLuaNovaModule::Get().GetEnv()`

### Global Functions
Namespace of UnLuaNova
* `Call` Call a global function in Lua.

* `CallTableFunc` Call a function in a given global table in Lua.

* `Get` Return a given type value from the Lua stack at the given inde

* `Push` / `PushXXX` Push a data with given type into the Lua stack

* `IsType` Return the data in Lua stack at given index is the given type or not

* `CreateState` Creates a `lua_State` (legacy single-environment entry point; prefer `FLuaEnv`)

* `GetState` Returns the main environment's `lua_State`, same as `FLuaEnv::GetMainState()`

* `Startup` Call to start the UnLuaNova, return true for success start, otherwise, return false

* `Shutdown` Call to stop the UnLuaNova

* `LoadFile` Call to load a Lua file with relative path based on Lua Script path without running it, return true if found the file and loaded successfully, otherwise return false

* `RunFile` Call to load and run a Lua file, return true if loaded and run successfully, otherwise return false

* `LoadChunk` Call to load a Lua chunk without running it, return true if loaded successfully, otherwise return false

* `RunChunk` Call to run a Lua chunk, return true if loaded successfully, otherwise return false

* `ReportLuaCallError` Return Lua error, will excute the error delegate if the error delegate was bounded, otherwise will call `UE_LOG` to output the error.

* `GetStackVariables` Get local variables and upvalues of the runtime stack.

* `GetLuaCallStack` Get the call stack of the running lua_State

### UnLuaNova's global Delegates
FUnLuaNovaDelegates
* `OnLuaStateCreated` Trriged after created a lua_State, you can get the lua_State in this delegate

* `OnLuaContextInitialized` Trriged after `FLuaEnv` has been initialized (**note**: no broadcast
  point exists in the current build, so a bound handler never fires)

* `OnPreLuaContextCleanup` Trriged before starting to clean `FLuaEnv` (**note**: no broadcast
  point exists in the current build, so a bound handler never fires)

* `OnPostLuaContextCleanup` Trriged after `FLuaEnv` has been cleaned (**note**: no broadcast
  point exists in the current build, so a bound handler never fires)

* `OnPreStaticallyExport` Trriged before starting to register the statically exported classes into lua_State

* `OnObjectBinded` Trriged after an UObject has been binned with a lua file

* `OnObjectUnbinded` Trriged after an UObject has been unbinned with a lua file

* `ReportLuaCallError` Trriged after Lua VM occured an error, could register user-defined implementation

* `ConfigureLuaGC` Trriged after lua_State was created, could register user-defined lua gc configuration

* `LoadLuaFile` Trriged while try to loading lua file, could register user-defined loading

### Interface
* `UUnLuaNovaInterface` Core interface for UnLuaNova, used to tell UnLuaNova which UE-Object should be bind with Lua file

### Class
* `FLuaEnv` Wrapper of one UnLuaNova environment (main `lua_State` plus its registry, modules and
  threads); multi-env builds have one instance per environment

* `FLuaIndex` Wrapper of the index of Lua stack

* `FLuaTable` Wrapper of Lua table, could get tables's data by `[]`, and also support call a function

* `FLuaValue` Wrapper of generic Lua value

* `FLuaFunction` Wrapper of Lua function, could be global function or funciton in global table

* `FLuaRetValues` Wrapper of Lua funciton's return value

* `FCheckStack` Helper for checking Lua stack is correct or not

* `FAutoStack` Helper to recover Lua stack automatically

* `FExportedEnum` Interface for exporting enum into lua

## Lua API

### Global Tables
* `UE` Could use `UE.UEClass` to access class in c++ if you open `WITH_UE4_NAMESPACE`, otherwise, could access class in c++ by `_G.UEClass`. Class will be registered into Lua while being used, so don’t worry about the performance problems caused by too much loading at one time.

* `UE.__newindex` The `UE` namespace is **read-only**: engine types are published by the binding
  layer with `lua_rawset`, so a script writing into it (say `UE.FVector = nil`) raises an error
  instead of silently replacing a binding everything else depends on. Registration is the one
  exception — the namespace stays writable until registration is done, and only then gets
  `__newindex` installed.

### Global Functions
* `RegisterEnum` Used to manually register an enum into Lua

* `RegisterClass` Used to manually register a class into Lua

* `GetUProperty` Return the UProperty of given UObject by given name

* `SetUProperty` Set the value of given UProperty in given UObject with given value

* `LoadObject` Loads an UObject, same as `UObject.Load("/Game/Core/Blueprints/AI/BehaviorTree_Enemy.BehaviorTree_Enemy`

* `LoadClass` Loads a UClass, same as `UClass.Load("/Game/Core/Blueprints/AICharacter.AICharacter_C`

* `NewObject` Creates an UObject with given Class and Outer(Optional) and Name(Opthional)

* `UEPrint` Wrapper of `UE_LOG`, internally use `UKismetSystemLibrary::PrintString`

### Functions of Instances
* `Initialize` Assuming that every Lua Table has this function, it will be called when binding with UObject

### Functions for UE Delegate
* `Bind` Binds the given lua callback with the given instance of FScriptDelegate

* `Unbind` Unbinds the lua callback of the given instance of FScriptDelegate

* `Execute` Manually excutes all callbacks of the given instance of FScriptDelegate

### UnLuaNova.lua (Functions in UnLuaNova.lua)
* `Class` Create a table like class in OOP

* `print` Override origin `print` by `UEPrint`

### The UnLuaNova global table (script side)

`UnLuaNova` is both a Lua module name (`require "UnLuaNova"`) and a global table injected at
runtime:

* `UnLuaNova.GetBackend()` returns the Lua backend this build was compiled against: `"Lua"` or
  `"Luau"`.

* `UnLuaNova.HasFeature(Name)` asks whether a capability is available on the current backend. The
  known names:

  | Name | Meaning | Lua 5.x | Luau |
  |---|---|---|---|
  | `sandbox` | native sandbox (`luaL_sandbox`) | no | yes |
  | `breakpoint` | VM level breakpoint / single-step channel | no | yes |
  | `allocCallbacks` | categorised memory callbacks (the breakdown view of `lua.memstats`) | no | yes |
  | `vector` | native `vector` and `FVector` convert directly | no | yes |
  | `buffer` | the `buffer` library | no | yes |

  An unknown name returns `false` rather than raising, so a script written against a newer version
  degrades instead of crashing on an older build.

Branch on capability, not on the backend name: capabilities change between versions of the same
backend, the name does not.

### String extensions

When the environment starts, the functions below are installed onto the global `string` table - no
`require` needed (see `Plugins/UnLuaNova/Content/Script/UnLuaNova/StringEx.lua`):

| Function | Meaning |
|---|---|
| `string.split(s, sep)` | splits on a **literal** separator (not a pattern); `sep` defaults to `,` |
| `string.strip(s)` / `string.trim(s)` | trims leading and trailing whitespace |
| `string.startswith(s, prefix)` / `string.endswith(s, suffix)` | prefix/suffix test; the empty string is always true |
| `string.count(s, sub)` | counts non-overlapping occurrences |

`join` lives only in the module table (`require("UnLuaNova.StringEx").join(sep, list)`); it is not
installed onto `string`.

### Lua modules shipped with the plugin

These modules live under `Plugins/UnLuaNova/Content/Script/UnLuaNova/` and **travel with the
plugin** - they are not part of the sample project. Reach them as `UnLuaNova.<Module>` /
`Lua.<Module>` (`UnLuaNova` is an alias of `Lua`, and `__index` requires on demand), or
`require("UnLuaNova.<Module>")` directly:

| Module | Meaning |
|---|---|
| `StringEx` | string extensions; required automatically at env startup, installs onto `string` |
| `Vector` | `vector` <-> `FVector` interop (`HasVector()` / `Create` / `ToFVector` / `FromFVector`) |
| `Buffer` | `buffer` <-> `TArray<uint8>` conversion (`HasBuffer()` / `FromArray` / `ToArray`) |
| `Input` | input binding |
| `EnhancedInput` | enhanced input binding |
| `HotReload` | hot reload: takes over `require` so already loaded modules can be replaced |

!!! warning "Do not put these modules in the project directory"

    `package.path` is `Content/Script/?.lua;Plugins/UnLuaNova/Content/Script/?.lua` - **the project
    comes first**. A module placed under `Content/Script/UnLuaNova/` loads fine in this repository,
    but any project that installs only the plugin will not find it - and no case here will fail.
    `UnLuaNova.API.RuntimeModules` asserts "the file lives in the plugin directory" precisely to
    stop that from happening again.

## Macros in UnLuaNova module
* `AUTO_UNLUA_STARTUP` Macro for whether to automatically start or not, default is true

* `WITH_UE4_NAMESPACE` Macro for whether to use `UE` to access Unreal Class's in Lua or not , default is true

* `ENABLE_TYPE_CHECK` Macro for whether to check arguments' type is correct or not while calling C++ from Lua, default is true. A supplied argument of the wrong type raises a Lua error (catchable with `pcall`) instead of the call proceeding with a substituted value.

* `UNLUANOVA_ENABLE_DEBUG` Macro for whether to output UnLuaNova's debugging log, default is false.

---

## Further reading

- [Programming Guide](UnLuaNova_Programming_Guide.md) — Binding, two-way calls and garbage collection
- [How To Implement Overriding](How_To_Implement_Overriding.md) — How overriding works
- [FAQ](FAQ.md) — Frequently asked questions

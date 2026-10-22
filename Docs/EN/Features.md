---
title: "Feature Overview"
description: "What UnLuaNova can do, and what it adds on top of UnLua v2.3.6"
---

# Features

## At a glance

* Static and dynamic binding
* Simulating server / client in the editor
* Blueprint template generation and export
* Manual static export
* Optimized struct access
* Accessing C++ types through the `UE` namespace

## Platform support

* Runtime: Windows / Android / iOS / Linux / macOS
* Engine baseline: **Unreal Engine 5.8.3** (source build). Compatibility code from the 4.17–5.x
  era is still present but is not verified.

## Replacing Blueprint implementations with Lua

* Every Blueprint event marked `BlueprintImplementableEvent` or `BlueprintNativeEvent`
* Replication notifies
* Animation notifies
* Input events

## Calling Blueprint from Lua

* Full access to `UCLASS`, `UPROPERTY`, `UFUNCTION`, `USTRUCT`, `UENUM`
* Calling the original Blueprint implementation that was overridden
* Default parameters of any `BlueprintCallable` / `Exec` UFUNCTION
* Calling latent functions from coroutines

## Calling C++ from Lua

* Unicast delegates: bind, unbind, fire
* Multicast delegates: bind, unbind, clear, broadcast
* `UENUM` access, including [custom collision enums](../EN/CollisionEnum.md)
* Member and static variables and functions, global functions

## Calling Lua from C++

* Global functions and functions in global tables

## Other tools

* Custom Lua code generation templates

---

## What UnLuaNova adds on top of UnLua v2.3.6

### Two Lua backends

* **Lua 5.5** and **Luau**, selected at build time (default: Luau) - see
  [Settings](Settings.md#lua-backend)
* Both are git submodules: `git submodule update --init --recursive` after cloning
* Backend differences live in `LuaBackend.h`; features are written against the abstraction
* Which Luau language features work today: [Luau backend](Luau.md)

### Observability

* **CPU profiling**: Lua function events into Unreal Insights, `lua.profiler` at runtime
* **Memory stats**: totals plus a per-category breakdown, `lua.memstats`
* **Debug service**: headless breakpoints, stepping, stack and locals, `lua.dbg`
* **Sandbox**: hardened globals, `lua.sandbox`
* **Dead-loop detection**: shares the VM hook with the profiler instead of overwriting it

See [Console Commands](ConsoleCommand.md) for the exact syntax.

### Static binding

* Type selection is **opt-out**: everything is exported unless marked
  `meta=(UnLuaNovaNoExport)`; `meta=(UnLuaNovaExport)` on a base class pulls a whole
  hierarchy back in
* The read path takes no exclusive lock, so cost does not grow with the number of types
* See [Static Export and Static Binding](../EN/StaticExportBinding.md)

### Script-side type checking

* Generates Luau `.d.lua` declarations for `luau-analyze`, moving a class of UE API misuse
  to compile time
* The Lua backend gets the same idea from the other side: the exported EmmyLua annotations
  are checked by LuaLS. Each backend uses its own analyzer - neither can parse the other's
  language, see [Testing](Testing.md)
* See [IntelliSense](../EN/IntelliSense.md) and [Luau backend](Luau.md)

### String extensions

* Pure-Lua helpers (`split` / `strip` / `startswith` / `endswith` / `count` / `join`),
  installed onto the global `string` table when an environment starts
* `split` and `count` match **literally**, not as patterns - `split("1.2.3", ".")` splits on
  the dot instead of returning nothing
* `join` lives on the module table: `require("UnLuaNova.StringEx").join(sep, list)`

### Containers and call paths

* `TSet` supports `pairs` (yielding `(element, true)`); previously `pairs(Set)` raised on
  both backends and `ToArray()` / `ToTable()` was the only way out
* `CallUEFast` skips `GetFunctionCallspace`, override redirection and interface lookup for
  already-resolved functions
* `LuaScriptCompat` fills in what Lua 5.x has and Luau does not (Luau's base library has no
  `collectgarbage`, only `gcinfo()`), so scripts do not need two code paths

### Docs that stay current

* The screenshots on the docs site are produced by the automation suite
  (`UnLuaNova.Showcase.*`), not by hand; recording a clip and encoding it into the report is on
  by default (`-NoVideo` opts out), and the encoder comes from the `imageio-ffmpeg` pip package -
  no system-wide ffmpeg needed
* See [Testing and Automation](../EN/Testing.md)

---

## Further reading

- [Programming Guide](UnLuaNova_Programming_Guide.md)
- [Settings](Settings.md) - every setting, including the backend switch
- [Console Commands](ConsoleCommand.md) - runtime debug commands

---
title: "Console Commands"
description: "lua.do / lua.profiler / lua.memstats / lua.dbg / lua.sandbox"
---

# Console Commands

Press `~` for the engine console. UnLuaNova registers the commands below; see
`UnLuaNovaConsoleCommands` if you want to add your own.

## Command list

### lua.do \<code\>

Runs a chunk of Lua in the default environment.

```
lua.do print("hello world")
```

### lua.dofile \<module path\>

Runs a Lua module in the default environment.

```
lua.dofile test1.test2
```

### lua.gc

Forces a garbage collection in the default environment.

### lua.profiler \[on\|off\]

Lua function-level CPU profiling, written to **Unreal Insights**. Turning it on lights up
both sides of the call:

* **Lua side**: events for Lua functions
* **UE side**: scopes around `UFunction` entry/exit into Lua (`CallLua` / `CallUE` /
  `CallUEFast`)

so the full `UE function -> Lua function` chain is visible. Without an argument it prints
the current state.

```
lua.profiler on
lua.profiler off
```

*Profiling and dead-loop detection can be on at the same time: both need a VM hook slot and
used to overwrite each other, but they now go through a shared hook dispatcher.*

### lua.memstats \[on\|off\|reset\]

Lua heap statistics.

* `on` / `off`: enable or disable
* `reset`: zero every counter
* no argument: print the report

The report has two parts:

1. **Totals**: current bytes, peak bytes, alloc / realloc / free counts
2. **Breakdown**: top N by memory category, Lua type and userdata tag

```
lua.memstats on
lua.memstats
lua.memstats reset
```

*The breakdown needs Luau's allocation callback (`onallocate`) and is unavailable on the
Lua 5.5 backend, which says so in the report. It measures allocation volume (churn), not live
bytes - `onfree` carries no size. Live and peak are in the totals.*

### lua.dbg \[subcommand\]

Headless debug service. It has no UI of its own: the commands exist so an external front end
(a future DAP adapter) can drive it, and so can you.

| Subcommand | Meaning |
|---|---|
| *(none)* | Status: attached, paused, hit count, breakpoint count |
| `bp <source> <line>` | Toggle a breakpoint on that source:line |
| `step` | Single step |
| `continue` | Continue execution |
| `stack` | Print the captured call stack |
| `locals` | Print the captured locals |
| `clear` | Remove every breakpoint |
| `detach` | Uninstall the debug hook |

```
lua.dbg
lua.dbg bp MyActor 42
lua.dbg stack
```

*No hook is installed until you ask for one, so the steady-state cost is zero. Native
breakpoints and stepping (`lua_breakpoint` / `lua_singlestep`) are Luau-only; on Lua 5.5
they degrade to matching source:line.*

### lua.sandbox (startup CVar)

Hardens the Lua globals table (Luau's `luaL_sandbox`): once enabled the globals are
read-only, so a script cannot accidentally rewrite engine bindings.

It is a **console variable, not a command** - it is read once when each Lua environment is
created, so changing it at runtime does nothing to environments that already exist. Put it in
an ini so it applies at startup:

```ini
; Config/DefaultEngine.ini
[ConsoleVariables]
lua.sandbox=1
```

The CVar itself exists on both backends, so `lua.sandbox` can be read and written anywhere -
a backend that cannot honour it answers "off" rather than "no such command". Whether setting
it actually does anything is a separate question, and scripts ask that one with
`UnLuaNova.HasFeature('sandbox')`.

*Off by default, because it changes the behaviour of existing scripts. It relies on Luau's
native sandbox and is unsupported on the Lua 5.5 backend.*

---

## Further reading

- [Settings](Settings.md) - related settings (backend, watchdog)
- [Luau backend](Luau.md) - sandbox, native breakpoints, categorized allocation

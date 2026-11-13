---
title: "Console Commands"
description: "lua.do / lua.profiler / lua.memstats / lua.memquota / lua.dbg / lua.sandbox"
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

Aggregate and trace subcommands:

| Subcommand | Effect |
|---|---|
| `agg on\|off` | Per-function aggregation (off by default; every call costs an extra stack push/pop) |
| `reset` | Clear the aggregates |
| `top [n]` | Print the n hottest functions by self time (default 20) |
| `dump [file]` | Write the same table as CSV to `Saved/Profiler/<file>` (default `lua-profiler.csv`) |
| `trace on\|off\|reset` | Sampled stack trace in Luau's `profile.out` format (~1 kHz, bounded sample count) |
| `trace dump [file]` | Write `profile.out`; relative paths land in `Saved/Profiling/<file>` |

Self time is measured exactly on the plain Lua backend (call/return pairing). On Luau a
safe point is neither, so that column is attributed sampling, `Hits` counts safe points
rather than calls, and only the ranking should be trusted.

End to end (UE console -> file -> flamegraph):

```
lua.profiler trace reset
lua.profiler trace on
:: exercise the workload ::
lua.profiler trace off
lua.profiler trace dump my-run.out
```

```powershell
python -B Tools/LuauPerfgraph/perfgraph.py --help
```

The wrapper forwards arguments verbatim to the repository's pinned upstream renderer;
query `--help` for that revision's argument names and pass `Saved/Profiling/my-run.out`
as its input. If the pinned Luau submodule is missing it fails with an explicit
`git submodule update --init ...` hint instead of a pretend success.

### lua.maxsafepoints (runtime CVar)

Per-host-call Lua execution budget, counted in VM safe points: Luau's
`lua_Callbacks::interrupt` (loop back-edges / calls) or plain Lua's `LUA_MASKCOUNT`
instruction hook. `0` (default) disables it. Crossing the cap raises a Lua error, and
`FDeadLoopCheck::FGuard::GetSafePointInterruptCount()` makes real interruptions
observable. GC interrupts are neither counted nor interrupted from. It is independent
of the time-based `DeadLoopCheck`. Luau has no per-instruction hook, so this is a
safe-point budget there, not an instruction quota; only plain Lua counts instructions.

### lua.memstats \[on\|off\|reset\]

Lua heap statistics.

* `on` / `off`: enable or disable call counts and categorized allocation volume; live bytes remain tracked
* `reset`: clear interval counts and categorized volume, reset peak to current usage, preserve live bytes
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

### lua.memquota

A per-VM Lua heap allocation budget, in KiB.

```
lua.memquota status        -- per-Env limit / live / rejections / last_request
lua.memquota 8192          -- 8 MiB: updates every live Env and lua.maxmemorykb
lua.memquota off           -- disable (same as 0)
lua.memquota reset         -- clear refusal counters only, limits unchanged
```

No argument means `status`. The startup CVar `lua.maxmemorykb` only **seeds new
environments** (`0` = off by default, because a quota changes existing script behaviour);
use this command to change environments that already exist. The check lives in
`FLuaEnv::DefaultLuaAllocator`: when `LiveBytes + Delta` would exceed the limit the allocator
returns `nullptr` and the VM raises a **recoverable** `not enough memory` error that `pcall`
can catch. Frees are never refused, so `lua_close()` cannot wedge, and a custom `lua_Alloc`
stays outside the quota. The first refusal emits one log line; later ones are counted in
`rejections` rather than spamming the log.

| Subcommand | Effect |
|---|---|
| `<kb>` | Set the limit on all live Envs and sync `lua.maxmemorykb` for new Envs |
| `off` | Disable the quota |
| `status` | Env count, CVar value, and each Env's limit / live bytes / refusals / last request |
| `reset` | Zero the refusal counters |

`lua.gc limit <kb>` only forces a full collection when the heap crosses a threshold; a quota
actually refuses the allocation. These live bytes are not `lua.memstats`' categorized churn.
Native callback allocations, GC scratch and abandoned-coroutine attribution are outside the
quota, and there is no per-script grant - **this is not complete untrusted-code isolation**.
See the [Luau backend](Luau.md#configuration).

### lua.requireresolution (runtime CVar)

`lua.requireresolution 1` enables a warning when the file-system searcher declines a module
(default `0`): unresolved name, number of patterns tried, an example path, editor/cooked build
kind, plus the case-sensitivity and cooked staging rules. It is off by default because a
searcher declining in favour of the next one is normal. Read the full contract from
`package.resolution()`; see [module resolution](Luau.md#module-resolution-and-unload-contract).

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

### lua.sandboxdeny (startup CVar)

With `lua.sandbox=1`, comma-separated globals are removed **before** `luaL_sandbox` freezes
`_G`, e.g. `lua.sandboxdeny=debug,io,socket,os`. The globals the host keeps writing after
startup (`_G`, `package`, `require`, `Lua`, `UnLua`, `UnLuaNova`, `UE`, `UE4`) are protected:
removing them would break lazy UE type registration and `require`, not harden anything. The
default is empty, i.e. the host's default capability set is kept.

This is capability trimming, **not a complete untrusted-code permission model**: UE/debug/io/
socket are still present unless explicitly denied, and there is no per-script grant.
`ApplyCapabilityPolicy()` returns how many globals were actually removed, so an automation
spec can tell "policy applied" from "silently did nothing".

### lua.iomaxbytes (runtime CVar)

Per-file byte cap for the `lua.io` buffered io library (default 16 MiB, `0` = unlimited).
Checked before `io.open` reads a file, after `file:write` concatenates, and before
`file:flush` writes to disk; exceeding it raises an error instead of letting one script push
an unbounded allocation into a Lua string. This is host policy, not Lua io semantics.

*Off by default, because it changes the behaviour of existing scripts. It relies on Luau's
native sandbox and is unsupported on the Lua 5.5 backend.*

---

## `lua.codegen` - Luau native compilation (off by default)

`lua.codegen.enabled=1` (default 0) affects only chunks loaded **after** the switch;
`lua.codegen.modules` narrows it by module-name substring. `lua.codegen on/off` and the
CVar are the same state.

| Item | Contract |
|---|---|
| Backend / architecture | Luau only; upstream `luau_codegen_supported()` decides at runtime (x64 / arm64). Other targets ignore the switch and warn |
| Verified scope | UE 5.8.3 / Win64 x64; arm64, packaged Shipping and native-debug attach have no run acceptance |
| Executable memory | Upstream allocates executable pages; the host does not take over W^X or reclamation |
| Failure semantics | `luau_codegen_compile()` returns void with no per-chunk success signal; an uncompilable chunk keeps interpreting |
| Counter | `lua.codegen` reports **request count** (`GetNativeCodeGenRequestCount()`), not successfully compiled functions |

This is an optional optimisation, not a correctness dependency: with it off every chunk
is interpreted and the feature set is identical. See [Luau backend](Luau.md) and
[Production Deployment](Production_Deployment.md).

---

## Further reading

- [Settings](Settings.md) - related settings (backend, watchdog)
- [Luau backend](Luau.md) - sandbox, native breakpoints, categorized allocation

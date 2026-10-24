---
title: "Debugging"
description: "Breakpoint debugging UnLuaNova scripts with lua.dbg, LuaPanda / LuaHelper"
---

# Debugging

Four ways in: the built-in DAP service (`lua.dap`, VSCode attaches directly), the built-in
headless debug service (`lua.dbg`, console driven), LuaPanda / LuaHelper (graphical
breakpoints in VSCode), or a third-party debugger. Which one depends on your backend and your
environment.

## DAP service (lua.dap)

`lua.dbg` deliberately owns no socket and no wire protocol - that layer is DAP:

```
VSCode ──DAP/JSON──► lua.dap (in the editor process, own thread) ──► the lua.dbg service
```

```
lua.dap start [port]     -- default 5699; starts the service and waits for the IDE
lua.dap status
lua.dap stop
```

From VSCode, **attach** to `127.0.0.1:5699` (`request: "attach"`). Implemented:
`initialize` / `attach` / `setBreakpoints` / `threads` / `stackTrace` / `scopes` /
`variables` / `continue` / `next` / `stepIn` / `stepOut` / `disconnect`.

Behaviour worth knowing before you conclude it is broken:

* **The game thread blocks in a spin when a breakpoint hits** (waiting for `Continue()`).
  That is what lets the IDE read the stack; it also means the editor UI stalls. Expected.
* **Breakpoint edits while paused take effect after `Continue()`** - the game thread is
  blocked and cannot be queued into.
* Attach only, not launch: the debug target is the editor / PIE process, not a separate one.
* **With several envs, say which one**: `"arguments": {"env": 1}` (0-based, default 0).
  Under `ULuaEnvLocator_ByGameInstance` a listen server plus two clients is three envs, and
  attaching to the wrong one shows up as breakpoints that never hit, with no error anywhere.

### pause and evaluate

Neither is free, so both are worth spelling out:

* **`pause`** - Lua has no way to interrupt a running VM from outside, so this does not force
  a stop; it **makes the next line a stop**. On Luau the debug hook only fires at breakpoints
  and single steps, so the implementation arms a single step and disarms it the moment it
  lands (`FLuaDebugService::RequestPause`).
* **`evaluate`** - evaluation has to run on the thread that owns the VM, and when a breakpoint
  hits that thread is exactly where it is blocked, which is the only safe moment. The
  expression is compiled as

  ```lua
  return (function(<that frame's locals...>) return <expr> end)(...)
  ```

  and called with that frame's locals, so `self.HP` and `i + 1` mean what they look like.
  Upvalues are collected too: Lua keeps most of its real state in them, and without them a
  variable that is plainly in scope answers "attempt to index a nil value", which reads as "it
  is gone" and sends you looking in the wrong place. `_ENV` is the one exception - it is every
  function's first upvalue, and binding it would shadow the globals. With nothing paused, or
  with no `frameId`, evaluation is against globals only (what a watch / repl wants).

## Built-in debug service (lua.dbg)

Luau exposes VM-native breakpoint primitives (`lua_breakpoint` / `lua_singlestep`); UnLuaNova
wraps them in a debug service with no UI of its own - no plugin to install, no `luasocket`,
driven straight from the console.

```
lua.dbg                        -- status: which env / attached / paused / hits / breakpoints
lua.dbg env                    -- list every Lua env, marking the one being debugged
lua.dbg env 1                  -- switch to env 1 (0-based)
lua.dbg bp MyActor 42          -- toggle a breakpoint on MyActor line 42
lua.dbg stack                  -- print the captured call stack
lua.dbg locals                 -- print the captured locals
lua.dbg step                   -- single step
lua.dbg continue               -- continue
lua.dbg clear                  -- remove every breakpoint
lua.dbg detach                 -- uninstall the debug hook
```

Points worth knowing:

* No hook is installed until you ask for one, so the steady-state cost is **zero**
* **Both backends now match source:line.** Luau's `lua_breakpoint()` wants a function that is
  already on the stack - it patches that one Proto - and a `(source, line)` pair does not
  identify a Proto until something is executing it. So while any breakpoint exists the service
  arms `lua_singlestep` and does the matching itself, the same mechanism plain Lua uses. The
  cost is one hook per line for as long as a breakpoint is armed; with none, still zero.
* **Pick the env when there is more than one.** One env per game instance is the default, and
  the service attaches to exactly one - `lua.dbg env <n>` from the console, `arguments.env`
  from DAP.
* There is deliberately no UI - the API is there for an external front end to drive, which is
  what [`lua.dap`](#dap-service-luadap) is

See [Luau backend](Luau.md).

## LuaPanda / LuaHelper

### 1. Set up

1. Install [LuaPanda](https://marketplace.visualstudio.com/items?itemName=stuartwang.luapanda)
   or [LuaHelper](https://marketplace.visualstudio.com/items?itemName=yinfei.luahelper) from the
   VSCode marketplace
2. Take `LuaPanda.lua` from the
   [LuaPanda repo](https://github.com/Tencent/LuaPanda/tree/master/Debugger) and drop it into
   `{UEProject}/Content/Script`
3. Add `require("LuaPanda").start("127.0.0.1", 8818)` to your Lua code

### 2. Start debugging

Follow each extension's own configuration docs:
[LuaPanda](https://github.com/Tencent/LuaPanda) · [LuaHelper](https://github.com/Tencent/LuaHelper)

Note: those debuggers need `luasocket`, which UnLuaNova ships through the extensions plugin.
If the connection fails, check that `{UEProject}/Plugins/UnLuaNovaExtensions` exists and is
enabled.

## Further reading

- [IntelliSense](IntelliSense.md) - teaching your IDE the UE API
- [Console Commands](ConsoleCommand.md) - lua.dbg and friends
- [Luau backend](Luau.md) - native breakpoints, sandbox and other Luau-only features
- [Testing and Automation](Testing.md) - automated tests and screenshots

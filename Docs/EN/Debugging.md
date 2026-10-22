---
title: "Debugging"
description: "Breakpoint debugging UnLuaNova scripts with lua.dbg, LuaPanda / LuaHelper"
---

# Debugging

Three ways in: the built-in headless debug service (Luau backend, zero dependencies),
LuaPanda / LuaHelper (graphical breakpoints in VSCode), or a third-party debugger. Which one
depends on your backend and your environment.

## Built-in debug service (lua.dbg)

Luau exposes VM-native breakpoint primitives (`lua_breakpoint` / `lua_singlestep`); UnLuaNova
wraps them in a debug service with no UI of its own - no plugin to install, no `luasocket`,
driven straight from the console.

```
lua.dbg                        -- status: attached / paused / hit count / breakpoint count
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
* Breakpoints and stepping are Luau-only; on the Lua 5.5 backend they degrade to matching
  source:line
* There is deliberately no UI - the API is there for an external front end to drive (a DAP
  adapter is planned)

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
- [Testing and Automation](../EN/Testing.md) - automated tests and screenshots

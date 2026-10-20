---
title: Luau Backend
description: UnLuaNova's Luau backend — language features you can use today, type declarations with luau-analyze, sandboxing, native breakpoints, and the differences from Lua 5.5
---

# Luau Backend

[Luau](https://luau.org) is UnLuaNova's **default backend**; the other one is official Lua 5.5.
This page is for script authors: what works, what does not, and what is not wired up yet.

## Switching backends

Pick one in `Config/DefaultUnLuaNovaEditor.ini` and **rebuild**:

```ini
[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]
LuaBackend=Luau   ; or Lua
```

!!! warning "The backend is a compile-time choice"
    Both backends export the same `lua_*` symbols and own the same header names, so they
    cannot coexist in one build. Changing the ini means recompiling — it is not a runtime switch.

---

## 1. Language features available today

These come from Luau itself (all Luau libraries are registered via `luaL_openlibs`), no engine changes needed:

| Feature | Status | Notes |
|---|:--:|---|
| Type annotations, `--!strict` / `--!nonstrict` | Available | Pair it with the generated `UnLuaNova.d.lua`, see [type declarations](#3-type-declarations-and-luau-analyze) |
| `continue` | Available | Lua 5.5 has no such keyword |
| Compound assignment `+=` `-=` `*=` `/=` `%=` `..=` | Available | Also syntax-level incompatibility |
| `type()` / `typeof()` | Available | `typeof` returns the class name for userdata |
| `table.freeze` / `isfrozen` / `clone` / `clear` | Available | Writing to a frozen table raises an error |
| `coroutine.close` | Available | Lua 5.4+ only |
| `bit32`, `utf8` | Available | Shipped with Luau |
| `vector` (native vector3 value type) | Library registered, **not wired to UE** | See [what is not wired up yet](#5-what-is-not-wired-up-yet) |
| `buffer` | Library registered, **not wired to UE** | Same |
| `load()` | Patched in by UnLuaNova | Luau has no source loader |
| `debug.getupvalue` / `setupvalue` / `upvalueid` / `getlocal` / `setlocal` | Patched in by UnLuaNova | Hot reload depends on them |

```lua
--!strict
local Config = table.freeze({
    MaxHp = 100,
    RegenPerSecond = 2,
})

function M:ReceiveTick(DeltaTime: number)
    for _, Member in ipairs(self.Members) do
        if not Member.bAlive then
            continue                                   -- Luau keyword; a syntax error on Lua 5.5
        end
        Member.Hp += Config.RegenPerSecond * DeltaTime  -- compound assignment
        if Member.Hp > Config.MaxHp then
            Member.Hp = Config.MaxHp
        end
    end
end
```

!!! tip "Writing for both backends"
    The snippet above does not even compile on Lua 5.5. Keep cross-backend code free of
    Luau-only syntax, or put it behind a runtime guard in a separate directory.

---

## 2. Luau capabilities UnLuaNova already wires up

These are places where UnLuaNova reaches for Luau's native API. Lua 5.5 has no equivalent:

| Capability | Luau API | Entry point | Notes |
|---|---|---|---|
| Sandbox | `luaL_sandbox` | CVar `lua.sandbox` | Read-only globals, see [sandbox](#sandbox) |
| Breakpoints / stepping | `lua_breakpoint`, `lua_singlestep` | `lua.dbg` | See [native breakpoints](#native-breakpoints) |
| Categorized allocation stats | `lua_callbacks->onallocate` | `lua.memstats` | By memory category / Lua type / userdata tag |
| Function-level profiling | `lua_Debug.protoid` | `lua.profiler` | Stable, unique cache key |
| Iteration protocol | `__iter` | Container bindings | `pairs(TArray)` works on both backends |
| 64-bit integers | `lua_pushinteger64` | Binding layer | Lua 5.5 uses `lua_pushinteger` |

### Sandbox

`luaL_sandbox` makes the global table read-only and flags the environment as safe, so a
hotfixed or downloaded script can no longer clobber engine bindings by accident.

!!! warning "It is a startup CVar, not a command"
    `lua.sandbox` is a console variable (default `0`) read **once, when each Lua environment
    is created**. Changing it at runtime does not affect environments that already exist.
    Enable it from an ini so it applies at startup:

    ```ini
    ; Config/DefaultEngine.ini
    [ConsoleVariables]
    lua.sandbox=1
    ```

Once enabled, writes like these raise an error:

```lua
UE = nil                     -- rewriting a global
UE.UObject = somethingElse   -- rewriting an engine binding
```

Recommended for hotfix / downloaded scripts; evaluate first on existing projects, since it
changes the "globals are writable" behaviour they may rely on. Luau-only
(`UNLUANOVA_HAS_SANDBOX == 0` on Lua 5.5).

### Native breakpoints

Luau's breakpoint primitives can make the VM stop on a given line — something `lua_sethook`
cannot do (it can only *report* every line). UnLuaNova wraps them in a headless debug service:

```
lua.dbg                        -- status: attached / paused / hits / breakpoints
lua.dbg bp MyActor 42          -- toggle a breakpoint at MyActor line 42
lua.dbg stack                  -- print the last captured callstack
lua.dbg locals                 -- print the last captured locals
lua.dbg step                   -- single step
lua.dbg continue               -- resume
lua.dbg clear                  -- remove all breakpoints
lua.dbg detach                 -- uninstall the hook, back to zero overhead
```

No hook is installed until you type a command, so the default is zero cost. Breakpoints and
stepping are Luau-only; Lua 5.5 falls back to line-number matching.

### Categorized memory stats

```
lua.memstats on     -- install the allocation callbacks
lua.memstats        -- print the report
lua.memstats reset  -- clear the counters
```

The report has two halves: **totals** (current, peak, alloc/realloc/free counts) and a
**breakdown** (top N by memory category, Lua type and userdata tag). The breakdown needs
Luau's allocation callbacks; on Lua 5.5 it prints
`Categorized breakdown requires the Luau backend.` and stops.

!!! note "The breakdown counts churn, not live bytes"
    The `onfree` callback carries no size, so the categorized view measures allocation
    *volume*. For live bytes and peak, read the totals half.

---

## 3. Type declarations and luau-analyze

The editor module generates Luau declarations for engine types so `luau-analyze` can catch a
class of UE API misuse (wrong function name, wrong arity, wrong argument type) before you run.

**Generate**: UnLuaNova toolbar → export IntelliSense. Output:

```
Plugins/UnLuaNova/Intermediate/IntelliSense/
├── *.lua                EmmyLua annotations, for IDE completion
└── UnLuaNova.d.lua      Luau declarations, for luau-analyze
```

!!! note "It is a `.d.lua`, not something to require"
    A pure declaration file made of `declare class` / `declare function`.
    It is referenced by the static checker only — never `require` it at runtime.

**Use**: drop `UnLuaNova.d.lua` into your script project, put `--!strict` at the top of your
scripts, then run `luau-analyze`:

```bash
luau-analyze --solver=V2 --settings=luau-settings.json Content/Script/
```

Mapping rules: integers of every width and floats collapse to `number`,
`FString`/`FName`/`FText` to `string`, `TArray<T>` to `{T}`, `TMap<K,V>` to `{[K]: V}`,
`TSet<T>` to `{[T]: boolean}`, structs keep their `USTRUCT` name, and delegates or anything
exotic degrade to `any`.

!!! tip "Tooling is being filled in"
    Declarations are generated today; a one-command check script is not in the repo yet.
    `Tools/CheckLuauTypes.ps1` is planned to wire `luau-analyze` into local and CI runs.

---

## 4. Differences from Lua 5.5

What matters when you write code for both backends:

| Item | Luau | Lua 5.5 |
|---|---|---|
| `lua_Integer` | **32-bit** | 64-bit |
| 64-bit integer passing | `UNLUANOVA_PUSHINT64/TOINT64` in the binding layer | `lua_pushinteger` is enough |
| userdata `__gc` | **does not exist** (`UNLUANOVA_HAS_USERDATA_GC = 0`) | exists |
| Iteration metamethod | `__iter` | `__pairs` / `__ipairs` |
| `load()` | not native, patched in by UnLuaNova | native |
| `debug` library | traceback / info / profiler hooks only, rest patched in | complete |
| Sandbox | `luaL_sandbox` | no equivalent |
| Breakpoints / stepping | native `lua_breakpoint` / `lua_singlestep` | no equivalent |
| Generational GC | none — tuned via goal / step multiplier | `LUA_GCGEN` |

Two practical consequences:

1. **Do not rely on `__gc` for cleanup.** Luau has no userdata `__gc`, so container userdata
   (`TArray`/`TMap`/`TSet`) are never finalized there. Tie their lifetime to the reference
   chain or release them explicitly.
2. **The yield boundary is stricter.** Luau refuses `lua_yield()` once another C boundary has
   been crossed, reporting `attempt to yield across metamethod/C-call boundary`. Starting a
   Latent action from script is fine; starting one inside an engine callback may fail — ask
   `UNLUANOVA_IS_YIELDABLE` first.

---

## 5. What is not wired up yet

| Item | Status | Why it matters |
|---|---|---|
| `vector` ↔ `FVector` | library usable, `FVector` still userdata | native vector is a register-sized value type — no allocation per operation |
| `buffer` ↔ `TArray<uint8>` | library usable, no container binding | zero-copy byte streams |
| Freeze type tables under sandbox | only the global table is hardened | freezing registered UE type tables makes accidental writes fail, and Luau optimizes frozen tables |
| `luau-analyze` tooling | declarations generated, no check script | static checks in CI, catch UE API misuse early |
| Examples and tests | examples are plain Lua, specs do not cover Luau-only paths | see below |

Planned alongside: an example set under `Content/Script/Luau/` (strict typing, sandbox
boundaries, `lua.dbg` hits, Luau syntax tour) and spec cases in `LuaBackend.spec.cpp`
branched on `UNLUANOVA_HAS_SANDBOX` / `UNLUANOVA_HAS_BREAKPOINT` / `UNLUANOVA_HAS_ALLOC_CALLBACKS`.

---

## Further reading

- [Programming Guide](UnLuaNova_Programming_Guide.md) — binding and two-way calls
- [API Reference](API.md) — C++ and Lua API
- [How To Implement Overriding](How_To_Implement_Overriding.md) — how overriding works
- [FAQ](FAQ.md) — frequently asked questions

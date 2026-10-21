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
| `vector` (native vector3 value type) | Wired up | See [vector and FVector](#vector-and-fvector) |
| `buffer` | Library registered, container needs explicit conversion | See [vector and FVector](#vector-and-fvector) |
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

On top of that, the `UE` namespace carries its own `__newindex`, so a script replacing a type
in it is refused outright - types are published by the binding layer with `lua_rawset`, and
nothing in script land has a reason to overwrite one. That half works on both backends.

!!! note "The sandbox freezes `UE`; UnLuaNova thaws it again"
    `luaL_sandbox` marks every table hanging off the globals read-only, `UE` included - and
    UE types are registered **lazily**, the first time `UE.XXX` is asked for. So right after
    sandboxing, UnLuaNova hands write access back to `UE`, `Lua` and `package.loaded`.
    Without that, the next script to ask for a new type - or the next `require` - errors out.
    Script-side writes are still stopped by `__newindex`, so nothing is lost.

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

### vector and FVector

Luau's `vector` lives inside a TValue (three floats): arithmetic on it allocates nothing.
`FVector` is a userdata with a metatable, so every `a + b` allocates one and hands it to
the GC. UnLuaNova bridges the two:

```lua
local Native = vector.create(1, 2, 3)   -- pure math, zero allocation
local Sum = (Native + Native) * 2

local Vec = UE.FVector(Native)          -- vector -> FVector, no three lua_tonumber calls
Vec:Set(vector.create(10, 20, 30))      -- all three components in one call
local Back = Vec:ToVector()             -- back to a native vector (nil on Lua 5.5)

-- a UFUNCTION taking FVector accepts a vector directly
self:K2_SetActorLocation(vector.create(0, 0, 100), false, false)
```

The reflection path accepts it too: a vector has no metatable, so without an explicit branch
there the type check rejects it with "userdata needed".

!!! warning "Precision: vector is float, FVector is double on UE5"
    UE 5's `FVector` stores doubles while `vector` stores floats, so a round trip loses
    precision. Fine for directions, velocities and offsets; keep `FVector` where you need
    exact values back.

The `buffer` library ships with Luau as well and converts to and from `TArray`:

```lua
local Bytes = UE.TArray(0)
Bytes:Add(1) Bytes:Add(2) Bytes:Add(3)
local Buf = UnLuaNova.Buffer.FromArray(Bytes)   -- TArray -> buffer
local Back = UnLuaNova.Buffer.ToArray(Buf)      -- buffer -> TArray
```

That loop is O(n) byte by byte: fine for one-shot packing, wrong for a per-frame hot path.
Both `UnLuaNova.Vector` and `UnLuaNova.Buffer` are safe to call on either backend
(`HasVector()` / `HasBuffer()` answer false on Lua 5.5 and the conversions return nil).

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

**One command**: `Tools/CheckLuauTypes.ps1` wires the two steps together - find
`luau-analyze`, pass it the `.d.lua`, run it over a script directory, and exit non-zero if
any type error is reported:

```powershell
# check all of Content/Script
.\Tools\CheckLuauTypes.ps1

# only the Luau examples, in strict mode
.\Tools\CheckLuauTypes.ps1 -Path Content/Script/Luau -Mode strict

# pin the binary; fail instead of skipping when it is missing
.\Tools\CheckLuauTypes.ps1 -LuauAnalyze C:\Tools\luau-analyze.exe -Require
```

`luau-analyze` itself is not in the repo (it is an executable). The script looks for it as
`-LuauAnalyze` → `LUAU_ANALYZE` → `PATH` → the Luau submodule's cmake build directory.
When it finds none it skips and exits 0, so it is safe to hang off any scheduler; add
`-Require` if a missing tool should count as a failure.

!!! note "Why this is not a CI step"
    GitHub-hosted runners have neither `luau-analyze` nor a local UE source build, so type
    checking is a **local** step. CI only checks that the script itself parses.

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

## 5. Examples and tests

### Examples

`Content/Script/Luau/` holds the examples, entered through `Index.lua`:

```lua
local Examples = require "Luau.Index"
if Examples.Available then
    Examples.StrictTyping:Run()
end
```

| File | What it shows |
|---|---|
| `Index.lua` | the backend guard: loads nothing at all unless the backend is Luau |
| `01_StrictTyping.lua` | `--!strict`, type annotations, a frozen config table |
| `02_SandboxBoundary.lua` | what you can still write once the sandbox is on |
| `03_NativeDebug.lua` | a target to set `lua.dbg` breakpoints in |
| `04_LuauSyntax.lua` | `continue`, compound assignment, `table.freeze/clone/clear`, `bit32` |
| `05_VectorInterop.lua` | the four `vector` ↔ `FVector` paths |

!!! warning "These files cannot load on Lua 5.5"
    They use Luau syntax (annotations, `continue`, compound assignment), which is a
    **compile error** on Lua 5.5. `Index.lua` asks `UnLuaNova.GetBackend()` first and only
    requires them when the answer is Luau - never require one of them directly.

### Tests

`LuaBackend.spec.cpp` grew a "Luau 专属能力" group, branched on capability macros:

* Both backends: `GetBackend()` reports the right backend, `HasFeature()` agrees with the
  compile-time capability, script writes into `UE` are refused
* Luau only: Luau syntax compiles and evaluates, `vector` round-trips through `FVector`,
  `UnLuaNova.Vector` converts, `buffer` reads and writes, the sandbox blocks global writes,
  type registration still works under sandbox, breakpoint primitives respond, allocation
  callbacks fire
* Lua 5.5 only: `ToVector()` degrades to `nil`

Run them like any other test. The backend is a compile-time choice, so the script builds
for the `-Backend` you ask for before it runs anything:

```powershell
# this group, Luau only
.\Tools\RunTests.ps1 -Backend Luau -Filter UnLuaNova.API.LuaBackend

# both backends, one report each (Both is the default)
.\Tools\RunTests.ps1 -Filter UnLuaNova.API.LuaBackend
```

---

## 6. What is still missing

| Item | Status | Note |
|---|---|---|
| Zero-copy `buffer` ↔ `TArray<uint8>` | explicit byte-by-byte conversion | true zero copy needs the container binding to read and write buffer memory directly |
| Freezing **class tables** under sandbox | globals hardened + writes into `UE` refused | class tables cache lazily (a first access `rawset`s), so freezing them breaks the cache |
| `luau-analyze` in CI | local script exists, CI has no binary | a self-hosted runner can run it as-is |
| DAP adapter | `lua.dbg` is headless | the capability is exposed; what is missing is the layer that plugs it into an IDE |

---

## Further reading

- [Programming Guide](UnLuaNova_Programming_Guide.md) — binding and two-way calls
- [API Reference](API.md) — C++ and Lua API
- [How To Implement Overriding](How_To_Implement_Overriding.md) — how overriding works
- [FAQ](FAQ.md) — frequently asked questions

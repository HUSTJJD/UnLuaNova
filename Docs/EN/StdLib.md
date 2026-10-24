# Standard Library

Lua modules shipped with the plugin live under `Content/Script/UnLuaNova/` and are
`require`d directly:

```lua
local Timer = require('UnLuaNova.Timer')
local Event = require('UnLuaNova.Event')
```

## Boundary: mechanisms go in, policies do not

That single rule decides whether something belongs in the standard library:

| | Belongs in the stdlib | Does not |
|---|---|---|
| Test | **It cannot be implemented correctly without touching the engine/VM**, and every project would write it the same way | Projects have strong opinions and half of them will rewrite it |
| Examples | Timers (world time), event bus (mutating during broadcast), module hot reload | UI stack, level streaming, HTTP, model layer, network protocol |

The reason is plain: **publishing `UnLuaNova.X` is a promise of API stability.** Changing
it is a breaking change, and nothing draws more complaints than "why did you change the
free thing you gave me". So:

- Mechanisms go in. They are the parts every project rewrites anyway, and usually gets
  wrong.
- Policies stay out. Ship them as reference implementations in the sample project, or as
  a separate extension plugin.

The other half of the rule: **not everything deserves to be C++.** The cost structure of
cross-language calls decides it - if the callback still lives in Lua, you do not save the
crossing. Dropping down to C++ only pays off when at least one of these holds:

1. **Only the engine can make it correct** (world time, pause, time dilation, PIE lifetime)
2. **The scale makes a per-frame table scan in Lua unacceptable** (tens of thousands of entries)
3. **C++ also needs to send or read it** (interop)
4. **Zero copy or a specific memory layout is required** (containers, buffers)

`Timer` hits 1 and 2. `Event` hits none of them - which is why timers are C++ and events
are Lua. **Do not write C++ for speed; write it for correctness.**

## Switching: one format for every feature

There is exactly **one** place where feature switches are defined:
`[/Script/UnLuaNova.UnLuaNovaSettings]` in `Config/DefaultUnLuaNova.ini`. Three fixed
rules:

1. **Keys are always `b<CamelCase>`**, and the query name is that key without the `b`,
   lower-cased (`bTimer` -> `HasFeature('timer')`)
2. **One entry point**: `UnLuaNova.HasFeature('<name>')` from scripts, the `Features[]`
   table in `UnLuaNovaLib.cpp` from C++. There is no second one.
3. **Case insensitive**: `oop` and `OOP` mean the same thing, so nobody has to guess the
   casing

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
bTimer=true                ; stdlib UnLuaNova.Timer (changing this needs a rebuild)
bEvent=true                ; stdlib UnLuaNova.Event (changing this needs a rebuild)
bOOP=true                  ; stdlib UnLuaNova.OOP (changing this needs a rebuild)
bDataDrivenBinding=false   ; data-driven binding (off by default, see Settings)
bDebugAdapter=true         ; lua.dap
DebugAdapterPort=5699      ; lua.dap default port
```

**There is exactly one exception to "runtime vs compile time"**: the stdlib modules
(Timer / Event / OOP) compile out entirely when switched off, so they are additionally
turned into macros by `UnLuaNova.Build.cs` from **the same ini and the same key names**
(`UNLUANOVA_HAS_STDLIB_*`) - not from a parallel `bWithStdLibXxx` set. Every other
feature is runtime only. The macros are defined only in `UnLuaNova.Build.cs`;
`UnLuaNovaBase.h` deliberately has no `#ifndef` fallback: one configuration, one answer,
and "forgot to define it" should show up as a build that never ran rather than as a
library quietly switched on.

What happens when you switch a stdlib module off:

| Module type | Behaviour |
|---|---|
| native (Timer) | No loader is registered, so `require` simply fails |
| Lua (Event / OOP) | The module self-checks and `require` raises a clear "this build has the library disabled" error instead of returning a half-built table |
| all | `UnLuaNova.HasFeature('<name>')` answers false |

Defaults ship with the plugin in `Plugins/UnLuaNova/Config/DefaultUnLuaNova.ini`
(complete, with comments); the project ini only lists overrides. Keys marked
`[needs rebuild]` must be changed in the **project's** copy - Build.cs does not read the
plugin's own ini.

Backend capabilities (`sandbox` / `vector` / `buffer` / `breakpoint` / `allocCallbacks`)
are **not configurable** - the backend decides them - but they surface through the same
`HasFeature`, so scripts look identical either way: you never need to know whether a value
came from ini or from the backend.

Ask for the capability, never guess it:

```lua
if UnLuaNova.HasFeature('timer') then
    local Timer = require('UnLuaNova.Timer')
end
```

Adding a feature = one `b<Name>` on the settings class + one line in the table
(`Features[]` in `UnLuaNovaLib.cpp`). **Those two places only**; there is no third.

## Backend parity: every module must run on Lua and on Luau

The backend is a compile-time choice (both export the same `lua_*` symbols and their
bytecode is not compatible, so they cannot coexist), which means every stdlib module has
to pass on both backends, and "it works on the backend I compiled this time" does not
count as working.

Hard requirements for Lua modules (Event / OOP):

- Keep the syntax **Lua 5.1 compatible**: no `continue`, no compound assignment (`+=`),
  no type annotations
- Never use a library function that exists on only one backend; read globals as
  `_G.xxx` rather than assuming they are there
- The repo's specs run against both backends (see [Testing and Automation](Testing.md));
  add coverage when you add a module

Hard requirements for native modules (Timer):

- Go through the `LuaBackend.h` shims on the C API; never include a backend's private headers
- Move integers with the `UNLUANOVA_PUSHINT` / `UNLUANOVA_TOINT64` macros: on Luau
  `lua_pushinteger` pushes a double, and writing it directly truncates silently at compile time
- Treat "0 means success" as the only contract; do not depend on `LUA_OK`, which the two
  backends do not define identically

## Timer (C++ driven)

Timer now cleans up with its Env/World and invokes callbacks on the owning Env's main state. Regression sources are added but unbuilt and unexecuted; see [LIFE-01](Benchmark_Review.md).

```lua
local Timer = require('UnLuaNova.Timer')

local Handle = Timer.Set(Callback, Interval, bLoop, Delay, bUnscaled, WorldContext)
Timer.Clear(Handle)
Timer.Pause(Handle, true)
Timer.IsValid(Handle)
Timer.ClearAll(WorldContext)
local Seconds = Timer.Time(bUnscaled, WorldContext)
local N = Timer.Count(WorldContext)
```

| Parameter | Meaning |
|---|---|
| `Callback` | Function invoked when it fires, no arguments. Use a closure to pass parameters |
| `Interval` | Seconds. `0` together with `bLoop` means every frame |
| `bLoop` | Repeating (default false, i.e. one-shot) |
| `Delay` | Extra seconds to wait before the first fire (default 0) |
| `bUnscaled` | Use real time: **ignores pause and time dilation** (default false) |
| `WorldContext` | UObject/UWorld in a Game/PIE world; omission requires exactly one Game/PIE world |

Interval and delay must be finite nonnegative numbers representable as floats; both default to zero. Numeric strings are rejected. Handles are process-local, never reused, and cannot be used by another Env or persisted.

### Lifetime and Scheduling

- Registry references retain callback closures; the registering coroutine can be collected. Callbacks cannot yield or resume latent operations through that coroutine.
- Env close and World subsystem shutdown cancel tasks and release references. Closing environments reject new registrations.
- Clear/Pause/IsValid resolve the original World from the handle. Count/ClearAll affect only the caller's Env in the selected World.
- One-shots are removed before invocation. Cancelling/pausing another due callback takes effect immediately; nested ticks cannot reenter.
- Pause preserves remaining delay. Repeating timers run at most once per frame, without replaying missed periods.
- Scaled timers stop while the World is paused; unscaled timers use real world time. Callback errors are logged and other tasks continue.

Example: [TimerLifecycle.lua](../../Content/Script/Examples/TimerLifecycle.lua). Regression entry: `UnLuaNova.API.Timer`. Multi-PIE, world pause/time dilation and packaged behavior still require execution.

## Event (pure Lua)

```lua
local Event = require('UnLuaNova.Event')

local Token = Event.On("PlayerDead", Handler)        -- subscribe
local Token = Event.Once("PlayerDead", Handler)      -- fires once
local Token = Event.On("PlayerDead", Handler, self)  -- subscribe with an owner (recommended)

Event.Emit("PlayerDead", DamageCauser)               -- broadcast, returns how many fired
Event.Off(Token)                                     -- remove one subscription
Event.Off("PlayerDead", Handler)                     -- remove that function from that event
Event.Off("PlayerDead")                              -- clear that event
Event.UnbindOwner(self)                              -- drop everything owned by `self`
Event.Count("PlayerDead")
Event.Clear()                                        -- clear everything (no name = all)
```

It does not exist to "store a table" (that part is trivial). It exists for these four:

| Hand-written problem | How it is handled |
|---|---|
| Listeners added/removed during broadcast, so some are missed or fired twice | Snapshot first, then invoke one by one |
| One listener's error takes down the whole broadcast | Each invocation is `pcall`ed; errors are only logged |
| A listener retains its owner | Tokens hold owners strongly; explicitly call `UnbindOwner(owner)` on teardown. A weak-key index does not provide automatic release |
| Stale closures surviving a hot reload | No silver bullet; the only reliable pattern is **register with an owner and `UnbindOwner` on teardown**, which is why both APIs exist |

Subscriptions without an owner leave their old closures on the bus after a hot reload -
the most common cause of "gets slower over time and sometimes runs twice" in scripted
projects.

## OOP (pure Lua)

```lua
local OOP = require('UnLuaNova.OOP')

local Animal = OOP.Class("Animal")
function Animal:Ctor(Name) self.Name = Name end
function Animal:Tag() return "Animal" end

local Dog = OOP.Class("Dog", Animal)
function Dog:Ctor(Name) Dog.Super.Ctor(self, Name) end   -- explicit base constructor
function Dog:Tag() return "Dog+" .. Dog.Super.Tag(self) end

local Rex = Dog:New("Rex")
OOP.Is(Rex, Animal)     -- true
OOP.IsClass(Dog)        -- true
OOP.NameOf(Rex)         -- "Dog"
print(Rex)              -- "instance of Dog"
```

| API | Meaning |
|---|---|
| `OOP.Class(Name)` / `OOP.Class(Name, Super)` / `OOP.Class(Super)` | Create a class, single inheritance |
| `Class:New(...)` | Create an instance, calling `Ctor` **from base to derived**, passing the arguments through to every level |
| `Class.Super` | The base class; call the base implementation as `Dog.Super.Ctor(self, ...)` (never `Dog.Super:Ctor()`, which passes the base table as `self`) |
| `OOP.Is(Instance, Class)` | Walks the inheritance chain |
| `OOP.IsClass(Value)` / `OOP.NameOf(Value)` | Is it a class / its name (for logging) |

The two mistakes hand-written class systems make most often are both pinned down here:

- **The constructor chain skips the base**: `New` walks `__Chain` from base to derived
  calling `Ctor`, and a level without a `Ctor` does not break the chain
- **`Super:` passes the base as self**: `Dog.Super:Tag()` passes the base table as `self`,
  which misbehaves without raising. The correct form is `Dog.Super.Tag(self)`

**Deliberately not provided**: multiple inheritance / mixins (use composition); inheriting
UE types (that is the native `UnLuaNova.Class()`, which binds a UCLASS - a different
thing); lifetime hooks (that is a project framework decision).

## Other bundled modules

| Module | Meaning |
|---|---|
| `UnLuaNova.StringEx` | String extensions |
| `UnLuaNova.Vector` | Pure-Lua `FVector` arithmetic (no crossing, saves call overhead on hot paths) |
| `UnLuaNova.Buffer` | Buffers (native on the Luau backend, see `UnLuaNova.HasFeature('buffer')`) |
| `UnLuaNova.Input` / `UnLuaNova.EnhancedInput` | Input wrappers |
| `UnLuaNova.HotReload` | Hot reload scripts |

Availability and capabilities always go through `UnLuaNova.HasFeature`:

```lua
if UnLuaNova.HasFeature('timer')  then ... end
if UnLuaNova.HasFeature('event')  then ... end
if UnLuaNova.HasFeature('oop')    then ... end
if UnLuaNova.HasFeature('buffer') then ... end
if UnLuaNova.HasFeature('sandbox') then ... end
```

## What adding a module requires

1. Go through the "mechanism vs policy" test and the four criteria for dropping to C++
   above, and write the result in the module header comment
2. Lua modules go to `Content/Script/UnLuaNova/<Name>.lua`, Lua 5.1 compatible
   (no `continue` / compound assignment / type annotations) - both backends must compile it
3. Native modules register through `FLuaEnv::AddBuiltInLoader` (the fourth `require`
   searcher hits it)
4. **There must be a spec**, and if the claim is "for performance", put both the Lua and
   the C++ variant into `UnLuaNova.Perf.Benchmark`'s CSV - the number has to justify
   going native (see [Testing and Automation](Testing.md))

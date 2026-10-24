---
title: "Why UnLuaNova"
description: "The pain point, mechanism, payoff and limits of every UnLuaNova addition over UnLua v2.3.6"
---

# Why UnLuaNova

[Features](Features.md) answers *what exists*. This page answers *why it is built that way*.

Every entry follows the same four-part structure:

| Part | Question |
|---|---|
| **Pain** | What upstream UnLua v2.3.6 looks like here, and what it costs |
| **Mechanism** | What UnLuaNova does instead (down to files and symbols) |
| **Payoff** | What that buys, and how it is verified |
| **Limits** | When it does not apply, and what you pay |

!!! note "Inherited vs. added"
    UnLuaNova is a continuation of upstream, not a rewrite. Overrides, container bindings, the
    reflective call path, dead-loop detection and `ENABLE_PERSISTENT_PARAM_BUFFER` all come from
    upstream. This page marks additions explicitly and states the role of inherited pieces —
    claiming upstream work as our own is the fastest way to lose a reader's trust.

---

## At a glance

| Topic | Mechanism in one line | Upstream |
|---|---|---|
| [Static binding](#1-static-binding-move-type-lookup-from-run-time-to-build-time) | POD binding tables generated at build time; the read path looks up a table instead of reflection | Runtime reflection |
| [CallUEFast](#12-why-calluefast-only-saves-half) | Skips only "find the function"; marshalling is untouched | Callspace / override / interface lookup on every call |
| [Dual backends](#2-dual-backends-fence-the-vm-difference-into-one-header) | Differences absorbed by `LuaBackend.h`; branches keyed on capabilities | A single Lua version |
| [LuaScriptCompat](#21-luascriptcompat-scripts-do-not-need-two-dialects) | Fill in only what Luau lacks | N/A (one backend) |
| [VM hook dispatcher](#31-fix-the-foundation-first-the-vm-hook-dispatcher) | OR the masks of all consumers, install one native hook | Last writer overwrites the first |
| [CPU profiler](#32-cpu-profiler-into-unreal-insights) | Reuse the dispatcher + Insights traces, both sides gated by one runtime toggle | Compile-time `ENABLE_UNREAL_INSIGHTS` |
| [Dead-loop watchdog](#33-the-dead-loop-watchdog-and-the-thread-model-it-forced-us-to-fix) | One polling thread for the whole process; hook attached only when suspicious | One thread per env, never shut down |
| [Memory stats](#34-memory-stats-two-layers-of-data-two-questions) | Default allocator for totals, Luau allocation callback for breakdown | Totals only |
| [Debug service](#35-headless-debug-service) | Luau native breakpoints/stepping wrapped as a UI-less service | Requires luasocket + an external debugger |
| [Sandbox](#36-sandbox) | `luaL_sandbox` hardens globals, then hands back the writes we still need | None |
| [Script-side type checking](#4-fail-earlier-script-side-type-checking) | Export engine signatures to each backend's own analyzer | EmmyLua annotations only |

---

## 1. Static binding: move type lookup from run time to build time

### Pain

Upstream binding is **runtime reflection**: when Lua touches `self.SomeProperty` or calls
`self:SomeFunc()`, `FClassDesc` / `FFieldDesc` walk the reflection system to find the
`FProperty` / `UFunction` by name, then build descriptor objects before anything can be marshalled.

Two things soften that, and neither removes it:

1. **The result is cached into the metatable after first access** — but what gets cached is the
   *descriptor*, and building the descriptor is itself a reflective lookup.
2. **Call-time lookups stay** — every `UFunction` call still queries `GetFunctionCallspace`,
   checks whether Lua overrode it, and interface functions still do a per-object
   `FindFunctionByName`.

Fine for small projects. On projects with thousands of types, "look up by name" is paid on every
property access and every call.

### Mechanism

Move the lookup to build time; keep only the table lookup at run time. Four stages:

| Stage | What happens | Why it matters |
|---|---|---|
| **Generate** | The `UnLuaNovaStaticBindingGenerator` commandlet emits POD descriptor tables | Output is `static constexpr` read-only data — it never enters runtime init |
| **Register** | Each type gets one `UNLUANOVA_REGISTER_STATIC_BINDING(Type)`, which expands to a static `FStaticBindingAutoRegistrar` | Its constructor only does `Pending.Add` + `PendingCount.store` — **no UObject touched**, so running it during DLL load is safe |
| **Resolve** | `ResolvePending()` turns names into real `FProperty*` / `UFunction*` pointers | Triggered **lazily by the read path**, so it always happens after the UObject system is up |
| **Consume** | `FFunctionDesc` queries once at construction and snapshots into `FFunctionDesc::StaticBinding` | Later calls never touch the registry |

Two details are easy to miss:

- **Lazy resolution is a correctness requirement, not laziness.** `FProperty*` is only valid once
  the UObject system is live; resolving during static initialization would hand out null pointers.
  Registration enqueues; the first real read resolves.
- **Two fast paths on the read side.** `PendingCount` is a `std::atomic<int32>` checked **before**
  any lock; if it is 0 the resolve branch is skipped entirely, so the steady state after startup
  takes no write lock and runs no resolution. Actual reads take a **shared** `FRWLock` lock, not an
  exclusive one — the comment in `Public/StaticBinding/StaticBindingRegistry.h` explains why:
  large projects register thousands of types and then hit the read path on every property and
  function access, which an exclusive lock would fully serialize.

**Type selection is opt-out**: everything is exported by default (skipping
`SKEL_` / `REINST_` / `TRASHCLASS_` / `HOTRELOADED_` / `PLACEHOLDER-CLASS` prefixes, deprecated and
transient types). Projects only mark what they do *not* want with `meta=(UnLuaNovaNoExport)`;
marking a base class with `meta=(UnLuaNovaExport)` pulls its whole hierarchy back in.

### Payoff

- **Shorter hot path**: "walk the reflection chain" becomes "read a pre-resolved pointer".
- **Scale no longer multiplies per-access cost**: shared reads plus the atomic fast path mean type
  count affects memory, not per-access latency.
- **Adoption cost is inverted**: you no longer need a whitelist of "what to export" — you declare
  what to exclude. The worst case of a missed marker is a few extra exported types, not a type that
  suddenly stops being reachable from Lua.
- **Generated data is POD**, so it can sit in read-only memory and take no part in static
  initialization order.

### Limits

- **Three kinds of functions keep the reflective path** (the dispatch in `FunctionDesc.cpp`):
  net functions (need `GetFunctionCallspace` / `CallRemoteFunction`), latent functions (coroutine
  bookkeeping), interface functions (per-object `FindFunctionByName`). That is not unfinished work —
  they genuinely need those steps.
- **It is a build-time step, not a runtime switch.** Regenerate and commit when the type set
  changes, and put the output inside `Source/<Module>/Generated/` so UBT compiles it — anywhere
  else and the optimization simply is not there.
- **Generated tables are not committed**, so under a normal build the registry is empty and the fast
  path is never taken. That directly shapes the tests: `UnLuaNova.API.CallUEFast` uses two probe
  functions for the bound and unbound paths, because `FFunctionDesc` snapshots the binding at
  **construction** — one function cannot travel both paths in a single process.

---

### 1.1 Parameter caching (inherited from upstream)

The third cost on the call path is parameter memory. Upstream's `ENABLE_PERSISTENT_PARAM_BUFFER`
is already the answer: `FParamBufferAllocator_Persistent` keeps a pool of blocks per `UFunction`,
`Get()` borrows one via a counter and `Pop()` just rewinds the counter — removing a
`FMemory::Malloc` + `Memzero` + `Free` per call.

UnLuaNova keeps it on by default (turned into a compile-time macro by `UnLuaNova.Build.cs`).
It is **not** an addition; it is listed here so the cost picture is complete: static binding removes
"find the type", `CallUEFast` removes "find the function", parameter caching removes "allocate".

---

### 1.2 Why `CallUEFast` only saves half

### Pain

With pre-resolved descriptors, the tempting mistake is to bypass marshalling altogether. Do that and
return values, out parameters, and container/struct lifetime semantics diverge from the reflective
path — surfacing as "some functions behave differently with the optimization on", which is
miserable to diagnose.

### Mechanism

The dispatch in `FFunctionDesc::CallUE` (`Private/ReflectionUtils/FunctionDesc.cpp`):

```cpp
// Pre-resolved descriptor available: take the fast path.
if (StaticBinding && !StaticBinding->Flags.bIsNet && !StaticBinding->Flags.bLatent && !bInterfaceFunc)
    return CallUEFast(L, NumParams, FirstParamIndex, Object, Userdata);
```

What `CallUEFast` **skips**:

- `GetFunctionCallspace()` (`ProcessEvent` handles `FUNC_Native` itself for non-net functions)
- override-redirection lookup (only needed when `Flags.bOverridable`)
- `FindFunctionByName` for interface functions

What `CallUEFast` **keeps**: `Buffer->Get()`, `PreCall`, `PostCall`, `DestroyParams`, `Buffer->Pop`.

### Payoff

**Observable behaviour is identical to the reflective path** — a design constraint, not marketing:
parameter marshalling and buffer management are the same code, so the two paths differ only in *how
the function is found*, never in *how it is called*. Therefore:

- correctness can be asserted directly by tests (`UnLuaNova.API.CallUEFast` asserts both paths
  produce the same result);
- when something breaks you can turn static binding off wholesale and fall back to reflection —
  behaviour stays the same, only speed changes.

### Limits

Only functions that are statically bound **and** outside those three categories take the fast path.
A project that never ran the commandlet hits it zero times.

---

## 2. Dual backends: fence the VM difference into one header

### Pain

Upstream is pinned to one Lua version. Switching VMs means either a sweeping rewrite or waiting for
someone else to do it. And "Lua 5.5 vs. Luau" is not a version bump: Luau is a *dialect* — **no**
userdata `__gc`, **no** `load` / `collectgarbage`, `__iter` instead of `__pairs`, 32-bit integers.
Semantic differences and API differences arrive together.

### Mechanism

`Public/LuaBackend.h` is the **only** place that absorbs VM differences, and says so bluntly:
everything else must go through `UNLUANOVA_*` macros and must not include `lua.h` / `lualib.h` /
`luacode.h` (both backends own those filenames).

Three decisions carry it:

1. **Branch on capabilities, not on backend id.**

    ```cpp
    #if UNLUANOVA_HAS_SANDBOX      // not: #if UNLUANOVA_LUA_BACKEND == LUAU
    ```

    The backend id only decides *which VM gets compiled*; capabilities decide *behaviour*. A third
    backend then only has to answer "which capabilities do you have", without editing any `if`.

2. **The capability macros are the inventory.**
   `UNLUANOVA_HAS_HOOK_EVENT`, `UNLUANOVA_HAS_NATIVE_VECTOR`, `UNLUANOVA_HAS_READONLY_TABLES`,
   `UNLUANOVA_HAS_SANDBOX`, `UNLUANOVA_HAS_BREAKPOINT`, `UNLUANOVA_HAS_ALLOC_CALLBACKS`,
   `UNLUANOVA_HAS_USERDATA_GC`, `UNLUANOVA_HAS_ITER_METAMETHOD` — a new Luau-only capability is
   registered here first, then consumed behind its macro.

3. **Three compile-time guardrails.** Both backends enabled → `#error`; the VM actually compiled
   does not match the selected backend → `#error`; wrong headers (`LUA_VECTOR_SIZE`,
   `sizeof(lua_Integer)`) → `#error` / `static_assert`. The last line of defence against the
   difference leaking elsewhere.

The script-side counterpart is `UnLuaNova.GetBackend()` / `UnLuaNova.HasFeature()`
(`Private/UnLuaNovaLib.cpp`):

```lua
if UnLuaNova.HasFeature('sandbox') then
    -- only take the branch that depends on it
end
```

`HasFeature` returns **`false` for unknown names rather than raising** — so scripts degrade safely
on older or newer builds.

### Payoff

- **Switching backends is one ini key** (`LuaBackend` in `Config/DefaultUnLuaNova.ini`).
  It is a compile-time choice, so you rebuild — and touch no business code.
- **Both VMs are git submodules**: sources belong upstream, wrappers (`*.Build.cs`, the single-TU
  aggregate, Luau's generated wrappers) belong to this project. Upgrading is moving a pointer.
- **Capability gaps are visible to scripts**, which ask "is there a sandbox?" rather than
  "am I on Luau?".

### Limits

- **The two backends cannot coexist in one build** (same symbol names, same `lua.h`), hence a
  compile-time choice rather than a runtime switch.
- **Some capabilities are single-backend**: sandbox, native breakpoints, categorized allocation
  stats, native `vector` / `buffer`. On Lua 5.5 these are **absent**, not degraded — both the docs
  and `HasFeature` report that honestly.

---

### 2.1 LuaScriptCompat: scripts do not need two dialects

### Pain

With a Luau backend, the first practical problem is that Luau's base and `debug` libraries are
subsets. The canonical case (Issue291): Luau has no `collectgarbage`, so scripts fail with
`attempt to call a nil value`. Hot reload depends on `debug.getupvalue` / `setupvalue` /
`upvalueid`, which Luau only partially provides.

### Mechanism

`Private/LuaScriptCompat.cpp` (whole file inside the Luau branch), installed by
`UNLUANOVA_SETUP_SCRIPT_COMPAT(L)` **before** `UnLuaNovaLib::Open`:

- patches the `debug` table: `getupvalue` / `setupvalue` / `upvalueid` / `getlocal` / `setlocal` /
  `getregistry` / `getuservalue` / `setuservalue` — **only when missing**, never overwriting;
- adds `load` (`LuaLoad`) and `collectgarbage` (`LuaCollectGarbage`, supports collect/step/count);
- **replaces** `pairs` when it is too narrow, keeping the original as an upvalue: tables go native,
  non-tables fall through to `__iter`. Required — Luau's `pairs` accepts only real tables, so
  `pairs(TArray/TMap/TSet)` would error.

### Payoff

Script authors write to Lua intuition: `pairs(container)`, `load`, `collectgarbage` work on both
backends, and **one copy of the game scripts is enough**.

### Limits

It fills the half Luau is missing; it cannot bridge differences running the other way.
`continue`, compound assignment and type annotations are syntax errors on Lua 5.5 — scripts must
avoid them or guard by backend (which is what `Content/Script/Luau/Index.lua` does). And Luau has
**no userdata `__gc`**, so container userdata is never finalized: do not hang resource release on
`__gc`.

---

## 3. Observability: Lua is no longer a black box

Upstream is not empty here — dead-loop detection (#428), allocation accounting, and a compile-time
`ENABLE_UNREAL_INSIGHTS` all exist. The real problems are three:

1. Getting Insights data required a **rebuild** (compile-time macro);
2. several features fought over **one VM hook slot**, the last writer won, and the loser
   **failed silently**;
3. data stopped at totals, so it could not answer "where did the memory go".

### 3.1 Fix the foundation first: the VM hook dispatcher

### Pain

A Lua VM exposes exactly **one** hook slot: `lua_sethook()` on plain Lua,
`lua_Callbacks::interrupt` on Luau. The CPU profiler and the dead-loop watchdog both need it, so
the last writer overwrites the first.

This is not theoretical — the comment in `Private/LuaVmHooks.h` records it happening:

> Two features that both need it - today the CPU profiler and the dead-loop watchdog - would
> otherwise overwrite each other's callback, and the loser silently stops working. That used to be
> real: the watchdog checked `lua_gethook()` first and quietly did nothing whenever the profiler
> had it.

Failing silently is the worst shape: the watchdog is on, does nothing while the profiler runs, and
prints nothing about it.

### Mechanism

`FLuaVmHooks` (`Private/LuaVmHooks.h`, header-only):

- one slot per `lua_State*`, holding a `TArray<FEntry>` of `{Mask, Owner, Callback}`, where `Owner`
  is an opaque `const void*` (usually `this`);
- `Add` / `RemoveOwner` / `RemoveAll` all funnel into **`Refresh()`**, which **ORs** every
  listener's mask into one composite mask and installs **a single native hook**
  (`lua_sethook(L, &Dispatch, Mask, 0)` on Lua, `lua_callbacks(L)->interrupt = &OnInterrupt` on Luau);
- `Dispatch` restores the event into mask space via `1 << Ar->event` and filters per listener;
- teardown is "**remove by owner, then recompute**" — never "set the hook to null";
- on Luau `Ar` is `nullptr` (interrupts fire at safe points and carry no `lua_Debug`), so listeners
  that need a frame call `UNLUANOVA_GETINFO` themselves.

### Payoff

- **N consumers coexist.** Profiler and watchdog can run together; a third one (coverage, say) just
  calls `Add` without knowing who else is installed.
- **Exactly one write site** for the hook slot, so "who clobbered my hook" becomes structurally
  impossible.
- **Safe withdrawal**: `RemoveOwner` matches on the owner pointer, so a feature keeps no handle.

### Limits

The composite mask is a **union**: if any consumer wants line events, everyone gets line callbacks
and filters locally. The dispatcher fixes clobbering, not cost.

---

### 3.2 CPU profiler into Unreal Insights

### Pain

Upstream's `ENABLE_UNREAL_INSIGHTS` is a **compile-time macro**: profiling meant a rebuild, and it
covered the UE side only — time spent inside Lua functions stayed invisible. "This is slow" was a guess.

### Mechanism

`UnLuaNova::FLuaProfiler` in `Private/LuaProfiler.cpp`:

1. **does not call `lua_sethook` itself** — it calls `FLuaVmHooks::Get().Add(...)`
   (`LUA_MASKCALL | LUA_MASKRET` on Lua; mask 0 on Luau, going through interrupt). It is the
   dispatcher's first customer;
2. **registers each event name once** — `FCpuProfilerTrace::OutputEventType()` turns
   `Name [Source:linedefined]` into a cached spec id, later hits only emit `OutputBeginEvent` /
   `OutputEndEvent` with that id;
3. **keeps the cache key stable across backends** — `UNLUANOVA_DEBUG_PROTOID` takes `protoid` on
   Luau and `source << 32 | linedefined` on Lua;
4. **gates the UE side with the same toggle** — `CallLua` (both overloads), `CallUE` and `CallUEFast`
   in `FunctionDesc.cpp` each carry
   `TRACE_CPUPROFILER_EVENT_SCOPE_TEXT_CONDITIONAL(*FuncName, FLuaProfiler::Get().IsEnabled())`.

### Payoff

- **One `lua.profiler on` lights up both sides** — the code comment says it outright: "no rebuild,
  and no second setting to remember". Toggling is runtime, no recompilation.
- **The cross-language call chain is intact**: Insights shows `UFunction → Lua function → UFunction`
  nesting instead of two unrelated samples. That comes directly from putting UE-side scopes and
  Lua-side events on one timeline.
- **When off, the UE scopes are gated out too** (the `_CONDITIONAL` variant) — not "still collected,
  just not looked at".

### Limits

- Luau's ` lua_Debug` has **no `event` field**, so call/return cannot be paired and each safe point
  is reported as a **zero-length event**: Insights shows hit density per function, not timed spans.
- The whole block sits inside `#if CPUPROFILERTRACE_ENABLED`, which Shipping builds typically omit.

---

### 3.3 The dead-loop watchdog, and the thread model it forced us to fix

### Pain

Dead-loop detection itself is upstream (#428). In our tree it had two real engineering problems:

1. **It fought the profiler for the hook slot and silently lost** — fixed in
   [3.1](#31-fix-the-foundation-first-the-vm-hook-dispatcher);
2. **Its thread never exited**: `FDeadLoopCheck` started a `FRunner` polling once per second, but
   `FRunner::Stop()` had no callers anywhere and `~FDeadLoopCheck` did not exist. So
   `delete DeadLoopCheck` in `~FLuaEnv` only deleted the shell: **every Lua environment leaked a
   thread that still pointed at a destructed `Env`**. The thread was started even when the feature
   was off (default), so tests leaked one per env.

### Mechanism

- **One shared thread for the whole process**: `static TUniquePtr<FRunner> GDeadLoopRunner` in
  `LuaDeadLoopCheck.cpp`, with the reason in the comment — a thread per env is what exhausted the
  engine's TLS thread pool and hung the editor mid-suite. Added `~FDeadLoopCheck` →
  `FRunner::Shutdown()` (`Stop()` + `WaitForCompletion()`).
- **Detection**: the polling thread sleeps 1s and increments `TimeoutCounter` only while
  `GuardCounter > 0` (script actually executing); reaching `Timeout` seconds trips it.
- **Trip**: `FGuard::SetTimeout()` installs a LINE hook through `FLuaVmHooks::Get().Add(...)`, and
  the callback does `RemoveOwner` then `luaL_error(InL, "lua script exec timeout")`.
- **Zero cost when disabled**: with `Timeout == 0` (the default), the thread is not even created.

### Payoff

- **Near-zero steady-state cost**: the polling thread only counts while a script runs, and installs
  no hook; the hook appears only once things look suspicious. That is an order of magnitude away from
  a permanent line hook.
- **A timeout is a Lua error, not a hang**: `luaL_error` is `pcall`-able and carries a Lua callstack,
  handled exactly like any other script error.
- **No thread leak**, so multi-environment (multi-GameInstance) setups and long test suites stop
  degrading.

### Limits

It works through VM hooks, so it can only catch loops in **bytecode**. A loop stuck inside a C++
call still freezes the process.

---

### 3.4 Memory stats: two layers of data, two questions

### Pain

Upstream has allocation accounting, but only totals. Totals answer "how much"; they cannot answer
"**where did it go**" — script tables, strings, or userdata churned out by one type?

### Mechanism

`Private/LuaMemoryStats.cpp` keeps two data sources deliberately separate:

| Layer | Source | Measures | Backends |
|---|---|---|---|
| **Totals** | `FLuaEnv::DefaultLuaAllocator` (every backend routes through it) → `TrackAlloc/TrackFree` | current bytes, peak, alloc/realloc/free counts | both |
| **Breakdown** | Luau-only `lua_callbacks->onallocate` | Top N per memory category / Lua type / userdata tag | Luau only |

Breakdown uses **fixed-size atomic arrays** (256 / 32 / 128 buckets) rather than hash maps — the
allocation callback is on the hot path, and the hot path wants array indexing. Peak is updated with a
relaxed CAS loop so accounting never introduces lock contention.

### Payoff

- **The two layers answer different questions**: totals are capacity (do we need to grow, did GC
  run), breakdown is attribution (who is churning).
- **Breakdown reaches concrete types**: the userdata-tag dimension shows directly whether it is
  `TArray` or `FVector` that is growing.

### Limits

- **Breakdown measures churn, not live bytes.** Luau's `onfree` callback carries **no size**, so the
  breakdown cannot subtract; use the totals section for live and peak. Both the docs and the command
  output state this.
- Lua 5.5 has no allocation callback (`UNLUANOVA_HAS_ALLOC_CALLBACKS == 0`); `lua.memstats` prints
  `Categorized breakdown requires the Luau backend.` and skips — it does not pretend to have measured.

---

### 3.5 Headless debug service

### Pain

Upstream needs an external debugger (LuaPanda / LuaHelper) for breakpoints, which depends on
`luasocket` and on dropping `require("LuaPanda").start(...)` into the script. Debugging therefore
depends on an **optional extension plugin**, and script code carries debug scaffolding.

### Mechanism

`Private/LuaDebugService.cpp` wraps Luau's native breakpoint primitives as a **UI-less** service:

- **Breakpoints / stepping** go through Luau's separate channels
  (`lua_callbacks->debugbreak` / `debugstep`), so they never collide with the `interrupt` slot shared
  by the profiler and the watchdog;
- **Callstack / locals** are fetched per backend: `lua_getinfo(L, Level, "sln")` on Luau,
  `lua_getstack` + `lua_getinfo("nSl")` on Lua, up to 128 frames; locals walk `lua_getlocal` over
  indices 1..63;
- **"Headless" is deliberate layering**: no socket, no JSON, no UI of its own — just a C++ API for
  console commands, automated tests and a future DAP adapter to drive.

### Payoff

- **No dependencies**: no plugin, no `luasocket`, no script edits.
- **Testable**: the capability is a C++ API, so tests can assert "breakpoint registered",
  "step hit and captured a callstack".
- **Zero cost at rest on Luau**: `Attach()` runs only from the `lua.dbg` command; stepping is a
  **template parameter** — with `L->singlestep == false` Luau runs `luau_execute<false>` and the
  per-instruction hook branch is compiled away; `debugbreak` only fires on `LOP_BREAK`, and BREAK
  instructions exist only where `lua_breakpoint` inserted them.

### Limits

- **Not zero-cost on Lua 5.5**: with no native primitives the service degrades to `FLuaVmHooks` +
  `LUA_MASKLINE`, a real per-line hook.
- The `lua_breakpoint` wrapper exists but the service currently takes the "hook + source-line match"
  route (`MatchesBreakpoint`).
- A **DAP adapter** to plug this into an IDE is still missing.

---

### 3.6 Sandbox

### Pain

Hot-reloaded or downloaded scripts share one globals table with local scripts. A single `UE = nil`
or `UE.UObject = somethingElse` can detach engine bindings, and the failure surfaces far from the
line that caused it.

### Mechanism

`Public/UnLuaNovaSandbox.h`: the `lua.sandbox` CVar is read once per Lua environment at creation,
and when set calls **`luaL_sandbox(L)`** (Luau native), which makes the globals table and its
attached tables read-only.

Then comes the step that is easy to miss: `RestoreTablesAfterSandbox`
(`Private/UnLuaNovaLib.cpp`) **hands write access back** to `UE` / `UE4` / `Lua` / `UnLua` /
`UnLuaNova` / `package` and `package.loaded`.

The reason: UE types are registered **lazily** — `UE.XXX` is written the first time it is accessed.
Freezing `UE` along with everything else would make the next new-type access and the next `require`
fail outright, turning the sandbox into "enable it and nothing works".

Script-side writes are still blocked by the `UE` namespace's own `__newindex`
(`UELib::SetNamespaceWritable`) — **and that one works on both backends**.

### Payoff

- **Protects against accidents without breaking features**: the "script rewrites a global" route is
  closed while lazy registration and `require` keep working.
- **Protection is not weakened**: what is handed back is the C++-side write permission; script-side
  writes are still intercepted by `__newindex`.

### Limits

- **Off by default** (`lua.sandbox=0`) because it changes behaviour for existing scripts; set it in
  an ini so it applies at startup.
- **It is a startup CVar, not a runtime command**: changing it later does not affect live environments.
- **Luau only** (`UNLUANOVA_HAS_SANDBOX`). The CVar is registered on both backends so the command
  never "does not exist", but writes do nothing on Lua 5.5 — whether it really took effect is
  answered by `UnLuaNova.HasFeature('sandbox')`.
- **Class tables are not frozen**: only the globals table is frozen plus script writes to `UE` are
  blocked. Class tables are lazily cached (`rawset` on first access), so freezing would break caching.

---

## 4. Fail earlier: script-side type checking

### Pain

UE API misuse in Lua (misspelled function name, wrong argument count, non-existent property) is
found at **run time** upstream — and usually as a silently propagated `nil` that blows up much later.

### Mechanism

Engine types are reflection-visible, so their signatures can be exported; script-side APIs are
runtime-injected Lua tables that reflection cannot see, so they must be hand-written. Hence two
artefacts, one per analyzer:

```
Plugins/UnLuaNova/IntelliSense/
├── EmmyLua/    Lua dialect   → lua-language-server (LuaLS)
└── Luau/       Luau dialect  → luau-lsp
    └── UnLuaNova.d.lua
```

- Reflected types are exported by `UUnLuaNovaIntelliSenseCommandlet` +
  `FUnLuaNovaIntelliSenseGenerator`;
- script-side API declarations come from the committed handwritten
  `Content/IntelliSense/UnLuaNova.d.lua`, concatenated in.

### Payoff

- **Some UE API misuse moves from run time to "compile time"**: `--!strict` plus `luau-lsp`
  reports it while you type.
- **Mapping rules are explicit**: integer widths and floats collapse to `number`,
  `FString`/`FName`/`FText` to `string`, `TArray<T>` → `{T}`, `TMap<K,V>` → `{[K]: V}`,
  `TSet<T>` → `{[T]: boolean}`, with delegates and other inexpressible types falling back to `any`.
- **Results land in the test report**: `Tools/RunTests.ps1` dispatches per backend
  (`luau-lsp` for Luau, LuaLS for Lua), and reports "skipped + reason" when the tool is absent
  instead of pretending it ran.

### Limits

- **Each backend uses its own analyzer — a syntactic fact, not a preference**: Lua 5.5 cannot parse
  Luau annotations, and Lua-backend scripts cannot be fed to `luau-lsp`. There is no shared one.
- **The `UE` global is currently an opaque table**: its ~18k members trip Luau's complexity limit,
  and scripts write `UE.FVector` while the declaration is named `Vector`. Not yet solved.
- `luau-lsp` is a **local step** — CI has no binary for it.

---

## 5. The principle behind the small changes

A few small changes are worth calling out, because they express the same rule:
**error messages must point at the real cause, and semantics must line up with C++.**

### `pairs(TSet)` yields `(element, true)`

`TSet_Pairs` in `Private/BaseLib/LuaLib_Set.cpp` does not hand out slot indices because `FScriptSet`
is **sparse** — after deletions there are holes, and a slot index means nothing. Returning
`(value, true)` makes `for k in pairs(Set)` match C++ range-for semantics. The cross-backend
difference is absorbed by `UNLUANOVA_ITER_ENTRY` (`__iter` on Luau, `__pairs` on Lua; identical
protocol, one C function).

### `TMap:Contains(Key)` (aligning with `TArray` / `TSet`)

With the method missing, `Map:Contains(2)` walked into the `__index` self-reference chain and
reported `'__index' chain too long; possible loop` — which says nothing about the real cause,
a name that does not exist. Separately, `Find` returning `nil` is ambiguous: either "no such key" or
"the value is nil".

### String extensions split on literals

`split` / `count` in `Content/Script/UnLuaNova/StringEx.lua` call `s:find(sep, Start, true)` — the
fourth argument `plain=true` disables pattern matching. So `("a.b.c"):split(".")` yields three
segments, not an empty table. Written to Lua 5.1 semantics, so both backends agree.

---

## 6. Costs and known limits

Collected here so "should we use this" can be answered in one pass:

| Item | Status |
|---|---|
| Static binding | Build-time step; regenerate and commit when the type set changes. Net / latent / interface functions keep the reflective path |
| Backend switching | Compile-time choice requiring a rebuild; the two backends cannot coexist |
| Luau-only features | Sandbox, native breakpoints, categorized allocation stats, `vector` / `buffer` are **absent** on Lua 5.5 (not degraded) |
| Luau semantics | No userdata `__gc`; `lua_Integer` is 32-bit; stricter yield boundaries |
| Profiler | No `event` field on Luau → zero-length events, not timed spans |
| Memory breakdown | Measures churn, not live bytes |
| Debug service | Headless, no DAP adapter yet; on Lua 5.5 it is a real per-line hook |
| Type checking | Separate analyzers per backend; the `UE` global is still an opaque table |
| `buffer` ↔ `TArray<uint8>` | Explicit per-byte conversion today; zero-copy not done |

---

## Further reading

- [Features](Features.md) — the "what exists" list
- [Architecture](Architecture.md) — layering, the full static binding chain, the dual-backend abstraction
- [Static Export and Binding](StaticExportBinding.md) — commandlet usage and opt-out rules
- [Luau Backend](Luau.md) — Luau-only capabilities and behavioural differences
- [Console Commands](ConsoleCommand.md) — `lua.profiler` / `lua.memstats` / `lua.dbg` / `lua.sandbox`
- [Testing and Automation](Testing.md) — which cases cover which capability

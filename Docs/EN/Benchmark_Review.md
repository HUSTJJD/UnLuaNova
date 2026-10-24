---
title: "Benchmark Review: Current Health and Next Steps"
description: "Second-round comparison. Feature richness, performance, usability, multi-backend and backend features, testing and engineering - each item with evidence, a verdict and a direction."
---

# Benchmark Review: Current Health and Next Steps

This is the **second round** after [Comparison with slua-unreal](Comparison_sluaunreal.md),
and it asks a different question:

* round one asked "**what does the competition have that we lack**", and landed features;
* this round asks "**how good is what we already claim to have**", and looks for problems.

Method: ignore what the docs say, look only at what the code does. Every conclusion carries a
`file:line` so it can be re-checked; anything that cannot be verified inside this repo (public
claims by slua / puerts) is marked `unverified`.

> When this page was written the repo was mid-way through two refactors (the "bucket by type +
> push model" network payload, and the static binding thunk). That does not change the
> conclusions, but it explains part of the "docs older than code" phenomenon.

---

## Zero. The one-sentence conclusion

**Feature richness is no longer the shortfall. The shortfall is in substance and in engineering.**

In three sentences:

1. **There is a real memory leak on the default backend (Luau)** - userdata for containers /
   FText / File is **never `__gc`'d** (`Public/LuaBackend.h:748`). This is not "a missing
   feature", it is a defect, and it sits on the default path.
2. **Not one of our published performance numbers is reproducible** - the numbers exist only
   in the docs; benchmark output is ignored by `.gitignore:27` and has never been committed;
   and the benchmark cases themselves only assert `elapsed > 0`, so **a performance regression
   can never turn a test red**.
3. **518 cases run on a laptop, not in CI** - `.github/workflows/ci.yml` only does
   `py_compile`, PowerShell syntax parsing and `mkdocs build`. The C++ automation has never
   run automatically.

The remaining problems (profiler self time permanently 0 on Luau, sandbox off by default,
`ToVector` behaving differently across backends, ghost APIs in the docs) are all the same kind:
**the capability was built, but it was never made real.**

---

## One. Multi-backend: the biggest selling point, and the biggest unkept promise

Dual backends (Lua 5.5.1 + Luau) are the only dimension in the comparison where "the
competition has nothing at all". Taken apart, the selling point rests on several unfinished
places.

### 1.1 [P0 · defect] Userdata is never collected on Luau

```cpp
// Public/LuaBackend.h:742-748
 * Luau's substitute, lua_newuserdatadtor(), is NOT wired in yet: every __gc
 * here ends in ContainerRegistry::Remove(), which goes back through FLuaEnv for the
 * main state and then touches the Lua stack, and that is not safe from a GC callback.
 * Closing the gap needs a different cleanup design - explicit release, or a sweep owned
 * by the env - not a rename. Until then Luau simply never finalizes these userdata.
#define UNLUANOVA_HAS_USERDATA_GC 0
```

Affected registrations: `TArray` (`BaseLib/LuaLib_Array.cpp:591`), `TMap` (:385), `TSet`
(:282), `FText` (`LuaLib_Text.cpp:53`), file handles (`LuaLib_File.cpp:486`).

**Why it is first**: Luau *is* the default backend (`UnLuaNova.Build.cs:141`, and Luau is also
chosen when the ini key is missing), so this is **a leak on the out-of-the-box path**, not a
defect under some obscure configuration. The comment also says this is not solved by renaming a
function - `ContainerRegistry::Remove()` would reach back into `FLuaEnv` and touch the Lua
stack from a GC callback.

**Direction**: move ownership of the cleanup; do not do it inside the GC callback. Two routes:
(1) explicit release (a `Release()` next to `UE.TArray`, or refcount-driven reclamation);
(2) a weak table owned by `FLuaEnv`, swept once per frame or per GC cycle. With (2), the
"churn vs live" accounting of `lua.memstats` has to change with it, or you cannot tell whether
the fix worked.

### 1.2 [P1] The profiler is sampling-only on Luau

`Private/LuaProfiler.cpp:73-79, 267-275`: Lua 5.5 uses `LUA_MASKCALL|LUA_MASKRET` to get real
call/return intervals; Luau goes through safe-point callbacks, so **self time is permanently 0
and `Hits` counts safe points, not calls**.

The comparison table says "CPU profiler ✅ even". On the default backend it is half broken, and
`Docs/EN/ConsoleCommand.md` does not say so clearly - a user who turns the profiler on
following the docs gets a table of all-zero self times and assumes they used it wrong.

### 1.3 [P1] The sandbox is off by default

`Private/LuaEnv.cpp:191-195`: `lua.sandbox` defaults to **0**, and it can only really take
effect on Luau (`UNLUANOVA_HAS_SANDBOX`). The comparison table's "sandbox / read-only globals
- we are stronger" **does not hold under the default configuration**. Off by default is the
right call (hardening the global table is a behaviour change for existing scripts), but it means
this advantage only exists once a user turns it on, and the docs should say so plainly.

### 1.4 [P1] Behaviour differs across backends, not just "how many features"

| Capability | Luau | Lua 5.5 | Evidence |
|:---|:---|:---|:---|
| userdata `__gc` | ❌ | ✅ | `LuaBackend.h:748/758` |
| Real breakpoints / stepping | ⚠️ line matching (`lua_breakpoint` not wired, see below) | ⚠️ degrades to line matching | `LuaDebugService.cpp:77-99, 241` |
| Profiler call intervals | ⚠️ sampling only | ✅ | `LuaProfiler.cpp:73-79` |
| Categorised memory stats | ✅ | ❌ | `LuaMemoryStats.cpp:66-98` |
| Sandbox | ✅ | ❌ | `LuaEnv.cpp:196-213` |
| Native `vector` / `buffer` | ✅ | ❌ | `LuaBackend.h` `LUA_VECTOR_SIZE` |
| Generational GC | ❌ incremental | ✅ `LUA_GCGEN` | `LuaBackend.h:513-524` |
| Dead loop detection | ✅ interrupt point | ✅ `LUA_MASKLINE` | `LuaDeadLoopCheck.cpp:166-171` |

The one that **should be treated as a bug rather than a difference**:
`MathLib/LuaLib_FVector.cpp:34,76,138` - on the Lua backend `ToVector` **returns nil**. The
same script producing different results on the two backends is far worse than "missing a
feature".

`HasFeature()` currently answers 5 backend capabilities (`sandbox` / `breakpoint` /
`allocCallbacks` / `vector` / `buffer`, `Private/UnLuaNovaLib.cpp:161-165`). **Userdata
reclamation and profiler precision cannot be asked about**, and those are precisely the two
that matter most to a project.

> Addendum (found while writing `DebugService.spec.cpp`, since fixed): the "real breakpoints
> ✅" cell above **was wrong**. Luau's debug hook only fires at `LOP_BREAK` or a single step
> (`lvmexecute.cpp:3338`), and `AddBreakpoint()` only ever appended to a `TArray` - **it never
> called `lua_breakpoint()`**. On the default backend `lua.dbg bp` and DAP `setBreakpoints`
> therefore **never hit at all**. It now works by arming `lua_singlestep` while any breakpoint
> exists, so the hook fires per line and the service matches source:line itself - the same
> mechanism the Lua backend uses. The cost is one hook per line for as long as a breakpoint
> session is armed; the gain is that "real breakpoints" went from a claim that did not hold to
> a stop that actually happens. Using the VM's own breakpoints would first require finding the
> Proto for a (source, line) pair - a separate piece of work.

### 1.5 [P1] Dependency versions are not reproducible

* the ini says `LuaVersion=lua-5.4.3` (`Config/DefaultUnLuaNova.ini:88`) while the vendored
  version is actually **5.5.1** (`ThirdParty/LuaLib/Lua/lua.h:20-22`), with a comment saying
  "display only";
* Luau has **no version record at all** - only `LBC_VERSION_MAX=14` from
  `Luau/Bytecode.h:538-545` and "since upstream release/743" from `Luau.Build.cs:59`.

Consequence: when upgrading Luau you cannot answer "from which point to which point", and a
regression cannot be attributed. **Direction**: write the exact version (tag / commit) of both
backends into `Docs/EN/Luau.md` and into a `Source/ThirdParty/VERSIONS.md`, and make the
editor's "about" page read that rather than the display string in the ini.

---

## Two. Performance: there is measurement, there is no yardstick

### 2.1 [P0] The numbers live only in the docs

Every number in `Docs/EN/Performance_Parity_slua.md` (our 0.459 µs / 1.439 µs, slua's 6.0x /
4.2x, thunk coverage 8123/11044) **has no artifact in the repo to support it**:

* benchmark output goes to `Saved/Benchmark/latest-<Backend>.csv`, and `.gitignore:27` ignores
  `/Benchmark`;
* `Tools/baseline/{lua,luau}.json` is a **case-name baseline** (`"total": 518`), not a
  performance baseline.

So: those numbers are the result of one local run, copied into the docs. **They cannot be
re-checked, and they cannot be used for regression.** The comparison draws conclusions from
them at its own risk - the docs do carry a warning ("two machines, two engine versions... do
not compare numbers directly"), but a warning is not a solution.

### 2.2 [P0] Performance tests cannot fail

The assertions in `Private/Specs/Benchmark.spec.cpp` and `BenchmarkParity.spec.cpp` are "ran N
iterations and elapsed > 0". **No upper bound, no comparison against a baseline.** So even if a
commit dragged "Lua calling a UE function" from 1.44 µs to 14 µs, all 518 cases stay green.

### 2.3 [P2] Thunk coverage is 73.6%, and the hottest paths are outside it

Static binding thunks exclude: net / latent / interface / out-ref / C++ default parameters /
`ParmsSize > 256` / more than 8 parameters - all of which are correct exclusions. But
`IsThunkableParamKind` has **no Struct, no Array/Map/Set, no Text, no Enum**
(`Public/StaticBinding/StaticBindingThunk.h:18-22, 41-55`).

In other words: **container element access and struct marshalling always go through
reflection.** Our own benchmark shows this too - `Lua.Container.PointsAreCoplanar`
(`TArray<FVector>` construction + marshalling) is a case of its own precisely because it takes
a different route.

There are two more costs paid on every call on the reflection hot path:

* `FFunctionDesc::CallUE` calls `Object->GetFunctionCallspace()` every time
  (`ReflectionUtils/FunctionDesc.cpp:228`);
* `ENABLE_TYPE_CHECK` is on by default (`UnLuaNova.Build.cs:115`) -> one
  `CheckPropertyType` virtual call per parameter (`FunctionDesc.cpp:413`).

Good news: the parameter buffer is pooled (`ENABLE_PERSISTENT_PARAM_BUFFER` on by default), so
no malloc per call.

### 2.4 [P2] Strings are converted on every call

`StaticBindingThunk.h:113-120`: String / Name parameters still do `UTF8_TO_TCHAR` plus
constructing an `FName`/`FString` inside the thunk - once per call. For functions with
high-frequency string parameters (UI, logging, network messages) that adds up.

---

## Three. Usability and engineering

### 3.1 [P0] No CI runs the tests

`.github/workflows/ci.yml` does three things: `py_compile` on the report generator, PowerShell
syntax parsing under `Tools/`, and `mkdocs build --strict`. Its own comment explains that a UE
build cannot run - and so **the C++ automation has never been executed automatically**;
`Tools/RunTests.ps1` and `Tools/Install-NightlyTask.ps1` are both local / scheduled-task only.

Consequence: 518/518 green means "somebody ran it", not "it is green on every commit". The
comparison table's "automated tests - we are stronger" is strictly about **the number of
cases**, not about **a gate**.

**Direction**: run one nightly on a self-hosted runner (even with a single backend), and gate on
the case count in `Tools/baseline/*.json` (turn red when it drops). This is the easiest thing to
add and the one that most changes whether tests are an asset or decoration.

### 3.2 [P1] Debugging exists only in the editor

`lua.dap` lives in the `UnLuaNovaEditor` module -> **no DAP in a packaged build**; `lua.dbg` is
in the runtime module and works packaged. There is **no copyable `launch.json`** in the repo
(`Docs/EN/Debugging.md:26` describes it but gives no configuration). Also, DAP's `evaluate`
**does not include upvalues**, and in a Lua project the logic often lives in upvalues
(especially with `HotReload.lua`).

### 3.3 [P2] Docs and code are out of sync

| Symptom | Location |
|:---|:---|
| `API.md` still documents `FLuaContext` / `GLuaCxt` (0 hits in code) and the deleted macro `SUPPORTS_RPC_CALL` | `Docs/EN/API.md` |
| licence says MIT, it is actually Polyform Small Business 1.0.0 | `Docs/index.md`, `mkdocs.yml:145` |
| "engine version 4.17-5.x" while the baseline is UE 5.8.3 | `Docs/EN/Features.md` |
| 5 pages are Chinese only (including `LuaNetworking` / `StdLib` / the two comparison pages that this page depends on) | `Docs/` |
| `EN/Testing.md` is newer than `CN/Testing.md` (CN lacks `-FailOnTypeErrors`) | `Docs/` |
| ~~tutorial `13_AnimNotify` has a map but no `13_*.lua`~~ | ~~`Content/Tutorials/`~~ **corrected**: the script is `Content/Script/Tutorials/AN_FootStep.lua`, named after the AnimNotify class it binds (`UnLuaNova.Class()` binds `AN_FootStep`), not after the numbered folder. Another conclusion made without checking the files |
| docs are latest-only, no version selector | `mkdocs.yml` |

For a project that sells "documentation quality", these are self-inflicted wounds.

### 3.4 [P2] Multi-env debugging is missing

`ULuaEnvLocator_ByGameInstance` can produce several `FLuaEnv`s, but `lua.dbg` / `lua.dap` work
against exactly one. The previous round already listed it as "neither side has it, we can move
first"; it was still not done.

---

## Four. Feature richness: the net position

Legend: ✅ have it and made real ｜ ⚠️ have it with caveats ｜ ❌ missing ｜ `unverified` nothing to compare against in this repo

| Dimension | UnLuaNova | slua-unreal | Verdict |
|:---|:---|:---|:---|
| Blueprint reflection binding | ✅ | ✅ | even |
| Static codegen | ✅ Commandlet | ✅ libclang | even (different route; we do not maintain clang) |
| Compile-time thunk | ⚠️ 73.6% coverage, no containers/structs | ✅ CppBinding template expansion | **still behind** (G5 phase two) |
| Lua-declared RPC | ✅ landed, C/S automation can verify | ✅ | **closed** |
| Lua-declared replicated properties | ⚠️ landed, bucketed by type, conditions per type | ✅ incl. arrays / nested structs | small gap |
| Typed C++ call into Lua | ✅ | ✅ | even |
| Overriding blueprint events + `self.Overridden` | ✅ opcode injection | ⚠️ | we are stronger |
| Containers / delegates / enums / structs | ✅ | ✅ | even |
| Extension methods "without C++" | ❌ needs `UnLuaNovaEx.h` | ✅ | small gap (G6) |
| **Multiple language backends** | ✅ Lua 5.5 + Luau | ❌ | **we are stronger** |
| Userdata reclamation on Luau | ❌ | — | **own defect** (1.1) |
| CPU profiler | ⚠️ self time always 0 on Luau | ✅ home-grown tool | **substantially behind** (1.2) |
| Memory profiling | ✅ categorised | ✅ | even |
| Debugger | ⚠️ in-editor DAP, no upvalues | ✅ dedicated plugin | small gap |
| Static type checking | ✅ `.d.lua` + luau-lsp (offline) | ⚠️ | we are stronger |
| Sandbox | ⚠️ off by default | ❌ | we are stronger (needs turning on) |
| Hot reload | ✅ Yunfeng scheme | ❌ | we are stronger |
| Automated tests | ✅ 518 cases / both backends | ❌ | case count ours, **zero gates** (3.1) |
| Engine version | ✅ UE 5.8.3 | 4.18/4.26/5.1 | we are stronger |
| Hot-patch packages / versioning / rollback | ❌ | ❌ | open ground, winnable |
| Attributing Lua memory by source | ❌ | ❌ | open ground, winnable |
| Coroutine leak detection | ❌ | ❌ | open ground, winnable |

> puerts (TypeScript / V8) is the other major competitor in the same lane, pushing static typing
> and VSCode debugging, which overlaps head-on with our Luau route. **There is nothing in this
> repo to compare against**, so it is not in the table - doing it would require a
> source-available comparison first, otherwise it is copying conclusions again.

---

## Five. Testing: where the 518 cases are

The count is good; **the distribution has holes**.

> Correction: the first version of this page said "zero cases for override lifecycle /
> locators / console commands" and that was wrong - `Overrides.spec.cpp`,
> `Locators.spec.cpp` and `ConsoleCommands.spec.cpp` all exist, with 6 cases each. The real
> problem is not "none" but **that these cases do not assert enough**: `Overrides` does not
> test the `IsOverridable()` flag rule directly, nor "a second Override of the same name is
> not added twice", nor the `bAddNew` branch; `Locators` does not cover the default
> `ULuaModuleLocator_ByBinding` at all; `ConsoleCommands` only asserts "did not crash" for
> `lua.profiler` / `memstats` / `gc` / `sandbox`, never an observable effect. (That
> conclusion was made without checking the files - this page's own lesson.)

| Area | State |
|:---|:---|
| `IsOverridable()` flag rule / no double add / `bAddNew` branch | indirect coverage only |
| `ULuaModuleLocator_ByBinding` (the default locator) resolution rules | zero cases |
| `lua.profiler` / `memstats` / `gc` / `sandbox` | only "does not crash", not observable effects |
| `UnLuaNovaDebugBase` (breakpoints / stack / **locals**) | no dedicated cases; `Observability.spec.cpp` covers registration and stepping only |
| DAP adapter | ✅ `DebugAdapter.spec.cpp`: starts the real server, connects over loopback, exchanges `initialize` / `attach` / `evaluate` / `threads` / `disconnect` in DAP framing, asserts `supportsEvaluateRequest` and an evaluated result, plus "an unknown command answers with an error instead of hanging the connection" |
| `FLuaEnv::HotReload()` | ⚠️ partially covered: `LuaEnv.spec.cpp` now has "does not break the env" and "callable twice, does not wipe globals". **"the module was actually replaced" is still uncovered** - that needs a file change or a hand-built loader to produce an mtime delta, which is not one more assertion |
| `FDanglingCheck` logic | ✅ `DanglingCheck.spec.cpp`. The semantics were pinned down before any assertion: it is **not** "scan and report a count of dangling pointers" (there is no `Run()` and no number to assert) - it is a **per-call guard**: containers and structs first handed to Lua during a call are recorded, and when the outermost guard drops they are tagged and removed from the registry. The case asserts "the `ScriptContainerMap` entry is gone once the call returns", with "the entry survives with the switch off" as the control |
| ~~`Private/Perfs/*` registers 0 cases~~ | **corrected**: there is no `Private/Perfs/` directory. The perf cases live in `Plugins/UnLuaNovaTestSuite/.../Private/Specs/Benchmark.spec.cpp` and `BenchmarkParity.spec.cpp`, and they are real automation cases (assert "ran N iterations", compare against the baseline) |
| Lua side | `Content/Script/Tests/**` is mostly fixtures for the C++ side, with no assertions |

---

## Six. Improvement plan

Ordering principle: **fix what is wrong first, then make the selling points real, then fill
the gaps.** Each item carries an acceptance test - if it cannot be accepted it is not done.
That rule was set in the previous round.

**Landing progress** (same batch of changes as this page): all three P0 items landed **in code**
(#2 still lacks its artifact, see below); **three of the four** P1 items landed - #7 "CI runs
the tests" is **not landed**: `.github/workflows/` still contains only `ci.yml` and `docs.yml`,
with no nightly workflow. Of P2, the DAP upvalue item and the documentation corrections (the
ghost API part) landed. Of P3, Lua memory attribution by source (`NetBytes` in the profiler
plus `lua.profiler mem`), coroutine leak detection (`FLuaCoroutineTracker` plus
`lua.coroutines`), cross-process `RPCId` agreement (covered by
`UnLuaNova.API.Net.DedicatedServer`) and multi-env debugging (`FLuaEnv::GetAllEnvs()`,
`lua.dbg env <n>`, DAP `arguments.env`) all landed.

**Still not landed**:

* extending thunk coverage to containers/structs - needs the code generator re-run, not just
  a code change
* hot-patch packages / versioning / rollback - out of scope this round
* **the baseline artifact of #2** - the writer and the gate exist (`Benchmark.spec.cpp` emits
  `Saved/Benchmark/perf-<backend>.json`, `RunTests.ps1 -UpdateBaseline` copies it into
  `Tools/baseline/`), but `Tools/baseline/` currently holds only `lua.json`, `luau.json` and
  `classification.json` - no `perf-*.json`. So the gate is in its "no baseline yet, warn
  instead of fail" state until a real run produces one
* **#8 is half done** - `Enum` and `Text` are now thunkable (their storage is fixed by the
  kind: an underlying numeric property, and one string). `Struct` / `Array` / `Set` / `Map`
  are still out: their Lua representation is built by `FPropertyDesc`, which lives in
  `Private/` and is unreachable from the generated file's side of the module boundary.
  Making them thunkable needs a public `Push` / `Read(L, Property, ValuePtr)` pair and a
  generator re-run - a design change, not another `||`.
* the coverage holes are closed - `DebugService.spec.cpp`, `DebugAdapter.spec.cpp` (real
  loopback), `DanglingCheck.spec.cpp`, the stronger Overrides / Locators /
  ConsoleCommands assertions and two `HotReload` cases are all in place

### P0 - correctness

| # | Item | Approach | Acceptance |
|:---|:---|:---|:---|
| 1 | Userdata never collected on Luau (1.1) | move cleanup ownership to an `FLuaEnv` sweep (or explicit release); never reach back into the Lua stack from a GC callback | one case per backend: after repeatedly creating and destroying TArray/TMap/FText, the container count in `lua.memstats` does not grow monotonically |
| 2 | Performance numbers not reproducible (2.1) | commit `Tools/baseline/perf-{lua,luau}.json`; `RunTests.ps1` writes it out and can optionally update the baseline | a baseline tagged with machine and engine version exists in the repo; every number in the docs can point at it |
| 3 | Performance tests cannot fail (2.2) | benchmark cases assert "not above baseline x 1.25" instead of `> 0` | after deliberately slowing a path, that case turns red |

### P1 - making the selling points real

| # | Item | Approach | Acceptance |
|:---|:---|:---|:---|
| 4 | Profiler self time always 0 on Luau (1.2) | attribute samples into an approximate self time, or at least label `top`/`dump` as "sampled, not exact"; document the backend difference | under Luau, `lua.profiler top` no longer shows all-zero self times, or it states the accounting explicitly |
| 5 | Cross-backend differences cannot be queried (1.4) | add `userdatagc` / `profilerselftime` to `HasFeature`; make `ToVector` returning nil on the Lua backend either consistent or an error instead of a silent nil | scripts can ask about both; one vector case gives the same result on both backends (or fails the same way) |
| 6 | Dependency versions not reproducible (1.5) | add `Source/ThirdParty/VERSIONS.md` recording the exact tag or commit of Lua / Luau; make the "about" page read it | when upgrading Luau you can answer "from where to where" |
| 7 | No CI runs the tests (3.1) | nightly (self-hosted) runs one backend; fail when the case count is below the baseline | a real case count appears in CI once |

### P2 - filling gaps

| # | Item | Approach | Acceptance |
|:---|:---|:---|:---|
| 8 | Thunk coverage excludes containers/structs (2.3) | add Struct and Array/Map/Set kinds to `IsThunkableParamKind` (G5 phase two) | thunk coverage rises and the `Lua.Container.*` cases get faster |
| 9 | Documentation corrections (3.3) | one sprint: ghost APIs, licence, engine version, missing EN pages, the `13_AnimNotify` script | `mkdocs build --strict` passes and `FLuaContext` cannot be found in the docs |
| 10 | Coverage holes (section five) | add override lifecycle, DebugBase, DAP, locator rules, console command behaviour | at least one case per area |
| 11 | DAP `evaluate` excludes upvalues (3.2) | resolve upvalues too | upvalues can be evaluated at a breakpoint |

### P3 - taking open ground

| # | Item | Note |
|:---|:---|:---|
| 12 | Hot-patch packages / versioning / verification / rollback | both release lines need it; `HotReload.lua` is a starting point |
| 13 | Attributing Lua memory by source | `lua.memstats` is categorised but still cannot answer "which function of which module" |
| 14 | Coroutine leak detection | latent functions create coroutines; a hung coroutine has no detection |
| 15 | Multi-env debugging | under `ByGameInstance`, dbg/dap work against one env only |
| 16 | Cross-process `RPCId` agreement | not provable in single-process C/S PIE (both ends share one UClass); needs a real two-process session |

---

## Seven. This round's lesson

The previous round's lesson was "**translate the other project's motivation into a fact in our
own code before deciding to act**" - it saved G1 (parallel base classes) and G3 (GC hitches)
from being direction errors.

This round's lesson is the other half:

> **Check a claimed capability by "what a user actually gets under the default configuration",
> not by "whether that path exists in the code".**

By the second measure, Luau's sandbox, the profiler, userdata reclamation, cross-backend
consistency, the performance numbers and the 518 tests all count as "present". By the first
measure they are discounted by **being off by default**, **self time being 0**, **never being
collected**, **returning nil**, **having no artifact** and **having no CI** respectively.

Benchmarking is addictive in "adding features", because adding is additive and visible. But
when we already lead in 9 dimensions and trail in only 2, **the marginal return on more
addition is lower than making what we have real**. Of the six P0/P1 items above, five need no
new capability - only finishing what exists.

---

## Further reading

- [Comparison with slua-unreal](Comparison_sluaunreal.md) - round one, about "what is missing"
- [Performance parity with slua](Performance_Parity_slua.md) - the batch of numbers in 2.1
- [Lua-declared Networking](LuaNetworking.md) - how G2 landed, and its known boundaries
- [Luau Backend](Luau.md) - backend differences
- [Testing and Automation](Testing.md) - case conventions (a new capability must bring cases)
- [Architecture](Architecture.md) - the dual-backend abstraction, override and static binding paths

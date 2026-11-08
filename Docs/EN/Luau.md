# Luau Backend

Integrated revision: `6d36d483cb88973276af611198169f76f3ce05b0` (0.743 + 1). Upstream 0.744 was inspected without upgrading. Backend selection is in `Config/DefaultUnLuaNova.ini` and requires rebuilding.

## Capability Boundaries

| Layer | Integration | Limit |
|---|---|---|
| Compiler / syntax | Pinned Luau syntax, annotations, generics, continue, compound assignment, interpolation and if expressions | Parsing annotations is not type checking; integrated conformance coverage remains incomplete |
| Standard libraries | base, coroutine, table, string, math, os/debug subsets, bit32, utf8, vector, buffer | Registration follows VM/src/linit.cpp; experimental integer/user-class features follow upstream flags |
| UE bridge | vector/FVector and buffer/TArray copies | float versus double precision; buffer does not alias external UE storage; result array is not guaranteed uint8 |
| Compiler settings | optimize/debuglevel/coverage | Coverage export and gates incomplete |
| CodeGen | Opt-in with module filter | Platform/executable-memory policy and debug interaction need validation; counters measure requests |
| Diagnostics | Debug primitives, profiler, allocation categories, coroutine census, GC controls, a time watchdog and a deterministic safe-point budget (`lua.maxsafepoints`) | Churn is not live bytes; Luau self time is sampled attribution, not measurement; a safe point is not an instruction on Luau; remote/packaged acceptance incomplete |
| Type checking | Generated declarations plus external luau-lsp | Analysis/Config are not runtime type checking; skip behavior must be checked |
| Modules | Host loader, package.searchers/preload, `package.aliases` redirection on both backends, a final `require` wrapper with cycle detection on both backends, `unrequire` / `reloadmodule` and `package.resolution()` | Upstream Require `@alias` config files are explicitly refused; names are case-sensitive with no fallback; cooked builds need loose scripts staged; package.path/cpath are not the host loader policy |

See upstream [compatibility](https://luau.org/compatibility/) and [sandbox](https://luau.org/sandbox/) documentation.

### Native codegen stability contract

`lua.codegen.enabled` defaults to 0. This is a **default-off optional optimisation**, not a
correctness dependency: with it off every chunk runs interpreted and the feature set is
identical. The contract below is public; anything outside it is explicitly not guaranteed.

| Item | Contract |
|---|---|
| Backend / architecture | Luau only; decided at runtime by upstream `luau_codegen_supported()` (x64 / arm64). Other targets compile with `UNLUANOVA_HAS_NATIVE_CODEGEN=0`: the switch is ignored with a warning rather than silently taking another path. Runtime acceptance here is UE 5.8.3 / Win64 x64; arm64 is compile-time supported but not runtime-validated in this repository |
| Scope | Only chunks loaded after the switch; `lua.codegen.modules` narrows by module-name substring; `lua.codegen on/off` and the CVar are one state |
| Executable memory | Upstream Luau allocates its own executable code pages. The host does not own the W^X / page-reclaim policy, and does not guarantee native debugger attachment or stepping into natively compiled functions while CodeGen is on |
| Failure semantics | Upstream `luau_codegen_compile()` returns void with no per-chunk success signal; a chunk that cannot be compiled keeps interpreted execution. The `lua.codegen` counter is a **request count** (`GetNativeCodeGenRequestCount()`), not a count of compiled functions; the old getter remains as a documented alias |
| Regression | `LuauIntegration.spec.cpp` covers VM / coroutine lifetimes and command / CVar coexistence; CodeGen executable pages are outside the per-block `UnLuaNova.Memory` release assertions |

It is therefore a **delivered optional optimisation with a published boundary**, not an
outstanding experimental gap. Depending on it for arm64, packaged Shipping or native
debugging still requires separate acceptance on that target.

### Upstream experimental boundary

Luau's integer / user-defined-class libraries are upstream feature-flag APIs whose semantics,
ABI and type declarations can change between upstream releases. Our `Luau.Build.cs` **does not
enable those flags**, and they appear in none of UnLuaNova's public APIs, generated declarations
or test contracts. They are **not a capability this plugin ships**: they are optional switches
inside the pinned upstream submodule. Enabling them yourself is outside the supported surface
and carries upstream-change and ABI risk. The plugin itself ships no experimental capability.

## Lua Compatibility

Full Lua 5.x language equivalence is not promised: integer/operator syntax, goto and close semantics differ. Use explicit resource cleanup. Replication buckets use strict int64 conversion, and the ±2^31 / ±2^53 and bad-input cases have executed on both backends; extreme values across real transports and mismatched schemas remain specialised gaps. Exact values beyond +/-2^53 require native integer64; conversion cannot recover precision already lost in a number.

Luau has no userdata __gc. `UNLUANOVA_HAS_GC_METAMETHOD=0` and `UNLUANOVA_HAS_USERDATA_FINALIZER=1`; containers, FText, nontrivial exported constructors, manual proxies and known smart-pointer allocations use native destructors. Forced-GC and teardown-order tests have executed on both backends; struct arrays and DataTable row copies still await dedicated memory acceptance. Destructors must not touch the Lua stack; the allocation and LONGJMP cleanup audits remain incomplete.

Debug registry/uservalue functions include placeholders and upvalue identity is approximate. `load()` now validates its mode: pure `b`, unknown letters and empty modes fail clearly, `t` (including `bt`) compiles source through mode `t`. Host io is optional and buffered, not full Lua io; path validation is not protection against filesystem links. package.path/cpath do not define the host loader policy. Yield boundaries and iteration metamethods differ. Budgeted incremental GC is neither generational nor multithreaded GC.

## Module resolution and unload contract

The host wraps the **final** `_G.require` after `UnLuaNovaLib::Open()` and after HotReload
installs its replacement, rather than wrapping the chunks returned by a searcher. That keeps
`package.searchers` signatures and HotReload's `setfenv` / `_ENV` rewriting intact while
covering Luau's own require, plain Lua's native require, `FLuaEnv::Require` and the
post-reload replacement. A `loading` table rejects re-entry with
`require: cyclic dependency involving 'X'`; the mark is cleared on both success and failure,
so a failed load cannot poison later requires. Alias self-loops are refused directly, and
longer cycles are caught by the same `loading` table.

| API | Contract |
|---|---|
| `require(name)` | Case-sensitive dotted names, one-to-one path mapping, no automatic case fallback. Five host searchers: aliases, preload, custom loaders, file system, builtins |
| `package.aliases[name] = target` | Runtime redirection; alias and target share one cached instance |
| `unrequire(name)` | Clears `package.loaded[name]` plus HotReload's `loaded_modules` / `loaded_module_times` / `ignore_modules`; returns the number of cleared entries. `package.preload` and `package.aliases` **survive** as loader/redirection declarations, otherwise `reloadmodule` would fail to load again |
| `reloadmodule(name)` | `unrequire(name)` followed by the current `require(name)` |
| `package.resolution()` | Machine-readable contract: `case_sensitive=true`, `automatic_case_fallback=false`, `alias_config_files=false`, `runtime_alias_table=true`, `cycle_detection=true`, `explicit_unrequire=true`, `cooked_build` (per build), `unrequire_scope="exact-name"`, `cleared_caches` |

Names starting with `@` (upstream Require `@alias` / `.luaurc` config files) now fail with an
explicit refusal pointing at `package.aliases`; they no longer degrade to a bare "module not
found". In cooked builds, loose `.lua` files resolve only if staged under the project dir or
`ProjectPersistentDownloadDir`. `lua.requireresolution 1` enables an opt-in warning when the
file-system searcher declines a module (off by default, because declining is normal).

`HotReload.lua` commits in two phases: load and compute new modules first, then `xpcall` the
commit and snapshot-roll back the old module's top-level keys, `package.loaded` and the two
private caches on failure. `M.reload()` / `reload_modules()` return `ok, err`; `module_loaded`
hooks fire only after a successful commit and file timestamps advance only on success, so
fixing the source allows a direct retry. **This is best-effort rollback, not a transaction**:
`_G`, live stack locals, arbitrary tables/upvalues/registry and references held elsewhere are
not snapshotted.

## Configuration

Defaults: `luau.optimize=1`, `luau.debuglevel=1`, `luau.coverage=0`, `lua.codegen.enabled=0`, GC goal/step multiplier 200/200 and step size 1 KB. The `lua.codegen on/off` command controls `lua.codegen.enabled`. Sandbox, sandboxthread, class-table freezing and host io default off. An empty io root means the project directory. `lua.maxsafepoints` (execution budget), `lua.maxmemorykb` (per-VM heap quota, KiB), `lua.requireresolution` (unresolved-path diagnostics) and `lua.memquota` all default off.

`lua.maxmemorykb` seeds a per-`FLuaEnv` budget through the default allocator's userdata. Before each allocation the allocator checks `LiveBytes + Delta`; over the limit it returns `nullptr` and the VM raises a **recoverable** `not enough memory` error that `pcall` can catch. Frees are never refused so `lua_close()` cannot wedge; a custom `lua_Alloc` stays outside the quota. `lua.memquota status` prints `limit / live / rejections / last_request` per Env, `reset` clears only the refusal counters, and `<kb>|off` updates all live Envs plus the CVar that seeds new ones. This tracks allocator-visible live bytes, not categorized churn, and does not cover native callback allocations, GC scratch or abandoned-coroutine attribution - it is **not complete untrusted-code isolation**. Unlike `lua.gc limit <kb>`, which only forces a collection, a quota refuses the allocation. See [console commands](ConsoleCommand.md#luamemquota).

Read-only globals do not restrict UE/debug/socket/io capabilities. Trusted and untrusted scripts need explicit host policies; the Lua backend's own io is not controlled by the Luau io switch.

Use `Tools/CheckLuauTypes.ps1 -Require` with existing tools and inspect skipped results. No LSP build or typecheck was run in this audit. Load examples through `require("Luau.Index")`; `Available` guards Luau-only files. VectorInterop and BufferInterop expose `:Run()`.

Profiler conversion uses the pinned submodule tool and its svg dependency:

```powershell
python Tools/LuauPerfgraph/perfgraph.py Saved/Profiling/profile.out > profile.svg
```

There is no --width option. New io/CodeGen/buffer regression source, plus the module-contract, memory-quota, reload-rollback and timer-diagnostics specs, awaits maintainer build and execution. See [testing](Testing.md).

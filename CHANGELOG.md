# Changelog

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

Entries follow the acceptance runs recorded under `Saved/ReleaseTests/` and the
[capability matrix](Docs/feature-matrix.md). Historical detail lives in Git; current scope and
outstanding work are maintained in the [audit](Docs/CN/Benchmark_Review.md).

## Unreleased — source changes, not yet validated

- Sixth source-only capability pass (`WATCHDOG`, `LUAU-REQUIRE`, `RUNTIME-RELOAD`,
  `RUNTIME-TIMER`, `LIFE-ENVS`, `PERFORMANCE`): a per-Env Lua heap quota (`lua.maxmemorykb`
  / `lua.memquota <kb>|off|status|reset`) enforced in `FLuaEnv::DefaultLuaAllocator`
  before each allocation, with live-byte, refusal and last-request counters, one actionable
  log per env and a recoverable VM error; frees are never refused so `lua_close` cannot
  wedge, and a custom `lua_Alloc` stays outside the quota. Module resolution gained a
  require-wrapper cycle guard installed *after* HotReload replaces `_G.require` (covering
  native require, `FLuaEnv::Require` and HotReload's replacement without changing loader
  signatures or the chunks HotReload applies `setfenv` / `_ENV` to), an explicit
  `unrequire(name)` / `reloadmodule(name)` cache contract clearing `package.loaded` and
  HotReload's three private caches while keeping `package.preload` / `package.aliases`
  declarations, `package.resolution()` as a machine-readable statement of the case /
  cooked / `@alias` / cycle / unload contract, an explicit refusal for upstream `@alias`
  config files, and `lua.requireresolution` for opt-in unresolved-path diagnostics.
  `HotReload.lua` now commits reloads in two phases with a snapshot rollback, returns
  `ok, err`, fires `module_loaded` hooks only after a successful commit, guards the
  `debug.getregistry()` scan and advances file timestamps only on success; `Lua.HotReload()`
  surfaces that boolean. Rollback is explicitly best-effort, not a transaction (`_G`,
  live stack locals, arbitrary tables/upvalues and the registry are not snapshotted).
  Timer registration now records the Lua source/line and owning Env (`Timer.Describe`,
  `Timer.Diagnostics`, attributable callback errors), and `DescribeEnv()` labels Lua call
  errors and timer failures with a stable env index, name and main-state pointer.
- Tooling: `Tools/CrossProject` gained a `#metric` log line type (separate from the ten
  `#case` timing cases) for `memory.peak_kb`, `startup.load_us`, `gc.pause_us` and
  `code_size.bytes`; `runner.py` aligns metric sets/units, `metrics.py` adds fail-closed
  `check_metric_alignment` / `summarize_metrics` behind an opt-in `--require-metrics`
  switch (old JSON behaviour unchanged), both workload adapters emit the lines and report
  `unavailable` rather than `0` when a value cannot be measured. 30 new tool regressions
  pass (130 total without UE). New UE automation: `ModuleContracts.spec.cpp` (memory quota,
  wrapped require cycle guard, unrequire/reload, resolution contract, env attribution),
  three `Timer.spec.cpp` cases (site diagnostics, cross-Env isolation, both world clocks)
  and three `RuntimeModules.spec.cpp` reload-rollback cases driven by an in-memory
  searcher source switch. **None of the C++ or UE automation in this pass was compiled or
  executed**; `release-status` stays `not-passed` and all 40 `verification` values stay
  `pending-runtime` until the maintainer's RunTests run.
- Source-only loading, require cycles, a deterministic execution budget and the profiler
  end-to-end path (`COMPAT-LANGUAGE`, `LUAU-REQUIRE`, `WATCHDOG`, `PROFILE`): `load()`
  validates its mode (pure `b`, unknown letters and empty modes fail clearly; `t` / `bt`
  compile source through mode `t`); the Luau-side `require` detects cyclic dependencies
  with a `loading` upvalue, names the module in the error and clears the marks on failure
  so a failed load cannot poison later requires; `lua.maxsafepoints` adds a per-host-call
  safe-point budget (Luau interrupts / plain Lua `LUA_MASKCOUNT` instructions) with an
  observable interruption counter; `FLuaVmHooks` forwards the GC flag so the profiler
  attributes collector time (no longer hardcoded to 0) while the watchdog and budget skip
  GC interrupts instead of longjmp'ing out of the collector; `lua.profiler trace dump`
  resolves relative paths under `Saved/Profiling`, the perfgraph wrapper gives an
  actionable submodule hint, and CN/EN console docs carry the collect -> dump -> render
  example. New specs in `ErrorHandling.spec.cpp`, `LuauIntegration.spec.cpp` and
  `LuaDeadLoopCheck.spec.cpp`.
- io and module resolution reached their code-complete subsets (`COMPAT-IO`,
  `LUAU-REQUIRE`): `file:write` now overwrites at `__pos` and advances the position while
  append mode still forces the end; `io.tmpfile()` returns an in-memory handle whose flush
  never touches disk; `io.popen()` raises an explicit refusal instead of pretending to be
  a pipe; `package.aliases[name] = target` redirects `require` on both backends through a
  searcher placed before preload, with alias/target sharing one cached instance and
  self-aliases rejected. `LuaEnv.spec.cpp` and `LuauIntegration.spec.cpp` cover alias
  redirection, self-loop rejection, overwrite/append positions, tmpfile and popen refusal.
  Searcher order in the docs is updated to five host entries.
- DAP command surface finished (`DEBUG-DAP`): `setFunctionBreakpoints` now matches
  `lua_getinfo('n')` names in the line hook (exact or `.` / `:` qualified suffix) with
  `verified=false` for empty names; `modules` returns a real `package.loaded` snapshot taken
  on the VM thread at attach / every stop with `startModule` / `moduleCount` paging, instead
  of a static placeholder. Specs cover verification, paging consistency and the snapshot.
- Compatibility and safety (`COMPAT-IO`, `WATCHDOG`, `LUAU-SANDBOX`, `LUAU-REQUIRE`):
  strict `io.open` modes (`r/w/a/+/b`, unknown or empty refused), strict `file:seek` whence
  clamped to the in-memory buffer, format lists for `file:lines()` / `io.lines()`, and a
  `lua.iomaxbytes` quota on open/write/flush; the watchdog now disarms the guard that
  actually finished (fixing a nested-call dangling pointer), lets nested guards take over,
  and exposes `FDeadLoopCheck::GetTimeoutCount()`; `lua.sandboxdeny` trims globals before
  `luaL_sandbox` freezes `_G` while protecting host-written tables; `LoadFromFileSystem`
  no longer mangles `./Foo` / `Foo.lua` into `//Foo` / `Foo/lua`. New Luau integration and
  dead-loop specs cover all of these.
- Debug adapter (`DEBUG-DAP`) closed its three remaining functional gaps: `setBreakpoints`
  now verifies each line against the resolved script's actual line count; exception filters
  (`raised` / `uncaught`) are wired into the shared `ReportLuaCallError` handler, arm the
  line hook, stop with `reason=exception` and answer `exceptionInfo`; `variables` expands
  nested tables through registry-backed handles with depth / cycle / count budgets, released
  at the next stop or `Detach()`. `stopped.reason` now reports the real cause. DAP and
  service specs cover verified breakpoints, capability bits, filters, last-error recording
  and expansion failure paths.
- Compatibility, event lifetime, codegen and benchmark tooling: `debug.getregistry` rejects
  explicitly instead of returning a misleading empty table, `getuservalue` documents its
  nil-only subset, `setuservalue` returns false instead of a pretend success, `upvalueid`
  documents value identity; `UnLuaNova.Event` prunes listeners whose owner reports
  `IsValid()==false` before each broadcast and accepts an injected validator; the native
  codegen counter is explicitly a REQUEST count (`GetNativeCodeGenRequestCount`);
  `Tools/CrossProject/metrics.py` produces fail-closed raw multi-run distributions. New
  `LuauConformance.spec.lua` samples syntax / library / vector / buffer boundaries on both
  backends. 100 tooling tests pass.
- Per-instance binding (`BIND-INSTANCE`) delivered: `UUnLuaNovaManager::BindInstance` /
  `UnbindInstance` (plus Blueprint entry points and query API) keep a shallow copy of the
  module table per object, `FObjectRegistry::RebindModule` swaps the INSTANCE metatable
  without invalidating pushed userdata, class overrides take the union of function names,
  and `FFunctionRegistry::Invoke` resolves functions per call for instance-bound objects so
  the class-level FuncRef cache cannot leak one instance's closure into another. Cleanup
  unrefs every instance table on object deletion / env shutdown. Five UE automation cases
  in `InstanceBinding.spec.cpp` cover coexistence, isolation, rebinding, fallback, unbind
  and rejected inputs; they still need a real UE run. See `Docs/CN/InstanceBinding.md`.
- DAP (`DEBUG-DAP`) command surface completed: `launch` (explicit refusal),
  `configurationDone`, `setExceptionBreakpoints`, `setFunctionBreakpoints`, `setVariable`
  (real `lua_setlocal` on a paused frame), `source` (disk read with dotted module
  resolution), `modules` (static view) and honest capability bits.
  `FLuaDebugService::BeginStep` gives `next` / `stepIn` / `stepOut` real stack-depth
  semantics instead of one persistent single-step switch; `DebugAdapter.spec.cpp` adds a
  per-command protocol probe. Exception filters, conditional breakpoints and nested
  variable expansion remain explicit scope limits.
- Luau coverage (`LUAU-COVERAGE`) delivered: `lua.coverage status|dump` exports per-source,
  per-line hit counters from `lua_getcoverage` as a self-describing JSON with declared
  exclusions. `Tools/ValidateCoverage.py` is a fail-closed gate that recomputes totals,
  merges several runs by maximum hits per line, requires named sources and a coverage
  threshold, and refuses missing/empty/level-0/mismatched reports. 15 tooling regressions
  in `Tools/tests/test_validate_coverage.py` pass without UE; the plain Lua backend is
  explicitly refused. Capability matrix moved from `planned` to `implemented`;
  `verification` stays `pending-runtime` until a real run is recorded. See
  [Luau coverage](Docs/CN/LuauCoverage.md).
- Cross-project external adapter (`PERFORMANCE`): `Tools/CrossProject/workload/external/cases.lua`
  now implements the full revision-1 workload for slua-unreal / FUnLua. All engine calls go
  through two adaptive hooks (`UEAPI.Static` / `UEAPI.Vector`) probing `UE` / `UE4` / `slua`
  globals and `import()`; an unreachable API fails that case with `peropus=0` instead of
  dropping a slow case. `Tools/tests/test_crossproject_workloads.py` statically enforces
  identical case sets, iteration counts, revision, meta fields, fail-fast identity and
  failure reporting (6 regressions). No cross-project conclusion is claimed until both
  external projects actually run it.
- Third-party notices: recorded in-tree version identifiers with file:line evidence
  (LuaSocket 3.0-rc1, MIME/LTN12/URL 1.0.3, serpent 0.30, lua-rapidjson `scm`,
  RapidJSON 1.1.0). Upstream revisions, full shipped notices and sample-asset rights remain
  rights-holder decisions and are labelled as such rather than guessed.
- Net release blockers closed: `UnLuaNovaTestSuite.Build.cs` now links `NetCore` (the
  net-scenario tests close a `UNetConnection`, and `ENetCloseResult`'s `StaticEnum` /
  `LexToString` are exported there), fixing LNK2019 on the TestSuite DLL. The generic-payload
  order-independence spec bound its integers to the loop index instead of the key, so the two
  maps it compared held different values; it now varies insertion order only. Luau backend
  build reported `Result: Succeeded`, and a real Luau Editor `UnLuaNova.API.Net` run reported
  35 succeeded / 3 with warnings / 0 failed. This does not replace the full 33-stage Release.
- Cross-project workload: removed the literal `FILL-ME` machine/CPU values. The workload
  refuses to emit a benchmark block until `SetMachineInfo()` is called with real, space-free
  values, and `runner.py` now rejects placeholder identities during alignment. Six new
  alignment regressions pass without UE. The slua-unreal / FUnLua adapters are still absent.
- Demo weapon script: `InstantFire` now passes a real `FHitResult` out parameter and
  dispatches hits to an overridable `OnInstantHit` instead of an empty `-- todo:`.
- Declarative networking: replicated fields now support eight lifetime conditions through one
  storage group per condition, because UE decides what to send from the property's own
  registration, which is cached per class. `COND_Custom` / `COND_Dynamic` / `COND_Never` /
  `COND_NetGroup` and the Replay* family are rejected at declaration time instead of being
  silently downgraded to `COND_None`. The obsolete component-wide `Condition` now only warns.
- A subclass with no declaration of its own inherits the parent class's schema, and
  `FUnLuaNovaNet::Cleanup(Env)` releases a schema by joint ownership (see below) instead of
  attributing it to whichever environment happened to shut down first. Orphan entries with no
  owner remain process-wide state that only `Cleanup(nullptr)` drops.
- Unit cases added for condition groups, reliable/unreliable RPC flags, scalar and Object
  bucket round-trips, schema-mismatch refusal and parent-class schema lookup.
- Evidence provenance: `RecordEvidence.py` now takes `--commit` / `--source` so importing a
  historical report cannot be misattributed to today's HEAD. The four real passing 2026-11-03
  baseline automation reports (Lua/Luau x Editor/Game) were recorded with `commit 347bb8c5` and
  flagged `historical`; `LuaNetDedicatedServerTest.cpp` is now registered in the matrix so DS
  cases map to the Networking capabilities instead of being dropped as unmapped.
- Connected-acceptance **scenario source** added: a non-owner Server RPC attempt with a unique
  token (proved never executed by exact `CallLog` comparison plus a `ValidateRelease.py` gate),
  and an `OwnerOnly` field mixed with an unconditional field with per-peer visibility assertions.
- Arbitrary `UStruct` / container types: corrected the earlier "architectural boundary" call.
  A `UClass` cannot gain a `UPROPERTY` at runtime, but neither delivery path needs one: RPC
  parameters attach real `FStructProperty` / `FArrayProperty` / `FSetProperty` / `FMapProperty`
  to the runtime-built `UFunction`, and replicated fields use a shared `Generic` bucket with a
  canonical self-describing payload. Specs (`Struct:<path>`, `Array:<spec>`, `Set:<spec>`,
  `Map:<key>:<value>`) are validated at bind time and folded into `SchemaVersion`. `UObject`
  references keep the dedicated Object bucket. Unit cases cover parsing, nesting, rejection and
  Lua <-> payload round-trips.
- Eight opt-in connected scenarios in `LuaNetNetScenariosTest.cpp`: late join, process-level
  reconnect (`FNetSession::RestartPeer`), dormancy, relevancy, join-order swap, packet loss /
  reordering (real `NetEmulation.PktLoss/PktOrder/PktLag`), `UNetConnection`-level reconnect
  (`Connection->Close()` plus an in-process reopen through `-UnLuaNovaNetReconnectUrl`), and
  cross-peer declaration fingerprints. `FNetSession` forwards the scenario flag, packet-loss
  percentage and exact reconnect URL to every peer, and records `launch_serial`, `first_pid`,
  `launch_count`, scenario and delivery variant in `topology.json`.
- Legacy / PushOff / Iris / Cooked delivery matrix: `Tools/RunNetMatrix.ps1` (also
  `RunTests.ps1 -Profile Development -NetMatrix`) and `RunTests.ps1 -ExtraArgs` pass real engine
  switches to every peer; the Cooked cell points the peer executables at staged
  `Windows` / `WindowsServer` binaries. `AppendWorldFacts` reads back Iris / push-model cvar
  state, compile-time `WITH_EDITOR`, and real `UNetConnection` byte / lost-packet counters.
  `BuildNetDeclarationReport()` exports each RPC parameter's spec from the **actual FProperty**,
  each field's `TypeSpec`, `RepSchemaVersion` and a deliberately different
  `VariantRepSchemaVersion`, so `ValidateRelease.py --net-matrix` can reject a cross-peer
  declaration mismatch instead of decoding into the wrong slot.
- `NetActor.lua` now declares a real `Inventory` (`Array:Int`) field and a `SendPayload` RPC with
  a `Struct:/Script/CoreUObject.Vector` parameter, giving the connected gate something to prove
  about generic types without changing the strict `CallLog` comparison.
- `FUnLuaNovaNet` schemas now use a **joint owner set**: identical declarations bound by several
  environments are released only when the last closes, in any order. Closing the first owner
  previously wiped a schema another Env still used. Two reverse-order unit cases and a weak
  `UObject` invalidation case were added.
- 58 Python tooling tests passed in the previous change. Five more gate cases (cross-peer schema,
  generic field / UStruct parameter, join-order swap + packet loss, connection-level reconnect,
  cooked variant) were added here and have **not** been run.
- These are **source and unit-case changes only**. None of the connected assertions, eight
  scenarios or four variants has been **executed** - they require a rebuild and real DS runs.
  There is no remaining source-level networking TODO; engine-level seamless travel
  (`ServerTravel`), hot reload and authorization checks remain explicitly out of scope rather
  than silently claimed. `release-status` stays `not-passed` and the three Networking capability
  groups stay `experimental`; see the [acceptance checklist](Docs/CN/LuaNetworking.md).

## Current release status: not passed

Current status follows `Saved/ReleaseTests/test/release-summary.json`. Later validations did not
pass for three diagnosed reasons: a Luau Editor performance regression that was reproduced as
measurement-environment interference (a later re-run passed 641/641 with the gate green, with the
committed baseline untouched); `ConflictingInstance` UBT mutex clashes and externally interrupted
compiler processes that failed packaging/build stages; and uncommitted Net source changes in the
current worktree that fail two `UnLuaNova.API.Net.*` cases. The baseline is not replaced and the
25% gate is not relaxed. Release stays blocked until the worktree is frozen, the generated matrix
is refreshed and a full Release run completes 33/33 with four green release gates.

## 2026-11-03 — baseline run, validated

The numbers below belong only to `.\Tools\RunTests.ps1 -Baseline` (`Saved/ReleaseTests/baseline`,
2026-11-03 03:32:11–04:20:26 UTC, commit `347bb8c5`, 8 uncommitted worktree changes recorded in
that run's provenance). They do not describe later commits or the current tree.

Complete Release validation on UE 5.8.3 / Win64 (Visual Studio 2026 18.10): 33/33 stages passed,
2482/2482 UE executions with zero failures and zero skips — Lua Editor 626, Lua cooked 600,
Luau Editor 641, Luau cooked 615.

- Both backends built from source: reflection, template exports and generated marshal thunks.
- Real dedicated-server plus two-client RPC and replication evidence on both backends
  (DedicatedServer and multicast sessions), with their runtime manifests and memory reports.
- Static analysis on both backends: 0 errors (298 Lua warnings and 7 Luau warnings retained,
  both a known analyzer limitation on `package.path`-style requires).
- Development, Server and Shipping packaging for both backends.
- 41 Python tooling tests and the PowerShell support checks passed.
- Source audit covers slua-unreal, FUnLua and Luau, with explicit gaps and acceptance criteria.
- Performance in this run: every benchmark case stayed inside the 25% gate against the committed
  reference; Luau leads Lua 5.5.1 on the container case (self-comparison, not a third-party comparison).

Boundaries that run did not close, recorded rather than waived: at that time conditional
replication stayed experimental and rejected non-`None` conditions explicitly (the per-condition
layout has since been delivered — see Unreleased above); third-party notices are still being
completed; non-Win64 platforms, Iris/push model and a same-machine comparison against
slua-unreal / FUnLua need their own acceptance runs.

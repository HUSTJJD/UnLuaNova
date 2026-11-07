# Current Version Status

<!-- release-status: passed -->
<!-- release-provenance: 347bb8c5 -->

This page records **conclusions that were actually produced**, not stale status. Capability and
outstanding-work detail lives in the [audit](Benchmark_Review.md) and the
[capability matrix](../feature-matrix.md); history lives in Git.

## Current release status: passed

Current status follows `Saved/ReleaseTests/test/release-summary.json`: the 2026-11-07 full
`.\Tools\RunTests.ps1` produced `complete=true`, `passed=true`, 33/33 stages passed, 2752/2752 UE executions, 0 failed, 0 skipped - Lua Editor 690, Lua cooked 661, Luau Editor 715, Luau cooked 686, with all four
release gates green. The 2482/2482 figure and the four carrier counts below belong only to
the 2026-11-03 baseline run; the latest run is also 33/33 stages, but with 2752/2752 executions.

### Historical failures, now closed (no gate was relaxed)

Earlier regular validations did not pass; every cause was diagnosed and fixed. They are no
longer the current state:

1. **The performance regression is not a code regression.** One re-run had five Luau Editor cases
   beyond the 25% gate (+43.8% to +55.8%), but product sources (C++ / Lua / ini) were unchanged,
   manifests, backend flags and process priority matched, and the five samples were stable. A later
   re-run passed Luau Editor 641/641 with the performance gate green. At that time the committed
   reference baseline was not replaced.
2. **UBT build processes were interrupted externally.** `ConflictingInstance` (a global mutex held
   by orphan UBA/UBT processes) and child compilers exiting with `Exit -1` failed
   `Lua-package-development`, `Lua-server-build` and `Luau-package-development`, which in turn
   blocked test and evidence stages. Those stages pass after the orphan processes are cleaned up;
   a re-run therefore requires a machine with no concurrent UE build.
3. **The Net source regression is closed (this round).** The uncommitted Net changes were committed
   as `5f2e5f00`; `UnLuaNovaTestSuite.Build.cs` now links `NetCore`, removing the unresolved
   `StaticEnum<ENetCloseResult>` / `LexToString(ENetCloseResult)` symbols; and the generic-payload
   order-independence spec was binding its integers to the loop index instead of the key, so the two
   maps it compared were different values. It now varies insertion order only. The Luau backend
   build reported `Result: Succeeded`, and a real Luau Editor `UnLuaNova.API.Net` run reported
   35 succeeded / 3 with warnings / 0 failed
   (`Saved/TestReports/20261103-234556/luau/index.json`). **This is not a full Release**: the 33
   stages, four carriers and four release gates still need a run with no concurrent UE build.

**Several runtime defects were closed in this round.** `BindInstance` no longer stacks a
second thunk on a UFunction the class-level bind already patched; Luau VM safe points
(`GC = -1`) are no longer mistaken for collector interrupts, so the safe-point budget and
timeout watchdog really cancel an infinite loop; `~FLuaEnv()` clears its VM-hook slot before
`lua_close` and skips `luaL_unref` on the closed state, removing both the cooked-shutdown
crash and hook leakage into a recycled `lua_State*`; and `FLuaVmHooks::Get()` is now an
exported process-wide singleton instead of one hook table per DLL. The passing record, per-stage detail and
archives are under `Saved/ReleaseTests/test/`. The 25% gate was not relaxed; `perf-lua.json` was refreshed in `1455c38d`
from an independent median-of-5 reference run matching the current sampler, while
`perf-luau.json` and the 2026-11-03 baseline run are unchanged.

## 2026-11-03 baseline run (validated)

The numbers below belong only to this run: `.\Tools\RunTests.ps1 -Baseline`, artifacts under
`Saved/ReleaseTests/baseline`, started 2026-11-03 03:32:11 UTC and finished 04:20:26 UTC. Its
`release-summary.json` provenance records commit `347bb8c5` (the worktree had 8 uncommitted
changes) plus source and project hashes. **It does not describe later commits or the current tree.**

Complete Release validation on UE 5.8.3 / Win64 (Visual Studio 2026 18.10): 33/33 stages passed,
2482/2482 UE executions with zero failures and zero skips — Lua Editor 626, Lua cooked 600,
Luau Editor 641, Luau cooked 615.

- Both backends built from source: bidirectional reflection calls, template exports, generated
  marshal thunks, overrides and RPC overrides.
- Real dedicated-server plus two-client RPC and replication evidence (DedicatedServer and
  Multicast sessions), with per-peer runtime manifests and memory reports.
- Static analysis on both backends: 0 errors (298 Lua warnings and 7 Luau warnings retained,
  a known analyzer limitation on `package.path`-style requires).
- Development, Server and Shipping packaging succeeded for both backends.
- 41 Python tooling tests and the PowerShell support checks passed.
- Performance in this run: every benchmark case stayed inside the 25% gate against the committed
  reference; Luau leads Lua 5.5.1 on the container case (self-comparison, not third-party parity).

## Boundaries this run does not close

These are scope statements, not pending conclusions: the per-condition storage layout (eight
conditions) is delivered and covered by unit cases, arbitrary `UStruct` / containers are
delivered via dynamic RPC FProperties and a replicated Generic bucket with FProperty-derived
parameter specs and per-peer declaration fingerprints, and multi-Env joint ownership plus weak
object invalidation have unit cases. Declarative networking stays experimental because
connected owner / non-owner acceptance, the non-owner RPC rejection assertion, all **eight**
connected scenarios and the Legacy / PushOff / Iris / **Cooked** variants have source and gates
but **none has been executed**; no source-level networking TODO remains. Third-party notices are
now reproducible and statically checkable, but their `UNRESOLVED` entries still need a
rights-holder decision; non-Win64 platforms, engine-level seamless travel, hot reload,
authorization checks and a same-machine comparison against slua-unreal / FUnLua each need their
own acceptance run.

Acceptance method and gates: [Testing](Testing.md). Diagnostic history:
[2026-10-24 regression record](Regression_20261024.md).

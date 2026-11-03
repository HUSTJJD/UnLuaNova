# Current Version Status

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

This page records **conclusions that were actually produced**, not stale status. Capability and
outstanding-work detail lives in the [audit](Benchmark_Review.md) and the
[capability matrix](../feature-matrix.md); history lives in Git.

## Current release status: not passed

Current status follows `Saved/ReleaseTests/test/release-summary.json`; the validated 33/33 figures
below belong only to the 2026-11-03 baseline run. Later regular validations did not pass, for
three diagnosed reasons, none of which relaxes a gate:

1. **The performance regression is not a code regression.** One re-run had five Luau Editor cases
   beyond the 25% gate (+43.8% to +55.8%), but product sources (C++ / Lua / ini) were unchanged,
   manifests, backend flags and process priority matched, and the five samples were stable. A later
   re-run passed Luau Editor 641/641 with the performance gate green. The committed reference
   baseline was not replaced.
2. **UBT build processes were interrupted externally.** `ConflictingInstance` (a global mutex held
   by orphan UBA/UBT processes) and child compilers exiting with `Exit -1` failed
   `Lua-package-development`, `Lua-server-build` and `Luau-package-development`, which in turn
   blocked test and evidence stages. Those stages pass after the orphan processes are cleaned up;
   a re-run therefore requires a machine with no concurrent UE build.
3. **The current worktree has uncommitted Net source changes.** `UnLuaNovaNet.cpp`, related headers
   and `LuaNet.spec.cpp` are modified, and two `UnLuaNova.API.Net.*` cases fail on Luau Editor.
   This is a real regression to close in development, not a scope statement.

**Release remains blocked.** Freeze the worktree (commit or revert the Net changes), refresh the
generated capability matrix, then run the full `.\Tools\RunTests.ps1` on a machine without
concurrent UE builds. Only `complete=true`, `passed=true`, 33/33 stages and four green release
gates can change this page back to passed. Per-run artifacts are archived under
`Saved/ReleaseTests/`.

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
conditions) is delivered and covered by unit cases, and arbitrary `UStruct` / containers are
delivered via dynamic RPC FProperties and a replicated Generic bucket. Declarative networking
stays experimental because connected owner / non-owner acceptance, the non-owner RPC rejection
assertion, the late join / process-level reconnect / dormancy / relevancy scenarios and the
Legacy / PushOff / Iris variants all have source and gates but **none has been executed**;
third-party notices are still being completed; non-Win64 platforms, cooked carriers and a
same-machine comparison against slua-unreal / FUnLua each need their own acceptance run.

Acceptance method and gates: [Testing](Testing.md). Diagnostic history:
[2026-10-24 regression record](Regression_20261024.md).

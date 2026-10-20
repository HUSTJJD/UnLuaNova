# Current Version Status

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

This page records **conclusions that were actually produced**, not stale status. Capability and
outstanding-work detail lives in the [audit](Benchmark_Review.md) and the
[capability matrix](../feature-matrix.md); history lives in Git.

## Current release status: not passed

The latest regular Release validation (finished 2026-11-03 15:23 local time, artifacts under
`Saved/ReleaseTests/test`) reports `complete=true`, `passed=false`. Of 33 stages,
`Luau-editor-tests` failed and `Luau-editor-evidence` was blocked; 1 of 641 Luau Editor tests
failed while the other three carriers (626 / 600 / 615) passed.

The failure is `UnLuaNova.Perf.Benchmark`: Reflection, VM.Arithmetic, VM.Call, VM.TableGrow and
VM.TableRead ran 47.9%, 46.7%, 43.8%, 51.6% and 55.8% slower than the committed reference
baseline, all beyond the 25% gate. Product sources (C++ / Lua / ini) did not change relative to
the baseline run, the runtime manifests, backend flags and process priority match, and the five
samples are stable — this points to measurement-environment interference to be re-checked, not a
confirmed code regression. The baseline is not replaced and the gate is not relaxed; a full Release
re-run on an idle machine is required before the status can return to passed.

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

These are scope statements, not pending conclusions: conditional replication stays experimental
and rejects non-`None` conditions explicitly; third-party notices are still being completed;
non-Win64 platforms, Iris / push model and a same-machine comparison against slua-unreal / FUnLua
each need their own acceptance run.

Acceptance method and gates: [Testing](Testing.md). Diagnostic history:
[2026-10-24 regression record](Regression_20261024.md).

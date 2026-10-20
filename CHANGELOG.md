# Changelog

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

Entries follow the acceptance runs recorded under `Saved/ReleaseTests/` and the
[capability matrix](Docs/feature-matrix.md). Historical detail lives in Git; current scope and
outstanding work are maintained in the [audit](Docs/CN/Benchmark_Review.md).

## Current release status: not passed

The latest regular Release validation (2026-11-03, `Saved/ReleaseTests/test`) completed with
`passed=false`: `Luau-editor-tests` failed and `Luau-editor-evidence` was blocked because five
Luau Editor benchmark cases exceeded the 25% regression gate (+43.8% to +55.8%). Product sources
are unchanged from the validated baseline run; the manifests, backend flags and process priority
match, so measurement-environment interference is the leading explanation. The committed
performance baseline is not replaced and the gate is not relaxed. A full Release re-run on an idle
machine is required.

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

Boundaries this run does not close, recorded rather than waived: conditional replication stays
experimental and rejects non-`None` conditions explicitly; third-party notices are still being
completed; non-Win64 platforms, Iris/push model and a same-machine comparison against
slua-unreal / FUnLua need their own acceptance runs.

# Baselines

This directory holds four kinds of committed reference data. Only the `perf-*.json` files are
performance references; the others are test inventories and classification.

## Performance references

`perf-lua.json` and `perf-luau.json` are committed. The release validator requires matching
backend, engine version, machine, case inventory and iteration counts, and fails when any case is
more than 25% slower. The 25% gate has never been relaxed.

| File | Backend | UTC measurement | Sampling | SHA-256 of the committed JSON |
|---|---|---|---|---|
| `perf-lua.json` | Lua | 2026-11-05T13:11:09.174Z | `median-of-5-batch-means`, warmup >= 250 ms, 5 samples >= 100 ms | `815093DFBAE9196F3BFF02B63764C1DBDDEC7E17B6CD025A9790BC6420ADA56B` |
| `perf-luau.json` | Luau | 2026-10-24T15:00:26.011Z | single-batch samples (older method) | `D70A6EF1882D81FAB2DBBBE935F019DF68AE5B99D2B1B67A70B5670A9BB04B27` |

`perf-luau.json` is the unmodified measurement from the independent Development Editor run
`Saved/TestReports/FullFix-Complete-11`. `perf-lua.json` was replaced in `1455c38d` by an
independent 2026-11-05 reference run using the current median-of-5 sampler; its older
single-batch content is history, not the current gate input. Neither file is a "before the fix"
measurement, so neither establishes a before/after speedup. Sampling-method differences and
background load remain limitations.

The current sampler warms each case for at least 250 ms, then records five samples of at least
100 ms. Each sample repeats the same iteration batch and reports elapsed time per batch; the
result is the median of the five batch means. Raw elapsed times and batch counts are retained
and independently checked by the release validator. Workload, per-batch iteration counts,
units and thresholds stay comparable across both files.

Scope: Win64 Editor Development, UE 5.8.3, MSVC 14.50, AMD Ryzen 7 4700G, machine
`davidsjian-PC2`. The original Lua reference run had one unrelated debugger fixture failure
(tail calls removed frames); that fixture was corrected before Release. Luau passed all 638
cases in its reference run. These are initial references for the corrected implementation, not
evidence of a crash-to-fix speedup. Do not replace them with a failed verification run to make a
regression disappear. For another machine, engine or intentional performance change, capture and
review a separate reference run, record its provenance here, then verify in a later run with
unchanged references.

## Case-set inventories (drift gates)

A green run that silently covers fewer tests is the failure mode these files exist to catch.
`GenerateTestReport.py --baseline <file>` compares totals, the set of test names and duration;
`--fail-on-drift` exits non-zero when names are added or removed. The full Release now passes
that flag for both the editor tests and the two report stages, so each carrier has its own
committed inventory from the 2026-11-07 full Release (tested commit `1455c38d`):

| File | Backend | Carrier | Cases |
|---|---|---|---:|
| `lua.json` | Lua | Editor | 690 |
| `lua-game.json` | Lua | cooked `-game` | 661 |
| `luau.json` | Luau | Editor | 715 |
| `luau-game.json` | Luau | cooked `-game` | 686 |

These are recorded **execution** inventories. Never hand-edit them to claim tests that have not
run. When a case set changes intentionally, review the diff, then refresh only the matching
file from a real run:

- Editor carriers: run `Tools/RunTests.ps1` with `-UpdateBaseline` and the matching `-Backend`.
  It rewrites `Tools/baseline/<backend>.json` from that run.
- Cooked carriers: run `Tools/GenerateTestReport.py` with `--reports <game run dir>`,
  `--out <game run dir>`, `--backend <Backend>`, `--baseline Tools/baseline/<backend>-game.json`
  and `--update-baseline`.

`classification.json` is a static per-test environment classification used by the published
report, and is not a gate input. The opt-in connected matrix is produced separately by
`Tools/RunNetMatrix.ps1`; its scenarios and variants are not part of the Editor/Game automation
inventory and are validated by `ValidateRelease.py --net-matrix`.

# Cross-Project Benchmark (UnLuaNova / slua-unreal / FUnLua)

This page explains how to establish a **reproducible** three-way performance
comparison between UnLuaNova, slua-unreal and FUnLua, and why no such comparison
exists yet.

## 1. Current conclusion: the cross-project comparison is NOT established

**As of now, the cross-project performance comparison is NOT established
(`not-established`).** This repository contains no real run record from
slua-unreal or FUnLua, `Tools/CrossProject/results/` is empty, and there is no
three-way aligned comparison table. Therefore:

- One cannot say "UnLuaNova is faster than slua-unreal" or "faster than FUnLua";
- One cannot publish any three-way ratio, ranking, or "fastest" conclusion;
- Any such claim is invalid until all three projects have produced **aligned real
  run records**.

`Tools/CrossProject/README.md` states this explicitly: the framework "has not run
slua-unreal and FUnLua yet" and "until they run, any claim of being faster than
slua does not hold". This page agrees with that statement and does not overturn it.

`bootstrap_external.py status` is the machine-readable source of this fact: when
`results/` is empty it emits `conclusion: not-established`, and `comparison` is
always `null`.

## 2. The README's 2.08x is a self-comparison, NOT a comparison with slua-unreal / FUnLua

The **2.08x** in the main `README.md` refers to: on the historical UE 5.8.3
baseline, on the same machine and build, **UnLuaNova's Luau backend relative to
its Lua 5.5.1 backend** on a container workload (the
`Luau.Container.PointsAreCoplanar` case). The README itself declares the boundary:

- "The table below is a 2026-10-24 self-comparison on the historical UE 5.8.3 baseline";
- "This is a self-baseline: it guards against regressions";
- "The data covers Lua 5.5.1 and Luau only. It is not a same-machine comparison against
  slua-unreal / FUnLua, so no 'faster than slua' claim is made."

In other words, 2.08x is a **self-comparison between this project's two backends**;
it has nothing to do with slua-unreal or FUnLua and cannot support any cross-project
conclusion. The numbers in `Tools/baseline/perf-*.json` and
`Tools/CrossProject/README.md` are likewise self-baselines or an unrun framework,
not cross-project conclusions.

## 3. Current state: the framework is ready, but no aligned real runs exist

| Item | State |
|---|---|
| Workload contract (10 cases, iterations, warmup/sampling rules) | Ready - revision 1, see `WORKLOAD.md` |
| Normalizer + alignment + comparison table `runner.py` | Ready, with regressions |
| Single-project multi-run distribution + fail-closed non-time metrics `metrics.py` | Ready, with regressions |
| UnLuaNova implementation `workload/unluanova/cases.lua` | Written; pending a real run confirming every UE call is reachable |
| External-binding adapter `workload/external/cases.lua` | Written; pending target-project API verification |
| External project manifest `external_projects.json` | Written; **all pins are `UNKNOWN`**, licenses/UE versions recorded per verified evidence |
| Bootstrap script `bootstrap_external.py` | Ready - prepare / check / status, self-check runs offline |
| slua-unreal real run record | None (project is not in this repository, never built or run here) |
| FUnLua real run record | None (project is not in this repository, never built or run here; its license grant is also unverified) |
| Three-way aligned comparison table | None |

`external_projects.json` contains **no fabricated commit hash**: every project's
`pin` is the literal string `UNKNOWN`, and `pin_source` / `note` state that a
maintainer must fill in the exact commit actually built and run after a real
checkout, build and run. License evidence comes from
`Docs/CN/Comparison_sluaunreal.md` and `LICENSE-THIRD-PARTY.md`: slua-unreal is
BSD-3-Clause; FUnLua's audited source tree lacks `LICENSE.TXT`, so its MIT claim is
**unverified** and is recorded as `UNVERIFIED` with `license_verified: false`.

## 4. Reproduction steps (preconditions and flow for one-command third-party reproduction)

Only by following this order strictly, with every step really happening, can a
legitimate comparison table be produced.

1. **Prepare the external projects**: first run an offline self-check with
   `python -B Tools/CrossProject/bootstrap_external.py prepare --root <workspace> --offline`
   to confirm the manifest, directory structure and workload contract files are
   complete. The real clone/checkout is done by `prepare` (without `--offline`) once
   pins are fixed; the destination defaults to `<root>/Saved/CrossProject/external/`
   (`Saved/` is gitignored), or point `--root` at a directory outside the repository.
   **Do not clone into the repository.**
2. **Fix the commits**: for each project, perform a real checkout and write the
   exact commit hash **actually built and run** back into `external_projects.json`'s
   `pin`. While a pin is `UNKNOWN`, `prepare` fails closed and refuses to check out
   over the network (it will not pull a drifting HEAD); `check` also reports
   `pins_set: false`. All three projects must be built against the **same UE
   version**, recorded in `ue_version`.
3. **Align the same conditions**: all three projects must run on the **same machine,
   same engine, same build config, same carrier (Editor/Game), same case set, same
   iteration counts, same workload revision**. `machine` / `cpu` are supplied at run
   time via the workload's `SetMachineInfo(...)` using real, space-free values; the
   scripts never fill them for you and reject placeholders (see Section 5).
4. **Run**: put `workload/unluanova/cases.lua` into the UnLuaNova project and
   `workload/external/cases.lua` into slua-unreal / FUnLua (each with
   `SetProject(...)`), run in a real Editor or Game, and collect the
   `#UNLUANOVA_BENCH_BEGIN` ... `#UNLUANOVA_BENCH_END` log blocks. **First read
   `-- case failed:`**: when a UE call is unreachable the case is reported with
   `peropus=0` and the runner refuses the comparison; fix the call, do not delete
   the case.
5. **Use `metrics.py`'s fail-closed alignment checks**: first inspect the
   single-project multi-run distribution
   (`metrics.py results/*.json -o results/metrics.md`), and when needed force
   non-time metrics with `--require-metrics`. A mismatch in machine/CPU/engine/
   config/carrier/revision/project/backend/case set/iteration counts, missing
   samples, or a bad metric value is refused rather than smoothed into a nice-looking
   but meaningless table.
6. **Normalize and align**: feed all three logs together to
   `runner.py --baseline UnLuaNova <logs>`. It repeats the alignment check
   (Section 5); any mismatch prints a reason and exits with code 1, producing no
   table.
7. **A comparison table is legitimate ONLY after all three projects have aligned
   real runs**: `runner.py` writes `comparison.md` and each
   `<project>-<backend>.json` only when every run is aligned. **Until then,
   `status` always emits `conclusion: not-established`** and never infers which is
   faster.

## 5. Every alignment-contract field, and why it is fail-closed

The contract is enforced jointly by `runner.py` (alignment + normalization) and
`metrics.py` (single-project distribution + non-time metrics), and is defined in
`Tools/CrossProject/WORKLOAD.md` (revision 1).

### 5.1 Fields that must be byte-for-byte identical (`runner.ALIGNMENT_FIELDS`)

| Field | Meaning | Why identical / why fail-closed |
|---|---|---|
| `engine` | UE engine version | Bindings, reflection paths and optimizations differ across engines; cross-engine comparison is self-deception |
| `machine` | Host identity | Different machines have different CPU/memory/clock; absolute times are not comparable |
| `cpu` | CPU model | Same as above, and the dominant source of variance |
| `config` | Build config (Development / Shipping...) | Different optimization levels make numbers incomparable |
| `carrier` | Carrier (Editor / Game) | Editor overhead differs completely from a standalone game |
| `workload_revision` | Workload revision | Changing cases/iterations must bump the revision, otherwise different definitions are treated as comparable |

### 5.2 Meta fields that must all be reported (`runner.REQUIRED_META`)

`project`, `backend`, `engine`, `machine`, `cpu`, `config`, `carrier`,
`workload_revision`. If any is missing, the runner reports "missing required meta
field" and exits with code 1. A result with incomplete identity cannot be
attributed, let alone aligned.

### 5.3 Placeholder identity rejection

`machine` / `cpu` values of `""`, `?`, `fill-me`, `unknown`, `tbd`, `n/a`, `none`,
`unspecified`, or any `<...>` form are refused. The workload source has **no
default value at all**: without a `SetMachineInfo(...)` call, `Bench.Run()` refuses
to emit a block rather than printing a placeholder that the runner would only
discover later. A placeholder can make two unrelated runs "look aligned" - the most
common way such comparisons are faked.

### 5.4 Case set and iteration counts

The 10 case names and each case's iteration count must match item by item (see the
case table in `WORKLOAD.md`). A missing case, an extra case, or an iteration drift
makes the runner report "missing cases / extra cases" and `metrics.py` report
"case sets or iteration counts differ". **"Looking faster because a slow case was
not run" is the most common self-deception in this kind of comparison**, so the
tools refuse to emit a table. This is also why the instruction is "fix the call,
do not delete the case".

### 5.5 Optional non-time metrics (revision 1 extension, not forced by default)

Separate `#metric` lines, four items: `memory.peak_kb`(`kb`),
`startup.load_us`(`us`), `gc.pause_us`(`us`), `code_size.bytes`(`bytes`). The unit
enumeration is only `kb|us|bytes` and each metric has a fixed unit. If a value
cannot be measured, write `value=unavailable` - **never 0 and never a placeholder**;
`unavailable` does not enter the numeric distribution and is never treated as 0.
The following are all refused fail-closed (`metrics.py` raises `ValueError` /
`main` returns 1): a missing item, a metric set that differs across runs, an
unknown / mismatched / drifting unit, a non-finite number (NaN / inf), a negative
value, or a non-numeric value that is not `unavailable`. By default
(`--require-metrics` off) they are not forced and behaviour on old JSON is
unchanged.

### 5.6 Why the whole thing is fail-closed

A comparison across machines, across builds, with different case counts, with
placeholder identities, or with bad metrics produces a table that "looks usable"
but describes nothing. The choice of `runner.py` and `metrics.py` is: **refuse to
emit a table rather than emit a misleading one**. This fail-closed design is the
technical guarantee behind "a comparison table is legitimate only after aligned
real runs".

## 6. Writing results into `Tools/CrossProject/results/` (format)

`results/` is generated by `runner.py` after the first successful alignment. It is
NOT written by hand and NOT generated by `bootstrap_external.py`. **Do not
hand-write or fabricate any `results/*.json`.**

- Each real run produces one normalized JSON named `<project>-<backend>.json`
  (lowercase), e.g. `unluanova-luau.json`, `slua-unreal-lua.json`,
  `funlua-lua.json`. Top-level fields: `Project`, `Backend`, `EngineVersion`,
  `Machine`, `Cpu`, `BuildConfig`, `Carrier`, `WorkloadRevision`, `MeasuredUtc`,
  `SourceLog`, plus `Cases: [{Case, Iterations, PerOpUs, Samples}]` and optional
  `Metrics: [{Name, Value, Unit}]`. `Samples` is the raw sample array (5 segments,
  each a per-operation mean in microseconds), not just a median.
- `comparison.md`: the comparison table, written by `runner.py` only when all input
  runs are aligned.
- `metrics.md`: the **single-project** multi-run distribution written by
  `metrics.py`, explicitly labelled "not a cross-project conclusion".

`bootstrap_external.py status` reads only the **factual metadata** of these JSON
files (`Project`, `Backend`, `EngineVersion`, `Machine`, `Cpu`, `BuildConfig`,
`Carrier`, `WorkloadRevision`, `MeasuredUtc`, `SourceLog`, case count); it
**never reads the `PerOpUs` / `Samples` / `Metrics` values**, never computes a
ratio, never names a winner, and never infers speed. It reports "which projects
have real run records and which do not"; when `results/` is empty it emits
`record_state: empty` and `conclusion: not-established`. Even when all three have
records, `status` does not establish a conclusion on its own - that requires a
fail-closed `runner.py` alignment pass producing `comparison.md`.

`--root` determines which workspace's `<root>/Tools/CrossProject/results/` is read;
to read the canonical repository results, pass the repository root as `--root`
(the default), or specify `--results-dir` explicitly.

## 7. Why this comparison is NOT done in CI

`.github/workflows/ci.yml` does not run the three-way comparison because it cannot
satisfy the alignment preconditions on this page:

- It needs **real builds of three separate UE projects** (UnLuaNova, slua-unreal,
  FUnLua), which are large, slow, and depend on the Epic engine and each project's
  toolchain; CI runners do not have them;
- It needs real Editor/Game runs on the **same machine / same engine / same
  config**; CI environments are ephemeral, virtualized and heterogeneous, so
  hardware and engine version cannot be guaranteed consistent across jobs;
- It needs network clones of external repositories at fixed commits, while the
  repository policy forbids cloning into the repo and avoids introducing mutable
  external dependencies into CI;
- Performance numbers are machine-dependent and CI numbers cannot be published as
  an external conclusion.

Therefore CI runs only the pure-Python, offline-reproducible fail-closed contract
tests (`Tools/tests/test_crossproject_*.py` and `test_bootstrap_external.py`),
verifying "refuse misalignment, refuse placeholders, refuse fabrication, say
not-established when results are empty" - and **not** producing a cross-project
performance conclusion. The real comparison is a one-off manual procedure on a
controlled machine.

## 8. `bootstrap_external.py` quick reference

All three commands support `--dry-run` / `--offline` and complete their self-check
fully offline; stdout is always structured JSON (pure ASCII), and `check` and
`status` are read-only.

```powershell
# Offline validation: manifest, pins, directory structure, workload contract files;
# no network, no writes
python -B Tools/CrossProject/bootstrap_external.py prepare --root <workspace> --offline --dry-run

# Read-only readiness check: engine path, prepared external projects, contract files,
# whether metadata can align
python -B Tools/CrossProject/bootstrap_external.py check --root <workspace> --offline

# Honest status: which projects have real run records and which do not;
# when results are empty, conclusion=not-established
python -B Tools/CrossProject/bootstrap_external.py status --root <workspace>
```

- `prepare` (without `--offline`) clones/checks out only when **all pins are real
  fixed commits**; if any pin is `UNKNOWN` it fails closed and refuses before any
  network access, with a diagnosable error. A network or git failure returns a
  non-zero exit code and prints the reason - **it is never silently skipped**.
- `check` never guesses an engine installation and never fabricates an engine
  version / machine / CPU; when no engine path is found, `engine.found: false` and
  `diagnostics` carry a diagnosable error message.
- `status`'s `conclusion` is always `not-established` in the current state, and
  `comparison` is always `null`.

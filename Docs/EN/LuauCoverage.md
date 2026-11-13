---
title: Luau Coverage Export and Quality Gate
description: Export Luau line-hit counters to a reviewable JSON report and re-compute the denominator with a fail-closed gate.
---

# Luau Coverage Export and Quality Gate

`LUAU-COVERAGE` exports Luau's compile-time instrumented line-hit counters as a
**reviewable JSON report**, then re-computes the denominator with a fail-closed gate.
It is *not* a claim that "test coverage reached a percentage": a percentage only holds
for one real run, and it must be recomputed from the raw line counters by
`Tools/ValidateCoverage.py`.

## What it is and what it is not

| Is | Is not |
|---|---|
| Per-instrumentable-line hit counts read through Luau `lua_getcoverage` | A source-line coverage report (there is no source text mapping) |
| Per-source data named by reachable global table / `package.loaded` path | Branch coverage, argument coverage or C++ coverage |
| Raw data that can be merged across runs and recomputed by the gate | Execution evidence |
| Luau-backend only | A plain-Lua feature (that backend has no counters and the command reports unsupported) |

C closures have no Proto and no counters, so they are skipped on export. Traversal is
capped at depth 8 and uses a set of visited table pointers to break cycles. Only
chunks loaded **after** `lua.coverage` becomes greater than 0 carry `LOP_COVERAGE`
instructions; already-`require`d modules are not retrofitted. This matches the switch
semantics of `lua.codegen` and `lua.sandbox`.

## Collecting

```text
; Must be set before scripts are loaded. 1=statements, 2=statements and expressions
luau.coverage 1

lua.coverage status
lua.coverage dump                                  ; Saved/Coverage/coverage-Luau.json
lua.coverage dump D:/work/coverage-run-a.json     ; or an explicit path
```

`status` reports the current compilation level and notes that already-loaded chunks
have no counters; `dump` writes a full snapshot and logs the function count,
hit/instrumented lines and level. Snapshots are **cumulative**: exporting does not
reset the counters, so two consecutive dumps in the same VM produce increasing hit
counts, and the gate takes the per-line maximum when merging.

With no reachable Lua function, with no attached environment, or when the file cannot
be written, the command errors instead of emitting an empty report - an empty report
must never read as "0% passed".

## Export format

`schema_version` is fixed at 1. `exclusions` must be declared explicitly; the gate
rejects reports without exclusions so nobody can quietly shrink the denominator.
`totals` is written by the runtime but the gate **does not trust it**: it recomputes
every line and compares.

```json
{
  "schema_version": 1,
  "backend": "Luau",
  "coverage_level": 1,
  "generated_utc": "2026-11-03T00:00:00Z",
  "exclusions": ["IntelliSense", "<generated>"],
  "sources": [
    {
      "source": "_G.UnLuaNova.Event",
      "functions": [
        { "name": "Emit", "linedefined": 1, "depth": 0,
          "lines": [ { "line": 3, "hits": 7 }, { "line": 4, "hits": 0 } ] }
      ]
    }
  ],
  "totals": { "functions": 1, "covered_functions": 1,
              "instrumented_lines": 2, "hit_lines": 1 }
}
```

`hits: 0` means the line was instrumented but never executed. Luau uses -1 for "no
instrumentation instruction on this line"; export drops those entries, so
`instrumented_lines` counts genuinely instrumented lines rather than every line
number spanned by debug information.

## The gate

```powershell
python Tools/ValidateCoverage.py `
    --report Saved/Coverage/coverage-Luau.json `
    --require-source "_G.UnLuaNova.Event" `
    --min-coverage 0.5 `
    --out Saved/Coverage/coverage-gate.json
```

`--report` may be repeated to merge several runs or carriers. The gate:

- rejects missing, empty or malformed JSON, schema mismatches, backend mismatches and
  `coverage_level=0` reports;
- validates every `line` / `hits` as non-negative and rejects duplicate line numbers
  within a function, then recomputes all `totals`;
- requires `--require-source` to appear among the **non-excluded** sources;
- removes sources matching an exclusion from the denominator but still lists them; a
  zero denominator fails outright;
- merges reports by `(source, function, linedefined, depth, line)`, taking the maximum
  `hits`;
- fails when `hit_lines / instrumented_lines` is below `--min-coverage`;
- writes the verdict to `coverage-gate.json` (`passed` / `errors` / denominator / hit
  count / coverage) and exits 1 on failure.

`--backend Lua` is rejected directly: the plain Lua backend has no `lua_getcoverage`,
so coverage export applies to the Luau backend only. The gate's own unit tests live in
`Tools/tests/test_validate_coverage.py` (15 cases, no UE startup) and run with
`python -B -m unittest discover -s Tools/tests -p "test_*.py"`.

## Further reading

[Luau Backend](Luau.md) · [Console Commands](ConsoleCommand.md) · [Test Architecture and Acceptance](Testing_Architecture.md)

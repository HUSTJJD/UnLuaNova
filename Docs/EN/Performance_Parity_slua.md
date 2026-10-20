# Performance Acceptance

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

No controlled measurement establishes that UnLuaNova is faster than slua-unreal or FUnLua.
Upstream README multipliers, screenshots from other machines and single local numbers are not
product conclusions.

**What exists is a committed self-comparison baseline** for both backends, used as the Release
regression gate. It is not a cross-project comparison.

## Committed self-comparison baseline

`Tools/baseline/perf-lua.json` and `perf-luau.json` are committed. The original measurements come
from the independent Development Editor run `Saved/TestReports/FullFix-Complete-11` (2026-10-24).
Each case warms up for at least 250 ms, then takes five samples of at least 100 ms and reports the
median of the batch means. `ValidateRelease.py` requires matching backend, engine version, machine,
case list and iteration counts, and fails any case slower than 25% above the baseline.

| Case | Iterations | Lua 5.5.1 | Luau | Luau / Lua |
|---|---:|---:|---:|---:|
| `VM.Arithmetic` | 200k | 0.0104 µs | 0.0059 µs | 1.75× |
| `VM.Call` | 200k | 0.0263 µs | 0.0203 µs | 1.30× |
| `VM.TableRead` | 200k | 0.0092 µs | 0.0076 µs | 1.20× |
| `VM.TableGrow` | 200k | 0.0120 µs | 0.0154 µs | 0.78× |
| `Lua.Container.PointsAreCoplanar` | 20k | 22.570 µs | 10.868 µs | **2.08×** |
| `Reflection` (UE reflection call) | 20k | 0.5006 µs | 0.4590 µs | 1.09× |
| `Lua` (script-side call) | 20k | 1.5972 µs | 1.5429 µs | 1.04× |
| `Native` (direct C++ call, lower bound) | 20k | 0.0019 µs | 0.0017 µs | — |

Measurement conditions: Win64 · Editor Development · UE 5.8.3 · MSVC 14.50 ·
AMD Ryzen 7 4700G · machine `davidsjian-PC2`.

Lua 5.5.1 is faster on `VM.TableGrow` — recorded as measured, not cherry-picked.

## Latest Release run

The 2026-11-03 regular Release validation (`Saved/ReleaseTests/test`) failed the gate on five
Luau Editor cases (+43.8% to +55.8%); product sources, manifests and process priorities are
unchanged from the validated baseline run, so this is measurement-environment interference to be
re-checked, not a confirmed code regression. The committed baseline was **not** replaced and the
25% gate was **not** relaxed. See the [current version status](ReleaseNotes.md).

## Implemented paths

- Reflection descriptors and parameter caches come from UnLua; `CallUEFast` avoids repeated lookups.
- `UnLuaNovaEx` provides hand-written template exports; generated thunks specialise argument
  marshalling but still call `UObject::ProcessEvent`.
- The generator accepts a subset of signatures; static/net and complex struct/container paths must
  verify fallback cost — they are not "no-reflection direct calls".
- Luau vector operations avoid some userdata allocations; buffer interop is still an O(n) copy.
- CodeGen, frozen environments, compiler optimisations and GC settings change startup cost,
  debug capability and peak memory; they need separate measurement.

## Reproducible cross-project comparison

1. Pin UE / compiler, platform, hardware, power policy, plugin commit, VM commit, build
   configuration and optimisation flags.
2. Write equivalent adapters for all three projects, covering pure VM, Lua→UE / UE→Lua,
   reflection/template/thunk, out/ref, strings, objects, container batches, delegates, overrides,
   networking and GC.
3. Pass correctness assertions first, warm up, repeat, keep raw samples and p50/p95/p99, and
   record operation counts, time units and timer resolution.
4. Measure cold start, script compilation, allocations, peak memory, longest GC pause and code
   size; separate debug attached/detached and CodeGen on/off.
5. Isolate baselines per target and configuration, agree on the regression limit and noise band
   before gating; do not hide one regression behind another improvement.

## Current tooling

`Benchmark.spec.cpp` and `BenchmarkParity.spec.cpp` provide the `UnLuaNova.Perf.*` cases.
The default `RunTests.ps1` runs them and `ValidateRelease.py` enforces baseline scope, case
completeness and the 25% threshold.

Until the cross-project comparison exists, only implementation mechanics and self-comparison data
may be claimed — no external speed advantage. See the [current audit](Benchmark_Review.md).

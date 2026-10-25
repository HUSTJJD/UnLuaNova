# Performance References

`perf-lua.json` and `perf-luau.json` contain unmodified measurements from the independent
Development Editor run `Saved/TestReports/FullFix-Complete-11`, captured before the
subsequent Release validation. Each case warms up and records the median of five samples.
The release validator requires matching backend, engine version, machine, case inventory
and iteration counts, and fails when any case is more than 25% slower.

After intermittent short-sample failures in Release runs 17, 20 and 21, the current
sampler warms each case for at least 250 ms, then records five samples of at least
100 ms each. Each sample repeats the same iteration batch and reports elapsed time
per batch; the result is the median of these five batch means. Raw elapsed times and
batch counts are retained and independently checked by the release validator.
The reference files above remain byte-for-byte unchanged. Their older samples used
one batch each, so this comparison retains the same workload, per-batch iteration
count and per-operation units, but does not establish an improvement caused by the
runtime fixes. Sampling-method differences and background load remain limitations.

| Backend | UTC measurement | SHA-256 of the original JSON |
|---|---|---|
| Lua | 2026-10-24T14:50:50.335Z | A26D74B5D24315CD52B6DC65754D81E092BD62E9B3C97490130950EAEB11F82E |
| Luau | 2026-10-24T15:00:26.011Z | D70A6EF1882D81FAB2DBBBE935F019DF68AE5B99D2B1B67A70B5670A9BB04B27 |

Scope: Win64 Editor Development, UE 5.8.3, MSVC 14.50, AMD Ryzen 7 4700G,
machine `davidsjian-PC2`. Each backend's benchmark case passed. The Lua reference run
had one unrelated debugger fixture failure (tail calls removed frames); that fixture
was corrected before Release. Luau passed all 638 cases.

These are initial references for the corrected implementation, not measurements of
the crashing implementation and not evidence of a before/after speedup. Do not replace
them with a failed verification run to make a regression disappear. For another machine,
engine or intentional performance change, capture and review a separate reference run,
record its provenance here, then verify in a later run with unchanged references.

The other JSON files in this directory track test inventories and classification, not
performance. Release acceptance also compares the runtime manifest against actual
executed test paths; an old inventory cannot substitute for that check.

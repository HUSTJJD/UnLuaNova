# Performance Acceptance

No controlled measurements establish that UnLuaNova is faster than slua-unreal or FUnLua. README multipliers, different hardware and single samples are not product evidence.

UnLuaNova inherits reflection/parameter caching and adds CallUEFast, template exports and generated marshalling thunks. Generated thunks still call UObject::ProcessEvent; unsupported signatures fall back. Vector can avoid userdata allocations, while buffer conversion remains O(n). CodeGen and sandbox/compiler/GC settings have startup, memory and debugging tradeoffs.

Acceptance requires identical UE/compiler/hardware/build/VM pins and equivalent adapters. Cover pure VM, both crossing directions, reflection/template/thunk, out/ref, strings, objects, containers, delegates, overrides, networking and GC. Check correctness first, warm up, repeat, retain raw samples and p50/p95/p99, and measure cold startup, allocations, peak memory, longest GC pause and code size. Separate debug and CodeGen modes.

Benchmark.spec.cpp and BenchmarkParity.spec.cpp exist. The default RunTests.ps1 runs benchmarks and ValidateRelease.py requires comparable, complete baselines with a 25% regression limit. Tools/baseline/perf-*.json is not committed, so missing baselines fail release validation while retaining candidate measurements. Candidates are never automatically approved. Development warnings and historical test-name baselines cannot replace this gate. No benchmark or build was run during this change. See the [audit](Benchmark_Review.md).

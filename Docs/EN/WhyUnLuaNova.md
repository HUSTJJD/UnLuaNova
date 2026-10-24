# Product Goals and Design

UnLuaNova inherits UE reflection, overrides, containers and delegates from Tencent/UnLua. Its goal combines slua-unreal workflows, FUnLua ownership ideas and Luau language/tooling with consistent lifecycle rules and reproducible validation. A project name or test count cannot establish an upgrade.

Priorities are explicit capability contracts, traceable ownership across Env/World/UObject/coroutines, measured fast paths with equivalent fallback, reproducible tool outputs, and capability policies for trusted versus untrusted scripts.

Proposed innovations include generation-aware Env/World callback handles, shared signature metadata for bindings and declarations, deterministic network schemas, and an exported capability manifest. These are roadmap items.

See the [comparison](Comparison_sluaunreal.md), [audit](Benchmark_Review.md) and [matrix](../feature-matrix.md).

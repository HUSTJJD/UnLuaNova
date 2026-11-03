# Product Goals and Design

UnLuaNova inherits UE reflection, overrides, containers and delegates from Tencent/UnLua. Its goal combines slua-unreal workflows, FUnLua ownership ideas and Luau language/tooling with consistent lifecycle rules and reproducible validation. A project name or test count cannot establish an upgrade.

Priorities are explicit capability contracts, traceable ownership across Env/World/UObject/coroutines, measured fast paths with equivalent fallback, reproducible tool outputs, and capability policies for trusted versus untrusted scripts.

Proposed innovations include generation-aware Env/World callback handles, shared signature metadata for bindings and declarations, and an exported capability manifest. Network schemas now have a deterministic per-condition layout (eight conditions, addresses derived from the declaration, a SchemaVersion fingerprint) and arbitrary `UStruct` / container type specs, but remain **experimental**: connected owner/non-owner acceptance, eight connected scenarios and the Legacy / PushOff / Iris / Cooked variants all have source and gates yet none has been executed, so they are not published as a mature capability. See [networking](LuaNetworking.md).

See the [comparison](Comparison_sluaunreal.md), [audit](Benchmark_Review.md) and [matrix](../feature-matrix.md).

# Product Goals and Design

UnLuaNova inherits UE reflection, overrides, containers and delegates from Tencent/UnLua. Its goal combines slua-unreal workflows, FUnLua ownership ideas and Luau language/tooling with consistent lifecycle rules and reproducible validation.

1. **Capabilities have contracts**: backend, version, defaults, failure modes, platforms and
   safety boundaries are queryable, and capability limits are explicitly marked.
2. **Ownership is explainable**: Env, World, UObject, coroutines, callbacks and borrowed memory
   have clear ownership and invalidation rules.
3. **Fast paths are provable**: generated bindings and reflection fallback behave equivalently,
   with gains and costs measured in same-scenario benchmarks.
4. **Tools form a closed loop**: source, declarations, examples, specs, build identity and runtime
   artefacts are traceable.
5. **Security is understandable**: trusted game scripts and untrusted extensions use different
   capability configurations, and a sandbox switch is not a substitute for a permission model.

Proposed innovations include generation-aware Env/World callback handles, shared signature metadata for bindings and declarations, and an exported capability manifest. Network schemas have a deterministic per-condition layout (eight conditions, addresses derived from the declaration, a SchemaVersion fingerprint) and arbitrary `UStruct` / container type specs; declared networking was promoted to **Stable** on 2026-11-15 with all four carriers and eight connected scenarios verified. See [networking](LuaNetworking.md).

See the [comparison](Comparison_sluaunreal.md).

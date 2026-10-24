# Lua-declared Networking (Experimental)

RPC and typed replicated fields exist in source, but **replication conditions and int64 have correctness blockers**. This API description is not a production/Iris compatibility guarantee. See [NET-01 / NET-02](Benchmark_Review.md).

```lua
local M = UnLuaNova.Class()
M.ServerRPC = {
    Ping = true,
    Chat = { Reliable = true, Params = { "Int", "String" } },
}
function M:Ping() end
function M:Chat(Channel, Text) end
function M:GetLifetimeReplicatedProps()
    return { { Name = "HP", Type = "Float" } }
end
return M
```

ClientRPC and MulticastRPC use the same declaration form. Parameters can use type strings or named entries. Supported types are Bool, Int/Int32, Int64, Float, Double, String/FString, Name, Vector, Rotator, Transform and Object, not arbitrary structs/containers.

Add UUnLuaNovaReplicationComponent and configure actor/component replication and ownership. ReplicatedProps can be a static table. Polled mode reads fields at the configured interval (default 0.1s); Explicit mode requires MarkFieldDirty. MarkAllFieldsDirty, FlushNetUpdate, OnRep_<Name> and OnReplicatedProps(Names) also exist. A flush request does not guarantee same-frame delivery.

RPC names are sorted within each kind. Inheritance, existing functions, net caches, reload and cooked ID consistency still require acceptance. Replication uses eleven typed arrays; declaration order selects indices. Schema fingerprints include names, order, types and conditions, but a 32-bit fingerprint is not a security mechanism or full version negotiation.

## Limits

- Same-type fields share a condition; conflicts select the first non-None value. Conditions are derived from an instance owner although UE replication layouts are class-cached. Do not rely on current OwnerOnly declarations to protect private fields.
- Unknown conditions fall back to None; invalid declarations need rejection.
- Luau int64 network reads use the int32 integer API.
- Math values require matching userdata; object resolution and lifetime need tests.
- Typed UPROPERTY and push-model macros do not prove Iris compatibility, element-level deltas, compression or runtime fallback.
- Reload, inheritance, duplicate declarations, peer schema mismatch and authorization need coverage.

API.Net tests declaration behavior; ClientServer covers PIE; DedicatedServer compares separate process declarations without proving connected transport; Slow.Net.Multicast describes a server plus two connected clients. None was executed during this audit. Required additions include owner/nonowner, multiple actors, numeric edges, late join/reconnect, dormancy/relevancy, packet loss and packaged legacy/Iris/push-model combinations.

[Testing](Testing.md) | [Object binding](LuaDrivenObjects.md) | [Capability matrix](../feature-matrix.md)

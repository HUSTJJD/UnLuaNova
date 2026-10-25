# Lua-declared Networking (Experimental)

RPC and typed replicated fields exist in source. **Non-None conditions reject the entire declaration** until conditional layouts are implemented. Exact int64 conversion and validation are implemented but unbuilt and unexecuted. This is not a production/Iris compatibility guarantee. See [NET-01 / NET-02](Benchmark_Review.md).

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

Unknown parameter types, duplicate parameter names, sparse arrays, invalid Params/Reliable types and parameter construction failures reject the entire RPC instead of shortening its signature.

Add UUnLuaNovaReplicationComponent and configure actor/component replication and ownership. ReplicatedProps can be a static table. Polled mode reads fields at the configured interval (default 0.1s); Explicit mode requires MarkFieldDirty. MarkAllFieldsDirty, FlushNetUpdate, OnRep_<Name> and OnReplicatedProps(Names) also exist. A flush request does not guarantee same-frame delivery.

RPC names are sorted within each kind. Inheritance, existing functions, net caches, reload and cooked ID consistency still require acceptance. Replication uses eleven typed arrays; declaration order selects indices. Schema fingerprints include names, order, types and conditions, but a 32-bit fingerprint is not a security mechanism or full version negotiation.

Property declarations require dense arrays, unique non-None FNames, known types and omitted/None conditions. Empty first declarations are valid. Cached class schemas are immutable; identical redeclarations succeed, invalid or changed declarations preserve the existing schema without partial registration. Schema changes require a full network/environment restart. Ownership of global declaration-cache cleanup across multiple Envs still needs work.

## Bind Before Connecting

Dynamic RPCs must be registered before either NetDriver first caches the class. Binding when the client's first replicated actor is created can be too late: class/archetype checksum resolution may already have cached a field table without the RPCs. Add loaded network classes to the project's `Config/DefaultUnLuaNova.ini`:

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+PreBindClasses=/Script/MyGame.MyNetworkActor
```

Configured classes bind when Lua activates and are checked again after engine initialization, including native classes registered by Default-phase modules. This resolves loaded classes only; Blueprint network classes must be loaded and bound before connecting. Both peers need identical declarations, and RPC layouts must remain unchanged during a network session. The demo config includes `UnLuaNovaTestNetActor`.

## Limits

- Component layouts now use fixed None conditions, independent of instance owners. OwnerOnly and other conditional declarations are rejected. Old assets with a non-None component Condition disable replication at BeginPlay. Full conditional layouts remain P0 work.
- int32/int64 reject fractions, NaN, infinities, overflow and numeric strings, preserving the old value. Luau emits numbers within +/-2^53 and native integer64 outside it. Already-rounded floating point values cannot be recovered; use integer64/C++ for larger exact values.
- Math values require matching userdata; object resolution and lifetime need tests.
- Typed UPROPERTY and push-model macros do not prove Iris compatibility, element-level deltas, compression or runtime fallback.
- Reload, inheritance, duplicate declarations, peer schema mismatch and authorization need coverage.

API.Net tests declaration behavior; ClientServer covers PIE; Slow.Net.Multicast runs a server plus two connected clients. Through Release22, DedicatedServer only compared disconnected declarations, so it did not establish DS/client transport. Its replacement connects one DS and two independent clients, with two server-spawned actors, ownership assigned only on the server, and two rounds of Server/Client/Multicast RPCs and Lua HP replication. Release28 passed this scenario and raw session/PID/HP/local-log validation in all four carriers, with all 33 release stages passed; see the [regression validation record](Regression_20261024.md).

The server generates the session identity and per-actor random challenges at runtime; clients learn them through actor replication. Both clients acknowledge observing round one's HP and exact RPC logs through Server RPCs; the server then authorizes round two through Client RPCs. This barrier prevents a fast client's next write from replacing the first-round value before a slower client observes it. Client1 deliberately waits at least five seconds before its first-round observation, and the gate checks that duration. Non-owners must never receive the targeted Client RPC. JSON files carry output evidence and coordinate shutdown, not gameplay inputs. The release validator requires matching session identity, open connections, three distinct PIDs, two distinct owners, and both rounds' raw field/call observations. Matching declaration IDs alone cannot pass.

Development diagnostics support `-UnLuaNovaNetFault=SkipServerRpc|SkipClientRpc|SkipReplication` to check that missing traffic fails; fault-injected runs are not acceptance evidence. Remaining coverage includes rejecting unauthorized Server RPC attempts, mixed replication conditions, numeric edges, late join/reconnect, dormancy/relevancy, packet loss and packaged legacy/Iris/push-model combinations.

[Testing](Testing.md) | [Object binding](LuaDrivenObjects.md) | [Capability matrix](../feature-matrix.md)

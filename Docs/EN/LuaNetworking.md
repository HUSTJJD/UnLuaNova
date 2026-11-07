# Lua-declared Networking (Experimental)

RPC and typed replicated fields are available, including per-field replication conditions.

**Supported conditions**: `None`, `InitialOnly`, `OwnerOnly`, `SkipOwner`, `SimulatedOnly`, `AutonomousOnly`, `SimulatedOrPhysics`, `InitialOrOwner`. Each one has its own storage group; a field is sent under the condition of the property it lives in.

**Rejected conditions**: every condition with no storage group - `Custom` / `Dynamic` (the engine refuses custom conditions on subobjects), `Never` / `NetGroup`, and the Replay* family (`ReplayOrOwner`, `ReplayOnly`, `SimulatedOnlyNoReplay`, `SimulatedOrPhysicsNoReplay`, `SkipReplay`). Their names parse, so "typo" and "condition not supported" are two distinct errors, but the whole declaration fails with an Error and is **not downgraded to None** - silently sending an owner-only value to every connection would be worse than refusing it.

Exact int64 conversion and validation run on both backends. Lua-declared networking is experimental and is not a release guarantee.

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

ClientRPC and MulticastRPC use the same declaration form. Parameters can use type strings or named entries. Builtin types are Bool, Int/Int32, Int64, Float, Double, String/FString, Name, Vector, Rotator, Transform and Object. `Type` also accepts a **type spec**:

| Syntax | Meaning |
|---|---|
| `Struct:/Script/Engine.Vector` or `/Script/Engine.Vector` | any resolvable `UScriptStruct` |
| `Array:<spec>` | `TArray` |
| `Set:<spec>` | `TSet` |
| `Map:<keyspec>:<valuespec>` | `TMap` (key spec must be hashable) |

A `UClass` cannot grow a `UPROPERTY` at runtime, but a replication field does not need one: it travels in a shared `Generic` bucket as a canonical self-describing text payload. RPC parameters are even less constrained because their `UFunction` is built at runtime, so `CreateParam` can attach a real `FStructProperty` / `FArrayProperty` / `FSetProperty` / `FMapProperty` and let UnLua's `FFunctionDesc` do the marshalling. Specs are part of `SchemaVersion`, so peers with different declarations cannot misdecode. `UObject` references keep the dedicated Object bucket because a text payload cannot carry a NetGUID.

Specs are validated as a whole at bind time: unknown keywords, missing children, unresolvable `UScriptStruct` paths, non-hashable Map keys and excessive nesting are rejected rather than producing an undecodable payload at runtime.

Unknown parameter types, duplicate parameter names, sparse arrays, invalid Params/Reliable types and parameter construction failures reject the entire RPC instead of shortening its signature.

Add UUnLuaNovaReplicationComponent and configure actor/component replication and ownership. ReplicatedProps can be a static table. Polled mode reads fields at the configured interval (default 0.1s); Explicit mode requires MarkFieldDirty. MarkAllFieldsDirty, FlushNetUpdate, OnRep_<Name> and OnReplicatedProps(Names) also exist. A flush request does not guarantee same-frame delivery.

RPC names are sorted within each kind. A subclass with no declaration of its own inherits the parent class's schema. Replication uses eleven builtin typed arrays per condition group plus a twelfth shared `Generic` group for type-spec fields; declaration order selects indices within (type, condition). Schema fingerprints include names, order, types, type specs and conditions, but a 32-bit fingerprint is not a security mechanism or full version negotiation.

Property declarations require dense arrays, unique non-None FNames and known types or valid type specs; `Condition` may be omitted (`None`) or name any of the supported conditions. Indices are counted within **(type, condition)**: two OwnerOnly Strings and one unconditional String each start at index 0 of their own group's String array. Empty first declarations are valid. Cached class schemas are immutable; identical redeclarations succeed, invalid or changed declarations preserve the existing schema without partial registration. Schema changes require a full network/environment restart. The global declaration cache uses a **joint owner set**: when several environments bind an identical declaration list, each registers as an owner and `Cleanup(Env)` only removes itself. The schema is released by the last environment to close, in any order. Entries with no owner remain process-wide and only `Cleanup(nullptr)` drops them.

## Bind Before Connecting

Dynamic RPCs must be registered before either NetDriver first caches the class. Binding when the client's first replicated actor is created can be too late: class/archetype checksum resolution may already have cached a field table without the RPCs. Add loaded network classes to the project's `Config/DefaultUnLuaNova.ini`:

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+PreBindClasses=/Script/MyGame.MyNetworkActor
```

Configured classes bind when Lua activates and are checked again after engine initialization, including native classes registered by Default-phase modules. This resolves loaded classes only; Blueprint network classes must be loaded and bound before connecting. Both peers need identical declarations, and RPC layouts must remain unchanged during a network session. The demo config includes `UnLuaNovaTestNetActor`.

## Limits

- Lua-declared networking is **experimental**: its API and behaviour may change. Engine-level seamless travel (`ServerTravel`) is out of scope - reconnection uses a `UNetConnection` close plus an in-process reopen.
- The component-wide `Condition` property is obsolete: a non-`None` value logs a warning and is ignored. Declare conditions per field in `GetLifetimeReplicatedProps`.
- A conditional group is dirtied as a whole because it is one UPROPERTY; unconditional fields keep per-type dirty tracking.
- int32 / int64 reject fractions, NaN, infinities, overflow and numeric strings, preserving the old value. Luau emits numbers within +/-2^53 and native integer64 outside it.
- Type-spec fields use a canonical text payload rather than the engine's per-property deltas; `UObject` references are outside the spec grammar.
- Iris / push-model compatibility and runtime fallback are not claimed. Schema changes require a full network / environment restart.
- Hot reload and authorization checks are not part of the declarative networking surface.

## See also

[Testing](Testing.md) | [Lua-driven objects](LuaDrivenObjects.md) | [Settings](Settings.md)

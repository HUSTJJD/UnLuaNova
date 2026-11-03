# Lua-declared Networking (Experimental)

RPC and typed replicated fields exist in source, including per-field replication conditions.

**Supported conditions**: `None`, `InitialOnly`, `OwnerOnly`, `SkipOwner`, `SimulatedOnly`, `AutonomousOnly`, `SimulatedOrPhysics`, `InitialOrOwner`. Each one has its own storage group; a field is sent under the condition of the property it lives in.

**Rejected conditions**: every condition with no storage group - `Custom` / `Dynamic` (the engine refuses custom conditions on subobjects), `Never` / `NetGroup`, and the Replay* family (`ReplayOrOwner`, `ReplayOnly`, `SimulatedOnlyNoReplay`, `SimulatedOrPhysicsNoReplay`, `SkipReplay`). Their names parse, so "typo" and "condition not supported" are two distinct errors, but the whole declaration fails with an Error and is **not downgraded to None** - silently sending an owner-only value to every connection would be worse than refusing it.

Exact int64 conversion and validation run on both backends. Connected owner / non-owner acceptance, late join / reconnect, dormancy / relevancy and the Legacy / PushOff / Iris delivery matrix now have scenario source and release gates, but none has an execution run; this is not a production guarantee. See [NET-01 / NET-02](Benchmark_Review.md).

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

The earlier claim that arbitrary UStruct / container support is an architecture boundary was wrong. A `UClass` cannot grow a `UPROPERTY` at runtime, but a replication field does not need one: it travels in a shared `Generic` bucket as a canonical self-describing text payload. RPC parameters are even less constrained because their `UFunction` is built at runtime, so `CreateParam` can attach a real `FStructProperty` / `FArrayProperty` / `FSetProperty` / `FMapProperty` and let UnLua's `FFunctionDesc` do the marshalling. Specs are part of `SchemaVersion`, so peers with different declarations cannot misdecode. `UObject` references keep the dedicated Object bucket because a text payload cannot carry a NetGUID.

Specs are validated as a whole at bind time: unknown keywords, missing children, unresolvable `UScriptStruct` paths, non-hashable Map keys and excessive nesting are rejected rather than producing an undecodable payload at runtime.

Unknown parameter types, duplicate parameter names, sparse arrays, invalid Params/Reliable types and parameter construction failures reject the entire RPC instead of shortening its signature.

Add UUnLuaNovaReplicationComponent and configure actor/component replication and ownership. ReplicatedProps can be a static table. Polled mode reads fields at the configured interval (default 0.1s); Explicit mode requires MarkFieldDirty. MarkAllFieldsDirty, FlushNetUpdate, OnRep_<Name> and OnReplicatedProps(Names) also exist. A flush request does not guarantee same-frame delivery.

RPC names are sorted within each kind. Existing functions, net caches, reload and cooked ID consistency still require acceptance; a subclass with no declaration of its own now inherits the parent class's schema. Replication uses eleven builtin typed arrays per condition group plus a twelfth shared `Generic` group for type-spec fields; declaration order selects indices within (type, condition). Schema fingerprints include names, order, types, type specs and conditions, but a 32-bit fingerprint is not a security mechanism or full version negotiation.

Property declarations require dense arrays, unique non-None FNames and known types or valid type specs; `Condition` may be omitted (`None`) or name any of the supported conditions. Indices are counted within **(type, condition)**: two OwnerOnly Strings and one unconditional String each start at index 0 of their own group's String array. Empty first declarations are valid. Cached class schemas are immutable; identical redeclarations succeed, invalid or changed declarations preserve the existing schema without partial registration. Schema changes require a full network/environment restart. The global declaration cache is torn down per Env: `Cleanup(Env)` removes only that Env's entries and dead classes, orphan entries belong to process-wide cleanup, so closing one Env no longer wipes another's schema; an execution run for multi-Env declaration/teardown order is still needed.

## Bind Before Connecting

Dynamic RPCs must be registered before either NetDriver first caches the class. Binding when the client's first replicated actor is created can be too late: class/archetype checksum resolution may already have cached a field table without the RPCs. Add loaded network classes to the project's `Config/DefaultUnLuaNova.ini`:

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+PreBindClasses=/Script/MyGame.MyNetworkActor
```

Configured classes bind when Lua activates and are checked again after engine initialization, including native classes registered by Default-phase modules. This resolves loaded classes only; Blueprint network classes must be loaded and bound before connecting. Both peers need identical declarations, and RPC layouts must remain unchanged during a network session. The demo config includes `UnLuaNovaTestNetActor`.

## Limits

- The component-wide `Condition` property is obsolete: a non-None value logs a Warning and is ignored, it no longer disables replication. Declare conditions per field in `GetLifetimeReplicatedProps`.
- A conditional group is dirtied as a whole rather than per type, because a group is one UPROPERTY. Unconditional fields still use per-type dirty tracking. **Connected owner / non-owner acceptance remains P0** (assertion source and gate delivered, not yet executed) - single-process cases prove grouping and indices, not that a non-owner really received nothing.
- int32/int64 reject fractions, NaN, infinities, overflow and numeric strings, preserving the old value. Luau emits numbers within +/-2^53 and native integer64 outside it. Already-rounded floating point values cannot be recovered; use integer64/C++ for larger exact values.
- Math values require matching userdata; object resolution and lifetime need tests. Late join now has scenario source and a gate, but no execution run.
- Type-spec fields use a canonical text payload, not the engine's per-property deltas: it proves "declarable, round-trippable, and safe against a mismatched peer", not bandwidth or compression gains. `UObject` references are outside the spec grammar.
- Typed UPROPERTY and push-model macros do not prove Iris compatibility, element-level deltas, compression or runtime fallback. The Legacy / PushOff / Iris launch switches and their gate are delivered in source; no variant has been executed.
- A conditional group is dirtied as a whole rather than per type: it is one UPROPERTY, so any change inside it re-sends the group. Unconditional fields keep per-type dirty tracking.
- Reload, duplicate declarations, **cross-peer** schema mismatch and authorization still need connected acceptance. Single-process cases cover parent-class lookup, idempotent redeclaration, and refusing to write Lua fields on a schema mismatch.

API.Net tests declaration behavior; ClientServer covers PIE; Slow.Net.Multicast runs a server plus two connected clients. Through Release22, DedicatedServer only compared disconnected declarations, so it did not establish DS/client transport. Its replacement connects one DS and two independent clients, with two server-spawned actors, ownership assigned only on the server, and two rounds of Server/Client/Multicast RPCs and Lua HP replication. Release28 passed this scenario and raw session/PID/HP/local-log validation in all four carriers, with all 33 release stages passed; see the [regression validation record](Regression_20261024.md).

The server generates the session identity and per-actor random challenges at runtime; clients learn them through actor replication. Both clients acknowledge observing round one's HP and exact RPC logs through Server RPCs; the server then authorizes round two through Client RPCs. This barrier prevents a fast client's next write from replacing the first-round value before a slower client observes it. Client1 deliberately waits at least five seconds before its first-round observation, and the gate checks that duration. Non-owners must never receive the targeted Client RPC. JSON files carry output evidence and coordinate shutdown, not gameplay inputs. The release validator requires matching session identity, open connections, three distinct PIDs, two distinct owners, and both rounds' raw field/call observations. Matching declaration IDs alone cannot pass.

Development diagnostics support `-UnLuaNovaNetFault=SkipServerRpc|SkipClientRpc|SkipReplication` to check that missing traffic fails; fault-injected runs are not acceptance evidence.

**Delivered in source, not yet executed**:

- A connected assertion that a non-owner Server RPC is dropped (the engine's `UNetDriver::ShouldCallRemoteFunction` already requires `RepFlags.bNetOwner`; the scenario sends a unique token and `ValidateRelease.py` requires both that the attempt happened and that the token appears in no peer's `CallLog`).
- Mixed conditions in one declaration list (`HP` unconditional + `OwnerSecret` OwnerOnly, with per-peer visibility exported for the gate).
- Arbitrary `UStruct` / containers: dynamic FProperty RPC parameters, the Generic-bucket canonical payload for replicated fields, and parsing / nesting / round-trip unit cases. The unit cases are source, not a run.
- Four connected scenarios (`LuaNetNetScenariosTest.cpp`): late join (the server spawns both actors while only one client is connected and writes a `Spawn` marker; topology must show Client2 launched later), process-level reconnect (`RestartPeer` replaces Client1; topology requires `launch_count=2` with `first_pid != pid`, and the server must observe a drop and recovery), dormancy (clients see no dormant change, then the flushed value), and relevancy (an owner-only actor is invisible to the non-owner).
- `Tools/RunNetMatrix.ps1` + `ValidateRelease.py --net-matrix`: Legacy / PushOff / Iris variants. Each variant passes a real engine switch to every peer (`-UseIrisReplication=1` or an ini override of the push-model cvar), `AppendWorldFacts` reads the resulting cvar state back into the report, and the gate rejects a renamed directory standing in for a variant.

**All of the above need a rebuild and real DS runs to become evidence - source existing is not a test passing.** The entry point is `Tools/RunTests.ps1 -Profile Development -NetMatrix` (backed by `Tools/RunNetMatrix.ps1`). Every cell launches three real processes, so it is not part of the ordinary test matrix or the normal Release stages.

**Still to implement / uncovered**: packet loss and reordering, seamless travel and `UNetConnection`-level reconnect (the delivered scenario replaces a process), cross-peer declaration mismatch, cooked carriers for the scenarios, Iris element-level deltas and compression gains, and the actual execution of every variant. Integer edges, scalar/Object bucket round-trips, schema mismatch and parent-class lookup are covered by unit cases.

## When the "experimental" label can come off

NET-RPC / NET-PROPS / NET-TYPES are still `experimental` in the capability matrix: that means
"not a release guarantee", while `partial` means "known gaps" and `implemented` means "source exists
within the stated boundary". `status` may be edited by hand (only `verification` is forbidden), but
it is **bought with evidence, not with words**. Moving to `partial` requires all of:

1. Rebuild Editor / Game / Server and run a complete `Tools/RunTests.ps1`, producing a new
   `Saved/ReleaseTests/baseline/` (or an equivalent full run).
2. `ValidateRelease.py` DedicatedServer gate fully green, including the two new assertions above:
   the non-owner Server RPC token executed on no peer, and the OwnerOnly field visible only to
   the owner.
3. Record that run's Editor / Game automation reports into `Tools/evidence/` with
   `RecordEvidence.py --commit <that run's commit>`; the records must not be `historical`.
4. `ValidateRelease.py --net-matrix` fully green: four scenario topologies and three variant topologies, each with three real peer reports, non-empty logs and PIDs matching topology, plus Iris / push-model state read back from cvars for the variants.
5. The `release-status` marker is updated from that Release result. It is **currently
   `not-passed`** (Luau Editor performance exceeded the 25% gate) and can only be changed by a
   genuinely passing Release run, never by editing documentation.

Until 1-5 are done, the README must keep the "(experimental)" label and the hard limits. Keeping
`status` at `experimental` is not an omission - it states accurately that source is delivered but
execution has not been accepted.

[Testing](Testing.md) | [Object binding](LuaDrivenObjects.md) | [Capability matrix](../feature-matrix.md)

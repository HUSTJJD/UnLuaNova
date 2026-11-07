# Lua-declared Networking (Experimental)

RPC and typed replicated fields exist in source, including per-field replication conditions.

**Supported conditions**: `None`, `InitialOnly`, `OwnerOnly`, `SkipOwner`, `SimulatedOnly`, `AutonomousOnly`, `SimulatedOrPhysics`, `InitialOrOwner`. Each one has its own storage group; a field is sent under the condition of the property it lives in.

**Rejected conditions**: every condition with no storage group - `Custom` / `Dynamic` (the engine refuses custom conditions on subobjects), `Never` / `NetGroup`, and the Replay* family (`ReplayOrOwner`, `ReplayOnly`, `SimulatedOnlyNoReplay`, `SimulatedOrPhysicsNoReplay`, `SkipReplay`). Their names parse, so "typo" and "condition not supported" are two distinct errors, but the whole declaration fails with an Error and is **not downgraded to None** - silently sending an owner-only value to every connection would be worse than refusing it.

Exact int64 conversion and validation run on both backends. Connected owner / non-owner acceptance, all eight connected scenarios (including join-order swaps, packet loss and `UNetConnection`-level reconnect) and the Legacy / PushOff / Iris / Cooked delivery matrix now have scenario source and release gates, but none has an execution run; this is not a production guarantee. See [NET-01 / NET-02](Benchmark_Review.md).

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

Property declarations require dense arrays, unique non-None FNames and known types or valid type specs; `Condition` may be omitted (`None`) or name any of the supported conditions. Indices are counted within **(type, condition)**: two OwnerOnly Strings and one unconditional String each start at index 0 of their own group's String array. Empty first declarations are valid. Cached class schemas are immutable; identical redeclarations succeed, invalid or changed declarations preserve the existing schema without partial registration. Schema changes require a full network/environment restart. The global declaration cache uses a **joint owner set**: when several environments bind an identical declaration list, each registers as an owner and `Cleanup(Env)` only removes itself. The schema is released by the last environment to close, in any order (closing the first owner used to wipe a schema another Env still used). Entries with no owner remain process-wide and only `Cleanup(nullptr)` drops them. Two reverse-order unit cases cover it; a connected execution run is still pending.

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
- Math values require matching userdata; object resolution and weak-reference invalidation have unit cases, while connected object-lifetime observation is still part of the unexecuted scenarios. Late join has scenario source and a gate, but no execution run.
- Type-spec fields use a canonical text payload, not the engine's per-property deltas: it proves "declarable, round-trippable, and safe against a mismatched peer", not bandwidth or compression gains. `UObject` references are outside the spec grammar.
- Typed UPROPERTY and push-model macros do not prove Iris compatibility, element-level deltas, compression or runtime fallback. The Legacy / PushOff / Iris / Cooked launch switches, connection byte counters and their gate are delivered in source; no variant has been executed.
- A conditional group is dirtied as a whole rather than per type: it is one UPROPERTY, so any change inside it re-sends the group. Unconditional fields keep per-type dirty tracking.
- Reload and authorization still need connected acceptance. Single-process cases cover parent-class lookup, idempotent redeclaration, order-independent multi-Env teardown, weak-object invalidation and refusing to write Lua fields on a schema mismatch; **cross-peer** schema mismatch is covered by `RepSchemaVersion` report export and the gate (not yet executed).

API.Net tests declaration behavior; ClientServer covers PIE; Slow.Net.Multicast runs a server plus two connected clients. Through Release22, DedicatedServer only compared disconnected declarations, so it did not establish DS/client transport. Its replacement connects one DS and two independent clients, with two server-spawned actors, ownership assigned only on the server, and two rounds of Server/Client/Multicast RPCs and Lua HP replication. Release28 passed this scenario and raw session/PID/HP/local-log validation in all four carriers, with all 33 release stages passed; see the [regression validation record](Regression_20261024.md).

The server generates the session identity and per-actor random challenges at runtime; clients learn them through actor replication. Both clients acknowledge observing round one's HP and exact RPC logs through Server RPCs; the server then authorizes round two through Client RPCs. This barrier prevents a fast client's next write from replacing the first-round value before a slower client observes it. Client1 deliberately waits at least five seconds before its first-round observation, and the gate checks that duration. Non-owners must never receive the targeted Client RPC. JSON files carry output evidence and coordinate shutdown, not gameplay inputs. The release validator requires matching session identity, open connections, three distinct PIDs, two distinct owners, and both rounds' raw field/call observations. Matching declaration IDs alone cannot pass.

Development diagnostics support `-UnLuaNovaNetFault=SkipServerRpc|SkipClientRpc|SkipReplication` to check that missing traffic fails; fault-injected runs are not acceptance evidence.

**Delivered in source, not yet executed**:

- A connected assertion that a non-owner Server RPC is dropped (the engine's `UNetDriver::ShouldCallRemoteFunction` already requires `RepFlags.bNetOwner`; the scenario sends a unique token and `ValidateRelease.py` requires both that the attempt happened and that the token appears in no peer's `CallLog`).
- Mixed conditions in one declaration list (`HP` unconditional + `OwnerSecret` OwnerOnly, with per-peer visibility exported for the gate).
- Arbitrary `UStruct` / containers: dynamic FProperty RPC parameters, the Generic-bucket canonical payload for replicated fields, and parsing / nesting / round-trip unit cases. `NetActor.lua` also declares a real `Inventory` (`Array:Int`) field and a `SendPayload` RPC whose parameter is `Struct:/Script/CoreUObject.Vector`. The declaration report derives each parameter's spec from the **actual FProperty**, not from the Lua script, and the gate requires a Generic field plus a UStruct parameter on all three peers. Registration is the assertion: if `CreateParam` failed, the RPC would not be listed. The unit cases are source, not a run.
- Cross-peer declaration identity: every peer exports the `RepSchemaVersion` it actually bound and a deliberately different `VariantRepSchemaVersion`; the gate requires the former to agree and the latter to differ, so "all equal" cannot be vacuous.
- Eight connected scenarios (`LuaNetNetScenariosTest.cpp`): late join, process-level reconnect, dormancy, relevancy, **join-order swap** (Client2 connects first; topology `launch_serial` must be reversed), **packet loss / reordering** (real `NetEmulation.PktLoss/PktOrder/PktLag`; reliable replication must converge on the final value), **`UNetConnection`-level reconnect** (server calls `Connection->Close()`, the same process reopens via `-UnLuaNovaNetReconnectUrl`; the gate requires `launch_count=1` and `first_pid == pid`, which cleanly separates it from process replacement), and **cross-peer schema mismatch** (declaration fingerprints compared across all three peers).
- `Tools/RunNetMatrix.ps1` + `ValidateRelease.py --net-matrix`: Legacy / PushOff / Iris / **Cooked** variants. The first three pass a real engine switch to every peer and read the resulting cvar state back. Cooked points `UNLUANOVA_TEST_PEER_EXE` / `_SERVER_EXE` at staged `Windows` / `WindowsServer` binaries and reads the compile-time `WITH_EDITOR` flag back. `AppendWorldFacts` also exports real `UNetConnection` byte and lost-packet counters, so Iris element-level deltas can be compared between variants from the reports (**no compression win is claimed**).
- Multi-Env declaration / teardown order: `FUnLuaNovaNet` now releases a schema by a **joint owner set**. Identical declarations bound by several environments survive until the last one closes, in any order; closing the first owner used to wipe a schema another Env was still using. Two reverse-order unit cases cover it.
- Object invalidation: a weak `UObject` reference that has gone invalid never writes a stale pointer back to Lua; covered by a unit case.

**All of the above need a rebuild and real DS runs to become evidence - source existing is not a test passing.** The entry point is `Tools/RunTests.ps1 -Profile Development -NetMatrix` (backed by `Tools/RunNetMatrix.ps1`). Every cell launches three real processes, so it is not part of the ordinary test matrix or the normal Release stages.

**There is no remaining source-level TODO.** What remains is execution only: none of the eight scenarios or four variants has been run; `release-status` is now `passed` after a full Release run, but `NET-*` is still `experimental` and
`verification` was not hand-edited; a green regular matrix is not the connected acceptance below. Engine-level seamless travel (`ServerTravel` / `TravelType`) is deliberately out of scope: what is delivered here is a `UNetConnection` close plus an in-process reopen, a different engine path, and the documentation does not conflate the two.

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
4. `ValidateRelease.py --net-matrix` fully green: eight scenario topologies and four variant topologies, each with three real peer reports, non-empty logs, PIDs matching topology, agreeing and discriminating declaration fingerprints, a Generic field / UStruct parameter and non-zero connection byte counters. Variants additionally need Iris / push-model state read back from cvars, and Cooked needs a staged binary path plus the `WITH_EDITOR`-derived flag.
5. The `release-status` marker was updated to `passed` by a genuinely passing full Release
   run. That only covers the regular matrix: item 4 (eight scenarios / four variants) still
   has no execution evidence, so `NET-*` cannot be promoted to `partial`.

Until 1-5 are done, the README must keep the "(experimental)" label and the hard limits. Keeping
`status` at `experimental` is not an omission - it states accurately that source is delivered but
execution has not been accepted.

[Testing](Testing.md) | [Object binding](LuaDrivenObjects.md) | [Capability matrix](../feature-matrix.md)

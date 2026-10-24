---
title: "Lua-declared Networking"
description: "Declaring Server/Client/Multicast RPCs and replicated properties from a Lua module: syntax, how it is implemented, and what still needs a networked run to verify"
---

# Lua-declared Networking

## Why it is needed

The gap is very specific. `ULuaFunction::IsOverridable()`:

```cpp
// Private/LuaFunction.cpp:74
static constexpr uint32 FlagMask   = FUNC_Native | FUNC_Event | FUNC_Net;
static constexpr uint32 FlagResult = FUNC_Native | FUNC_Event;
return Function->HasAnyFunctionFlags(FUNC_BlueprintEvent) || (Function->FunctionFlags & FlagMask) == FlagResult;
```

`FUNC_Net` is excluded from the second branch, and `FunctionDesc.cpp:43` only renames Net
functions that **already exist** to `Xxx_RPC`. Together that means: **Lua can
re-implement RPCs the engine already declared, but it cannot declare one of its own.**

This page is about closing that.

## Declaring RPCs

```lua
local M = UnLuaNova.Class()

M.ServerRPC = {
    -- shorthand: declare it, unreliable by default (same as UFUNCTION(Server))
    Ping = true,

    -- full form
    Chat = {
        Reliable = true,
        Params = { "Int", "Str" },                     -- shorthand, named Arg0, Arg1
        -- or: { Name = "Channel", Type = "Int" }
    },
}

M.ClientRPC    = { ShowDamage = true }
M.MulticastRPC = { Explode    = true }

function M:Chat(Channel, Text) end
function M:Ping() end
return M
```

Parameter type names (case insensitive): `Bool` `Int`/`Int32` `Int64` `Float` `Double`
`String`/`FString` `Name` `Vector` `Rotator` `Transform` `Object`.

**Why strings instead of enums**: the declaration layer then needs no UHT involvement (no
generated.h, no reflection dependency) and does not rely on any enum being registered in
the Lua registry - it cannot break because a name changed, and it works before binding
time. The price is no compile-time type checking; `.d.lua` and runtime warnings cover it.

## Declaring replicated properties

```lua
function M:GetLifetimeReplicatedProps()
    return {
        { Name = "HP",   Type = "Float" },
        { Name = "Name", Type = "String", Condition = "OwnerOnly" },
    }
end
```

Or as a static table `M.ReplicatedProps = { ... }` - when the list is a constant there is
no reason to pay for a function call.

Condition names are the engine's own: `None` `InitialOnly` `OwnerOnly` `SkipOwner`
`SimulatedOnly` `AutonomousOnly` `SimulatedOrPhysics` `InitialOrOwner` `Custom`
`ReplayOrOwner` `ReplayOnly` `SimulatedOnlyNoReplay` `SimulatedOrPhysicsNoReplay`
`SkipReplay`.

### A component has to be attached

Replicated properties **must** be carried by a real `UPROPERTY(Replicated)`: a property
lives in the class's memory layout and, unlike a function, cannot be added to `FuncMap` at
runtime. This is the one thing the override mechanism cannot fabricate.

```lua
function M:Initialize()
    self.Replication = self:AddComponent(UE.UUnLuaNovaReplicationComponent)
end
```

You can also add it in the blueprint directly. At `BeginPlay` the component reads the
declaration: with nothing declared it does not even tick (a component added "just in case"
should cost nothing); with a declaration it allocates slots following the bucketed layout.

### How the server learns a field changed

Two modes, selected by `UpdateMode` on the component:

| Mode | Behaviour | Cost |
|:---|:---|:---|
| `Polled` (default) | Scans the Lua table every `PollInterval` (default 0.1s) | One Lua read per field |
| `Explicit` | Never scans; scripts say so themselves | **Zero cost per frame** |

`Explicit` is the one to use in production. Mark the field when you change it:

```lua
function M:TakeDamage(Amount)
    self.HP = self.HP - Amount
    self.Replication:MarkFieldDirty("HP")
end
```

The component also offers `MarkAllFieldsDirty()` (one full pass, good right after
initialisation) and `FlushNetUpdate()` (send this frame instead of waiting for
`NetUpdateFrequency`).

Dormant actors are not scanned (`DORM_DormantAll` is skipped outright);
`MarkFieldDirty` calls `FlushNetDormancy()` first, otherwise the change just sits there
with nobody sending it.

### How the client learns which field changed

Before writing back into the Lua table the component reads the old value and compares, and
dispatches only for fields that really changed:

```lua
function M:OnRep_HP()          -- per field
    self:UpdateHealthBar()
end

function M:OnReplicatedProps(Names)   -- fallback: { "HP", "Mana" }
end
```

That change set is **computed on the client, so it costs no bandwidth**.

## How it is implemented

**RPCs do not build a second pipeline** - `FLuaOverrides::Override()` already has an
"add a new function" path:

```cpp
// LuaOverrides.cpp:41
const auto bAddNew = Function->GetOuter() != Class;
```

Anything whose source UFunction has a different outer than the target class takes that
path, and `ULuaFunction::SetActive()` then calls `AddFunctionToFunctionMap()` +
`AddNativeFunction()` with `execCallLua` as the native func.

So "declaring an RPC" = **build a UFunction carrying `FUNC_Net` and typed parameters**, then
feed it to `ULuaFunction::Override()`. Two things come for free:

* **Automatic restore on PIE exit** - the function hangs off `ULuaOverridesClass`, and
  `RestoreOverrides()` removes it;
* **No `FUNC_BlueprintEvent`** - an RPC is not an event meant to be overridden, and
  `IsOverridable()` already refuses functions carrying `FUNC_Net`; adding it would be wrong.

For comparison, slua adds functions to the UClass at runtime with `LuaNet::addClassRPC`
plus a cleanup set (`onObjectDeleted` / `addedRPCClasses`). All of that already exists here.

**Replicated properties** use "bucket by type": one replicated array per type, fields land
in the bucket in declaration order, and the address is `(bucket, index)`. Both ends compute
the layout from the same declaration, so nothing on the wire carries a field name or a type
tag.

```cpp
// UUnLuaNovaReplicationComponent - eleven of them, one per type
UPROPERTY(ReplicatedUsing = OnRep_Floats) TArray<float> Floats;   // [HP, Mana]
UPROPERTY(ReplicatedUsing = OnRep_Strings) TArray<FString> Strings; // [NickName]
```

A `float` is now 4 bytes instead of the "FName + int32 + string" package. Costs and gains
are in the next section.

## Network optimisation: push model and Iris

Bucketing is not about "saving a few bytes". It is what makes the next two things
**possible**.

### Push model

Every bucket registers with `bIsPushBased` and is marked dirty individually:

```cpp
FDoRepLifetimeParams Params;
Params.bIsPushBased = true;                                   // under WITH_PUSH_MODEL
DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Floats, Params);
...
MARK_PROPERTY_DIRTY_FROM_NAME(UUnLuaNovaReplicationComponent, Floats, this);   // only this bucket becomes dirty
```

The replication system then stops polling these properties: changing `HP` makes the engine
look at `Floats` and not glance at `Strings` at all. The change path **deliberately has no**
`MarkNetDirty()` / `ForceNetUpdate()` - that is exactly what push model removes; to send
immediately, scripts call `FlushNetUpdate()` explicitly.

Both degradation paths are safe:

* `WITH_PUSH_MODEL=0` (project disables it): the macro does not exist, buckets become
  ordinary properties compared by the replication system itself, behaviour unchanged.
* Compiled in but `net.IsPushModelEnabled 0` at runtime: `MARK_PROPERTY_DIRTY` is a no-op,
  and it falls back to polling the same way.

The component logs one line at startup saying which path it took, so you do not have to
guess.

It is only complete together with `UpdateMode = Explicit`: **no scanning and no polling**,
so one field change costs one read, one comparison and one dirty mark from Lua to the wire.

### Iris

Compatibility is **structural** - it does not call any Iris API. Three things are
deliberately absent from the whole replication path:

* no `NetDeltaSerialize` / `WithNetDeltaSerializer`
* no `FFastArraySerializer`
* no custom `NetSerializer`

What is left is the most ordinary typed `UPROPERTY` there is. Both legacy and Iris accept
that, so switching to Iris needs no code change. Iris also gives two things for free:

* **Per-element delta** - `TArray` is dynamic state in Iris, so changing `Floats[0]` sends
  only `[0]`; under legacy the whole array goes (the one remaining waste on legacy, see
  known boundary 3).
* **Per-type quantisation** - float / FVector / FRotator get their own compressing
  NetSerializer, which a string payload can never get.

The flip side: the "`FLuaNetSerialization` + hand-written `NetDeltaSerialize`" design
considered in earlier versions was dropped, for two reasons - legacy delta serialisation
means maintaining your own baseline, and a lost packet makes state diverge (you need a
resync protocol to recover), while `FNetDeltaSerializeInfo` changes between engine
versions; more importantly, whether it works under Iris at all was an unknown. Full buckets
are self-healing: every update carries complete state, so one lost packet never shifts
anything.

### SchemaVersion

Bucket indices come from declaration order, so both ends must agree. The component carries
a 32-bit fingerprint (`ComputeSchemaVersion`, covering names, order and types); a client
that receives a mismatching one logs an error and refuses to apply it - when a hot reload
changes the declaration list you get one explicit Error instead of `HP` being written into
`Mana`.

## Known boundaries

1. **Cross-process `RPCId` agreement: covered by tests.** `UnLuaNova.API.Net.DedicatedServer`
   (`Plugins/UnLuaNovaTestSuite/.../Private/Tests/LuaNetDedicatedServerTest.cpp`) starts
   **two separate processes** (the server with `-server`), has each declare the same Lua
   networking set, writes the `name:RPCId` pairs it sees into
   `Saved/UnLuaNovaNet/<Role>.json`, and the main process reads both and compares.

   Why two processes are required: RPCs travel by id, not by name, and in C/S PIE both ends
   share one UClass - so the ids are trivially identical and such a test proves nothing.
   `EnsureRpcId()` allocates as "use the engine's id if it assigned one; otherwise take the
   largest existing id on the class + 1", so both ends only land on the same id by running
   the same module and adding in the same order - which is precisely what needs verifying.

   Note that it does **not** need the two ends to actually connect: the id depends only on
   the module and the declaration order, not on connectivity. That avoids the port /
   firewall / "is the server even listening" class of failures that makes CI brittle; the
   extra value of `-server` is that declaration happens under `NM_DedicatedServer`, a path
   PIE never takes.

2. **Replication conditions are per type, not per field.** Fields of the same type share one
   condition (on conflict the first non-`None` wins and a warning is logged). That is one
   step finer than "one condition for the whole payload", but per-field is still out of
   reach: it would need one UPROPERTY per condition, which cannot be added at runtime. If
   you genuinely need per-field, the way out is build-time code generation, or Iris
   condition groups.

3. **Under legacy, changing one field resends the whole bucket.** Changing `HP` sends every
   `Float` on that actor. Iris has no such problem (per-element delta). When many fields
   share a type and only one of them changes frequently, Iris is the answer - or declare
   that one field as a real UPROPERTY on the C++ side.

4. **`Object` is handled as `UObject*`**, with no subclass distinction. `Vector` /
   `Rotator` / `Transform` require the Lua side to hold real userdata (the kind
   `UE.FVector(...)` produces); anything else is skipped with a log line rather than being
   reinterpreted as a vector.

5. **In `Explicit` mode a missed mark means it is never sent.** If you change a field and do
   not call `MarkFieldDirty`, the server never learns. That is the cost of the mode, not a
   bug - if you are unsure, start with `Polled`.

6. **RPCs re-register after a hot reload.** If the class already has a function of that
   name, the declaration is skipped with a warning - it will not overwrite an RPC declared
   by the engine or by a blueprint. Replicated fields instead carry a new `SchemaVersion`,
   and the other end reports a mismatch.

## Verification checklist

* `UnLuaNova.API.Net` - parsing, marking, parameters, restore (no networking needed)
* `UnLuaNova.API.Net.ClientServer` - automatically opens a PIE session with one listen
  server + 1 client and asserts that a Server RPC runs on the server and not on the client
  that raised it, that a Client RPC runs only on clients, that Multicast runs on both ends,
  that parameters cross unchanged, and that Lua-declared replicated fields reach the
  client. The session also carries **an engine-replicated `int32` as a control**: if that
  one does not arrive the session itself is broken and none of the Lua assertions below are
  worth reading
* `UnLuaNova.API.Net.DedicatedServer` - two independent processes each declaring, comparing
  `RPCId` and replicated fields. The sub-case `...DedicatedServer.Report` is the half that
  runs inside the peer process, distinguished by `-UnLuaNovaNetRole`; running it alone is
  meaningless
* Not covered yet: a `COND_OwnerOnly` field keeping its default on a non-owning client
  (needs a scenario that can tell owning clients apart); Multicast with more than one
  client; a push model vs Iris comparison (see below). Cross-process `RPCId` agreement
  **is** covered by `...DedicatedServer`

### Push model and Iris items still to verify

The engine source is not on this machine, so the following **must** be walked through where
an engine is available, in this order of priority:

1. `MARK_PROPERTY_DIRTY` / `FDoRepLifetimeParams::bIsPushBased` / `WITH_PUSH_MODEL`
   compile as named today (stable 5.x API, but confirm this source has not changed it).
   If it does not compile, `UNLUANOVA_PUSH_MODEL` becomes 0 and everything still works,
   just degraded to polling - the startup log says so.
2. In C/S PIE with `net.IsPushModelEnabled 1`: the change **reaches** the client (proving
   the dirty mark works); then flip it to `0`: the change **still reaches** the client
   (proving the degradation path is written correctly).
3. `Explicit` mode: mark only `MarkFieldDirty("HP")` and the client sees only `HP` change,
   with `OnRep_HP` called once and `OnRep_Mana` never called (proving per-field dispatch).
4. Iris: enable it the way this source version enables it (in 5.x a cvar like
   `Net.Iris.Enabled` - go by the source) and run the same assertions, comparing results
   literally. Watch two things in particular: whether `TArray` takes the per-element delta
   path, and whether `MARK_PROPERTY_DIRTY` is bridged under Iris (if not, buckets fall back
   to full comparison, which is still correct).
5. Dormancy: put an actor into `DORM_DormantAll`, change a field and call
   `MarkFieldDirty`, then confirm it wakes up and the data arrives.

---

## Further reading

- [C++ API Reference](CxxApi.md) - `ULuaFunction`, `FLuaEnv`
- [Architecture](Architecture.md) - the override path
- [Comparison with slua-unreal](Comparison_sluaunreal.md) - how this gap was located

---
title: Declared Networking Production Guide
description: Full example, policy switches, RPC authorization, hot-reload schema migration, seamless travel, error codes and the acceptance checklist.
---

# Declared Networking Production Guide

> **Status**: this page describes the target contract for a supported release and the
> policy layer added in this round. The capability is still pending promotion: the latest
> Release is `passed=false` and the connected matrix has not completed. Satisfy L2–L4 of
> [Release Readiness](Release_Readiness.md) before production use. For syntax see
> [Lua-declared Networking](LuaNetworking.md).

## 1. Complete example

```lua
-- Content/Script/MyNetActor.lua
local M = UnLuaNova.Class()

M.ServerRPC = {
    RequestDamage = { Reliable = true, Params = { "Int", "String" } },
}
M.ClientRPC    = { ShowDamage = true }
M.MulticastRPC = { Explode = true }

function M:GetLifetimeReplicatedProps()
    return {
        { Name = "HP",     Type = "Float" },
        { Name = "Secret", Type = "String", Condition = "OwnerOnly" },
    }
end

function M:Initialize()
    self.Replication = self:AddComponent(UE.UUnLuaNovaReplicationComponent)
end

function M:RequestDamage(Amount, Source) end
function M:OnRep_HP() self:UpdateHealthBar() end

return M
```

The project's `Config/DefaultUnLuaNova.ini` must pre-bind the network class, otherwise
the NetDriver may cache the class before its RPCs exist:

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+PreBindClasses=/Script/MyGame.MyNetActor
bDeclaredNetworking=True
```

Startup now self-checks these paths: unresolvable entries produce a `LogUnLuaNova`
warning instead of a missing RPC discovered only during a session.

## 2. Four policy switches

| Key | Default | Purpose |
|---|---|---|
| `bDeclaredNetworking` | `True` | Master switch; off skips `DeclareRPCs` / `DeclareReplicatedProps` entirely, so no dynamic `UFunction` is built |
| `bRequireRpcAuthorization` | `False` | Requires an application-level handler; enabled-but-not-installed is a config error and the policy **fails closed** |
| `bAllowHotReloadSchemaChange` | `False` | Allows a reload to change the replicated layout, staged as a pending migration |
| `bPreserveSchemaAcrossSeamlessTravel` | `True` | Pins schemas and advances the generation across travel so old-world RPC IDs cannot be reused |

Query from script: `UnLuaNova.HasFeature('declarednetworking')`.

## 3. RPC authorization

The engine only blocks a non-owner Server RPC. The host installs the application policy;
`CanInvoke` is wired into the real RPC dispatch path `FFunctionRegistry::Invoke` (kind
derived from `FUNC_Net*` flags), so every declared RPC passes through it before reaching
its Lua handler:

```cpp
#include "UnLuaNovaNetPolicy.h"

UnLuaNova::FUnLuaNovaNetPolicy::Get().SetAuthorizationHandler(
    [](const UObject* Target, const FName& RpcName, UnLuaNova::ELuaRpcKind Kind)
        -> UnLuaNova::ELuaNetPolicyResult
    {
        // The engine's owner check remains independent and still applies.
        return MyProject::IsRpcAllowed(Target, RpcName, Kind)
            ? UnLuaNova::ELuaNetPolicyResult::Ok
            : UnLuaNova::ELuaNetPolicyResult::InvocationDenied;
    });
```

Denials can be captured with `SetDeniedLogger`; by default they go to `LogUnLuaNova`
with the result code, reason, object, RPC name, local/remote schemas and generation.

## 4. Hot-reload schema changes

1. Set `bAllowHotReloadSchemaChange=True`.
2. After `FLuaEnv::HotReload()` completes, `FUnLuaNovaNetPolicy::NotifyModulesReloaded()`
   advances the generation and discards unapproved staged migrations.
3. If the next declaration changes the layout, `OnHotReload(Class, NewSchema, Reason)`
   stages it as a pending migration.
4. Every peer must accept that migration through `NegotiateSchema` before any bucket is
   applied; otherwise `OnRep_SchemaVersion` keeps rejecting with an Error.
5. With the switch off, a layout change is refused explicitly with "rebuild the network
   environment", instead of silently mis-decoding.

> This limitation is fixed: the one-shot `BeginPlay` freeze has been replaced by a
> re-entrant `RefreshDeclaration()`, and hot reload / travel completion automatically
> call `RefreshAllDeclarations()` to rebuild slots, SchemaVersion and the RPC layout
> fingerprint. **The change has not been compiled or run yet** - see the defect table in
> [Release Readiness](Release_Readiness.md).

## 5. Seamless travel

Call the policy layer once before and once after the travel in the host's travel callback:

```cpp
UnLuaNova::FUnLuaNovaNetPolicy::Get().OnSeamlessTravel(GetWorld());
// ... travel completes ...
UnLuaNova::FUnLuaNovaNetPolicy::Get().CompleteSeamlessTravel();
```

It advances the generation, pins still-valid class schemas, prunes dead weak references
and refuses new declarations during the travel (`TravelInProgress`), so RPC IDs and field
indices from the old world cannot be reused. `CompleteSeamlessTravel()` also calls
`RefreshAllDeclarations()` automatically to re-parse declarations for the new world.
This is an **explicit entry point**, not an auto-registered engine delegate: an unverified
delegate signature must not enter a release branch. For automatic hooking, call these two
lines from a `UGameInstance` world-change callback after compile verification.

## 6. Error codes

| `ELuaNetPolicyResult` | Meaning | Action |
|---|---|---|
| `Ok` | Passed | — |
| `FeatureDisabled` | Master switch off | Check `bDeclaredNetworking` |
| `DeclarationDenied` | Null class or refused declaration | Check class path and pre-binding |
| `InvocationDenied` | Application policy refused | Check the host handler |
| `SchemaIncompatible` | Layout mismatch with no approved migration | Align scripts, or use the hot-reload flow |
| `TravelInProgress` | Declaration refused during travel | Wait for `CompleteSeamlessTravel()` |
| `NotOwner` | Non-owner invocation | Engine already blocks; exposed for hosts |
| `Unauthorized` | Authorization required, no handler | Install a handler or disable the switch |

## 7. Acceptance checklist

- All L2–L4 items in `Tools/CheckPromotion.py` are green;
- Per backend, 8 Legacy scenarios + 4 variants (independent groups, 12 cells) produce
  topologies and reports;
- **Real fault injection implemented this round**: the scenario injects a different
  fingerprint through the real negotiation path, and the gate requires
  `InjectedSchemaVersion` / `SchemaMismatchRejected` / `SchemaMismatchReason`; pending run verification;
- **Explicit RPC layout rejection implemented this round**: a replicated
  `RpcSchemaVersion` plus `OnRep_RpcSchemaVersion` / `PushSlots` guards; pending compile/run verification;
- Non-owner Server RPC rejection, real NetEmulation convergence for PacketLoss, and a
  staged exe for Cooked.

## Further reading

[Lua-declared Networking](LuaNetworking.md) · [API Stability](API_Stability.md) · [Release Readiness](Release_Readiness.md) · [Production Deployment](Production_Deployment.md) · [Test Architecture](Testing_Architecture.md)

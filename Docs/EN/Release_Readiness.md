---
title: Release Readiness and Definition of Done
description: The Definition of Done for promoting experimental capabilities to a supported release: status semantics, the five-level evidence chain, capability-specific checklists and prohibited shortcuts.
---

# Release Readiness and Definition of Done

This page is the **single criterion** for "a capability is ready for a supported release".
The 40 `acceptance` fields in `Tools/feature-matrix.json` describe a source audit and are
scattered across the matrix; none of them answers "can we ship this now?" at a glance.
This page consolidates them into one executable checklist.

## Status semantics (read this first)

| Carrier | Meaning | Written by |
|---|---|---|
| `feature-matrix.json` `status` | **Source delivery state** (implemented / partial / experimental / planned) | Manual source audit |
| `feature-matrix.json` `verification` | **Always `pending-runtime`**, enforced by `AuditRepository.py:80` | Gate; must not be edited |
| `Tools/evidence/index.json` | Real execution evidence, including its commit | `Tools/RecordEvidence.py` |
| `Saved/ReleaseTests/*/release-summary.json` | Verdict of one complete Release | `Tools/ReleasePipeline.ps1` |
| `release-status` / `release-provenance` on the eight pages | Public release claim | `Tools/UpdateReleaseStatus.py` |

> **Changing `status` is not promotion.** The matrix is deliberately designed never to
> claim execution ("execution claims require an evidence schema, not a status edit").
> Hand-editing `experimental` into `implemented` makes both the audit and reality wrong.
> `Tools/CheckPromotion.py` is a read-only judge; it never writes a file.

## Five-level evidence chain

| Level | Name | Required | Evidence |
|---|---|---|---|
| L0 | Source delivery | Implementation, error handling, diagnostics, dedicated tests | Sources + `feature-matrix.json` |
| L1 | Unit / static checks | Python unit tests, repository audit, type checks, script syntax | Local command output |
| L2 | Complete Release | `complete=true`, `passed=true`, `provenance.commit == HEAD`, no `failed`/`blocked` stage | `Saved/ReleaseTests/test/release-summary.json` |
| L3 | Capability-specific matrix | Every dedicated scenario passes (below) | Per-scenario topology / reports |
| L4 | Release claim | Marker `passed`, CHANGELOG and version tag | Eight pages + git tag |

L0–L1 can be done statically; **L2–L3 require a real Unreal build**. This repository offers
no way to bypass them.

## Promotion checklist for experimental capabilities

### Lua-declared networking (NET-RPC / NET-PROPS / NET-TYPES, all P0)

| # | Condition | Evidence / command | Now |
|---|---|---|---|
| 1 | Per backend, 4 variants (Legacy / PushOff / Iris / Cooked) with three-peer topologies | The `net-matrix-Lua` / `net-matrix-Luau` stages of `RunTests.ps1` + `Saved/ReleaseTests/test/net-matrix/<Backend>/net/variants/*/DedicatedServer/topology.json` | Unmet: Luau has only Cooked |
| 2 | Per backend, all eight scenarios (LateJoin / Reconnect / Dormancy / Relevancy / JoinOrderSwap / PacketLoss / SeamlessReconnect / SchemaMismatch) produce topology and reports | Same, `net/scenarios/*/topology.json` | Unmet: five Luau scenarios missing |

> **Matrix shape (do not estimate as a cross product)**: `Tools/RunNetMatrix.ps1`
> runs **two independent groups** - the eight scenarios are driven in a single Legacy
> cell (`:202-204`), and the four variants each run their own DedicatedServer
> (`:155-197`). That is **8 + 4 = 12 cells per backend**, 24 for both, **not 8 x 4 = 32
> cross-product cells**. The scenarios have never run under Iris / PushOff / Cooked;
> extending them there is new work and should be scoped separately.
> `Docs/EN/Testing.md` now states the two independent groups explicitly. Both are
> executed automatically by `RunTests.ps1`; no separate `RunNetMatrix.ps1` command is needed.
| 3 | `ValidateRelease.py --net-matrix` reports zero errors | Gate output | Unmet |
| 4 | A non-owner Server RPC is rejected (unique token + exact CallLog) | DS scenario assertion | Assertion exists in source, unexecuted |
| 5 | `RepSchemaVersion` matches on all three peers; `VariantRepSchemaVersion` discriminates | Declaration report | Export exists in source, unexecuted |
| 6 | Cooked variant uses real staged WindowsServer / Windows exes | topology `exe` field | Luau/Cooked topology exists, not judged |
| 7 | PacketLoss uses real NetEmulation and converges on the final value | Scenario report | Unmet |
| 8 | Three newly supported capabilities: Seamless Travel, hot-reload schema changes, RPC authorization | New scenarios + code + reports | In development, unverified |
| 9 | A master switch can disable declared networking entirely | `HasFeature('declarednetworking')` + config | In development |
| 10 | All `EXPERIMENTAL` markers, titles and "not for production" wording removed | Sources / docs | Only after L1–L3 pass |

### Other status corrections (not features to build)

| Capability | Situation | Action |
|---|---|---|
| `LUAU-CODEGEN` | `Luau.md:22-32` already defines it as "delivered, bounded, optional optimisation", yet the matrix says `experimental` | Correct status to `implemented`; arm64 / Shipping / native-debug attach remain separate acceptance items |
| `LUAU-EXPERIMENTAL` | Upstream Luau integer / user-class feature flags, not enabled by `Luau.Build.cs`; `Luau.md:36` states they are "not a capability delivered by this plugin" | Correct status to `planned` (not shipped); **it should not be promoted at all** |

## Source-level defects: fixed this round (compile/run verification still pending)

Several `gap` fields in `feature-matrix.json` claimed "delivered in source, execution
pending". Source review confirmed the following five were **not** merely awaiting a test
run. All five were changed in this round; none has been compiled or executed, so the
unified verification decides whether the fixes are effective.

| # | Defect | Fix | Location |
|---|---|---|---|
| 1 | Generic payload read side only compared the top-level tag instead of recursively validating the type spec | `DecodeLuaNetValue` now reuses `CheckShape` after decoding (explicit nil excepted) and rejects violations | `UnLuaNovaNetTypeSpec.cpp` |
| 2 | `BeginPlay` froze the schema once, so late-bound components stayed inert (root cause of the hot-reload / travel exclusions) | Extracted a re-entrant `RefreshDeclaration()`; added `RefreshAllDeclarations()`, generation detection and automatic rebuild on reload / travel completion | `UnLuaNovaReplicationComponent.h/.cpp`, `UnLuaNovaNetPolicy.cpp` |
| 3 | No runtime RPC layout detection/negotiation | Added `FUnLuaNovaNet::GetRpcLayoutVersion()` and a replicated `RpcSchemaVersion`; a mismatch is rejected in `OnRep_RpcSchemaVersion` and `PushSlots` | `UnLuaNovaNet.h/.cpp`, `UnLuaNovaReplicationComponent.*`, `UnLuaNovaNetPolicy.*` |
| 4 | No application-level authorization | `CanInvoke` inserted into the real RPC dispatch path `FFunctionRegistry::Invoke`, with kind derived from `FUNC_Net*` flags | `Registries/FunctionRegistry.cpp` |
| 5 | `SchemaMismatch` only proved "everyone looks the same" | The scenario now injects a deliberately different fingerprint through the real negotiation path and exports `InjectedSchemaVersion` / `SchemaMismatchRejected` / `SchemaMismatchReason`; the gate requires all three; policy unit tests added | `LuaNetNetScenariosTest.cpp`, `ValidateRelease.py`, `LuaNet.spec.cpp` |

**Still to confirm in the unified verification**: the C++ compiles; the fault-injection
case produces reports in a real multi-process session; an RPC layout mismatch is actually
rejected; `RefreshDeclaration()` behaves correctly on cooked / Shipping carriers.

## Prohibited shortcuts

- Never hand-edit `feature-matrix.json`'s `verification`.
- Never mark NET-* as `implemented`, or remove experimental markers, before the
  `net-matrix-Lua` / `net-matrix-Luau` stages of `RunTests.ps1` are green.
- Never refresh a release claim from a failed run, or one whose `provenance.commit` is not HEAD.
- Never treat "an assertion exists in source" as "the assertion ran".

## Current state (2026-11-13)

`Tools/CheckPromotion.py --release-gate --strict` now runs automatically as the
`promotion-gate` stage of `RunTests.ps1` (the L4 published marker and release tag are
human decisions, reported but not gating). Known today: the latest Release is
`passed=false` (`Luau-package-development` hit the 60-minute timeout on a cold UE 6.0
cook), the eight markers read `not-passed`, the Luau net-matrix Editor process exited
with 255, and the five source-level defects are fixed but **not yet compiled or
executed**. **No experimental capability currently satisfies the promotion criteria.**

## Further reading

[Engine and Platform Support Matrix](Engine_Support.md) · [Dual-backend Capability Matrix](Backend_Capability_Matrix.md) · [Test Architecture and Acceptance](Testing_Architecture.md) · [Testing and Automation](Testing.md)

---
title: API Stability and Versioning Policy
description: Stability tiers, deprecation cycle, semantic versioning rules and the promotion criteria for experimental APIs in UnLuaNova.
---

# API Stability and Versioning Policy

About 85 headers under `Source/UnLuaNova/Public/` are externally visible, but **being in
`Public/` does not mean being a stable contract**. This page is the single classification
rule; every new public API must be registered here.

## Four stability tiers

| Tier | Meaning | Change rule | Marker |
|---|---|---|---|
| **Stable** | Supported contract | Breaking change requires a major version plus at least one deprecation cycle | `API: Stable since <version>` |
| **Bounded** | Delivered with explicit boundaries (backend / platform / config) | Stable within the boundary; boundary changes go in CHANGELOG | `API: Bounded - <boundary>` |
| **Experimental** | Interface and behaviour may change | May change freely; no compatibility promise | Header banner + doc title |
| **Internal** | Implementation detail exposed in Public, not promised | May be removed freely | `API: Internal - do not include directly` |

## Current classification

| Area | Tier | Notes |
|---|---|---|
| `LuaEnv.h` / `Binding.h` / `LuaCore.h` / `LuaModuleLocator.h` / `LuaEnvLocator.h` | Stable | Environment and binding main path |
| `Registries/*` / `ReflectionUtils/*` / `UnLuaNovaPrivate.h` / `UnLuaNovaCompatibility.h` / `UnLuaNovaLegacy.h` | Internal | Should not be referenced as public contracts |
| `LuaBackend.h` and the 12 `UNLUANOVA_HAS_*` capability macros | Bounded | Backend-dependent; see [Capability Matrix](Backend_Capability_Matrix.md) |
| `LuaCoverage.h` / `UnLuaNovaSandbox.h` / `LuaVmHooks.h` | Bounded | Luau-only; plain-Lua stubs are documented degradations, not missing work |
| `UnLuaNovaNetPolicy.h` | **Stable** | Policy and error-code contract for declared networking (new) |
| `UnLuaNovaNet*.h` / `UnLuaNovaReplicationComponent.h` / `UnLuaNovaNetBuckets.h` | **Stable** | Promoted 2026-11-15: complete Release `passed=true` (41/41 stages) and zero errors across 4 variants + 8 scenarios per backend |

> `Luau.md:36` says "this plugin provides no experimental capabilities of its own".
> That refers to **upstream Luau's integer / user-class feature flags**
> (`LUAU-EXPERIMENTAL`, marked not-shipped). This plugin's declared networking was
> promoted to Stable on 2026-11-15; the two statements are not in conflict.

## Deprecation policy

1. Keep the old signature for at least one minor release after a new API lands.
2. A deprecation must add `UE_DEPRECATED(<version>, "<replacement>")`, document the
   migration in CHANGELOG, and move the entry into the deprecation list here.
3. Removal happens only in the next major version.
4. A plain `// Deprecated` comment is not a deprecation marker. `UnLuaNovaBase.h:50/:57`
   are exactly that case and need fixing; the only formal `UE_DEPRECATED` is at
   `UnLuaNovaBase.h:160`.

## Promotion from Experimental to Stable (achieved 2026-11-15)

This could not be done by editing documentation. The required conditions and their
final evidence:

- [Release Readiness](Release_Readiness.md) L2 (complete Release passed, commit equals HEAD)
  and L3 (net matrix zero errors across two independent groups: 8 Legacy scenarios + 4
  variants per backend = 12 cells per backend, **not an 8 x 4 cross product**) ->
  one complete Release on the `4a87d214` verification baseline: `complete=true`,
  `passed=true`, `41/41 stages`, `provenance.commit=4a87d214 == HEAD`; both `net-matrix-Lua`
  and `net-matrix-Luau` passed. Whether the current tree has execution evidence is
  determined by `Saved/ReleaseTests/test/release-summary.json`; the sole acceptance entry
  point is `Tools/RunTests.ps1`.
- the `promotion-gate` stage of `RunTests.ps1` (`CheckPromotion.py --release-gate --strict`)
  is green -> 7/7 gate items, 0 unmet; the release tag is an informational L4 decision.
- `Tools/UpdateReleaseStatus.py` has generated `release-status: passed` on all eight pages.
- all `EXPERIMENTAL` banners, doc titles and "not for production" wording are removed.
- `NET-RPC` / `NET-PROPS` / `NET-TYPES` in `Tools/feature-matrix.json` are `implemented`
  (`verification` stays `pending-runtime`).

**Current state: Stable.** Declared networking is verified on Legacy / PushOff / Iris /
Cooked carriers and all eight connected scenarios, on both the Lua and Luau backends.

## Versions and tags

- Semantic versioning; `.uplugin` `VersionName` must match the release tag (all five now
  carry the official `1.0.0` release version).
- `v1.0.0` has been released and tagged as UnLuaNova's own first official tag; the other
  21 tags inherited from upstream Tencent/UnLua are **not this project's releases**.
- The first official tag `v1.0.0` has been created on release commit `31e2af3f`; the
  release verdict refers to the `4a87d214` baseline.

## Further reading

[Release Readiness](Release_Readiness.md) · [Engine and Platform Support Matrix](Engine_Support.md) · [Dual-backend Capability Matrix](Backend_Capability_Matrix.md) · [Migration Guide](Migration_Guide.md)

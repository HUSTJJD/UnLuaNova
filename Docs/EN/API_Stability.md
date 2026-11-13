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
| **Experimental** | Interface and behaviour may change | May change freely; not for production | Header banner + doc title |
| **Internal** | Implementation detail exposed in Public, not promised | May be removed freely | `API: Internal - do not include directly` |

## Current classification

| Area | Tier | Notes |
|---|---|---|
| `LuaEnv.h` / `Binding.h` / `LuaCore.h` / `LuaModuleLocator.h` / `LuaEnvLocator.h` | Stable | Environment and binding main path |
| `Registries/*` / `ReflectionUtils/*` / `UnLuaNovaPrivate.h` / `UnLuaNovaCompatibility.h` / `UnLuaNovaLegacy.h` | Internal | Should not be referenced as public contracts |
| `LuaBackend.h` and the 12 `UNLUANOVA_HAS_*` capability macros | Bounded | Backend-dependent; see [Capability Matrix](Backend_Capability_Matrix.md) |
| `LuaCoverage.h` / `UnLuaNovaSandbox.h` / `LuaVmHooks.h` | Bounded | Luau-only; plain-Lua stubs are documented degradations, not missing work |
| `UnLuaNovaNetPolicy.h` | **Stable** | Policy and error-code contract for declared networking (new) |
| `UnLuaNovaNet*.h` / `UnLuaNovaReplicationComponent.h` / `UnLuaNovaNetBuckets.h` | **Experimental, pending promotion** | Criteria in [Release Readiness](Release_Readiness.md) section 8; must not be relabelled Stable before L2–L3 pass |

> `Luau.md:36` says "this plugin provides no experimental capabilities of its own".
> That refers to **upstream Luau's integer / user-class feature flags**
> (`LUAU-EXPERIMENTAL`, now marked not-shipped). It does not cover this plugin's
> declared-networking experimental status.

## Deprecation policy

1. Keep the old signature for at least one minor release after a new API lands.
2. A deprecation must add `UE_DEPRECATED(<version>, "<replacement>")`, document the
   migration in CHANGELOG, and move the entry into the deprecation list here.
3. Removal happens only in the next major version.
4. A plain `// Deprecated` comment is not a deprecation marker. `UnLuaNovaBase.h:50/:57`
   are exactly that case and need fixing; the only formal `UE_DEPRECATED` is at
   `UnLuaNovaBase.h:160`.

## Promotion from Experimental to Stable

This cannot be done by editing documentation. All of the following must hold:

- [Release Readiness](Release_Readiness.md) L2 (complete Release passed, commit equals HEAD)
  and L3 (net matrix zero errors across two independent groups: 8 Legacy scenarios + 4
  variants per backend = 12 cells per backend, **not an 8 x 4 cross product**);
- every L2–L4 item in `Tools/CheckPromotion.py` is OK;
- `Tools/UpdateReleaseStatus.py` has generated `release-status: passed` on all eight pages;
- all `EXPERIMENTAL` banners, doc titles and "not for production" wording are removed;
- `NET-RPC` / `NET-PROPS` / `NET-TYPES` in `Tools/feature-matrix.json` move from
  `experimental` to `implemented` (`verification` stays `pending-runtime`).

**Current state: not satisfied.** The latest Release is `passed=false` and five Luau
net-matrix scenarios are missing.

## Versions and tags

- Semantic versioning; `.uplugin` `VersionName` must match the release tag (all five are
  placeholder `1.0.0` today).
- The 22 existing tags (`v1.0.0` … `v2.3.6`) are inherited from upstream Tencent/UnLua
  and are **not UnLuaNova releases**.
- The first supported release must create an UnLuaNova tag and record the fork point in
  CHANGELOG.

## Further reading

[Release Readiness](Release_Readiness.md) · [Engine and Platform Support Matrix](Engine_Support.md) · [Dual-backend Capability Matrix](Backend_Capability_Matrix.md) · [Migration Guide](Migration_Guide.md)

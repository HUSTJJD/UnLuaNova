---
title: Engine and Platform Support Matrix
description: Verified, adapted and unvalidated combinations of engine version, Lua backend and platform, plus the acceptance requirements for adding a new one.
---

# Engine and Platform Support Matrix

This page is the **single authoritative statement of support scope**. README badges
and the `.uplugin` `PlatformAllowList` describe build targets and declarations, not
acceptance status. Where they disagree, this page wins.

## Status definitions

| Status | Meaning |
|---|---|
| Verified | A complete pass was produced by this repository's Release gate (both backend builds + Editor/cooked automation + connected peers + type checks + packaging + performance) |
| Adapted | The sources contain version-specific conditional branches, but there is no complete passing Release record for that version |
| Unvalidated | No automated acceptance record, no commitment that it works (GitHub Actions builds the docs site only; it does not validate any engine/platform combination) |

## Matrix

| Engine | Adaptation | Lua 5.5.1 | Luau 0.743-1 | Platform | Status |
|---|---|---|---|---|---|
| UE 5.8.3 | `UE_5_8_OR_LATER` / `UE_VERSION_OLDER_THAN(5,8,0)` | ✓ | ✓ | Win64 | Verified (2026-11-03 baseline, `347bb8c5`) |
| UE 6.0 | `!UE_VERSION_OLDER_THAN(6,0,0)` plus replacements for APIs removed in 6.0 | ✓ | ✓ | Win64 | Adapted, unverified: the latest Release timed out after 60 minutes in the `Luau-package-development` cold cook |
| UE 5.1 – 5.7 | `UE_VERSION_NEWER_THAN` / `OLDER_THAN` branches | — | — | Win64 | Adapted, unverified |
| UE 4.x | Compatibility layer inherited in `UnLuaNovaCompatibility.h` | — | — | — | Not supported; fork legacy only |
| Mac / Linux / iOS / Android / consoles | Declared as build targets by `.uplugin` `PlatformAllowList` | — | — | non-Win64 | Unvalidated |

> `PlatformAllowList` means "permitted as a compile target", NOT "accepted by the
> test gate". The build and acceptance toolchain (`Tools/*.ps1`, the six
> backend-isolated targets, UBT engine discovery) has only ever run on Win64.

## Three version-detection schemes

| Scheme | Location | Purpose |
|---|---|---|
| `UE_5_x_OR_LATER` / `UE_6_x_OR_LATER` | `*.Build.cs`, UBT plugin C# | Compile-time module rules and warning switches |
| `UE_VERSION_OLDER_THAN` / `NEWER_THAN` / `NEWER_THAN_OR_EQUAL` | C++ sources | Engine API divergence branches |
| Hand-written `ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` comparisons | A few C++ files | Numeric comparisons |

Check all three when adding an engine version. Their coexistence is historical and is
not being forcibly unified, but **every new branch must also update this page**.

## Build environment notes

- The demo project's `UnLuaNovaDemo.uproject` uses a GUID `EngineAssociation` bound to
  the maintainer's local source-engine install. On a fresh clone you must re-associate
  the engine or set `UNREAL_ENGINE_PATH`. This is a property of the demo, not of the
  plugin: integrating into your own project means using your own `.uproject`.
- Several plugins explicitly enabled in the demo `.uproject`
  (`ModelContextProtocol`, `ToolsetRegistry`, `EditorToolset`,
  `AutomationTestToolset`, `ConfigSettingsToolset`, ...) come from the engine or the
  parent workspace and are **not in this repository**. Pick what you need; do not copy
  the whole plugin list verbatim.
- The first UE 6.0 package triggers a FULL COOK and a full shader compile, and a single
  stage can exceed the default 60 minutes. `RunTests.ps1` deliberately stays
  parameterless; adjust it through an environment variable (see
  [Testing and Automation](Testing.md)):

  ```powershell
  $env:UNLUANOVA_RELEASE_TIMEOUT_MINUTES = 180
  .\Tools\RunTests.ps1
  ```

## Acceptance requirements for a new combination

1. Run a complete `RunTests.ps1` on the target engine / platform and produce
   `Saved/ReleaseTests/test/release-summary.json`.
2. Confirm `complete=true` and `passed=true`, and that `provenance.commit` matches the
   current HEAD.
3. Record the result here and in `CHANGELOG.md`, then create an UnLuaNova release tag.
4. Published release claims are generated from the summaries by
   `Tools/UpdateReleaseStatus.py`; never hand-edit the markers on the eight pages.

## Further reading

[Testing and Automation](Testing.md) · [Test Architecture and Acceptance](Testing_Architecture.md) · [Release Notes](ReleaseNotes.md) · [FAQ](FAQ.md)

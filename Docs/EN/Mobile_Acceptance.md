---
title: Mobile Acceptance Path (Android / iOS)
description: Reproducible acceptance path for Android / iOS, prerequisites, script result fields and the criteria to upgrade to "Verified". Both platforms are currently unvalidated; this page only provides a reproducible path.
---

# Mobile Acceptance Path (Android / iOS)

> **Current status: Android / iOS are both UNVALIDATED; this page only provides a
> reproducible path.**
> This repository has only **UE 6.0 / Win64** passing a complete Release; there is
> no mobile compile, package, cooked smoke or on-device acceptance record. This page
> does not claim that Unreal Engine was run, and does not treat a precheck as an
> acceptance verdict. The authoritative status is the
> [Engine and Platform Support Matrix](Engine_Support.md); read this page alongside
> [Production Deployment and Operations](Production_Deployment.md).

## Current status (platform matrix)

| Platform | Local OS | Compile feasibility | Device / cooked smoke | Repository acceptance status |
|---|---|---|---|---|
| Win64 | Windows 11 | Passed | Passed (dual-backend automation) | Verified: UE 6.0 complete Release, 41/41 stages on both backends |
| Android | Windows 11 | Android toolchain variables are set; **whether the source engine's Android platform support is installed is UNVERIFIED** | Needs a device / emulator, **availability UNVERIFIED** | Unvalidated |
| iOS | Windows 11 | **Physically impossible on this host** (needs macOS + Xcode + iOS SDK) | Impossible on this host | Unvalidated |
| Mac / Linux / consoles | - | Declared as compile targets | - | Unvalidated (compile targets, not accepted configurations) |

The main plugin's `UnLuaNova.uplugin` lists `Win64 / Mac / IOS / Android / Linux` in
`PlatformAllowList`, but the README and the
[Engine and Platform Support Matrix](Engine_Support.md) are explicit: this only means
**permitted as a compile target**, not accepted by the test gate. The build and
acceptance toolchain has only ever run on Win64.

## Verified local mobile environment

The following are **verified facts** about the maintainer's machine, recorded here for
accuracy. They do NOT mean the engine's mobile platform support is installed or accepted,
and they are not a requirement for other machines. The script discovers these values
dynamically through environment variables / the registry and **never hardcodes any
machine-local absolute path in its source**.

| Item | Verified value / fact |
|---|---|
| Operating system | Windows 11 (therefore iOS compile and on-device acceptance are physically impossible locally) |
| `ANDROID_HOME` | `C:\Android\Sdk` |
| `NDKROOT` | `C:\Android\Sdk\ndk\23.2.8568313` |
| `ANDROID_NDK_ROOT` | `D:\dev\android-ndk-r21e` |
| `JAVA_HOME` | `C:\Program Files\TencentKona-21.0.5.b1` |
| Source engine | Registered under `HKCU:\Software\Epic Games\Unreal Engine\Builds`; the GUID maps to a UE 6.0 source build, resolved from the `.uproject` `EngineAssociation` via the registry |
| Unverified items | Whether the source engine's Android platform SDK support is installed, and whether a usable device / emulator exists, are **both UNVERIFIED** |

> iOS needs macOS + Xcode + iOS SDK + a signing identity + a physical device. A GitHub
> hosted `macos-14` runner has Xcode but **no UE source engine, no signing identity and
> no provisioned device**, so it cannot build UE; see below and
> `.github/workflows/mobile-acceptance.yml`.

## Android prerequisites and full steps

Prerequisites:

1. A Windows host with a **UE 6.0 source engine** installed and associated (discovered
   through `UNREAL_ENGINE_PATH` or the `.uproject` registered GUID).
2. `ANDROID_HOME` / `NDKROOT` / `JAVA_HOME` set, and UE's Android platform SDK
   configuration completed (the engine's `SetupAndroid` flow). **Whether these are
   ready must be confirmed on the machine; this repository cannot guarantee it.**
3. At least one usable Android device or emulator (`adb devices` lists it), on arm64.
4. This plugin deliberately **does not accept bytecode input** (see the
   [Luau Backend](Luau.md)); `lua.codegen` has **no run acceptance on arm64 / Shipping**
   (see [Production Deployment and Operations](Production_Deployment.md)), so a cooked
   smoke must use source scripts and must not rely on codegen.

Full steps (all Operator actions; this repository does not run them automatically):

```powershell
# 1) Read-only precheck: CI / a device-less host can really run this, verdict=not-run
.\Tools\RunAndroidAcceptance.ps1 -PrecheckOnly

# 2) Precheck only, skip the build
.\Tools\RunAndroidAcceptance.ps1 -SkipBuild

# 3) Full build orchestration (needs a real environment and device; the engine is
#    discovered from UNREAL_ENGINE_PATH / the registry by default)
.\Tools\RunAndroidAcceptance.ps1 -EngineRoot <UE source engine root> -Project <.uproject> -Configuration Development
```

`Tools/RunAndroidAcceptance.ps1` only does **precheck + build orchestration + artifact
check + result output**:

- Precheck: whether `ANDROID_HOME` / `NDKROOT` / `JAVA_HOME` exist, a static probe of
  UE's Android platform SDK configuration, whether `adb devices` lists a device,
  whether the engine `Engine/Build/Build.version` is readable, and the arm64 target
  architecture. These are **best-effort static probes**, not proof of a working toolchain.
- Build: invokes `Engine/Build/BatchFiles/RunUAT.bat BuildCookRun -platform=Android -cook
  -stage -package ...`. The arguments are the **conventional UE 6.0 shape** (for example
  `-architectures=arm64`, `-cookflavor=ASTC`) and **MUST be adjusted to the local engine
  point release / device**; the engine path is discovered from a parameter / environment /
  registry and is never hardcoded.
- Artifact check: whether an APK / AAB exists under `Saved/StagedBuilds`,
  `Binaries/Android`, its size, and whether the archive contains `Content/Script/`
  script assets (see [Script Delivery](ScriptDelivery.md)). This is a **best-effort zip
  entry probe**: cooked assets may live inside `.pak` / `.utoc` (IoStore), so a literal
  path is not guaranteed; a real script smoke must run on a device.

> The script **does not execute an on-device smoke and does not fabricate any device,
> compile or acceptance result**. `-PrecheckOnly` and `-SkipBuild` never trigger a build
> and always yield `not-run`, so CI can honestly run it once.

The standalone workflow `.github/workflows/mobile-acceptance.yml` (`workflow_dispatch`):

- `android-precheck` (`windows-latest`): runs `-PrecheckOnly` and uploads the JSON as an
  artifact. A hosted runner has **no UE source engine**, so the expected
  `verdict = not-run`.
- `ios-precheck` (`macos-14`): performs a static toolchain check and prints "real
  acceptance needs self-hosted macOS + UE source engine + Xcode"; it **does not pretend
  it can build UE**.

## Why iOS needs macOS / a self-hosted runner

- Building an iOS package needs macOS + Xcode + iOS SDK + an Apple signing identity;
  this is **physically impossible on the local Windows 11 host** and is not attempted here.
- A GitHub hosted `macos-14` runner has Xcode but **no UE source engine, no signing
  identity and no provisioned device**, so it cannot carry out UE iOS compile and
  acceptance; `ios-precheck` therefore only performs a static check, outputs `not-run`,
  and **does not claim to have built UE**.
- Real iOS acceptance can only happen on a **self-hosted macOS runner** (UE source engine
  + Xcode + iOS SDK + a physical device + signing), following the same "Verified"
  criteria as Android.

## Script result fields

The result JSON is written to `Saved/MobileAcceptance/android-<utc>.json` (override with
`-OutputDir`). `Tools/tests/test_mobile_acceptance.py` regression-checks the script's
precheck items, the verdict enum, the absence of hardcoded paths, and the "a failing
precheck must never produce passed" rule via static text and constructed JSON.

| Field | Meaning |
|---|---|
| `schema` | Fixed `unluanova.mobile-acceptance/1` |
| `generated_utc` | UTC generation time |
| `platform` / `os` / `configuration` / `mode` | Platform (Android), OS (Windows), configuration, mode (`precheck-only` / `skip-build` / `full`) |
| `engine_root` / `engine_root_source` / `project` | Runtime-discovered engine root, its source (`env` / `registry` / `parameter`), and project path; all real runtime data, not hardcoded |
| `precheck` | `ok` (all prechecks passed), `target_arch` (`arm64`), `checks[]`, `reasons[]`, `notes[]` |
| `build` | `status` (`not-run` / `passed` / `failed`), `command`, `exit_code`, `reason` |
| `artifacts` | `status`, `package` (relative path), `bytes`, `ok`, `has_script_assets`, `script_asset_count`, `notes`, `reason` |
| `smoke` | `status` (`not-run` / `passed`), `evidence`, `device`, `backend`, `reason` |
| `verdict` | Only `passed` / `failed` / `not-run` |
| `reason` | Text explanation of the verdict |
| `verdict_enum` | Allowed enum `['passed', 'failed', 'not-run']` |

Verdict semantics (honest labeling):

- **A failing precheck => `not-run` with reasons, NEVER `passed`** (the script has a hard
  rule and a defensive invariant for this).
- `-PrecheckOnly` / `-SkipBuild` => no build and no smoke were run => `not-run`.
- A real build failure => `failed`; a successful build with missing artifacts => `failed`.
- `passed` is reachable **only when** precheck / build / artifacts are all green AND
  external real cooked on-device smoke evidence is supplied (`-SmokeEvidence`; the
  script only verifies its shape, not its credibility). The script itself **will not**
  produce `passed`. In other words, running this script alone will never yield `passed`;
  that is by design.

## Criteria to upgrade to "Verified"

To upgrade Android or iOS from "Unvalidated" to "Verified", **all** of the following must
hold (consistent with the [Engine and Platform Support Matrix](Engine_Support.md) and
[Release Readiness](Release_Readiness.md)):

1. **Compile passes**: the target platform (Android / iOS) builds and packages with a
   real UE source engine, `BuildCookRun` succeeds, and the APK / AAB (or IPA) is non-empty.
2. **Cooked smoke**: run a script smoke against a cooked (not Editor) build covering
   script loading / require / basic dual-backend capability; bytecode input is not
   accepted, and the arm64 / Shipping run acceptance of `lua.codegen` (not yet done)
   must not be relied upon.
3. **At least one physical device**: install, launch and collect logs on a real device,
   recording device model, OS, architecture, backend and commit; an emulator does not
   substitute for a physical device.
4. **Both backends**: Lua 5.5.1 **and** Luau 0.743-1 each pass the above compile +
   cooked smoke + physical device steps independently.
5. **Release record**: produce traceable evidence (equivalent to
   `release-summary.json`), with `provenance.commit` matching HEAD, write the result into
   the [Engine and Platform Support Matrix](Engine_Support.md) and `CHANGELOG.md`, create
   an UnLuaNova release tag, and have the gate from
   [Testing and Automation](Testing.md) re-check it.

Until then, every platform keeps the "Unvalidated" status and no `passed` conclusion is
valid.

## Further reading

[Engine and Platform Support Matrix](Engine_Support.md) ·
[Production Deployment and Operations](Production_Deployment.md) · [Luau Backend](Luau.md) ·
[Testing and Automation](Testing.md) · [Script Delivery](ScriptDelivery.md) ·
[Release Readiness](Release_Readiness.md)

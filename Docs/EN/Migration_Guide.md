---
title: Migrating from UnLua to UnLuaNova
description: A complete UnLua 2.3.6 migration checklist: globals, config layering, binding, backend choice, static binding and behavioural differences.
---

# Migrating from UnLua to UnLuaNova

The README's "batch-replace the global `UnLua` with `UnLuaNova`" covers step one only.
Upstream stopped at **UnLua v2.3.6 (2023-11)**; UnLuaNova is a community continuation,
not a drop-in replacement.

## 1. Globals and module paths

| UnLua | UnLuaNova |
|---|---|
| `UnLua.Class()` | `UnLuaNova.Class()` |
| `UnLua.Log*` / `UnLua.*` | `UnLuaNova.*` |
| `Content/Script/<module>.lua` | unchanged |
| `require("UnLua.*")` | `require("UnLuaNova.*")` |

`Config/DefaultEngine.ini` keeps `UnLua -> UnLuaNova` `PackageRedirects` / `ClassRedirects`
(lines 12–23) for upgrading old assets. New projects do not need them.

## 2. One config file

Defaults live in `Plugins/UnLuaNova/Config/DefaultUnLuaNova.ini`; the project's
`Config/DefaultUnLuaNova.ini` must contain **overrides only**. Copying the whole file
freezes old defaults and hides later plugin changes.

| Section | Contents |
|---|---|
| `[/Script/UnLuaNova.UnLuaNovaSettings]` | Runtime + feature switches (both backends) |
| `[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]` | Editor behaviour + build config (editor only) |

`LuaBackend` sits in the editor section but is read by Game / Server `Build.cs`, so it is a
compile-time switch.

## 3. Binding (the largest behavioural difference)

| | UnLua | UnLuaNova |
|---|---|---|
| Must implement `IUnLuaInterface` / `GetModuleName` | Yes | **No** |
| Data-driven binding | None | ini `+ClassModuleBindings` or binding component; off by default (`bDataDrivenBinding=False`) |
| Per-instance binding | None | `BindInstance` / `UnbindInstance` (class-level remains the default) |

The default-off switch is a **security boundary**: enabling it binds scripts to every
object matching a mapping or component without that object's knowledge. Read
[Per-instance Module Binding](InstanceBinding.md) first.

## 4. Backend choice

UnLuaNova has two compile-time backends, **Lua 5.5.1** and **Luau 0.743-1**; they never
coexist at runtime. Default is `LuaBackend=Luau`. Switching requires a rebuild; the demo
project ships six isolated targets so both can be prebuilt. Capability differences
(sandbox, native codegen, coverage, read-only tables) are in the
[Capability Matrix](Backend_Capability_Matrix.md).

Moving from UnLua (Lua 5.3/5.4 semantics) to Luau:

- Integer/bitwise syntax, `goto` and 5.4/5.5 `close` semantics are not language-compatible promises;
- `bit32` is not the integer operator semantics;
- `debug.getregistry` is explicitly rejected, `getuservalue` is a nil-only subset,
  `setuservalue` returns false;
- `load()` accepts source only (`'t'` / `'bt'`); pure `'b'` and unknown modes are rejected;
- `io` is an optional rooted buffered subset; `io.popen` is explicitly unsupported.

## 5. Static binding

UnLuaNova exports everything by default (opt-out) and generates marshalling thunks at
build time, removing runtime reflection lookups and marshalling. Add
`meta=(UnLuaNovaNoExport)` to exclude a type. Generated sources live per backend under
`Source/UnLuaNova/Generated/Backends/<Backend>/`, are gitignored and must not be committed.
See [Static Export and Binding](StaticExportBinding.md).

## 6. Declared networking (new capability)

UnLua's `ULuaFunction::IsOverridable()` explicitly excludes `FUNC_Net` functions, so scripts
could only re-implement engine-declared RPCs. UnLuaNova lets scripts declare their own
Server/Client/Multicast RPCs and replicated fields — see
[Lua-declared Networking](LuaNetworking.md) and the [production guide](Networking_Guide.md).
**This capability is pending promotion**; read [Release Readiness](Release_Readiness.md)
before production use.

## 7. Verification

| UnLua | UnLuaNova |
|---|---|
| Manual test runs | `.\Tools\RunTests.ps1` (zero parameters, complete Release) |
| No evidence chain | `Tools/evidence/index.json` + `release-summary.json` + generated markers on eight pages |
| No provenance check | `Tools/ValidateProvenance.py --require-current` |

`RunTests.ps1` takes no parameters; filters, carriers, tiers, `-BuildOnly` and the net matrix
belong to `Tools/RunDevelopmentTests.ps1`. A cold UE 6.0 cook can use
`UNLUANOVA_RELEASE_TIMEOUT_MINUTES`.

## 8. Post-migration checks

```powershell
python -B Tools/AuditRepository.py
python -B Tools/CheckPromotion.py
python -B -m unittest discover -s Tools/tests -p "test_*.py"
```

None of these start UE; they only verify repository consistency. A real migration still
needs one complete Release.

## Further reading

[Features](Features.md) · [Settings](Settings.md) · [API Stability](API_Stability.md) · [FAQ](FAQ.md)

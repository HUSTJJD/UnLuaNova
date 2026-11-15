# Release notes

<!-- release-status: passed -->
<!-- release-provenance: 5f9c9004 -->

**UnLuaNova v1.0.0** (first stable release, 2026-11-15) passed a complete Release on
**Unreal Engine 6.0 / Win64** (41/41 stages on both backends); UE 5.8.3 / Win64 remains
the historical baseline. It is distributed under the **MIT license**.

## What this release contains

- Two backends, **Lua 5.5.1** and **Luau**, selected in `Config/DefaultUnLuaNova.ini` at build time.
- Reflection calls both ways, property access, containers and delegates, and Lua overrides of Blueprint events / UFunctions with `self.Overridden`.
- Static export binding: template exports plus build-time generated binding code, with a reflection fallback.
- Runtime libraries shipped with the plugin: Timer, Event, OOP, HotReload, StringEx, Vector, Buffer and Input; availability is queryable through `UnLuaNova.HasFeature`.
- Observability: CPU profiler into Unreal Insights, DAP debugging, an infinite-loop watchdog, type declaration export and Luau coverage export.
- Lua-declared networking (**Stable**): RPCs and replicated fields declared from scripts, verified on the Legacy / PushOff / Iris / Cooked carriers and all eight connected scenarios on both backends (complete Release `passed=true`, 41/41 stages). See [Release Readiness](Release_Readiness.md).
- One-command build, automation and packaging validation via `Tools/RunTests.ps1`.

## Validation environment

Windows 11 · Unreal Engine 6.0 (source build, v1.0.0 Release) · Win64 · Visual Studio 2026 (MSVC 14.50). UE 5.8.3 / Win64 is the earlier verified baseline.

## Known limitations

- Lua-declared networking is Stable and verified on all four carriers and eight connected scenarios on both backends. Not supported: engine-level seamless travel (`ServerTravel`), declarative-networking hot reload, and RPC authorization (`bRequireRpcAuthorization` defaults to `False`).
- Validated on UE 6.0 / Win64 (v1.0.0) and UE 5.8.3 / Win64 (baseline). Other engine versions and platforms require separate validation.
- Luau native codegen is opt-in and limited to x64 / arm64.
- `lua.sandbox` is off by default; path checks are not an OS-level sandbox.

See [Testing and Automation](Testing.md) for commands and [Features](Features.md) for the capability overview.

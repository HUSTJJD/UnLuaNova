# Release notes

<!-- release-status: not-passed -->
<!-- release-provenance: 0b5b29b2 -->

The current UnLuaNova release targets **Unreal Engine 5.8.3 / Win64** and is distributed
under the **MIT license**.

## What this release contains

- Two backends, **Lua 5.5.1** and **Luau**, selected in `Config/DefaultUnLuaNova.ini` at build time.
- Reflection calls both ways, property access, containers and delegates, and Lua overrides of Blueprint events / UFunctions with `self.Overridden`.
- Static export binding: template exports plus build-time generated binding code, with a reflection fallback.
- Runtime libraries shipped with the plugin: Timer, Event, OOP, HotReload, StringEx, Vector, Buffer and Input; availability is queryable through `UnLuaNova.HasFeature`.
- Observability: CPU profiler into Unreal Insights, DAP debugging, an infinite-loop watchdog, type declaration export and Luau coverage export.
- Lua-declared networking (experimental): RPCs and replicated fields declared from scripts.
- One-command build, automation and packaging validation via `Tools/RunTests.ps1`.

## Validation environment

Windows 11 · Unreal Engine 5.8.3 (source build) · Win64 · Visual Studio 2026 (MSVC 14.50).

## Known limitations

- Lua-declared networking is experimental; its API may change. Engine-level seamless travel (`ServerTravel`), hot reload and authorization checks are out of scope.
- Validated on UE 5.8.3 / Win64. Other engine versions and platforms require separate validation.
- Luau native codegen is opt-in and limited to x64 / arm64.
- `lua.sandbox` is off by default; path checks are not an OS-level sandbox.

See [Testing and Automation](Testing.md) for commands and [Features](Features.md) for the capability overview.

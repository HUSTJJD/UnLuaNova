---
title: "Testing the Extension Plugins"
description: "Test coverage, gaps, and the path to automation specs for LuaProtobuf, LuaRapidjson and LuaSocket"
---

# Testing the Extension Plugins

## What the three plugins are: thin wrappers plus vendored third-party sources

`LuaProtobuf`, `LuaRapidjson` and `LuaSocket` are three optional extension plugins (disabled
by default, independent of each other; see [Plugins and Modules](Plugins_And_Modules.md)).
They are almost pure thin wrappers: each `*Module.cpp` only registers a third-party Lua C
module as a built-in loader with `AddBuiltInLoader` inside `FLuaEnv::OnCreated`, and, where
needed, appends its own script directory to `UnLuaNova.PackagePath`. There is no business
logic in them.

The real implementation is vendored third-party source kept in this repository:

- `LuaProtobuf`: `Source/src` (`pb.cpp` / `pb.h`) and `Content/Script` (`protoc.lua` /
  `serpent.lua`, lua-protobuf);
- `LuaRapidjson`: `Source/src` and `Source/include/rapidjson` (lua-rapidjson / RapidJSON);
- `LuaSocket`: `Source/src` and `Content/Script` (`socket.lua` and helpers, LuaSocket 3.0-rc1).

The third-party C sources do not include `lua.h` directly; they include the `lua.hpp` that
UnLuaNova provides to adapt to the Lua / Luau backends. The third-party components and their
licenses are recorded in `LICENSE-THIRD-PARTY.md` at the repository root. The built-in loader
names actually registered in the sources are `pb` (plus `pb.unsafe`), `rapidjson`, and
`socket` / `socket.core` / `mime.core`; from Lua these are `require("pb")`,
`require("rapidjson")` and `require("socket.core")`. For the debugger dependency on
luasocket, see [Debugging](Debugging.md).

## Current test coverage

Two layers of checks, neither of which needs a UE build or changes the automation case set:

1. **Python static contract**: `Tools/tests/test_extension_plugins.py` (`unittest`, reading
   files with `pathlib` / `re` / `json` only; it loads no Lua and runs no UE). It checks that
   the three `.uplugin` files exist, declare the `UnLuaNova` plugin dependency correctly,
   have a sane `Modules.Type` and an `EnabledByDefault` contract; it extracts the registered
   names from each `*Module.cpp` and verifies they agree with the `require` names in the smoke
   script and those recorded in [Plugins and Modules](Plugins_And_Modules.md); it confirms the
   vendored third-party source directories exist, that `LICENSE-THIRD-PARTY.md` records all
   three components, and that all three module names appear in the smoke script. Run it with:

   ```powershell
   python -B -m unittest discover -s Tools/tests -p "test_extension_plugins.py"
   ```

2. **Lua manual smoke**: `Content/Script/Tests/Extensions/SmokeExtensions.lua`. It requires
   the three modules and makes one minimal real call each (protobuf encode/decode, JSON
   encode/decode, basic socket object creation), isolates each module with `pcall` and returns
   `ok` / `err`, and returns a summary table at the end. A disabled module produces a
   diagnosable error instead of a crash. How to run it and the expected output are in
   `README.md` in the same directory.

## Not covered: UE runtime

This delivery has **no UE runtime verification**: no `*.spec.cpp` is added, no `UnLuaNova*`
automation case is registered, no UE build is run, and no real test report is generated.

- The Python tests are static contract checks only; they do not load these Lua modules and
  do not execute any Lua code.
- The smoke script must be run by hand from the editor Lua console or PIE; its result is an
  `ok` / `err` summary table, not an automation verdict.
- **No claim is made that this was run inside UE.** The behaviour of the third-party
  libraries under the real Lua/Luau backends, and any cross-backend differences, still need
  to be verified in UE.

## Why this is not an automation spec

`Tools/baseline/{lua,lua-game,luau,luau-game}.json` are the case-list baselines for the
`--fail-on-drift` gate; a full Release passes that flag for the Editor cases and for both
report stages (see [Testing and Automation](Testing.md) and
[Testing Architecture](Testing_Architecture.md)). Adding a `*.spec.cpp` registers a new
`UnLuaNova*` case and changes the case-name set, but this delivery cannot run a UE build and
therefore cannot generate a new real case list. Adding a case without refreshing the four
baselines would make a full Release fail on case-list drift.

That is why this work deliberately takes a form that needs no UE build and does not change the
automation case set: a Python static contract plus a Lua manual smoke. The smoke script
registers no case, is referenced by no C++ spec, and running it does not affect the four
baselines.

## Correct path to upgrade to automation specs later

To promote these checks to real automation cases, follow this order; skipping a step trips
the drift gate:

1. Add the corresponding `*.spec.cpp` in the test plugin (`UnLuaNovaTestSuite`) and register
   the `UnLuaNova*` cases; the case-name set changes at this point.
2. Run a full Release (`Tools/RunTests.ps1`) so it really builds and produces the Editor and
   cooked `-game` reports for every backend (never use a failed verification run as a baseline).
3. Refresh the four baselines from that run's **real reports** with `--update-baseline`:
   Editor carriers use `Tools/RunDevelopmentTests.ps1 -UpdateBaseline -Backend <B>` to rewrite
   `Tools/baseline/<B>.json`; cooked carriers use
   `Tools/GenerateTestReport.py --reports <game dir> --out <game dir> --backend <B>
   --baseline Tools/baseline/<B>-game.json --update-baseline` to rewrite
   `Tools/baseline/<B>-game.json`. In other words all four files `lua.json`, `lua-game.json`,
   `luau.json` and `luau-game.json` must be refreshed from real reports.
4. Re-check the drift gate: a full Release passes `--fail-on-drift` for the Editor and both
   report stages; confirm the case totals and case-name sets agree with the four baselines
   before merging. Do not hand-edit a baseline to fabricate cases that never ran.

---

## Further reading

- [Plugins and Modules](Plugins_And_Modules.md) - layout and switches of the extensions
- [Debugging](Debugging.md) - luasocket and the debuggers
- [Testing and Automation](Testing.md) - single-entry Release verification
- [Testing Architecture](Testing_Architecture.md) - how the test plugin is organised

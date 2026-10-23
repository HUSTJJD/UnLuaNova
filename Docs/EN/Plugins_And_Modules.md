---
title: "Plugins and Modules"
description: "What UnLuaNova, UnLuaNovaExtensions and UnLuaNovaTestSuite contain, and how to switch them"
---

# Plugins and Modules

## UnLuaNova

The core plugin - this is the only one a project actually needs.

Modules:

* `UnLuaNova` - the runtime module
* `UnLuaNovaEditor` - editor module: Lua template generation, IntelliSense export, console
  commands
* `UnLuaNovaDefaultParamCollector` - a C# UBT plugin (not declared under `Modules`; UBT
  discovers the `*.ubtplugin.csproj` itself) that collects `UFUNCTION` default parameter
  values

## UnLuaNovaExtensions

An extensions plugin that exists as a worked example: read it when you need to integrate a
third-party Lua library.

Both of these are **disabled by default** - they are optional, only `UnLuaNova` itself is
required, so their `.uplugin` keeps `EnabledByDefault` false and they are not dragged into
another project by accident. Set `"Enabled"` to `true` for the one you want in the project's
`.uproject`; there is no dependency between them, so you can enable just one.

This demo project (`UnLuaNovaDemo`) enables all of them, because the automated tests have to
cover them - see [Testing and Automation](Testing.md).

Modules:

* [LuaSocket](https://github.com/lunarmodules/luasocket) - some pure-Lua debuggers need it
* [LuaRapidjson](https://github.com/xpol/lua-rapidjson) - JSON encode/decode
* [LuaProtobuf](https://github.com/starwing/lua-protobuf) - protobuf encode/decode

All three are Lua C modules, registered by the plugin as built-in loaders in
`FLuaEnv::OnCreated`, so from Lua it is just `require("socket.core")` /
`require("rapidjson")` / `require("pb")`. The C sources are third-party code and do not include
`lua.h` directly: they include the `lua.hpp` UnLuaNova provides, which picks headers by
`LuaBackend` and fills in the `lauxlib` spellings Luau lacks - so switching backends needs no
change in these plugins.

## UnLuaNovaTestSuite

The automated test plugin: specs for the UnLuaNova API plus regression tests for individual
issues.

---

## Further reading

- [Testing and Automation](Testing.md) - how the test plugin is laid out and run
- [Settings](Settings.md) - every setting
- [FAQ](FAQ.md) - frequently asked questions

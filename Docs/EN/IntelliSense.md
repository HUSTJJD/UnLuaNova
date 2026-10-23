---
title: "IntelliSense"
description: "Generating engine type symbols so VSCode and friends can complete the UE API"
---

# IntelliSense

## 1. Generate the hint files

![Generating IntelliSense](../Images/generate_intellisense.png)

Open the UnLuaNova toolbar and click **Export IntelliSense**. It writes the declarations to
`{UEProject}/Plugins/UnLuaNova/IntelliSense`.

That directory is generated per machine and gitignored - its contents depend on which plugins and
modules this machine has. It is not under `Intermediate/`: that is UBT's build scratch and can be
wiped at any time, while this is input for the IDE and the type checkers. Do not confuse it with
`Content/IntelliSense`, which holds the hand-written annotations and *is* checked in.

## 2. Add them to your Lua IDE

![VSCode workspace](../Images/vscode_workspace.png)

With VSCode, follow the demo project's
[UnLuaNovaDemo.code-workspace](https://github.com/HUSTJJD/UnLuaNova/blob/master/UnLuaNovaDemo.code-workspace):
the workspace lists both `Script` and `IntelliSense`, so regenerating refreshes completion
without any copying.

## 3. Check it works

![UE IntelliSense](../Images/ue_intellisense.png)

Type `UE.` in any Lua file of the project - the engine types should come up. If nothing
appears, check that your IDE's Lua language extension is installed and enabled.

## 4. Luau declarations (for luau-lsp)

The same export also produces `UnLuaNova.d.lua`:

One export produces **two dialects in two directories**:

```
Plugins/UnLuaNova/IntelliSense/
├── EmmyLua/             Lua dialect, for IDE completion + LuaLS (Lua backend static check)
│   ├── UE.lua           aggregate entry point for engine types
│   ├── UnLuaNova.lua    UnLuaNova's own API (copied from the plugin's Content)
│   └── <module>/<type>.lua  by UE package, e.g. Script/Engine/, Game/Foo/
└── Luau/                Luau dialect, for luau-lsp (Luau backend static check)
    └── UnLuaNova.d.lua
```

They are split because neither analyzer can parse the other's language - feeding
`declare class` to LuaLS only produces syntax errors.

`EmmyLua/` is type annotation for an IDE and for LuaLS; `Luau/UnLuaNova.d.lua` is
`declare class` / `declare function` declarations for a static checker - **do not `require`
it**, it is only referenced at check time. Put the `.d.lua` next to your scripts and start
the file with `--!strict`, and `luau-lsp` will surface UE API misuse before you run
anything.

See [Luau backend](Luau.md).

---

## Further reading

- [Debugging](Debugging.md) - debugging Lua scripts
- [Luau backend](Luau.md) - declarations and luau-lsp
- [FAQ](FAQ.md) - IntelliSense questions
- [Settings](Settings.md) - the "Generate IntelliSense" setting

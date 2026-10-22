---
title: "Custom Lua Templates"
description: "What 'Create Lua Template' generates, and how to override the templates"
---

# Lua Templates

Once a type is bound to Lua, **UnLuaNova toolbar -> Create Lua Template** writes the script at
the bound module path.

The template is looked up by type name under `UnLuaNova/Config/LuaTemplates`; if there is no
file for that name, the lookup walks up the super class chain and repeats.

## Customising

Put your own template file under `<ProjectDir>/Config/LuaTemplates` - it wins over the plugin's.
The project copies shipped with the plugin are:

```
Plugins/UnLuaNova/Config/LuaTemplates/
├── Actor.lua
├── ActorComponent.lua
├── AnimInstance.lua
├── AnimNotifyState.lua
├── Object.lua
└── UserWidget.lua
```

Inside a template, `ClassName` is replaced with the type name being generated. Files must be
UTF-8 **without** BOM.

---

## Further reading

- [Quickstart for UE Newcomers](Quickstart_For_UE_Newbie.md) - generating the first template
- [Programming Guide](UnLuaNova_Programming_Guide.md) - binding and module path rules
- [Settings](Settings.md) - editor settings

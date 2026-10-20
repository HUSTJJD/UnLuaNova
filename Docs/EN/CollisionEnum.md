---
title: "Custom Collision Enums"
description: "Reaching custom collision channels and query enums from Lua by config name"
---

# Custom Collision Enums

Besides the object types Unreal ships for physics collision, a project can add its own
collision configuration under **Project Settings -> Engine -> Collision**. Epic's own
walkthrough is
[here](https://docs.unrealengine.com/4.27/en-US/InteractiveExperiences/Physics/Collision/HowTo/AddCustomCollisionType/).

From Lua, reach them by the **name you configured**:

Collision channel:
`UE.ECollisionChannel.Pawn`

Object type query:
`UE.EObjectTypeQuery.Player`

Trace type query:
`UE.ETraceTypeQuery.Weapon`

---

## Further reading

- [Programming Guide](UnLuaNova_Programming_Guide.md) - how enums are reached from Lua
- [API Reference](API.md) - interface cheat sheet
- [FAQ](FAQ.md) - frequently asked questions

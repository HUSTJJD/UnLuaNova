---
title: "自定义碰撞枚举"
description: "让 Lua 侧按项目配置名称访问自定义碰撞通道与查询枚举"
---

# 自定义碰撞枚举

除了UE默认在物理碰撞上提供的一些对象类型，我们也可以在 `项目设置 -> 引擎 -> 碰撞` 里新增项目自定义的碰撞配置。具体的配置操作可以参考[官方文档](https://docs.unrealengine.com/4.27/zh-CN/InteractiveExperiences/Physics/Collision/HowTo/AddCustomCollisionType/)。

在Lua中使用的时候，可以直接通过**配置名称**来访问，比如：

碰撞检测通道：
`UE.ECollisionChannel.Pawn`

物体类型查询：
`UE.EObjectTypeQuery.Player`

追踪类型查询:
`UE.ETraceTypeQuery.Weapon`

---

## 延伸阅读

- [编程指南](UnLuaNova_Programming_Guide.md) — 枚举在 Lua 侧的访问方式
- [API 参考](API.md) — 接口速查
- [常见问题 FAQ](FAQ.md) — 高频问题

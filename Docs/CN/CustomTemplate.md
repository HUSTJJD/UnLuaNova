---
title: "自定义 Lua 模板"
description: "定制「创建 Lua 模版文件」生成的模板内容与查找规则"
---

# 生成Lua模版

在绑定类型到Lua后，可以通过 `UnLuaNova工具栏 -> 创建Lua模版文件` 来快速在绑定的路径下生成对应的Lua文件。

默认会根据类型名称在 `UnLuaNova/Config/LuaTemplates` 目录下来查找模版文件，如果文件不存在会使用它的父类名称来继续查找。

## 自定义

可以根据自己的需要，在 `工程目录/Config/LuaTemplates` 目录下创建模版文件，生成的时候会优先使用。

注意文件编码需要为UTF-8(No BOM)。

---

## 延伸阅读

- [UE 新手快速上手](Quickstart_For_UE_Newbie.md) — 生成第一个 Lua 模板文件
- [编程指南](UnLuaNova_Programming_Guide.md) — 绑定与模块路径规则
- [设置项](Settings.md) — 编辑器相关设置

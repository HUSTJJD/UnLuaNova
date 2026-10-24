---
title: 中文文档
description: UnLuaNova 中文文档导航 —— 从新手上手到进阶配置、调试与测试
---

# 中文文档

从「新手上手」开始，或者直接跳到你关心的那一页。

<div class="grid cards" markdown>

- :material-school: **入门**

    [UE 新手快速上手](Quickstart_For_UE_Newbie.md) —— 照图走一遍，完成第一个 Lua 绑定

    [功能清单](Features.md) —— 能做什么、相对 UnLua 多了什么

    [相对 UnLua：原理与优势](WhyUnLuaNova.md) —— 每个增量为什么这么做、收益与边界

    [对标 slua-unreal：差距与吸收](Comparison_sluaunreal.md) —— 另一个主流方案做对了什么、我们怎么补

- :material-book-open-variant: **编程**

    [编程指南](UnLuaNova_Programming_Guide.md) —— 绑定、双向调用、GC 的核心概念

    [Luau 后端](Luau.md) —— Luau 语言特性、沙盒、原生断点与类型检查

    [Lua 驱动对象与 C++ 调 Lua](LuaDrivenObjects.md) —— 绑定对象的既有三条路、从 C++ 类型化调用 Lua

    [Lua 声明式网络](LuaNetworking.md) —— 在 Lua 模块里声明 RPC 与复制属性

    [API 参考](API.md) · [如何实现覆写](How_To_Implement_Overriding.md)

- :material-bookshelf: **内置库**

    [内置库](StdLib.md) —— 随插件发布的 Timer / Event / Vector / Buffer …，以及"什么该进、什么不该进"的边界

- :material-cog: **配置与工具**

    [设置项](Settings.md) · [控制台命令](ConsoleCommand.md) · [自定义模板](CustomTemplate.md)

    [自定义碰撞枚举](CollisionEnum.md) · [插件与模块](Plugins_And_Modules.md)

- :material-bug: **诊断与质量**

    [调试](Debugging.md) · [智能提示](IntelliSense.md)

    [测试与自动化](Testing.md) · [测试报告 Luau](../test-luau-report.md) · [测试报告 Lua](../test-lua-report.md)

- :material-palette: **参考**

    [静态导出绑定](StaticExportBinding.md) · [Showcase 示例](Showcase.md) · [FAQ](FAQ.md)

</div>

!!! tip "找不到想要的内容？"
    欢迎到 [GitHub Issues](https://github.com/HUSTJJD/UnLuaNova/issues) 提问，或直接补一页文档提 PR。

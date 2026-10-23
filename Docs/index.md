---
title: 首页
description: UnLuaNova —— 面向 Unreal Engine 的 Lua 脚本方案，Tencent/UnLua 的社区续作
---

# UnLuaNova

<p style="font-size:1.15rem;opacity:.85">
面向 Unreal Engine 的 Lua 脚本方案 —— <a href="https://github.com/Tencent/UnLua">Tencent/UnLua</a> 的社区续作。
保留 UnLua 的编程手感，补上它缺失的<b>维护</b>、<b>性能</b>与<b>可观测性</b>。
</p>

[在线文档](https://hustjjd.github.io/UnLuaNova/) · [GitHub 仓库](https://github.com/HUSTJJD/UnLuaNova) · [English](#english-docs)

---

## 三个关键增量

<div class="grid cards" markdown>

- :material-table-cog: **静态绑定**

    构建期为声明类型生成绑定表，绕开反射热路径与运行时查找开销。类型选择是 opt-out：默认全导出。

    [静态导出与静态绑定 →](CN/StaticExportBinding.md)

- :material-swap-horizontal: **双 Lua 后端**

    [Lua 5.5](https://www.lua.org/) 与 [Luau](https://luau.org)（默认）均以子模块引入，差异收敛在 `LuaBackend.h`。

    [后端设置 →](CN/Settings.md#lua后端)

- :material-chart-timeline-variant: **Lua 可观测性**

    CPU Profiler 写入 Unreal Insights、内存统计、headless 调试服务、沙盒、死循环看门狗。

    [控制台命令 →](CN/ConsoleCommand.md)

</div>

每一项**为什么这么做、换来了什么、代价在哪**：[相对 UnLua：原理与优势](CN/WhyUnLuaNova.md)
（英文版 [Why UnLuaNova](EN/WhyUnLuaNova.md)）。

## 五分钟跑起来

<div class="grid" markdown>

1. 把 `Plugins` 目录复制到你的 UE 工程根目录
2. 初始化子模块 —— 不初始化会缺 VM 源码：

    ```bash
    git submodule update --init --recursive
    ```

3. 重启工程，新建蓝图
4. UnLuaNova 工具栏 → `绑定` → 填入 Lua 模块路径（如 `GameModes.BP_MyGameMode`）
5. `创建 Lua 模版文件` → 在 `Content/Script/GameModes/BP_MyGameMode.lua` 里写代码

</div>

第一次接触？[UE 新手快速上手](CN/Quickstart_For_UE_Newbie.md) 有每一步的截图。

---

## 中文文档

<div class="grid cards" markdown>

- :material-school: **新手上手**

    [UE 新手快速上手](CN/Quickstart_For_UE_Newbie.md) · [功能清单](CN/Features.md)

- :material-book-open-variant: **编程指南**

    [Luau 后端](CN/Luau.md) · [编程指南](CN/UnLuaNova_Programming_Guide.md) · [如何实现覆写](CN/How_To_Implement_Overriding.md)

- :material-code-braces: **API 参考**

    [API](CN/API.md) · [静态导出绑定](CN/StaticExportBinding.md) · [自定义碰撞枚举](CN/CollisionEnum.md)

- :material-cog: **配置与工具**

    [设置项](CN/Settings.md) · [控制台命令](CN/ConsoleCommand.md) · [插件与模块](CN/Plugins_And_Modules.md)

- :material-bug: **调试与诊断**

    [调试](CN/Debugging.md) · [智能提示](CN/IntelliSense.md) · [测试与自动化](CN/Testing.md)

- :material-palette: **进阶**

    [自定义模板](CN/CustomTemplate.md) · [Showcase 示例](CN/Showcase.md) · [FAQ](CN/FAQ.md) · [许可与商业授权](CN/License.md)

</div>

## English Docs

<div class="grid cards" markdown>

- :material-book-open-variant: **[Programming Guide](EN/UnLuaNova_Programming_Guide.md)**

    Core concepts: binding, calling the engine from Lua, calling Lua from the engine, GC.

- :material-language-lua: **[Luau Backend](EN/Luau.md)**

    Language features, sandboxing, native breakpoints and luau-lsp on the Luau backend.

- :material-api: **[API Reference](EN/API.md)**

    C++ API, Lua API and the build-time macros.

- :material-call-split: **[How To Implement Overriding](EN/How_To_Implement_Overriding.md)**

    How Blueprint events get replaced by Lua implementations.

- :material-scale-balance: **[License](EN/License.md)**

    Polyform Small Business 1.0.0 with tighter size limits; commercial license above threshold.

- :material-help-circle: **[FAQ](EN/FAQ.md)**

    Frequently asked questions.

</div>

---

## 状态

| 项 | 值 |
|---|---|
| 当前版本 | **1.0.0**（[CHANGELOG](https://github.com/HUSTJJD/UnLuaNova/blob/master/CHANGELOG.md)） |
| 引擎基线 | **UE 5.8.3**（源码版） |
| Lua 后端 | **Luau（默认）** ／ Lua 5.5 —— 均以子模块引入 |
| 上游基线 | Tencent/UnLua v2.3.6 + `develop`(2026-04) |
| 许可 | Polyform Small Business 1.0.0 ／ 商用超阈值需付费 |

## 测试结果

自动化测试在**两个后端各跑一遍全量**，报告随仓库发布 —— 不是抽样，是仓库里的每一条用例：

| 后端 | 用例 | 通过 | 失败 | 耗时 | 静态类型检查 |
|:---|---:|---:|---:|---:|:---|
| **Luau**（默认） | **518** | 518 ✅ | 0 | 205.9s | `luau-lsp` strict：**0 错误** |
| Lua 5.5 | 509 | 509 ✅ | 0 | 225.1s | LuaLS：**0 错误** |

完整报告：[Luau 后端](test-luau-report.md) · [Lua 5.5 后端](test-lua-report.md)

!!! tip "上生产前"
    Luau 后端 518 条全绿，可以评估与接入。建议锁定版本，并先读完两节已知边界：
    [相对 UnLua：原理与优势](CN/WhyUnLuaNova.md#六代价与已知边界) 与
    [Luau 后端](CN/Luau.md#六仍未完成的部分)。

## 从 UnLua 迁移

Lua 全局名 `UnLua` → **`UnLuaNova`**，UCLASS 与 ini section 同步改名（需 resave 资产）：

```lua
local M = UnLua.Class()       -- UnLua
local M = UnLuaNova.Class()   -- UnLuaNova
```

上游 2019–2023 年的历史变更见 `Archive/UnLua_CHANGELOG.md`（本站导航未收录，请从仓库查看）。

---

<p align="center">
觉得有用？给 <a href="https://github.com/HUSTJJD/UnLuaNova">HUSTJJD/UnLuaNova</a> 点个 Star ⭐
</p>

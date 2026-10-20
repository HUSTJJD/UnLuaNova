---
title: "控制台命令"
description: "lua.do / lua.profiler / lua.memstats / lua.dbg / lua.sandbox 等运行时调试命令"
---

# 控制台命令

## 概述

在游戏运行的时候直接执行一些调试命令，比如调用特定对象上的方法、修改属性等等，可以为我们调试游戏环境提供一些便利。

默认按 `~` 键可以呼出引擎自带的命令控制台，UnLuaNova自带了一些基础的命令，如果你有一些好的想法，也可以参考 `UnLuaNovaConsoleCommands` 的实现来扩展。

## 命令清单

### lua.do \<code\>

在默认环境执行一段Lua代码。

示例：
```
lua.do print("hello world")
```

### lua.dofile \<module path\>

在默认环境执行指定的Lua文件。

示例：
```
lua.do test1.test2
```

### lua.gc

在默认环境强制执行一次垃圾回收。

### lua.profiler \[on\|off\]

Lua 函数级 CPU 性能分析，数据写入 **Unreal Insights**。

开启后会同时点亮两侧的数据：

- **Lua 侧**：Lua 内部函数的执行事件
- **UE 侧**：`UFunction` 在进出 Lua 时的作用域（`CallLua` / `CallUE` / `CallUEFast`）

因此在 Insights 里可以直接看到 `UE 函数 → Lua 函数` 的完整调用链。不带参数时输出当前开关状态。

示例：
```
lua.profiler on
lua.profiler off
```

*注：与「无限循环检测」可以同时开启。两者都需要 VM Hook 槽，过去会互相覆盖，现在统一由 Hook 分发器分配，不再冲突。*

### lua.memstats \[on\|off\|reset\]

Lua 堆内存统计。

- `on`：开启统计
- `off`：关闭统计
- `reset`：清零所有计数器
- 不带参数：输出当前报告

报告分两部分：

1. **总量**：当前占用、峰值、分配 / 重分配 / 释放次数
2. **分类**：按「内存类别」「Lua 类型」「userdata 标签」三个维度各输出 Top N

示例：
```
lua.memstats on
lua.memstats
lua.memstats reset
```

*注：分类统计依赖 Luau 后端的分配回调（`onallocate`），在 Lua 5.5 后端不可用，报告中会给出提示。另外分类维度统计的是**分配量（churn）**而不是存活字节 —— 存活与峰值请看总量部分。*

### lua.dbg \[子命令\]

Headless 调试服务。它本身不提供界面，而是把能力暴露出来给外部工具（未来的 DAP 适配器）驱动，同时也可以直接用命令交互。

| 子命令 | 说明 |
|---|---|
| （不带参数） | 输出状态：是否已挂载、是否暂停、命中次数、断点数 |
| `bp <源码> <行号>` | 断点开关，对同一位置再执行一次即移除 |
| `step` | 单步执行 |
| `continue` | 继续执行 |
| `stack` | 打印上次捕获的调用栈 |
| `locals` | 打印上次捕获的局部变量 |
| `clear` | 清除所有断点 |
| `detach` | 卸载调试 Hook |

示例：
```
lua.dbg
lua.dbg bp MyActor 42
lua.dbg stack
```

*注：不敲命令时不安装任何 Hook，常态零开销。断点与单步的原生能力（`lua_breakpoint` / `lua_singlestep`）仅 Luau 后端提供，Lua 5.5 后端退化为按行号匹配。*

### lua.sandbox（启动期 CVar）

加固 Lua 全局表（Luau 的 `luaL_sandbox`）：开启后全局表只读，脚本无法意外改写引擎绑定。

它是一个**控制台变量，不是可执行命令** —— 每个 Lua 环境创建时读取一次，运行中修改不会影响已经存在的环境。
要开启请写进 ini，让它在启动时生效：

```ini
; Config/DefaultEngine.ini
[ConsoleVariables]
lua.sandbox=1
```

*注：默认关闭，因为这是对已有脚本的行为变更。该能力依赖 Luau 原生沙盒，Lua 5.5 后端不支持。
详见[Luau 后端](Luau.md#沙盒)。*

---

## 延伸阅读

- [调试](Debugging.md) — 断点调试 Lua 脚本
- [设置项](Settings.md) — 相关设置项（后端、看门狗等）
- [Luau 后端](Luau.md) — 沙盒、原生断点、分配分类统计等 Luau 专属能力
- [测试与自动化](Testing.md) — 自动化测试与报告

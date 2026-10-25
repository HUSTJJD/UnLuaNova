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

### lua.gc \[auto \| slice \[kb\] \| budget \<kb\> \| mult \<pct\> \| limit \<kb\> \| stats\]

Lua 堆的垃圾回收。不带参数 = 一次同步全量回收（旧行为，一次帧尖峰）。

| 子命令 | 说明 |
|---|---|
| `auto` | 交回 VM 自己调度（默认，也是 1.0.0 之前唯一的行为） |
| `slice [kb]` | 切到**帧分摊**模式：停掉 VM 自己的节奏，改为每帧步进，可选每帧预算（KB）。会 `LUA_GCSTOP`，也就是**放弃 Lua 5.5 的分代回收**，只在过场 / 加载界面这类已知安全点才划算 |
| `budget <kb>` | 每帧最多做多少工作量（KB）；`0` 表示不限 —— 那正是帧分摊要避免的尖峰 |
| `mult <pct>` | 回收器追赶分配器的速度，`100` = 分配多少收多少（同 Luau 的 `gcstepmul`） |
| `limit <kb>` | 硬上限：堆超过它就整轮回收（宁可吃一次尖峰，也不要把内存跑爆）；`0` 关闭 |
| `stats` | 输出模式、当前堆、峰值堆、欠账、帧数、步进总量与最差单帧耗时 |

命令行上的大小都是 **KB**（与 `lua.memstats` 一致），底层 API 是字节。

示例：
```
lua.gc                 -- 一次全量回收
lua.gc slice 64        -- 帧分摊，每帧最多 64 KB
lua.gc stats
lua.gc auto            -- 退回 VM 自己调度
```

**欠账怎么算**：债务 = 「距上一次**完整周期**以来堆增长了多少」，不是「这一帧分配了多少」。
Lua 在跑完一轮之前不会释放任何东西，所以"先建 4 MB 再整个丢掉"的脚本，这一帧的分配量是 0、
待回收的垃圾却是 4 MB —— 按每帧分配量记账永远不会去收它。

> **默认 `Automatic`，也建议保持。** 这里没有周期性 GC 尖峰要修：全仓库对
> `FLuaEnv::GC()` 的调用方只有本命令一处，没有任何自动调用；两个后端本来就各自在跑
> 分代 / 增量。`slice` 是把这件事接管过来，代价是放弃 Lua 5.5 的 `LUA_GCGEN` 分代模式 ——
> 只有在过场、加载界面这类已知安全点主动调 GC 时才值得，先测再换。

**为什么不是多线程 GC**：Lua 堆是单个 `lua_State`，判断可达性要遍历持有（也被持有于）UObject 的
userdata，这些只能在游戏线程上做；把清扫搬去工作线程意味着改 VM 的回收器并给每次分配加锁。
把同一份工作摊到若干帧，延迟收益一样，风险小得多。

详见[架构](Architecture.md#所有权与调度)与 [GC 调度器 API](CxxApi.md#帧分摊-gc)。

### lua.profiler \[on\|off\|agg on\|off\|reset\|top \[n\]\|dump \[文件\]\]

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

除了上面的开关，还有一组**按函数聚合**的子命令 —— Insights 适合看时间线，不适合回答
"我这 400 个 Lua 函数里到底哪个是热点"，因为它没有自底向上的视图：

| 子命令 | 说明 |
|---|---|
| `agg on\|off` | 打开 / 关闭按函数聚合（默认关：每次调用多一次栈压弹） |
| `reset` | 清零聚合结果 |
| `top [n]` | 打印 Self 时间最高的前 n 个函数（默认 20） |
| `dump [文件]` | 把同一张表写成 CSV，落到 `Saved/Profiler/<文件>`（默认 `lua-profiler.csv`），便于 CI 跨提交对比 |

示例：
```
lua.profiler on
lua.profiler agg on
lua.profiler top 20
lua.profiler dump my-run.csv
```

*注：**Self 时间只在 Lua 5.5 后端有意义**。Luau 的 hook 分不出 call / return，拿不到调用区间，
也就减不掉被调函数的耗时 —— 那一列恒为 0，`Hits` 记的是安全点采样次数而不是调用次数。
采样最多的函数仍然是热点函数，排序在这两个后端上都对。*

### lua.memstats \[on\|off\|reset\]

Lua 堆内存统计。

- `on`：开启分配次数和分类统计
- `off`：停止详细统计，存活字节仍持续记账
- `reset`：清零区间次数和分类分配量，将峰值设为当前占用；不清除仍存活的字节
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

### lua.dap \[start \[端口\] \| stop \| status\]

DAP（Debug Adapter Protocol）服务：把 headless 的调试服务接成 IDE 能 attach 的服务。

| 子命令 | 说明 |
|---|---|
| `start [端口]` | 起服务（默认取 `DefaultUnLuaNova.ini` 的 `DebugAdapterPort`，缺省 `5699`），等 IDE 以 **attach** 方式连上来 |
| `stop` | 停服务并断开客户端 |
| `status` | 输出是否在跑与端口号 |

VSCode 侧配 `request: "attach"`、连 `127.0.0.1:5699` 即可断点 / 单步 / 看局部变量。
受统一开关 `bDebugAdapter` 控制（默认开）：关掉时 `lua.dap start` 会直接拒绝。
命中断点时游戏线程**阻塞自旋**等 `Continue()`，编辑器 UI 会跟着卡住，属预期；
暂停中改断点要等 `Continue()` 之后才生效。

`pause` 与 `evaluate` 现已支持：

* `pause`：请求在**下一行**停下。Lua 无法被强行打断，所以它是"让下一行成为停止点"，
  实际停止会在很短之后发生；Luau 后端靠临时武装单步实现（落地即解除）。
* `evaluate`：断点命中时求值。表达式被编译成
  `return (function(<该帧的局部变量...>) return <表达式> end)(...)`，
  所以 `self.HP`、`i + 1` 写起来就是看上去的意思。**upvalue 不参与**（需要把函数对象压栈，
  两个后端写法不同，暂时没人需要）。没暂停时（或没带 `frameId`）只对全局求值，
  供 watch / repl 用。

详见[调试](Debugging.md#使用-dap-服务在-ide-里调试luadap)。

### lua.dbg \[子命令\]

Headless 调试服务。它本身不提供界面，而是把能力暴露出来给外部工具（`lua.dap` 的 DAP 适配器）驱动，同时也可以直接用命令交互。

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
详见[Luau 后端](Luau.md#配置与安全)。

CVar 本身在两个后端上都存在，所以 `lua.sandbox` 在任何后端都能读写，不会因为后端不支持而变成
"命令不存在"——只是写入在 Lua 5.5 下不产生效果。脚本要判断它是否真的生效，用
`UnLuaNova.HasFeature('sandbox')`。

---

## 延伸阅读

- [调试](Debugging.md) — 断点调试 Lua 脚本
- [设置项](Settings.md) — 相关设置项（后端、看门狗等）
- [Luau 后端](Luau.md) — 沙盒、原生断点、分配分类统计等 Luau 专属能力
- [测试与自动化](Testing.md) — 自动化测试与报告

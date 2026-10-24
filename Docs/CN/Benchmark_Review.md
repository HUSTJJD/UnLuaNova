---
title: "对标复盘：现状体检与改进方向"
description: "第二轮对标。覆盖功能丰富度、性能、易用性、多后端与后端特性、测试与工程化，逐项给出证据、判定与改进方向。"
---

# 对标复盘：现状体检与改进方向

本文是[对标 slua-unreal](Comparison_sluaunreal.md) 之后的**第二轮**，视角换了：

* 第一轮问的是"**对手有什么、我们缺什么**"，落点是补功能；
* 这一轮问的是"**我们已经宣称拥有的东西，到底成色如何**"，落点是找问题。

方法：不看文档写了什么，只看代码做了什么。每条结论都给了 `文件:行`，
能被复核；凡是仓库里核不到的（slua / puerts 的公开说法），单独标注 `未核对`。

> 写下本文时仓库正处于两处重构中途（网络 payload 的"分类型桶 + push model"、
> 静态绑定 thunk）。这本身不影响结论，但它解释了一部分"文档比代码旧"的现象。

---

## 零、一句话结论

**功能丰富度已经不是短板，短板在"成色"和"工程化"上。**

具体三句话：

1. **默认后端（Luau）上有一条真实的内存泄漏** —— 容器 / FText / File 的 userdata
   **永远不会 `__gc`**（`Public/LuaBackend.h:748`）。这不是"少个特性"，是缺陷，
   而且它在默认路径上。
2. **我们宣称的性能数字一条都不可复现** —— 所有数字只存在于文档里，benchmark 产物
   被 `.gitignore:27` 忽略、从没提交过；而 benchmark 用例本身只断言 `elapsed > 0`，
   **性能退化永远不会让测试变红**。
3. **518 条用例跑在本机，不在 CI** —— `.github/workflows/ci.yml` 只做
   `py_compile`、PowerShell 语法解析和 `mkdocs build`。C++ 自动化从未自动跑过。

其余问题（Profiler 在 Luau 上 self time 恒为 0、沙盒默认关、跨后端 `ToVector`
行为不一致、文档里有幽灵 API）性质相同：**能力做出来了，但没有把它坐实。**

---

## 一、多后端：最大的卖点，也是最大的未兑现

双后端（Lua 5.5.1 + Luau）是我们在对标里唯一"对手完全没有"的维度。
但把它拆开看，卖点下面垫着几处没做完的地方。

### 1.1 【P0·缺陷】Luau 上 userdata 永不回收

```cpp
// Public/LuaBackend.h:742-748
 * Luau's substitute, lua_newuserdatadtor(), is NOT wired in yet: every __gc
 * here ends in ContainerRegistry::Remove(), which goes back through FLuaEnv for the
 * main state and then touches the Lua stack, and that is not safe from a GC callback.
 * Closing the gap needs a different cleanup design - explicit release, or a sweep owned
 * by the env - not a rename. Until then Luau simply never finalizes these userdata.
#define UNLUANOVA_HAS_USERDATA_GC 0
```

受影响的注册项：`TArray`（`BaseLib/LuaLib_Array.cpp:591`）、`TMap`(:385)、
`TSet`(:282)、`FText`(`LuaLib_Text.cpp:53`)、文件句柄(`LuaLib_File.cpp:486`)。

**为什么它排第一**：默认后端就是 Luau（`UnLuaNova.Build.cs:141`，ini 键缺失时也是 Luau），
所以这是**开箱路径上的泄漏**，不是某个冷门配置下的缺陷。注释里也说了这不是改个函数名
能解决的 —— `ContainerRegistry::Remove()` 会在 GC 回调里回摸 `FLuaEnv` 和 Lua 栈。

**方向**：换清理的归属，不要在 GC 回调里做这件事。两条路：
① 显式释放（`UE.TArray` 之外再给一个 `Release()`，或引用计数到期自动回收）；
② 由 `FLuaEnv` 拥有一张弱表，每帧/每次 GC 后 sweep 一次。
选②的话 `lua.memstats` 的"churn vs 存活"口径要一起改，否则看不出来修复了没有。

### 1.2 【P1】Profiler 在 Luau 上只剩采样

`Private/LuaProfiler.cpp:73-79, 267-275`：Lua 5.5 用 `LUA_MASKCALL|LUA_MASKRET`
拿到真实的调用/返回区间；Luau 走的是安全点回调，**self time 恒为 0，
`Hits` 数的是安全点次数而不是调用次数**。

对标表里我们写的是"CPU Profiler ✅ 平"。在默认后端上它是半残的，
而 `Docs/CN/ConsoleCommand.md` 没把这件事讲清楚 —— 用户按文档开 profiler，
拿到一张 self time 全 0 的表，只会以为是自己用错了。

### 1.3 【P1】沙盒默认关

`Private/LuaEnv.cpp:191-195`：`lua.sandbox` 默认 **0**，且只有 Luau 能真正生效
（`UNLUANOVA_HAS_SANDBOX`）。对标表里"沙盒 / 只读全局表 —— 我们更强"这句
在**默认配置下不成立**。默认关是对的（硬化全局表是对既有脚本的行为变更），
但它意味着这条优势需要用户主动打开才存在，文档应当明说。

### 1.4 【P1】跨后端行为不一致，而不只是"特性多少"

| 能力 | Luau | Lua 5.5 | 证据 |
|:---|:---|:---|:---|
| userdata `__gc` | ❌ | ✅ | `LuaBackend.h:748/758` |
| 真断点 / 单步 | ✅ | ⚠️ 退化为行号匹配 | `LuaDebugService.cpp:77-99, 241` |
| Profiler 调用区间 | ⚠️ 仅采样 | ✅ | `LuaProfiler.cpp:73-79` |
| 内存分类统计 | ✅ | ❌ | `LuaMemoryStats.cpp:66-98` |
| 沙盒 | ✅ | ❌ | `LuaEnv.cpp:196-213` |
| 原生 `vector` / `buffer` | ✅ | ❌ | `LuaBackend.h` `LUA_VECTOR_SIZE` |
| 分代 GC | ❌ 增量 | ✅ `LUA_GCGEN` | `LuaBackend.h:513-524` |
| 死循环检测 | ✅ 中断点 | ✅ `LUA_MASKLINE` | `LuaDeadLoopCheck.cpp:166-171` |

其中**最该被当成 bug 而不是差异**的一条：`MathLib/LuaLib_FVector.cpp:34,76,138`
—— 在 Lua 后端上 `ToVector` **返回 nil**。同一个脚本在两个后端上跑出不同结果，
比"少一个特性"严重得多。

`HasFeature()` 目前能问 5 个后端能力（`sandbox` / `breakpoint` / `allocCallbacks` /
`vector` / `buffer`，`Private/UnLuaNovaLib.cpp:161-165`）。**userdata 回收与
Profiler 精度这两条问不出来**，而它们恰恰是最影响项目的两条。

### 1.5 【P1】依赖版本不可复现

* ini 里 `LuaVersion=lua-5.4.3`（`Config/DefaultUnLuaNova.ini:88`），实际 vendored
  是 **5.5.1**（`ThirdParty/LuaLib/Lua/lua.h:20-22`），注释写"仅展示"；
* Luau **完全没有版本记录** —— 只有 `Luau/Bytecode.h:538-545` 的
  `LBC_VERSION_MAX=14` 和 `Luau.Build.cs:59` 的 "since upstream release/743"。

后果：升级 Luau 时无法回答"我们从哪个点升到哪个点"，回归也无法归因。
**方向**：把两个后端的精确版本（tag / commit）写进 `Docs/CN/Luau.md` 和一个
`Source/ThirdParty/VERSIONS.md`，并让编辑器"关于"页读它而不是读 ini 里的展示串。

---

## 二、性能：有测量动作，没有测量标尺

### 2.1 【P0】数字只活在文档里

`Docs/CN/Performance_Parity_slua.md` 里的每一个数字（我们的 0.459 µs / 1.439 µs、
slua 的 6.0x / 4.2x、thunk 覆盖 8123/11044）**在仓库里没有任何产物可支撑**：

* benchmark 输出到 `Saved/Benchmark/latest-<Backend>.csv`，
  而 `.gitignore:27` 忽略 `/Benchmark`；
* `Tools/baseline/{lua,luau}.json` 是**用例名基线**（`"total": 518`），不是性能基线。

也就是说：这些数字是某一次本机运行的结果，被誊进了文档。**它们无法被复核，
也无法用于回归**。对标用它们下结论，风险自担 —— 这一点文档自己有警告
（"两台机器、两个引擎版本…不能直接比数字"），但警告不等于解决。

### 2.2 【P0】性能测试不会失败

`Private/Specs/Benchmark.spec.cpp` 与 `BenchmarkParity.spec.cpp` 的断言是
"跑了 N 次迭代，且 elapsed > 0"。**没有上界，没有与基线对比。**
所以哪怕某次提交把「Lua 调 UE 函数」从 1.44 µs 拖到 14 µs，518 条用例照样全绿。

### 2.3 【P2】thunk 覆盖率 73.6%，且最热的路径不在覆盖内

静态绑定 thunk 排除了：net / latent / interface / out-ref / C++ 默认参数 /
`ParmsSize > 256` / 超过 8 参数 —— 这些排除是对的。
但 `IsThunkableParamKind` 里**没有 Struct、Array/Map/Set、Text、Enum**
（`Public/StaticBinding/StaticBindingThunk.h:18-22, 41-55`）。

也就是说：**容器元素访问和结构体编组永远走反射**。对标我们自己的 benchmark 也能看出
这一点 —— `Lua.Container.PointsAreCoplanar`（TArray<FVector> 构建 + 编组）是单独
一条用例，正因为它走的是另一条路。

反射热路径上还有两处每次调用都在付的成本：

* `FFunctionDesc::CallUE` 每次都 `Object->GetFunctionCallspace()`
  （`ReflectionUtils/FunctionDesc.cpp:228`）；
* `ENABLE_TYPE_CHECK` 默认开（`UnLuaNova.Build.cs:115`）→ 每参数一次
  `CheckPropertyType` 虚调用（`FunctionDesc.cpp:413`）。

好消息：参数缓冲是池化的（`ENABLE_PERSISTENT_PARAM_BUFFER` 默认开），
每次调用不 malloc。

### 2.4 【P2】字符串每次调用都转换

`StaticBindingThunk.h:113-120`：String / Name 参数在 thunk 里仍然
`UTF8_TO_TCHAR` + 构造 `FName`/`FString` —— 每调用一次一次。
对于高频的字符串参数函数（UI、日志、网络消息）这是可观的。

---

## 三、易用性与工程化

### 3.1 【P0】没有 CI 跑测试

`.github/workflows/ci.yml` 做三件事：报告生成器的 `py_compile`、
`Tools/` 下 PowerShell 的语法解析、`mkdocs build --strict`。
它自己的注释说明 UE 构建跑不了 —— 于是**C++ 自动化从未自动执行**，
`Tools/RunTests.ps1` 和 `Tools/Install-NightlyTask.ps1` 都是本机/计划任务。

后果：518/518 全绿是"有人跑过"，不是"每次提交都绿"。
对标表里"自动化测试 —— 我们更强"这条，严格说强的是**用例数量**，不是**门禁**。

**方向**：self-hosted runner 上跑一次 nightly（哪怕只跑一个后端），
并把 `Tools/baseline/*.json` 的条数当作门禁（少了就红）。这是最容易补、
也最能改变"测试是资产还是摆设"性质的一步。

### 3.2 【P1】调试只在编辑器里

`lua.dap` 在 `UnLuaNovaEditor` 模块里 → **打包后没有 DAP**；
`lua.dbg` 在运行时模块，打包可用。仓库里**没有一份可抄的 `launch.json`**
（`Docs/CN/Debugging.md:26` 只描述，不给配置）。
另外 DAP 的 `evaluate` **不包含 upvalue**，而 Lua 项目里逻辑常常就活在 upvalue 里
（尤其用了 `HotReload.lua` 的）。

### 3.3 【P2】文档与代码不同步

| 现象 | 位置 |
|:---|:---|
| `API.md` 仍在写 `FLuaContext` / `GLuaCxt`（代码里 0 命中）与已删宏 `SUPPORTS_RPC_CALL` | `Docs/CN/API.md` |
| 许可写 MIT，实际 Polyform Small Business 1.0.0 | `Docs/index.md`、`mkdocs.yml:145` |
| "引擎版本 4.17–5.x"，基线实为 UE 5.8.3 | `Docs/CN/Features.md` |
| 5 个页面只有中文（含本文依赖的 `LuaNetworking` / `StdLib` / 对标两篇） | `Docs/` |
| `EN/Testing.md` 比 `CN/Testing.md` 新（CN 缺 `-FailOnTypeErrors`） | `Docs/` |
| 教程 `13_AnimNotify` 有地图、没有 `13_*.lua` | `Content/Tutorials/` |
| 文档只有 latest，没有版本选择器 | `mkdocs.yml` |

对一个把"文档质量"当作卖点的项目，这些是自伤。

### 3.4 【P2】多 Env 调试缺失

`ULuaEnvLocator_ByGameInstance` 下可以有多个 `FLuaEnv`，但 `lua.dbg` / `lua.dap`
只对一个环境工作。上一轮已列为"两边都缺、我们可以先做"，仍未做。

---

## 四、功能丰富度：净位置

图例：✅ 有且坐实 ｜ ⚠️ 有但有保留 ｜ ❌ 缺 ｜ `未核对` 仓库里没有对照物

| 维度 | UnLuaNova | slua-unreal | 判定 |
|:---|:---|:---|:---|
| 蓝图反射绑定 | ✅ | ✅ | 平 |
| 静态代码生成 | ✅ Commandlet | ✅ libclang | 平（路线不同，我们不维护 clang） |
| 编译期 thunk | ⚠️ 覆盖 73.6%，不含容器/结构体 | ✅ CppBinding 模板展开 | **仍差**（G5 第二期） |
| Lua 声明 RPC | ✅ 已落地，C/S 自动化已能验 | ✅ | **已补平** |
| Lua 声明复制属性 | ⚠️ 已落地，按类型分桶、条件按类型 | ✅ 含数组/嵌套 struct | 小差 |
| C++ 类型化调 Lua | ✅ | ✅ | 平 |
| 覆写蓝图事件 + `self.Overridden` | ✅ opcode 注入 | ⚠️ | 我们更强 |
| 容器 / 委托 / 枚举 / 结构体 | ✅ | ✅ | 平 |
| 扩展方法"不写 C++" | ❌ 需 `UnLuaNovaEx.h` | ✅ | 小差（G6） |
| **多语言后端** | ✅ Lua 5.5 + Luau | ❌ | **我们更强** |
| Luau 上 userdata 回收 | ❌ | — | **自身缺陷**（1.1） |
| CPU Profiler | ⚠️ Luau 上 self time 恒 0 | ✅ 自研工具 | **实质落后**（1.2） |
| 内存 Profile | ✅ 分类统计 | ✅ | 平 |
| 调试器 | ⚠️ 编辑器内 DAP，无 upvalue | ✅ 专属插件 | 小差 |
| 静态类型检查 | ✅ `.d.lua` + luau-lsp（离线） | ⚠️ | 我们更强 |
| 沙盒 | ⚠️ 默认关 | ❌ | 我们更强（需打开） |
| 热重载 | ✅ 云风方案 | ❌ | 我们更强 |
| 自动化测试 | ✅ 518 条 / 双后端 | ❌ | 用例数我们强，**门禁 0**（3.1） |
| 引擎版本 | ✅ UE 5.8.3 | 4.18/4.26/5.1 | 我们更强 |
| 热更补丁包 / 版本 / 回滚 | ❌ | ❌ | 空位，可抢 |
| Lua 内存按来源归因 | ❌ | ❌ | 空位，可抢 |
| 协程泄漏检测 | ❌ | ❌ | 空位，可抢 |

> puerts（TypeScript / V8）是同一赛道上另一个主要对手，主打静态类型与 VSCode 调试，
> 与我们的 Luau 路线正面重叠。**本仓库没有任何对照物**，本表不列 —— 若要做，
> 需要先做一次有源码的对标，否则又是在抄结论。

---

## 五、测试：518 条的覆盖分布

数量已经很好，**分布有洞**。

> 更正：本文初版写"Override 生命周期 / locator / 控制台命令零用例"是错的 ——
> `Overrides.spec.cpp`、`Locators.spec.cpp`、`ConsoleCommands.spec.cpp` 都在，
> 各有 6 条用例。真正的问题不是"没有"，而是**这些用例断言得不够**：
> `Overrides` 没有直接测 `IsOverridable()` 的 flag 规则、没有测"同名二次 Override 不重复添加"、
> 没有走 `bAddNew` 分支；`Locators` 完全没有覆盖默认的 `ULuaModuleLocator_ByBinding`；
> `ConsoleCommands` 对 `lua.profiler` / `memstats` / `gc` / `sandbox` 只断言"没崩"，
> 不断言可观测效果。（这是写这段时没核文件就下的结论，属本文自己的教训。）

| 区域 | 现状 |
|:---|:---|
| `IsOverridable()` 的 flag 规则 / 同名不重复添加 / `bAddNew` 分支 | 只有间接覆盖 |
| `ULuaModuleLocator_ByBinding`（默认 locator）的解析规则 | 零用例 |
| `lua.profiler` / `memstats` / `gc` / `sandbox` | 只测"没崩"，不测可观测效果 |
| `UnLuaNovaDebugBase`（断点 / 栈 / **局部变量**） | 无专有用例；`Observability.spec.cpp` 只覆盖注册与单步 |
| DAP 适配器 | 零用例 |
| `FLuaEnv::HotReload()` | 没被驱动（`HotReload` 只测"模块能加载"） |
| `FDanglingCheck` 逻辑 | 没被驱动（只测开关可设） |
| `Private/Perfs/*` | 注册 **0** 条自动化用例，只出 CSV |
| Lua 侧 | `Content/Script/Tests/**` 大多是给 C++ 用的 fixture，无断言 |

---

## 六、改进路线

排序原则：**先修正确的，再坐实卖点，最后补缺口。** 每条都给了验收 ——
验收不到就不算做完，这是上一轮就定的规矩。

**落地进度**（与本文同一批改动）：P0 三条、P1 四条全部落地；P2 的 DAP upvalue 与
文档校正（幽灵 API 部分）落地；P3 的 DAP upvalue 属于调试项，已落地。
**仍未落地**：thunk 覆盖扩到容器/结构体（需要重跑代码生成器，不是只改代码）、
热更补丁包/版本/回滚、Lua 内存按来源归因、协程泄漏检测、多 Env 调试、
五个 EN 页面、覆盖洞里剩下的 DebugService 专用用例。原因见各条下方标注。

### P0 —— 正确性

| # | 项 | 做法 | 验收 |
|:---|:---|:---|:---|
| 1 | Luau userdata 永不回收（1.1） | 清理归属换到 `FLuaEnv` 的 sweep（或显式释放），不要在 GC 回调里回摸 Lua 栈 | 双后端各一条用例：反复创建销毁 TArray/TMap/FText 后 `lua.memstats` 的容器计数不单调增长 |
| 2 | 性能数字不可复现（2.1） | 提交 `Tools/baseline/perf-{lua,luau}.json`；`RunTests.ps1` 落盘并可选择更新基线 | 仓库里存在一份带机器/引擎版本标记的基线；文档里的每个数字都能指到它 |
| 3 | 性能测试不会失败（2.2） | benchmark 用例断言"不超过基线 × 1.25"而不是 `> 0` | 人为把某条路径改慢后，该用例变红 |

### P1 —— 把卖点坐实

| # | 项 | 做法 | 验收 |
|:---|:---|:---|:---|
| 4 | Profiler 在 Luau 上 self time 恒 0（1.2） | 采样归因出近似的 self time，或至少在 `top`/`dump` 里标明"采样，非精确"；文档写明后端差异 | Luau 下 `lua.profiler top` 不再给出全 0 的 self time，或明确标注口径 |
| 5 | 跨后端差异问不出来（1.4） | 给 `HasFeature` 补 `userdatagc` / `profilerselftime`；`ToVector` 在 Lua 后端返回 nil 的行为要么对齐、要么报错而不是静默 nil | 脚本能问出这两条；同一个 vector 用例在两个后端上结果一致（或一致地报错） |
| 6 | 依赖版本不可复现（1.5） | 新增 `Source/ThirdParty/VERSIONS.md`，记 Lua / Luau 的精确 tag 或 commit；"关于"页读它 | 升级 Luau 时能回答"从哪到哪" |
| 7 | 没有 CI 跑测试（3.1） | nightly（self-hosted）跑一个后端；条数少于 baseline 即失败 | CI 上出现一次真实的用例数输出 |

### P2 —— 补缺口

| # | 项 | 做法 | 验收 |
|:---|:---|:---|:---|
| 8 | thunk 覆盖不含容器/结构体（2.3） | 给 `IsThunkableParamKind` 加 Struct 与 Array/Map/Set 的 kind（G5 第二期） | thunk 覆盖率上升，且 `Lua.Container.*` 用例耗时下降 |
| 9 | 文档校正（3.3） | 一轮 sprint：幽灵 API、许可、引擎版本、缺失的 EN 页、`13_AnimNotify` 脚本 | `mkdocs build --strict` 通过 + 文档里搜不到 `FLuaContext` |
| 10 | 覆盖洞（第五节） | 补 Override 生命周期、DebugBase、DAP、locator 规则、控制台命令行为 | 每个区域至少一条用例 |
| 11 | DAP 的 `evaluate` 不含 upvalue（3.2） | 一并求 upvalue | 断点处能求到 upvalue |

### P3 —— 抢空位

| # | 项 | 说明 |
|:---|:---|:---|
| 12 | 热更补丁包 / 版本 / 校验 / 回滚 | 两条发行线都需要，我们有 `HotReload.lua` 这个起点 |
| 13 | Lua 内存按来源归因 | `lua.memstats` 有分类，还答不了"哪个模块的哪个函数分配的" |
| 14 | 协程泄漏检测 | Latent 函数建协程，悬挂协程没有检测手段 |
| 15 | 多 Env 调试 | `ByGameInstance` 下 dbg/dap 只对一个环境工作 |
| 16 | 跨进程 `RPCId` 一致 | 单进程 C/S PIE 证明不了（两端共用同一个 UClass），需真双进程联机 |

---

## 七、这一轮的教训

上一轮的教训是"**先把对方的动机翻译成我们自己代码里的事实，再决定要不要动手**"
—— 它救回了 G1（平行基类）和 G3（GC 卡顿）两个方向性错误。

这一轮的教训是它的另一半：

> **宣称拥有的能力，要按"默认配置下、用户真的能得到什么"来核，
> 而不是按"代码里有没有这条路径"来核。**

按后一种口径，Luau 的沙盒、Profiler、userdata 回收、双后端一致性、性能数字、
518 条测试，全都算"有"。按前一种口径，它们分别因为**默认关**、**self time 为 0**、
**永不回收**、**返回 nil**、**没有产物**、**没有 CI** 而打折扣。

对标容易上瘾在"补功能"，因为补功能是加法、看得见。但当我们已经在 9 个维度上领先、
只剩 2 个维度落后时，**继续做加法的边际收益，低于把已有的东西坐实**。
本文 P0/P1 的六条里，五条不需要写新能力，只需要把已有的修完。

---

## 延伸阅读

- [对标 slua-unreal：差距与吸收](Comparison_sluaunreal.md) — 第一轮，偏"缺什么"
- [性能对标 slua](Performance_Parity_slua.md) — 本文 2.1 所说的那批数字
- [Lua 声明式网络](LuaNetworking.md) — G2 的落地与已知边界
- [Luau 后端](Luau.md) — 后端差异
- [测试与自动化](Testing.md) — 用例约定（新能力必须带用例）
- [架构](Architecture.md) — 双后端抽象、覆写与静态绑定链路

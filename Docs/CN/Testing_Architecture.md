---
title: "测试框架规划：载体 × 网络角色 × 断言通道"
description: "把用例的‘跑在什么进程里’‘扮演什么网络角色’‘结果怎么汇合’三件事解耦，并给出 P0–P4 路线"
---

# 测试框架规划：载体 × 网络角色 × 断言通道

## 为什么要现在梳理

前几轮一直在往测试框架上打补丁：加了静态绑定、加了 `...ClientServer`、加了
`...DedicatedServer`、录屏多了窗口。每一块单独看都成立，合起来暴露出一个结构问题
——**这三件事从来没被分开过**：

* 用例在**什么进程**里跑；
* 它在网络拓扑里**扮演什么角色**；
* 断言从**哪里**拿到结果。

现在这三件事被压成了一个常量："全都在一个 `UnrealEditor-Cmd.exe` 里跑，网络靠同进程
PIE 模拟，断言直接摸对象指针"。

现有 **118** 个用例文件按"真正需要什么"分（`Tools/ClassifyTests.py` 静态扫出来的，
`Tools/baseline/classification.json`，报告里也有一节）：

| 类 | 需要什么 | 文件数 | 代表 |
|:--|:--|--:|:--|
| A | 纯逻辑，不需要 `UWorld` | **51** | `Specs/LuaBackend.spec.cpp`、`Specs/LuaNet.spec.cpp` |
| B | 要 `UWorld`，不联网 | **62** | `Tests/Issue276Test.cpp:27`、`Specs/LuaLib_World.spec.cpp:36` |
| C | 联网（PIE 会话或跨进程 peer） | **3** | `Tests/LuaNetClientServerTest.cpp:391`、`Tests/LuaNetMulticastTest.cpp` |
| D | 真实视口 / RHI | **1** | `Tests/Issue434Test.cpp:24` |
| E | 截图 / 连拍 | **1** | `Tests/ShowcaseTest.cpp:47,51` |

**真正需要编辑器的只有 D 和 E 那 2 个。** 剩下 115 个被绑在编辑器进程里，是历史惯性
不是需求 —— 编辑器启动占掉整轮的几十秒，而 A 类用例连 world 都不需要。

曾经还有一条更硬的约束（`UnLuaNovaTestSuite.Build.cs`）：

```
PrivateDependencyModuleNames (if Target.bBuildEditor) = { UnrealEd, UnLuaNovaEditor }
```

配合 `TestCommands.cpp` 里的 `#if !WITH_EDITOR` 分支、`GEditor->RequestPlaySession` 与
`FEditorFileUtils::LoadMap` —— 测试套件一度是 **editor-only 的**，连编译 `-game` 目标都过
不去。解掉它需要两处改动：`TestCommands.h` 不再 include 编辑器头（`EPlayNetMode` 改成自己的
`EUnLuaNovaPlayNetMode`，由 `TestCommands.cpp` 映射回去），`FOpenMapLatentCommand` 在
`-game` 下改为"等 world 起来"而不是直接 `return true`。

---

## 一、三轴模型

规划的目标就是把这三件事拆成三个独立可配的轴。

### 轴 1：载体（Carrier）—— 进程是什么

| 载体 | 启动 | 代价 | 什么时候用 |
|:--|:--|:--|:--|
| `Editor` | `UnrealEditor-Cmd` | 几十秒 | 只有要操作编辑器本身时：PIE 编排、资产、工具栏 |
| `Game` | 同一 exe `-game` | 秒级 | **默认**。A / B 两类 113 个用例的真实归属 |
| `Packaged` | 打包二进制 | 构建一次，分钟级 | 只有要验证打包后的行为时，nightly |

`Game` 才是接近真实部署的运行时：编辑器里 `UWorld` 的生命周期、`GetNetMode()`、
网络驱动初始化都跟玩家手上跑的不一样。**用 `-game` 跑 B 类用例不只是更快，是更真。**

### 轴 2：网络角色（Net Role）—— 拓扑里是什么

| 角色 | 说明 |
|:--|:--|
| `None` | A 类，不碰网络 |
| `Standalone` | 有 world 无网络 |
| `ListenServer` / `Client` | 同进程 PIE（`FOpenMapLatentCommand` 现支持的形态） |
| `DedicatedServer` / `Client` | 跨进程，`-game -server` 与 `-game <ip>` |

角色应该是**用例声明**的，不该是"调用某个 latent command 时顺带传了几个参数"。现在只有
`LuaNetClientServerTest.cpp:391` 一处声明了角色，方式是往 `FOpenMapLatentCommand` 传第 3、
4 个参数 —— 这个信息没有任何地方记录，也没有任何工具能按它筛选。

### 轴 3：断言通道（Assert Channel）—— 结果怎么汇合

| 通道 | 适用 | 现状 |
|:--|:--|:--|
| `InProcess` | 同一进程直接摸对象指针 | A/B 两类与同进程 PIE |
| `FileRendezvous` | 跨进程，各端写 JSON，主进程读 | **已落地为 `FNetSession`**（`Public/NetSession.h`），两个联网用例都在用 |
| `IPC` | 未来若需要实时协调 | 未做，不急 |

---

## 二、目标形态

用例声明需求，框架决定编排：

```cpp
// 设想（不是现在就要写的 API，是目标）
BEGIN_TESTSUITE_EX(FMyTest, "UnLuaNova.X",
    .Carrier = ECarrier::Game,          // 默认；只有要编辑器才写 Editor
    .NetRole = ENetRole::Standalone,
    .Tier    = ETier::Fast)
```

相应地：

* **载体**由 Runner 决定：默认用 `-game` 拉起，需要编辑器的用例单列一轮。
* **角色**由用例声明进元数据，Runner 按角色组合出会话（要几个 peer、谁先起）。
* **断言通道**由"是否跨进程"自动选：同进程用 `InProcess`，跨进程用 `FileRendezvous`，
  用例作者不用选。

---

## 三、路线

### P0 · 先测出来，别猜 —— ✅ 已完成

现在连"哪一类用例占多少时间"都没有：入库基线 `Tools/baseline/*.json` 只有聚合值
（`total 518 / duration 162.762s`），`names` 数组**没有 duration 字段**；逐用例耗时只
存在于 `Saved/` 下的 `report.md`，而仓库里一份都没有。没有这个数，任何"把 A/B 类挪到
-game 能省多少"都是估的。

* baseline 补 per-test duration，并让 `Tools/GenerateTestReport.py` 的最慢表（`SLOWEST_COUNT`
  在 `:65`）能跨提交对比
* 给用例打上 A–E 分类（先看能不能从代码静态推：`FOpenMapLatentCommand` 的传参个数 +
  `BEGIN_VIEWPORT_TESTSUITE` + `FScreenshot*` 三个 grep 基本能分出来）
* **验收**：能回答"A/B/C/D/E 各占多少用例、多少秒" —— ✅ `Tools/ClassifyTests.py` +
  报告里的"按运行环境分类"一节（用例数 / 耗时 / 占比），baseline 也带上了 per-test
  duration

### P1 · 解耦载体（收益最大） —— ✅ 已完成（脚本与源码侧）

让 B 类 63 个用例脱离编辑器。

* `FOpenMapLatentCommand` 拆两条实现：编辑器 PIE 版（保留现状）与游戏实例版（新）。
  两者对外同一个名字，内部按编译期/运行期条件选。
* `UnLuaNovaTestSuite.Build.cs` 把 `UnrealEd` / `UnLuaNovaEditor` 拆到条件分支或子模块，
  让 `-game` 目标能编出测试模块。
* `StaticBindingGenerator.spec.cpp`（`EditorContext|EngineFilter`）这类纯编辑器用例移出通用模块。
  （已确认它带 `#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR`，`-game` 目标里根本不编译。）
* **落地**：`RunTests.ps1 -Carrier Auto|Editor|Game`。`Auto` 按静态分类挑：这一轮要跑的
  用例里没有 D/E/C 就用 `-game`（编 `<Project>` 目标、跑 `<Project>.exe -game`），否则回编辑器。
  截图目录（`WindowsEditor` / `Windows`）、日志名（`Editor.log` / `Game.log`）、后端 DLL 名字
  （`UnrealEditor-Lua.dll` / `<Project>-Lua.dll`）都跟着载体走。
* **验收**：A+B 两类至少 80% 能在 `-game` 下跑通；整轮耗时下降可量化 —— ⚠️ **源码与脚本
  已就位，但没在装了引擎的机器上跑过（本仓库的开发机只有注册表里的 4.0），数字待补。**

### P2 · 角色成为一等公民 —— ✅ 已完成

* peer 用 **`-game`**，不是编辑器（P1 解锁后才做得到） —— ✅ `FNetSession::ResolvePeerExe()`
  优先用项目自己的 `Binaries/Win64/<Project>.exe`，起不来才退回本进程 exe + `-game`
* 新增 `FNetSession` 抽象：声明拓扑 → 起 peer → 等汇合 → 收结果 → 清理 —— ✅
  `Public/NetSession.h`：`FNetPeerSpec`（角色 / 地图 / 连谁 / headless / 端口）+ 一组
  latent command（launch / wait / terminate / spawn / write report）+ `topology.json`
* 把 `...DedicatedServer` 从"起两个编辑器"改成"起 `-game -server` + `-game <ip>`" —— ✅
  两个 peer 都是 `-game -NullRHI`（headless），不连；`...Multicast` 才是连起来的那个
  （`-game -server` + 两个 `-game 127.0.0.1`）
* **验收**：`...DedicatedServer` 单轮 < 60s（现在两个编辑器进程、超时 180s） —— ⚠️ 同上，
  待实测；结构性收益已经拿到：peer 从编辑器进程换成游戏进程，且不再各自开窗口

### P3 · 分层与 CI —— ✅ 已完成（分层，CI 流水线不归本仓库管）

现在**没有任何"跳过慢测试"的机制**：全仓库 `FastFilter` / `Priority` 零使用，
`EAutomationTestFlags` 只有三种固定组合，`RunTests.ps1 -Filter`（`:151`）是单个前缀字符串，
拼成 `"Automation RunTests $Filter"` —— 一次只能传一个，没有排除语法。

* 用例名分段约定（例如 `UnLuaNova.Slow.*`）或给 `-Filter` 加 `;` / `-Exclude` —— ✅
  采用分段约定：`-Tier Fast` 跳过 `-SlowPrefix`（默认 `UnLuaNova.Slow.`）
* `RunTests.ps1` 支持多 filter —— ✅ 一条 `-ExecCmds` 里排多个 `Automation RunTests`；
  Fast 层的段是从 `classification.json` 的用例名算出来的
* CI 默认跑快层；跨进程层走 nightly —— ✅ 用法层：`.\Tools\RunTests.ps1 -Tier Fast`
  是快层，`-Tier All`（默认）是 nightly 那一层；流水线本身不在这个仓库里
* **验收**：`--Fast` 一轮 < 当前整轮的 40%；CI 有两条不同频率的流水线 —— ⚠️ 数字待实测
* 顺带补上了当时列的"没覆盖"第一条：多客户端下的 Multicast，见 `LuaNetMulticastTest.cpp`

### P4 · 录屏与报告跟着角色走 —— ✅ 已完成

* 录屏现在按"进程名"识别窗口；改为按**角色**命名（`session-server.mp4` /
  `session-client1.mp4`），Runner 从会话拓扑自动推出要录哪几个 —— ✅ `FNetSession` 每起
  一个 peer 就写 `topology.json`（label / role / pid / headless），`RunTests.ps1` 在跑的
  过程中每 3 秒轮询一次，给新出现的非 headless peer 补一个录制，文件名 `session-<label>.mp4`
* headless 的 DS 仍然不录屏，改为把它的日志归档到 `RunDir/logs/` —— ✅ peer 的 `-abslog`
  就写在会话目录的 `logs/` 下，`Copy-NetArtifacts` 把整个 `Saved/UnLuaNovaNet`
  （报告 JSON + 日志 + topology）归档到 `<RunDir>/net/`
* **验收**：一次双端跑挂了，报告里能同时看到两端画面 + DS 日志 —— ⚠️ 待实测

---

## 四、决策记录

**不引入 Gauntlet。** UE 官方的 Gauntlet 正是做多进程测试的，但它面向**打包二进制**，
而我们的迭代节奏在源码态；引入它要先解决打包流水线，成本远大于收益。**借鉴它的角色模型，
不用它的框架。** 若 P2 之后编排复杂度失控，再重新评估。

**不做 IPC 通道。** `FileRendezvous` 已经够：跨进程要验证的是"两端各自看到了什么"，
不是实时协调。IPC 会引入同步复杂度，等有真实需求再说。

**先不做打包层（Packaged）。** 放到 P3 之后。打包验证有价值，但它验证的是打包流程，
而这个框架要验证的是 Lua 绑定行为 —— 两者混在一起会让失败归因变难。

---

## 五、当前进度在规划里的位置

| 已做 | 在规划里的定性 |
|:--|:--|
| `...ClientServer`（同进程 PIE ListenServer） | 保留。它覆盖"每种 RPC 在哪一端执行"，这个**不需要跨进程**，同进程更快更稳 |
| `...DedicatedServer` | **已是目标形态**：两个 `-game` 进程（`-server` / standalone），不连，比对 RPCId。`FNetSession` 里没有一行编辑器逻辑 |
| `...Slow.Net.Multicast` | 新增。1 个 `-game -server` + 2 个连上它的 `-game` 客户端，验证一次 multicast 到达每个客户端进程各自的 Lua |
| 录屏多窗口（`-RecordWindow 标签=进程名`） | 手填仍可用；联网 peer 现在从 `topology.json` 自动识别，不用填 |
| 报告侧多 session 视频 | 已是目标形态，命名来自 peer label |

`LuaNetworking.md` 边界第 1 条（跨进程 `RPCId` 一致）**已经有测试覆盖**，而且现在是
"两个游戏实例"，不再是"两个编辑器进程" —— 措辞已经收紧。

---

## 六、还没覆盖的（不含糊过去）

* ~~多客户端（>1）下的 Multicast~~ —— 已覆盖：`UnLuaNova.Slow.Net.Multicast`
  （`Tests/LuaNetMulticastTest.cpp`，服务器 1 个 + 客户端 2 个，跨进程）
* `COND_OwnerOnly` 字段在非拥有客户端保持默认值
* push model 与 Iris 的行为对照（`net.IsPushModelEnabled` 0/1、`Net.Iris.Enabled`）
* 打包客户端的验证（`Packaged` 载体，见轴 1）

前三项都要求**真的连上**并且**真的有第三个视角**（非拥有客户端 / 另一种复制后端），
跟"不连也能测的 RPCId"不是一回事。`FNetSession` 已经把这些的门槛降到"再加一个 peer"，
剩下的是用例本身怎么写。

---

## 延伸阅读

- [测试](Testing.md) —— 现在的用法：一键运行、截图、录屏
- [Lua 声明式网络](LuaNetworking.md) —— 被这套测试覆盖的 feature，含已知边界
- [对标复盘](Benchmark_Review.md) —— P0 里"性能数字无法复核"与 baseline 缺 duration 是同一类问题

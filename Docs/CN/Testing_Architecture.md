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

现有 115 个用例文件按"真正需要什么"分：

| 类 | 需要什么 | 文件数 | 代表 |
|:--|:--|--:|:--|
| A | 纯逻辑，不需要 `UWorld` | **49** | `Specs/LuaBackend.spec.cpp`、`Specs/LuaNet.spec.cpp` |
| B | 要 `UWorld`，不联网 | **63** | `Tests/Issue276Test.cpp:27`、`Specs/LuaLib_World.spec.cpp:36` |
| C | PIE 且联网 | **1** | `Tests/LuaNetClientServerTest.cpp:391` |
| D | 真实视口 / RHI | **5** | `Tests/ShowcaseTest.cpp`（4 个 showcase）、`Tests/Issue434Test.cpp:24` |
| E | 截图 / 连拍 | **4** | `Tests/ShowcaseTest.cpp:47,51` |

**真正需要编辑器的只有 D 和 E 那 9 个。** 剩下 100+ 个被绑在编辑器进程里，是历史惯性
不是需求 —— 编辑器启动占掉整轮的几十秒，而 A 类用例连 world 都不需要。

更硬的一条约束（`UnLuaNovaTestSuite.Build.cs`）：

```
PrivateDependencyModuleNames (if Target.bBuildEditor) = { UnrealEd, UnLuaNovaEditor }
```

配合 `TestCommands.cpp:93` 的 `#if !WITH_EDITOR return true`、`GEditor->RequestPlaySession`
与 `FEditorFileUtils::LoadMap` —— **测试套件是 editor-only 的**。所以"起一个 `-game` 的
peer"现在起不来。上一轮 `...DedicatedServer` 用编辑器冒充 peer，是绕不过去之后的妥协，
不是设计。

---

## 一、三轴模型

规划的目标就是把这三件事拆成三个独立可配的轴。

### 轴 1：载体（Carrier）—— 进程是什么

| 载体 | 启动 | 代价 | 什么时候用 |
|:--|:--|:--|:--|
| `Editor` | `UnrealEditor-Cmd` | 几十秒 | 只有要操作编辑器本身时：PIE 编排、资产、工具栏 |
| `Game` | 同一 exe `-game` | 秒级 | **默认**。A / B 两类 112 个用例的真实归属 |
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
| `InProcess` | 同一进程直接摸对象指针 | 现在唯一的一种 |
| `FileRendezvous` | 跨进程，各端写 JSON，主进程读 | `...DedicatedServer` 已实现，但只此一处 |
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

### P0 · 先测出来，别猜

现在连"哪一类用例占多少时间"都没有：入库基线 `Tools/baseline/*.json` 只有聚合值
（`total 518 / duration 162.762s`），`names` 数组**没有 duration 字段**；逐用例耗时只
存在于 `Saved/` 下的 `report.md`，而仓库里一份都没有。没有这个数，任何"把 A/B 类挪到
-game 能省多少"都是估的。

* baseline 补 per-test duration，并让 `Tools/GenerateTestReport.py` 的最慢表（`SLOWEST_COUNT`
  在 `:65`）能跨提交对比
* 给用例打上 A–E 分类（先看能不能从代码静态推：`FOpenMapLatentCommand` 的传参个数 +
  `BEGIN_VIEWPORT_TESTSUITE` + `FScreenshot*` 三个 grep 基本能分出来）
* **验收**：能回答"A/B/C/D/E 各占多少用例、多少秒"

### P1 · 解耦载体（收益最大）

让 B 类 63 个用例脱离编辑器。

* `FOpenMapLatentCommand` 拆两条实现：编辑器 PIE 版（保留现状）与游戏实例版（新）。
  两者对外同一个名字，内部按编译期/运行期条件选。
* `UnLuaNovaTestSuite.Build.cs` 把 `UnrealEd` / `UnLuaNovaEditor` 拆到条件分支或子模块，
  让 `-game` 目标能编出测试模块。
* `StaticBindingGenerator.spec.cpp`（`EditorContext|EngineFilter`）这类纯编辑器用例移出通用模块。
* **验收**：A+B 两类至少 80% 能在 `-game` 下跑通；整轮耗时下降可量化

### P2 · 角色成为一等公民

* peer 用 **`-game`**，不是编辑器（P1 解锁后才做得到）
* 新增 `FNetSession` 抽象：声明拓扑 → 起 peer → 等汇合 → 收结果 → 清理
* 把 `...DedicatedServer` 从"起两个编辑器"改成"起 `-game -server` + `-game <ip>`"
* **验收**：`...DedicatedServer` 单轮 < 60s（现在两个编辑器进程、超时 180s）

### P3 · 分层与 CI

现在**没有任何"跳过慢测试"的机制**：全仓库 `FastFilter` / `Priority` 零使用，
`EAutomationTestFlags` 只有三种固定组合，`RunTests.ps1 -Filter`（`:151`）是单个前缀字符串，
拼成 `"Automation RunTests $Filter"` —— 一次只能传一个，没有排除语法。

* 用例名分段约定（例如 `UnLuaNova.Slow.*`）或给 `-Filter` 加 `;` / `-Exclude`
* `RunTests.ps1` 支持多 filter
* CI 默认跑快层；跨进程层走 nightly
* **验收**：`--Fast` 一轮 < 当前整轮的 40%；CI 有两条不同频率的流水线

### P4 · 录屏与报告跟着角色走

* 录屏现在按"进程名"识别窗口；改为按**角色**命名（`session-server.mp4` /
  `session-client1.mp4`），Runner 从会话拓扑自动推出要录哪几个
* headless 的 DS 仍然不录屏，改为把它的日志归档到 `RunDir/logs/`
* **验收**：一次双端跑挂了，报告里能同时看到两端画面 + DS 日志

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
| `...DedicatedServer`（起两个编辑器进程） | **过渡态**。逻辑（JSON 汇合、RPCId 比对）是对的，载体是错的。P2 换成 `-game` |
| 录屏多窗口（`-RecordWindow 标签=进程名`） | 底层能力可用；P4 会把它从"手填进程名"改成"从会话拓扑自动推" |
| 报告侧多 session 视频 | 已是目标形态，P4 只需改命名来源 |

`LuaNetworking.md` 边界第 1 条（跨进程 `RPCId` 一致）**已经有测试覆盖**，但在 P2 完成前，
它证明的是"两个编辑器进程各自声明出的 id 一致"，不是"两个游戏实例"。这一条在 P2 之后
要回来收紧措辞。

---

## 六、还没覆盖的（本轮不做，不含糊过去）

* 多客户端（>1）下的 Multicast
* `COND_OwnerOnly` 字段在非拥有客户端保持默认值
* push model 与 Iris 的行为对照（`net.IsPushModelEnabled` 0/1、`Net.Iris.Enabled`）
* 打包客户端的验证

前三项都要**真的连上**，跟"不连也能测的 RPCId"不是一回事，属 P2 之后的 P3。

---

## 延伸阅读

- [测试](Testing.md) —— 现在的用法：一键运行、截图、录屏
- [Lua 声明式网络](LuaNetworking.md) —— 被这套测试覆盖的 feature，含已知边界
- [对标复盘](Benchmark_Review.md) —— P0 里"性能数字无法复核"与 baseline 缺 duration 是同一类问题

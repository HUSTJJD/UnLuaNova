---
title: 正式发布就绪度与判定标准
description: 把实验能力升格为正式发布的 Definition of Done：状态语义、五级证据链、专项检查表与禁止事项。
---

# 正式发布就绪度与判定标准（Definition of Done）

本页定义「一个能力可以正式发布」的**唯一判定标准**。`Tools/feature-matrix.json` 里 40 个
`acceptance` 字段描述的是源码审计状态，散落各处、无法一眼回答"现在能不能发"。本页把它们
收敛成一张可执行的检查表。

## 状态语义（先读这一节）

| 载体 | 含义 | 谁写 |
|---|---|---|
| `feature-matrix.json` 的 `status` | **源码交付状态**（implemented / partial / experimental / planned） | 源码审计，人工 |
| `feature-matrix.json` 的 `verification` | **恒为 `pending-runtime`**，`AuditRepository.py:80` 强制校验 | 门禁，不可改 |
| `Tools/evidence/index.json` | 真实执行证据（含 commit） | `Tools/RecordEvidence.py` |
| `Saved/ReleaseTests/*/release-summary.json` | 一次完整 Release 的判定 | `Tools/ReleasePipeline.ps1` |
| 8 个页面的 `release-status` / `release-provenance` | 对外发布声明 | `Tools/UpdateReleaseStatus.py` |

> **改 `status` 不等于转正。** 矩阵被刻意设计成永不声称执行（"execution claims require an
> evidence schema, not a status edit"）。把 `experimental` 手工改成 `implemented` 只会让
> `AuditRepository.py` 和事实同时失真。`Tools/CheckPromotion.py` 是只读判定器，它不会写任何文件。

## 五级证据链

| 级别 | 名称 | 必须满足 | 证据 |
|---|---|---|---|
| L0 | 源码交付 | 实现、错误处理、诊断、专属测试存在 | 源码 + `feature-matrix.json` |
| L1 | 单元/静态验证 | Python 单测、仓库审计、类型检查、脚本语法通过 | CI 之外的本地命令输出 |
| L2 | 完整 Release | `complete=true`、`passed=true`、`provenance.commit == HEAD`、无 `failed`/`blocked` stage | `Saved/ReleaseTests/test/release-summary.json` |
| L3 | 专项矩阵 | 能力专属场景全部通过（见下） | 各自的 topology / 报告 |
| L4 | 发布声明 | marker 为 `passed`、CHANGELOG 与版本标签 | 8 个页面 + git tag |

L0–L1 可以静态完成；**L2–L3 必须在真实 UE 构建上执行**，本仓库不提供任何绕过方式。

## 实验能力转正专项检查表

### 声明式网络（NET-RPC / NET-PROPS / NET-TYPES，均 P0）

| # | 条件 | 证据 / 命令 | 当前 |
|---|---|---|---|
| 1 | 每个后端的 4 变体（Legacy / PushOff / Iris / Cooked）三 peer 拓扑齐全 | `RunTests.ps1` 的 `net-matrix-Lua` / `net-matrix-Luau` 阶段 + `Saved/ReleaseTests/test/net-matrix/<Backend>/net/variants/*/DedicatedServer/topology.json` | **已满足**：两后端各 4/4 变体拓扑齐全，两个 `net-matrix` 阶段 passed |
| 2 | 每个后端的 8 场景（LateJoin / Reconnect / Dormancy / Relevancy / JoinOrderSwap / PacketLoss / SeamlessReconnect / SchemaMismatch）全部产出拓扑与报告 | 同上 `net/scenarios/*/topology.json` | **已满足**：两后端各 8/8 场景拓扑与报告齐全 |

> **矩阵形状（勿按交叉估算）**：`Tools/RunNetMatrix.ps1` 是**两组独立运行**——
> 8 个场景固定跑在 Legacy 单 cell（`:202-204`），4 个变体各跑一次 DedicatedServer
> （`:155-197`）。每个后端合计 **8 + 4 = 12 个 cell**，两个后端 24 个，
> **不是 8 × 4 = 32 个交叉 cell**。8 个场景从未在 Iris / PushOff / Cooked 下跑过；
> 若要把这三个变体也纳入场景覆盖，属于新增工作量，应单独立项。
> `Docs/CN/Testing.md` 已明确写成「8 场景 + 4 变体两组独立」。
> 这两组现在都由 `RunTests.ps1` 自动执行，无需单独跑 `RunNetMatrix.ps1`。
| 3 | `ValidateRelease.py --net-matrix` 零错误 | 门禁输出 | **已满足**：两个后端 gate 均零错误 |
| 4 | 非 owner 的 Server RPC 被拒绝（唯一 token + 精确 CallLog） | DS 场景断言 | **已满足**：DS 场景真实执行并通过 |
| 5 | 三 peer `RepSchemaVersion` 一致、`VariantRepSchemaVersion` 有区分度 | 声明报告 | **已满足**：真实三 peer 报告通过跨端校验 |
| 6 | Cooked 变体使用真实 staged WindowsServer / Windows exe | topology 的 `exe` 字段 | **已满足**：两后端 Cooked cell 通过 |
| 7 | PacketLoss 使用真实 NetEmulation 且最终值收敛 | 场景报告 | **已满足**：PacketLoss 场景通过 |
| 8 | 正式发布三项能力：Seamless Travel、热重载 schema 变更、RPC 权限鉴权 | 新增场景 + 代码 + 报告 | 已交付，专项验收见 `LuaNet.spec.cpp`；不属 L2/L3 阻断项，`ServerTravel` 仍明确不在范围内 |
| 9 | 声明式网络总开关可彻底关闭 | `HasFeature('declarednetworking')` + 配置 | 已交付，不属 L2/L3 阻断项 |
| 10 | 移除全部 `EXPERIMENTAL` 标记、标题与"不建议生产"措辞 | 源码 / 文档 | **已执行**：6 个头文件横幅、中英文文档标题与措辞均已转正 |

### 其余状态修正（不是待开发功能）

| 能力 | 现状 | 处理 |
|---|---|---|
| `LUAU-CODEGEN` | 文档 `Luau.md:22-32` 已定义为「已交付、带边界的可选优化」，矩阵却标 `experimental` | 状态修正为 `implemented`；arm64 / Shipping / 原生调试附加仍属独立验收项 |
| `LUAU-EXPERIMENTAL` | 上游 Luau 的 integer / 用户类 feature flag，`Luau.Build.cs` 不启用，`Luau.md:36` 明确「不是本插件交付的能力」 | 状态修正为 `planned`（不交付）；**不需要也不应该"转正"** |

## 源码级缺陷：已修复并通过完整 Release 验证

`feature-matrix.json` 的多个 `gap` 曾写着「delivered in source, execution pending」，
但源码审查确认以下 5 项**不是**只差执行。逐项改代码后，已由 2026-11-15 的完整 Release
（41/41 阶段、两后端各 12 个矩阵 cell）编译并运行验证。

| # | 缺陷 | 本轮修复 | 位置 |
|---|---|---|---|
| 1 | Generic payload 读侧只比顶层 tag，不按 type spec 递归校验 | `DecodeLuaNetValue` 在解码成功后复用 `CheckShape` 做递归校验（显式 nil 除外），违反声明即拒绝 | `UnLuaNovaNetTypeSpec.cpp` |
| 2 | `BeginPlay` 一次性冻结 schema，晚绑定组件永久失效（热重载 / travel 的根因） | 抽成可重入的 `RefreshDeclaration()`；新增 `RefreshAllDeclarations()`、generation 自动检测，并在热重载与 travel 完成时自动重建 | `UnLuaNovaReplicationComponent.h/.cpp`、`UnLuaNovaNetPolicy.cpp` |
| 3 | RPC 布局无运行时检测/协商 | 新增 `FUnLuaNovaNet::GetRpcLayoutVersion()` 与组件 `RpcSchemaVersion` 副本；两端不一致时 `OnRep_RpcSchemaVersion` 与 `PushSlots` 明确拒收 | `UnLuaNovaNet.h/.cpp`、`UnLuaNovaReplicationComponent.*`、`UnLuaNovaNetPolicy.*` |
| 4 | 无应用层鉴权 | `CanInvoke` 已插入 `FFunctionRegistry::Invoke` 的实际 RPC dispatch 路径，按 `FUNC_Net*` 判定种类 | `Registries/FunctionRegistry.cpp` |
| 5 | `SchemaMismatch` 只证明「大家都一样」 | 场景内注入故意不同的指纹并走真实协商路径，导出 `InjectedSchemaVersion` / `SchemaMismatchRejected` / `SchemaMismatchReason`；门禁强制要求三者；另补策略层单元测试 | `LuaNetNetScenariosTest.cpp`、`ValidateRelease.py`、`LuaNet.spec.cpp` |

**统一验证结论**：上述 C++ 已编译；故障注入用例在真实多进程会话里产生报告；
RPC 布局不一致时确实拒收；`RefreshDeclaration()` 在 cooked / Shipping 载体下行为正确。

## 禁止事项

- 不要手工修改 `feature-matrix.json` 的 `verification`。
- 不要在 `RunTests.ps1` 的 `net-matrix-Lua` / `net-matrix-Luau` 阶段未全绿之前把 NET-*
  标为 `implemented`，或移除实验标记。
- 不要用一次失败的、或 `provenance.commit` 不等于 HEAD 的运行去刷新发布声明。
- 不要把「源码里写了断言」当成「断言执行过」。

## 当前状态（2026-11-15）：v1.0.0 已正式发布

`Saved/ReleaseTests/test/release-summary.json`：`complete=true`、`passed=true`、
**41/41 阶段 passed/cached**，发布验证绑定 `4a87d214`（v1.0.0 的验证基线）；
`net-matrix-Lua` / `net-matrix-Luau` 均 passed，`promotion-gate` passed，
`CheckPromotion.py --release-gate --strict` 为 **7/7，0 unmet**。
8 个页面的 `release-status` 已由 `UpdateReleaseStatus.py` 生成为 `passed`。

`NET-RPC` / `NET-PROPS` / `NET-TYPES` 已由 `experimental` 晋升为 `implemented`
（`verification` 恒为 `pending-runtime`），头文件与中英文文档的 `EXPERIMENTAL` 标记、
实验性标题及"不建议生产"措辞均已移除。**声明式网络已转正为 Stable。**

L4 的 release tag `v1.0.0` 已由维护者创建在发布提交上（informational，不阻断
`--release-gate`）。发布判定以验证基线 `4a87d214` 为准；tag 提交之后若需把本机门禁
重新绑定到新 HEAD，重跑一次 `RunTests.ps1` 即可。

## 延伸阅读

[引擎版本与平台支持矩阵](Engine_Support.md) · [双后端能力矩阵](Backend_Capability_Matrix.md) · [测试架构与验收](Testing_Architecture.md) · [测试与自动化](Testing.md)

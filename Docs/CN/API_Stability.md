---
title: API 稳定性与版本策略
description: UnLuaNova 公开 API 的稳定性分级、弃用周期、语义化版本规则，以及实验能力晋升为稳定契约的条件。
---

# API 稳定性与版本策略

`Source/UnLuaNova/Public/` 下约 85 个头文件全部对外可见，但**"在 Public 目录"不等于"是稳定契约"**。
本页给出唯一的分级规则。任何新增公开 API 都必须在此登记。

## 四级稳定性

| 级别 | 含义 | 变更规则 | 标记方式 |
|---|---|---|---|
| **Stable** | 正式支持的契约 | 破坏性变更需跨主版本 + 至少一个弃用周期 | 头文件注释 `API: Stable since <版本>` |
| **Bounded** | 已交付但有明确边界（特定后端 / 平台 / 配置） | 边界内按 Stable 对待，边界变化需在 CHANGELOG 声明 | `API: Bounded - <边界>` + 能力矩阵 |
| **Experimental** | 接口与行为可能调整 | 可随时变更，不建议生产依赖 | 头文件横幅 + 文档标题 |
| **Internal** | 实现细节泄漏到 Public，不承诺 | 可随时移除 | `API: Internal - do not include directly` |

## 当前分级

| 区域 | 级别 | 说明 |
|---|---|---|
| `LuaEnv.h` / `Binding.h` / `LuaCore.h` / `LuaModuleLocator.h` / `LuaEnvLocator.h` | Stable | 环境与绑定主路径 |
| `Registries/*` / `ReflectionUtils/*` / `UnLuaNovaPrivate.h` / `UnLuaNovaCompatibility.h` / `UnLuaNovaLegacy.h` | Internal | 不应当作公开契约引用 |
| `LuaBackend.h` 及 12 个 `UNLUANOVA_HAS_*` 能力宏 | Bounded | 按后端分化；见 [双后端能力矩阵](Backend_Capability_Matrix.md) |
| `LuaCoverage.h` / `UnLuaNovaSandbox.h` / `LuaVmHooks.h` | Bounded | Luau 专属；plain Lua 侧为显式空实现，不是待补齐 |
| `UnLuaNovaNetPolicy.h` | **Stable** | 声明式网络的策略与错误码契约（本次新增） |
| `UnLuaNovaNet*.h` / `UnLuaNovaReplicationComponent.h` / `UnLuaNovaNetBuckets.h` | **Experimental → 待晋升** | 晋升条件见 [发布就绪度](Release_Readiness.md) 第 8 节；在 L2–L3 通过前不得改标 Stable |

> 注意：`Docs/CN/Luau.md:36` 写"本插件自身不提供任何实验性能力"，指的是**上游 Luau 的
> integer / 用户类 feature flag**（`LUAU-EXPERIMENTAL`，已标为不交付）。它不覆盖本插件自己的
> 声明式网络实验状态，两者不要混淆。

## 弃用策略

1. 新 API 上线后至少一个次版本内保持旧签名。
2. 弃用必须同时：加 `UE_DEPRECATED(<版本>, "<替代方案>")`、在 CHANGELOG 写明迁移路径、
   在本页把旧条目移入"弃用清单"。
3. 移除只发生在下一个主版本。
4. 纯文字注释 `// Deprecated` 不算弃用标记 —— 目前 `UnLuaNovaBase.h:50/:57` 就是这种情况，
   属于待修正项；唯一正式的 `UE_DEPRECATED` 在 `UnLuaNovaBase.h:160`。

## 实验 → Stable 的晋升条件

不可由手工改文档完成，必须同时满足：

- [发布就绪度](Release_Readiness.md) 的 L2（完整 Release 通过、commit 等于 HEAD）与
  L3（联网矩阵两组独立运行全部零错误：每后端 8 个 Legacy 场景 + 4 个变体，共 12 个 cell，
  **不是 8×4 交叉**）；
- `Tools/CheckPromotion.py` 全部 L2–L4 项为 OK；
- 8 个页面的 `release-status` 由 `Tools/UpdateReleaseStatus.py` 生成为 `passed`；
- 头文件的 `EXPERIMENTAL` 横幅、文档标题与 README 的"不建议生产"措辞统一移除；
- `Tools/feature-matrix.json` 中 `NET-RPC` / `NET-PROPS` / `NET-TYPES` 的 status 由
  `experimental` 改为 `implemented`（`verification` 仍恒为 `pending-runtime`）。

**当前状态：未满足。** 最新 Release 为 `passed=false`，Luau 联网矩阵有 5 个场景缺失。

## 版本与标签

- 采用语义化版本；`.uplugin` 的 `VersionName` 必须与发布 tag 一致（当前 5 份都是占位的 `1.0.0`）。
- 仓库现有的 22 个 tag（`v1.0.0` … `v2.3.6`）全部继承自上游 Tencent/UnLua，**不是 UnLuaNova 的发布**。
- 首个正式发布必须创建 UnLuaNova 自己的 tag，并在 CHANGELOG 写明与上游的分叉点。

## 延伸阅读

[发布就绪度与判定标准](Release_Readiness.md) · [引擎版本与平台支持矩阵](Engine_Support.md) · [双后端能力矩阵](Backend_Capability_Matrix.md) · [迁移指南](Migration_Guide.md)

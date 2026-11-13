---
title: 引擎版本与平台支持矩阵
description: UnLuaNova 已验证 / 已适配 / 未验收的引擎版本、Lua 后端与平台组合，以及新增组合的验收要求。
---

# 引擎版本与平台支持矩阵

本页是支持范围的**唯一权威说明**。README 徽章和 `.uplugin` 的 `PlatformAllowList`
描述的是构建目标与声明，不等于验收状态。两者冲突时以本页为准。

## 状态定义

| 状态 | 含义 |
|---|---|
| 已验证 | 在本仓库的 Release 门禁下跑出过完整通过结果（双后端构建 + Editor/cooked 自动化 + 联网 + 类型检查 + 打包 + 性能） |
| 已适配 | 源码含针对该版本的条件编译分支，但没有对应版本的完整 Release 通过记录 |
| 未验收 | 无自动验收记录，不承诺可用（GitHub Actions 只构建文档站，不承担任何引擎/平台验收） |

## 矩阵

| 引擎 | 适配方式 | Lua 5.5.1 | Luau 0.743-1 | 平台 | 状态 |
|---|---|---|---|---|---|
| UE 5.8.3 | `UE_5_8_OR_LATER` / `UE_VERSION_OLDER_THAN(5,8,0)` | ✓ | ✓ | Win64 | 已验证（2026-11-03 baseline，`347bb8c5`） |
| UE 6.0 | `!UE_VERSION_OLDER_THAN(6,0,0)` 与 UE 6.0 移除 API 的替换 | ✓ | ✓ | Win64 | 已适配，未验收：最新 Release 在 `Luau-package-development` 冷 cook 阶段 60 分钟超时 |
| UE 5.1 – 5.7 | `UE_VERSION_NEWER_THAN` / `OLDER_THAN` 分支 | — | — | Win64 | 已适配，未验收 |
| UE 4.x | `UnLuaNovaCompatibility.h` 继承的兼容层 | — | — | — | 不承诺；仅为 fork 遗留 |
| Mac / Linux / iOS / Android / 主机 | `.uplugin` 的 `PlatformAllowList` 声明为可编译目标 | — | — | 非 Win64 | 未验收 |

> `PlatformAllowList` 的含义是「允许作为编译目标」，不是「已通过验收」。
> 构建与验收工具链（`Tools/*.ps1`、6 个后端隔离 target、UBT 引擎发现）目前只在 Win64 跑过。

## 版本判断的三套体系

| 体系 | 位置 | 用途 |
|---|---|---|
| `UE_5_x_OR_LATER` / `UE_6_x_OR_LATER` | `*.Build.cs`、UBT 插件 C# | 编译期模块规则与告警开关 |
| `UE_VERSION_OLDER_THAN` / `NEWER_THAN` / `NEWER_THAN_OR_EQUAL` | C++ 源码 | 引擎 API 差异分支 |
| `ENGINE_MAJOR_VERSION` / `ENGINE_MINOR_VERSION` 手写比较 | 少量 C++ | 数值比较 |

新增引擎版本支持时三处都要检查；三套体系并存是历史演进结果，暂时不强行统一，
但**任何新增分支都必须同步更新本页**。

## 构建环境说明

- 示例工程 `UnLuaNovaDemo.uproject` 的 `EngineAssociation` 是 GUID，绑定维护者本机的源码引擎安装。
  克隆到新机器后需重新关联引擎，或设置 `UNREAL_ENGINE_PATH`。这不是插件本身的要求，
  集成到自己工程时使用自己的 `.uproject` 即可。
- `.uproject` 显式启用的部分插件（`ModelContextProtocol`、`ToolsetRegistry`、
  `EditorToolset`、`AutomationTestToolset`、`ConfigSettingsToolset` 等）由引擎或父工程提供，
  **不在本仓库内**。集成到自己的工程时按需要取舍，不要照抄整份插件列表。
- 首次在 UE 6.0 上打包会触发 FULL COOK 与全量 shader 编译，单个阶段可能超过默认 60 分钟。
  `RunTests.ps1` 保持零参数设计，通过环境变量调整（详见 [测试与自动化](Testing.md)）：

  ```powershell
  $env:UNLUANOVA_RELEASE_TIMEOUT_MINUTES = 180
  .\Tools\RunTests.ps1
  ```

## 新增组合的验收要求

1. 在目标引擎 / 平台上跑一次完整 `RunTests.ps1`，产出 `Saved/ReleaseTests/test/release-summary.json`。
2. 确认 `complete=true` 且 `passed=true`，并检查 `provenance.commit` 与当前 HEAD 一致。
3. 把结果写入本页与 `CHANGELOG.md`，再打上 UnLuaNova 自己的版本标签。
4. 发布声明由 `Tools/UpdateReleaseStatus.py` 从摘要自动生成，不要手工改 8 个页面的 marker。

## 延伸阅读

[测试与自动化](Testing.md) · [测试架构与验收](Testing_Architecture.md) · [版本说明](ReleaseNotes.md) · [常见问题](FAQ.md)

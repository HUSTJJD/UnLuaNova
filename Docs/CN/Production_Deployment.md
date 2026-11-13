---
title: 生产部署与运维
description: 正式发布前的部署检查、许可范围、安全边界、诊断能力、打包与升级回滚流程。
---

# 生产部署与运维

本页面向把 UnLuaNova 集成进正式项目的团队。发布判定标准见
[发布就绪度](Release_Readiness.md)，API 契约见 [API 稳定性](API_Stability.md)。

## 1. 发布前检查（只有一条命令）

```powershell
.\Tools\RunTests.ps1
```

这条命令内部已包含仓库审计、Python / PowerShell 工具回归、文档站严格构建、
`ValidateProvenance.py --require-current`、双后端联网矩阵（每后端 8 场景 + 4 变体，
两组独立而非交叉）、四份 carrier 的执行证据采集、`CheckPromotion.py
--release-gate --strict` 转正门禁，以及 8 处发布标记的自动刷新。
每一项都是 `release-summary.json` 里的一个阶段，任一失败即整轮失败。

`CheckPromotion.py` 只读，不会替你改任何状态；流水线在 `--release-gate --strict`
之外还会打印完整 L0–L4 报告，其中发布标记与 release tag 属人工决策，只报告不阻断。
不需要单独装依赖、单独跑脚本或单独校验来源。任何阶段未绿都不能发布。

## 2. 许可范围

本仓库（UnLuaNova）采用 **MIT**，见 `LICENSE`；继承与第三方组件按各自许可，
清单见 `LICENSE-THIRD-PARTY.md`。外层 `UEAbilityKit` 是**另一个独立项目**，
有自己的许可，不在本页与发布门禁的考虑范围内。

`Tools/GenerateNotices.py` 保留为独立的清单生成工具（可从仓库内已有事实产出可复现的
`NOTICES.txt`），但按 2026-11-13 的维护者决策，它**不是 Release 阶段**，
也不要求 `Docs/legal/` 下的外部证据文件。

## 3. 安全边界（默认关闭的都不要随手打开）

| 开关 | 默认 | 风险 |
|---|---|---|
| `bDataDrivenBinding` | `False` | 开启后匹配映射/组件的对象会在不知情时被绑脚本 |
| `bDeclaredNetworking` | `True` | 总开关；关闭后运行期不再构造动态 UFunction、不注册复制字段 |
| `bRequireRpcAuthorization` | `False` | 开启但未安装鉴权处理器 = 配置错误，策略层 fail-closed 拒绝 |
| `bAllowHotReloadSchemaChange` | `False` | 开启后复制布局可在热重载时变更，需所有 peer 协商 |
| `lua.sandbox` | 关 | 路径检查不是 OS 级沙盒，不可信脚本需另定能力策略 |
| `lua.codegen.enabled` | 0 | 上游申请可执行代码页；宿主不接管 W^X 策略 |

查询能力用 `UnLuaNova.HasFeature('<名>')`，例如 `'declarednetworking'`、`'sandbox'`。
RPC 鉴权处理器通过 C++ `FUnLuaNovaNetPolicy::SetAuthorizationHandler` 安装；
引擎自带的 owner 检查始终有效，策略层是**额外的应用层**检查。

## 4. 诊断与可观测

| 能力 | 命令 / 入口 | 说明 |
|---|---|---|
| CPU Profiler | `lua.profiler` → Unreal Insights | plain Lua 是 call/return 精确自时；Luau 是安全点归因，不能当精确数值 |
| 断点调试 | `lua.dap` | 只支持 attach，不支持 launch；18 个不支持的 DAP 命令会给出具体原因 |
| headless 调试 | `lua.dbg` | 与编辑器 DAP 是不同部署范围 |
| 内存 | `lua.memstats` / `Saved/ReleaseTests/*/metadata/memory` | 九类用例，含保留引用对照组 |
| 死循环 | `lua.maxsafepoints` / 看门狗 | `DeadLoopCheck` 默认 0（关） |
| 覆盖率 | `lua.coverage` | **仅 Luau**；plain Lua 明确报错 |
| 网络策略日志 | `FUnLuaNovaNetPolicy::SetDeniedLogger` | 每次拒绝给出 `ELuaNetPolicyResult`、原因、对象、schema、generation |

## 5. 打包与平台

- 已验证基线：**UE 5.8.3 / Win64**；UE 6.0 有适配分支但尚无通过的完整 Release。
  完整矩阵见 [支持矩阵](Engine_Support.md)。
- UE 6.0 首次打包触发 FULL COOK 与全量 shader 编译，单阶段可能超过默认 60 分钟：
  `$env:UNLUANOVA_RELEASE_TIMEOUT_MINUTES = 180` 后再跑 `RunTests.ps1`。
- `.uplugin` 的 `PlatformAllowList` 表示**允许编译的目标**，不是已验收平台。
- Shipping 是打包门禁，不等于 Shipping 运行时认证；`lua.codegen` 在 arm64 / Shipping 下
  未做运行验收。

## 6. 升级与回滚

1. 升级前保存当前 `Saved/ReleaseTests/baseline/release-summary.json` 与 commit。
2. 插件升级后**不要**用工程侧的整份 ini 覆盖新默认值；只保留差异项。
3. 生成源码 `Source/UnLuaNova/Generated/Backends/<后端>/` 按后端隔离，不要提交。
4. 回滚以 git tag 为准；仓库现有的 22 个 tag 全部继承自上游 UnLua，不是本插件的发布版本。
5. 发布声明由 `Tools/UpdateReleaseStatus.py` 从摘要生成，任何情况下都不要手工改 8 个页面的 marker。

## 延伸阅读

[发布就绪度](Release_Readiness.md) · [API 稳定性](API_Stability.md) · [引擎版本与平台支持矩阵](Engine_Support.md) · [测试与自动化](Testing.md)

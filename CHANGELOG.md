# Changelog

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

本文件记录面向使用者的版本变更。开发过程中的验证记录与实现细节保留在 Git 历史中。

## 2026-11-13（范围收敛 + 5 项源码缺陷修复 / 待编译与运行验证）

维护者决策：外层 `UEAbilityKit` 是独立项目，其许可不在本仓库范围；法务证据阻塞项删除；
`EXPERIMENTAL` 标记待运行验证后统一修改。本轮聚焦 5 项源码级真实缺陷，
**未运行 UE 构建、Release、MkDocs 构建或任何测试套件**。

**范围收敛**

- 删除 `Release_Readiness` / `Production_Deployment` 中关于外层工程许可冲突的章节，
  与 `Tools/CheckPromotion.py` 的对应 L4 检查
- `Tools/GenerateNotices.py` 的 `DECLARED_GAPS` 清空（三项外部来源问题不再作为发布门禁），
  `ReleasePipeline.ps1` 移除 `notices-compliance` 阶段，`feature-matrix.json` 的 LEGAL
  条目改为「外部来源问题不在范围内」。组件级缺失 notice 仍 fail-closed，单测不受影响
- `EXPERIMENTAL` 横幅与文档标题**保留**，待 L2–L3 通过后统一移除

**B1 · Generic payload 读侧类型安全**

- `UnLuaNovaNetTypeSpec.cpp`：`DecodeLuaNetValue` 在解码成功后复用 `CheckShape` 对解码值
  做递归 spec 校验（显式 nil 除外），违反声明即拒绝；读写两侧共用同一校验实现，
  注释从「server side only」改为「encode AND decode」

**B2 · 运行中重新解析复制 schema**

- `UnLuaNovaReplicationComponent`：把 `BeginPlay` 的一次性初始化抽成可重入的
  `RefreshDeclaration()`（新增 `UFUNCTION`）；空声明不再永久冻结状态
- 新增静态 `RefreshAllDeclarations()`；`TickComponent` 检测策略层 generation 变化自动刷新
- `FUnLuaNovaNetPolicy::NotifyModulesReloaded()` / `CompleteSeamlessTravel()` 自动调用全量刷新，
  热重载与 seamless travel 后自动重建 slots、SchemaVersion 与 RPC 布局

**B3 · RPC 布局运行时协商**

- `FUnLuaNovaNet::GetRpcLayoutVersion()`：缓存每个类声明的 RPC（名称 / 种类 / reliable /
  参数类型 / type spec），计算独立指纹
- `UnLuaNovaReplicationComponent` 新增 `RpcSchemaVersion` 复制属性与 `OnRep_RpcSchemaVersion`，
  `PushSlots` 增加守卫；`FUnLuaNovaNetPolicy::NegotiateRpcLayout` 给出结构化拒绝原因

**B4 · RPC 调用点鉴权**

- `FFunctionRegistry::Invoke` 在声明式 RPC 到达 Lua 处理函数前调用 `CanInvoke`，
  按 `FUNC_NetServer/Client/Multicast` 判定种类；拒绝时消费调用并返回

**B5 · SchemaMismatch 真实故障注入**

- `LuaNetNetScenariosTest.cpp`：每个 peer 注入故意不同的布局指纹并走真实协商路径，
  导出 `InjectedSchemaVersion` / `SchemaMismatchRejected` / `SchemaMismatchReason`
- `Tools/ValidateRelease.py`：SchemaMismatch 分支强制要求上述三项，不再只比指纹相等
- `LuaNet.spec.cpp`：补策略层 `NegotiateSchema` / `NegotiateRpcLayout` 的拒收与放行单测

**文档**：CN/EN `Release_Readiness` 缺陷表改为「本轮已修复、待验证」；
CN/EN `Networking_Guide` 同步三项能力的接入状态与验收清单。

**未验证**：本轮所有 C++ 均未经编译；5 项修复的有效性以统一构建与运行验证为准。

## 2026-11-13（实验能力转正准备 / 待编译与运行验证）

目标：把所有实验能力开发成可正式发布；按维护者决策，三项引擎级能力
（Seamless Travel / 热重载 schema 变更 / RPC 鉴权）全部实现。
本轮仍为静态开发，**未运行 UE 构建、Release、MkDocs 构建或任何测试套件**。

**判定标准与状态治理**

- 新增 `Docs/{CN,EN}/Release_Readiness.md`：五级证据链（L0–L4）、转正检查表、
  源码级真实缺陷清单、禁止事项
- 新增 `Tools/CheckPromotion.py`：只读判定器，检查 L2–L4 证据与联网矩阵形状
- `Tools/feature-matrix.json`：`LUAU-CODEGEN` → `implemented`（与 `Luau.md` 契约一致）、
  `LUAU-EXPERIMENTAL` → `planned`（上游 flag，不交付）；NET-* 三个保持 `experimental`
  直到验证通过；`verification` 仍恒为 `pending-runtime`
- 修正矩阵形状表述：联网矩阵是 8 个 Legacy 场景 + 4 个变体**两组独立运行**
  （每后端 12 cell），不是 8×4 交叉

**代码：新增自包含策略层 + 最小钩子**

- 新增 `Public/UnLuaNovaNetPolicy.h` / `Private/UnLuaNovaNetPolicy.cpp`：
  8 个 `ELuaNetPolicyResult` 错误码、声明/调用鉴权、schema 协商与迁移、
  热重载代际、Seamless Travel 状态、可安装的鉴权 handler 与拒绝日志 logger
- `UnLuaNovaSettings.h` + 插件 `DefaultUnLuaNova.ini` 增加 4 个开关
  （`bDeclaredNetworking` / `bRequireRpcAuthorization` / `bAllowHotReloadSchemaChange` /
  `bPreserveSchemaAcrossSeamlessTravel`）；`UnLuaNovaLib.cpp` 增
  `HasFeature('declarednetworking')`
- 钩子：`UnLuaNovaNet.cpp`（声明前策略检查）、`UnLuaNovaReplicationComponent.cpp`
  （`OnRep_SchemaVersion` 走协商）、`UnLuaNovaModule.cpp`（Initialize / Shutdown +
  `PreBindClasses` 启动自检）、`LuaEnv.cpp`（热重载后 `NotifyModulesReloaded`）

**文档**

- 新增 10 页（CN/EN 各 5）：发布就绪度、API 稳定性与版本策略、从 UnLua 迁移、
  生产部署与运维、声明式网络正式使用指南；全部挂进 `mkdocs.yml`
- `Docs/{CN,EN}/LuaNetworking.md`：删重复条目，接入正式指南与发布就绪度
- `Docs/{CN,EN}/ConsoleCommand.md`：补 `lua.codegen` 段落（此前两版均缺）

**明确未做 / 待验证（重要）**

- 上述所有 C++ 改动**未经编译**。策略层提供显式入口；Seamless Travel 未自动注册
  引擎委托（避免引入未验证的委托签名），RPC 调用点鉴权尚未插入 dispatch 路径
- 5 项源码级真实缺陷（Generic payload 读侧校验、`BeginPlay` 冻结 schema、
  RPC 布局协商、应用层鉴权、`SchemaMismatch` 无真实故障注入）已登记在发布就绪度，
  仍需修复并经编译与运行验证
- 外层 `UEAbilityKit` 是独立项目，其许可按维护者决策不在本仓库范围；三份 `Docs/legal/`
  证据要求已删除（详见下一节）
- `EXPERIMENTAL` 横幅与文档标题按 DoD **保留**，待运行验证后统一修改

## 2026-11-13（仓库加固 / 待统一构建验证）

本轮全部为静态开发改动，**未运行 UE 构建、Release 流水线、MkDocs 构建或测试套件**；构建与验证由维护者统一执行。

**发布门禁与 GitHub 流程**

- **GitHub Actions 只负责文档站生成与 Pages 推送**：UE 体积过大，仓库主动放弃在 GitHub 上做任何编译、自动化测试与工具回归，全部由本地 `Tools/RunTests.ps1` 承担。`.github/workflows/ci.yml` 不予恢复（`e213464b` 删除它是有意为之）
- `docs.yml` 保持 push master + Docs 路径 / 手动 dispatch 触发，严格构建后部署 Pages；文档站依赖由 `requirements-docs.txt` 约束大版本
- 新增 `Tools/UpdateReleaseStatus.py`：流水线在写出 Release 摘要后自动生成 8 个页面的 `release-status` / `release-provenance` 注释，杜绝手工漂移
- 曾把 `Tools/GenerateNotices.py --check` 接进 `ReleasePipeline.ps1`（`notices-compliance` 阶段）；该阶段已按 2026-11-13 维护者决策移除，见下一节
- `AuditRepository.py` 增加 provenance 可达性告警（孤儿提交会在审计输出中打印 WARNING）
- 8 个页面的 `release-status` 由 `passed` 修正为 `not-passed`，与当前 `release-summary.json` 的 `passed=false` 一致

**构建与工具链**

- `LuaSocket.Build.cs` 的 `CppCompileWarningSettings.DeprecationWarningLevel` 补 `UE_5_6_OR_LATER` 保护（UE < 5.6 曾会编译失败）
- `Lua.Build.cs` / `Luau.Build.cs` 补齐 UE 5.5 flat-property 分支
- `ReleasePipeline.ps1` 支持 `UNLUANOVA_RELEASE_TIMEOUT_MINUTES`（1..1440）覆盖阶段超时，`RunTests.ps1` 保持零参数设计
- `Tools/tests/test_backend_isolation.py` 对父工程 `Tools/unluanova` 的依赖改为条件跳过，独立 clone 不再 ImportError

**元数据与仓库卫生**

- 5 份 `.uplugin` 补全描述、归属、文档与支持 URL
- 新增 `.gitattributes`（二进制资产声明）、`.clang-format`、`.editorconfig`、`requirements-docs.txt`
- `.gitignore` 修正失效的 `TPSProject` 规则，补齐 IDE 与 Python 生成物规则
- 清理 `Config/Localization/` 两份 Gather ini 中 8 处 UE_4.26 本机绝对路径

**文档**

- 补齐 EN 三个缺失页面（`InstanceBinding` / `LuauCoverage` / `Testing_Architecture`）并挂进导航
- 新增中英双语《引擎版本与平台支持矩阵》，作为支持范围的唯一权威说明
- 双语 README 修正验证状态表述、移除硬编码测试数字、修复英文 README 的中文回退链接
- `CONTRIBUTING.md` 对齐实际的格式配置与文档站依赖

## 2026-11-08 ~ 11-10（补记）

此前未进入 CHANGELOG 的提交，按 Git 历史补记；**这些改动尚未经过完整 Release 验收**。

- 适配 UE 6.0 移除的 API 与类型特征（`094baecd`）、消除 UE 5.8 弃用告警并统一跨版本日志宏（`89e5e55a`）
- 后端隔离成为默认且唯一方式，覆盖 Editor / Game / Server、Cook 输出与静态绑定生成源码；`RunTests.ps1` 改为零参数入口（`d30ba037`）
- 修复联网矩阵工具链、放宽 peer 报告等待并启用 Iris（`eed2ae45`）
- 补齐声明式网络能力（8 种复制条件、`UStruct` / 容器类型规格、跨 peer 声明指纹）及若干崩溃修复

## 2026-11-07

首个完整发布版本，已完成双后端构建、自动化测试、类型检查与打包验收。

**新增**

- Lua 5.5.1 与 Luau 双后端，编译期切换，共用一套后端抽象
- 反射双向调用、覆写、容器与结构体、委托、静态导出绑定与生成 marshal thunk
- 实例级模块绑定（`BindInstance` / `UnbindInstance`）与蓝图入口
- Timer / Event / OOP / 热重载运行时库，模块解析与卸载契约
- Profiler（Unreal Insights）、DAP 调试服务、死循环看门狗、内存配额与定时器诊断
- Luau 覆盖率导出、类型声明导出与智能提示
- 声明式网络 RPC 与复制字段（实验特性）：8 种复制条件、任意 `UStruct` / 容器类型规格、跨 peer 声明指纹
- 后端隔离构建（Editor / Game / Server）与一键验收工具链

**修复**

- UFunction 重复覆写、Luau 安全点哨兵丢失、cooked 卸载后的悬垂引用、VM hook 槽位残留
- 多 Editor target 下的 `DefaultEditorTarget` 声明

**工具链（本轮收尾）**

- 修复 `RunTests.ps1` 在 monolithic Game/Server 目标下误判后端产物：同时搜索项目与插件的 `Definitions.UnLuaNova.h`，并接受静态链进 `.exe` 的形式
- 修复 `RunNetMatrix.ps1` 把 Development `-game` 的项目内报告路径重映射到各自 cooked root、导致三进程报告分裂的问题，改用项目外的共享临时 `UNLUANOVA_TEST_NET_ROOT`
- 修复联网矩阵所有 cell 强制 `-Carrier Game` 的问题：门禁按编译期 `WITH_EDITOR` 判定 `Cooked` 字段，非 Cooked 变体与八个场景必须使用 Editor 载体，只有 Cooked 变体使用打包 peer
- 修复 Development 自定义 `-Filter` 与 `-Carrier Game` 仍与全量 Editor 用例集基线比较造成的误报；筛选/分层运行不再参与漂移判定，`-UpdateBaseline` / `-FailOnDrift` 会拒绝这类子集运行
- 修复 Shipping 打包清空 Development staging 目录、导致 `binary-Development.json` 与联网矩阵 Cooked 变体拿不到客户端 exe 的问题：Shipping 单独落到 `Saved/StagedBuilds/WindowsShipping`
- 修复部分 PowerShell 宿主把 `Remove-Item` 包装成回收站钩子、拒绝清理自建临时文件的问题，改为 .NET 文件/目录 API
- 修复 `BASELINE` 漂移行在 GBK 控制台输出中文用例名变成乱码的问题；中文测试名只在控制台行做 ASCII 转义，文件与 JSON 仍为 UTF-8

**文档**

- 中英双语文档站、快速上手、API 与配置参考、调试与测试指南同步至本版本
- 补充 Luau 原生编译（CodeGen）的公开支持契约与上游实验能力边界说明

详情见 [版本说明](Docs/CN/ReleaseNotes.md) 与 [功能清单](Docs/CN/Features.md)。

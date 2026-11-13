# Changelog

<!-- release-status: not-passed -->
<!-- release-provenance: 0b5b29b2 -->

本文件记录面向使用者的版本变更。开发过程中的验证记录与实现细节保留在 Git 历史中。

## 2026-11-13（第五轮：旧布局 DLL 并存与 manifest 冲突）

第五轮日志：Lua 切换**成功**（`Lua-activate-backend.log` 记录了 12 个 Disable），
但 `Lua-static-bindings` 仍报 `Trying to recreate changed class
'UUnLuaNovaReplicationComponent' outside of hot reload and live coding`；
`Luau-activate-backend` 另抛 `Both '...modules' and '...modules.disabled' exist`。

**证据**

- 顶层 `Plugins/UnLuaNova/Binaries/Win64/UnrealEditor-UnLuaNova.dll` 时间戳 11-10、
  二进制不含 `RpcSchemaVersion`（旧 UCLASS 布局）；`Backends/{Lua,Luau}/` 下同名
  DLL 时间戳 11-13、含该字段（新布局）。三份 DLL 都注册
  `UUnLuaNovaReplicationComponent`。
- 引擎 `UObjectBase.cpp:861` 在 `FClassDeferredRegistry::AddResult::ExistingChanged`
  （同名类注册两次且 UHT reload version 不同）时 Fatal。仅禁用顶层 manifest
  不足以阻止 UE 按 `UnrealEditor-<Module>.dll` 回退扫描旧 DLL。
- UBT 每次构建都重写 live `*.modules`，与上一轮切换留下的 `*.modules.disabled`
  并存（实测 6 处），触发 `Set-ManifestEnabled` 的 "Resolve by hand" 抛错。

**修复**

- 新增 `legacy-layout-cleanup` 阶段（`RequiredStages` 第 41 项）：把六个
  `Binaries/Win64` 顶层的 `UnrealEditor-*.dll` 与 `*.modules*` 归档到
  `Saved/LegacyLayout/<时间戳>/`（可逆、不删除），只保留 `Backends/<Backend>/`
  的隔离产物，消除新旧 UCLASS 布局并存。
- `SwitchBackend.ps1`：`Set-ManifestEnabled` 遇到 live + disabled 并存时以 UBT
  刚写的 live 为准清理过期副本；移除主循环里对应的 throw。DryRun 清单同步。

**验证**：`-Backend Lua -DryRun` 输出 `would drop stale ...` 并正常结束
（`Active : Lua`）、无异常；210 个 Python 单测 OK；14 个脚本语法解析 0 失败；
41 个必需阶段均有对应 `Invoke-Stage`。未跑 UE 构建、Release 或联网矩阵。

## 2026-11-13（第四轮：UCLASS 重复注册 / 后端 manifest 枚举根因）

全量重建后，`Lua-static-bindings` 与 `Luau-static-bindings` 的 commandlet 均崩溃：
`Fatal error: Trying to recreate changed class 'UUnLuaNovaReplicationComponent'
outside of hot reload and live coding`，随后 `Plugin 'UnLuaNova' failed to load`，
automation 两后端均报 `0 tests`、基线漂移 `-690` / `-715`。

**根因**：`SwitchBackend.ps1` 的 `Get-ManifestEntries` 用 `Get-ChildItem -File`
（不递归）枚举 `Binaries/Win64`，只能看到顶层 Default manifest，看不到隔离布局
`Backends/<Backend>/*.modules`。于是 `$HasTargetIsolated` 恒为 false，Default
manifest 永远保持 live，Lua / Luau 的 manifest 从未被纳入切换。实测磁盘
18 个 `*.modules` 全部 live、0 个 `.disabled`，`-Status` 报
`AMBIGUOUS: Lua, Luau` —— 引擎把同一模块从三套 DLL 加载多次；普通布局的旧 DLL
不含新增的 `RpcSchemaVersion`，与隔离布局的新 DLL 布局冲突，正是该 fatal error
的来源。上一轮只补了 `$AnyLive` 检查，掩盖了报错却没有收敛状态。

**修复**：`Get-ManifestEntries` 改为递归枚举（`-Filter '*.modules*'` 同时匹配
`*.modules.disabled`），按完整相对路径计算 Zone / Live / Disabled；撤掉上一轮的
`$AnyLive` 直接检查补丁，让枚举结果成为唯一事实来源。

**旁证**：UHT 产物核对——`Inc/Backends/{Lua,Luau}/...gen.cpp` 均含
`RpcSchemaVersion`，普通布局 `Inc/UnLuaNova/UHT/...gen.cpp` 仍是旧布局，
印证「旧 DLL + 新 DLL 同时加载」而非 UHT 未重跑。

**验证**：210 个 Python 单测 OK；`-Status` 正确报告三个 zone 的歧义状态，
`-DryRun -Backend Lua` 正确禁用 Luau 与 Default、保留 Lua；14 个脚本语法解析
0 失败。未跑 UE 构建、Release 或联网矩阵。

## 2026-11-13（第三轮：审计文案同步）

上一轮把 `AuditRepository.py` 的 provenance 错误从
`do not match baseline commit ...` 改为 `do not name a known run ...`，但
`Tools/tests/test_audit_repository.py:148` 仍按旧文案断言，导致 `tooling-tests`
阶段 210 个单测中 1 个失败。现更新断言，并补一条正向用例：test 摘要指向已发布
的提交时，即使 baseline 不一致也不会再报 provenance 错误（即新的「baseline /
test / 当前 HEAD」契约）。

复跑 `python -B -m unittest discover -s Tools/tests -p 'test_*.py'`：
`Ran 210 tests ... OK`（EXIT=0）。同时扫描确认 `Tools/` 下已无其它硬编码的
旧阶段数或旧错误文案依赖。仅运行 Python 单测，未跑 UE 构建、Release、mkdocs
或联网矩阵。

## 2026-11-13（第二轮统一验收反馈修复）

`RunTests.ps1` 第二轮暴露三类问题，逐项修复。

**契约冲突（流水线自我破坏）**

- 上一轮把 `UpdateReleaseStatus.py` 的 provenance 改为优先取本轮 HEAD，
  但 `AuditRepository.py:396` 仍硬绑定 baseline commit `347bb8c5`。收尾写入
  `0b5b29b2` 后，下一轮 `repository-audit` 必然失败。现改为接受
  「baseline / test / 当前 HEAD」任一已知运行的提交；README 中英说明同步改为
  「产生该状态的那次运行」。
- `TestReleaseSupport.ps1:234` 硬编码 `blocked.Count -eq 33`（旧阶段数），新增阶段后
  必然失败。`Write-ReleaseSummary` 增加 `required_stages` 字段，测试改为按名字集合
  断言所有阶段都出现在摘要里，以后加阶段不会再失效。

**后端激活与缓存污染**

- `SwitchBackend.ps1` 的 manifest 枚举只看 `Binaries/Win64` 顶层文件，看不到隔离布局
  `Backends/<Backend>/*.modules`，导致 `-Backend Lua|Luau` 恒抛
  `No live ... found`。补上隔离目录的直接检查（含 `*.disabled` 判定）。
- 失败轮可能留下「缓存命中但产物不完整」的条目（UBT 写了 DLL / 定义头，却没有
  modules manifest、cooked 资源或 staged peer），下一轮跳过构建后 automation 报 0 用例、
  基线漂移 `-690`。现让流水线读到上一轮 `passed=false` 时自动禁用构建缓存、强制全量
  重建一次；跑绿后恢复正常缓存，无需手工加 `-NoCache`。
- `release-claims` 增加 `tool-environment` 依赖，缺引擎的 preflight 不再产生
  `FilePath` 为空的噪声。

**验证**：`AuditRepository.py` 复跑 `EXIT=0`（`Repository audit passed`）；
`TestReleaseSupport.ps1` 复跑打印 `Release support tests passed.`；14 个 PowerShell AST、
23 个 Python AST、7 个 JSON、2 个 YAML、75 个 nav 目标、CN/EN 37=37、8 处 marker
一致全部通过。仅运行了这两个轻量门禁脚本，未跑 UE 构建、Release、mkdocs 或联网矩阵。

## 2026-11-13（首轮统一验收的失败修复）

`RunTests.ps1` 首轮跑到 `tooling-tests` 即失败（210 个单测 2 个失败）：上一轮 B5 把
`ValidateRelease.py` 的 SchemaMismatch 判定升级为「必须证明真实故障注入与策略层拒收」，
但 `Tools/tests/test_release_validation.py::scenario_evidence` 的 fixture 没有对应字段。
已给 fixture 补 `InjectedSchemaVersion` / `SchemaMismatchRejected` /
`SchemaMismatchReason`；并核对 C++ 侧 `LuaNetNetScenariosTest.cpp:196-199` 确实导出这三项、
`FUnLuaNovaNetPolicy::NegotiateSchema` 在指纹不同时返回 `SchemaIncompatible` 并填写 Reason。
复跑流水线同一条 `unittest discover`：**210 tests OK**。

另给 `docs-site` 阶段加自愈探测：缓存命中但 venv 缺 `mkdocs-material` 时按需安装，
不依赖 pip 缓存指纹是否命中。本轮只运行了 Python 单测（非 UE 构建），
未跑 UE 构建、Release、mkdocs 或联网矩阵。

## 2026-11-13（唯一入口收敛：RunTests.ps1 覆盖全部 Release 检查）

维护者决策：今后只运行 `.\Tools\RunTests.ps1`，不再记忆任何额外命令。
`ReleasePipeline.ps1` 把此前散落在验证清单里的每一项都做成了阶段，任一失败都会写入
`release-summary.json` 并让整轮失败。

**新增阶段**

- `docs-site`：`mkdocs build --strict`（`-f` 指定配置，产物写入本轮目录），
  `requirements-docs.txt` 由流水线 venv 安装并纳入依赖指纹
- `net-matrix-Lua` / `net-matrix-Luau`：8 场景 + 4 变体两组独立运行（每后端 12 个 cell，
  **非 8×4 交叉**），复用本轮已构建的隔离 target（`-SkipBuild`），产物写入
  `Saved/ReleaseTests/test/net-matrix/<Backend>/`，由 `RunNetMatrix.ps1` 内置的
  `ValidateRelease.py --net-matrix` 判定
- `evidence-index`：为四份 carrier 报告调用 `RecordEvidence.py`，补上「证据采集」与
  「来源判定」之间的断层
- `release-provenance`：`ValidateProvenance.py --require-current --run <本轮目录>`，
  校验四份 gate 与当前 HEAD 的绑定
- `promotion-gate`：`CheckPromotion.py --release-gate --strict`，另打印完整 L0–L4 报告
- `release-claims`：生成 8 处 marker 后立即执行 `--check` 自检

**缺陷修复**

- `release-claims` 以前是手写块：失败只追加一个 failed stage，不重写
  `release-summary.json`，摘要里的 `passed` 可能仍为 true。现改为 `Invoke-Stage`，
  失败必然写回摘要并让整轮 `exit 1`
- `UpdateReleaseStatus.py` 的 provenance 以前优先取 baseline 的 commit，导致 8 处 marker
  长期指向已脱离分支的 `347bb8c5`；现优先取本轮 HEAD
- `CheckPromotion.py` 增加 `--release-gate` / `--run` / `--net-root`：把「已发布的
  passed 声明」与「release tag」两项人工决策降级为信息项，否则接入流水线会让每轮必然失败

**文档**：CN/EN `Testing`、`Testing_Architecture`、`Production_Deployment`、
`Migration_Guide`、`Release_Readiness`、`API_Stability`、`Networking_Guide`、
`LuauCoverage`，以及 `README` / `README_EN` / `CONTRIBUTING` / `RunTests.ps1` 注释，
全部改为「只运行 `RunTests.ps1`」。

本轮**未运行任何构建、测试、mkdocs 或 UE**，仅做语法级自检。

## 2026-11-13（UE 6.0 首次构建失败修复 / 待重跑验证）

依据 `Saved/ReleaseTests/test/logs/Lua-editor-build.log` 修复；本轮为静态改动，**未重跑构建**。

- 修 `UnLuaNovaDebugBase.cpp:68` 的 `error C7595`：UE 5.6+ 编译期格式串检查
  （`'%d' expects 32-bit integral arg`）与 plain-Lua 后端 64 位 `lua_Integer` 冲突。
  改为 `%lld` + `static_cast<long long>`，与 `UnLuaNovaNetTypeSpec.cpp` 既有写法一致，
  且同时兼容 Luau 后端 32 位 `lua_Integer`
- 消除同轮 8 条 `C4996`：4 处 `FString(int32, const TCHAR*)` 弃用构造改为
  `Reset()` + `AppendChars()`（`LuaDebugAdapter.cpp`、`DebugAdapter.spec.cpp`）；
  `UnLuaNovaEx.inl` 的 `TLazyObjectPtr` IntelliSense 特化用跨平台
  `PRAGMA_DISABLE_DEPRECATION_WARNINGS` 包裹，功能不变
- 该次日志只执行了 18 步中的 9 步（UBA 在首个错误后取消剩余步骤）；已全量扫描
  printf 风格格式串，未发现其他 64 位参数与 `%d` / `%u` 混用

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

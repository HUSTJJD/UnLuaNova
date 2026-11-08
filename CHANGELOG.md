# Changelog

<!-- release-status: passed -->
<!-- release-provenance: 347bb8c5 -->

本文件记录面向使用者的版本变更。开发过程中的验证记录与实现细节保留在 Git 历史中。

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

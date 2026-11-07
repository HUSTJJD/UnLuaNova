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

**文档**

- 中英双语文档站、快速上手、API 与配置参考、调试与测试指南同步至本版本

详情见 [版本说明](Docs/CN/ReleaseNotes.md) 与 [功能清单](Docs/CN/Features.md)。

# 版本说明

<!-- release-status: passed -->
<!-- release-provenance: 4a87d214 -->

## 当前版本

**UnLuaNova v1.0.0**（首个正式版本，2026-11-15）在 **UE 6.0 / Win64** 上通过完整 Release
验证（双后端 41/41 阶段）；UE 5.8.3 / Win64 保留为历史 baseline。采用 **MIT 许可**，
可免费用于个人与商业项目。

## 本版本主要功能

- **声明式网络已转正（Stable）**：RPC 与带条件的复制字段在 Legacy / PushOff / Iris / Cooked
  四种复制载体、八个连接场景、Lua 与 Luau 两个后端上通过真实多进程发布验证；
  完整 Release `passed=true`（41/41 阶段）。详见 [发布就绪度](Release_Readiness.md)。
- **双 Lua 后端**：Lua 5.5.1 与 Luau 二选一，在 `Config/DefaultUnLuaNova.ini` 中切换（需重新编译）。
- **反射与覆写**：UE 反射类型双向调用、属性访问、容器与委托，以及 Lua 对蓝图事件 / UFunction 的覆写与 `self.Overridden` 原实现调用。
- **静态导出绑定**：模板导出与构建期生成的绑定代码，配合反射回退路径。
- **运行时库**：随插件分发的 Timer、Event、OOP、HotReload、StringEx、Vector、Buffer、Input 等模块，统一通过 `UnLuaNova.HasFeature` 查询。
- **开发与诊断工具**：Profiler、DAP 调试适配器、死循环看门狗、内存与协程诊断、智能提示声明导出。
- **声明式网络**：Lua 侧声明 RPC 与带条件的复制字段，见 [网络](LuaNetworking.md)。
- **一条命令验收**：在仓库根目录运行 `.\Tools\RunTests.ps1` 即可执行完整 Release 验证，见 [测试](Testing.md)。

功能清单与逐项边界见 [功能清单](Features.md)。

## 验证环境

v1.0.0 在 UE 6.0 / Win64（源码引擎）上验证，使用项目关联的 UE、Visual Studio C++ 工具与 Python 3.10 及以上版本。默认 `Release` 配置覆盖双后端的构建、Editor 与 cooked 测试、静态类型检查、性能门禁、Server 构建与 Shipping 打包。

## 已知边界

- **平台**：v1.0.0 面向 Win64；其他平台需单独验收。
- **声明式网络已转正**：Legacy / PushOff / Iris / Cooked 四种载体与八个连接场景，在 Lua / Luau 双后端通过完整发布验证；不宣称压缩收益或运行时回退。不支持：引擎级 seamless travel（`ServerTravel`）、声明式网络热重载、RPC 鉴权（`bRequireRpcAuthorization` 默认 `False`）。
- **Luau 原生编译**：CodeGen 需显式开启（`lua.codegen.enabled`），且目标架构与可执行内存策略有限制。
- **沙盒**：默认关闭（`lua.sandbox=0`）；开启后也只是 VM 级可选硬化，不等同于 OS 级沙盒或不可信脚本的完整隔离。
- **类型检查**：由外部 LuaLS / luau-lsp 完成，不发生在游戏运行时。

验收方法与门禁见 [测试](Testing.md)。

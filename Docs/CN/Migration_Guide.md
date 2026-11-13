---
title: 从 UnLua 迁移到 UnLuaNova
description: UnLua 2.3.6 迁移清单：全局名、配置分层、绑定方式、后端选择、静态绑定与行为差异。
---

# 从 UnLua 迁移到 UnLuaNova

README 里那句「把全局名 `UnLua` 批量换成 `UnLuaNova`」只覆盖第一步。本页是完整清单。
上游发布线停在 **UnLua v2.3.6（2023-11）**，UnLuaNova 是社区续作，不是 drop-in 替换。

## 1. 全局名与模块路径

| UnLua | UnLuaNova |
|---|---|
| `UnLua.Class()` | `UnLuaNova.Class()` |
| `UnLua.Log*` / `UnLua.*` 全局 | `UnLuaNova.*` |
| `Content/Script/<模块>.lua` | 不变 |
| `require("UnLua.*")` | `require("UnLuaNova.*")` |

`Config/DefaultEngine.ini` 里已保留 `UnLua → UnLuaNova` 的 `PackageRedirects` / `ClassRedirects`
（第 12–23 行），用于旧资产升级；新工程不需要这些重定向。

## 2. 配置文件：只有一份

UnLuaNova 的**默认配置**在 `Plugins/UnLuaNova/Config/DefaultUnLuaNova.ini`；
工程侧 `Config/DefaultUnLuaNova.ini` **只写覆盖项**，不要整份复制。复制后插件升级改默认值时，
工程那份会停在旧值且看不出来。

两个 section 分工：

| Section | 内容 |
|---|---|
| `[/Script/UnLuaNova.UnLuaNovaSettings]` | 运行时 + 特性开关（两个后端都要） |
| `[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]` | 编辑器行为 + 构建配置（仅编辑器） |

`LuaBackend` 虽写在 Editor section 下，但被 Game / Server target 的 `Build.cs` 读取，属于编译期开关。

## 3. 绑定方式（最大的行为差异）

| | UnLua | UnLuaNova |
|---|---|---|
| 必须实现 `IUnLuaInterface` / `GetModuleName` | 是 | **否** |
| 数据驱动绑定 | 无 | ini 的 `+ClassModuleBindings` 或绑定组件，默认关闭（`bDataDrivenBinding=False`） |
| 实例级绑定 | 无 | `BindInstance` / `UnbindInstance`（类级绑定仍是默认） |

`bDataDrivenBinding` 默认关是**安全边界**：开启后凡是匹配映射表或绑定组件的对象，都会在它
不知情的情况下被绑上脚本。开启前先读 [实例级模块绑定](InstanceBinding.md)。

## 4. 后端选择

UnLuaNova 有 **Lua 5.5.1** 与 **Luau 0.743-1** 两个编译期后端，运行时不共存。
默认 `LuaBackend=Luau`。切换要重新编译；示例工程提供 6 个隔离 target，各编一次后切后端零编译。
能力差异（沙盒、原生 codegen、覆盖率、只读表）见 [双后端能力矩阵](Backend_Capability_Matrix.md)。

从 UnLua（Lua 5.3/5.4 语义）迁到 Luau 时需注意：

- 整数/位运算、`goto`、5.4/5.5 `close` 语义不承诺同语言兼容；
- `bit32` 不等于整数操作符语义；
- `debug.getregistry` 显式拒绝，`getuservalue` 是 nil-only 子集，`setuservalue` 返回 false；
- `load()` 只接受源码（`'t'` / `'bt'`），纯 `'b'` 与未知模式被拒绝；
- `io` 是可选的 rooted buffered 子集，`io.popen` 明确不支持。

## 5. 静态绑定

UnLuaNova 默认**全导出**（opt-out），构建期为导出类型生成 marshal thunk，运行时不再做
反射查找与参数编组。不想要的类型加 `meta=(UnLuaNovaNoExport)`。
生成源码按后端分目录落在 `Source/UnLuaNova/Generated/Backends/<后端>/`，已被 `.gitignore` 忽略，
不要提交。见 [静态导出绑定](StaticExportBinding.md)。

## 6. 网络声明（新增能力）

UnLua 的 `ULuaFunction::IsOverridable()` 明确排除带 `FUNC_Net` 的函数，脚本只能重写引擎已声明
的 RPC。UnLuaNova 允许脚本自己声明 Server/Client/Multicast RPC 与复制字段 —— 见
[Lua 声明式网络](LuaNetworking.md) 与 [正式教程](Networking_Guide.md)。
**该能力当前处于待晋升状态**，生产使用前先读 [发布就绪度](Release_Readiness.md)。

## 7. 验证方式

| UnLua | UnLuaNova |
|---|---|
| 手工跑用例 | `.\Tools\RunTests.ps1`（零参数，完整 Release） |
| 无证据链 | `Tools/evidence/index.json` + `release-summary.json` + 8 个页面的生成式 marker |
| 无来源校验 | `Tools/ValidateProvenance.py --require-current` |

`RunTests.ps1` 不接受参数；局部筛选、载体、分层、`-BuildOnly` 与联网矩阵属于
`Tools/RunDevelopmentTests.ps1`。UE 6.0 冷 cook 可用 `UNLUANOVA_RELEASE_TIMEOUT_MINUTES` 调整超时。

## 8. 迁移后自检

```powershell
python -B Tools/AuditRepository.py
python -B Tools/CheckPromotion.py
python -B -m unittest discover -s Tools/tests -p "test_*.py"
```

这三条都不启动 UE，只验证仓库一致性。真正的迁移验收仍需跑一次完整 Release。

## 延伸阅读

[功能清单](Features.md) · [设置项](Settings.md) · [API 稳定性与版本策略](API_Stability.md) · [常见问题](FAQ.md)

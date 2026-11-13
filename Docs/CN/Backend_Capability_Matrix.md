---
title: 双后端能力矩阵
description: Lua 5.5.1 与 Luau 后端的能力差异、plain-Lua 侧显式降级清单，以及新增能力时的验收要求。
---

# 双后端能力矩阵

UnLuaNova 在**编译期**选择后端，两个 VM 不会在运行时共存。绝大多数能力由 Luau 提供，
plain Lua 5.5.1 走能力宏关闭后的降级路径。本页记录这些差异，避免把「能力边界」误读成
「未实现」。

## 唯一权威来源

`Plugins/UnLuaNova/Source/UnLuaNova/Public/LuaBackend.h` 中的 `UNLUANOVA_HAS_*` 宏是能力声明的
唯一权威来源，共 12 项：`HOOK_EVENT`、`NATIVE_VECTOR`、`NATIVE_BUFFER`、`READONLY_TABLES`、
`SANDBOX`、`BREAKPOINT`、`ALLOC_CALLBACKS`、`NATIVE_CODEGEN`、`THREAD_CALLBACK`、
`GC_METAMETHOD`、`USERDATA_FINALIZER`、`ITER_METAMETHOD`。大项呈 Luau=1 / Lua=0 分布。

该头文件还带三重 `#error` 保护和 `lua_Integer` 宽度的 `static_assert`；后端宏配置错误会在
编译期失败，而不是运行期出现难查的行为差异。**不要在业务代码里自己推断后端**，一律使用
能力宏分支。

## plain-Lua 侧的显式降级

`Private/LuaBackend.cpp` 的 `// plain-Lua backend` 分支有以下 6 处空实现 / 固定返回值。
它们是设计上的能力边界，不是遗漏的 TODO：

| 位置 | 形式 | 影响 |
|---|---|---|
| `InstallSandboxThreadHardening` | 空实现 | 沙盒线程加固仅 Luau 可用 |
| `IsNativeCodeGenAvailable` | `return false` | 原生 codegen 仅 Luau + x64/arm64 |
| `MaybeCompileNativeChunk` | 空实现 | 同上 |
| `Get*Count` × 2 | `return 0` | codegen 统计仅 Luau 有意义 |
| `IsClassTableFreezeEnabled` | `return false` | 只读类表仅 Luau 可用 |

对应的 Luau 侧在同一文件有完整实现。调用方应通过能力宏判断，而不是检查返回值是否为 0。

## 相关行为差异

| 能力 | Lua 5.5.1 | Luau |
|---|---|---|
| 静态类型检查器 | `lua-language-server`（Lua 5.5 模式） | `luau-lsp analyze -Mode strict` |
| 覆盖率导出 `lua.coverage` | 不支持，命令明确报错 | 支持，出口见 [Luau 覆盖率](LuauCoverage.md) |
| 原生 codegen（`lua.codegen.enabled`） | 不适用 | opt-in，默认 0，仅对切换后加载的 chunk 生效 |
| 沙盒 `lua.sandbox` | 路径检查外的加固不可用 | 完整 |

## 新增能力的要求

1. 在 `LuaBackend.h` 暴露 `UNLUANOVA_HAS_*` 宏，**两个后端都要定义**，不能只写 Luau 分支。
2. 在 `LuaBackend.spec.cpp` 补用例；涉及行为的改动两个后端都要验证。
3. 若 plain-Lua 侧只能降级，在 `LuaBackend.cpp` 对应实现处写明「这是能力边界」，并登记到本页表格。
4. 更新 `Docs/EN/Backend_Capability_Matrix.md`，保持双语同步。

## 延伸阅读

[Luau 后端](Luau.md) · [Luau 覆盖率导出与门禁](LuauCoverage.md) · [引擎版本与平台支持矩阵](Engine_Support.md)

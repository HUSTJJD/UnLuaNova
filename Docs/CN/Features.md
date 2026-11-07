# 功能清单

本页按领域列出当前版本包含的功能与边界。

| 领域 | 当前实现 | 主要边界 |
|---|---|---|
| UE 集成 | 反射调用、属性、覆写、容器、委托、Latent | 生命周期、继承与多环境存在已知复杂性 |
| 绑定 | 手写模板导出、生成 thunk、反射回退 | thunk 仍调用 ProcessEvent；并非所有签名都生成 |
| 后端 | Lua 5.5.1 与 Luau | 编译期选择；Lua 5.x 语义不完全等价 |
| Luau | 语法、标准库、vector、buffer、原生调试、可选 CodeGen | 类型检查在外部；实验库受上游 flag 控制 |
| 开发工具 | DAP、Profiler、内存/协程诊断、声明导出 | CI 与打包场景需单独接入 |
| 运行时模块 | Event、OOP、Timer、HotReload | Timer 生命周期和 HotReload upvalue 语义有已知边界 |
| 网络 | RPC、按类型分桶的复制属性、按条件分组的条件复制、`UStruct` / 容器类型规格 | 实验特性；8 种复制条件各有独立存储组，不支持的条件显式拒绝；多 Env 按共同 owner 释放。真实联机与打包变体需单独验收，详见[网络](LuaNetworking.md) |

[Luau](Luau.md) · [内置库](StdLib.md) · [网络](LuaNetworking.md)

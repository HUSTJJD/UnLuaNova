# 功能清单

[能力与证据矩阵](../feature-matrix.md) 是唯一逐项状态表。状态描述实现范围；测试文件存在不等于当前构建通过。

| 领域 | 当前实现 | 主要边界 |
|---|---|---|
| UE 集成 | 反射调用、属性、覆写、容器、委托、Latent | 生命周期、继承、多环境需要系统验收 |
| 绑定 | 手写模板导出、生成 thunk、反射回退 | thunk 仍调用 ProcessEvent；并非所有签名都生成 |
| 后端 | Lua 5.5.1 与固定提交的 Luau | 编译期选择；Lua 5.x 语义不完全等价 |
| Luau | 语法、标准库、vector、buffer、原生调试、可选 CodeGen | 类型检查在外部；实验库受上游 flag 控制 |
| 开发工具 | DAP、Profiler、内存/协程诊断、声明导出 | CI 与打包场景的证据尚不完整 |
| 运行时模块 | Event、OOP、Timer、HotReload | Timer 生命周期和 HotReload upvalue 语义有缺口 |
| 网络 | RPC、按类型分桶的复制属性、按条件分组的条件复制、`UStruct` / 容器类型规格 | 实验阶段；8 种条件已有独立存储组与单测，不支持的条件显式拒绝；复制声明已按 Env 归属；owner/非 owner 联机验收、晚加入/进程级重连、dormancy/relevancy 四类场景与 Legacy/PushOff/Iris 变体均为**源码与门禁已补、尚未执行** |

[当前风险与验收](Benchmark_Review.md) · [Luau](Luau.md) · [内置库](StdLib.md)

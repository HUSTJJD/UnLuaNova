# 架构

本页说明当前源码职责和调用路径，验收状态见 [能力矩阵](../feature-matrix.md)。

## 模块边界

| 路径（仓库根起） | 职责 |
|---|---|
| Plugins/UnLuaNova/Source/UnLuaNova | 游戏运行时：Env、绑定、覆写、库、诊断与实验网络 |
| Plugins/UnLuaNova/Source/UnLuaNovaEditor | 编辑器工具、声明/绑定生成、DAP 适配器 |
| Plugins/UnLuaNova/Source/UnLuaNovaDefaultParamCollector | UBT 默认参数收集插件 |
| Plugins/UnLuaNova/Source/ThirdParty | Lua / Luau 子模块、UBT wrapper |
| Plugins/UnLuaNova/Content/Script/UnLuaNova | 随插件分发的 Lua 模块 |
| Plugins/UnLuaNovaExtensions | Socket、Protobuf、RapidJSON 独立插件 |
| Plugins/UnLuaNovaTestSuite | C++ Spec、场景、视口、网络与性能测试 |
| Content/Script | 示例工程脚本及 Lua 侧 Spec |
| Tools | 测试编排、报告、类型检查和仓库验证 |

运行时的 `ReflectionUtils/` 描述 UE 反射类型，`Registries/` 管理对象/类/函数/属性/委托/容器映射，`BaseLib/` 和 `MathLib/` 提供 Lua API。VM 由 UBT 从固定版本源码构建。

## 后端适配

`Public/LuaBackend.h` 与 `Private/LuaBackend.cpp` 集中处理能力宏、编译、GC 和 CodeGen；`LuaScriptCompat.cpp` 提供 Luau 的 Lua API 子集适配。不能仅凭宏存在判定语义完整。

| 能力 | Lua 5.5 | Luau |
|---|---|---|
| UNLUANOVA_HAS_GC_METAMETHOD | 1 | 0 |
| UNLUANOVA_HAS_USERDATA_FINALIZER | 1（__gc） | 1（VM dtor） |
| 原生 vector / buffer | 无 | 有 |
| 只读表 / sandbox / 原生断点 | 无同名原语 | 有 |
| call/return hook | 有 | 使用 interrupt/debug 原语与采样 |
| 整数 API | 64 位 | 32 位；64 位值需要专用表示 |

资源分配必须与对应终结路径配对；Luau dtor 不得访问 Lua 栈。兼容函数中存在近似/占位，详见 [Luau](Luau.md)。

## 绑定和调用

1. Env 定位器按对象确定环境；`FLuaEnv::TryBind` 经 `UnLuaNovaManager` 加载模块。
2. `BindClass` 以 UClass 缓存模块与覆写信息，`ObjectRegistry::Bind` 登记实例。当前不能推导同类不同实例模块相互独立。
3. 字段访问经 LuaCore 和属性描述器；可写类表缓存查找结果，冻结类表跳过写缓存。
4. 覆写经 `FLuaOverrides` / `ULuaFunction` 替换 thunk 与脚本入口；原实现通过 `self.Overridden` 访问。
5. 引擎到 Lua 使用函数查找、参数编组和错误处理；Latent 还涉及协程引用、yield 边界和恢复。
6. 普通调用、CallUEFast、模板导出、生成 thunk 是不同路径。生成 thunk 仍调用 ProcessEvent，复杂签名回退；行为等价需要测试证明。

## 所有权与调度

Env 拥有 VM、注册表和诊断状态；UE 拥有 UObject 与 World。Lua 持有对象、借用结构体或容器时必须分别定义保活与失效规则。委托、Timer、Latent 都可能让回调比发起者活得更久。

当前容器、FText、非平凡导出构造、手动引用代理和已知智能指针分配点已接 VM dtor；Timer 使用 Env 主状态、显式 World 和销毁清理。新增生命周期用例尚未运行，完整分配路径审查仍未结束，不能据此宣称 GC 全覆盖。

GC 默认由 VM 自动调度；TimeSliced 模式通过每帧预算发出 GC step。预算不是硬性最长停顿保证，不能等同多线程 GC，也不能保证相同延迟收益。必须测量最坏帧、分配压力与销毁成本。

## 工具和实验网络

编辑器生成绑定与类型声明，外部 LuaLS/luau-lsp 做静态分析，运行时并不执行类型检查。调试服务与 DAP 分属运行时/编辑器模块；打包目标不自动具备完整 IDE 链路。

网络注册动态 UFunction，并用固定 typed UPROPERTY 桶承载字段。目前条件复制依赖实例而布局按类缓存，不能视作成熟网络 schema 系统。详见 [网络](LuaNetworking.md)。

## 修改要求

新增能力应同时明确后端差异、错误契约、所有权、开关与目标平台；补充对应测试与示例，并更新能力矩阵。生成路径与反射回退应做同输入对照。禁止将源码分支、未执行用例或某次旧截图视作当前验收证据。

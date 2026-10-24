# Luau 后端

当前集成 `6d36d483cb88973276af611198169f76f3ce05b0`（0.743 + 1），通过子模块固定；上游对照为 0.744，未在本轮升级。后端在 `Config/DefaultUnLuaNova.ini` 选择，切换需要编译。源代码与验收入口见 [能力矩阵](../feature-matrix.md)。

## 支持范围

| 层 | 当前接入 | 边界 |
|---|---|---|
| 语言与编译器 | 类型注解语法、continue、复合赋值、if 表达式、字符串插值、泛型等由固定版本 Compiler 处理 | 语法可编译不代表类型已检查；应导入对应版本 conformance 用例，维护差异清单 |
| VM 标准库 | base、coroutine、table、string、math、os 子集、debug 子集、bit32、utf8、vector、buffer | 以 `VM/src/linit.cpp` 注册为准；integer / 用户类等上游实验能力受 feature flag 控制，不作稳定承诺 |
| UE 数值/容器 | vector ↔ FVector；buffer ↔ TArray 的 C++ 拷贝路径 | vector 为 float，UE5 FVector 为 double；buffer 不借用 UE 外部内存，返回数组类型不保证是 uint8 |
| 编译配置 | optimize、debuglevel、coverage | coverage 编译插桩存在，导出/汇总/门禁尚未闭环 |
| 原生编译 | CodeGen 显式开启，可按模块筛选 | 目标架构与可执行内存策略有限制；计数是请求数，不等于成功函数数；需验证断点协作 |
| 调试/诊断 | 原生断点/单步、Profiler、内存分类、协程统计、GC 调节 | 分类统计是分配 churn，不等于分类存活字节；远程/打包验收未完成 |
| 类型工具 | 编辑器导出声明、外部 luau-lsp | Analysis/Config 不作为游戏运行时类型检查器；声明完整性与跳过行为必须验收 |
| 模块加载 | UnLuaNova 自有 loader 与 package.searchers/preload | 不等同于上游 Require 的相对路径/别名/配置解析；path/cpath 不是自动加载策略 |

上游 [兼容说明](https://luau.org/compatibility/) 与 [沙盒说明](https://luau.org/sandbox/) 是语义边界的来源；本插件补充 API 不改变 Luau 的全部语言语义。

## 与原生 Lua 的差异

| 能力 | 当前处理与缺口 |
|---|---|
| Lua 5.3+ 整数、位运算语法、goto、5.4/5.5 close 语义 | 不承诺同语言兼容；bit32 不等于整数/操作符语义；资源需显式 Close/Destroy |
| int64 | 普通绑定有专用转换；网络路径仍有 int32 截断风险，不能声称全链路精确 |
| userdata 终结 | `UNLUANOVA_HAS_GC_METAMETHOD=0`，`UNLUANOVA_HAS_USERDATA_FINALIZER=1`；容器、FText、非平凡导出构造走 VM dtor，手工对象代理仍需修复 |
| debug/upvalue | getregistry、get/setuservalue 等存在占位，upvalueid 近似；热重载共享标量 upvalue 需专门验证 |
| load/字节码 | 宿主提供源代码编译；load mode 与后端字节码兼容未完整保证；只加载可信且匹配版本的字节码 |
| io / os | 可选的缓冲文件 io，不提供完整 Lua io/进程能力；模式、seek、错误约定、链接逃逸、容量限制仍需完善 |
| package | 提供 preload/searchers 等子集；不提供任意原生动态库加载保证 |
| yield / coroutine | 跨 C/元方法边界受 VM 限制；宿主回调、Latent、Timer 需分别验证 |
| GC | 增量 GC 与预算调度；不是 Lua 分代 GC，也不是多线程 GC |
| 迭代 | Luau 使用 __iter；与 __pairs / __ipairs 及容器增删期间行为分别测试 |

VM dtor 只能销毁 C++ 状态，不能操作 Lua 栈。使用 LONGJMP 的路径还需要检查 Lua error/yield 是否跳过 C++ 临时对象清理。安全相关阻塞见 [审计](Benchmark_Review.md)。

## 配置与安全

| CVar | 默认 | 生效范围 |
|---|---|---|
| `luau.optimize` / `luau.debuglevel` / `luau.coverage` | 1 / 1 / 0 | 后续编译 chunk |
| `lua.codegen` / `lua.codegen.modules` | 0 / 空 | 后续加载模块；空筛选为全部 |
| `lua.gcgoal` / `lua.gcstepmul` / `lua.gcstepsize` | 200 / 200 / 1 KB | 环境初始化 |
| `lua.sandbox` / `lua.sandboxthread` | 0 / 0 | 环境/线程初始化 |
| `lua.sandbox.freezeclasstables` | 0 | 类注册 |
| `lua.io` / `lua.ioroot` | 0 / 项目目录 | 环境初始化 / 路径解析 |

沙盒与类表冻结是可选硬化，不是对不可信脚本的完整隔离。UE 对象访问、debug、socket 和 io 是额外宿主能力；必须明确授权范围。文件路径校验不抵御 OS 符号链接/重解析点。Lua 5.5 自带 io 的策略也不由 Luau 的开关控制。

## 类型、示例与剖析

导出智能提示产生 `Plugins/UnLuaNova/IntelliSense/` 下的后端声明。用 `Tools/CheckLuauTypes.ps1 -Require` 明确要求工具存在；发布验收还必须检查报告是否跳过，不能只读退出码。本轮没有构建 LSP 或执行类型检查。

```lua
local Examples = require("Luau.Index")
if Examples.Available then
    Examples.VectorInterop:Run()
    Examples.BufferInterop:Run()
end
```

示例目录包含严格类型、沙盒边界、调试目标、语法、vector 和 buffer。守卫入口可在 Lua 后端加载，Luau 专有语法文件不能绕过守卫加载。尚缺完整的热重载、原生编译、io 错误路径、打包远程调试和跨设备示例验收。

`lua.profiler trace on` / `trace dump [path]` 产生 profile.out。转换入口调用固定 Luau 子模块内的官方工具及其 svg 依赖：

```powershell
python Tools/LuauPerfgraph/perfgraph.py Saved/Profiling/profile.out > profile.svg
```

不支持 `--width`。本轮新增 io/CodeGen/buffer 回归源码，均需维护者编译后执行，详见 [测试](Testing.md)。

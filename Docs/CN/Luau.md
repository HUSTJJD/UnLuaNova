# Luau 后端

后端集成 Luau（0.743 + 1），通过子模块固定。后端在 `Config/DefaultUnLuaNova.ini` 选择，切换需要编译。

## 支持范围

| 层 | 当前接入 | 边界 |
|---|---|---|
| 语言与编译器 | 类型注解语法、continue、复合赋值、if 表达式、字符串插值、泛型等由固定版本 Compiler 处理 | 语法可编译不代表类型已检查；类型检查由外部工具完成 |
| VM 标准库 | base、coroutine、table、string、math、os 子集、debug 子集、bit32、utf8、vector、buffer | 以 `VM/src/linit.cpp` 注册为准；integer / 用户类等上游实验能力受 feature flag 控制，不作稳定承诺 |
| UE 数值/容器 | vector ↔ FVector；buffer ↔ TArray 的 C++ 拷贝路径 | vector 为 float，UE5 FVector 为 double；buffer 不借用 UE 外部内存，返回数组类型不保证是 uint8 |
| 编译配置 | optimize、debuglevel、coverage | coverage 编译插桩存在；导出格式与门禁见 [覆盖率导出](LuauCoverage.md) |
| 原生编译 | CodeGen 显式开启，可按模块筛选 | 目标架构与可执行内存策略有限制；计数是请求数，不等于成功函数数 |
| 调试/诊断 | 原生断点/单步、Profiler、内存分类、协程统计、GC 调节 | 分类统计是分配 churn，不等于分类存活字节；远程调试与打包场景需单独接入 |
| 类型工具 | 编辑器导出声明、外部 luau-lsp | Analysis/Config 不作为游戏运行时类型检查器；声明完整性由使用方自行核对 |
| 模块加载 | 自有 loader、package.searchers/preload/aliases，双后端统一的最终 `require` 包装（循环检测），`unrequire` / `reloadmodule` 与 `package.resolution()` | 上游 `@alias` 配置文件（`.luaurc`）明确拒绝；大小写敏感、无自动大小写回退；cooked 构建需要散脚本 staged 到工程根目录；path/cpath 不是自动加载策略 |

上游 [兼容说明](https://luau.org/compatibility/) 与 [沙盒说明](https://luau.org/sandbox/) 是语义边界的来源；本插件补充 API 不改变 Luau 的全部语言语义。

### 原生编译（CodeGen）支持契约

`lua.codegen.enabled` 默认 0，是一个**默认关闭的可选优化**，不是正确性依赖：不开启时所有 chunk 走解释执行，功能集完全一致。契约如下，超出部分明确不在保证范围内。

| 项 | 契约 |
|---|---|
| 后端 / 架构 | 仅 Luau 后端；运行期由上游 `luau_codegen_supported()` 判定（x64 / arm64）。其他目标 `UNLUANOVA_HAS_NATIVE_CODEGEN=0`，开关被忽略并打印告警，不会静默变成别的执行路径。已做运行验收的平台是 UE 5.8.3 / Win64 x64；arm64 仅编译期支持，未在本仓库做运行验收 |
| 生效范围 | 只影响开关之后加载的 chunk；`lua.codegen.modules` 可按模块名子串收窄；`lua.codegen on/off` 与 CVar 是同一个状态 |
| 可执行内存 | 上游 Luau 自行申请可执行代码页。宿主不接管 W^X / 代码页回收策略，也不保证在启用 CodeGen 时可附加原生调试器或单步进入已编译函数 |
| 失败语义 | 上游 `luau_codegen_compile()` 返回 void，没有逐 chunk 成功信号；无法编译的 chunk 继续解释执行。`lua.codegen` 输出的计数是**请求数**（`GetNativeCodeGenRequestCount()`），不是成功编译的函数数；旧 getter 保留为文档化别名 |
| 回归 | `LuauIntegration.spec.cpp` 覆盖 VM / 协程生命周期与命令 / CVar 共存；CodeGen 可执行页不纳入 `UnLuaNova.Memory` 的逐块释放断言 |

因此它是**已交付、带边界的可选优化**：这些限制是公开的契约，不是待补齐的实验缺口；但如需在 arm64、打包 Shipping 或原生调试场景下依赖它，仍需针对该目标单独验收。

### 上游实验能力边界

Luau 上游的 integer / 用户类等库由上游 feature flag 控制，语义、ABI 与类型声明都可能随上游版本变化。本仓库的 `Luau.Build.cs` **不启用这些 flag**，它们不出现在 UnLuaNova 的公开 API、生成声明或测试契约里。换句话说，它们**不是本插件交付的能力**，而是固定上游子模块内部的可选开关；自行开启即超出支持范围，需自行承担上游变更与 ABI 兼容风险。本插件自身不提供任何实验性能力。

## 与原生 Lua 的差异

| 能力 | 当前处理与边界 |
|---|---|
| Lua 5.3+ 整数、位运算语法、goto、5.4/5.5 close 语义 | 不承诺同语言兼容；bit32 不等于整数/操作符语义；资源需显式 Close/Destroy |
| int64 | 复制桶已接严格 int64 读写。超出 ±2^53 的精确值使用原生 integer64，不能恢复 number 已丢失的精度 |
| userdata 终结 | `UNLUANOVA_HAS_GC_METAMETHOD=0`，`UNLUANOVA_HAS_USERDATA_FINALIZER=1`；容器、FText、导出构造、手动代理及已知智能指针路径走 native dtor |
| debug/upvalue | 精确子集：`getregistry` 明确拒绝，`getuservalue` 恒 nil，`setuservalue` 返回 false，`upvalueid` 是值身份（不能证明标量别名）；不承诺 Lua 5.x debug 全兼容 |
| load/字节码 | 宿主只编译源码：`load` 的 mode 现在显式校验，纯 `b`、未知字母与空模式都明确失败，`t`（含 `bt`）走源码编译并以 `"t"` 交给加载器；不接受字节码输入，字节码 / 版本兼容不承诺 |
| io / os | 可选的 rooted 缓冲文件 io：模式 / seek / lines 格式校验、`lua.iomaxbytes` 配额、按 `__pos` 覆盖写、`io.tmpfile()` 内存句柄；`io.popen` 明确拒绝，链接 / 进程 / 原生流不在能力集内 |
| package | 提供 preload / searchers / aliases 子集与双后端一致的 require 缓存；**最终 `_G.require` 被统一包装**（在 HotReload 替换 `require` 之后安装），因此 Luau 自有 require、plain Lua 原生 require、`FLuaEnv::Require` 与热重载后的替换版本都有循环依赖检测，明确报 `cyclic dependency involving 'X'` 且失败后清理标记、不毒化后续 require；别名自环同样拒绝；`unrequire` / `reloadmodule` 提供显式缓存契约；不提供任意原生动态库加载保证；上游 Require 的 config / `@alias` 语义明确拒绝而不是静默降级 |
| yield / coroutine | 跨 C/元方法边界受 VM 限制；宿主回调、Latent、Timer 各有独立边界 |
| GC | 增量 GC 与预算调度；不是 Lua 分代 GC，也不是多线程 GC |
| 迭代 | Luau 使用 __iter；与 __pairs / __ipairs 及容器增删期间的行为存在差异 |

VM dtor 只能销毁 C++ 状态，不能操作 Lua 栈。使用 LONGJMP 的路径需要留意 Lua error/yield 是否跳过 C++ 临时对象清理。

## 模块加载与卸载契约

宿主在 `UnLuaNovaLib::Open()` 之后包装**最终**的 `_G.require`，而不是包装某个 searcher 返回的
chunk：这样既不改变 `package.searchers` 的签名，也不会破坏 `HotReload.lua` 对 chunk 做的
`setfenv` / `_ENV` 替换，同时覆盖两个后端的原生 `require`、`FLuaEnv::Require` 和 HotReload
安装后的替换版本。包装器用一张 `loading` 表记录正在加载的模块名，重入即报
`require: cyclic dependency involving 'X'`，成功与失败都会清除标记。

| API | 契约 |
|---|---|
| `require(name)` | 大小写敏感，点分名一对一映射为路径；不做自动大小写回退。5 个宿主 searcher 顺序：aliases → preload → 自定义 loader → 文件系统 → 内建模块 |
| `package.aliases[name] = target` | 运行时重定向表，别名与目标共享同一缓存实例；自引用直接报错，任意长度的环由最终 `require` 包装的 `loading` 表检测 |
| `unrequire(name)` | 清 `package.loaded[name]` 与 HotReload 的 `loaded_modules` / `loaded_module_times` / `ignore_modules` 三个私有缓存，返回清理计数。**保留** `package.preload[name]` 与 `package.aliases[name]`（它们是加载器/重定向声明，不是缓存），否则 `reloadmodule` 会重新加载失败 |
| `reloadmodule(name)` | `unrequire(name)` 后再走当前 `require(name)`，拿到新实例；不改变模块对象身份策略 |
| `package.resolution()` | 返回机器可读契约：`case_sensitive=true`、`automatic_case_fallback=false`、`alias_config_files=false`、`runtime_alias_table=true`、`cycle_detection=true`、`explicit_unrequire=true`、`cooked_build`（按构建）、`unrequire_scope="exact-name"`、`cleared_caches` |

以 `@` 开头的模块名（上游 Require 的 `@alias` / `.luaurc` 配置文件语义）现在会**明确报错**
并指向 `package.aliases` 与 `package.resolution()`，不再退化成一句含糊的 "module not found"。
cooked 构建只有把散 `.lua` 文件 staged 到工程根目录或 `ProjectPersistentDownloadDir` 才能解析；
`lua.requireresolution 1` 可打开文件系统 searcher 未命中时的诊断日志（默认关，因为 searcher
正常让位是常态）。

`HotReload.lua` 的 `reload()` / `reload_modules()` 改为两阶段提交：先只加载并计算新模块，
再用 `xpcall` 提交，失败时按快照回滚旧模块顶层键、`package.loaded` 与两个私有缓存；返回
`ok, err`，`module_loaded` 钩子只在提交成功后触发，文件时间戳也只在成功后推进，所以修好源码后
可以直接重试。**这是 best-effort 回滚，不是事务**：`_G`、运行栈局部变量、任意
table/upvalue/registry 与已被别处持有的引用都不做全量快照。

## 配置与安全

| CVar | 默认 | 生效范围 |
|---|---|---|
| `luau.optimize` / `luau.debuglevel` / `luau.coverage` | 1 / 1 / 0 | 后续编译 chunk |
| `lua.codegen.enabled` / `lua.codegen.modules` | 0 / 空 | 后续加载模块；空筛选为全部；`lua.codegen on/off` 命令控制开关 |
| `lua.gcgoal` / `lua.gcstepmul` / `lua.gcstepsize` | 200 / 200 / 1 KB | 环境初始化 |
| `lua.sandbox` / `lua.sandboxthread` | 0 / 0 | 环境/线程初始化 |
| `lua.sandbox.freezeclasstables` | 0 | 类注册 |
| `lua.io` / `lua.ioroot` | 0 / 项目目录 | 环境初始化 / 路径解析 |
| `lua.maxsafepoints` | 0 | 每次宿主调用的安全点预算（Luau interrupt / plain Lua 指令钩子） |
| `lua.maxmemorykb` / `lua.memquota` | 0（关） | 新 Env 的 Lua 堆字节配额；命令可在运行时改所有 live Env，单位 KiB |
| `lua.requireresolution` | 0 | 文件系统 searcher 未命中时的诊断日志 |

`lua.maxmemorykb` 通过默认 allocator 的 userdata 为每个 `FLuaEnv` 独立记账：分配前检查
`LiveBytes + Delta`，超限返回 `nullptr`，让 VM 抛**可恢复**的 `not enough memory`（`pcall`
可捕获）；释放永不受限，所以 `lua_close()` 不会卡死。自定义 `lua_Alloc` 不纳入配额。
`lua.memquota status` 输出每个 Env 的 `limit / live / rejections / last_request`，
`reset` 只清拒绝计数。它记账的是 allocator 看到的存活字节，不是 VM 分类 churn，
也不覆盖原生回调分配、GC scratch、被放弃协程的归因 —— **不是完整恶意代码隔离**。
与 `lua.gc limit <kb>` 的区别：后者只触发一次整轮回收，不拒绝分配。详见[控制台命令](ConsoleCommand.md#luamemquota)。

沙盒与类表冻结是可选硬化，不是对不可信脚本的完整隔离。UE 对象访问、debug、socket 和 io 是额外宿主能力；必须明确授权范围。文件路径校验不抵御 OS 符号链接/重解析点。Lua 5.5 自带 io 的策略也不由 Luau 的开关控制。

## 类型、示例与剖析

导出智能提示产生 `Plugins/UnLuaNova/IntelliSense/` 下的后端声明。用 `Tools/CheckLuauTypes.ps1 -Require` 明确要求工具存在；检查报告时不能只读退出码，还需确认分析器没有被跳过。

```lua
local Examples = require("Luau.Index")
if Examples.Available then
    Examples.VectorInterop:Run()
    Examples.BufferInterop:Run()
end
```

示例目录包含严格类型、沙盒边界、调试目标、语法、vector 和 buffer。守卫入口可在 Lua 后端加载，Luau 专有语法文件不能绕过守卫加载。

`lua.profiler trace on` / `trace dump [path]` 产生 profile.out。转换入口调用固定 Luau 子模块内的官方工具及其 svg 依赖：

```powershell
python Tools/LuauPerfgraph/perfgraph.py Saved/Profiling/profile.out > profile.svg
```

不支持 `--width`。io / CodeGen / buffer 的用法详见 [测试](Testing.md)。

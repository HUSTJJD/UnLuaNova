# Changelog

UnLuaNova 的变更记录。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

> 本项目源自 [Tencent/UnLua](https://github.com/Tencent/UnLua)（MIT）。
> 上游 2019–2023 年的历史变更请见 `Docs/Archive/UnLua_CHANGELOG.md`，或查阅本仓库的 git 历史。

## [Unreleased]

### Added
- **Lua 声明式网络**（`Public/UnLuaNovaNet.h` / `UnLuaNovaNetDecl.h` /
  `UnLuaNovaNetPayload.h` / `UnLuaNovaReplicationComponent.h`）
  - **RPC**：Lua 模块声明 `M.ServerRPC` / `ClientRPC` / `MulticastRPC`，自动生成带
    `FUNC_Net` 的 UFunction。此前 `ULuaFunction::IsOverridable` 明确排除 `FUNC_Net`，
    Lua 只能重新实现引擎已声明的 RPC，不能自己声明
  - **复制属性**：`M:GetLifetimeReplicatedProps()` 声明字段与条件，由
    `UUnLuaNovaReplicationComponent` 承载（复制属性必须挂在真正的
    `UPROPERTY(Replicated)` 上，这是运行时唯一造不出来的东西，所以用一个组件而不是基类）
  - 实现复用 `FLuaOverrides::Override` 已有的 `bAddNew` 分支，PIE 退出由
    `RestoreOverrides` 自动还原 —— 不需要 slua 那套运行时改 UClass + 清理逻辑
  - 参数类型与条件一律用**字符串**声明，声明层因此不需要 UHT 参与
  - 文档 [Lua 声明式网络](Docs/CN/LuaNetworking.md)；其中"已知边界"记了两点：
    RPC 的跨端行为待 C/S PIE 验证（`RPCId` 两端是否一致），复制属性为整体发送且条件单一
- **从 C++ 类型化调用 Lua 成员函数**（`Public/UnLuaNovaOverrider.h`）
  - `UnLuaNova::CallLuaFunction<Ret>(Object, Name, Args...)`，另有
    `CallLuaFunctionVoid` / `CallLuaFunctionIfExist`。此前引擎侧只能调**全局**函数
    （`Call` / `CallTableFunc`），没有"按对象调用并按类型取回返回值"的入口
  - 实现建立在既有的 `PushFunction` / `CallFunction` 之上，因此与 Lua 侧共享同一套
    self 表、Super 链与 `ReportLuaCallError`，不会各自漂移
  - 另附文档 [Lua 驱动对象与 C++ 调 Lua](Docs/CN/LuaDrivenObjects.md)，其中记录了一个
    已纠正的误判：**不需要 slua 那套 `ALua*` 平行基类** —— `ULuaFunction::IsOverridable`
    认 `FUNC_BlueprintEvent`，原生类本来就可覆写，而"不建蓝图"本来就有
    ini `ClassModuleBindings` / `IUnLuaNovaInterface` / `UnLuaNovaBindingComponent` 三条路
- **显式 GC 可分片**：`FLuaGcScheduler` + `lua.gc slice/budget/mult/limit/stats`。
  债务按**距上一次完整周期以来堆的增长**记账（不是每帧分配量 —— 否则"先建 4 MB 再整个
  丢掉"的脚本永远不会被记账）；硬上限触发时改整轮回收。
  **默认仍是 `Automatic` 且推荐保持**：这里本来没有周期性尖峰（`GC()` 的唯一调用方是
  `lua.gc` 命令，两个后端本就在跑分代 / 增量），`slice` 会 `LUA_GCSTOP` 从而放弃 Lua 5.5
  的 `LUA_GCGEN` 分代模式，只在过场 / 加载界面主动调 GC 时才划算。
  不做多线程 GC，理由见[架构](Docs/CN/Architecture.md#垃圾回收)
- **Profiler 按函数聚合**：`lua.profiler agg on|off` / `reset` / `top [n]` / `dump [file]`。
  Self 时间自底向上排行，补 Insights 没有的"哪个函数最热"视角；CSV 便于 CI 跨提交对比。
  该能力只在 Lua 5.5 后端给出 Self 时间 —— Luau 的 hook 分不出 call/return，只能采样
- **DAP 补 `pause` 与 `evaluate`**：`pause` 让下一行成为停止点（Lua 无法从外部强行打断；
  Luau 后端靠临时武装单步，落地即解除）；`evaluate` 按帧把局部变量编成形参求值，
  未暂停时只对全局求值
- **跨项目性能对照用例**：`UnLuaNova.Perf.BenchmarkParity`，用例名与迭代次数对齐
  slua-unreal 的 `TestPerf` 表，产出 `Saved/Benchmark/parity-latest-<Backend>.csv`，
  并额外跑两行 `ProcessEvent` 裸开销用于拆分"UE 调用成本"与"UnLuaNova 编组成本"
- **文档**：新增 [对标 slua-unreal：差距与吸收](Docs/CN/Comparison_sluaunreal.md)，
  逐项对照双方能力矩阵、写明哪三处不抄
- **数据驱动绑定**：新增 `ULuaModuleLocator_ByBinding` 并设为默认定位器。对象到 Lua 模块的映射
  可以不写一行 UE 侧代码：① `DefaultUnLuaNova.ini` 的 `ClassModuleBindings`（改配置即生效，
  匹配沿继承链向上找，绑基类即覆盖蓝图子类）② 蓝图 Actor 上的 `UUnLuaNovaBindingComponent`
  ③ 老路子 `IUnLuaNovaInterface`。"给不给没实现接口的对象绑"由配置项
  `bDataDrivenBinding` 决定（**默认关**）—— 它是项目决策，不是某个 locator 子类的属性，
  所以没有做成 C++ 里覆写的虚函数
- **DAP 适配器**：`lua.dap start [port]`（默认 5699）。`FLuaDebugService` 刻意不带 socket
  与线协议，缺的这层现在有了：VSCode 以 attach 方式连上即可断点 / 单步 / 看栈 / 看局部变量
  （`initialize` / `attach` / `setBreakpoints` / `threads` / `stackTrace` / `scopes` /
  `variables` / `continue` / `next` / `stepIn` / `stepOut` / `disconnect`）。
  命中断点时游戏线程阻塞自旋等 `Continue()`；不支持 `pause` 与 `evaluate`
- **IntelliSense 首次启动自动生成**：编辑器设置 `bAutoGenerateIntelliSenseOnStartup`（默认开），
  产物目录不存在时启动即导出一次，之后仍交给工具栏与目录监听做增量
- **内置库 `UnLuaNova.Timer`**（C++ 驱动）：`Set / Clear / Pause / IsValid / ClearAll / Time / Count`。
  到期时间存在世界时间上（`GetTimeSeconds()`，受暂停与时间膨胀；`bUnscaled` 时取 `GetRealTimeSeconds()`），
  只比较不累加；到期项先快照再回调；掉帧时按 `Interval` 追平并夹紧。
  由 `UTickableWorldSubsystem` 驱动，经 `FLuaEnv::AddBuiltInLoader` 注册为内建模块
- **内置库 `UnLuaNova.Event`**（纯 Lua）：`On / Once / Off / Emit / UnbindOwner / Count / Clear`。
  快照广播（广播中增删不漏不重）、逐个 `pcall`（一个监听者报错不带崩整条）、
  owner 弱键索引 + `UnbindOwner`（摘掉抓着已死对象的闭包，也是热重载后清残留的正解）
- **内置库 `UnLuaNova.OOP`**（纯 Lua，可插拔）：`Class / New / Super / Is / IsClass / NameOf`。
  单继承 + 方法查找沿父类链；`New` 从基类到子类依次调 `Ctor` 且参数透传；
  `Class.Super` 显式可用。不提供多继承、不继承 UE 类型（那是 native 的 `UnLuaNova.Class()`）
- **配置文件合并成一份**：`Config/DefaultUnLuaNova.ini`。两个设置类都改成 `Config=UnLuaNova`
  （运行时 section 与编辑器 section 同处一个文件），不再有 `DefaultUnLuaNovaSettings.ini` /
  `DefaultUnLuaNovaEditor.ini` 两份 —— 拆开之后"改一个开关该去哪个文件找"本身就成问题。
  ⚠️ **破坏性变更**：`DefaultUnLuaNovaSettings.ini` / `DefaultUnLuaNovaEditor.ini` 不再读取，
  不做兼容分支（配置入口只有一个，读的地方也就只该有一个）。升级请把配置迁到新文件。
  所有读配置的 Build.cs（UnLuaNova / 三个扩展插件 / Demo / TestSuite）与 `Tools/RunTests.ps1`
  已一并改到新文件
- **插件自带默认值 ini**：`Plugins/UnLuaNova/Config/DefaultUnLuaNova.ini`，按注释写全默认值
  （此前插件 Config/ 下只有 Localization 与 LuaTemplates，默认值只存在于 C++ 初始化器里
  —— 等于"两处"，且装完插件看不见有哪些开关）。工程侧 ini 只写覆盖项
- **特性开关统一格式**：全插件只有 `[/Script/UnLuaNova.UnLuaNovaSettings]` 一处定义特性开关，
  键名一律 `b<CamelCase>`（`bTimer` / `bEvent` / `bOOP` / `bDataDrivenBinding` / `bDebugAdapter` /
  `DebugAdapterPort`），查询名是去掉 `b` 的小写且大小写不敏感。
  原先散在 `DefaultUnLuaNova.ini` 的 `bWithStdLibXxx` 全部并入，编辑器设置只留编辑器行为
- **内置库可插拔**：`bTimer` / `bEvent` / `bOOP` 同时是编译期开关（Build.cs 读**同一份 ini、
  同一批键名**生成 `UNLUANOVA_HAS_STDLIB_*`，这样"改一处"就够了）。
  关掉后：native 库不注册、Lua 库 require 明确报错、`HasFeature` 回答 false。
  宏**只有 Build.cs 一个来源**：`UnLuaNovaBase.h` 里的 `#ifndef` 兜底默认值已删除 ——
  同一件事不该有两个答案，且"忘记定义"应表现为构建没跑
- **`HasFeature` 改成一张特性表**（`Features[]`，每项一个取值函数）：后端能力（不可配置）
  与配置开关（可配置）对外走同一个入口，加一个特性就是"设置类加 `b<Name>` + 表里加一行"
- `UnLuaNova.HasFeature('event')` / `('oop')` / `('timer')`：内建模块也按能力暴露
- **文档**：新增 [内置库](Docs/CN/StdLib.md)，写明"机制 vs 策略"的边界与下沉 C++ 的四条判据
- **性能用例**：`Lua.Container.PointsAreCoplanar`（自动化 CSV 第八行）与关卡脚本里同名两行
  —— 每轮"建 `TArray<FVector>` + 塞 8 个 `FVector` + 整包传给 UFunction"，
  拆成"只建"与"建+传"两行计时，第二行减第一行 ≈ 调用成本

### Changed
- **默认模块定位器**由 `ULuaModuleLocator` 换成 `ULuaModuleLocator_ByBinding`。没配任何绑定时
  行为逐字不变（接口优先、其余返回空串），已有项目无需改动
- **`package.path` 改为按插件实例拼**，不再硬编码 `Plugins/UnLuaNova/Content/Script/?.lua`。
  硬编码的后果是"工程套插件"（真正的插件在 `Plugins/UnLuaNova/Plugins/UnLuaNova/`）时，
  随包发布的模块（StringEx / Vector / Buffer / Input / HotReload / Event）全部 require 不到，
  而扁平布局的本仓库一切正常，没人会发现。顺序不变：工程在前，插件在后
- **用例**：`UnLuaNova.API.RuntimeModules` 补 Event 与 Timer；Event 断言的是手写版本最易错的
  四件事（广播中增删、单个监听者报错、owner 摘除、Once 语义）
- **打包 stage 路径按插件实例枚举**：原来只补名字含 `UnLuaNovaExtensions` 的插件，工程套插件
  （真正的插件在 `Plugins/<名>/Plugins/<名>/`）时插件自己的运行时模块会被漏掉，
  表现为编辑器一切正常、打包后 `require` 失败。现在凡名字以 `UnLuaNova` 开头且带
  `Content/Script` 的已启用插件都会补上

---

## [1.0.0] - 2026-10-23

UnLuaNova 的第一个正式版本。上游 Tencent/UnLua 停在 v2.3.6（2023-11），这一版把散落的社区成果
收敛成一条有人维护的发行线，并把三件事做成默认能力：构建期静态绑定、Lua 5.5 / Luau 双后端、
Lua 可观测性（Profiler、内存统计、headless 调试服务、沙盒、死循环看门狗）。

版本号从 `2.3.6` 回到 `1.0.0`：沿用上游的版本号会让"这是上游的哪个版本"和"这是 UnLuaNova 的哪个
版本"混在一起，而两者的兼容性承诺并不相同。UnLuaNova 从这里开始按自己的版本线走。

### Added
- **「相对 UnLua：原理与优势」两页文档**：`Docs/CN|EN/WhyUnLuaNova.md`。此前只有
  `Features.md` 的「相对 UnLua 的增量」清单，只说"有什么"，没说"**为什么这么做**"。
  新页对 12 个主题逐条给出 **痛点 → 原理 → 优势 → 边界**：静态绑定（构建期 POD 表 +
  惰性解析 + 读锁与 `PendingCount` 快判）、`CallUEFast` 为什么只省一半、参数缓存（注明沿用
  上游 `ENABLE_PERSISTENT_PARAM_BUFFER`，不冒充增量）、双后端能力宏、LuaScriptCompat、
  VM Hook 分发器（记录"看门狗查 `lua_gethook` 后静默失效"这个真实发生过的问题）、
  Profiler 写 Insights、看门狗线程模型、内存统计两层数据、headless 调试服务、沙盒、
  脚本侧类型检查，另附「代价与已知边界」总表。已从 `Features` / `index` / `README` 交叉链接，
  两页均挂进 `mkdocs.yml` 导航
- **架构与 C++ API 两页文档**：`Docs/CN|EN/Architecture.md`（目录结构、双后端抽象、
  绑定与覆写、静态绑定全链路、如何新增 LuaLib 类型）与 `Docs/CN|EN/CxxApi.md`
  （`FLuaEnv` / `IUnLuaNovaModule` / 定位器 / 导出与注册 / 静态绑定注册表 / `LowLevel` /
  覆写 / 调试类型，签名逐条抄自头文件）。两页已挂进 `mkdocs.yml` 导航
- **三个新 spec，共 30 条用例**：`UnLuaNova.API.ErrorHandling`（10 条：编译错误、运行时错误、
  栈平衡、引擎侧错误、错误消息不含机器路径）、`UnLuaNova.API.Lifetime`（6 条：释放标记、
  脚本持有对象的回收语义、手动引用、环境销毁）、`UnLuaNova.API.FLuaEnv` 由 2 条扩到 14 条
  （多环境隔离、DoString 失败路径、GC/HotReload、loader 优先级、require 失败、启动模块、
  OnCreated/OnDestroyed、GetAll/FindEnv）
- **七个新 spec**：
  - `UnLuaNova.API.Overrides`（8 条）：可覆写性判定、`GetOverridableFunctions`、
    覆写与 `RestoreOverrides`、分发是否走 Lua、Suspend/Resume、两个对象互相独立、
    覆写内部 `error()` 不崩。为此新增 `UUnLuaNovaTestStubForOverrides`
    （`IUnLuaNovaInterface` + `BlueprintImplementableEvent`）
  - `UnLuaNova.API.Latent`（2 条）：把"主线程调用 latent"的报错文案固化成契约
  - `UnLuaNova.API.Locators`（7 条）：默认 / `ByPackage` / 自定义模块与环境定位器
  - `UnLuaNova.Editor.ConsoleCommands`（7 条）：`lua.do` / `lua.gc` / `lua.profiler` /
    `lua.memstats` / `lua.dbg` / `lua.sandbox` 的**行为**（原来只验"命令存在"）
  - `UnLuaNova.Settings.General`（5 条）：启动模块名、诊断开关、定位器类"改了看得见效果"，
    且必须能还原
  - `UnLuaNova.Editor.IntelliSense`（5 条）：生成的类型声明内容与函数级辅助
  - `UnLuaNova.Lua`：新增 Lua 侧自测框架 `Content/Script/Tests/Lua/Spec.lua`
    （`describe` / `it` / `equal`，不引第三方）+ `StringEx.spec.lua`；C++ 侧逐条
    `TestTrue` 搬回结果，失败带 Lua 文件名与行号
- **性能基线**：`UnLuaNova.Perf.Benchmark` 跑七项 —— 跨语言调用三项（Native / 反射 / Lua）
  与纯 VM 计算四项（`VM.Arithmetic` / `VM.Call` / `VM.TableGrow` / `VM.TableRead`），按后端写 CSV 到
  `Saved/Benchmark/{latest,previous}-<后端>.csv`；`GenerateTestReport.py` 读出来与**同一后端**
  的上一轮做环比，劣化超 ±30% 标 ⚠️ 并在 stderr 提示，**不判定失败**（计时受机器负载影响）。
  两组分开是必要的：跨语言那一跳的开销在参数编组与函数解析上，两个后端因此落在同一量级，
  只有 `VM.*` 三项才真正区分虚拟机。CSV 按后端分开同理 —— `-Backend Both` 先后跑两个后端，
  共用一份文件会让 Luau 那一轮拿 Lua 5.5 的结果当基线，把两个虚拟机的差异报成回归
- **容器迭代中增删用例**：`TArray` / `TMap` / `TSet` 各补两条（迭代中 Add / Remove 不崩，
  且改动真的生效）
- `Docs/EN/TestReports.md`：说明两份测试报告是自动生成的中文页、以及为什么不做翻译，
  英文导航改为挂这一页
- **Luau 后端的静态检查改用 `luau-lsp`**：以子模块引入 `Tools/luau-lsp`
  （`Tools/Build-LuauLsp.ps1` 用 cmake + MSVC 构建）。上游把 `luau-analyze` 的 `--defs`
  删了，`UnLuaNova.d.lua` 因此喂不进去 —— 缺了它每个 `UE.xxx` 都是 Unknown global，
  报告全是假错，这一步只能整段跳过。`luau-lsp analyze --definitions` 读的是同一份
  `.d.lua`

### Changed
- **showcase 剪辑默认抓 30 帧而不是 60 帧**：每一帧都是一次完整的视口回读 + PNG 编码，
  是跑测试时最主要的卡顿来源（每个 showcase 抓 60 帧时，4 个 showcase 占了整轮 183s 里的
  77s）。`GenerateTestReport.py` 改成按帧数反推播放帧率（`CLIP_SECONDS`，默认 2 秒），
  所以 30 帧按 15fps 播、60 帧按 30fps 播 —— 剪辑时长不变，截图数减半。
  想改长度就设 `UNLUANOVA_RECORD_FRAMES`（帧数，不是帧率）
- **`Tools/*.ps1` 丢掉 UTF-8 BOM 会整轮崩，现在能自愈**：Windows PowerShell 5.1 没有 BOM 就按
  系统 ANSI（GBK）读 `.ps1`，中文字符串被解成乱码后落在字符串字面量里，脚本一行没跑就
  `ParseException` —— 报告上只看到"类型检查失败"，看不到原因。BOM 可能被编辑器、也可能被
  脚本化的改写弄丢，所以 `RunTests.ps1` 在调用 `CheckLuaTypes.ps1` / `CheckLuauTypes.ps1`
  之前用 `Ensure-Utf8Bom` 检查并补回，而不是默认它还在
- **切后端不再删 `Plugins/UnLuaNova/Intermediate/`**：`UnLuaNova.Build.cs` 把
  `Config/DefaultUnLuaNova.ini` 登记进 `ExternalDependencies`，改 ini 即让 UBT 的
  makefile 失效，Build.cs 重新求值。以前 UBT 不盯着这个 ini，只能整目录删掉来逼它重编 ——
  75 MB、69 个 `.obj`，其中 62 个是跟后端无关的 Luau VM。现在只重编真正用到
  `UNLUANOVA_LUA_BACKEND` 的模块（切后端本身免不了重编：那两个后端的头文件不是同一份）
- **IntelliSense 产物移到插件自己的目录**：`Plugins/UnLuaNova/Intermediate/IntelliSense/` →
  `Plugins/UnLuaNova/IntelliSense/`（已 gitignore）。它不再是构建中间产物：IDE 指过去、
  两个静态检查器读它，而 `Intermediate/` 是 UBT 随时会清的目录。**破坏性改动**：`UnLuaNovaDemo.code-workspace`
  和 IDE 的 `workspace.library` 要改指新路径
- **测试自己导出声明文件**：`Tools/RunTests.ps1` 在类型检查前跑一次
  `UnrealEditor-Cmd.exe <uproject> -run=UnLuaNovaIntelliSense -BP=1`（两个后端只付一次启动成本）。
  之前只有人点过工具栏按钮的机器才有声明文件，别的机器上类型检查等于没跑却照样报绿
- **LuaLS 的假绿灯**：`--check_format` 默认是 `pretty`，`check.json` 根本不落盘，而脚本把
  "没有结果文件"当成"0 错误"写进 summary —— 一轮 700 条诊断的报告显示全绿。现在显式要
  JSON、结果文件缺失时改用 stdout 解析、实在没有就记 `skipped`（`-Require` 下报错退出）
- **Luau 声明改成这个 Luau 版本认得的语法**：`declare class` 已被上游移除，现为
  `declare extern type X extends Y with ... end`；out 参数按 `FFunctionDesc` 的语义回到返回值
  而不是既当入参又写成 `any...`；重名类型/重名成员去重；`end` 这类关键字参数名加下划线。
  `UnLuaNova` / `UE` 两个全局也声明了 —— `UnLuaNova` 按 `Content/IntelliSense/UnLuaNova.d.lua`
  真实建模（与 EmmyLua 那份并列维护），`UE` 因为 18k 个成员会触发 Luau 的复杂度上限、
  且脚本写 `UE.FVector` 而声明叫 `Vector`，只能先做成不透明表
- **`Tools/CheckLuauTypes.ps1` 在喂不进声明时跳过**：这个版本的 luau-analyze 没有 `--defs`
  （上游删了，vendored 的 Luau 不归我们改），传了会是 `Unrecognized option` 且一条诊断都看不到。
  先探测再决定传不传；喂不进去就跳过并写明原因，和 Lua 后端缺 EmmyLua 声明时的处理一致
- `GenerateTestReport.py` 新增 `load_benchmark` / `compare_benchmark` / `render_benchmark`，
  报告里多一节「性能环比」
- `Tools/Install-NightlyTask.ps1` 优先用 `pwsh.exe`，找不到时退回 `powershell.exe`
  （原来硬编码 `pwsh.exe`：只装了 Windows PowerShell 5.1 的机器会注册一个永远跑不起来的任务）
- `Docs/CN|EN/Testing.md` 新增「用例清单」表（三列：注册名 / 覆盖什么 / 备注），
  由 `Private/` 下的注册宏生成
- `Docs/EN/API.md` 补上中文有而英文没有的三节（脚本侧 `UnLuaNova` 全局表、字符串扩展、
  随插件发布的 Lua 模块）；`Docs/EN/Settings.md` 补「Lua 环境分配器」「检查新版本」，
  并把合并成一节的四项兼容开关拆开
- CI 的 `mkdocs build` 恢复 `--strict`：两份测试报告已作为占位页入库，首次克隆也能干净构建
- `.codebuddy/PROJECT_PLAN.md` 修正重复的 §11 编号（前者改为 §10.5）

### Removed
- `.codebuddy/_scan_log.py`：被并发进程误提交进 `11301173` 的临时脚本，已从索引移除
- **`Tools/Build-LuauAnalyze.ps1`**：Luau 后端的静态检查改用 `luau-lsp` 之后没人调用了。
  `luau-analyze` 本体还在 Luau 子模块的 `CLI/` 里（UBT 不编），真要用时 cmake 编
  `Luau.Analyze.CLI` 就是了
- **`Tools/RunTests.ps1 -BuildOnly`**：只编译不跑（编完退出，不启动编辑器、不生成报告），
  与 `-SkipBuild` 互斥。让"测试代码编译不过"这类问题能在一天内被发现
- **`Tools/Install-NightlyTask.ps1`**：注册/删除"每晚编译一次测试代码"的 Windows 计划任务
  （自托管 runner 的降级形态；有 runner 时直接用 Actions 的 `schedule`）
- **用例清单基线护栏**：`summary.json` 增加 `names`（用例全名清单），
  `GenerateTestReport.py` 新增 `--baseline` / `--update-baseline` / `--fail-on-drift`，
  `RunTests.ps1` 新增 `-UpdateBaseline` / `-FailOnDrift`。用例被改名/误删/被 `-Filter` 漏掉
  时报告会明确说出来（默认只告警，避免合理的用例调整把 CI 变红）；基线
  `Tools/baseline/{lua,luau}.json` 入库
- `TSet` 支持 `pairs` 迭代：注册 `UNLUANOVA_ITER_ENTRY(TSet_Pairs)`（Luau 下是 `__iter`、
  Lua 5.x 下是 `__pairs`），产出 `(元素, true)`；之前 `pairs(Set)` 在两个后端都会报错，
  只能靠 `ToArray()` / `ToTable()`
- **静态绑定层** `StaticBinding/`：POD 描述符 + 注册表（延迟解析）+ 自动注册器
- **`UnLuaNovaStaticBindingGenerator`** Commandlet：构建期为声明类型生成静态绑定表
- `CallUEFast`：对已解析函数跳过 `GetFunctionCallspace` / 覆写重定向查找 / 接口函数查找
- **`UnLuaNovaShowcaseDressing` Commandlet**：给示例关卡补装"能被拍到的东西"——每关一盏可移动
  太阳光 + 一盏可移动天光 + 一块地板 + 三个立方体 + 一个 PlayerStart。四个教程关卡是空世界
  （无灯光/网格体/PlayerStart），示例内容只在屏幕文字与日志里，所以拍出来必然是纯黑；这补的是
  根因，不是截图参数。全部 Movable（静态光要烘光，Commandlet 跑不了），幂等，
  `-Undo` 只删带 `UnLuaNovaShowcaseDressing` tag 的 Actor
- **Lua 后端抽象** `LuaBackend.h`：吸收 Lua 5.4 与 Luau 的 API 差异（`UNLUANOVA_LUA_BACKEND` 开关）
- Luau 以 git submodule 引入（`Source/ThirdParty/Luau`，0.743）+ `Luau.Build.cs`
- 改名脚本 `Tools/Rename-To-UnLuaNova.ps1`
- **Luau 能力落地**：
  - `UnLuaNova.GetBackend()` / `UnLuaNova.HasFeature()`：脚本按能力分支，而不是猜后端
  - `vector` ↔ `FVector`：`UE.FVector(vector)`、`v:Set(vector)`、`v:ToVector()`，
    UFUNCTION 的 `FVector` 参数与反射写入路径都能直接收 `vector`
  - Luau 模块 `UnLuaNova.Vector` / `UnLuaNova.Buffer`（`buffer` ↔ `TArray` 显式转换）
  - 示例集 `Content/Script/Luau/`（严格类型、沙盒边界、`lua.dbg` 目标、语法速览、vector 互操作），
    由 `Index.lua` 做后端守卫，Lua 5.5 下一个文件都不会加载
  - `Tools/CheckLuauTypes.ps1`：用 `luau-analyze` + 生成的 `UnLuaNova.d.lua` 做静态类型检查
  - 文档：`Docs/CN/Luau.md` 与 `Docs/EN/Luau.md`
- **示例截图由测试产出**（`Private/Tests/ShowcaseTest.cpp`）：`UnLuaNova.Showcase.Binding /
  Override / Container / UMG` 四条用例各打开一个教程关卡（`Content/Tutorials/`）截一张图，
  文档站 `Docs/CN/Showcase.md` 和每份报告的画廊都用它。此前**没有任何用例截图**，所以报告里
  永远写着"没有截图"、Showcase 页的四张图一直是注释掉的占位符。
  它们用 `BEGIN_VIEWPORT_TESTSUITE` 注册：无视口就没有画面可截，`-NullRHI` 那一轮把整条剔除，
  而不是记成"截图失败"
- **录屏（默认开，不是选项）**：`FCaptureFramesLatentCommand` 按帧连续截图成帧序列，
  报告生成时编码成 `.mp4` 内嵌进报告。`-NoVideo` 才不录。
  编码器按顺序找：`--ffmpeg` → `$FFMPEG` → PATH 上的 `ffmpeg` → **`imageio-ffmpeg` 自带的
  静态 ffmpeg**（`Tools/requirements.txt` 里一个普通 pip 包，不用装系统级 ffmpeg、不用改 PATH）。
  编不出来就是失败：写进报告的「视频」一节（带原因）+ 脚本非 0 退出 ——
  不再"保留帧序列凑合看"，因为凑合看的报告跟"这轮没录上"长得一样。
  编码时加了 `scale=trunc(iw/2)*2:...`：libx264 不接受奇数宽高，视口尺寸不是测试能决定的
- **新增规格用例**，补此前完全没有用例覆盖的能力：
  - `UnLuaNova.API.StringEx`：字符串扩展在 `string` 表上可用、`split`/`count` 按字面量
    （传 `"."` 得到按点分隔而不是空表）、两个后端行为一致
  - `UnLuaNova.API.Observability`：CPU 分析开关与"开着也能正常跑脚本"、内存统计的开/关与
    记账/清零、调试服务的断点登记（重复添加被拒）/ 挂载 / 单步命中并抓到调用栈。
    此前这三类**只有底层 Hook 有覆盖**，`lua.profiler` 装不上钩子、`lua.memstats` 不记账
    都不会被任何用例发现
  - `UnLuaNova.API.LuaScriptCompat`：`collectgarbage` / `load` 在两个后端都存在且行为一致
    （Issue291 踩过的坑：Luau 基础库没有 `collectgarbage`，脚本直接 `attempt to call a nil value`）
  - `UnLuaNova.API.RuntimeModules`：运行时 Lua 模块（`StringEx`/`Vector`/`Buffer`/`Input`/
    `EnhancedInput`/`HotReload`）必须随插件发布 —— 断言的是**文件在插件目录里**，
    不只是 `require` 得出来（后者在本仓库无论文件放哪都通过）；外加 `Vector` / `Buffer`
    的模块表与 `HasFeature` 一致性、以及后端可用时的实际互转
  - `UnLuaNova.API.StaticBinding`：注册 / 解析 / 按名字与按 `UStruct` 查找 / 属性与函数解析到
    真实 `FProperty`、`UFunction` / `GetPropertyKind` 的类型映射。此前**整个静态绑定层没有
    一条用例**：生成的表是构建产物、不入库，所以正常构建下注册表永远是空的、快速调用路径
    永远走不到，注册表坏了也没人知道
  - `UnLuaNova.API.CallUEFast`：预解析路径的分发条件（非网络 / 非 latent / 非接口）成立，
    且"有静态绑定"与"无静态绑定"两条路径的调用结果一致。两条探针函数是因为
    `FFunctionDesc` 在**构造时**就把绑定快照下来了，同一个函数在一个进程里走不了两条路
  - `UnLuaNova.API.DefaultParamCollector`：集合启动后非空（UHT 侧的采集器跑没跑，以前没有任何
    信号——空集合的表现不是编译错误，而是每个默认参数静默变成零），以及 Lua 省略实参时默认值
    生效、显式传参会覆盖默认值
  - `UnLuaNova.Editor.Commands`：`lua.*` 控制台命令全部已注册、六份 Lua 模板随插件发布且带
    `ClassName` 占位符、脚本根目录指向工程 `Content/Script`、编辑器命令上下文已注册
  - `UnLuaNova.Editor.StaticBindingGenerator`：端到端跑一次 Commandlet，断言它把
    `UNLUANOVA_REGISTER_STATIC_BINDING` 写进了 `-OutputDir` 指定的目录（不写默认目录，
    否则它会变成真的被编译进工程的测试产物）
- **英文文档补三页**：`Docs/EN/Features.md`、`Settings.md`、`ConsoleCommand.md`
  （此前 EN 只有 7 页，"功能清单 / 设置项 / 控制台命令"这三个英文读者最先找的页面全是缺的）
- **`TMap:Contains(Key)`**：与 `TArray` / `TSet` 对齐。此前只有 `Find` 可用，
  而 `Find` 返回 `nil` 既可能是"没有这个键"也可能是"值本身是 nil"；更糟的是缺一个方法时
  `Map:Contains(2)` 走到 `__index` 自引用链上，报的是
  `'__index' chain too long; possible loop`，看不出真正原因是名字不存在

### Fixed
- **`lua.sandbox` 现在两个后端都存在**：它原先只注册在能沙盒化的后端里（`UNLUANOVA_HAS_SANDBOX`），
  plain-Lua 下敲 `lua.sandbox 1` 得到的是"命令不存在"。CVar 改为无条件注册，是否真的生效
  由 `UnLuaNova.HasFeature('sandbox')` 回答
- **`IntelliSense::IsValidFunctionName` 拒绝 `__` 开头的名字**：`__index` / `__newindex`
  这类脚本内部名会被写进生成的声明文件，补全因此给出脚本根本调用不了的东西
- **覆写分发用例取的是分发真正会用的那个环境**：`ULuaFunction::execCallLua` 用
  `IUnLuaNovaModule::Get().GetEnv(Context)`（环境定位器的结果）解析环境，而不是
  `TryBind` 被调用的那个。spec 自建 `FLuaEnv` 时绑定与分发落在两个 VM 上，
  表现是"覆装上了、调用却静默走原实现"
- **已发布的报告终于有「静态类型检查」这一节**：`GenerateTestReport.py` 写 `Docs/` 那份时
  调 `render()` 漏了 `typecheck` / `encode_failures`，于是入库的页面永远写着"这一轮没有跑：
  `RunTests.ps1` 没写 `luau-analyze.json`"，只有 `Saved/` 下那份能看到检查结果
- **Luau 示例 `02_SandboxBoundary.lua` 的 `Try` 注解**：`Fn: () -> ()` 让 Luau 认为
  `pcall(Fn)` 只返回一个值，取第二个返回值被判成 "Function only returns 1 value, but 2
  are required here"（喂进声明之后静态检查报出的唯一一条错误）。改成 `() -> (...any)`

### Changed
- **测试注册名统一大小写**：`UNLUANOVA.API.Base` / `UNLUANOVA.API.Enum` →
  `UnLuaNova.API.Base` / `UnLuaNova.API.Enum`（spec 类名同步改为 `FUnLuaNovaBaseSpec` /
  `FUnLuaNovaEnumSpec`）。全大写写法有被 `-Filter` 漏掉的风险，也与 `Automation List` 输出
  对不上；约定已写进 `Docs/CN|EN/Testing.md`
- 引擎基线升到 **UE 5.8.3**：`BuildSettingsVersion.V6` + `IncludeOrderVersion.Latest`
- `LuaOverridesClass` 的 UField 链遍历适配 UE 5.7+（`UField::Next` 已是 `TObjectPtr`）
- 项目全面改名 `UnLua` → `UnLuaNova`（模块、UCLASS、宏、文件名、Lua 全局名、ini section）
- `EInternalObjectFlags::AsyncLoading` → `EInternalObjectFlags_AsyncLoading`（UE 5.8 已移除该枚举项）
- `FSoftObjectPtr::operator=` 导出返回类型修正为 `FSoftObjectPtr&`
- `UNLUA_LOGERROR` 格式串缺参修正（原为未定义行为）
- **许可变更**：MIT → **Polyform Small Business 1.0.0** + 附加条款
  （人数 < 10 且营收/融资 < $1M 可免费商用，超出需商业授权，见 `LICENSE` / `COMMERCIAL.md`）；
  上游组件的 MIT 授权不变，见 `LICENSE-THIRD-PARTY.md`
- 贡献需要签 CLA（见 `CLA.md` / `CONTRIBUTING.md`），否则商业授权无法向下再许可
- `UE` 命名空间新增 `__newindex`：脚本改写引擎类型会被直接拒绝
- `Tools/RunTests.ps1` 切换 Lua 后端改为**原子操作**：后端是编译期选择，ini 与二进制一旦分叉，
  结果就会被记在错误的后端名下（真发生过：目录叫 `lua`，跑的是 Luau 的二进制）。现在加了
  单实例锁（`Saved/RunTests.lock`，带持有者 PID）、编译失败立刻还原 ini、启动编辑器前按
  UBT 生成的 `Definitions.UnLuaNova.h` 校验磁盘上真正的后端，`-SkipBuild` 与切后端互斥
- **`Tools/RunTests.ps1` 默认不再 `-NullRHI`**：无头更快，但那不是"同一套用例跑得快一点"——
  UMG 控件在无渲染时不会被 Tick（Issue434 因此永远等不到委托广播，看着就像绑定坏了），
  框架会把这类用例整条剔除。默认改成带真实视口跑全套，要快再加 `-NullRHI`（此时报告总数会少
  几条，那几条是没跑，不是通过）。`-Screenshots` 现在只管"收不收集 PNG"，不再兼管渲染开关
- **`Tools/RunTests.ps1` 默认收集截图**：`-Screenshots` 换成 `-NoScreenshots`。之前默认不收，
  于是每次报告的画廊都是空的，看的人只能认为"这套测试不产图"。
  跑之前还会先清空 `Saved/Screenshots/WindowsEditor/`——编辑器不清它，不清就会把上一次的 PNG
  当成这一次的图发布出去，这比没有图更糟，因为看的人看不出来
- **`Tools/GenerateTestReport.py` 新增「视频」一节**：帧序列合成一条而不是并列几十张同名图，
  编码成功就内嵌 `<video>`，没编码就说明"只录到了帧序列"
- **文档按代码校正**：
  - 删掉代码里已经没有的东西：`SUPPORTS_RPC_CALL` / `SUPPORTS_COMMANDLET` 两个宏
    （`API.md`）、「启用Insights分析支持」（`ENABLE_UNREAL_INSIGHTS` 已移除，profiler 现在是
    运行时开关）和「启用RPC调用支持」两个设置项（`Settings.md`）
  - `API.md` 补 `UnLuaNova.GetBackend()` / `HasFeature()`（含完整的五個能力名与后端对照表）、
    字符串扩展一节、以及完整的宏清单（原先只列了 6 个，其中一个已不存在）
  - `Settings.md` 说清 Lua 后端的默认值（键不存在时是 `Luau`）与"仓库 ini 里的 `Lua` 是上次
    跑完测试还原出来的，不是项目选择"，避免读者照着文件反推默认值
  - `Luau.md` 给 buffer ↔ TArray 单独开一节；`StaticExportBinding.md` 补 commandlet 的完整
    调用方式与 `meta=(UnLuaNovaExport/UnLuaNovaNoExport)` 的代码示例
  - `Showcase.md` 写明四张图分别由哪条用例、哪个关卡产出，以及"要出图就不能 `-NullRHI`"
- **英文文档补齐 9 页**，与中文目录一一对应（此前 EN 只有 10 页，Quickstart / CustomTemplate /
  CollisionEnum / Plugins_And_Modules / Debugging / IntelliSense / Testing / Showcase /
  StaticExportBinding 全缺，英文读者点进去就是 404）：
  `EN/Quickstart_For_UE_Newbie.md`、`CustomTemplate.md`、`CollisionEnum.md`、
  `Plugins_And_Modules.md`、`Debugging.md`、`IntelliSense.md`、`Testing.md`、`Showcase.md`、
  `StaticExportBinding.md`。`mkdocs.yml` 的英文导航也按中文同样的分组重排（入门 / 编程 /
  配置与工具 / 诊断与质量 / 参考）
- **编码从 `RunTests.ps1` 挪进 `GenerateTestReport.py`**：原来 ps1 里找 PATH 上的 ffmpeg，
  找不到就散一地 PNG；现在"用哪个编码器"只有一个地方决定，ps1 只管录帧和拷帧。
  新增 `Tools/requirements.txt`（`imageio-ffmpeg`，自带静态 ffmpeg）
- **静态类型检查进报告**：`RunTests.ps1` 现在会在每个后端跑完后调 `Tools/CheckLuauTypes.ps1`
  （新增 `-SummaryPath` 写 `luau-analyze.json`），`GenerateTestReport.py` 把它渲染成
  「静态类型检查（luau-analyze）」一节，`summary.json` 增加 `typecheck`。
  之前 `luau-analyze` 只存在于一个独立 ps1 里，跑没跑、过没过在任何报告里都看不到；
  没装 luau-analyze 时报告写"跳过 + 原因"，不假装查过。新增 `-FailOnTypeErrors`
  让它成为门禁（默认不，因为它是静态检查不是用例）
- **Lua 后端也有静态检查了** `Tools/CheckLuaTypes.ps1`：用 `lua-language-server`（LuaLS）+
  编辑器导出的 EmmyLua 注解（`Intermediate/IntelliSense/*.lua`）检查 `Content/Script`
  （排除 `Luau/`）。**两个后端各用自己的分析器**，这不是偏好而是语法事实：Luau 的类型注解
  Lua 5.5 解析不了，反过来 Lua 后端的脚本也喂不进 `luau-analyze`，所以不存在"共用一个"。
  `RunTests.ps1` 按后端分派（Luau → `luau-analyze.json`，Lua → `lua-analyze.json`），
  报告里两个后端都会出一节「静态类型检查」；没装 LuaLS、或没导出过智能提示时明确写
  "跳过 + 原因"，不硬跑出几百条假错

- **静态检查器以子模块引入**：`luau-analyze` 本来就在 Luau 子模块里
  （`CLI/src/Analyze.cpp`，只是 UBT 用 `.ubtignore` 排除了 `CLI/`），新增
  `Tools/Build-LuauAnalyze.ps1` 只构建 `Luau.Analyze.CLI` 这一个 target；
  LuaLS 新增子模块 `Tools/lua-language-server`（跟 master，与 Lua / Luau 两个子模块一致的
  安排：父仓库记录一个具体 commit，要往前走就 `git submodule update --remote`），
  配套 `Tools/Build-LuaLS.ps1`（要 Ninja + MSVC，首次会拉它的 21 个嵌套子模块）。
  两个检查脚本都优先用仓库里构建出来的那份，而不是 PATH 上碰巧装的那个

### Changed
- **IntelliSense 产物按方言分目录**：`Intermediate/IntelliSense/` 下拆成
  `EmmyLua/`（Lua 方言，`UE.lua` / `UnLuaNova.lua` / `<模块>/<类型>.lua`）和
  `Luau/`（Luau 方言，`UnLuaNova.d.lua`）。之前两种方言混在一个目录里，
  把整个目录当库喂给 LuaLS 就会让它去解析 `declare class` —— 那不是合法 Lua。
  **破坏性改动**：之前的产物要重新导出一次，IDE 的 `workspace.library` 要改指到
  `IntelliSense/EmmyLua`，`Tools/CheckLuauTypes.ps1` 的默认 `-Defs` 同步改成
  `IntelliSense/Luau/UnLuaNova.d.lua`

### Fixed
- **传错类型的 UFunction 实参不再只是打日志**：`ENABLE_TYPE_CHECK` 打开时这类错误过去只
  `UE_LOG(Error)` 一行然后**照常发起调用** —— 那个参数保持初始值，于是脚本拿到一个自己从没
  传过的 0，真正的错误要到后面以"数字不对"的形式才暴露。现在改为抛 Lua error：`pcall` 能捕获，
  没捕获的照旧由 `ReportLuaCallError` 打出来，诊断只是推迟到了能判断"这次调用该不该失败"的人
  手里。**少传实参**的那一路仍然只打日志：那个栈位在调用者提供的最后一个实参之后，没有"传错的
  值"可以归责，而声明了默认值的函数由 UE 自己补齐
- **`UnLuaNovaEditor` 模块编不过**：`UnLuaNovaShowcaseDressingCommandlet.cpp` 里的
  `#include "Engine/SkyAtmosphere.h"` 在本引擎（UE 5.8.3 源码构建）中不存在 ——
  `ASkyAtmosphere` 声明在 `Components/SkyAtmosphereComponent.h`。改掉后整个 `UnLuaNovaEditor`
  模块才能编译。这正是 T1/T2 的"每晚编译一次"回路该抓的那类问题
- **`RunTests.ps1` 读 `summary.json` 未指定编码**：用例名含中文时 `ConvertFrom-Json` 直接抛错，
  崩溃报告因此读不到结论。加 `-Encoding UTF8`
- **`RunTests.ps1` 取锁偶发失败**：删掉残留锁之后立刻 `File::Open(FileShare::None)`，
  在 Windows 上会因删除尚未真正生效而抛 `IOException`。加重试（5 次 / 400ms）
- **「静态类型检查」这一节从来没出现过**：`RunTests.ps1` 写摘要用的是
  `Set-Content -Encoding UTF8`（PowerShell 会加 BOM），而 `GenerateTestReport.py` 用
  `utf-8` 读，解析直接抛异常 —— 结果这一节被整块跳过，报告只留下"这一轮没有跑"，
  看起来就像静态检查从来没接过。现在和读自动化 JSON 一样用 `utf-8-sig`
- **报告里多出一条用例 `luau-analyze`**：`GenerateTestReport.py` 把报告目录下所有 `*.json`
  当用例读，静态检查的摘要也被算成一条「跳过」的用例（并写进了基线）。现在按名字排除，
  `Tools/baseline/{lua,luau}.json` 也清掉了这条伪用例（总数各减 1）
- **文档"幽灵 API"清理**：`Docs/CN|EN/API.md` 里的 `FLuaContext` / `GLuaCxt` 改写为
  `UnLuaNova::FLuaEnv` 与 `IUnLuaNovaModule::Get().GetEnv()`；`AUTO_UnLuaNova_STARTUP` →
  `AUTO_UNLUA_STARTUP`、`UnLuaNova_ENABLE_DEBUG` → `UNLUANOVA_ENABLE_DEBUG`（与
  `UnLuaNova.Build.cs` 一致）；删掉已移除的 `SUPPORTS_RPC_CALL` / `SUPPORTS_COMMANDLET`；
  标注 `OnLuaContextInitialized` / `OnPre/PostLuaContextCleanup` 当前构建里**没有广播点**
- **文档口径统一**：许可 MIT → Polyform（`Docs/index.md`、`mkdocs.yml`）；引擎版本
  "4.17.x - 5.x" → 基线 UE 5.8.3（`Docs/CN|EN/Features.md`）
- **README 乱码/截断修复**：`Luau 后端`（原为 GBK 误读产生的乱码）与"跑测试"条目
  （原被截断成 `.\Tools\RunT`），`README.md` / `README_EN.md` 均为 UTF-8
- **`Docs/EN/*` 回链修正**：指向 `../CN/*.md` 的跨语言链接改回 `../EN/*.md`
  （`Docs/EN/index.md` 指向中文索引那一条是有意为之，保留）
- `CHANGELOG` 里的 `UnLuaStaticBindingGenerator` → `UnLuaNovaStaticBindingGenerator`
- **Showcase 示例截图全是纯黑**：`FScreenshotLatentCommand` 之前用 `RequestScreenshot(Name, false, false)`
  截"裸场景渲染目标"，而教程关卡（`Content/Tutorials/`）本身没有灯光和网格体，可见内容
  （`Screen.Print` 的屏幕调试文字、ReleaseUMG 的 UMG 控件）全在 UI 层；引擎在 `bShowUI=false`
  的截图请求里还会显式关闭屏幕调试文字。于是四张示例图（连同录屏帧序列）都是"空世界的
  场景渲染"= 纯黑，且能安静通过。现在改为截**游戏视口含 UI**（`bShowUI=true` +
  `bRestrictToGameViewport=true`，PIE 内嵌在编辑器视口时不会把编辑器界面截进来），并新增
  `FVerifyScreenshotNotBlankLatentCommand`：解码 PNG，整图单一颜色即判失败 —— 黑图从此
  进不了文档站
- **录屏第 1 帧拍到编辑器视口**：`FCaptureFramesLatentCommand` 现在先丢 3 帧预热，等 PIE 视口稳定
  再开始连拍，否则每段视频开头都是一帧"关卡编辑器网格 + 坐标轴"
- **报告里的视频在 GitHub 上"播不了"**：`<video>` 是裸 HTML，GitHub 的 markdown 渲染器会把它整块
  过滤掉。视频下方补了一条纯 markdown 的"直接打开视频文件"链接，任何渲染器都能显示
- **Lua 5.5 后端一跑就崩**：`TSharedPtr` 的 `__gc`（`ReleaseSharedPtr`）用 `lua_touserdata()` 取到的是
  块首的 `FUserdataDesc`，而不是 `NewSmartPointer()` 放置智能指针的 payload，于是 `Reset()` 把描述符
  当成 `TSharedPtr` 拆了 —— 静态导出之后第一个 `lua_close()` 必然破坏堆，整个 Lua 后端跑不出任何一条
  测试结果。Luau 没有 `__gc`，这条路径在 Luau 下从未执行过，所以一直没暴露。
  现在改用 `GetSmartPointer()`（按 payload 取），`FObjectRegistry::Get()` 一并修正
- **Luau 下 64 位整数无法转字符串**：vendored Luau 带第二个数值 tag `LUA_TINTEGER`（`lua_pushinteger64`，
  用来精确表示 64 位值），但 VM 的 `luaV_tostring()` 只认 `LUA_TNUMBER`，于是所有走
  `lua_tolstring()` 的路径都拒绝整数：`string.format('%s', v)` 报
  `invalid argument #N to 'format' (string expected, got integer)`，拼接同样不行。
  任何从 64 位 UE 属性读出来的值都是这一类。已按 `luaL_tolstring()` 的写法补上
  `LUA_TINTEGER` 分支（对 vendored Luau 的本地补丁，`lvmutils.cpp` 内有标注）
- **Luau 下 `require('string')` 报 `module 'string' not found`**：plain Lua 的 `luaL_openlibs()`
  同时把每个标准库登记进 `package.loaded`，Luau 根本没有 package 库，`package` 是我们自己建的，
  之前只放了 `loaded` / `searchers`。第三方脚本就吃这个（LuaSocket 的 `socket.lua` 前两行就是
  `require("string")` / `require("math")`），所以在 Luau 上整个 LuaSocket 加载不起来。
  现在自建 `package` 时把标准库一并登记成模块（后端没有的库名自动跳过）
- `UE4File` 的 `Read` / `Write` / `__gc` 同样用 `lua_touserdata()` 取实例，拿到的是块首的
  `FUserdataDesc` 而不是 `TConstructor` 放置对象的 payload —— 与 `ReleaseSharedPtr` 是同一个
  布局问题。`UE.File` 目前没有被任何脚本或测试使用，所以还没踩出来；现在三处都改用
  `GetCppInstanceFast()`（按 payload 取，没有描述符时退回块首）
- 合并上游 `develop`：PR #700（UE5.4）、#707、#758（UE5.6）、#761、#765
- 合入社区 PR：#767 #756 #748 #711 #639 #643 #684 #705 #704 #672
- 合入 PR #771 的测试套件修复（`UnLua::Shutdown()` 前置、`GetUObject` 回读等）
- 沙盒（`lua.sandbox`）会顺带冻结 `UE` / `Lua` / `package.loaded`，导致类型懒注册与 `require` 失败；
  现在沙盒后会还回这几张表的写权限，脚本侧写入仍由 `UE` 表的 `__newindex` 拦截
- 文档把 `lua.sandbox` 写成了可执行命令，实际它是启动期 CVar（每个环境创建时读取一次）
- **撤下 Luau 调试 hook 必崩**：`UNLUANOVA_SET_DEBUG_HOOK` 被映射到了 `lua_callhook()`，而它不是
  setter —— 它是"就地调用一次 hook"。于是 `FLuaDebugService::Attach()` 什么都没装上，
  `Detach()` 传 nullptr 时则带着空函数指针、且 `L->ci` 停在 `base_ci`（`L->ci->p` 为空）
  走进 `luau_callhook()`，在 `lvmexecute.cpp` 里解引用一个不存在的 proto —— 编辑器直接
  `EXCEPTION_ACCESS_VIOLATION reading address 0x88`。
  现在改为写 `lua_callbacks(L)->debugbreak` / `debugstep`（Luau 真正回调调试 hook 的两个槽位），
  nullptr 即撤下；顺带修好 `lua.dbg` 的单步：此前没有任何回调装在 `debugstep` 上，
  `SetSingleStep()` 是静默失效的
- **测试进程崩溃后报告只剩"0 tests"**：崩溃时自动化框架不导出任何 JSON，报告与"压根没跑起来"
  长得一模一样，只能去翻几百 KB 的 `Editor.log`。现在 `GenerateTestReport.py` 会从中抓出
  **崩溃时正在跑的用例**和调用栈，写进报告的「崩溃」一节，`summary.json` 增加 `crashed` /
  `crash_test`；`RunTests.ps1` 同时把崩溃栈打到控制台并把退出码置 1（否则 CI 会当成通过）
- **Luau 没有 `collectgarbage`**：Luau 的基础库只到 `gcinfo()`，凡是强制 gc 的脚本（Issue291
  就是靠它证明委托绑定能活过一次 gc）都死在 `attempt to call a nil value`。现在
  `LuaScriptCompat` 在名字空闲时补上（plain Lua 自带，不会被覆盖），支持
  `collect` / `step` / `count`，单位与 plain Lua 一致
- **Luau 的 `%s` 只吃字符串**：`string.format("%s", 整数或布尔)` 在 Luau 下是
  `invalid argument #4 to 'format' (string expected, got integer)`，plain Lua 会自动
  `tostring`。`Tests/Binding/BP_UnLuaTestActor.lua` 的错误拼接因此炸掉，连带 Dynamic /
  Static / Priority 三条绑定用例在 Luau 上失败；已改为显式 `tostring()`
- **Issue362 的 chunk 里类名大小写被改名脚本改坏**：`UE.UUNLUANOVATestStub` 应为
  `UE.UUnLuaNovaTestStub`（改名把 `UnLua` 替换成 `UNLUANOVA` 时把 `UUnLua...` 拼坏了），取到 nil，
  两个后端都失败
- **Issue288 / Issue295 按新类名取 UMG 类**：资产文件改名成了 `UnLuaNovaTestUMG_Issue288`，
  但里面的 `BlueprintGeneratedClass` 仍是 `UnLuaTestUMG_Issue288_C`（改文件名不会改资产里的类），
  `UE.UClass.Load` 因此拿到 nil、`NewObject` 报 `Invalid class!`。已按资产里真实存在的类取
- **Luau 下 int64 属性没法跟脚本里的字面量比较**：Luau 的相等是先比 tag 的
  （`equalobj()` 就是 `ttype(o1) == ttype(o2) && luaV_equalval(...)`），`LUA_TINTEGER` 永远不等于
  `LUA_TNUMBER`，而 Luau 编译器把每个字面量都编成 number。于是 `self.Integer64Property ~= 3`
  对"值就是 3"的属性也成立，对它做算术还会报 `attempt to perform arithmetic (add) on integer
  and number`。现在 `UNLUANOVA_PUSHINT64` 对 |v| ≤ 2^53 的值推 number（double 能精确表示这些整数），
  只有超出 double 精度的才保留 64 位整型 tag —— 需要精确时仍然精确，其余情况回到脚本可用的状态
- **`UNLUANOVA_IS_YIELDABLE` 用例按 plain Lua 的语义断言主线程**：Luau 的
  `lua_isyieldable()` 是 `nCcalls <= baseCcalls`，主线程在没跨过 C 边界时回答"可 yield"，
  所以 `== 0` 在 Luau 上必然失败。现在按后端断言，并额外验证"跨过 C 边界后两边都回答不可 yield"
- **`StringEx` 有两份且行为不一致**：`Content/Script/UnLuaNova/`（工程）和
  `Plugins/UnLuaNova/Content/Script/UnLuaNova/`（插件）各一份，而 package.path 里**工程在前**，
  于是实际加载的是工程那份 —— 它只返回模块表、**不往 `string` 表上装**，所以文档承诺的
  "直接用 `string.split`" 根本不成立，而插件那份（会装）永远轮不到。现在删掉工程侧副本，
  插件版成为唯一实现，并补上文档里写了却没有的 `join`
- **`lua.profiler on` 的日志还在说会顶掉死循环检测**：两者早就都走 `FLuaVmHooks` 分发了，
  这行日志是过期文案，会让人不敢同时开
- **`Tools/CheckLuauTypes.ps1` 在 Windows PowerShell 5.1 下根本跑不起来**：文件是 UTF-8 无 BOM，
  而 5.1 默认按 ANSI（GBK）读 `.ps1`，中文注释里的字节被解成别的字符，直接"字符串缺少终止符"
  语法错（PowerShell 7 读 UTF-8 所以看不出来）。`RunTests.ps1` 同样有中文，只是中文恰好都在
  `<# #>` 注释块里才没炸。两个脚本都补上了 UTF-8 BOM，5.1 和 7 都按 UTF-8 读
- **Commandlet 头文件注释里写了不存在的 `-Types=` 参数**：`Main()` 只解析 `-OutputDir=`，
  类型选择是 opt-out（全部导出，除非被过滤），没有按名字指定的开关。照注释敲会以为筛了三个类型，
  实际生成的是全量
- **`UnLuaNova.Vector` / `UnLuaNova.Buffer` 只存在于示例工程，插件里没有**：文档把
  `UnLuaNova.Buffer.FromArray(...)` 当特性在讲，但这两个模块此前放在
  `Content/Script/UnLuaNova/`（工程），而 `package.path` 工程在前 —— 在本仓库能加载，
  任何只装插件的工程 `require` 直接失败、`UnLuaNova.Buffer` 是 nil，且本仓库没有任何用例会发现。
  现在随插件发布到 `Plugins/UnLuaNova/Content/Script/UnLuaNova/`（与 `StringEx` / `Input` /
  `HotReload` 一致，require 名字不变），并新增 `UnLuaNova.API.RuntimeModules` 断言
  "这些模块的文件在插件目录里"，而不只是"require 得出来"（后者在本仓库无论放哪都通过）

### Fixed
- **`TSet:ToTable()` / `TMap:ToTable()` 每次调用泄漏两块内存**：两者都在 `FMemory::Malloc`
  出来的块上 placement-new 一个**自持有**的 `FLuaArray`（`ToArray()` / `Keys()` 内部
  `new FScriptArray` + `malloc(ElementCache)`），但结尾只 `FMemory::Free` 了那块裸内存，
  从不调用 `~FLuaArray()` —— 而 `FLuaArray` 持有的两个分配**只有**析构函数会还回去。
  现在两处都补上 `->~FLuaArray()`（与 `TArray` 的 `TArray_Delete` 同一套配对），
  并给 `ToArray`/`Keys` 返回 null 的情况加了受控报错（元素接口缺失时原来会直接解引用空指针）
- **死循环看门狗的线程永不退出**：`FDeadLoopCheck` 构造时 `new FRunner()` 起一条每秒轮询的
  后台线程，而 `FRunner::Stop()` 全仓库没有任何调用点，`~FDeadLoopCheck` 也不存在 ——
  于是 `~FLuaEnv` 的 `delete DeadLoopCheck` 只是把壳删了：**每个 Lua 环境留一条线程，
  且那条线程还持有已经析构的 `Env`**（`FGuard::SetTimeout()` 会 `Env->GetMainState()`）。
  默认 `DeadLoopCheck=0`（关闭）时线程照样起，所以测试里每建一个 env 就漏一条。
  现在加了 `~FDeadLoopCheck` → `FRunner::Shutdown()`（`Stop()` + `WaitForCompletion()`）；
  线程对象由 `Shutdown()` 释放，`FRunner` 自己仍由 `Exit()` 的 `delete this` 回收
- **`FClassRegistry::Register(名字)` 会在成功路径上返回空**：`PushMetatable` 有一个
  "元表在 Lua 注册表里但 `Name2Classes` 里没有描述符" 的分支，原来照样 `return true`，
  把 `*OutClassDesc` 留在 null。`GetFieldInternal` 拿到它直接 `->RegisterField()`。
  现在该分支改为落到重新注册的路径上，保证"返回 true 时 `OutClassDesc` 一定非空"；
  调用侧仍补了空检查（受控 Lua error 好过空指针解引用）
- **一批 `FindEnv(L)->` 未判空**：`TryToSetMetatable` 把空检查加在了**下一层**结果上
  （`if (!Registry)`），解引用发生在检查之前；`PushFunction` / `LuaLib_Enum` /
  `LuaLib_Class` / `UnLuaNovaBase` / `PropertyDesc` 同样直接解引用 `FindEnv` 的返回值，
  而它明确会返回 nullptr。现已分别改为判空提前返回或 `FindEnvChecked`（后者与同文件
  既有写法一致，失败是断言而不是静默空指针）
- **`ULuaFunction` 的两处 `Cast<ULuaOverridesClass>(GetOuter())` 未判空后即解引用**：
  `Restore()` 与 `SetActive()` 里都是先 `->GetOwner()` 再判结果 —— 判的是**返回值**，
  不是 `Cast` 的结果。改为复用 `GetOverriddenUClass()`（它本来就做了这个判空）。
  `Override()` 里 `Get(Function)` 同样可能是空，改为先取值再 `check`，断言直接点名真实原因
- **`FLuaRetValues(FLuaEnv*, int32)` 与 `FLuaRetValues(lua_State*, int32)` 行为不一致**：
  前者只初始化 `L`，**不登记返回值**，于是 `Num()` 恒为 0、`Pop()` 永远不弹栈。
  现在改为委托给 `lua_State*` 版本，两个构造函数语义一致。
  五个调用点原本全是 `INDEX_NONE`（所以没爆），且其中 `Call` / `CallTableFunc` 在
  `L == nullptr` 时会把 `FindEnv(nullptr)` 的空结果喂进去、在构造函数里解引用 ——
  现统一改用 `lua_State*` 版本提前返回（`INDEX_NONE` 路径不碰 `L`）
- **模块卸载不摘委托**：`StartupModule` 用 `AddRaw` 注册了 `PostLoadMapWithWorld` 与四个
  PIE/EndPlayMap 回调，但 `ShutdownModule` 只做 `UnregisterSettings()` + `SetActive(false)`。
  Live Coding 重载或编辑器关模块之后，这些委托仍指向已析构的 module 对象。
  现在保存 `FDelegateHandle` 并在 `ShutdownModule` 里逐一 `Remove`（与 `SetActive(false)`
  里对 `OnHandleSystemError` / `OnHandleSystemEnsure` 已有的做法一致）
- **`NotifyUObjectCreated` 未判空**：同一文件 `PostLoadMapWithWorld` 对 `EnvLocator->Locate()`
  判空，这条路径没有，直接 `Env->TryBind()`。已补
- **`FRuntimeEnumParamValue::GetValue()` 解引用 `FindFirstObject<UEnum>` 的空结果**：
  枚举所在包没加载时返回 nullptr，下一行无条件 `->GetValueByIndex()`。
  改为取不到就返回 0 且**不**标记已初始化（下次调用会重试），而不是崩在第一次
  用到这个默认参数的函数上
- **`ReleaseManualRef` 未判 `lua_touserdata` 结果**：同文件紧邻的 `ReleaseSharedPtr`
  对同类取值判了空，这里没有。已补

### Changed
- **`PeekTableElement` 去掉 8 处 `check(true)`**：这是 `TraverseTable` 的 worker 签名，
  但全仓库没有调用点，且函数体里 8 个分支全是 `check(true)` + 一个谁也不读的局部变量
  —— 恒真的假断言，只为消掉未使用变量警告。现在只保留真正有行为的那一支
  （值是非原生 UStruct 的 userdata 时返回 false），行为与原来完全一致。
  顺带说明：key 那一路会对 `lua_next` 正在用的 key 调 `lua_tostring`，在 Lua 5.x 上
  会把 key **就地**转成字符串 —— 一旦这个函数真被当成 worker 挂上去，就会破坏遍历本身

### Reverted
- **PR #719**：其守卫会在子类实例上跳过 Lua 覆写（详见 `Docs/Archive/` 说明），
  需先补「子类实例仍能执行 Lua 覆写」的测试再评估

### Removed
- **`UnLuaNova.Editor.RepairTestAssets`**（`Private/Tests/AssetRepairTest.cpp`）：一次性用例，
  改名留下的资产损伤（未编译的蓝图、丢了的 DataTable RowStruct、从关卡里消失的 actor、
  丢了 struct 的 `MakeStruct` 节点）**已经全部修好并入库**，用例留着只会让人以为每次都要跑它。
  同类损伤万一重现，手工做法写在 `Docs/CN/Testing.md` 的「测试资产」一节

### Known Issues
- **剩余测试失败的分类结论**（Luau 16 条失败、Lua 15 条失败，逐条比对 + 直接读 `.uasset` 二进制得出）：
  - 上一轮修掉的：Issue288 / Issue295（按资产里真实的类名取 UMG 类）、Issue362（chunk 里
    `UE.UUNLUANOVATestStub` 大小写）、Issue291（Luau 缺 `collectgarbage`）、Dynamic / Static /
    Priority 的 `string.format` 与 int64 比较、`UNLUANOVA_IS_YIELDABLE`
  - **资产未编译（3 条，两个后端）**：`Tests/Binding/BP_UnLuaNovaTestActor_StaticBindingChild`
    （Multiple / Overridden）、`Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Child`
    （Issue314）、`Tests/Regression/Issue398/BP_Character`（Issue398）这三个 `.uasset` 里既没有
    `SKEL_*` 也没有 `Default__*`，更没有 `BlueprintGeneratedClass` 导出——是"未编译"状态，
    所以 `Class ..._C` 取不到。同目录的 Parent / 兄弟蓝图都有，说明是落盘时没编译，
    **必须在编辑器里编译并保存**（`BP_Character` 内部还残留旧包路径 `/UnLuaTestSuite/`）
  - **DataTable（3 条，两个后端）**：`Tests/Misc/DataTable_CppTest.uasset` 的 RowStruct 是空的
    （资产很旧，只剩一个 `RowStructure` 属性名，没有任何 `/Script/...Row` 引用），
    需要设成 `UnLuaNovaTestTableRow` 并补数据；`Tests/Regression/Issue583/DataTable_Issue583.uasset`
    引用的是 `/Script/UnLuaNovaTestSuite.Issue583Row`（结构体还在），但同样需要 resave
  - **Issue634**：`Test` 函数图里的 `MakeStruct` 节点丢了 struct（改名后模块里已没有
    `Issue634Struct`），连带的 `NewParam` 参数也丢了类型，蓝图编译不过。已按三级修复：
    struct 填回节点；参数按 `RemoveUserDefinedPinByName` + `CreateUserDefinedPin` 重建
    （签名存在 `UserDefinedPins` 里，直接改 pin 的 `PinSubCategoryObject` 会被下次重建覆盖，
    编译照样说 `had null struct object`）；再把 struct → 返回值的连线接回去。
    修好后用例通过 —— 之前"一修就崩编辑器"是**半修状态**（签名还是错的）造成的，不是插件 bug
  - **"值不回传"（6 条，两个后端一致）**：Issue286 / 300 / 401 / 443 / 453 其实不是运行时问题 ——
    是地图里的 actor 丢了（改名时一起没的），按包路径放回地图后全部通过。
    剩下的 Issue434 是另一种情况（见下）
  - **Issue434（两个后端）**：不是委托绑定坏了。插桩后看得很清楚：`Construct` 跑了、
    `BP_CreateEntry` 拿到了 3 个 entry、`:Add(self, M.OnTestDelegate)` 加了 3 个 handler、
    在 `Construct` 里直接 `Broadcast` 时三个 handler 全部命中（`Result1 == 7`）—— 但控件的
    `Tick` 在 `-NullRHI` 下**根本不会被调用**（Slate 无渲染 → `SObjectWidget::Tick` 不跑），
    委托没人广播，`Result1` 就一直是 0。走真实视口时同一条用例通过。
    已改用 `BEGIN_VIEWPORT_TESTSUITE`（`EAutomationTestFlags::NonNullRHI`）：无头跑时框架把它
    剔除，而不是记一条失败 —— 它没跑，不算通过也不算失败
  - **Luau 独有 1 条**：`创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator`
    （5 个参数版 Translating 的 X/Y 是脏值）已随其余一并修好，当前两个后端均通过
- 尚未在 UE 5.8 上编译验证
- UCLASS 改名需要 resave 资产与迁移 `Config/*.ini`
- Luau 后端未接线：`UNLUANOVA_LUA_BACKEND` 默认 0，`LuaCore` 仍依赖 Lua 5.4 内部头

[Unreleased]: https://github.com/HUSTJJD/UnLuaNova/commits/master

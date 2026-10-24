# Changelog

UnLuaNova 的变更记录。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

> 本项目源自 [Tencent/UnLua](https://github.com/Tencent/UnLua)（MIT）。
> 上游 2019–2023 年的历史变更请见 `Docs/Archive/UnLua_CHANGELOG.md`，或查阅本仓库的 git 历史。

## 这个文件怎么维护

**一条一行，只回答「变了什么」。** 动机、取舍、证据、复现步骤一律写进
[版本说明](Docs/CN/ReleaseNotes.md)（按版本归档）或该功能自己的文档页，不进这里。

理由很实际：变更记录变长的速度比任何人读它的速度都快，一旦长到要滚动，它就只剩
`grep` 一个用途了。而 `grep` 一个 700 行的文件和一个 100 行的文件没有区别 ——
真正会读它的人是翻版本的，那种用法下长条目是纯负担。

规则：

* 一条一行，动词开头；能指到文档或文件就指过去
* 详细内容写进 `Docs/CN/ReleaseNotes.md` 对应版本；功能性的写进功能自己的文档
* 破坏性变更在 `### Changed` 里用 **破坏性** 标出，并写明迁移方式
* 发布时把 `[Unreleased]` 换成版本号与日期，另起一个空的 `[Unreleased]`

## [Unreleased]

### Added

- **按 luau.org 三份文档补齐 Luau 官方特性**：编译优化级别、原生编译（可选 JIT）、GC 旋钮、
  协程级沙盒、`io` 与 `package` 补齐、`profile.out` 导出 ——
  见 [Luau](Docs/CN/Luau.md) 「六、官方特性接入」
- **Lua 声明式网络**：`M.ServerRPC` / `ClientRPC` / `MulticastRPC` 与
  `M:GetLifetimeReplicatedProps()`，含单进程 C/S PIE 与双进程 `RPCId` 一致两项自动测试 ——
  见 [Lua 声明式网络](Docs/CN/LuaNetworking.md)
- **整场录屏支持多窗口**：`RunTests.ps1 -RecordWindow "标签=进程名"` 可重复，报告逐个列出；
  联网 peer 的窗口由 `FNetSession` 写的 `topology.json` 自动识别，不用手填 ——
  见 [测试与自动化](Docs/CN/Testing.md)
- **跨进程网络会话抽象 `FNetSession`**：用例声明拓扑（角色/地图/谁连谁），peer 以 `-game`
  进程起、各写一份 JSON 报告汇合；`...DedicatedServer` 改为两个 `-game` 进程 ——
  见 [测试框架规划](Docs/CN/Testing_Architecture.md)
- **多客户端 Multicast 用例** `UnLuaNova.Slow.Net.Multicast`：1 个 `-game -server` + 2 个客户端进程 ——
  见 [Lua 声明式网络](Docs/CN/LuaNetworking.md)
- **报告新增「联网会话」一节**：列出本轮每个 peer 的角色、有无窗口、报告与日志 ——
  见 [测试与自动化](Docs/CN/Testing.md)
- **DAP 适配器**：`lua.dap start [port]`；`evaluate` 现含 upvalue，并补 `pause` ——
  见 [调试](Docs/CN/Debugging.md)
- **内置库**：`UnLuaNova.Timer` / `Event` / `OOP`，均可插拔（`bTimer` / `bEvent` / `bOOP`）——
  见 [内置库](Docs/CN/StdLib.md)
- **显式 GC 可分片**：`FLuaGcScheduler` + `lua.gc slice/budget/mult/limit/stats`
- **Profiler 按函数聚合**：`lua.profiler agg on|off` / `reset` / `top [n]` / `dump [file]`
- **从 C++ 类型化调用 Lua 成员函数**（`Public/UnLuaNovaOverrider.h`）
- **IntelliSense 首次启动自动生成**（`bAutoGenerateIntelliSenseOnStartup`，默认开）
- **跨项目性能对照用例** `UnLuaNova.Perf.BenchmarkParity`
- **`lua.sandbox.freezeclasstables`**：类表注册即只读，补上"沙盒只硬化全局表、类表仍可被脚本
  改写"这个洞。此前做不了是因为类表懒缓存首次访问会 `rawset`，现在写不进去就跳过 ——
  见 [Luau](Docs/CN/Luau.md) 第六节

- **对标复盘三页文档**：[现状体检](Docs/CN/Benchmark_Review.md)、
  [对标 slua-unreal](Docs/CN/Comparison_sluaunreal.md)、
  [性能对照](Docs/CN/Performance_Parity_slua.md)
- **六个英文页面补齐**：`StdLib` / `LuaNetworking` / `LuaDrivenObjects` /
  `Comparison_sluaunreal` / `Performance_Parity_slua` / `Benchmark_Review`，并进 mkdocs nav；
- **`buffer` ↔ `TArray` 下沉到 C++**：`UnLuaNova.BufferFromArray` / `BufferToArray`，
  `TArray<uint8>` 是一次 memcpy；`UnLuaNova.Buffer` 模块自动优先走它，逐字节的 Lua 版本留作
  plain Lua 后端的回退 —— 见 [Luau](Docs/CN/Luau.md) 第七节
  `EN/Debugging.md` 补 DAP 段、upvalue 与多环境，并纠正"断点是 Luau 专属"的说法
- **协程泄漏检测**：`FLuaCoroutineTracker`（弱键表 + 创建点，快照前先 collect）与
  `lua.coroutines` 人口普查 —— 见 [控制台命令](Docs/CN/ConsoleCommand.md)
- **Lua 内存按来源归因**：Profiler 每个函数记 `NetBytes`，`lua.profiler mem [n]` 出表 ——
  见 [控制台命令](Docs/CN/ConsoleCommand.md)
- **多 Env 调试**：`FLuaEnv::GetAllEnvs()`（按主状态地址排序）、`lua.dbg env [n]`、
  DAP `arguments.env` —— 见 [调试](Docs/CN/Debugging.md)
- **nightly 跑测试**：新增 `.github/workflows/nightly-tests.yml` —— preflight 探测
  `[self-hosted, Windows, UnLuaNova]` 标签的 runner（3 分钟超时），有则跑
  `RunTests.ps1 -NullRHI -NoScreenshots -NoVideo -FailOnDrift`，没有则**故意失败告警**，
  不让"没人跑"伪装成"跑过了"。需要仓库侧配 runner 与可选变量 `UNREAL_ENGINE_PATH`
- **thunk 覆盖扩到 Enum / Text**：`IsThunkableParamKind` 放行这两类（存储形式由 kind
  完全确定），并补上 `FText` 的构造 / 析构与 `FEnumProperty` 的底层数值读写。
  `Struct` / `Array` / `Set` / `Map` 仍走反射：它们的 Lua 表示由 `Private/` 里的
  `FPropertyDesc` 构造，需要一个新的公共编组入口才够得着
- **依赖版本补齐**：`Source/ThirdParty/VERSIONS.md` 记下 Lua `v5.5.1`
  （`7579fc9d`，2026-07-23）与 Luau `0.743-1-g6d36d483`
  （`6d36d483`，2026-10-20）—— 两个都是 submodule，指向即版本
- **文档事实纠错**：`13_AnimNotify` 教程的脚本其实是
  `Content/Script/Tutorials/AN_FootStep.lua`（跟 AnimNotify 类名走，不是 `13_*.lua`）；
  `Private/Perfs/*` 这个目录不存在，性能用例一直在 TestSuite 的 spec 里
- **`HotReload` 补一条用例**：可重复调用且不顺手清掉环境里的全局
- **DAP 适配器用例**（`UnLuaNova.API.DebugAdapter`）：起真服务 + loopback 连接，按 DAP
  帧格式跑完 `initialize` / `attach` / `evaluate` / `threads` / `disconnect`，
  断言 `supportsEvaluateRequest` 与求值结果；另一条断言未知命令回 error 而不是挂死。
  端口用 bind(0) 现取，不与本地已开的 `lua.dap` 抢
- **`FDanglingCheck` 用例**（`UnLuaNova.API.DanglingCheck`）：先定语义再写断言 ——
  它是按调用生效的 guard（调用返回时把首次交给 Lua 的容器/结构体从注册表摘掉），
  不是"扫描并报告数量"；用例断言条目消失，并用"开关关闭时条目保留"作对照
- **性能基线与回归门禁**：每轮落 `Saved/Benchmark/perf-<后端>.json`，与
  `Tools/baseline/perf-<后端>.json` 比对（容差 1.25），`RunTests.ps1 -UpdateBaseline` 更新。
  基线产物尚未提交（需先跑一次），在此之前缺基线只告警不失败 ——
  见 [对标复盘](Docs/CN/Benchmark_Review.md) P0 #2
- **覆盖洞补齐**：新增 `UnLuaNova.API.DebugService`（断点停在声明行、栈逐层展开、
  局部变量、求值错误路径）；加强 `Overrides`（`IsOverridable` 两条分支、`bAddNew`、
  不二次覆写）、`Locators`（默认 `ULuaModuleLocator_ByBinding` 的解析与缓存）、
  `ConsoleCommands`（profiler / memstats / gc / dbg 的可观测效果）
- **`HasFeature` 补两项**：`profilerselftime`、`userdatafinalizer`
- **依赖版本记录** `Source/ThirdParty/VERSIONS.md`（Lua 5.5.1；Luau 精确到 tag/commit 的
  取法写在里面）

### Changed

- **测试载体可配**：`RunTests.ps1 -Carrier Auto|Editor|Game`，`Auto` 按静态分类挑；配套
  `-Tier Fast|All` 与 `-SlowPrefix`（默认 `UnLuaNova.Slow.`）分层 ——
  见 [测试与自动化](Docs/CN/Testing.md)
- **用例静态分类与耗时**：新增 `Tools/ClassifyTests.py`，报告增加「按运行环境分类」一节 ——
  见 [测试框架规划](Docs/CN/Testing_Architecture.md)
- **复制属性改为按类型分桶 + push model + Iris 兼容**（**破坏性**：`FLuaNetPayload` 换成
  十一条 typed UPROPERTY，Lua 侧声明格式不变）——
  见 [Lua 声明式网络](Docs/CN/LuaNetworking.md)
- **对标复盘 P0/P1 落地**
- **配置文件合并成一份** `Config/DefaultUnLuaNova.ini`；插件自带默认值 ini；特性开关统一格式 ——
  见 [设置项](Docs/CN/Settings.md)
- **默认模块定位器**由 `ULuaModuleLocator` 换成 `ULuaModuleLocator_ByBinding`
- **`package.path` 按插件实例拼**，不再硬编码路径；**打包 stage 路径按插件实例枚举**
- **`HasFeature` 改成一张特性表**（`Features[]`，每项一个取值函数）—— 见 [API](Docs/CN/API.md)
- **文档校正**：删掉 `API.md` 里源码中 0 命中的接口
- **CHANGELOG 只做索引**：1.0.0 的逐条详情归档到 [版本说明](Docs/CN/ReleaseNotes.md)，
  `CHANGELOG.md` 每条一行（763 行 → 约 130 行）。顶部写明维护规则：动机、取舍、证据一律进
  文档，不进变更记录

### Fixed

- **Luau 上 userdata 永不回收**：改走 `lua_newuserdatadtor()` 的 VM 级析构，
  并把"是否注册 `__gc`"与"是否会被析构"拆成两个宏 ——
  见 [对标复盘](Docs/CN/Benchmark_Review.md) 1.1
- **Luau 上断点从不命中**：`AddBreakpoint()` 只往表里加记录，而 Luau 的 debug hook 只在
  `LOP_BREAK` 或 single step 时触发；现在有断点时借 `lua_singlestep` 让 hook 按行走，
  再自己匹配 source:line —— 见 [对标复盘](Docs/CN/Benchmark_Review.md) 1.4
- **Profiler 在 Luau 上 self time 恒为 0**：改为采样归因并显式标注 `sampled|measured`，
  可用 `HasFeature('profilerselftime')` 问 —— 见 [对标复盘](Docs/CN/Benchmark_Review.md) 1.2
- **性能数字不可复现 / 测试不会失败**：见上「性能基线与回归门禁」
- **`ToVector` 在 Lua 后端静默返回 nil**：现在打日志并说明用 `HasFeature('vector')` 守

## [1.0.0] - 2026-10-23

详细版本说明：[Docs/CN/ReleaseNotes.md](Docs/CN/ReleaseNotes.md#10---2026-10-23)

### Added

- **文档**：[相对 UnLua：原理与优势](Docs/CN/WhyUnLuaNova.md)、
  [架构](Docs/CN/Architecture.md) 两页（目录结构、双后端抽象、生命周期）
- **三个新 spec 共 30 条用例**（`UnLuaNova.API.ErrorHandling` 等）+ **七个新 spec**
- **性能基线** `UnLuaNova.Perf.Benchmark`：跨语言调用三项、容器三项、GC 一项
- **容器迭代中增删用例**：`TArray` / `TMap` / `TSet` 各两条
- **Luau 后端静态检查改用 `luau-lsp`**（子模块 `Tools/luau-lsp`）
- **静态绑定层** `StaticBinding/` + **`UnLuaNovaStaticBindingGenerator`** Commandlet
- **`CallUEFast`**：对已解析函数跳过 Callspace 查找 / 覆写重定向 / 接口函数查找
- **`UnLuaNovaShowcaseDressing`** Commandlet：给示例关卡补装可拍摄内容
- **Lua 后端抽象** `LuaBackend.h`（`UNLUANOVA_LUA_BACKEND` 开关）；Luau 以 git submodule
  引入（0.743）+ `Luau.Build.cs`
- **Luau 能力落地**、**示例截图与录屏由测试产出**、改名脚本 `Tools/Rename-To-UnLuaNova.ps1`
- `Docs/EN/TestReports.md`：说明两份测试报告是自动生成的中文页及其原因

### Changed

- showcase 剪辑默认抓 30 帧（每帧一次视口回读 + PNG 编码，60 帧是两倍成本）
- **`Tools/*.ps1` 丢掉 UTF-8 BOM 会整轮崩，现在能自愈**（Windows PowerShell 5.1 无 BOM
  按本地代码页读）
- **切后端不再删** `Plugins/UnLuaNova/Intermediate/`；**IntelliSense 产物移到插件自己的目录**
- **测试自己导出声明文件**；修掉 **LuaLS 的假绿灯**（`--check_format` 默认 `pretty`，
  `check.json` 根本不落盘）
- **Luau 声明改成当前 Luau 认得的语法**（`declare class` 已被上游移除）；
  `CheckLuauTypes.ps1` 在喂不进声明时跳过
- `GenerateTestReport.py` 支持 benchmark 对比；`Install-NightlyTask.ps1` 优先用 `pwsh.exe`
- `Testing.md` 新增用例清单表；`EN/API.md` 补上中文有而英文没有的三节；CI 恢复 `mkdocs --strict`
- `TSet` 支持 `pairs` 迭代（Luau 下是 `__iter`）

### Removed

- `Tools/Build-LuauAnalyze.ps1`（改用 luau-lsp 后无人调用）
- `Tools/RunTests.ps1 -BuildOnly`、`Tools/Install-NightlyTask.ps1`、`用例清单基线护栏`
- `.codebuddy/_scan_log.py`（被并发进程误提交的临时脚本）

### Fixed

- **沙盒（`lua.sandbox`）顺带冻结 `UE` / `Lua` / `package.loaded`**，导致类型懒注册与
  `require` 失败；文档把启动期 CVar 写成了可执行命令
- **撤下 Luau 调试 hook 必崩**：`UNLUANOVA_SET_DEBUG_HOOK` 被映射到了 `lua_callhook()`
- **Luau 没有 `collectgarbage`**（基础库只到 `gcinfo()`）；**`%s` 只吃字符串**
- **Luau 下 int64 属性没法跟脚本字面量比较**（相等先比 tag）
- **`TSet:ToTable()` / `TMap:ToTable()` 每次调用泄漏两块内存**
- **死循环看门狗的线程永不退出**；**模块卸载不摘委托**（`StartupModule` 用 `AddRaw`）
- **`FClassRegistry::Register(名字)` 在成功路径上返回空**（`PushMetatable` 的分支错误）
- **`UnLuaNova.Vector` / `UnLuaNova.Buffer` 只存在于示例工程**（插件里没有，文档却写了）
- **`StringEx` 有两份且行为不一致**；**`FLuaRetValues` 两个构造行为不一致**
- 一批空指针：`ULuaFunction` 的 `Cast<>`、`FindEnv(L)->`、`NotifyUObjectCreated`、
  `GetValue()` 解引用 `FindFirstObject<UEnum>` 空结果、`ReleaseManualRef` 未判
  `lua_touserdata`；`UE4File` 的 `Read` / `Write` / `__gc` 用 `lua_touserdata()` 取实例
  拿到块首
- **测试进程崩溃后报告只剩 "0 tests"**：崩溃时自动化框架不导出 JSON，与"没跑起来"无法区分
- `PeekTableElement` 去掉 8 处 `check(true)`；Commandlet 注释里不存在的 `-Types=` 参数
- 合并上游 `develop`（PR #700/#707/#758/#761/#765）与社区 PR
  （#767 #756 #748 #711 #639 #643 #684 #705 #704 #672 #771）

### Reverted

- **PR #719**：其守卫会在子类实例上跳过 Lua 覆写（详见 `Docs/Archive/`）
- **`UnLuaNova.Editor.RepairTestAssets`**：一次性用例，已完成使命

### Known Issues（1.0.0 发布时的状态，详见版本说明）

- 尚未在 UE 5.8 上编译验证
- UCLASS 改名需要 resave 资产并迁移 `Config/*.ini`
- Luau 后端未接线：`UNLUANOVA_LUA_BACKEND` 默认 0，`LuaCore` 仍依赖 Lua 5.4 内部头
  （已在后续提交中接线，见 [Unreleased] 与 [Luau](Docs/CN/Luau.md)）

---
title: "测试与自动化"
description: "单入口 release 验证与失败判定"
---

# 测试与自动化

<!-- release-status: passed -->
<!-- release-provenance: 347bb8c5 -->

维护者只需在仓库根目录运行：

```powershell
.\Tools\RunTests.ps1
```

默认 `Release` 配置自动执行下表，不需要分别切换后端、构建分析器、导出声明、启动网络 peer 或选择测试过滤器。当前固定为 **Win64**，使用项目关联的 UE、已安装的 Visual Studio C++ 工具和 Python。首次执行会联网初始化仓库固定提交的子模块，并在 `Saved/ReleaseTools` 准备 Python 工具；不会安装 UE 或 Visual Studio。

> **Python 需要 3.10 或更高版本。** `python -m venv` 在 3.7–3.9 上会调用当时还不存在的
> `ensurepip --default-pip`，失败时只会给出一个看不懂的子进程错误，后面所有阶段都记成
> `blocked`。流水线现在会先检查版本并直接告诉你版本不够。
> 如果 PATH 上的默认 `python` 太旧，不必改 PATH，用 `UNLUANOVA_PYTHON` 指定一个即可：

```powershell
$env:UNLUANOVA_PYTHON = 'C:\Users\<你>\scoop\apps\python311\current\python.exe'
.\Tools\RunTests.ps1
```

> 另外请用 **PowerShell 7（`pwsh`）** 运行，不要用 Windows PowerShell 5.1（`powershell.exe`）。

## 后端隔离构建（-BackendTargets）

不带开关时，Development 与 Release 的行为都和以前一致：改 ini → 重编 → 跑 → 换后端 →
再重编。加上 `-BackendTargets` 后，**每个后端的 Editor / Game / Server 各编一次，产物按
`Backends/<后端>/` 分开，之后切换只改模块清单，不编译、不覆盖**。这个开关对 Development、
Release、`-Carrier Game`、`-NetMatrix` 都生效。

### 一次性构建

```powershell
# Editor
Build.bat UnLuaNovaDemoLuaEditor  Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
Build.bat UnLuaNovaDemoLuauEditor Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
# Game
Build.bat UnLuaNovaDemoLua        Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
Build.bat UnLuaNovaDemoLuau       Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
# Dedicated Server
Build.bat UnLuaNovaDemoLuaServer  Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
Build.bat UnLuaNovaDemoLuauServer Win64 Development -Project=UnLuaNovaDemo.uproject -WaitMutex
```

只跑 Editor 自动化时编前两条就够；`-Carrier Game`、NetMatrix 或 Release 还需要 Game/Server 两套。

```powershell
# 之后随意切换（只改模块清单 + ini，不编译、不覆盖）
.\Tools\SwitchBackend.ps1 -Backend Lua
.\Tools\SwitchBackend.ps1 -Backend Luau
.\Tools\SwitchBackend.ps1 -Status                 # 看当前哪套生效
.\Tools\SwitchBackend.ps1 -Backend Default       # 回到普通单 target 布局

# Development：Editor 或 Game 载体
.\Tools\RunTests.ps1 -Profile Development -BackendTargets -Backend Both
.\Tools\RunTests.ps1 -Profile Development -BackendTargets -Backend Both -Carrier Game

# 联网矩阵（每个 cell 启真实 server/client 进程）
.\Tools\RunTests.ps1 -Profile Development -NetMatrix -BackendTargets -Backend Both -SkipBuild

# Release 完整 Win64 矩阵（每后端用隔离 target + 独立 stage 目录）
.\Tools\RunTests.ps1 -Profile Release -BackendTargets
```

### 为什么需要六个 target

UBT 给所有 Editor target 同一个 `AppName`（`UnrealEditor`），只加一个 `Target.cs` 会写出
同名的 `UnrealEditor-UnLuaNova.dll` 和同一份 obj；只加一个 `.uproject` 也一样，插件模块的
产物落在插件自己的 `Binaries/`、`Intermediate/`。六个隔离 target 通过每个 `*.Build.cs` 里
完全相同的 `[UAK BACKEND LAYOUT]` 块设置 `ModuleRules.BinariesSubFolder`，把 DLL **和**
中间产物一起重定向到 `Binaries/Win64/Backends/<后端>/` 与 `Intermediate/.../Backends/<后端>/`。

命名契约是 `UnLuaNovaDemo<后端>{Editor,Server,''}`。Editor 是共享构建环境，AppName 恒为
`UnrealEditor`；Game/Server 不是，AppName 就是 target 名，所以它们的 exe、模块 DLL 前缀、
中间目录和 `*.modules` 清单名都跟着变（例如 `UnLuaNovaDemoLua.exe`、
`UnLuaNovaDemoLuaServer.modules`）。`RunTests.ps1` / `ReleasePipeline.ps1` 在隔离模式下按
carrier 使用对应的名字，不会误拿另一个后端的二进制。

多出来的两个隔离 Editor target 让工程同时有三个 `*Editor.Target.cs`。UAT 的
`BuildCookRun` 在无法确定默认编辑器目标时会直接拒绝执行（即使带了 `-skipbuildeditor`），
因此 `Config/DefaultEngine.ini` 必须显式声明
`[/Script/BuildSettings.BuildSettings] DefaultEditorTarget=UnLuaNovaDemoEditor`。
`Tools/tests/test_backend_isolation.py` 把这条约束作为静态回归守着：它不影响隔离模式，
隔离模式的 Editor target 由 `RunTests.ps1` / `ReleasePipeline.ps1` 按后端名字显式指定。

### 为什么需要 SwitchBackend

引擎会递归扫描 `Binaries/**` 找 `*.modules`（`FModuleManager::FindModulePathsInDirectory`）。
两套产物并存时两份清单都会被发现，模块名冲突、加载哪个后端不确定。脚本**动态发现**所有
`*.modules`（不再硬编码 `UnrealEditor.modules`），把非活动后端的清单改名为 `*.disabled`
（引擎不读），并把 `Config/DefaultUnLuaNova.ini` 的 `LuaBackend` 写成当前后端。isolated
target 的后端**由 target 名决定**，ini 只是运行时标签；因此可以在 Luau 生效时预先编 Lua
target。某个 Binaries root 如果没有该后端的隔离产物，脚本会保留它的根清单，避免把模块
整个藏起来。

### 约束与边界

| 项 | 说明 |
|---|---|
| 首次成本 | 六个 target 各一次全量编译；磁盘约为原来的两倍。之后切换零编译 |
| 载体 | Editor / Game / Server 全部隔离。`-BackendTargets` 可与 `-Carrier Game`、`-NetMatrix`、`-Profile Release` 组合 |
| Release | 每后端用隔离 Editor/Game/Server target，UAT 加 `-stagingdirectory` 指向 `Saved/StagedBuilds/{Windows,WindowsServer}/Backends/<后端>`（UAT 每次 `-stage` 会清空目标目录，共用目录会删掉另一个后端的 exe）；缓存指纹含 `isolated`/`targets`，结束把清单复位为 Default 并恢复原 ini |
| 清单 | 编某个 isolated target 时 UBT 会重写该后端的清单。若之后又编了普通 target，根目录会重新出现一份清单，再跑一次 `SwitchBackend.ps1` 即可 |
| 并发 | 切换仍是全局状态，`Saved/RunTests.lock` 继续生效；不要在切换时另开构建/编辑器 |
| 上层工程 | `Tools/unluanova.ps1 doctor` 现在认识 `Backends/<后端>/`、live/disabled 清单与示例工程 ini 标签：两套 DLL 并存不再报错，两个后端清单同时 live 才报错 |

静态契约由 `Tools/tests/test_backend_isolation.py` 守住：布局块逐字节一致、六个 target 存在、
脚本参数贯通，并用合成目录对上层 doctor 做功能回归。

## 默认执行内容

| 阶段 | 自动执行内容 |
|---|---|
| 仓库与工具 | 能力矩阵/文档链接检查、Python 工具回归、PowerShell 语法与进程/参数/超时回归 |
| 分析器 | 初始化 MSVC 环境，从固定子模块构建 LuaLS 和 luau-lsp |
| 每个后端 | Lua、Luau 分别执行，开启 Timer/Event/OOP 和静态绑定 |
| Editor | 备份移出旧静态绑定源码，构建 Editor/生成器，重新生成并再次构建，确保本轮生成代码参与编译 |
| Development | 构建、cook、打包 Game 和 Dedicated Server target 到默认输出路径 |
| Editor 全量测试 | 全部 `UnLuaNova` 注册用例，包括 Slow、VM/绑定/生命周期、扩展、示例截图及性能用例 |
| 跨进程联网 | 本轮同后端 cooked Server/Game 组成一个 DS 和两个客户端；验证两个服务端生成 Actor 的两轮 Lua 字段同步、双向 RPC、Owner 定向投递及 multicast，保留逐端原始观测 |
| 静态类型 | 备份旧生成声明，重新导出；LuaLS 和 luau-lsp 必须实际执行，不能静默跳过 |
| Cooked 全量测试 | 在 Development 包中执行 UE 当前游戏上下文可运行的全部注册用例 |
| 内存 | 两个后端的 Editor 与 cooked Development 均执行 `UnLuaNova.Memory`，逐块核对 VM 释放、资源释放和预热后堆增长；报告缺失或残留超标失败 |
| Shipping | 构建、cook、打包到默认输出路径并检查可执行文件；这是打包门禁，尚不等于 Shipping 运行验收 |
| 收尾 | 恢复配置原始字节及修改过的环境变量，清理本轮残留进程，输出总结果 |

使用 `Saved/RunTests.lock` 防止脚本并发切换后端。测试期间不要在另一编辑器或构建任务中修改同一项目配置/二进制。脚本恢复配置，但最后构建的二进制仍属于最后一个后端。

Release 自动设置 `UNLUANOVA_TEST_PEER_EXE` 和 `UNLUANOVA_TEST_SERVER_EXE`，分别指向默认 staging 路径的当前 Game 和 Server 程序。编译复用 `Binaries/Win64`；Development 打包复用 `Saved/StagedBuilds/Windows` 和 `WindowsServer`，**Shipping 单独落在 `Saved/StagedBuilds/WindowsShipping`**。UAT 每次 `-stage` 都会清空 staging 目录，两个配置共用同一目录时，最后执行的 Shipping 会删掉 Development 的客户端 exe —— 而 `metadata/binary-Development.json` 与联网矩阵 Cooked 变体都还要用它。拆开后一次完整 Release 结束仍留下可用的 Development staged peer。后续后端会覆盖相应输出，不为每轮复制 exe。各后端的 `metadata/binary-*.json` 保存当时的路径、大小及 SHA-256。UE 5.8 普通 Game 二进制不会因 `-server` 参数变成 Dedicated Server。拓扑文件记录每个 peer 的实际可执行文件，清理时同时核对 PID、路径及启动时间。

Release 的 Editor、Game 和非 headless peer 使用 `-RenderOffScreen`，保留真实 RHI、视口用例、单张游戏视口截图，避免依赖桌面窗口是否可见；已移除桌面录屏、连续帧采集及视频编码。DS 仍使用 `-NullRHI`。Development 可显式选择 `-RenderOffScreen`。

Release 测试 Editor 固定使用 `AboveNormal` 进程优先级，设置与 PID 写入 `metadata/process.json`，减少普通优先级后台任务的调度干扰；只调整本轮测试进程。原参考来自 Normal 环境，优先级差异必须保留为测量条件，不能据此声称运行时优化；工作量、采样、基线和阈值不变，仍可能受频率、内存和其他负载影响。

LuaLS 构建在 `Saved/ReleaseTools/luals-temp/` 使用每次独立的临时目录，并在退出时恢复 TMP/TEMP。引导工具和主分析器的构建、测试分别检查退出码，任何一步失败都会阻止分析器阶段通过。

静态绑定备份保存在各后端的 `metadata/static-binding-backup/`，仅处理带生成器标记的固定输出文件；其他源码不参与此流程。若流程结束时未生成新文件，自动恢复旧文件并保留备份。这样旧版生成代码不会阻止新版生成器启动，也不需要手动删除源码。生成器指纹包含格式版本，格式修复可使旧输出失效。

DAP 会话测试使用系统分配的回环端口，由游戏线程逐帧处理非阻塞收发，保留 TCP 分包、粘包和 UTF-8 字节。每步超时 5 秒、会话上限 20 秒，清理时先移除轮询再关闭连接。用例覆盖真实会话、未知命令、分片请求、帧解码、并发停止与重启；两个后端的 Editor 全量测试会自动包含这些回归。

## 结果与失败

总结果在 `Saved/ReleaseTests/test/release-summary.md` 和 `release-summary.json`；加 `-Baseline` 则写到 `Saved/ReleaseTests/baseline/`。结果只保留这两份：本轮的 `test` 与用来对比的 `baseline`，每轮覆盖自己那一份（不再按时间戳分目录）。跑 `test` 时总结果会附一节「与基线对比」，列出基线通过而本轮未通过的阶段，以及哪个载体少了用例或多了失败；对比只报告、不改变本轮判定。

每个阶段记录 `passed`、`cached`、`failed` 或 `blocked`；`cached` 表示该阶段的输入没有变化、直接复用了上一轮产物（见下）。任一失败或依赖阻塞均返回非零退出码。执行中 `complete=false`，不能视为完成。每个独立外部阶段默认最多 60 分钟，可统一通过 `-TimeoutMinutes` 调整。

## 构建产物复用

编译、cook、打包、声明导出和静态检查都是"输入变了才需要重做"的工作，`Tools/ReleaseCache.ps1` 据此复用：每个阶段把自己的输入（源码、资产、项目配置、引擎版本、后端、分析器）算成指纹，连同产物路径、大小、修改时间和 SHA-256 记到 `Saved/ReleaseCache/index.json`。指纹一致且产物仍与记录一致时，阶段记为 `cached` 并跳过；产物被删除、被另一次构建替换，或输入变化时立即失效并重做。

- **测试、证据、报告阶段从不复用**：它们是本轮的证据，不是别人的输入。
- **构建树只有一个**：后端是编译期选择，切换后端会覆盖前一个后端的二进制，所以流水线先跑"磁盘上现在编好的那个"后端（按 `Definitions.UnLuaNova.h` 判断），它这一轮的编译/烘焙/打包全部复用；另一个后端重建，并把构建树留给下一轮。固定顺序等于每轮两次全量构建。
- 声明导出两个后端共用一次；类型检查结果（`*-analyze.json`）由缓存回写到本轮目录，因为证据门禁要读它，"没有文件"是门禁失败而不是跳过。
- `-NoCache` 强制全部重做；删掉 `Saved/ReleaseCache` 同上。`-PruneLegacy` 删除旧的时间戳结果目录（`Saved/ReleaseTests/<yyyyMMdd-HHmmss-fff>`）。

每个后端保留 `editor/<backend>/`、`game/`、二进制元数据和日志。`metadata/manifest.json` 来自实际进程，记录编译后端、能力开关、Editor/cooked 状态及完整测试路径。`ValidateRelease.py` 逐项核对 UE `index.json`，零用例、漏跑、重复、跳过、未知状态、错误信息、错误后端和未注册源码 suite 都不能通过。测试数量和名称基线不再决定 release 完整性。

Editor 证据门禁要求四个 Showcase 的 PNG。cooked 报告也自动收集本轮新生成的单张截图。网络要求 Server/Client1/Client2 的日志、角色、后端、RPCId、不同 PID、实际打开的连接及相同服务器地址。DedicatedServer schema 2 还要求相同服务端生成的 SessionId、两个不同 Owner、两轮 Lua HP 与准确的逐端 RPC 日志；Standalone 或只有声明编号一致的旧报告不能通过。单独运行报告 helper 的空操作不算联网通过。

Editor 局部调试默认使用当前 Editor 程序启动全部 peer，避免混入旧 Game 二进制；Release 通过环境变量显式指定默认 staging 路径的当前 Server/Game。客户端不在本地生成测试 Actor 或设置 Owner。

性能门禁要求 `Tools/baseline/perf-lua.json`、`perf-luau.json`，核对后端、引擎、机器、case 与迭代次数，并拒绝超过 25% 的回退。缺少可比参考值时门禁失败，**不会自动将待测结果提升为基线**。

性能采样对每项至少预热 250 毫秒，再测量五组各至少 100 毫秒的样本。每组重复相同迭代批次，取五组平均每操作耗时的中位数；`metadata/perf.json` 保留原始耗时和批次数，门禁独立重算并拒绝缺失、过短或与汇总不一致的数据。

## 内存泄漏门禁

无需额外命令或手动开启 `lua.memstats`。`metadata/memory/*.json` 保存九类用例：环境反复创建销毁、预热后堆稳定性、真实模块热重载、故意保留引用的对照、原生智能指针载荷、委托、UMG 图片/纹理、先销毁环境及先销毁 World。报告与本轮 manifest 的运行标识绑定，缺文件、错误后端/载体、漏注册、循环数量不足或失败断言均阻止 release 通过。

- 测试环境从 VM 创建前安装逐块分配账本，持续到 `lua_close` 完成；每个环境的残留字节、块数和记账错误必须全部为零。普通环境不维护此指针表。
- 创建销毁场景执行 24 轮，World 场景每种顺序执行 12 轮。原生载荷使用弱指针检查 GC/环境关闭释放，委托和 UMG 使用弱 UObject 指针，Timer 检查回调和捕获对象释放。
- 稳定性和热重载场景预热 8 轮后记录 32 轮完整 GC 的存活字节与块数；允许最多 8 KiB、16 块的预热后增长。schema 2 的 `live_bytes` 来自 VM 的 GC 计数，`bytes` 保留分配账本字节，`blocks` 保留分配块数。Luau 的分配块是内存页，可能正常保留约 32 KiB 的空闲空间，不能当作存活对象增长；环境关闭时仍要求整个分配账本归零。阈值在代码和验证器中固定，不接受报告自行放宽。
- 对照用例保留一组对象，要求完整 GC 后仍能检测到至少 64 KiB 的增长，再解除引用确认回落。验证器独立检查采样，不能只填写 `passed=true`。

这些是指定场景的泄漏回归，不是全进程内存安全证明。原生资源检查尚不等于覆盖每一处 C++ 分配；GPU/RHI、Luau CodeGen 可执行页、第三方库和长时间线上工作负载仍需专项检测。进程工作集未下降也不能单独判为泄漏。

## 静态发布门禁（无需 UE）

下面这些只检查仓库里的文件与 JSON，不编译、不启动 UE，可以在冻结工作树之前先跑：

```powershell
python -B Tools/AuditRepository.py --write-matrix
python -B -m unittest discover -s Tools/tests -p "test_*.py"
python -B Tools/ValidateProvenance.py --run Saved\ReleaseTests\baseline --summary Saved\ReleaseTests\baseline\release-summary.json
python -B Tools/GenerateNotices.py --check Saved\StagedBuilds\Windows\NOTICES.txt Saved\StagedBuilds\WindowsServer\NOTICES.txt
```

## 开发调试

局部测试必须明确指定 `Development`，其结果不能作为 release 总结：

```powershell
.\Tools\RunTests.ps1 -Profile Development -Backend Luau -Filter UnLuaNova.API.Timer
.\Tools\RunTests.ps1 -Profile Development -Tier Fast
.\Tools\RunTests.ps1 -Profile Development -Filter UnLuaNova.Showcase -Backend Luau -Publish
```

`-SkipBuild`、`-BuildOnly`、`-NullRHI`、`-NoReport`、单后端和自定义 filter 均限 Development。release 拒绝这些降低覆盖范围的选项。旧的 nightly build 注册器也使用 Development，仅检查编译，不代表 release 验收。

### 联网矩阵（独立 opt-in）

八类连接场景（晚加入、进程级重连、dormancy、relevancy、加入顺序互换、丢包/乱序、
`UNetConnection` 级重连、跨 peer 声明差异）与 Legacy / PushOff / Iris / Cooked
四个变体各起三个真实进程，不进普通测试矩阵，也不属于常规 Release 阶段：

```powershell
.\Tools\RunTests.ps1 -Profile Development -NetMatrix -Backend Lua -SkipBuild
python Tools\ValidateRelease.py --net-matrix --run Saved\ReleaseTests\net-matrix\Lua --backend Lua --carrier game
```

底层脚本是 `Tools/RunNetMatrix.ps1`；`-ExtraArgs` 把 `-UnLuaNovaNetScenarios`、
`-UnLuaNovaNetPktLoss=25` 和变体的真实引擎开关（`-UseIrisReplication=1` 或 push-model cvar 的
ini 覆盖）传到进程命令行，`FNetSession` 再把场景开关、丢包率、精确重连 URL 与
`UNLUANOVA_TEST_NET_VARIANT` 转发给每个 peer。

**载体按变体分开**：`AppendWorldFacts` 把编译期 `WITH_EDITOR` 写成 peer 报告的 `Cooked` 字段，
门禁据此要求 Legacy / PushOff / Iris 与八个场景用 **Editor 载体**（`Cooked=false`），只有 Cooked
变体用 **Game 载体 + staged 打包 peer**（`Cooked=true`）。所有 cell 统一跑 `-Carrier Game` 会让
前三个变体也报 `Cooked=true`，门禁必然失败。Cooked 变体需要 `Saved/StagedBuilds/Windows`
里的 **Development** 客户端 exe（Shipping 包裁掉了 automation peer 测试，不能替代），以及
`WindowsServer` 里的 Development 服务器 exe；一次完整 Release 现在会保留这两个产物。

每个 cell 跑的是筛选后的单条联网用例，判定依据是拓扑、三份 peer 报告与日志，不是全量用例清单；
因此 cell 使用 `-NoReport`，不与 `Tools/baseline/*.json` 做用例集漂移比较。UE 进程退出码、
`Copy-Tree` 的拓扑存在性检查与门禁仍然 fail-closed。`UNLUANOVA_TEST_NET_ROOT` 指向项目外的
共享临时目录，避免 Development `-game` 把项目内绝对路径重映射到各自的 cooked root。

`AppendWorldFacts` 从 cvar、`WITH_EDITOR` 与 `UNetConnection`
读回状态写进报告，`BuildNetDeclarationReport()` 导出真实 FProperty 参数规格与
`RepSchemaVersion` / `VariantRepSchemaVersion`，因此改目录名冒充不了变体、声明不一致也混不过去。产物落在
`net/scenarios/<Name>/` 与 `net/variants/<Name>/DedicatedServer/`，由 `metadata/net-matrix-gate.json`
判定；它**不是** UE automation evidence，也不写 `Tools/evidence/index.json`。
该门禁 fail-closed：缺拓扑、缺 peer 日志、PID 不符、伪造 Iris/push-model/Cooked 标志、
声明指纹不一致或不可区分、缺 Generic 字段 / UStruct 参数、连接字节计数为零、场景没跑，都会失败。

只看默认计划、不修改配置、不安装工具、不启动编译或 UE：

```powershell
.\Tools\RunTests.ps1 -DryRun
```

把这一轮写成以后用来对比的基线（写到 `Saved/ReleaseTests/baseline`）：

```powershell
.\Tools\RunTests.ps1 -Baseline
```

不用缓存、全部重新构建与打包：

```powershell
.\Tools\RunTests.ps1 -NoCache
```

## 覆盖边界

新增 C++ Spec / 回归用例应在 `Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs` 或 `Tests` 注册 `UnLuaNova.*` 路径。`ClassifyTests.py` 提供源码 suite 清单，运行时 manifest 提供当前上下文的 leaf 清单；新增注册宏时需同步分类器，不能添加静默排除规则。Lua 侧多个断言仍可能汇总成一个 UE 用例。

此入口覆盖当前定义的 Win64 测试矩阵，不等于所有产品需求已实现，也不是 100% 代码覆盖率。feature-off 组合、其他目标平台/设备、Shipping 运行 smoke、Iris/push model 和外部项目性能对照需单独验收。

静态检查保留所有警告和分析器原始 `analyzer_exit`；最终 `exit` 依据错误级诊断判定。仅警告不阻断，类型错误、异常退出和缺少结果均失败。

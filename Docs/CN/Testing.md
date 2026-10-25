---
title: "测试与自动化"
description: "单入口 release 验证、证据与失败判定"
---

# 测试与自动化

维护者只需在仓库根目录运行：

```powershell
.\Tools\RunTests.ps1
```

默认 `Release` 配置自动执行下表，不需要分别切换后端、构建分析器、导出声明、启动网络 peer 或选择测试过滤器。当前固定为 **Win64**，使用项目关联的 UE、已安装的 Visual Studio C++ 工具和 Python。首次执行会联网初始化仓库固定提交的子模块，并在 `Saved/ReleaseTools` 准备 Python 工具；不会安装 UE 或 Visual Studio。

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

Release 自动设置 `UNLUANOVA_TEST_PEER_EXE` 和 `UNLUANOVA_TEST_SERVER_EXE`，分别指向默认 staging 路径的当前 Game 和 Server 程序。编译复用 `Binaries/Win64`，打包复用 `Saved/StagedBuilds/Windows` 和 `WindowsServer`；后续后端及 Shipping 会覆盖相应输出，不为每轮复制 exe。各后端的 `metadata/binary-*.json` 保存当时的路径、大小及 SHA-256。UE 5.8 普通 Game 二进制不会因 `-server` 参数变成 Dedicated Server。拓扑文件记录每个 peer 的实际可执行文件，清理时同时核对 PID、路径及启动时间。

Release 的 Editor、Game 和非 headless peer 使用 `-RenderOffScreen`，保留真实 RHI、视口用例、单张游戏视口截图，避免依赖桌面窗口是否可见；已移除桌面录屏、连续帧采集及视频编码。DS 仍使用 `-NullRHI`。Development 可显式选择 `-RenderOffScreen`。

Release 测试 Editor 固定使用 `AboveNormal` 进程优先级，设置与 PID 写入 `metadata/process.json`，减少普通优先级后台任务的调度干扰；只调整本轮测试进程。原参考来自 Normal 环境，优先级差异必须保留为测量条件，不能据此声称运行时优化；工作量、采样、基线和阈值不变，仍可能受频率、内存和其他负载影响。

LuaLS 构建在 `Saved/ReleaseTools/luals-temp/` 使用每次独立的临时目录，并在退出时恢复 TMP/TEMP。引导工具和主分析器的构建、测试分别检查退出码，任何一步失败都会阻止分析器阶段通过。

静态绑定备份保存在各后端的 `metadata/static-binding-backup/`，仅处理带生成器标记的固定输出文件；其他源码不参与此流程。若流程结束时未生成新文件，自动恢复旧文件并保留备份。这样旧版生成代码不会阻止新版生成器启动，也不需要手动删除源码。生成器指纹包含格式版本，格式修复可使旧输出失效。

DAP 会话测试使用系统分配的回环端口，由游戏线程逐帧处理非阻塞收发，保留 TCP 分包、粘包和 UTF-8 字节。每步超时 5 秒、会话上限 20 秒，清理时先移除轮询再关闭连接。用例覆盖真实会话、未知命令、分片请求、帧解码、并发停止与重启；两个后端的 Editor 全量测试会自动包含这些回归。

## 结果与失败

总结果在 `Saved/ReleaseTests/<时间>/release-summary.md` 和 `release-summary.json`。每个阶段记录 `passed`、`failed` 或 `blocked`；任一失败或依赖阻塞均返回非零退出码。执行中 `complete=false`，不能视为完成。每个独立外部阶段默认最多 60 分钟，可统一通过 `-TimeoutMinutes` 调整。

每个后端保留 `editor/<backend>/`、`game/`、二进制元数据和日志。`metadata/manifest.json` 来自实际进程，记录编译后端、能力开关、Editor/cooked 状态及完整测试路径。`ValidateRelease.py` 逐项核对 UE `index.json`，零用例、漏跑、重复、跳过、未知状态、错误信息、错误后端和未注册源码 suite 都不能通过。测试数量和名称基线不再决定 release 完整性。

Editor 证据门禁要求四个 Showcase 的 PNG。cooked 报告也自动收集本轮新生成的单张截图。网络要求 Server/Client1/Client2 的日志、角色、后端、RPCId、不同 PID、实际打开的连接及相同服务器地址。DedicatedServer schema 2 还要求相同服务端生成的 SessionId、两个不同 Owner、两轮 Lua HP 与准确的逐端 RPC 日志；Standalone 或只有声明编号一致的旧报告不能通过。单独运行报告 helper 的空操作不算联网通过。

Editor 局部调试默认使用当前 Editor 程序启动全部 peer，避免混入旧 Game 二进制；Release 通过环境变量显式指定默认 staging 路径的当前 Server/Game。客户端不在本地生成测试 Actor 或设置 Owner。

性能门禁要求 `Tools/baseline/perf-lua.json`、`perf-luau.json`，核对后端、引擎、机器、case 与迭代次数，并拒绝超过 25% 的回退。当前参考值来自独立的 `FullFix-Complete-11` 诊断轮，来源见基线目录 README；它们不是修复前基线，不能证明修复带来了性能提升。缺少可比参考值时门禁失败，**不会自动将待测结果提升为基线**。对外性能结论仍须遵守 [性能验收](Performance_Parity_slua.md)。

性能采样对每项至少预热 250 毫秒，再测量五组各至少 100 毫秒的样本。每组重复相同迭代批次，取五组平均每操作耗时的中位数；`metadata/perf.json` 保留原始耗时和批次数，门禁独立重算并拒绝缺失、过短或与汇总不一致的数据。原参考文件保持不变，来自旧的单批采样方法；这能按相同工作内容、每批迭代数和单位继续检查原门槛，不能据采样方法变化声称运行时性能提升。

## 内存泄漏门禁

无需额外命令或手动开启 `lua.memstats`。`metadata/memory/*.json` 保存九类用例：环境反复创建销毁、预热后堆稳定性、真实模块热重载、故意保留引用的对照、原生智能指针载荷、委托、UMG 图片/纹理、先销毁环境及先销毁 World。报告与本轮 manifest 的运行标识绑定，缺文件、错误后端/载体、漏注册、循环数量不足或失败断言均阻止 release 通过。

- 测试环境从 VM 创建前安装逐块分配账本，持续到 `lua_close` 完成；每个环境的残留字节、块数和记账错误必须全部为零。普通环境不维护此指针表。
- 创建销毁场景执行 24 轮，World 场景每种顺序执行 12 轮。原生载荷使用弱指针检查 GC/环境关闭释放，委托和 UMG 使用弱 UObject 指针，Timer 检查回调和捕获对象释放。
- 稳定性和热重载场景预热 8 轮后记录 32 轮完整 GC 的存活字节与块数；允许最多 8 KiB、16 块的预热后增长。schema 2 的 `live_bytes` 来自 VM 的 GC 计数，`bytes` 保留分配账本字节，`blocks` 保留分配块数。Luau 的分配块是内存页，可能正常保留约 32 KiB 的空闲空间，不能当作存活对象增长；环境关闭时仍要求整个分配账本归零。阈值在代码和验证器中固定，不接受报告自行放宽。
- 对照用例保留一组对象，要求完整 GC 后仍能检测到至少 64 KiB 的增长，再解除引用确认回落。验证器独立检查采样，不能只填写 `passed=true`。

这些是指定场景的泄漏回归，不是全进程内存安全证明。原生资源检查尚不等于覆盖每一处 C++ 分配；GPU/RHI、Luau CodeGen 可执行页、第三方库和长时间线上工作负载仍需专项检测。进程工作集未下降也不能单独判为泄漏。`FullFix-Release-22` 已通过双后端 Editor/cooked 四组实际运行与证据校验，共 36 份内存报告，关闭后残留及记账错误均为零。

## 开发调试

局部测试必须明确指定 `Development`，其结果不能作为 release 总结：

```powershell
.\Tools\RunTests.ps1 -Profile Development -Backend Luau -Filter UnLuaNova.API.Timer
.\Tools\RunTests.ps1 -Profile Development -Tier Fast
.\Tools\RunTests.ps1 -Profile Development -Filter UnLuaNova.Showcase -Backend Luau -Publish
```

`-SkipBuild`、`-BuildOnly`、`-NullRHI`、`-NoReport`、单后端和自定义 filter 均限 Development。release 拒绝这些降低覆盖范围的选项。旧的 nightly build 注册器也使用 Development，仅检查编译，不代表 release 验收。

只看默认计划、不修改配置、不安装工具、不启动编译或 UE：

```powershell
.\Tools\RunTests.ps1 -DryRun
```

## 覆盖边界

新增 C++ Spec / 回归用例应在 `Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs` 或 `Tests` 注册 `UnLuaNova.*` 路径。`ClassifyTests.py` 提供源码 suite 清单，运行时 manifest 提供当前上下文的 leaf 清单；新增注册宏时需同步分类器，不能添加静默排除规则。Lua 侧多个断言仍可能汇总成一个 UE 用例。

此入口覆盖当前定义的 Win64 测试矩阵，不等于所有产品需求已实现，也不是 100% 代码覆盖率。feature-off 组合、其他目标平台/设备、Shipping 运行 smoke、Iris/push model 和外部项目性能对照仍是 [审计](Benchmark_Review.md) 中的未完成工作。许可归属与第三方 notice 也不能由自动化通过替代。

验证状态：2026-10-25 的 `ConnectedNet-Release-28` 完整通过，33/33 阶段、2482/2482 次 UE 执行、0 失败、0 跳过，四组真实 DS/双客户端及证据门禁均通过，两后端 Shipping 打包成功。40 项 Python 工具测试和 PowerShell 支持检查通过。静态检查零错误，保留 Lua 298 条、Luau 7 条警告。LuaLS 使用已记录的夹具初始化补丁，其 bee 依赖仍有两项上游固定跳过。当前默认输出为 Luau，按默认目录覆盖复用。Release22 的旧 DedicatedServer 不构成传输证据；历史失败和验收边界见下方回归记录。

本次修复和运行证据见 [2026-10-24 回归记录](Regression_20261024.md)。静态检查保留所有警告和分析器原始 `analyzer_exit`；最终 `exit` 依据错误级诊断判定。仅警告不阻断，类型错误、异常退出和缺少结果均失败。

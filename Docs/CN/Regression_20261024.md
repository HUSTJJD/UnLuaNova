---
title: "2026-10-24 回归修复与验证"
description: "静态绑定崩溃、双后端兼容和完整测试验证记录"
---

# 2026-10-24 回归修复与验证

## 验收状态

2026-10-25 完整运行 `FullFix-Release-22` 已通过：`release-summary.json` 为 `complete=true`、`passed=true`，33/33 个阶段通过，进程退出码 0。四组 UE 自动化合计 **2478/2478 项通过，0 失败、0 跳过**。下表均来自同一次完整运行，不拼接历史诊断结果。

| 后端 | Editor | cooked Development |
|---|---:|---:|
| Lua | 625/625 | 599/599 |
| Luau | 640/640 | 614/614 |

其中带警告通过的 UE 用例依次为 Lua Editor 14、Lua cooked 6、Luau Editor 18、Luau cooked 10 项，共 48 项；其状态均为 `Success`。全部通过不代表没有日志警告。

环境：Windows 11、UE 5.8.3 源码引擎、Win64。UE 目标使用 MSVC 14.50，分析器使用 MSVC 14.51。分别验证 Lua 5.5 与 Luau。局部 Development 结果不能代替 Release 验收。

运行时间为北京时间 2026-10-25 08:07:51 至 09:06:45。两后端 Development Game/Server 构建、打包、归档及 Shipping 打包均通过；四组严格证据门禁全部通过。已发布 [Lua Editor 完整报告](../test-lua-report.md) 和 [Luau Editor 完整报告](../test-luau-report.md)，cooked 完整报告保留在本地运行目录。

## 最终证据

| 检查 | Release22 结果 |
|---|---|
| 内存与资源 | 每组 9 份，共 36 份报告；环境关闭后残留字节、分配块、资源和记账错误均为 0，稳定性及保留引用对照通过 |
| 跨进程联网 | 四组 DedicatedServer 与 multicast 均通过；每次 multicast 到达两个客户端，RPC ID 1/2/3 和 14 项 NetFields 一致 |
| 静态类型 | Lua：92 个文件、0 错误、298 警告；Luau：7 个文件、0 错误、7 警告。分析器原始退出码均为 1，解析后 checker 判定均为 0，警告保留 |
| 工具回归 | 34 项 Python 测试及 PowerShell 支持检查通过 |
| 文档与交付检查 | MkDocs 严格构建、40 组能力/本地链接审计及 `git diff --check` 通过；发布目录现有 10 段视频均解码通过，其中 8 段为本轮 Editor Showcase |
| 截图与视频 | 四组各 4 张主截图、4 段 Showcase 视频，共 16 张、16 段；全部视频完整解码通过，各组代表截图已目视检查 |
| 性能 | 原独立基线和 25% 门槛保持不变，全部通过；Lua Editor `VM.Call` 为 0.0273270003，Luau Editor `VM.TableGrow` 为 0.0143017428、`VM.TableRead` 为 0.00807557255 微秒/操作 |

证据根目录为 `Saved/ReleaseTests/FullFix-Release-22/`：

- `release-summary.md`、`release-summary.json`：33 阶段结论、四组报告入口、源码差异哈希、未跟踪文件哈希、工具链及子模块版本；`metadata/untracked-source/` 保存本次未跟踪源码快照。
- `lua/editor/lua/`、`lua/game/`、`luau/editor/luau/`、`luau/game/`：各自的 `report.md`、UE `index.json`、`metadata/manifest.json` 和 `metadata/release-gate.json`。
- 各运行目录的 `metadata/perf.json`、`metadata/memory/`、`net/`、`screenshots/`、`videos/`：原始性能样本、内存、网络 peer 报告与日志、图片和视频。
- 各后端的 `packages/Development/`、`packages/Server/`、`packages/Shipping/`：本轮归档；根目录 `logs/` 保留构建、打包、分析器和工具检查日志，Python 测试输出在 `logs/python-tests.log.stderr`。

最终报告发布和文档收尾发生在测试完成之后；运行来源快照保留测试启动时的内容。脚本已恢复配置，最后构建的二进制属于 Luau。

## 修复原因与行为

| 问题 | 原因及处理 |
|---|---|
| 首个 Base 测试访问违规 | 静态绑定解析会写入描述符，生成器此前将其声明为 const；统一可变描述符契约并重新生成、编译 |
| CodeGen 控制台注册冲突 | 开关和命令同名；开关改为 `lua.codegen.enabled`，命令保留 |
| 热重载加载器参数错误 | `_ENV` 定位不应覆盖任意首个 upvalue；保留 searcher loader data 和 Start 的显式参数 |
| Luau 热重载遍历失败 | Luau 不做尾调用消除，递归检查调用栈会继续增长；使用迭代遍历，只记录可递归对象，避免 NaN 表键 |
| 重复整体模块替换 | 同步 `loaded_modules` 和 `package.loaded`，避免后续 reload 仍使用初次模块 |
| GC 模式切换后无进展 | 进入分帧回收时将现有堆计入调度，并在未完成周期中保留最低工作债务 |
| 默认 Luau GC 参数误报警 | 推荐上界本身合法，修正边界条件和说明 |
| 短类名定位配置失效 | FSoftClassPath 无法保存裸类名，增加 `ClassName` 配置项，完整对象路径继续使用 `Class` |
| 动态 RPC 注册不完整 | 动态函数需要稳定的原始声明生命期、根函数关系及 NetFields；Lua 自声明处理函数使用原名 |
| PIE 网络测试崩溃 | 客户端连接期间会替换 World，等待 Actor 时重新获取当前客户端 World |
| 跨进程 multicast 时序 | 连接建立不等于 Actor 已可用；客户端报告就绪后发送，服务器等待接收报告再退出 |
| 跨进程报告偶发读取失败 | 文件存在时 JSON 可能仍在写入；报告与拓扑先写同目录临时文件，关闭后再发布，等待阶段必须成功解析 JSON。新增 `UnLuaNova.API.Net.ReportReadiness` 覆盖临时文件、不完整 JSON 与完整报告 |
| 跨进程字段缓存不一致 | 服务器缓存 14 项、客户端缓存 11 项，客户端在 Actor 创建前已缓存未声明 RPC 的类；网络类须连接前预绑定，并在引擎初始化完成后重查配置类 |
| 静态属性返回 nil | 委托和容器等类型需要专用描述器；静态描述器仅接管明确支持的类型，其余走原有反射路径 |
| 静态绑定用例影响后续测试 | 用例结束后恢复被替换的生成绑定表，避免运行顺序隐藏真实访问路径 |
| 报告性能数据串轮 | 性能结果写入本轮报告目录，生成报告时不再读取共享 latest 文件；增加内存资源采样汇总 |
| 蓝图声明缺失 | UE 5.1+ 完整资产类路径被误与短名比较，改为比较完整类路径 |
| 全量烹饪蓝图报错 | 早期模块重命名后保存资产丢失了原生引用；从历史完整图表恢复 3 个 Benchmark 蓝图、Issue494 蓝图/关卡和 Issue500 关卡，配置精确重定向后由 UE 重保存 |
| cooked 地图测试未切图 | 游戏载体原来只等待任意 World；改为 OpenLevel 后等待目标 World 完成 BeginPlay，并恢复测试所需的 Lua 环境 |
| cooked 容量与模板断言 | Reserve 验证最小容量及添加元素时不重分配；编辑器模板文件检查仅注册到编辑器 |
| 地图卸载后的结构体数组析构崩溃 | 属性拥有者存活不代表元素 UScriptStruct 存活；描述器持有类型引用，并使用保存的类型直接析构值。新增 Lua API 用例强制 UE GC 后读取、清理结构体数组，再验证环境关闭后类型可回收 |
| 打包服务器角色错误 | UE 5.8 Game 二进制不支持通过 -server 变成 Dedicated Server；独立打包 Server，并按角色选择可执行文件 |
| Shipping 条件编译失败 | DanglingCheck 与跨进程网络测试缺少自动化编译条件；按 `WITH_DEV_AUTOMATION_TESTS` 包围测试实现。Profiler 的 Insights 调用按 `CPUPROFILERTRACE_ENABLED` 编译，保留不依赖 CPU Trace 的统计与采样逻辑 |
| Release 测试未启动 | 引擎 ExecCmds 用逗号分隔命令，Automation 内部链使用分号；manifest 导出和测试启动使用各自正确分隔符 |
| Native 性能数值失真及短时采样波动 | 防止优化器消除空循环；至少预热 250 毫秒，再记录五组各至少 100 毫秒样本，按批次归一化取中位数；门禁独立核对原始样本 |
| 测试夹具问题 | 补齐显式导出和枚举，修正模块环境、隐式 self、临时 UFunction 命名及预期错误登记 |

## 结构体容器的生命周期

地图卸载后，Lua 持有的结构体数组可能继续存活。原先析构路径通过属性描述器访问元素类型，但属性拥有者仍有效并不能证明其引用的 `UScriptStruct` 仍有效。诊断中的调用栈为 `lua_close -> FLuaArray::~FLuaArray -> FPropertyDesc::Destruct -> FStructProperty::DestroyValueInternal`。

现在 `FScriptStructPropertyDesc` 使用 `FObjectReferencer` 保持类型存活，并保存类型指针和 `ArrayDim`；销毁元素时直接调用该类型的 `DestroyStruct`，不再依赖可能已经失效的声明属性。保活范围是描述器生命周期，因此清空一个数组不保证立即释放类型；环境中的描述器缓存仍可能需要它。

`UnLuaNova.Regression.StructContainerLifetime` 创建含字符串字段的临时结构体，通过公开 Lua API 构造数组，移除 Lua 类型变量后强制 UE GC，再读取并清理数组。最后关闭独立 Lua 环境并再次执行 UE GC，检查类型的弱指针已经失效。这同时覆盖容器可安全析构和保活引用最终释放。

## 静态检查判定

类型错误、分析器异常退出或缺少有效结果均失败。已解析的警告不阻断；摘要的 `exit` 是脚本判定，`analyzer_exit` 是分析器原始退出码，两者均保留。

Luau 的普通模块名使用运行时 package.path 规则，例如 `Luau.Index`。luau-lsp 的原生解析规则不同；脚本仅对确认能在运行时脚本目录找到的模块降级为警告。此限制意味着跨这些 require 边界的类型推断不完整，不能把零类型错误理解为完整静态类型证明。

## 诊断历史

已完成的诊断阶段：`FullFix-Luau-02` 全量 Editor 638 项中 621 成功、17 失败；`FullFix-Luau-03` 局部 529 项中 527 成功、2 失败。它们用于定位问题，均不是最终通过报告。原始静态绑定崩溃和 Issue562 的双后端局部验证在 `Saved/TestReports/StaticBindingCrash-Fix`。

工具回归：最终 34 项 Python 测试和 PowerShell 支持测试已通过。LuaLS 引导依赖曾在系统临时目录下出现 7 项文件系统/Unix socket 失败；使用工作区内独立临时目录后，同一套测试为 110 通过、0 失败、2 跳过。两项跳过是上游固定禁用的 Lua 栈溢出用例，本次未修改其测试列表。构建脚本隔离并恢复 TMP/TEMP，分别检查引导和主构建退出码，避免官方批处理后续成功覆盖引导失败。

完整 Development 诊断轮 `FullFix-Complete-11`：Lua 为 622/623 通过，唯一失败是调用栈夹具中的尾调用消除了预期帧；夹具已改为显式保存返回值。Luau 为 638/638 通过，含修正后的夹具。两者静态检查均为 0 错误，分别有 298、7 条警告，跨进程 multicast 均到达两个客户端。该轮仅覆盖 Editor，不能代替 cooked 与打包验证。

Release 分析器构建使用独立的 `Saved/ReleaseTools/luau-lsp-ninja` 目录及显式 Ninja 生成器，避免固定版本 CMake 与已有 Visual Studio 生成器缓存冲突。

资产恢复后独立完整 cook 为 732/732、0 错误、0 警告。Lua DebugService 独立复测 8/8 通过。`FullFix-LuaCooked-16/regression` 为 56/57 通过，唯一失败是地图对象路径与包路径比较导致超时；结构体析构及新增类型生命期用例已通过。上述均为诊断证据，不能代替最终完整 Release。

修复后的 `FullFix-LuauCooked-18/regression` 为 59/59 通过；其全量预跑为 611/613 通过，剩余两项因 Server 包尚未生成而失败。四组截图、视频已生成。该手动预跑未设置 Release token，严格内存证据关联校验未通过，且与编译并行执行，不能用于最终内存或性能验收。

`FullFix-Release-17` 的 Luau cooked 正式阶段为 **613/613 通过、0 跳过**，运行清单、九项内存报告及真实 DedicatedServer/Multicast 证据校验均通过；报告补齐四张截图和四段视频。Luau Editor 为 638/639 通过，唯一失败为 `VM.TableGrow`：0.021013 微秒/操作，相比独立基线 0.015414 增加 36.3%，超过 25% 门槛。基线保持不变，须进一步复测判断稳定性。该轮 Lua 因当时尚未修复的测试链接问题未运行，且部分修复发生在来源快照之后，因此本轮不能作为最终完整验收。

该轮 Shipping 首次源码编译暴露了测试实现及 CPU Trace 调用缺少条件编译保护的问题，修复方式见上表。编译退出后，`FullFix-Perf-18b/18c/18d` 三次独立 Editor 性能诊断的 `TableGrow` 分别为 0.020612、0.015199、0.014035 微秒/操作，第一轮超限，后两轮全部性能项通过。这说明当前测量存在波动，不能据此断言存在稳定退化或性能提升；原基线、测量逻辑和门槛均保持不变，最终仍要求完整 Release 通过。

`FullFix-Shipping-18` 已完成修复后的 Luau Shipping 增量编译、链接、cook、打包与归档，UAT 返回 0。`FullFix-Release-19` 中 Lua Editor 624/624、Lua cooked 598/598、Luau Editor 639/639 均通过，Luau 的 `TableGrow` 为 0.017375 微秒/操作，低于原基线的 25% 门槛。Luau cooked 为 612/613，唯一失败是 DedicatedServer 比较时无法读取服务器报告；连接式 multicast 通过。此次发现报告存在与 JSON 可读之间的竞争窗口，按上表修复并新增回归。该失败轮保留，修复后的完整复跑使用 `FullFix-Release-20`。

`FullFix-Release-20` 完整执行了 33 个阶段：Lua Editor 624/625、Lua cooked 599/599、Luau Editor 640/640、Luau cooked 614/614；无跳过，两后端 Shipping 均成功。唯一失败为 Lua Editor 的 `VM.Call`：0.033788 微秒/操作，相比独立基线 0.026266 增加 28.6%，超过 25% 门槛。运行阶段与对应证据校验均正确报失败，因此总结果仍为 `complete=true`、`passed=false`。网络报告就绪回归、DedicatedServer 和 multicast 在四组运行中均通过。所有原始结果保留，继续单独复测性能后再做完整验收。

随后 `FullFix-Perf-21a/21b/21c` 三轮独立 Lua Editor 诊断均为 2/2 通过，`VM.Call` 分别为 0.025297、0.024287、0.025425 微秒/操作，均低于原基线。复测没有改动基线、迭代次数、测量逻辑或阈值。机器同时存在持续消耗 CPU 的无关后台应用，测量波动需要保留为限制；尚不能将超限归因于某个具体进程，也不能据复测宣称性能提升。下一轮完整验收为 `FullFix-Release-21`。

`FullFix-Release-21` 的 Lua Editor 为 625/625，Lua cooked 为 599/599，Luau Editor 为 639/640，Luau cooked 为 614/614，无跳过；两后端 Shipping 通过。Lua `VM.Call` 为 0.027748 微秒/操作，低于门槛；唯一失败用例包含 Luau `VM.TableGrow`（0.021962）和 `VM.TableRead`（0.010876）两项超限，均较各自参考值高约 42.5%。因此该轮仍为失败，不能将多轮通过的子项拼成一次完整验收。

连续出现短时测量波动后，采样改为每项至少预热 250 毫秒，再测量五组各至少 100 毫秒的样本，按每批迭代数归一化后取中位数。每批循环内容、迭代次数、原始参考文件及 25% 门槛不变；增加了总采样时间和原始批次记录。严格门禁重算中位数，拒绝过短、缺失或与汇总不一致的样本；工具回归增至 34 项并通过。旧参考采用单批采样，方法差异必须保留为限制，不能据此声称运行时变快。随后 Release22 完成了修改后的完整矩阵验证。

新采样的 `FullFix-Perf-22a/22b/22c` 三轮 Luau 诊断均为 2/2 通过，`TableGrow` 分别为 0.015982、0.014371、0.016551，`TableRead` 为 0.008285、0.007716、0.008346 微秒/操作。Lua `FullFix-Perf-22d` 为 2/2 通过，`VM.Call` 为 0.026046 微秒/操作。四轮原始样本和原参考值比对均通过独立校验。基线 SHA-256 与来源记录一致，完整复跑使用 `FullFix-Release-22`。

上述历史诊断之后，`FullFix-Release-22` 已完成整套矩阵并全部通过，最终证据见本文开头。历史失败记录保留；性能基线来自独立运行，没有用待测结果替换参考值。

## 验收边界

整场桌面录屏未成功：Windows gdigrab 捕获返回错误 5，ffmpeg 退出码 -22，因此没有可交付的整场录像。16 段 Showcase 视频来自测试帧，均完整解码通过，不能等同于整场录屏。

性能采样改为定时多批次中位数，旧基线仍采用单批次方法；后台应用持续消耗 CPU，测量存在环境波动。此次通过表示原门槛下的回归检查通过，不证明运行时性能提升。内存证据限于既定场景，不是全进程内存安全证明。

运行入口及范围见 [测试与自动化](Testing.md) 和 [Lua 联网](LuaNetworking.md)。Shipping 打包成功不等于 Shipping 运行认证；本矩阵不覆盖其他平台、所有 feature-off 组合、Iris/push model 及联网文档中列出的其他未覆盖场景。

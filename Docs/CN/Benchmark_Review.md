# 当前源码审计与路线图

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

核查日期：2026-10-24；2026-11-02 增补修复与执行结果，2026-11-03 完成完整 Release 验证（均见文末）。范围：运行时、绑定生成器、编辑器调试、测试工具、示例、文档、第三方来源。此页只记录当前结论，历史修改以 Git 为准。

**结论：尚未达到 slua-unreal / FUnLua 产品能力全面覆盖，也未证明性能领先或测试全覆盖。** 源码存在、用例存在、执行通过、目标平台验收是四种不同证据。逐项状态及源码入口见 [能力矩阵](../feature-matrix.md)。

## 对照基线

| 项目 | 核查提交 | 用途 |
|---|---|---|
| slua-unreal | `7970ab72fd59118e1b3f86aac2d016828b54b748` | 产品场景、CppBinding、对象/委托、Profiler、GC |
| FUnLua | `7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3` | 对象引用树、属性失效、实例绑定、委托身份 |
| Luau 上游 0.744 | `32c28089305f488f901aaf1dbf3fd1cb65fa3d11` | VM、库、Compiler、Require 与测试 |
| 当前集成 Luau | `6d36d483cb88973276af611198169f76f3ce05b0` | 0.743 + 1；未升级依赖 |
| UnLua 参考 HEAD | `3f112e8689405eaad79bd1f8f0bc8d598c8f7fc1` | 继承来源，非本轮主要重审对象 |

Luau 0.744 与当前 pin 的公开 VM 头文件一致，但 18 个 VM 实现文件有变化，不能把“接口未变”当作无风险升级。对照链接与具体吸收方式见 [源码对照](Comparison_sluaunreal.md)。

## 发布阻塞与验收

| ID / 优先级 | 源码事实与影响 | 下一步及通过条件 |
|---|---|---|
| NET-01 / P0 | **条件布局已交付**：8 种条件各有独立存储组，`(条件组, 类型桶, 下标)` 由声明推导；不支持的 Custom/Dynamic/Never/NetGroup/Replay* 显式拒绝且不降级成 None，单测覆盖分组、下标、读写与拒绝路径。旧组件级 `Condition` 只报警告、不再禁用复制；`Cleanup(Env)` 按共同 owner 集合释放，多 Env 任意销毁顺序都不会抹掉仍在用的 schema（已补反向顺序单测），无主条目归进程级清理，弱对象失效不写回野指针（已有单测）。任意 `UStruct` / 容器已由 RPC 动态 FProperty 与复制 Generic 桶交付（此前"架构边界"的判断是错的），规格参与 `SchemaVersion`，单测覆盖解析 / 嵌套 / 往返；声明报告按真实 FProperty 导出参数规格、逐端导出 `RepSchemaVersion` 与对照指纹 | **源码层面已无待实现项**。交付但未执行：owner/非 owner 可见性、混合条件与非 Owner RPC 拒绝断言；八类场景（含加入顺序互换、丢包/乱序、`UNetConnection` 级重连、跨 peer 声明差异）；Legacy / PushOff / Iris / Cooked 四变体（含 staged 二进制、cvar 回读与真实连接字节/丢包计数，供 Iris delta 对比）；这些都需要重建 + 真实 DS 才算证据。引擎级 seamless travel（`ServerTravel`）、热重载与权限检查明确不在范围内 |
| NET-02 / P0（专项未验收） | 复制桶已使用严格 int32/int64 转换；未知条件、稀疏数组、重名、坏类型拒绝，缓存 schema 禁止改写；RPC 错误参数不再缩短签名 | 已补实际桶读写的 ±2^31、±2^53、int64 极值和坏输入用例；仍需双后端及真实传输运行、schema 不匹配验收 |
| LIFE-01 / P0（专项未验收） | Timer 回调归属 Env 主状态；Env/World teardown 清理，显式 World、跨 Env 句柄隔离、暂停剩余时间与重入保护已实现 | 新增 coroutine GC、两种销毁顺序、多 World、跨 Env、重入取消回归；仍需世界暂停/时间膨胀、多 PIE 和打包运行 |
| LIFE-02 / P0（已修，专项未验收） | 手动引用代理和已知智能指针分配点已接 Luau native dtor；旧代理不能释放新代理的引用，未构造或已释放 payload 跳过析构。**UScriptStruct 拷贝已补终结器**（2026-11-02 修）：此前 `ClassRegistry.cpp` 的 `__gc` 没做 `UNLUANOVA_HAS_GC_METAMETHOD` 保护，Luau 上 `TM_GC` 不存在，结构体拷贝永不 `DestroyStruct`；现由 `NewScriptStructUserdata()` 把类型指针存进块内并挂 native dtor | 双后端强制 Lua GC + UE GC + Env close 用例已补；继续逐分配点审查 owner / borrowed / manual ref，不在 VM dtor 操作 Lua 栈；结构体数组与 DataTable 行拷贝需专项内存验收 |
| COMPAT-01 / P1 | `LuaScriptCompat` 的 upvalueid、registry、uservalue 是近似或占位，load mode 未完整执行 | 不支持的操作显式失败或发布明确子集；验证共享标量 upvalue、闭包别名、热重载失败回滚、文本/字节码模式 |
| BIND-01 / P1 | **已交付**：`BindInstance` / `UnbindInstance` 分层于类默认绑定之上，实例专属模块表、`RebindModule` 原地换元表、覆写函数名并集、实例路径逐次解析 Lua 函数；`InstanceBinding.spec.cpp` 覆盖 A/B 并存、交替调用、重绑、回落、解绑与拒绝路径。仍需真实 UE 运行产出证据 | 在真实 UE 会话跑 `UnLuaNova.API.InstanceBinding`；补继承 / 多 Env / 蓝图热重载不串线的专项执行 |
| EVENT-01 / P1 | Event token 强持有 Owner；弱键索引不自动解决 owner 保活。Once 重入需要在调用前取消 | 明确主动注销契约；验证递归 Emit、异常、解绑、闭包持有 owner；本轮修复 Once 重入，其他生命周期仍需验收 |
| SAFE-01 / P1 | sandbox 默认关闭；UE、debug、io、socket 等宿主能力不由只读表自动隔离。io 文件表可修改，路径检查不是 OS 沙盒 | 可信脚本 / 不可信脚本分别定义能力策略；检查符号链接、配额、超时、长跳转清理、调试服务暴露 |
| TOOL-01 / P1 | **coverage 导出与门禁已交付**：`lua.coverage dump` 按行导出 Luau 插桩计数、声明排除项；`Tools/ValidateCoverage.py` 重算分母、合并多轮、要求指定源与阈值并 fail-closed，15 项工具回归不依赖 UE 已通过。CodeGen 计数仍只记录请求。默认 release 已要求分析器/声明、实际用例清单、身份与失败原因 | 在真实 UE 会话里设 `luau.coverage=1`、跑目标脚本、导出并用门禁判定，把 JSON 与 gate 一起归档；仍需单入口 Release 的端到端执行 |
| QA-01 / P1 | 源码用例与旧报告不能覆盖当前修改；部分网络测试只比较独立进程 RPCId | 真正连接的服务器/多客户端、错误输入、GC/销毁、打包、多平台矩阵；按能力证明，不能只按注册用例数 |
| PERF-01 / P1（自比基线已建，跨项目对照仍缺） | `Tools/baseline/perf-lua.json` 与 `perf-luau.json` **已提交**（2026-10-24，Win64 Editor Development，UE 5.8.3，Ryzen 7 4700G），并作为 Release 的 25% 回退门禁；同机同构建下 Luau 比 Lua 5.5.1 更快，容器负载为 2.08×。thunk 仍调用 `ProcessEvent`。**与 slua-unreal / FUnLua 的同机等价适配器对照尚未建立**，跨项目速度结论仍不成立 | 为三个项目写等价适配器并同机跑；记录分布、分配、GC、冷启动和代码体积；见[性能页](Performance_Parity_slua.md) |
| LEGAL-01 / P0（已关闭） | 根许可已于 2026-11-02 改为 **MIT**，与继承文件的 MIT 头一致；规模限定、附加条款与商业授权一并移除（已删 `COMMERCIAL.md` 与 `CLA.md`）。此前"根许可与文件头范围不清"的冲突不再存在 | 剩余项与许可类型无关：补齐扩展库 / 工具 / 二进制依赖 / 示例资产的版本、来源与随包 notice 清单；新增第三方代码须记录 URL / revision、保留原声明并注明改动 |

## 实施顺序

1. **正确性与归属**：先关闭 NET/LIFE 阻塞，补失败输入和销毁顺序测试；修复之前把网络明确标为实验功能。
2. **覆盖与工具闭环**：完成能力矩阵各行的验收条件、示例 smoke、后端契约测试；运行报告必须包含源码提交与 dirty 状态、UE/编译器、后端、平台、flags、拓扑和原始产物。
3. **性能与发布**：建立可复现对照和阈值，执行打包与目标设备验收，完成许可清单。通过后才发布能力和性能结论。

## 设计创新的交付标准

| 方向 | 要解决的问题 | 验收 |
|---|---|---|
| Env/World 回调归属 | 计时器已采用不复用句柄、Env 隔离和关闭屏障；注册点诊断尚缺 | 双销毁顺序、地址复用、多 PIE 运行验收，补注册点诊断 |
| 同源签名描述 | thunk、反射回退、类型声明互相漂移 | 同一签名生成三者；异常、out/ref、默认值行为对照 |
| 确定性网络 schema | 运行时顺序、版本差异、条件冲突 | 稳定排序、版本/哈希协商、拒绝未知 schema，双端一致 |
| 能力与验证清单 | “有 API”被误当作“全支持” | 版本/后端/平台/示例/测试/验证产物可追溯 |

以上方向仍未完成整体验收。线程 GC 也不能仅为对标照搬：先证明 VM 与 UObject 的线程约束和停顿收益，再决定分片 GC 或更深层 VM 改造。

## 本轮变更与验证边界

已实施 Timer 归属与销毁、手动引用和智能指针析构、网络精确整数与声明拒绝；相关示例和双语 API 说明同步更新。CodeGen VM 归属、io 输入/CRLF、RPC 排序、schema 条件指纹和 Event.Once 重入也已有修正。以上改动由 2026-11-03 的完整 Release 验证从源码编译并执行通过（见文末）。

维护者只需运行 `.\Tools\RunTests.ps1`。双后端构建、Timer 开关、全量用例、真实 cooked peer、类型检查与打包由默认 Release 流程完成，缺证据失败；无需额外命令。该流程已在 2026-11-03 完整跑通（33/33 阶段），范围见 [测试说明](Testing.md)。

文档删除重复历史计划、保留当前使用说明；两份测试报告在每轮验证后由 `GenerateTestReport.py` 重新生成并发布，不手工改写。仓库检查仅验证文件引用、矩阵结构与脚本逻辑，不执行 UE、不衡量运行覆盖率。测试步骤见 [测试架构](Testing_Architecture.md)。

## 2026-11-02 增补

本轮不只读源码，实际编译并执行了用例。环境：Windows 11、UE 5.8.3 源码引擎、Win64、MSVC 14.50。

修复两项，均落在上表：

| 项 | 修复 | 位置 |
|---|---|---|
| LIFE-02 | `UScriptStruct` 拷贝在 Luau 后端不析构。此前 `ClassRegistry.cpp` 注册 `__gc` 时没有 `UNLUANOVA_HAS_GC_METAMETHOD` 保护，而 Luau 没有 `TM_GC`，该注册是死代码；分配点也未传 dtor，含 `FString` / `TArray` 成员的结构体拷贝永不 `DestroyStruct`。改为由 `NewScriptStructUserdata()` 把类型指针写进块内（占 padding 前 8 字节，读取端公式不变）并挂 native dtor | `LuaCore.h/.cpp`、`PropertyDesc.cpp`、`LuaLib_DataTable.cpp`、`ClassRegistry.cpp` |
| NET-01 | 复制声明表 `FUnLuaNovaNet::RepProps` 是进程级静态表，`Cleanup()` 无差别 `Empty()`，却由每个 Env 的 Manager 析构触发 —— 关闭任一 Env 会清空所有 Env 的 schema。改为按 Env 记录归属，`Cleanup(Env)` 只清该 Env 声明的及类已失效的条目 | `UnLuaNovaNet.h/.cpp`、`UnLuaNovaManager.cpp` |

验证结果（两个后端均从源码完整编译，无新增编译错误；Editor 单载体）：

| 后端 | 用例 | 结果 |
|---|---:|---|
| Luau（默认） | 527 | 527 通过 / 0 失败 / 0 跳过 |
| Lua 5.5.1 | 502 | 502 通过 / 0 失败 / 0 跳过 |

覆盖 `UnLuaNova.API.*`、`UnLuaNova.Regression.*`、`UnLuaNova.Binding`、`UnLuaNova.Memory`、`UnLuaNova.Editor`、`UnLuaNova.Settings`、`UnLuaNova.Lua`、`UnLuaNova.Extensions`。

**这不是完整 Release 验收**：未跑 cooked / 打包 / Shipping、未跑真实 DS + 多客户端联网、未跑性能门禁与类型检查，也未覆盖 `UnLuaNova.Perf` 与 `UnLuaNova.Net.*` 段。这些在次日补上，见下节。

## 2026-11-03 基线轮 Release 验证（已验证）

`.\Tools\RunTests.ps1 -Baseline` 跑完整矩阵并通过，产物在 `Saved/ReleaseTests/baseline`；
该轮 provenance 记录 commit `347bb8c5` 与当时的 8 项未提交改动，下列数字只对这次运行成立。
环境：Windows 11、UE 5.8.3 源码引擎、Win64、Visual Studio 2026 18.10（MSVC 14.50）。

| 后端 | 载体 | 用例 | 结果 |
|---|---|---:|---|
| Luau（默认） | Editor | 641 | 641 通过 / 0 失败 / 0 跳过 |
| Luau（默认） | cooked `-game` | 615 | 615 通过 / 0 失败 / 0 跳过 |
| Lua 5.5.1 | Editor | 626 | 626 通过 / 0 失败 / 0 跳过 |
| Lua 5.5.1 | cooked `-game` | 600 | 600 通过 / 0 失败 / 0 跳过 |

33/33 阶段通过，覆盖 cooked、真实 Dedicated Server + 两客户端（DedicatedServer 与 Multicast 两种会话）、
两个后端各自的类型检查（0 错误）、性能 25% 门禁，以及 Development / Server / Shipping 三种打包，
并包含 `UnLuaNova.Perf` 与 `UnLuaNova.Net.*` 段。

**因此该轮能关闭的是"改完没跑"这一层**：上面标了修复的 LIFE-02、NET-01 已进入执行证据。
**不能关闭的是各行的专项缺口** —— 条件复制的**联机验收**（布局与单测已交付）、多 PIE / 世界暂停、
Iris / push model、结构体数组与 DataTable 行拷贝的内存专项，以及跨项目同机对照，
仍需要各自的用例与运行。它们仍是范围声明，不是"跑过了但没写"。

## 最近常规验证（未通过）

`Saved/ReleaseTests/test` 的多轮重跑未通过：性能回退已排除为代码回归（重跑 Luau Editor
641/641 且门禁通过，参考基线未替换）；UBT 全局锁冲突与编译进程被外部中断曾使打包 / 构建
阶段失败并连锁阻塞证据阶段；当前工作树未提交的 Net 源码改动使 2 个 `UnLuaNova.API.Net.*`
用例失败，属需要关闭的真实回归。发布前需冻结工作树、同步能力矩阵，并在无并发 UE 构建的
环境重跑完整 Release。当前状态见[版本说明](ReleaseNotes.md)。

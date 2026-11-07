# 当前版本状态

<!-- release-status: passed -->
<!-- release-provenance: 347bb8c5 -->

本页只记录**已经跑出来的结论**，不叠加已过期的状态。能力与未尽工作逐项见
[审计与路线图](Benchmark_Review.md) 与 [能力矩阵](../feature-matrix.md)，历史细节从 Git 获取。

## 当前发布状态：通过

当前状态以 `Saved/ReleaseTests/test/release-summary.json` 为准：2026-11-07 的完整
`.\Tools\RunTests.ps1` 产出 `complete=true`、`passed=true`，33/33 阶段通过、2752/2752 次 UE 执行、0 失败、0 跳过 —— Lua Editor 690、Lua cooked 661、Luau Editor 715、Luau cooked 686，四个 release gate 全绿。
下面 33/33 与 2482/2482 的旧结论只属于 2026-11-03 基线轮。

### 已关闭的历史失败（不是放宽门禁）

此前多轮常规验证未通过，原因均已逐项定位并修复；最新一轮之后不再是当前状态：

1. **性能回退已排除为代码回归**：首次重跑的 Luau Editor 有 5 项超过 25% 门禁
   （+43.8%~+55.8%），但两轮之间产品源码（C++ / Lua / ini）零改动，运行清单、后端开关与
   进程优先级一致，5 组采样稳定；后续重跑中 Luau Editor 641/641 全过且性能门禁通过，
   属测量环境干扰；当时参考基线未替换（后续 `1455c38d` 已用 median-of-5 独立参考运行刷新 `perf-lua.json`）。
2. **本机 UBT 编译进程被外部中断**：`ConflictingInstance`（全局互斥锁被孤儿 UBA/UBT 进程占用）
   与编译子进程 `Exit -1` 曾使 `Lua-package-development`、`Lua-server-build`、
   `Luau-package-development` 等阶段失败，并连锁阻塞测试与证据阶段。清理孤儿进程后这些阶段
   可恢复通过；这不是产品代码问题，但要求重跑时机器无并发 UE 构建。
3. **Net 源码回归已关闭（本轮）**：未提交的网络改动已提交为 `5f2e5f00`；
   `UnLuaNovaTestSuite.Build.cs` 补 `NetCore` 依赖后消除了
   `StaticEnum<ENetCloseResult>` / `LexToString(ENetCloseResult)` 的链接错误；
   `LuaNet.spec.cpp` 的通用 payload 顺序无关性用例修正了键值绑定（原先两次构造的 map
   内容本身不同，只改插入顺序却把整数绑在循环序号上）。本轮 Luau 后端增量编译
   `Result: Succeeded`，Luau Editor 以 `UnLuaNova.API.Net` 过滤实测
   35 成功 / 3 带警告 / 0 失败（`Saved/TestReports/20261103-234556/luau/index.json`）。
   **这不是完整 Release**：33 个阶段、四种载体与四个 release gate 仍需在无并发构建的机器上重跑。

**本轮还关闭了多处运行时缺陷**：类级绑定后的 `BindInstance` 不再对同一 UFunction 二次打补丁；
Luau 的 `lua_Callbacks::interrupt` 普通安全点（`GC = -1`）不再被当成 GC 中断跳过，
safe-point 预算与超时看门狗都能真正取消死循环；`~FLuaEnv()` 在 `lua_close` 前清理 VM hook
槽位并跳过已关闭状态上的 `luaL_unref`，消除 cooked 卸载崩溃与跨 Env 的 hook 残留；
`FLuaVmHooks::Get()` 改为导出的进程级单例，避免每个 DLL 各持一份 hook 表。
2026-11-07 的通过记录、阶段明细与归档在 `Saved/ReleaseTests/test/` 下；25% 门禁未放宽；`perf-lua.json` 参考基线已在 `1455c38d` 换用 median-of-5 独立参考运行（与当前采样法一致），`perf-luau.json` 与 2026-11-03 基线轮记录未改动。

## 2026-11-03 基线轮（已验证）

下面的数字只属于这次运行：`.\Tools\RunTests.ps1 -Baseline`，产物
`Saved/ReleaseTests/baseline`，2026-11-03 03:32:11 UTC 开始、04:20:26 UTC 结束；
该轮 `release-summary.json` 的 provenance 记录 commit `347bb8c5`（当时工作树含 8 项未提交改动）
与源码 / 工程指纹。**它不代表之后提交或当前工作树的状态。**

UE 5.8.3 / Win64（Visual Studio 2026 18.10）上的一次完整 Release 验证：33/33 阶段通过，
2482/2482 次 UE 执行，0 失败、0 跳过 —— Lua Editor 626、Lua cooked 600、Luau Editor 641、
Luau cooked 615。

- 两个后端均从源码完整编译：反射双向调用、模板导出、生成 marshal thunk、覆写与 RPC 覆写。
- 真实 Dedicated Server + 两客户端的 RPC 与复制证据（DedicatedServer 与 Multicast 两种会话），
  各端运行时清单与内存报告一并归档。
- 两个后端各自的静态检查：0 错误（保留 Lua 298 条、Luau 7 条告警，均为分析器对
  `package.path` 式 require 的已知限制，不影响加载）。
- Development / Server / Shipping 三种打包在两个后端上均成功。
- 41 项 Python 工具测试与 PowerShell 支持检查通过。
- 该轮性能：每个基准用例都在已提交参考基线的 25% 门禁之内；Luau 在容器用例上领先 Lua 5.5.1
  （同机自比，不是与第三方方案的同机对照）。

## 本轮没有关闭的边界

这些是范围声明，不是待补的结论：条件复制的按条件分组布局已交付并有单测（8 种条件），
任意 `UStruct` / 容器也已由 RPC 动态 FProperty 与复制 Generic 桶交付，并按真实 FProperty 导出
参数规格、逐端导出声明指纹；多 Env 按共同 owner 释放、弱对象失效不写回野指针均有单测。但仍属实验特性——
owner / 非 owner 联机验收、非 Owner RPC 拒绝、**八类**连接场景（含加入顺序互换、丢包/乱序、
`UNetConnection` 级重连、跨 peer 声明差异）与 Legacy / PushOff / Iris / **Cooked** 四变体的
场景源码与门禁均已补齐，**但都尚未执行**；源码层面已无网络待实现项。
第三方 notice 清单已能确定性生成并静态校验，但 `UNRESOLVED` 项仍需权利人决定；
非 Win64 平台、引擎级 seamless travel、热重载与权限检查、
以及与 slua-unreal / FUnLua 的同机对照都需要各自独立的验收运行。

验收方法与门禁见 [测试](Testing.md)，诊断历史见 [2026-10-24 回归记录](Regression_20261024.md)。

# 当前版本状态

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

本页只记录**已经跑出来的结论**，不叠加已过期的状态。能力与未尽工作逐项见
[审计与路线图](Benchmark_Review.md) 与 [能力矩阵](../feature-matrix.md)，历史细节从 Git 获取。

## 当前发布状态：未通过

当前状态以 `Saved/ReleaseTests/test/release-summary.json` 为准；下面 33/33 的已验证结论只属于
2026-11-03 基线轮。之后的多轮常规验证未通过，原因分三类，均已定位，不含"放宽门禁"：

1. **性能回退已排除为代码回归**：首次重跑的 Luau Editor 有 5 项超过 25% 门禁
   （+43.8%~+55.8%），但两轮之间产品源码（C++ / Lua / ini）零改动，运行清单、后端开关与
   进程优先级一致，5 组采样稳定；后续重跑中 Luau Editor 641/641 全过且性能门禁通过，
   属测量环境干扰。参考基线未替换。
2. **本机 UBT 编译进程被外部中断**：`ConflictingInstance`（全局互斥锁被孤儿 UBA/UBT 进程占用）
   与编译子进程 `Exit -1` 曾使 `Lua-package-development`、`Lua-server-build`、
   `Luau-package-development` 等阶段失败，并连锁阻塞测试与证据阶段。清理孤儿进程后这些阶段
   可恢复通过；这不是产品代码问题，但要求重跑时机器无并发 UE 构建。
3. **当前工作树存在未提交的 Net 源码改动**：`UnLuaNovaNet.cpp`、相关头文件与
   `LuaNet.spec.cpp` 等有改动，Luau Editor 的 2 个 `UnLuaNova.API.Net.*` 用例因此失败。
   这是需要开发侧关闭的真实回归，不能写成范围声明。

**发布仍被阻断**：需先冻结工作树（提交或回滚 Net 改动）、同步能力矩阵，再在无并发构建的环境
跑完整 `.\Tools\RunTests.ps1`，拿到 `complete=true`、`passed=true`、33/33、四个 release gate 全绿，
才能把本页状态改回通过。诊断明细与各轮归档在 `Saved/ReleaseTests/` 下。

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
任意 `UStruct` / 容器也已由 RPC 动态 FProperty 与复制 Generic 桶交付；但仍属实验特性——
owner / 非 owner 联机验收、非 Owner RPC 拒绝、晚加入 / 进程级重连 / dormancy / relevancy
四类场景、Legacy / PushOff / Iris 变体的场景源码与门禁均已补齐，**但都尚未执行**；
第三方 notice 仍在补齐；非 Win64 平台、cooked 载体、以及与 slua-unreal / FUnLua 的
同机对照都需要各自独立的验收运行。

验收方法与门禁见 [测试](Testing.md)，诊断历史见 [2026-10-24 回归记录](Regression_20261024.md)。

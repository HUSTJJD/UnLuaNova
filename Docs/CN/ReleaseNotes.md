# 当前版本状态

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

本页只记录**已经跑出来的结论**，不叠加已过期的状态。能力与未尽工作逐项见
[审计与路线图](Benchmark_Review.md) 与 [能力矩阵](../feature-matrix.md)，历史细节从 Git 获取。

## 当前发布状态：未通过

最近一次常规 Release 验证（2026-11-03 15:23 本地结束，产物 `Saved/ReleaseTests/test`）：
`complete=true`、`passed=false`。33 个阶段里 `Luau-editor-tests` 失败、`Luau-editor-evidence`
被阻塞；Luau Editor 641 个用例中 1 个失败，其余三个载体 626 / 600 / 615 全通过。

失败点是 `UnLuaNova.Perf.Benchmark`：Reflection、VM.Arithmetic、VM.Call、VM.TableGrow、
VM.TableRead 分别比已提交参考基线慢 47.9%、46.7%、43.8%、51.6%、55.8%，均超过 25% 门禁。
对比基线轮，产品源码（C++ / Lua / ini）零改动，运行清单、后端开关与进程优先级一致，
5 组采样稳定，属测量环境干扰待复核，不是已确认的代码回归。基线不会被替换，门禁不会放宽；
需在无并发负载的机器上重跑完整 Release 才能改回通过状态。

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

这些是范围声明，不是待补的结论：条件复制仍为实验特性，非 `None` 条件会被显式拒绝；
第三方 notice 仍在补齐；非 Win64 平台、Iris / push model、以及与 slua-unreal / FUnLua 的
同机对照都需要各自独立的验收运行。

验收方法与门禁见 [测试](Testing.md)，诊断历史见 [2026-10-24 回归记录](Regression_20261024.md)。

# UnLuaNova

基于 Tencent/UnLua 的 Unreal Engine Lua / Luau 脚本集成。不 fork 引擎、能运营期热更、性能可测。

[中文入口](CN/index.md) · [English](EN/index.md) · [GitHub](https://github.com/HUSTJJD/UnLuaNova)

## 当前状态

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

源码包含双后端、UE 绑定与覆写、容器与委托、静态导出与生成 thunk、调试与 Profiler、
类型声明导出、运行时库和实验性声明式网络。许可是 **MIT**。

**当前发布状态：未通过**（以 `Saved/ReleaseTests/test/release-summary.json` 为准）。
33/33 的已验证结论只属于 2026-11-03 基线轮（commit `347bb8c5`）。后续重跑中性能回退已排除为
代码回归（重跑 Luau Editor 641/641 且门禁通过）；UBT 全局锁冲突与编译进程被外部中断曾使打包 /
构建阶段失败；当前工作树还有未提交的 Net 源码改动，使 2 个 `UnLuaNova.API.Net.*` 用例失败。
基线不替换、门禁不放宽，发布前需冻结工作树并在无并发构建环境重跑完整 Release。
详见[版本说明](CN/ReleaseNotes.md)。

**2026-11-03 基线轮完整 Release 验证通过**（`.\Tools\RunTests.ps1 -Baseline`、
`Saved/ReleaseTests/baseline`、commit `347bb8c5`；下列数字只对该轮成立。
UE 5.8.3 源码引擎 / Win64 / Visual Studio 2026 18.10）：

| 后端 | 载体 | 用例 | 结果 |
|---|---|---:|---|
| Luau（默认） | Editor | 641 | 641 通过 / 0 失败 / 0 跳过 |
| Luau（默认） | cooked `-game` | 615 | 615 通过 / 0 失败 / 0 跳过 |
| Lua 5.5.1 | Editor | 626 | 626 通过 / 0 失败 / 0 跳过 |
| Lua 5.5.1 | cooked `-game` | 600 | 600 通过 / 0 失败 / 0 跳过 |

33/33 阶段通过，含真实 Dedicated Server + 两客户端联网、两个后端各自的类型检查（0 错误）、
性能 25% 门禁，以及 Development / Server / Shipping 三种打包。覆盖 `UnLuaNova.API.*`、
`UnLuaNova.Regression.*`、`UnLuaNova.Binding`、`UnLuaNova.Memory`、`UnLuaNova.Editor`、
`UnLuaNova.Settings`、`UnLuaNova.Lua`、`UnLuaNova.Extensions`、`UnLuaNova.Perf`、
`UnLuaNova.Net.*` 与 `UnLuaNova.Showcase`。

性能基线已提交（`Tools/baseline/perf-lua.json`、`perf-luau.json`），并作为 Release 的 25% 回退门禁。
同机同构建下 Luau 比 Lua 5.5.1 更快，容器工作负载上为 **2.08×**（22.570 → 10.868 µs/op）。
**这仍不是与 slua-unreal / FUnLua 的同机对照**，跨项目速度结论尚未建立。

本轮没有覆盖的范围（范围声明，不是遗漏）：非 Win64 平台、条件复制的**联机验收**
（owner / 非 owner、晚加入 / 进程级重连、dormancy / relevancy）、Legacy / PushOff / Iris
变体的真实执行、第三方 notice 清单，以及与 slua-unreal / FUnLua 的同机对照。按条件分组布局
（8 种条件）、任意 `UStruct` / 容器类型规格、四类连接场景源码和变体开关与门禁**已交付但尚未执行**，
见 [Lua 声明式网络（实验）](CN/LuaNetworking.md)。
诊断历史与更早轮次见 [回归报告](CN/Regression_20261024.md)。

- [现状、风险与路线图](CN/Benchmark_Review.md)
- [能力与证据矩阵](feature-matrix.md)
- [slua / FUnLua 对照](CN/Comparison_sluaunreal.md)
- [Luau 支持边界](CN/Luau.md)
- [性能验收方法](CN/Performance_Parity_slua.md)
- [测试框架与验收](CN/Testing_Architecture.md)
- [Lua 验证状态](test-lua-report.md) / [Luau 验证状态](test-luau-report.md)

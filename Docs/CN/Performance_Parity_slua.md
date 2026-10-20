# 性能验收

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

尚未建立"比 slua-unreal / FUnLua 更快"的同条件实测。上游 README 的倍数、不同机器的截图、
单轮本机数字都不作为产品结论。

**已有的是自比基线**：两个后端互相跑的实测数据已提交，并作为 Release 的回退门禁。

## 已提交的自比基线

`Tools/baseline/perf-lua.json` 与 `perf-luau.json`，原始测量来自独立 Development Editor 运行
`Saved/TestReports/FullFix-Complete-11`（2026-10-24）。每个用例预热 ≥ 250 ms，再取 5 段各
≥ 100 ms 的样本，报批次均值的中位数；Release 校验器要求后端、引擎版本、机器、用例清单与
迭代数一致，任一用例慢超过 25% 即失败。

| 用例 | 迭代 | Lua 5.5.1 | Luau | Luau 相对 |
|---|---:|---:|---:|---:|
| `VM.Arithmetic` | 200k | 0.0104 µs | 0.0059 µs | 1.75× |
| `VM.Call` | 200k | 0.0263 µs | 0.0203 µs | 1.30× |
| `VM.TableRead` | 200k | 0.0092 µs | 0.0076 µs | 1.20× |
| `VM.TableGrow` | 200k | 0.0120 µs | 0.0154 µs | 0.78× |
| `Lua.Container.PointsAreCoplanar` | 20k | 22.570 µs | 10.868 µs | **2.08×** |
| `Reflection`（UE 反射调用） | 20k | 0.5006 µs | 0.4590 µs | 1.09× |
| `Lua`（脚本侧调用） | 20k | 1.5972 µs | 1.5429 µs | 1.04× |
| `Native`（C++ 直调，参考下限） | 20k | 0.0019 µs | 0.0017 µs | — |

测量条件：Win64 · Editor Development · UE 5.8.3 · MSVC 14.50 · AMD Ryzen 7 4700G ·
机器 `davidsjian-PC2`。

`VM.TableGrow` 上 Lua 5.5.1 更快 —— 照实记录，不挑好看的写。

## 已实现的路径

- 反射描述和参数缓存来自 UnLua；`CallUEFast` 减少部分重复查找。
- `UnLuaNovaEx` 提供手写模板导出；生成 thunk 特化参数编组，但仍调用 `UObject::ProcessEvent`。
- 生成器只接受部分签名；static/net、复杂 struct/container 等必须核实回退成本，不能统称"无反射直调"。
- Luau vector 运算减少某些 userdata 分配；buffer 互操作仍是 O(n) 拷贝。
- CodeGen、冻结环境、编译优化与 GC 参数会改变启动成本、调试能力和峰值内存，需要独立测量。

## 建立跨项目对照的可复现方案

1. 固定 UE/编译器、平台、硬件、电源策略、插件提交、VM 提交、构建配置和优化 flags。
2. 为三个项目写等价适配器，覆盖纯 VM、Lua→UE / UE→Lua、反射/模板/thunk、out/ref、字符串、对象、容器批处理、委托、覆写、网络和 GC。
3. 正确性断言先通过；预热后多轮采样，保留原始数据及 p50/p95/p99，不只均值。记录操作次数、时间单位与计时器分辨率。
4. 同时测冷启动、脚本编译、每次分配、峰值内存、最长 GC 停顿和代码体积；分别测 debug attached / detached、CodeGen on / off。
5. 性能基线按目标和配置隔离；约定回退上限及噪声区间后再作为门禁，不用某项快掩盖另一项退化。

## 最近一轮 Release

2026-11-03 的常规验证（`Saved/ReleaseTests/test`）Luau Editor 有 5 项超过 25% 门禁，
整轮 `passed=false`。与已验证的基线轮相比，产品源码、运行清单与进程优先级均未变化，
属测量环境干扰待复核，不是已确认的代码回归；**已提交基线未被替换，25% 门禁未被放宽**。
结论见[版本说明](ReleaseNotes.md)。

## 当前测试工具

`Benchmark.spec.cpp`、`BenchmarkParity.spec.cpp` 提供 `UnLuaNova.Perf.*` 用例。
默认 `RunTests.ps1` 自动运行基准，并由 `ValidateRelease.py` 强制校验基线范围、case 完整性与 25% 回退阈值。

在这套跨项目对照完成之前，宣传只能描述实现机制与自比数据，不能承诺对外的速度优势。
[当前审计](Benchmark_Review.md)

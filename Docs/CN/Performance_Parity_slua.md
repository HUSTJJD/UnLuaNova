# 性能验收

当前没有可支持“比 slua-unreal / FUnLua 更快”的同条件实测。上游 README 的倍数、不同机器的截图、单轮本机数字都不作为产品结论。

## 已实现的路径

- 反射描述和参数缓存来自 UnLua；`CallUEFast` 减少部分重复查找。
- `UnLuaNovaEx` 提供手写模板导出；生成 thunk 特化参数编组，但仍调用 `UObject::ProcessEvent`。
- 生成器只接受部分签名；static/net、复杂 struct/container 等必须核实回退成本，不能统称“无反射直调”。
- Luau vector 运算减少某些 userdata 分配；buffer 互操作仍是 O(n) 拷贝。
- CodeGen、冻结环境、编译优化与 GC 参数会改变启动成本、调试能力和峰值内存，需要独立测量。

## 可复现方案

1. 固定 UE/编译器、平台、硬件、电源策略、插件提交、VM 提交、构建配置和优化 flags。
2. 为三个项目写等价适配器，覆盖纯 VM、Lua→UE / UE→Lua、反射/模板/thunk、out/ref、字符串、对象、容器批处理、委托、覆写、网络和 GC。
3. 正确性断言先通过；预热后多轮采样，保留原始数据及 p50/p95/p99，不只均值。记录操作次数、时间单位与计时器分辨率。
4. 同时测冷启动、脚本编译、每次分配、峰值内存、最长 GC 停顿和代码体积；分别测 debug attached / detached、CodeGen on / off。
5. 性能基线按目标和配置隔离；约定回退上限及噪声区间后再作为门禁，不用某项快掩盖另一项退化。

## 当前测试工具

`Benchmark.spec.cpp`、`BenchmarkParity.spec.cpp` 已存在。用例有性能阈值逻辑，`RunTests.ps1 -UpdateBaseline` 可记录基线；目前 `Tools/baseline/perf-*.json` 未提交，缺基线只告警。用例名基线不等于性能基线。

本轮没有运行任何基准或编译。完成以上验收之前，宣传只能描述实现机制，不能承诺速度优势。[当前审计](Benchmark_Review.md)

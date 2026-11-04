# 跨项目性能适配器

用来回答"UnLuaNova 是不是 UE 最快的脚本方案"。

**它现在能做什么**：定义一份三边共用的工作负载契约，跑 UnLuaNova 侧，把结果归一化成
统一 JSON，并在对齐检查通过后出对比表。

**它现在还不能做什么**：**还没有跑过 slua-unreal 和 FUnLua。** 那两个项目不在本仓库里，
必须在它们各自的工程里跑。`Tools/baseline/perf-*.json` 里已有的数字是**自比基线**
（Lua 5.5.1 vs Luau），不是跨项目结论。

## 目录

| 文件 | 作用 |
|---|---|
| [WORKLOAD.md](WORKLOAD.md) | 工作负载契约与用例表。改任何一条都要 bump `revision` |
| `runner.py` | 归一化 + 对齐检查 + 出对比表 |
| `metrics.py` | 单项目多轮原始分布（min / median / mean / p95 / max / stddev），fail-closed 对齐检查 |
| `workload/unluanova/cases.lua` | UnLuaNova 侧实现（可直接跑） |
| `workload/external/cases.lua` | 外部绑定通用适配器，slua-unreal / FUnLua 各配一次项目名即可 |
| `results/` | runner 产出的归一化 JSON 与 `comparison.md`（首次运行时生成） |

## 设计取舍

**为什么输入是日志块而不是 JSON**：不能假设每个 VM 里都有 JSON 写文件的能力，但每个
绑定都能 `print`。所以契约是**行格式**，schema 由 `runner.py` 独占。

```
#UNLUANOVA_BENCH_BEGIN
#meta project=<名> backend=<名> engine=<版本> machine=<主机> cpu=<CPU> config=<配置> carrier=<载体> workload_revision=1 measured_utc=<ISO>
#case name=<用例> iterations=<次数> peropus=<µs> samples=<用逗号分隔的原始样本>
#UNLUANOVA_BENCH_END
```

`python Tools/CrossProject/runner.py --emit-template` 可以打出一份模板。

**为什么不比对不上的结果**：`machine`、`cpu`、`engine`、`config`、`carrier`、
`workload_revision`、用例名集合、迭代数——任何一项不一致，runner 打印原因并退出码 1。
跨机器、跨构建、用例数不同的对比是最常见的自欺来源，宁可拒绝出表。

## 跑 UnLuaNova 这一侧

1. 把 `workload/unluanova/cases.lua` 复制到工程的 `Content/Script/` 下（比如叫
   `CrossProjectBench.lua`）。**不要改源码填机器信息**：先调用，再跑：

   ```
   lua.do "require('CrossProjectBench').SetMachineInfo('my-pc','AMD-Ryzen-7-4700G')"
   lua.do "require('CrossProjectBench').Run()"
   ```

   `machine` / `cpu` 是对齐字段，必须是不含空格的真实值（CPU 型号里的空格用 `-`）。
   没设置时 `Bench.Run()` 直接拒绝输出，而不是打印 `FILL-ME` 让 runner 事后才发现问题；
   runner 自己也会拒绝任何占位值。
2. 运行游戏或编辑器，控制台执行：

```
lua.dofile CrossProjectBench
```

   更省事的做法是在文件末尾调用 `Bench.Run()`，或者在别处 `require` 后调用它。

3. 日志里会出现 `#UNLUANOVA_BENCH_BEGIN` … `#UNLUANOVA_BENCH_END` 块。先读一遍有没有
   `-- case failed:`：某个用例的 UE API 在这个绑定里调不到时，它会以 `peropus=0` 报出，
   runner 因此拒绝比较。**请修调用，不要删用例。**

4. 把日志交给 runner（可一次给多份）：

```powershell
python Tools/CrossProject/runner.py --baseline UnLuaNova Saved\Logs\*.log
```

产物落在 `Tools/CrossProject/results/`：`unluanova-luau.json`、`comparison.md`。

## 出表之前先看方差（单项目多轮分布）

`runner.py` 给每个用例一个中位数；一个中位数不足以支撑"快 X 倍"。`metrics.py` 把同一个
project / backend 的所有轮次的 `samples` 合并成原始分布：

```powershell
python Tools\CrossProject\metrics.py Tools\CrossProject\results\*.json -o Tools\CrossProject\results\metrics.md
```

输出 min / median / mean / p95 / max / stddev 与 per-run median。机器、CPU、引擎、配置、
载体、workload revision、project、backend、用例集或迭代数不一致时拒绝合并；某个用例没有
样本也拒绝，不会发明一个平滑但无意义的分布。这是**单项目**分布，不是跨项目结论。

## 跑 slua-unreal / FUnLua 那两侧

把 `workload/external/cases.lua` 复制到目标工程的脚本目录（例如
`Content/Script/CrossProjectBench.lua`），在各自工程里配置后运行：

```lua
local Bench = require("CrossProjectBench")
Bench.SetProject("slua-unreal", "Lua", "5.8.3")   -- 或 "FUnLua"
Bench.SetMachineInfo("my-pc", "AMD-Ryzen-7-4700G")
Bench.Run()
```

适配器把全部引擎调用收敛到 `UEAPI.Static` / `UEAPI.Vector`：依次尝试 `UE` / `UE4` /
`slua` 全局与 `import()`（slua-unreal 的常见拼写）。都不可达时该用例输出
`peropus=0` 并报 `-- case failed:`，runner 会拒绝出表，而不是悄悄少算一个慢用例。
若你的工程用了别的拼写，**只改这两个钩子**，不要删用例或改迭代数——用例集、迭代数、
revision、meta 字段与失败上报由 `Tools/tests/test_crossproject_workloads.py` 静态校验。
用例全部只用引擎自带类，不需要给两侧加 C++；把各自打印的日志和 UnLuaNova 的日志一起
喂给 runner 即可。

在它们跑起来之前，**任何"比 slua 快"的说法都不成立**。README 与
[Docs/CN/Performance_Parity_slua.md](../Docs/CN/Performance_Parity_slua.md) 目前只陈述
自比数据与实现机制，就是这个原因。

## 状态

| 项 | 状态 |
|---|---|
| 契约与用例表 | ✅ revision 1 |
| runner（归一化 / 对齐检查 / 对比表） | ✅ 已自测：缺用例、machine 不一致、GBK 控制台、占位身份拒绝（6 项对齐回归） |
| `metrics.py`（单项目多轮分布） | ✅ 已写，10 项回归通过；待真实多轮日志产生分布 |
| UnLuaNova 侧实现 | ✅ 已写，待在真实工程里跑一遍确认每个 UE 调用可达 |
| 外部绑定通用适配器 | ✅ 已写（UE 调用自适应 + fail-fast），静态契约 6 项回归通过；待在目标工程核对 API |
| workload 静态契约 | ✅ 用例集 / 迭代数 / revision / meta 字段 / 失败上报 / 无占位身份（6 项回归） |
| slua-unreal 侧 | ⚠️ 适配器已就绪，需在 slua-unreal 工程里实跑 |
| FUnLua 侧 | ⚠️ 适配器已就绪，需在 FUnLua 工程里实跑 |
| 跨项目结论 | ❌ 尚未建立 |

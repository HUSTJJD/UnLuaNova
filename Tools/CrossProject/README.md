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
| [WORKLOAD.md](WORKLOAD.md) | 工作负载契约、用例表与非时间指标定义。改用例要 bump `revision` |
| `runner.py` | 归一化 + 对齐检查（含可选指标集 / 单位）+ 出对比表 |
| `metrics.py` | 单项目多轮时间分布（min / median / mean / p95 / max / stddev，fail-closed 对齐）+ 可选非时间指标分布（`--require-metrics`） |
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
#metric name=memory.peak_kb value=1024.0 unit=kb
#metric name=startup.load_us value=812.0 unit=us
#metric name=gc.pause_us value=2048.0 unit=us
#metric name=code_size.bytes value=unavailable unit=bytes
#UNLUANOVA_BENCH_END
```

`#metric` 是 **revision 1 的可选扩展字段**：独立的行类型，不是 `#case`，不进入用例集 /
迭代数对齐，也不 bump revision。四项定义、单位枚举 `kb|us|bytes`、采集方式与拒绝规则见
[WORKLOAD.md](WORKLOAD.md) 的「非时间指标」小节。

`python Tools/CrossProject/runner.py --emit-template` 可以打出一份模板（含 `#metric` 示例）。

**为什么不比对不上的结果**：`machine`、`cpu`、`engine`、`config`、`carrier`、
`workload_revision`、用例名集合、迭代数——任何一项不一致，runner 打印原因并退出码 1。
跨机器、跨构建、用例数不同的对比是最常见的自欺来源，宁可拒绝出表。带 `#metric` 时，
runner 还会校验指标集是否齐全、单位是否合法 / 与指标匹配；旧日志没有 `#metric` 行时
这部分完全不触发，行为不变。

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
JSON 里会多一个可选的 `Metrics` 数组（四项 `Name` / `Value` / `Unit`），供 `metrics.py` 消费。

## 出表之前先看方差（单项目多轮分布）

`runner.py` 给每个用例一个中位数；一个中位数不足以支撑"快 X 倍"。`metrics.py` 把同一个
project / backend 的所有轮次的 `samples` 合并成原始分布：

```powershell
python Tools\CrossProject\metrics.py Tools\CrossProject\results\*.json -o Tools\CrossProject\results\metrics.md
```

输出 min / median / mean / p95 / max / stddev 与 per-run median。机器、CPU、引擎、配置、
载体、workload revision、project、backend、用例集或迭代数不一致时拒绝合并；某个用例没有
样本也拒绝，不会发明一个平滑但无意义的分布。这是**单项目**分布，不是跨项目结论。

## 非时间指标（可选扩展，默认不强制）

时间用例之外，一次运行还可以打印四项**非时间指标**：`memory.peak_kb`、`startup.load_us`、
`gc.pause_us`、`code_size.bytes`。它们走独立的 `#metric` 行（值不含空格，放在 `#case`
循环之后、`#UNLUANOVA_BENCH_END` 之前）：

```
#metric name=<名字> value=<值> unit=<单位>
```

单位枚举只有 `kb` / `us` / `bytes`，且每个指标单位固定。**fail-closed**：拿不到就写
`value=unavailable`，**绝不写 0 或占位值**；`unavailable` 不参与数值分布，也不会被当成 0。
缺项、指标集跨轮不一致、单位未知 / 串位 / 跨轮漂移、非有限数（NaN / inf）、负值、非数值
且非 `unavailable` 一律拒绝（`metrics.py` 抛 `ValueError` / `main` 返回码 1）。

`metrics.py` **默认不强制**这四项，对旧 JSON（无 `Metrics` 字段）的 stdout 文本与返回码
完全不变。要强制每次运行都带齐并对多轮做分布，加 `--require-metrics`；`--metrics` 可指定
要哪几项（默认四项全要，逗号分隔）：

```powershell
python Tools\CrossProject\metrics.py Tools\CrossProject\results\*.json --require-metrics -o Tools\CrossProject\results\metrics.md
python Tools\CrossProject\metrics.py Tools\CrossProject\results\*.json --require-metrics --metrics memory.peak_kb,gc.pause_us
```

开启后报告在原有 "Benchmark distribution" 表后追加一张 "Non-time metrics" 表
（min / median / mean / p95 / max / stddev），原有的 "not a cross-project conclusion"
声明保持不变。某项全轮 `unavailable` 时该行标注 `unavailable`、不给数值。

**采集方式**：`memory.peak_kb` 用 `collectgarbage("count")` 在预热 / 各用例间采样取峰值；
`startup.load_us` 用 `os.clock()` 包住加载并执行一段初始化 Lua chunk；`gc.pause_us` 用
`os.clock()` 包住全量 GC 取平均停顿；`code_size.bytes` 尝试读回脚本自身源码字节数。
四项都是**纯 VM 采集**，不经过 `UEAPI.Static` / `UEAPI.Vector` 引擎钩子。

**外部绑定侧（slua-unreal / FUnLua）**：脚本常托管在 UE 资产而非真实文件系统路径，
`code_size.bytes` 通常拿不到，适配器按规则输出 `value=unavailable unit=bytes`（在真实工程
确认脚本路径可读后才会有数值）；其余三项是纯 VM 采集，与 UnLuaNova 侧同构。任何一项
拿不到都按 `unavailable` 处理，不伪造 0。

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
喂给 runner 即可。非时间指标是纯 VM 采集，不走这两个钩子；`code_size.bytes` 拿不到时
输出 `unavailable` 而不是 0（见上节）。

在它们跑起来之前，**任何"比 slua 快"的说法都不成立**。README 中的性能数据只陈述
自比数据与实现机制，就是这个原因。

## 状态

| 项 | 状态 |
|---|---|
| 契约与用例表 | ✅ revision 1 |
| runner（归一化 / 对齐检查 / 对比表） | ✅ 已自测：缺用例、machine 不一致、GBK 控制台、占位身份拒绝（6 项对齐回归）；新增可选指标集 / 单位校验 |
| `metrics.py`（单项目多轮时间分布） | ✅ 已写，10 项回归通过；待真实多轮日志产生分布 |
| `metrics.py`（非时间指标 · memory/startup/GC/code-size） | ✅ 已写，回归覆盖缺项 / 单位 / 坏值 / 分布 / unavailable / `--require-metrics`；默认不强制，待真实多轮日志 |
| UnLuaNova 侧实现 | ✅ 已写，待在真实工程里跑一遍确认每个 UE 调用可达 |
| 外部绑定通用适配器 | ✅ 已写（UE 调用自适应 + fail-fast），静态契约 6 项回归通过；待在目标工程核对 API |
| workload 静态契约 | ✅ 用例集 / 迭代数 / revision / meta 字段 / 失败上报 / 无占位身份（6 项回归） |
| slua-unreal 侧 | ⚠️ 适配器已就绪，需在 slua-unreal 工程里实跑 |
| FUnLua 侧 | ⚠️ 适配器已就绪，需在 FUnLua 工程里实跑 |
| 跨项目结论 | ❌ 尚未建立 |

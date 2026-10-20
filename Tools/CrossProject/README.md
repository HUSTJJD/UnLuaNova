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
| `workload/unluanova/cases.lua` | UnLuaNova 侧实现（可直接跑） |
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
   `CrossProjectBench.lua`），先填 `Bench.Machine` 与 `Bench.Cpu`。
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

## 跑 slua-unreal / FUnLua 那两侧

这两侧**还没有实现**，需要在各自工程里按 [WORKLOAD.md](WORKLOAD.md) 的用例表各写一个
等价脚本，并打印同样的行格式。用例全部只用引擎自带类，所以不需要给它们加 C++；
把它们各自打印的日志和 UnLuaNova 的日志一起喂给 runner 即可。

在它们跑起来之前，**任何"比 slua 快"的说法都不成立**。README 与
[Docs/CN/Performance_Parity_slua.md](../Docs/CN/Performance_Parity_slua.md) 目前只陈述
自比数据与实现机制，就是这个原因。

## 状态

| 项 | 状态 |
|---|---|
| 契约与用例表 | ✅ revision 1 |
| runner（归一化 / 对齐检查 / 对比表） | ✅ 已自测：缺用例、machine 不一致、GBK 控制台均覆盖 |
| UnLuaNova 侧实现 | ✅ 已写，待在真实工程里跑一遍确认每个 UE 调用可达 |
| slua-unreal 侧 | ❌ 未实现 |
| FUnLua 侧 | ❌ 未实现 |
| 跨项目结论 | ❌ 尚未建立 |

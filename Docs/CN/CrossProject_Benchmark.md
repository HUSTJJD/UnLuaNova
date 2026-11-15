# 跨项目性能对照（UnLuaNova / slua-unreal / FUnLua）

本页说明如何**可复现地**建立 UnLuaNova、slua-unreal、FUnLua 三方的性能对照，以及为什么
现在还没有这样的对照。

## 1. 当前结论：跨项目性能对照尚未建立

**截至现在，跨项目性能对照尚未建立（`not-established`）。** 本仓库里没有任何来自
slua-unreal 或 FUnLua 的真实运行记录，`Tools/CrossProject/results/` 为空，也不存在任何
经过三方对齐的对比表。因此：

- 不能说 "UnLuaNova 比 slua-unreal 快"，也不能说 "比 FUnLua 快"；
- 不能给出任何三方倍数、排名或 "最快" 结论；
- 任何这类说法在三个项目都产出**对齐的真实运行记录**之前都不成立。

`Tools/CrossProject/README.md` 里也明确写着 "还没有跑过 slua-unreal 和 FUnLua"、"在它们
跑起来之前，任何'比 slua 快'的说法都不成立"。本页与那个表述一致，不推翻它。

`bootstrap_external.py status` 是这一事实的机器可读来源：当 `results/` 为空时，它输出
`conclusion: not-established`，并且 `comparison` 始终为 `null`。

## 2. README 的 2.08× 是自比，不是与 slua-unreal / FUnLua 的比较

主 `README.md` 里的 **2.08×** 指的是：在历史 UE 5.8.3 基线上、同一台机器、同一构建下，
**UnLuaNova 的 Luau 后端相对 Lua 5.5.1 后端**在容器工作负载上的速度（`Luau.Container.PointsAreCoplanar`
一例）。README 自己就声明了边界：

- "The table below is a 2026-10-24 self-comparison on the historical UE 5.8.3 baseline"；
- "This is a self-baseline: it guards against regressions"；
- "The data covers Lua 5.5.1 and Luau only. It is not a same-machine comparison against
  slua-unreal / FUnLua, so no 'faster than slua' claim is made."

也就是说，2.08× 是**本项目两个后端之间的自比**，与 slua-unreal、FUnLua 无关，不能拿来
支撑任何跨项目结论。`Tools/baseline/perf-*.json` 与 `Tools/CrossProject/README.md` 里已有的
数字同样是自比基线或待跑框架，不是跨项目结论。

## 3. 现状：框架已就绪，但三方都还没有对齐的真实运行

| 项 | 状态 |
|---|---|
| 工作负载契约（10 个用例、迭代数、预热/采样规则） | ✅ revision 1，见 `WORKLOAD.md` |
| 归一化 + 对齐检查 + 对比表 `runner.py` | ✅ 已写并有回归 |
| 单项目多轮分布 + 非时间指标 fail-closed `metrics.py` | ✅ 已写并有回归 |
| UnLuaNova 侧实现 `workload/unluanova/cases.lua` | ✅ 已写，待在真实工程里确认每个 UE 调用可达 |
| 外部绑定适配器 `workload/external/cases.lua` | ✅ 已写，待在目标工程核对 API |
| 外部项目清单 `external_projects.json` | ✅ 已写；**所有 pin 为 `UNKNOWN`**，许可/UE 版本按核实结果标注 |
| 引导脚本 `bootstrap_external.py` | ✅ prepare / check / status，离线可自检 |
| slua-unreal 真实运行记录 | ❌ 无（项目不在本仓库，从未在此构建/运行） |
| FUnLua 真实运行记录 | ❌ 无（项目不在本仓库，从未在此构建/运行；其许可授权亦未核实） |
| 三方对齐的对比表 | ❌ 无 |

`external_projects.json` 里**没有任何编造的 commit hash**：三个项目的 `pin` 都是字符串
`UNKNOWN`，并在 `pin_source` / `note` 里写明需要维护者在真实检出、真实构建、真实运行之后
填写当时实际使用的确切提交。许可依据 `Docs/CN/Comparison_sluaunreal.md` 与
`LICENSE-THIRD-PARTY.md`：slua-unreal 为 BSD-3-Clause；FUnLua 在所审计源码树中缺少
`LICENSE.TXT`，其 MIT 主张**未核实**，故标注 `UNVERIFIED` 且 `license_verified: false`。

## 4. 复现步骤（第三方一键复现的前置条件与流程）

只有严格按以下顺序做，且每一步都真实发生，才可能得到一张合法的对比表。

1. **准备外部工程**：用
   `python -B Tools/CrossProject/bootstrap_external.py prepare --root <工作区> --offline`
   先做离线自检，确认清单、目录结构、工作负载契约文件都齐全。真正 clone/检出由
   `prepare`（不带 `--offline`）在固定 pin 之后完成；目标默认落在 `<root>/Saved/CrossProject/external/`
   （`Saved/` 已被 gitignore），也可以把 `--root` 指到仓库外的目录。**不要往仓库内 clone。**
2. **固定提交**：维护者对每个项目做一次真实检出，把**当时实际构建并运行**的确切 commit
   hash 写回 `external_projects.json` 的 `pin`。在 pin 仍是 `UNKNOWN` 时，`prepare` 会
   fail-closed 拒绝联网检出（不会拉一个会漂移的 HEAD）；`check` 也会报告 `pins_set: false`。
   三个项目必须用**同一个 UE 版本**构建，并在清单里填 `ue_version`。
3. **对齐同一组条件**：三个项目必须在**同一台机器、同一引擎、同一构建配置、同一载体
   （Editor/Game）、同一用例集、同一迭代数、同一 workload revision** 下运行。`machine` /
   `cpu` 通过工作负载的 `SetMachineInfo(...)` 在运行时用真实、无空格的值提供；脚本不会替你
   填，也拒绝占位值（见第 5 节）。
4. **运行**：把 `workload/unluanova/cases.lua` 放进 UnLuaNova 工程，把
   `workload/external/cases.lua` 分别放进 slua-unreal / FUnLua 工程（各自 `SetProject(...)`），
   在真实 Editor 或 Game 里跑，收集日志里的 `#UNLUANOVA_BENCH_BEGIN` … `#UNLUANOVA_BENCH_END`
   块。**先读 `-- case failed:`**：某个 UE 调用不可达时该用例会以 `peropus=0` 报出，runner 会
   因此拒绝比较；请修调用，不要删用例。
5. **用 `metrics.py` 的 fail-closed 对齐检查**：先看单项目多轮分布
   （`metrics.py results/*.json -o results/metrics.md`），必要时用 `--require-metrics` 强制非
   时间指标。机器/CPU/引擎/配置/载体/revision/项目/后端/用例集/迭代数不一致、缺样本、坏指标
   值一律被拒绝，不会被平滑成一张好看但无意义的表。
6. **归一化并对齐**：把三份日志一起喂给
   `runner.py --baseline UnLuaNova <logs>`。它再次执行对齐检查（第 5 节的字段），任一不一致
   就打印原因并退出码 1，不出表。
7. **只有三方都有对齐的真实运行记录才能出对比表**：`runner.py` 仅在所有运行对齐后写出
   `comparison.md` 与各 `<project>-<backend>.json`。**在此之前，`status` 永远输出
   `conclusion: not-established`**，且绝不推断谁快谁慢。

## 5. 对齐契约的每一条字段，以及为什么 fail-closed

契约由 `runner.py`（对齐 + 归一化）和 `metrics.py`（单项目分布 + 非时间指标）共同执行，
定义见 `Tools/CrossProject/WORKLOAD.md`（revision 1）。

### 5.1 必须逐字一致的字段（`runner.ALIGNMENT_FIELDS`）

| 字段 | 含义 | 为什么必须一致 / 为什么 fail-closed |
|---|---|---|
| `engine` | UE 引擎版本 | 不同引擎的绑定开销、反射路径、优化都不同；跨引擎比是自欺 |
| `machine` | 主机标识 | 不同机器 CPU/内存/频率不同，绝对时间不可比 |
| `cpu` | CPU 型号 | 同上，且是方差的主要来源 |
| `config` | 构建配置（Development / Shipping…） | 优化级别不同，数值没有可比性 |
| `carrier` | 载体（Editor / Game） | 编辑器开销与独立游戏完全不同 |
| `workload_revision` | 工作负载修订号 | 改用例/迭代数必须 bump revision，否则不同定义被当成可比 |

### 5.2 必须报全的元数据（`runner.REQUIRED_META`）

`project`、`backend`、`engine`、`machine`、`cpu`、`config`、`carrier`、`workload_revision`。
缺任意一个，runner 报 "missing required meta field" 并退出码 1。身份不完整的结果无法归属，
更无法对齐。

### 5.3 占位身份拒绝

`machine` / `cpu` 若为 `""`、`?`、`fill-me`、`unknown`、`tbd`、`n/a`、`none`、
`unspecified` 或 `<...>` 形式，一律拒绝。工作负载源码里也**没有默认值**：没调用
`SetMachineInfo(...)` 时 `Bench.Run()` 直接拒绝输出，而不是打印一个占位串让 runner 事后才
发现。占位值能让两次无关的运行"看起来对齐"，正是这类对比最常见的造假方式。

### 5.4 用例集与迭代数

10 个用例的名字集合与每个用例的迭代数必须逐项一致（见 `WORKLOAD.md` 用例表）。缺用例、
多用例、迭代数漂移，runner 报 "missing cases / extra cases"，`metrics.py` 报
"case sets or iteration counts differ"。**"（因为少跑了慢用例）所以看起来更快"是这类对比最
常见的自欺**，所以宁可拒绝出表。这也是要求"请修调用，不要删用例"的原因。

### 5.5 可选非时间指标（revision 1 扩展，默认不强制）

独立 `#metric` 行，四项：`memory.peak_kb`(`kb`)、`startup.load_us`(`us`)、
`gc.pause_us`(`us`)、`code_size.bytes`(`bytes`)。单位枚举只有 `kb|us|bytes`，且每项单位固定。
拿不到就写 `value=unavailable`，**绝不写 0 或占位值**；`unavailable` 不参与数值分布，也不会
被当成 0。以下情况一律 fail-closed 拒绝（`metrics.py` 抛 `ValueError` / `main` 返回 1）：
缺项、指标集跨轮不一致、单位未知 / 串位 / 跨轮漂移、非有限数（NaN / inf）、负值、
非数值且不是 `unavailable`。默认（`--require-metrics` 未开）不强制，对旧 JSON 行为不变。

### 5.6 为什么整体是 fail-closed

跨机器、跨构建、用例数不同、带占位身份或坏指标的对比，会产出一张"看起来能用"但描述不了
任何东西的表。`runner.py` 与 `metrics.py` 的选择是：**宁可拒绝出表，也不给出一张会误导
人的表**。这套 fail-closed 是"只有对齐的真实运行才能出对比表"这一结论的技术保证。

## 6. 把结果写入 `Tools/CrossProject/results/`（格式说明）

`results/` 由 `runner.py` 在首次成功对齐后生成，不由手工编写、也不由 `bootstrap_external.py`
生成。**不要手写或伪造任何 `results/*.json`。**

- 每个真实运行一份归一化 JSON，文件名为 `<project>-<backend>.json`（小写），例如
  `unluanova-luau.json`、`slua-unreal-lua.json`、`funlua-lua.json`。顶层字段：
  `Project`、`Backend`、`EngineVersion`、`Machine`、`Cpu`、`BuildConfig`、`Carrier`、
  `WorkloadRevision`、`MeasuredUtc`、`SourceLog`，以及
  `Cases: [{Case, Iterations, PerOpUs, Samples}]`、可选的
  `Metrics: [{Name, Value, Unit}]`。`Samples` 是原始样本数组（5 段每次操作的 µs 均值），
  不只报中位数。
- `comparison.md`：仅当所有输入运行对齐后由 `runner.py` 写出的对比表。
- `metrics.md`：`metrics.py` 写出的**单项目**多轮分布，明确标注
  "not a cross-project conclusion"。

`bootstrap_external.py status` 只读取这些 JSON 的**事实元数据**（`Project`、`Backend`、
`EngineVersion`、`Machine`、`Cpu`、`BuildConfig`、`Carrier`、`WorkloadRevision`、
`MeasuredUtc`、`SourceLog`、用例数），**从不读取 `PerOpUs` / `Samples` / `Metrics` 的数值**，
从不计算比率、从不命名赢家、从不推断快慢。它报告"哪个项目有真实运行记录、哪个没有"；
当 `results/` 为空时输出 `record_state: empty` 与 `conclusion: not-established`。即便三方都
有了记录，`status` 仍不会自行建立结论 —— 结论需要 `runner.py` fail-closed 对齐通过并产出
`comparison.md` 才算数。

`--root` 决定读取哪个工作区的 `<root>/Tools/CrossProject/results/`；要读仓库里的标准结果，
把仓库根目录作为 `--root`（默认即是），或用 `--results-dir` 显式指定。

## 7. CI 里为什么不做这个对照

`.github/workflows/ci.yml` 不跑三方对照，原因是它无法满足本页的对齐前提：

- 需要**三个独立 UE 工程的真实构建**（UnLuaNova、slua-unreal、FUnLua），体量大、耗时长、
  依赖 Epic 引擎与各自的工具链，CI runner 不具备；
- 需要**同一台机器 / 同一引擎 / 同一配置**下真实 Editor/Game 运行，CI 环境是短暂、虚拟化、
  异构的，无法保证硬件与引擎版本跨任务一致；
- 需要联网 clone 外部仓库并固定提交，而仓库策略不向仓库内 clone、也不在 CI 里引入外部
  可变依赖；
- 性能数字本身是机器相关的，CI 上的数字不能作为对外结论。

因此 CI 只跑纯 Python 的、可离线复现的 fail-closed 契约测试
（`Tools/tests/test_crossproject_*.py` 与 `test_bootstrap_external.py`），验证"拒绝不一致、
拒绝占位、拒绝伪造、结果为空时说未建立"，而**不是**去产出跨项目性能结论。真实对照是维护者
在受控机器上的一次性人工流程。

## 8. `bootstrap_external.py` 用法速查

三个命令都支持 `--dry-run` / `--offline`，不联网即可完整自检；stdout 一律是结构化 JSON
（纯 ASCII），`check` 与 `status` 只读。

```powershell
# 离线校验：清单、pin、目录结构、工作负载契约文件；不联网、不写文件
python -B Tools/CrossProject/bootstrap_external.py prepare --root <工作区> --offline --dry-run

# 只读就绪检查：引擎路径、外部工程是否准备、契约文件、元数据能否对齐
python -B Tools/CrossProject/bootstrap_external.py check --root <工作区> --offline

# 诚实状态：哪个项目有真实运行记录、哪个没有；results 为空时 conclusion=not-established
python -B Tools/CrossProject/bootstrap_external.py status --root <工作区>
```

- `prepare`（不带 `--offline`）在**所有 pin 都是真实固定提交**时才 clone/检出；只要有一个 pin
  是 `UNKNOWN`，就在联网之前 fail-closed 拒绝并给出可诊断错误。网络或 git 失败会返回非零
  退出码并打印原因，**不会静默跳过**。
- `check` 不猜引擎安装、不编造引擎版本 / 机器 / CPU；找不到引擎路径时
  `engine.found: false` 且 `diagnostics` 里给出可诊断错误信息。
- `status` 的 `conclusion` 在当前状态下恒为 `not-established`，`comparison` 恒为 `null`。

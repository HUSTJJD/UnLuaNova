# Baselines

本目录保存已提交的参考数据：`perf-*.json` 是性能参考，其余是用例清单与分类。
**所有数值以 JSON 文件为准，本页只作说明**；若两者不一致，以 JSON 为准。

## 性能参考

`perf-lua.json` / `perf-luau.json` 已提交。发布校验要求后端、引擎版本、机器、用例集合、
迭代数、采样方法与样本数一致，任一用例慢于基线 1.25 倍即失败。

| 文件 | 后端 | 引擎版本 | 机器 | 测量时间 (UTC) | 采样方法 | SHA-256 前 16 位 |
|---|---|---|---|---|---|---|
| `perf-lua.json` | Lua | `6.0.0-0+UE6` | `davidsjian-PC2` | 2026-11-13T19:33:21.885Z | `median-of-5-batch-means` | `871DE8B8CB56685B` |
| `perf-luau.json` | Luau | `6.0.0-0+UE6` | `davidsjian-PC2` | 2026-11-14T01:23:21.887Z | `median-of-5-batch-means` | `1F22673BA67AA03A` |

两份文件都是 `Tolerance: 1.25`、`WarmupMinMs: 250`、`SampleMinMs: 100`。

**25% 门禁的适用边界（重要）**：门禁只在 `EngineVersion` 与 `Machine` 不变的同一基线内
成立。`Tools/ReleaseSupport.ps1::Sync-ReleasePerfBaseline` 在引擎版本或机器变化时会用
**本轮真实测量**覆盖已提交的基线（见 `CHANGELOG.md` 第九轮），那一轮是「自己比自己」，
25% 判定不适用。因此「门禁从未放宽」只应理解为「阈值常量始终是 1.25，从未调大」，
不是「每一轮都有独立的回退比较」。换引擎或换机器后，应在后续一轮保持基线不变，
再让该门禁真正生效。

采样：每个用例预热 ≥ 250 ms，再取 5 段各 ≥ 100 ms 的样本，报**批次均值的中位数**。
原始耗时与批次计数由发布校验器独立核对。两份文件都不是「修复前」测量，不能用来计算
修复带来的加速比。`README.md` 表格中的 `2.08×` 来自 2026-10-24 / UE 5.8.3 历史基线，
与当前 UE 6.0 基线不是同一组数据，**不要跨版本比较数值**。

范围：Win64 · Editor Development · UE 6.0.0-0+UE6 · MSVC 14.50 · AMD Ryzen 7 4700G ·
`davidsjian-PC2`。UE 5.8.3 的历史参考运行、单批次采样法与 2026-10/11-05 的测量记录
已被上述文件取代，只保留在 Git 历史中。不要用一次失败的验证运行替换基线来掩盖回退；
换机器、换引擎或有意的性能改动，应另行采集并审阅一次参考运行，在此记录其来源，
然后在后续一轮基线不变的验证中确认。

## 用例清单（漂移门禁）

一次「变绿」的运行悄悄少跑了用例，是这些文件要拦住的失效模式。
`GenerateTestReport.py --baseline <file>` 比对总数、用例名集合与耗时；
`--fail-on-drift` 在用例名增删时以非零码退出。完整 Release 对 Editor 用例与两个报告
阶段都传该标志，因此每个 carrier 都有自己的清单：

| 文件 | 后端 | Carrier | 用例数 | 记录时 failed | 生成时间 |
|---|---|---|---:|---:|---|
| `lua.json` | Lua | Editor | 691 | 23 | 2026-11-14 03:10:51 |
| `lua-game.json` | Lua | cooked `-game` | 662 | 2 | 2026-11-14 03:10:52 |
| `luau.json` | Luau | Editor | 716 | 0 | 2026-11-07 11:57:40 |
| `luau-game.json` | Luau | cooked `-game` | 687 | 0 | 2026-11-07 11:57:40 |

**`failed` 字段是记录时那一轮的快照，不是当前 Release 的结论。**
`lua.json` / `lua-game.json` 的 23 / 2 来自 2026-11-13 第八轮的失败轮，随后该轮真实报告
经 `--update-baseline` 刷新了用例名集合（`CHANGELOG.md` 第八轮）。当前结论一律以
`Saved/ReleaseTests/test/release-summary.json` 为准。历史章节里出现的 690 / 661 / 715 / 686
是更早一轮的用例数，已被上表取代。

这些是**执行**清单，不要手工编辑成「跑过但没跑」的用例。用例集合确需变更时，
先审阅 diff，再用真实运行刷新对应文件：

- Editor carriers：`Tools/RunDevelopmentTests.ps1` 加 `-UpdateBaseline` 与对应的 `-Backend`，
  从该轮运行重写 `Tools/baseline/<backend>.json`。
- Cooked carriers：`Tools/GenerateTestReport.py` 加 `--reports <game run dir>`、
  `--out <game run dir>`、`--backend <Backend>`、
  `--baseline Tools/baseline/<backend>-game.json` 与 `--update-baseline`。

`classification.json` 是发布报告用的静态用例环境分类，不是门禁输入。可选的联网矩阵由
`Tools/RunNetMatrix.ps1` 单独产出，其场景与变体不属于 Editor/Game 自动化清单，
由 `ValidateRelease.py --net-matrix` 校验。

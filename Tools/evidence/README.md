# 执行证据模式（`unluanova-evidence/1`）

## 它解决什么

`Tools/feature-matrix.json` 里每一项都有 `verification` 字段，而仓库检查**拒绝**把它从
`pending-runtime` 改成别的值：

```
{id}: execution claims require an evidence schema, not a status edit
```

意思是：**"跑过了"这件事不能靠改一个字段来宣称**，得有可被核对的证据。这个文件就是那个证据模式。

分工是这样的：

| 文件 | 谁改 | 说明 |
|---|---|---|
| `feature-matrix.json` 的 `status` | 人工 | 描述**实现范围**（implemented / partial / experimental / planned） |
| `feature-matrix.json` 的 `verification` | **不许改** | 永远是 `pending-runtime`，它是"这里没有证据"的占位 |
| `Tools/evidence/*.json` | 由 `RecordEvidence.py` 从真实报告生成 | 描述**执行结果** |
| `Docs/feature-matrix.md` | 由 `AuditRepository.py` 生成 | 把两者并排渲染出来 |

所以"40 项能力里有多少已经跑过"这个问题的答案，**不在矩阵里，在证据文件里**。

## Schema

一份 `Tools/evidence/<run-id>.json`：

```json
{
  "schema": "unluanova-evidence/1",
  "run_id": "2026-11-02-editor-luau",
  "commit": "93b78167...",
  "worktree_dirty": false,
  "engine": "5.8.3-0+UE5",
  "platform": "Win64",
  "config": "Development",
  "carrier": "Editor",
  "backend": "Luau",
  "machine": "davidsjian-PC2",
  "cpu": "AMD Ryzen 7 4700G with Radeon Graphics",
  "report_created_on": "2026.11.02-10.04.54",
  "totals": { "tests": 641, "passed": 641, "failed": 0, "not_run": 0 },
  "capabilities": {
    "UE-REFLECT": { "tests": 42, "passed": 42, "failed": 0,
                    "specs": ["Private/Specs/UnLuaNovaBase.spec.cpp"] }
  }
}
```

`capabilities` 以能力 id 为键，是从"这条用例属于哪个 spec、这个 spec 挂在哪个能力项下"
推出来的，**不是手填的**。`RecordEvidence.py` 扫描测试源码里的
`BEGIN_DEFINE_SPEC` / `BEGIN_TESTSUITE` 得到 spec 名，用最长前缀匹配把报告里的
`fullTestPath` 归到 spec，再按 `feature-matrix.json` 的 `tests` 字段归到能力项。

## 怎么用

跑完一轮 UE 自动化（`-ReportExportPath=<dir>`）之后：

```powershell
python Tools/RecordEvidence.py --report Saved\MyRun --backend Luau --carrier Editor --config Development
```

生成 `Tools/evidence/<run-id>.json` 并更新 `Tools/evidence/index.json`。
再跑 `python Tools/AuditRepository.py`，矩阵页会带上每项的证据行。

## 边界

- 证据只覆盖**这一轮跑过的组**。没跑的（cooked / 打包 / 联网 / Perf）不会出现。
- 一份证据只描述**一个后端 + 一个载体**。双后端要两份。
- `worktree_dirty` 为 true 时证据仍然记录，但矩阵页会标出来：源码和提交对不上。

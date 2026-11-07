# Luau 覆盖率导出与质量门禁

`LUAU-COVERAGE` 交付的内容：把 Luau 编译期插桩的行命中计数**导出成可复核的 JSON**，并用一个
fail-closed 门禁重算分母。它不是"测试覆盖率达到某个百分比"的宣称——百分比只对一次真实运行成立，
而且必须由 `Tools/ValidateCoverage.py` 从原始行计数重算出来。

## 它是什么、不是什么

| 是 | 不是 |
|---|---|
| Luau `lua_getcoverage` 读出的**每个可插桩行**命中次数 | 源码行覆盖率报告（没有源码文本映射） |
| 按可达的全局表 / `package.loaded` 路径命名的 per-source 数据 | 分支覆盖率、函数参数覆盖、C++ 代码覆盖 |
| 一份可以合并多轮运行、可被门禁重算的原始数据 | 执行证据 |
| Luau 后端专用 | 纯 Lua 后端能力（该后端没有 counters，命令会明确报不支持） |

C 闭包没有 Proto，也没有计数器，导出时跳过；遍历深度上限 8，并用访问过的表指针集合防环。
只有 `lua.coverage` 大于 0 之后加载的 chunk 带 `LOP_COVERAGE` 指令，**已经 `require` 过的模块
不会追溯生效**。这和 `lua.codegen`、`lua.sandbox` 的开关语义一致。

## 采集

```text
; 必须在脚本加载之前设置。1=语句，2=语句与表达式
luau.coverage 1

lua.coverage status
lua.coverage dump                                  ; Saved/Coverage/coverage-Luau.json
lua.coverage dump D:/work/coverage-run-a.json     ; 也可指定路径
```

`status` 报告当前编译级别并说明"已加载的 chunk 没有计数器"；`dump` 写出全量快照，
日志里给出函数数、命中行 / 插桩行与级别。快照是**累积**的：计数器不会被导出重置，
所以同一 VM 连续 dump 两次得到的是递增的命中数，门禁合并时按行取最大值。

没有可达 Lua 函数、没有附着 Env、写文件失败时命令报错而不写空报告——空报告不能当"0% 通过"。

## 导出格式

`schema_version` 固定为 1。`exclusions` 必须显式声明，门禁拒绝没有排除项的报告，
防止有人悄悄缩小分母。`totals` 由运行时写出，但门禁**不信任它**：会逐行重算并比对。

```json
{
  "schema_version": 1,
  "backend": "Luau",
  "coverage_level": 1,
  "generated_utc": "2026-11-03T00:00:00Z",
  "exclusions": ["IntelliSense", "<generated>"],
  "sources": [
    {
      "source": "_G.UnLuaNova.Event",
      "functions": [
        { "name": "Emit", "linedefined": 1, "depth": 0,
          "lines": [ { "line": 3, "hits": 7 }, { "line": 4, "hits": 0 } ] }
      ]
    }
  ],
  "totals": { "functions": 1, "covered_functions": 1,
              "instrumented_lines": 2, "hit_lines": 1 }
}
```

`hits` 为 0 表示这行被插桩但从未执行；Luau 用 -1 标记"该行没有插桩指令"，导出时直接丢弃，
所以 `instrumented_lines` 数的是真实插桩行，而不是调试信息跨过的所有行号。

## 门禁

```powershell
python Tools/ValidateCoverage.py `
    --report Saved/Coverage/coverage-Luau.json `
    --require-source "_G.UnLuaNova.Event" `
    --min-coverage 0.5 `
    --out Saved/Coverage/coverage-gate.json
```

`--report` 可重复，用于合并多轮 / 多载体运行。门禁会：

- 拒绝缺失、空、坏 JSON、schema 不符、后端不符、`coverage_level=0` 的报告；
- 逐行校验 `line` / `hits` 非负、同一函数内行号不重复，并重算全部 `totals`；
- 要求 `--require-source` 出现在**未被排除**的源里；
- 把命中排除项的源从分母剔除，但仍在结果中列出；分母为 0 直接失败；
- 按 `(source, function, linedefined, depth, line)` 合并多份报告，`hits` 取最大值；
- `hit_lines / instrumented_lines` 低于 `--min-coverage` 时失败；
- 把判定写成 `coverage-gate.json`（`passed` / `errors` / 分母 / 命中数 / 覆盖率），并以退出码 1 结束失败。

`--backend Lua` 会被直接拒绝：纯 Lua 后端没有 `lua_getcoverage`，覆盖率导出只适用于 Luau 后端。
门禁自身的单元测试在 `Tools/tests/test_validate_coverage.py`（15 项，不启动 UE），随
`python -B -m unittest discover -s Tools/tests -p "test_*.py"` 一起跑。

相关：[Luau 后端](Luau.md) · [控制台命令](ConsoleCommand.md) · [测试架构](Testing_Architecture.md)

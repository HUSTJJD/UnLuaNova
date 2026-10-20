# 跨项目工作负载定义（revision 1）

这份文件是"最快"这个主张能不能成立的依据。**改任何一条都要 bump `revision`**，
否则 runner 会把不同 revision 的结果当成可比的。

## 为什么要先定这个

三个项目的绑定 API 完全不同，各自跑各自的"hello world"基准没有任何可比性：
迭代数不同、计时方式不同、有的算了 GC 有的没算、有的在编辑器里跑有的在 game 里跑。
所以先固定**同一件事做几次**，再让每个项目用自己的 API 把它写出来。

## 契约

| 规则 | 值 |
|---|---|
| 一个用例 | `Fn(n)` 做 **n 次操作**，不多不少 |
| 预热 | 跑到累计 **≥ 250 ms** |
| 样本 | **5 段**，每段跑到累计 **≥ 100 ms** |
| 一段里 | 重复跑整批，直到这段耗时 ≥ 100 ms |
| 报什么 | `耗时 / (批次 × 迭代数)`，即**每次操作的均值**（µs） |
| 汇总 | 5 段均值的**中位数** |
| 原始样本 | 一并打印，不只报中位数 |

用"跑到 100 ms 为止"而不是"固定跑 N 次"，是为了让不同速度的用例都有可比的计时精度；
报每次操作均值而不是总耗时，是为了让不同迭代数的用例不会互相污染。

## 用例表（revision 1）

| 用例 | 迭代 | 做什么 | 为什么挑它 |
|---|---:|---|---|
| `VM.Arithmetic` | 200000 | 浮点累加循环 | 纯 VM 算力，不含任何 UE 交互 |
| `VM.Call` | 200000 | 调一个空 Lua 函数 | 函数调用开销 |
| `VM.TableRead` | 200000 | 读表字段 | 表查找 |
| `VM.TableGrow` | 200000 | 顺序插入新表 | 表扩容 + 分配 |
| `VM.StringConcat` | 100000 | 字符串拼接 | 字符串实现差异很大 |
| `UE.CallMath1` | 20000 | 调 1 参数的静态 UFUNCTION | 跨进 UE 的最低成本 |
| `UE.CallMath4` | 20000 | 调 4 参数的静态 UFUNCTION | 让参数编组成为主要成本 |
| `UE.StructRoundTrip` | 20000 | 构造 FVector + 向量加法 | 结构体传参与返回 |
| `UE.StringRoundTrip` | 20000 | string → FText → string | 字符串与 UE 的往返 |
| `GC.Collect` | 200 | 强制全量 GC | GC 停顿与实现差异 |

**为什么现在只有这些**：全部只用引擎自带类（`UKismetMathLibrary`、`UKismetTextLibrary`、
`FVector`），不需要往任何一个项目里加 C++。这样三边能立刻跑起来。

**第二轮要补的**（需要三个项目各加一个同构夹具类，写好 C++ 之后再加进本表并 bump revision）：
`UE.PropertyGetSet`、`UE.ContainerIterate`、`UE.OverrideCall`、`UE.DelegateFire`、
`UE.SpawnDestroy`、`UE.StructArrayCopy`、`Startup.LoadModule`。

## 对齐要求（runner 会强制，不通过就拒绝出表）

必须一致：`engine`、`machine`、`cpu`、`config`、`carrier`、`workload_revision`、
用例名集合、每个用例的迭代数。

必须报全：`project`、`backend`、`engine`、`machine`、`cpu`、`config`、`carrier`、
`workload_revision`。

不一致时 runner 打印原因并**退出码 1**，不会给你一张看起来能用的表。
"（因为少跑了慢用例）所以看起来更快"是这类对比最常见的自欺方式，所以宁可拒绝。

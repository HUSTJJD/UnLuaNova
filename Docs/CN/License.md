---
title: 许可与商业授权
description: UnLuaNova 采用 Polyform Small Business 1.0.0 并附加规模限定：小团队免费商用，超过阈值需购买商业授权
---

# 许可与商业授权

源码公开，但**不是无限制免费商用**。

UnLuaNova 采用 [Polyform Small Business 1.0.0](https://polyformproject.org/licenses/small-business/1.0.0)，
并附加了比许可原文更严格的规模限定。完整文本见仓库根目录的
[`LICENSE`](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE)。

## 我能不能用

规模按**集团合并计算**：你的组织与它的母公司、子公司、受共同控制的关联实体算作一个主体。
两个条件**同时**满足才免费：

| 条件 | 免费区间 |
|---|---|
| 员工 + 独立承包商合计 | **< 10 人** |
| 上一财年总营收与融资合计 | **< 1,000,000 USD** |

| 你的情况 | 结论 |
|:---|:---|
| 个人学习、业余项目、开源项目 | 免费 |
| 独立开发者 | 免费，可商用 |
| 小工作室（人数与营收都在阈值内） | 免费，可商用 |
| 超过 10 人，或营收/融资超过 100 万美元 | 需要[商业授权](https://github.com/HUSTJJD/UnLuaNova/blob/master/COMMERCIAL.md) |
| 大厂旗下的工作室/子公司 | 需要（按集团算） |
| 外包公司为大厂项目集成 | 需要（按最终受益方算） |
| 打包进你售卖的产品或中间件再分发 | 需要 |

!!! tip "不确定就问"
    开一个标注 `commercial licensing` 的 issue 说明你的场景，比事后补救便宜得多。
    按许可的 VIOLATIONS 条款，收到书面通知后有 32 天整改期，逾期授权立即终止。

## 商业授权

见 [`COMMERCIAL.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/COMMERCIAL.md)：
授权范围、价格档位、联系方式，以及"不买会怎样"。

商业版规划中的能力包括 DAP 调试适配器、Profiler/Insights 深度集成、
静态绑定生成器的命令行工具链，以及指定 UE 版本的适配保证与优先支持。

## 上游组件仍是 MIT

本次变更**不影响**这些组件，它们由各自的版权人按 MIT 授权，你可以单独按 MIT 使用：

* [Tencent/UnLua](https://github.com/Tencent/UnLua)（MIT）
* [Lua 5.5](https://www.lua.org/) — Lua.org, PUC-Rio（MIT）
* [Luau](https://github.com/luau-lang/luau)（MIT）

清单与条款原文见
[`LICENSE-THIRD-PARTY.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE-THIRD-PARTY.md)。

## 贡献需要签 CLA

提交 PR 前请先签
[`CLA.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/CLA.md) ——
它授予维护者把你的贡献**再许可**给商业客户的权利。没有它，商业授权在法理上卖不干净，
项目也就没有收入来继续维护你写的代码。流程见
[`CONTRIBUTING.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/CONTRIBUTING.md)。

!!! note "不是法律意见"
    本页是对许可条款的说明，不是法律意见。涉及合规审查的场景，请以 `LICENSE` 原文为准，
    并咨询你的法务。

---

## 延伸阅读

- [功能清单](Features.md) —— 哪些能力现在就有
- [Luau 后端](Luau.md) —— 后端相关的能力与限制
- [设置项](Settings.md) —— 后端选择与子模块初始化

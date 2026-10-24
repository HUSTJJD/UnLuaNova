# 许可与来源现状

本仓库根 `LICENSE` 包含 PolyForm Small Business 1.0.0 及附加条款；继承的 UnLua 和多个依赖有独立许可。不能将整个项目统一描述为 MIT，也不能把根许可自动套到所有上游文件。

## 当前需要权利人确认的事项

- 根许可附加第 2 条写“非为所定义的小型企业利益的使用需另行授权”，与开头和商业说明对个人/业余使用的概括不一致。
- 大量文件仍带 MIT 头，需要按来源和贡献授权明确哪些代码、哪些修改适用根许可。不要删除上游声明，也不要给原创代码错误添加 Tencent 版权。
- 扩展库、工具、二进制依赖和示例资产需要完整版本、来源和随包 notice 清单；当前清单仍有待补项。
- 商业条款与 CLA 不能自动证明历史贡献者或第三方代码的再授权范围。

本轮未改动根许可证或 CLA，以上是文本与来源核查发现的待确认事项，不是已完成的授权结论。根文件与详细清单请从仓库查看：[LICENSE](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE)、[第三方清单](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE-THIRD-PARTY.md)、[商业说明](https://github.com/HUSTJJD/UnLuaNova/blob/master/COMMERCIAL.md)。

## 对标项目

[slua-unreal 的许可](https://github.com/Tencent/sluaunreal/blob/7970ab72fd59118e1b3f86aac2d016828b54b748/LICENSE.TXT) 为 BSD-3-Clause。FUnLua README 声称 MIT，但本轮固定提交缺少其链接的许可文件；在核实前只记录设计，不复制实现。UnLua、Lua、Luau 的来源与版权声明继续保留。

产品对比不得暗示腾讯、Roblox 或 FUnLua 作者为本项目背书。复用代码必须记录准确来源与改动；独立实现应以行为需求和自主测试为依据。[当前审计](Benchmark_Review.md)

# 许可与来源

**UnLuaNova 采用 [MIT 许可](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE)。**
没有规模阈值，没有商业授权，没有附加条款：个人、爱好者、工作室与公司都可以免费使用，
包括商业用途。也不要求签 CLA，直接提 PR 就行。

## 覆盖与不覆盖

MIT 许可覆盖 UnLuaNova 自身的源码、文档、脚本与示例（即本仓库原创的部分）。

不覆盖、也不重新授权的部分：

| 部分 | 许可 | 说明 |
|---|---|---|
| 继承的 Tencent/UnLua 代码 | MIT，Copyright (C) 2019 Tencent | 保留其原有文件头与版权声明，按其自身条款可用 |
| Lua 5.5.1（子模块） | MIT，Lua.org / PUC-Rio | 见固定提交 `lua.h` 中的原文 |
| Luau（子模块） | MIT，Roblox Corporation / Lua.org | 保留 `LICENSE.txt` 与 `lua_LICENSE.txt` |
| LuaSocket、lua-protobuf、lua-rapidjson、RapidJSON | MIT（部分含 BSD-3 子组件） | 各自版权人授予 |
| `Tools/` 下的工具子模块 | 各自许可 | 分发其二进制时需一并携带 |
| Unreal Engine | Epic 的独立条款 | 本许可不授予引擎代码或资产的任何权利 |

完整清单见 [第三方来源与 notice](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE-THIRD-PARTY.md)，
依赖版本见 [VERSIONS.md](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/ThirdParty/VERSIONS.md)。

## 为什么改成 MIT

根许可曾为 PolyForm Small Business 1.0.0 加附加条款。它带来两个问题：一是与仓库里大量
继承文件的 MIT 头不一致，读者无法判断哪些代码适用哪一许可；二是"非为小型企业利益的使用需
另行授权"这类规模限定，让项目在真正被使用前就多了一道门槛。改为 MIT 后，根许可与上游文件头
一致，也不再有任何规模或商业授权条件。

## 历史遗留事项

改为 MIT 解决了许可本身的冲突，下面这些仍然需要补齐，与许可类型无关：

- 扩展库、工具、二进制依赖与示例资产仍需要完整的版本、来源与随包 notice 清单。
- 新增第三方代码时要记录准确的 URL / revision、保留原有声明并注明改动。
- 不要给原创代码错误添加 Tencent 版权，也不要删除上游声明。

## 对标项目

[slua-unreal](https://github.com/Tencent/sluaunreal/blob/7970ab72fd59118e1b3f86aac2d016828b54b748/LICENSE.TXT)
为 BSD-3-Clause，不能误写为 MIT。FUnLua README 声称 MIT，但本轮固定提交缺少其链接的许可文件；
在核实前只记录设计，不复制实现。

本项目与 slua-unreal、FUnLua 之间只存在设计对照关系：不代表其作者为本项目背书，
本许可也不授权复制其源码。UnLua、Lua、Luau 的来源与版权声明继续保留。

[当前审计](Benchmark_Review.md)

# 贡献指南

先说最重要的两条：

1. **本项目是 MIT 许可，不需要签 CLA。** 直接提 PR 就行。
2. **你的贡献会按 [LICENSE](LICENSE)（MIT）授权。** 提交前请确认你能接受；
   你仍然保留自己代码的版权，授予的是许可而不是转让。

## 提交流程

```bash
git submodule update --init --recursive   # Lua 与 Luau 都是子模块，不初始化编不过
```

1. Fork 并新建分支
2. 改动 + 自测
3. 跑验收：`.\Tools\RunTests.ps1` —— **这是唯一需要记住的命令**，不接受参数，
   已包含仓库审计、Python / PowerShell 工具回归、`mkdocs build --strict` 文档站、
   双后端自动化与打包、联网矩阵、证据 / 来源 / 转正门禁和发布标记刷新
4. 提 PR，说明动机与验证方式；涉及行为变更的，说明对现有项目的影响

需要单独装依赖、单独跑脚本、单独校验来源或单独构建文档站的时代已经结束：
`ReleasePipeline.ps1` 把每一项都做成了阶段，任何一项失败都会写进
`Saved/ReleaseTests/test/release-summary.json` 并让整轮失败。

## 代码

* C++ 遵循仓库根目录的 `.clang-format`（Allman / 4 空格 / 120 列）；新写或大改的文件提交前格式化。
  **不要**对继承的 UnLua 源码或 `Source/ThirdParty/` 做全库重排，那会产生无法评审的 diff。
  `.editorconfig` 统一缩进与尾随空白。GitHub Actions 只构建并推送文档站，不跑代码检查；
  本地 `Tools/RunTests.ps1` 会跑仓库审计、Python 单测、PowerShell 回归与文档站严格构建，
  格式本身仍靠提交者自律。
* 后端相关的改动请**两个后端都验证**（`LuaBackend=Luau` 与 `Lua`），
  或者明确用 `UNLUANOVA_HAS_*` 能力宏分支，而不是假定当前后端
* 新增能力请在 `LuaBackend.h` 里暴露能力宏，并在 `LuaBackend.spec.cpp` 里补用例
* 不确定的地方先开 issue 讨论，别先写两千行

## 文档

* 文档在 `Docs/`，中文 `CN/`、英文 `EN/`，改了就两边一起改；**新增页面必须同时创建 CN/EN 两份**，
  并各自挂进 `mkdocs.yml` 对应语言的导航（此前有 3 个页面只有中文版）
* 新增页面记得加 YAML front matter（`title` / `description`）、页尾「延伸阅读」
* 行为变更要同步更新 `Docs/CN/*`、`Docs/EN/*`、`CHANGELOG.md`，涉及引擎版本 / 平台 / 后端的
  还要更新《引擎版本与平台支持矩阵》
* **不要手工改** `release-status` / `release-provenance` 注释。流水线在收尾阶段调用
  `Tools/UpdateReleaseStatus.py` 从本轮摘要自动生成，随即执行 `--check` 自检；
  `ValidateProvenance.py --require-current` 也是同一轮 Release 的一个阶段，无需另跑

## 报告问题

开 issue 时请带上：引擎版本、Lua 后端、`.uproject` 相关配置、复现步骤与日志。
崩溃请附调用栈。

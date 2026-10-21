# 贡献指南

先说最重要的两条：

1. **提交 PR 前请签 [CLA](CLA.md)** —— 只需在 PR 里留一句 `I have read and agree to the CLA in CLA.md`。
   没有 CLA，商业授权在法理上卖不干净，项目也就没钱继续维护你写的代码。
2. **你的贡献会按 [LICENSE](LICENSE) 授权**（Polyform Small Business 1.0.0 + 附加条款），
   并可能被包含在商业授权里。这一点 CLA 里写得很清楚，提交前请确认你能接受。

## 提交流程

```bash
git submodule update --init --recursive   # Lua 与 Luau 都是子模块，不初始化编不过
```

1. Fork 并新建分支
2. 改动 + 自测
3. 跑测试：`.\Tools\RunTests.ps1`
4. 文档站能构建：`python -m mkdocs build --strict`（改了 Docs 就必须跑）
5. 提 PR，说明动机与验证方式；涉及行为变更的，说明对现有项目的影响

## 代码

* C++ 遵循仓库现有的 `.clang-format`；提交前格式化
* 后端相关的改动请**两个后端都验证**（`LuaBackend=Luau` 与 `Lua`），
  或者明确用 `UNLUANOVA_HAS_*` 能力宏分支，而不是假定当前后端
* 新增能力请在 `LuaBackend.h` 里暴露能力宏，并在 `LuaBackend.spec.cpp` 里补用例
* 不确定的地方先开 issue 讨论，别先写两千行

## 文档

* 文档在 `Docs/`，中文 `CN/`、英文 `EN/`，改了就两边一起改
* 新增页面记得加 YAML front matter（`title` / `description`）、页尾「延伸阅读」，
  并挂进 `mkdocs.yml` 的导航
* 行为变更要同步更新 `Docs/CN/*` 与 `CHANGELOG.md`

## 报告问题

开 issue 时请带上：引擎版本、Lua 后端、`.uproject` 相关配置、复现步骤与日志。
崩溃请附调用栈。

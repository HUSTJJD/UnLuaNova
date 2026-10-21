---
title: "测试报告（Luau 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Luau 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-21 14:46:08。请勿手工编辑。

## 结论

**没有找到任何自动化测试结果。**

这通常是测试进程没跑起来（引擎启动失败、过滤器没匹配到任何用例），而不是"全部通过"。先看同目录下的 `Editor.log`。
## 截图

没有截图。截图需要真实视口，跑的时候加 `-Screenshots`（脚本会关掉 `-NullRHI` 并把 `Saved/Screenshots` 收进报告目录）。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Backend Luau -Publish`（要两份就 `-Backend Both`）。


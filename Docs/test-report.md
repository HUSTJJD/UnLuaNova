---
title: "测试报告"
description: "自动化测试产出的通过率、失败明细与截图"
---

> 由 `Tools/GenerateTestReport.py` 自动生成于 — 。请勿手工编辑。

# 测试报告

**还没有生成过报告。**

在本机跑一次测试即可生成这一页：

```powershell
.\Tools\RunTests.ps1 -Publish
```

`-Publish` 会把 `report.md` 和截图复制到 `Docs/`（就是这一页和 `Images/tests/`），
提交后 GitHub Pages 会自动发布。详见 [测试与自动化](CN/Testing.md)。

之所以不放在 CI 里跑：UnLuaNova 依赖本地 UE 源码构建，GitHub 托管 runner 拿不到。

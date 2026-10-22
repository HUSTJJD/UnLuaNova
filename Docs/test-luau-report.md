---
title: "测试报告（Luau 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Luau 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-22 17:25:57。请勿手工编辑。

## 结论

**全部通过 ✅** — 通过率 100.0%

| 指标 | 数值 |
|---|---|
| 用例总数 | 4 |
| 通过 | 4 |
| 失败 | 0 |
| 跳过 | 0 |
| 总耗时 | 60.09s |
| 截图 | 4 |
| Lua 后端 | `Luau` |

## 通过明细（4 项）

| 用例 | 来源 | 耗时 |
|---|---|---|
| `Binding 静态绑定示例截图` | `index.json` | 15.568s |
| `Container 容器访问示例截图` | `index.json` | 14.905s |
| `Override 蓝图事件覆写示例截图` | `index.json` | 14.706s |
| `UMG UMG 与 Lua 生命周期示例截图` | `index.json` | 14.914s |
## 最慢的用例

| 用例 | 耗时 |
|---|---|
| `Binding 静态绑定示例截图` | 15.568s |
| `UMG UMG 与 Lua 生命周期示例截图` | 14.914s |
| `Container 容器访问示例截图` | 14.905s |
| `Override 蓝图事件覆写示例截图` | 14.706s |

## 截图

### Showcase_Binding

![Showcase_Binding](Images/tests-luau/Showcase_Binding.png)

### Showcase_Container

![Showcase_Container](Images/tests-luau/Showcase_Container.png)

### Showcase_Override

![Showcase_Override](Images/tests-luau/Showcase_Override.png)

### Showcase_UMG

![Showcase_UMG](Images/tests-luau/Showcase_UMG.png)

## 视频

### Showcase_Binding_Clip

<video controls muted loop playsinline width="640" src="Images/tests-luau/Showcase_Binding_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-luau/Showcase_Binding_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Container_Clip

<video controls muted loop playsinline width="640" src="Images/tests-luau/Showcase_Container_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-luau/Showcase_Container_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Override_Clip

<video controls muted loop playsinline width="640" src="Images/tests-luau/Showcase_Override_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-luau/Showcase_Override_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_UMG_Clip

<video controls muted loop playsinline width="640" src="Images/tests-luau/Showcase_UMG_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-luau/Showcase_UMG_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

## 静态类型检查

这一轮没有跑：`Tools/RunTests.ps1` 没写 `luau-analyze.json`。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Backend Luau -Publish`（要两份就 `-Backend Both`）。


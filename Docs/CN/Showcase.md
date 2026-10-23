---
title: "Showcase 示例"
description: "可运行示例清单：绑定、覆写、容器访问与 UMG"
---

# Showcase 示例

这一页汇集 UnLuaNova 的**可运行示例**：每个示例都是一小段真实代码 + 一张跑出来的截图，
用来回答"这东西到底怎么用"，而不只是描述 API 签名。

示例的源码在 `Source/UnLuaNovaDemo/` 下，配套 Lua 脚本在工程的 `Content/Script/`
目录里；本页的截图由自动化测试产出，不是手截的。

## 为什么截图要自动化

手截的图会过期——代码一改，图还是旧的，读者却以为它是当前的。所以截图走
自动化：跑 `Tools/RunTests.ps1 -Publish` 时顺带刷新，
和[测试报告](../test-luau-report.md)同一批产物。详见[测试与自动化](Testing.md)。

生成方式（只刷图，跑得很快）：

```powershell
.\Tools\RunTests.ps1 -Filter UnLuaNova.Showcase -Backend Luau -Publish
```

四条用例各打开一个教程关卡并截一张图，图落在 `Docs/Images/tests-luau/`：

| 用例 | 关卡 | 图片 |
|---|---|---|
| `UnLuaNova.Showcase.Binding` | `Content/Tutorials/01_HelloWorld` | `Showcase_Binding.png` |
| `UnLuaNova.Showcase.Override` | `Content/Tutorials/02_OverrideBlueprintEvents` | `Showcase_Override.png` |
| `UnLuaNova.Showcase.Container` | `Content/Tutorials/06_NativeContainers` | `Showcase_Container.png` |
| `UnLuaNova.Showcase.UMG` | `Content/Tutorials/11_ReleaseUMG` | `Showcase_UMG.png` |

!!! warning "要有视口才出图"
    截图需要真实渲染，`-NullRHI` 那一轮会把这四条整条剔除（不是记成失败）。
    刷图时不要加 `-NullRHI`。

## 示例清单

### 1. 绑定：UObject 与 Lua 双向调用

`UTutorialObject` 展示了最基础的一条链路——C++ 侧的类如何被 Lua 拿到、
Lua 侧的函数如何被 C++ 调回去。

要点：静态导出绑定把反射信息预生成好，运行时不需要遍历 UCLASS，
启动开销和 UObject 数量无关。

![绑定示例](../Images/tests-luau/Showcase_Binding.png)

### 2. 覆写：用 Lua 替换蓝图事件

`UTutorialBlueprintFunctionLibrary` 里的 `CallLuaByGlobalTable` 演示了从蓝图
侧进入 Lua、再由 Lua 覆写 `ReceiveBeginPlay` 这类事件的完整路径。

要点：覆写是**替换**而非追加，被覆写的实现默认不再执行；需要调用原实现时用
`Overridden` 系列的显式调用。

![覆写示例](../Images/tests-luau/Showcase_Override.png)

### 3. 容器：TArray / TMap / TSet 的 Lua 侧访问

`LuaLib_Array` / `LuaLib_Map` / `LuaLib_Set` 三个模块给出容器在 Lua 里的读写方式，
以及它们在 Lua 侧是引用语义（不拷贝容器本身）。`TSet` 现在也能 `pairs` 迭代。

![容器示例](../Images/tests-luau/Showcase_Container.png)

### 4. UMG：Lua 接管界面与生命周期

`11_ReleaseUMG` 演示 UMG 控件交给 Lua 之后，父子控件的创建、释放与引用关系如何收敛——
这也是最容易写出悬挂引用的一类场景。

![UMG 示例](../Images/tests-luau/Showcase_UMG.png)

调试相关能力集中在[调试](Debugging.md)一页；内存统计走 `lua.memstats`
控制台命令，可以按内存类别看到 Lua 侧的分配分布。

---

上面的图片注释会在下一次 `-Publish` 之后替换成真实图片（图已经由
`UnLuaNova.Showcase.*` 产出，等第一次发布进仓库后就取消注释）。
想补新示例：加一个示例类 + 对应的 Lua 脚本，再在
`Private/Tests/ShowcaseTest.cpp` 里加一条截图命令即可。

---

## 延伸阅读

- [测试与自动化](Testing.md) — 示例截图由自动化测试产出
- [测试报告（Lua 后端）](../test-lua-report.md) · [测试报告（Luau 后端）](../test-luau-report.md)
- [编程指南](UnLuaNova_Programming_Guide.md) — 编程指南

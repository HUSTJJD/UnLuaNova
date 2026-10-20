---
title: "Showcase 示例"
description: "可运行示例清单：绑定、覆写、容器访问与调试"
---

# Showcase 示例

这一页汇集 UnLuaNova 的**可运行示例**：每个示例都是一小段真实代码 + 一张跑出来的截图，
用来回答"这东西到底怎么用"，而不只是描述 API 签名。

示例的源码在 `Source/UnLuaNovaDemo/` 下，配套 Lua 脚本在工程的 `Content/Script/`
目录里；本页的截图由自动化测试产出，不是手截的。

## 为什么截图要自动化

手截的图会过期——代码一改，图还是旧的，读者却以为它是当前的。所以截图走
自动化：跑 `Tools/RunTests.ps1 -Screenshots -Publish` 时顺带刷新，
和[测试报告](../test-report.md)同一批产物。详见[测试与自动化](Testing.md)。

生成方式：

```powershell
.\Tools\RunTests.ps1 -Map /UnLuaNovaTestSuite/Tests/Misc/<关卡> -Screenshots -Publish `
    -ExtraCommands "UnLuaNovaTest.Screenshot Showcase_Binding"
```

## 示例清单

### 1. 绑定：UObject 与 Lua 双向调用

`UTutorialObject` 展示了最基础的一条链路——C++ 侧的类如何被 Lua 拿到、
Lua 侧的函数如何被 C++ 调回去。

要点：静态导出绑定把反射信息预生成好，运行时不需要遍历 UClass，
启动开销和 UObject 数量无关。

<!-- ![绑定示例](Images/tests/Showcase_Binding.png) -->

### 2. 覆写：用 Lua 替换蓝图事件

`UTutorialBlueprintFunctionLibrary` 里的 `CallLuaByGlobalTable` 演示了从蓝图
侧进入 Lua、再由 Lua 覆写 `ReceiveBeginPlay` 这类事件的完整路径。

要点：覆写是**替换**而非追加，被覆写的实现默认不再执行；需要调用原实现时用
`Overridden` 系列的显式调用。

<!-- ![覆写示例](Images/tests/Showcase_Override.png) -->

### 3. 容器：TArray / TMap / TSet 的 Lua 侧访问

`LuaLib_Array` / `LuaLib_Map` / `LuaLib_Set` 三个模块给出容器在 Lua 里的读写方式，
以及它们在 Lua 侧是引用语义（不拷贝容器本身）。

<!-- ![容器示例](Images/tests/Showcase_Container.png) -->

### 4. 调试：断点、堆栈与内存统计

调试相关能力集中在[调试](Debugging.md)一页；内存统计走 `lua.memstats`
控制台命令，可以按内存类别看到 Lua 侧的分配分布。

<!-- ![调试示例](Images/tests/Showcase_Debug.png) -->

---

截图占位注释会在首次 `-Publish` 之后替换成真实图片。想补新示例：
加一个示例类 + 对应的 Lua 脚本，再在测试里加一条截图命令即可。

---

## 延伸阅读

- [测试与自动化](Testing.md) — 示例截图由自动化测试产出
- [测试报告](../test-report.md) — 最新一次测试报告
- [编程指南](UnLuaNova_Programming_Guide.md) — 编程指南

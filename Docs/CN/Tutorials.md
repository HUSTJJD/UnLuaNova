---
title: "教程索引"
description: "13 个可运行教程的关卡、主脚本与能力说明"
---

# 教程索引

这里按编号列出 `Content/Tutorials/` 下的 13 个教程：每个教程是一个 `.umap` 关卡，配套一
份 Lua 主脚本。下面的能力说明与代码片段都取自仓库里实际存在的脚本，不含推测。

## 如何打开教程

在编辑器里双击打开 `Content/Tutorials/<编号>_<名字>/` 下的 `.umap`，然后点 Play（PIE）运行。
教程里的 `Screen.Print` 输出到屏幕，`print` 输出到 Output Log。部分教程需要主动按键触发
（03、04、06、11），10 号教程建议用多人模式运行。

## 前置条件

- **插件已编译**：`Plugins/UnLuaNova` 是 C++ 插件，工程需要先按 C++ 工程生成工程文件并编译。从零接入第三方工程的清单见 `examples/minimal/README.md`。
- **脚本目录是 `<工程>/Content/Script/`**：教程主脚本放在 `Content/Script/Tutorials/` 下，模块路径用点号分隔、不带后缀，例如 `Tutorials.01_HelloWorld`。`Content/Script/` 下的 `.lua` 在内容浏览器里看不到，请用系统文件浏览器查看。
- **教程关卡与蓝图资产属于本仓库的 UnLuaNovaDemo 工程**：第三方工程不会自动带上 `Content/Tutorials/`，要复现这些关卡需连同 `Content/Script/Tutorials/` 一起拷贝。

## 双后端与 Luau 专属示例

`LuaBackend` 在工程 `Config/DefaultUnLuaNova.ini` 里选择后端（编译期开关，改后需重新编译），
取值 `Lua`（官方 Lua）或 `Luau`（默认）。Luau 专属语法（`continue`、复合赋值、类型注解、
vector / buffer 等）在 `Lua` 后端下 `require` 的那一刻就是语法错误。

这类示例单独放在 `Content/Script/Luau/` 下，统一经守卫入口 `Content/Script/Luau/Index.lua`
访问：`require "Luau.Index"` 先用 `UnLuaNova.GetBackend()` 判断，只有确认是 Luau 才加载
`01_StrictTyping` 等示例文件，否则返回 `Available = false`。用法形如
`local Examples = require "Luau.Index"; if Examples.Available then ... end`。
本页 13 个教程的脚本不经过这个守卫入口。

## 教程清单

01 到 12 的主脚本是 `Content/Script/Tutorials/<编号>_<名字>.lua`；13 号没有编号主脚本，
差异见第 13 条。

### 1. 01_HelloWorld

- 关卡：`Content/Tutorials/01_HelloWorld/HelloWorld.umap`
- 主脚本：`Content/Script/Tutorials/01_HelloWorld.lua`
- 演示：最基础的静态绑定——蓝图实现 `UnLuaNovaInterface` 接口、通过 `GetModuleName` 指定脚本路径；以及绑定对象初始化时的回调与屏幕输出。
- 要点：`local M = UnLuaNova.Class()`，在 `M:Initialize()` 里 `print(msg)`，并用 `require "Tutorials.Screen"` 的 `Screen.Print(msg)` 打印 Hello World。注意这一课用的是 `Initialize`，不是 `ReceiveBeginPlay`。

### 2. 02_OverrideBlueprintEvents

- 关卡：`Content/Tutorials/02_OverrideBlueprintEvents/OverrideBlueprintEvents.umap`
- 主脚本：`Content/Script/Tutorials/02_OverrideBlueprintEvents.lua`
- 演示：用 Lua 覆盖蓝图事件与蓝图函数，并调用被覆盖的原实现。
- 要点：声明 `M:ReceiveBeginPlay()` 覆盖事件、`M:SayHi(name)` 覆盖函数；原实现通过 `self.Overridden.SayHi(self, name)` 调用，脚本注释特别提醒不能写成 `self.Overridden:SayHi(name)`。

### 3. 03_BindInputs

- 关卡：`Content/Tutorials/03_BindInputs/BindInputs.umap`
- 主脚本：`Content/Script/Tutorials/03_BindInputs.lua`
- 演示：按键与轴输入绑定，以及带修饰键的精细绑定。
- 要点：声明 `{KeyName}_Pressed` / `{KeyName}_Released` 即可接收按键，轴映射用同名函数；脚本循环生成 `M[key_name .. "_Pressed"]`，并用 `UnLuaNova.Input.BindKey(M, "C", "Pressed", fn, { Ctrl = true })` 绑定 Ctrl+C 这类组合键。

### 4. 04_DynamicBinding

- 关卡：`Content/Tutorials/04_DynamicBinding/DynamicBinding.umap`
- 主脚本：`Content/Script/Tutorials/04_DynamicBinding.lua`
- 演示：运行时把对象动态绑定到 Lua（不依赖蓝图实现接口）。
- 要点：鼠标左键 `World:SpawnActor(SpawnClass, Transform, AlwaysSpawn, self, self, "Tutorials.GravitySphereActor")`，右键 `NewObject(WidgetClass, self, nil, "Tutorials.IconWidget")`；最后一个参数是相对 `Content/Script/` 的模块路径，对应 `GravitySphereActor.lua` 与 `IconWidget.lua`。

### 5. 05_BindDelegates

- 关卡：`Content/Tutorials/05_BindDelegates/BindDelegates.umap`
- 主脚本：`Content/Script/Tutorials/05_BindDelegates.lua`
- 演示：UMG 控件委托的绑定 / 解绑，以及定时器委托。
- 要点：`Construct` 里 `self.ClickMeButton.OnClicked:Add(self, self.OnButtonClicked)`、`UE.UKismetSystemLibrary.K2_SetTimerDelegate({ self, self.OnTimer }, 1, true)`；`Destruct` 里成对 `Remove` 并 `K2_ClearAndInvalidateTimerHandle`。

### 6. 06_NativeContainers

- 关卡：`Content/Tutorials/06_NativeContainers/NativeContainers.umap`
- 主脚本：`Content/Script/Tutorials/06_NativeContainers.lua`
- 演示：在 Lua 里创建并操作 UE 原生容器 `TArray` / `TSet` / `TMap`。
- 要点：`UE.TArray(0)`、`UE.TSet(0)`、`UE.TMap(0, true)` 用示例值确定元素类型；数字键 1 / 2 / 3 分别触发 `One_Pressed` / `Two_Pressed` / `Three_Pressed`，演示 `Add`、`AddUnique`、`RemoveItem`、`Contains`、`Find`、`Clear` 等，结果打到控制台。

### 7. 07_CallLatentFunction

- 关卡：`Content/Tutorials/07_CallLatentFunction/CallLatentFunction.umap`
- 主脚本：`Content/Script/Tutorials/07_CallLatentFunction.lua`
- 演示：在 Lua 协程里调用 UE 的 Latent 延迟函数。
- 要点：协程函数 `run` 里循环调用 `UE.UKismetSystemLibrary.Delay(self, 1)`；`ReceiveBeginPlay` 用 `coroutine.resume(coroutine.create(run), self, "A")` 启动 A、B 两个并行协程，各打印 5 次。

### 8. 08_CppCallLua

- 关卡：`Content/Tutorials/08_CppCallLua/CppCallLua.umap`
- 主脚本：`Content/Script/Tutorials/08_CppCallLua.lua`
- 演示：从 C++ 侧调用 Lua 函数，分别是全局表方式与 `FLuaTable` 方式。
- 要点：`ReceiveBeginPlay` 调 `UE.UTutorialBlueprintFunctionLibrary.CallLuaByGlobalTable()` 与 `CallLuaByFLuaTable()`，两者最终调用 Lua 侧的 `M.CallMe(a, b)` 求和并返回；C++ 源码在 `Source/UnLuaNovaDemo/TutorialBlueprintFunctionLibrary.cpp`，工程 `*.Build.cs` 需加 `UnLuaNova` 依赖（访问 Lua 原生 API 再加 `Lua`）。

### 9. 09_StaticExport

- 关卡：`Content/Tutorials/09_StaticExport/StaticExport.umap`
- 主脚本：`Content/Script/Tutorials/09_StaticExport.lua`
- 演示：使用 C++ 静态导出的非 UObject 类型。
- 要点：`local tutorial = UE.FTutorialObject("教程")`，再取 `tutorial:GetTitle()`；该类型在 `Source/UnLuaNovaDemo/TutorialObject.cpp` 里由 `BEGIN_EXPORT_CLASS(FTutorialObject)` / `ADD_FUNCTION(GetTitle)` 导出。

### 10. 10_Replications

- 关卡：`Content/Tutorials/10_Replications/Replications.umap`
- 主脚本：`Content/Script/Tutorials/10_Replications.lua`
- 演示：网络复制 / RPC 覆写，建议多人模式运行；关卡内另有 `ChatCharacter.uasset`。
- 要点：编号主脚本只在服务端 `self:HasAuthority()` 时打印操作提示；真正演示 RPC 与同步的是配套角色脚本 `Content/Script/Tutorials/ChatCharacter.lua`，其中 `Say_Server_RPC`、`Say_Multicast_RPC`、`OnRep_Name` 对应 `{函数名}_RPC` 覆盖 RPC、`OnRep_{变量名}` 覆盖同步回调。

### 11. 11_ReleaseUMG

- 关卡：`Content/Tutorials/11_ReleaseUMG/ReleaseUMG.umap`
- 主脚本：`Content/Script/Tutorials/11_ReleaseUMG.lua`
- 演示：UMG 控件交给 Lua 后的释放与引用收敛流程，避免悬挂引用。
- 要点：`ReceiveBeginPlay` 用 `UE.UClass.Load("/Game/Tutorials/11_ReleaseUMG/ReleaseUMG_Root.ReleaseUMG_Root_C")` 加 `NewObject` 建根控件；按 `L` 执行 `collectgarbage("collect")`、按 `C` 执行 `UE.UKismetSystemLibrary.CollectGarbage()`；配套 `ReleaseUMG_Root.lua` / `ReleaseUMG_ItemParent.lua` / `ReleaseUMG_ItemChild.lua` 在 `Destruct` 里调 `self:Release()`。可用 `Obj List Class=...` / `Obj Refs Name=...` 查引用。

### 12. 12_CustomLoader

- 关卡：`Content/Tutorials/12_CustomLoader/CustomLoader.umap`
- 主脚本：`Content/Script/Tutorials/12_CustomLoader.lua`
- 演示：自定义 Lua 文件加载器，通过 `FUnLuaNovaDelegates::CustomLoadLuaFile` 委托实现。
- 要点：`ReceiveBeginPlay` 依次 `SetupCustomLoader(1)`（固定路径查找）、`SetupCustomLoader(2)`（按 `package.path` 查找，脚本先 `package.loaded["Tutorials"] = nil` 并追加 `./?/Index.lua`）、最后 `SetupCustomLoader(0)` 解绑；两个加载器的 C++ 实现在 `TutorialBlueprintFunctionLibrary.cpp`。

### 13. 13_AnimNotify

- 关卡：`Content/Tutorials/13_AnimNotify/AnimNotify.umap`（目录内另有 `AN_FootStep.uasset`、`AM_Walk.uasset`、`BP_AnimNotifyTutorialCharacter.uasset`）
- 主脚本：`Content/Script/Tutorials/AN_FootStep.lua`
- 命名差异：01 到 12 的主脚本都是带编号的 `<编号>_<名字>.lua`，与关卡目录同名；13 号**没有编号主脚本**，脚本是 `AN_FootStep.lua`，与关卡里的动画通知资产 `AN_FootStep.uasset` 同名。这里如实保留这个差异，不虚构一个 `13_AnimNotify.lua`。
- 演示：用 Lua 实现动画通知（AnimNotify）回调。
- 要点：脚本顶部标注 `---@type AN_FootStep_C : UAnimNotify`，`local M = UnLuaNova.Class()`，`function M:Received_Notify(MeshComp, Animation)` 里 `print("foot step")` 并 `return true`。

## 延伸阅读

- [Showcase 示例](Showcase.md) — 可运行示例与自动化截图
- [UE 新手快速上手](Quickstart_For_UE_Newbie.md) — 新建蓝图、绑定 Lua、生成模板
- [编程指南](UnLuaNova_Programming_Guide.md) — 开始写真正的逻辑

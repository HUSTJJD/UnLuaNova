---
title: "测试报告（Luau 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Luau 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-25 21:43:28。请勿手工编辑。

## 结论

**全部通过 ✅** — 通过率 100.0%

| 指标 | 数值 |
|---|---|
| 用例总数 | 641 |
| 通过 | 641 |
| 失败 | 0 |
| 跳过 | 0 |
| 总耗时 | 158.73s |
| 截图 | 4 |
| Lua 后端 | `Luau` |

## 通过明细（641 项）

| 用例 | 来源 | 耗时 |
|---|---|---|
| `UNLUANOVA.Regression.Issue362 TArray引用做委托参数在lua中修改不生效` | `index.json` | 0.040s |
| `UnLuaNova.API.Base.UnLuaNova::Call.支持多参数传入和传出` | `index.json` | 0.296s |
| `UnLuaNova.API.Base.UnLuaNova::Call.调用失败，返回值被标记为无效` | `index.json` | 0.055s |
| `UnLuaNova.API.Base.UnLuaNova::Call.调用成功，返回正确的返回值` | `index.json` | 0.056s |
| `UnLuaNova.API.Base.UnLuaNova::CallTableFunc.调用失败，返回值被标记为无效` | `index.json` | 0.053s |
| `UnLuaNova.API.Base.UnLuaNova::CallTableFunc.调用成功，返回正确的返回值` | `index.json` | 0.052s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入FName到Lua堆栈` | `index.json` | 0.050s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入FString到Lua堆栈` | `index.json` | 0.053s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入UObject到Lua堆栈` | `index.json` | 0.053s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入bool到Lua堆栈` | `index.json` | 0.058s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入const char*到Lua堆栈` | `index.json` | 0.047s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入double到Lua堆栈` | `index.json` | 0.053s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入float到Lua堆栈` | `index.json` | 0.056s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入int16到Lua堆栈` | `index.json` | 0.052s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入int32到Lua堆栈` | `index.json` | 0.050s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入int64到Lua堆栈` | `index.json` | 0.053s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入int8到Lua堆栈` | `index.json` | 0.056s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入struct到Lua堆栈` | `index.json` | 0.058s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入uint16到Lua堆栈` | `index.json` | 0.057s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入uint32到Lua堆栈` | `index.json` | 0.050s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入uint64到Lua堆栈` | `index.json` | 0.048s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入uint8到Lua堆栈` | `index.json` | 0.050s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入void*到Lua堆栈` | `index.json` | 0.047s |
| `UnLuaNova.API.Base.UnLuaNova::Push.正确传入中文FString到Lua堆栈` | `index.json` | 0.052s |
| `UnLuaNova.API.Base.UnLuaNova::RunChunk.执行失败返回false` | `index.json` | 0.059s |
| `UnLuaNova.API.Base.UnLuaNova::RunChunk.执行成功返回true` | `index.json` | 0.055s |
| `UnLuaNova.API.Base.支持调用包含按引用传递参数的函数.原生：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.054s |
| `UnLuaNova.API.Base.支持调用包含按引用传递参数的函数.原生：调用的函数本身没有返回值，依次返回参数` | `index.json` | 0.051s |
| `UnLuaNova.API.Base.支持调用包含按引用传递参数的函数.蓝图：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.056s |
| `UnLuaNova.API.Base.支持调用包含按引用传递参数的函数.蓝图：调用的函数本身没有返回值，依次返回参数` | `index.json` | 0.056s |
| `UnLuaNova.API.Binding.Dynamic 动态绑定，SpawnActor时指定脚本路径` | `index.json` | 0.067s |
| `UnLuaNova.API.Binding.Multiple 多重绑定：同一个Lua脚本绑定到不同类` | `index.json` | 0.073s |
| `UnLuaNova.API.Binding.Overridden 覆写：同一个Lua脚本绑定到不同类` | `index.json` | 0.049s |
| `UnLuaNova.API.Binding.Priority 绑定冲突：静态绑定优先于动态绑定` | `index.json` | 0.051s |
| `UnLuaNova.API.Binding.Static 静态绑定，继承UnLuaInterface::GetModuleName指定脚本路径` | `index.json` | 0.050s |
| `UnLuaNova.API.CallUEFast.分发条件：非网络 / 非 latent / 非接口函数会走预解析路径` | `index.json` | 0.039s |
| `UnLuaNova.API.CallUEFast.有静态绑定时出参与返回值都正确` | `index.json` | 0.041s |
| `UnLuaNova.API.CallUEFast.有静态绑定时，Lua 调用结果正确` | `index.json` | 0.040s |
| `UnLuaNova.API.CallUEFast.没有静态绑定时，同一个调用的结果一致` | `index.json` | 0.039s |
| `UnLuaNova.API.DanglingCheck.开关.默认关闭，关闭时 guard 是空的` | `index.json` | 0.041s |
| `UnLuaNova.API.DanglingCheck.调用结束时释放.关闭时留在缓存里（对照）` | `index.json` | 0.041s |
| `UnLuaNova.API.DanglingCheck.调用结束时释放.开启后，容器在调用返回时从 ScriptContainerMap 摘掉` | `index.json` | 0.041s |
| `UnLuaNova.API.DataTable.GetTableDataRowFromName.获取数据表中指定行的数据（C++）` | `index.json` | 0.048s |
| `UnLuaNova.API.DataTable.GetTableDataRowFromName.获取数据表中指定行的数据（蓝图）` | `index.json` | 0.051s |
| `UnLuaNova.API.DebugAdapter.Concurrent stop and restart use a fresh server` | `index.json` | 0.097s |
| `UnLuaNova.API.DebugAdapter.DAP framing preserves coalesced messages` | `index.json` | 0.026s |
| `UnLuaNova.API.DebugAdapter.DAP framing preserves fragmented UTF-8 bytes` | `index.json` | 0.032s |
| `UnLuaNova.API.DebugAdapter.DAP session accepts fragmented UTF-8 requests` | `index.json` | 1.543s |
| `UnLuaNova.API.DebugAdapter.一轮真实会话：initialize / attach / evaluate / threads / disconnect` | `index.json` | 0.244s |
| `UnLuaNova.API.DebugAdapter.未知命令回 error 而不是把连接挂死` | `index.json` | 0.089s |
| `UnLuaNova.API.DebugService.后端一致性.HasFeature 的回答与编译期常量一致` | `index.json` | 0.056s |
| `UnLuaNova.API.DebugService.局部变量.停下时读到当前帧的局部变量` | `index.json` | 0.046s |
| `UnLuaNova.API.DebugService.断点.停在声明的那一行，且只停一次` | `index.json` | 0.045s |
| `UnLuaNova.API.DebugService.断点.没声明的行不停` | `index.json` | 0.040s |
| `UnLuaNova.API.DebugService.求值.没有 attach 时求值失败而不是崩溃` | `index.json` | 0.055s |
| `UnLuaNova.API.DebugService.求值.没有停的时候按全局求值` | `index.json` | 0.052s |
| `UnLuaNova.API.DebugService.求值.表达式写错时返回 false 并给出原因` | `index.json` | 0.054s |
| `UnLuaNova.API.DebugService.调用栈.停在嵌套调用里时逐层展开` | `index.json` | 0.055s |
| `UnLuaNova.API.DefaultParamCollector.调用时填充默认值.显式传参会覆盖默认值` | `index.json` | 0.050s |
| `UnLuaNova.API.DefaultParamCollector.调用时填充默认值.枚举默认值也生效` | `index.json` | 0.056s |
| `UnLuaNova.API.DefaultParamCollector.调用时填充默认值.省略的 C++ 默认参数照样生效` | `index.json` | 0.047s |
| `UnLuaNova.API.DefaultParamCollector.集合.启动时已填充，且至少有一个函数带默认值` | `index.json` | 0.054s |
| `UnLuaNova.API.EKeys.比较FKey.不相等` | `index.json` | 0.041s |
| `UnLuaNova.API.EKeys.比较FKey.相等` | `index.json` | 0.050s |
| `UnLuaNova.API.EKeys.访问EKeys.F1` | `index.json` | 0.052s |
| `UnLuaNova.API.Enum.GetDisplayNameTextByValue.不存在的枚举值返回nil` | `index.json` | 0.052s |
| `UnLuaNova.API.Enum.GetDisplayNameTextByValue.获取枚举的本地化文本` | `index.json` | 0.053s |
| `UnLuaNova.API.Enum.GetMaxValue.获取枚举类型的最大值` | `index.json` | 0.055s |
| `UnLuaNova.API.Enum.GetNameStringByValue.不存在的枚举值返回nil` | `index.json` | 0.057s |
| `UnLuaNova.API.Enum.GetNameStringByValue.获取枚举的名称` | `index.json` | 0.062s |
| `UnLuaNova.API.ErrorHandling.引擎侧错误.参数类型错误是受控错误` | `index.json` | 0.050s |
| `UnLuaNova.API.ErrorHandling.引擎侧错误.对 nil 引擎对象调用方法是受控错误` | `index.json` | 0.058s |
| `UnLuaNova.API.ErrorHandling.引擎侧错误.调用不存在的 UFunction 是受控错误` | `index.json` | 0.053s |
| `UnLuaNova.API.ErrorHandling.编译错误.DoString 返回 false，且之后的环境仍可用` | `index.json` | 0.051s |
| `UnLuaNova.API.ErrorHandling.编译错误.load 返回 nil 与错误信息，而不是抛出` | `index.json` | 0.047s |
| `UnLuaNova.API.ErrorHandling.编译错误.错误信息带上 chunk 名与行号` | `index.json` | 0.052s |
| `UnLuaNova.API.ErrorHandling.运行时错误.DoString 返回 false 并把栈还原` | `index.json` | 0.054s |
| `UnLuaNova.API.ErrorHandling.运行时错误.pcall 捕获后错误不逃逸，后续调用照常` | `index.json` | 0.047s |
| `UnLuaNova.API.ErrorHandling.运行时错误.连续三次错误后栈顶不变` | `index.json` | 0.052s |
| `UnLuaNova.API.ErrorHandling.错误信息.不含机器绝对路径` | `index.json` | 0.054s |
| `UnLuaNova.API.FColor.Add.颜色相加` | `index.json` | 0.053s |
| `UnLuaNova.API.FColor.Set.设置R` | `index.json` | 0.057s |
| `UnLuaNova.API.FColor.Set.设置RG` | `index.json` | 0.060s |
| `UnLuaNova.API.FColor.Set.设置RGB` | `index.json` | 0.061s |
| `UnLuaNova.API.FColor.Set.设置RGBA` | `index.json` | 0.049s |
| `UnLuaNova.API.FColor.tostring().转为字符串` | `index.json` | 0.054s |
| `UnLuaNova.API.FColor.构造FColor.指定ColorValue` | `index.json` | 0.055s |
| `UnLuaNova.API.FColor.构造FColor.指定RGB` | `index.json` | 0.058s |
| `UnLuaNova.API.FColor.构造FColor.指定RGBA` | `index.json` | 0.065s |
| `UnLuaNova.API.FColor.构造FColor.默认参数` | `index.json` | 0.062s |
| `UnLuaNova.API.FIntPoint.Add.两点相加` | `index.json` | 0.059s |
| `UnLuaNova.API.FIntPoint.Div.两点相除` | `index.json` | 0.057s |
| `UnLuaNova.API.FIntPoint.Mul.两点相乘` | `index.json` | 0.064s |
| `UnLuaNova.API.FIntPoint.Set.设置X` | `index.json` | 0.064s |
| `UnLuaNova.API.FIntPoint.Set.设置XY` | `index.json` | 0.064s |
| `UnLuaNova.API.FIntPoint.Sub.两点相减` | `index.json` | 0.061s |
| `UnLuaNova.API.FIntPoint.tostring().转为字符串` | `index.json` | 0.060s |
| `UnLuaNova.API.FIntPoint.unm.取相反数` | `index.json` | 0.067s |
| `UnLuaNova.API.FIntPoint.构造FIntPoint.分别指定X和Y` | `index.json` | 0.066s |
| `UnLuaNova.API.FIntPoint.构造FIntPoint.同时指定X和Y` | `index.json` | 0.063s |
| `UnLuaNova.API.FIntPoint.构造FIntPoint.默认参数` | `index.json` | 0.068s |
| `UnLuaNova.API.FIntVector.Add.向量相加` | `index.json` | 0.064s |
| `UnLuaNova.API.FIntVector.Div.向量除以整数` | `index.json` | 0.068s |
| `UnLuaNova.API.FIntVector.Mul.向量乘以整数` | `index.json` | 0.057s |
| `UnLuaNova.API.FIntVector.Set.设置X` | `index.json` | 0.068s |
| `UnLuaNova.API.FIntVector.Set.设置X/Y` | `index.json` | 0.058s |
| `UnLuaNova.API.FIntVector.SizeSquared.获取平方后的长度` | `index.json` | 0.060s |
| `UnLuaNova.API.FIntVector.Sub.向量相减` | `index.json` | 0.062s |
| `UnLuaNova.API.FIntVector.tostring().转为字符串` | `index.json` | 0.061s |
| `UnLuaNova.API.FIntVector.unm.取反向量` | `index.json` | 0.063s |
| `UnLuaNova.API.FIntVector.构造FIntVector.分别指定X/Y/Z` | `index.json` | 0.060s |
| `UnLuaNova.API.FIntVector.构造FIntVector.同时指定X/Y/Z` | `index.json` | 0.056s |
| `UnLuaNova.API.FIntVector.构造FIntVector.默认参数` | `index.json` | 0.053s |
| `UnLuaNova.API.FLinearColor.Add.颜色相加` | `index.json` | 0.055s |
| `UnLuaNova.API.FLinearColor.Div.颜色相除` | `index.json` | 0.058s |
| `UnLuaNova.API.FLinearColor.Mul.颜色相乘` | `index.json` | 0.055s |
| `UnLuaNova.API.FLinearColor.Set.设置R` | `index.json` | 0.053s |
| `UnLuaNova.API.FLinearColor.Set.设置RG` | `index.json` | 0.063s |
| `UnLuaNova.API.FLinearColor.Set.设置RGB` | `index.json` | 0.054s |
| `UnLuaNova.API.FLinearColor.Set.设置RGBA` | `index.json` | 0.054s |
| `UnLuaNova.API.FLinearColor.Sub.颜色相减` | `index.json` | 0.052s |
| `UnLuaNova.API.FLinearColor.tostring().转为字符串` | `index.json` | 0.048s |
| `UnLuaNova.API.FLinearColor.构造FLinearColor.指定RGB` | `index.json` | 0.054s |
| `UnLuaNova.API.FLinearColor.构造FLinearColor.指定RGBA` | `index.json` | 0.058s |
| `UnLuaNova.API.FLinearColor.构造FLinearColor.默认参数` | `index.json` | 0.054s |
| `UnLuaNova.API.FLuaEnv.DoString.语法错误返回 false，且之后仍可用` | `index.json` | 0.057s |
| `UnLuaNova.API.FLuaEnv.DoString.运行时错误返回 false 且不污染栈顶` | `index.json` | 0.050s |
| `UnLuaNova.API.FLuaEnv.GC 与热重载.GC 之后环境仍然可用` | `index.json` | 0.047s |
| `UnLuaNova.API.FLuaEnv.GC 与热重载.HotReload 之后环境仍然可用` | `index.json` | 0.047s |
| `UnLuaNova.API.FLuaEnv.GC 与热重载.HotReload 可重复调用，且不顺手清掉环境` | `index.json` | 0.051s |
| `UnLuaNova.API.FLuaEnv.package 的兼容形状.package 上有 searchers / preload / config / path / cpath` | `index.json` | 0.048s |
| `UnLuaNova.API.FLuaEnv.package 的兼容形状.package.loaded 是 require 的缓存，第二次不重跑` | `index.json` | 0.049s |
| `UnLuaNova.API.FLuaEnv.package 的兼容形状.package.preload 里的模块被 require 到，并且优先于文件系统` | `index.json` | 0.053s |
| `UnLuaNova.API.FLuaEnv.创建Lua环境.启动模块不存在时不会被记成已加载` | `index.json` | 0.050s |
| `UnLuaNova.API.FLuaEnv.创建Lua环境.启动脚本能取到多个参数` | `index.json` | 0.053s |
| `UnLuaNova.API.FLuaEnv.创建Lua环境.多个环境之间的全局变量互不可见` | `index.json` | 0.061s |
| `UnLuaNova.API.FLuaEnv.创建Lua环境.支持启动脚本和参数` | `index.json` | 0.063s |
| `UnLuaNova.API.FLuaEnv.创建Lua环境.支持多个Lua环境` | `index.json` | 0.074s |
| `UnLuaNova.API.FLuaEnv.模块加载.require 不存在的模块报错，而不是返回 nil` | `index.json` | 0.056s |
| `UnLuaNova.API.FLuaEnv.模块加载.内建 loader 能提供模块` | `index.json` | 0.058s |
| `UnLuaNova.API.FLuaEnv.模块加载.搜索路径命中 Content/Script 下的真实模块` | `index.json` | 0.053s |
| `UnLuaNova.API.FLuaEnv.模块加载.文件系统优先于内建 loader` | `index.json` | 0.057s |
| `UnLuaNova.API.FLuaEnv.模块加载.自定义 loader 能提供文件系统里没有的模块` | `index.json` | 0.060s |
| `UnLuaNova.API.FLuaEnv.环境的生命周期.GetAll 与 FindEnv 能找到新环境` | `index.json` | 0.062s |
| `UnLuaNova.API.FLuaEnv.环境的生命周期.OnCreated 与 OnDestroyed 各触发一次` | `index.json` | 0.060s |
| `UnLuaNova.API.FMulticastScriptDelegate.Add.添加绑定` | `index.json` | 0.058s |
| `UnLuaNova.API.FMulticastScriptDelegate.Add.添加绑定：UFunction` | `index.json` | 0.056s |
| `UnLuaNova.API.FMulticastScriptDelegate.Add.添加绑定：赋值` | `index.json` | 0.055s |
| `UnLuaNova.API.FMulticastScriptDelegate.Broadcast.广播事件` | `index.json` | 0.051s |
| `UnLuaNova.API.FMulticastScriptDelegate.Clear.清理所有的绑定` | `index.json` | 0.061s |
| `UnLuaNova.API.FMulticastScriptDelegate.Remove.移除绑定` | `index.json` | 0.065s |
| `UnLuaNova.API.FMulticastScriptDelegate.Remove.移除绑定：UFunction` | `index.json` | 0.055s |
| `UnLuaNova.API.FQuat.Mul.四元数乘以浮点数` | `index.json` | 0.056s |
| `UnLuaNova.API.FQuat.Normalize.大于等于容差，归一化` | `index.json` | 0.055s |
| `UnLuaNova.API.FQuat.Normalize.小于容差，不归一化` | `index.json` | 0.051s |
| `UnLuaNova.API.FQuat.Set.设置X` | `index.json` | 0.059s |
| `UnLuaNova.API.FQuat.Set.设置X/Y` | `index.json` | 0.054s |
| `UnLuaNova.API.FQuat.Set.设置X/Y/Z` | `index.json` | 0.053s |
| `UnLuaNova.API.FQuat.Set.设置X/Y/Z/W` | `index.json` | 0.052s |
| `UnLuaNova.API.FQuat.tostring().转为字符串` | `index.json` | 0.049s |
| `UnLuaNova.API.FQuat.构造FQuat.分别指定X/Y/Z/W` | `index.json` | 0.054s |
| `UnLuaNova.API.FQuat.构造FQuat.指定轴与角度` | `index.json` | 0.055s |
| `UnLuaNova.API.FQuat.构造FQuat.默认参数` | `index.json` | 0.055s |
| `UnLuaNova.API.FRotator.Set.设置Pitch` | `index.json` | 0.065s |
| `UnLuaNova.API.FRotator.Set.设置Pitch/Yaw` | `index.json` | 0.064s |
| `UnLuaNova.API.FRotator.Set.设置Pitch/Yaw/Roll` | `index.json` | 0.065s |
| `UnLuaNova.API.FRotator.tostring().转为字符串` | `index.json` | 0.065s |
| `UnLuaNova.API.FRotator.构造FRotator.分别指定Pitch/Yaw/Roll` | `index.json` | 0.072s |
| `UnLuaNova.API.FRotator.构造FRotator.同时指定Pitch/Yaw/Roll` | `index.json` | 0.066s |
| `UnLuaNova.API.FRotator.构造FRotator.默认参数` | `index.json` | 0.058s |
| `UnLuaNova.API.FScriptDelegate.Bind.绑定` | `index.json` | 0.057s |
| `UnLuaNova.API.FScriptDelegate.Bind.绑定：UFunction` | `index.json` | 0.053s |
| `UnLuaNova.API.FScriptDelegate.Bind.绑定：赋值` | `index.json` | 0.055s |
| `UnLuaNova.API.FScriptDelegate.Execute.执行委托` | `index.json` | 0.058s |
| `UnLuaNova.API.FScriptDelegate.Execute.执行委托：从Lua返回值` | `index.json` | 0.055s |
| `UnLuaNova.API.FScriptDelegate.Execute.执行委托：赋值` | `index.json` | 0.054s |
| `UnLuaNova.API.FScriptDelegate.Unbind.解绑` | `index.json` | 0.054s |
| `UnLuaNova.API.FTransform.BlendWith.将当前Transform与目标Transform混合` | `index.json` | 0.058s |
| `UnLuaNova.API.FTransform.BlendWith.将当前Transform与目标两个Transform混合` | `index.json` | 0.065s |
| `UnLuaNova.API.FTransform.tostring().转为字符串` | `index.json` | 0.060s |
| `UnLuaNova.API.FTransform.构造FTransform.指定Rotation(FQuat)` | `index.json` | 0.062s |
| `UnLuaNova.API.FTransform.构造FTransform.指定Rotation(FQuat)/Translation` | `index.json` | 0.061s |
| `UnLuaNova.API.FTransform.构造FTransform.指定Rotation(FQuat)/Translation/Scale` | `index.json` | 0.068s |
| `UnLuaNova.API.FTransform.构造FTransform.指定Rotation(FRotator)` | `index.json` | 0.071s |
| `UnLuaNova.API.FTransform.构造FTransform.指定Rotation(FRotator)/Translation` | `index.json` | 0.062s |
| `UnLuaNova.API.FTransform.构造FTransform.指定Rotation(FRotator)/Translation/Scale` | `index.json` | 0.065s |
| `UnLuaNova.API.FTransform.构造FTransform.指定Translation` | `index.json` | 0.060s |
| `UnLuaNova.API.FTransform.构造FTransform.默认参数` | `index.json` | 0.062s |
| `UnLuaNova.API.FVector.Add.向量相加` | `index.json` | 0.062s |
| `UnLuaNova.API.FVector.Div.向量除以浮点数` | `index.json` | 0.067s |
| `UnLuaNova.API.FVector.Mul.向量乘以浮点数` | `index.json` | 0.062s |
| `UnLuaNova.API.FVector.Normalize.大于等于容差，归一化并返回true` | `index.json` | 0.055s |
| `UnLuaNova.API.FVector.Normalize.小于容差，直接返回false` | `index.json` | 0.060s |
| `UnLuaNova.API.FVector.Set.设置X` | `index.json` | 0.066s |
| `UnLuaNova.API.FVector.Set.设置X/Y` | `index.json` | 0.058s |
| `UnLuaNova.API.FVector.Set.设置X/Y/Z` | `index.json` | 0.064s |
| `UnLuaNova.API.FVector.Sub.向量相减` | `index.json` | 0.066s |
| `UnLuaNova.API.FVector.tostring().转为字符串` | `index.json` | 0.066s |
| `UnLuaNova.API.FVector.unm.取反向量` | `index.json` | 0.067s |
| `UnLuaNova.API.FVector.构造FVector.分别指定X/Y/Z` | `index.json` | 0.066s |
| `UnLuaNova.API.FVector.构造FVector.同时指定X/Y/Z` | `index.json` | 0.067s |
| `UnLuaNova.API.FVector.构造FVector.默认参数` | `index.json` | 0.057s |
| `UnLuaNova.API.FVector2D.Add.向量相加` | `index.json` | 0.055s |
| `UnLuaNova.API.FVector2D.Div.向量除以数字` | `index.json` | 0.045s |
| `UnLuaNova.API.FVector2D.Mul.向量乘以数字` | `index.json` | 0.047s |
| `UnLuaNova.API.FVector2D.Normalize.大于等于容差，归一化` | `index.json` | 0.053s |
| `UnLuaNova.API.FVector2D.Normalize.小于容差，不归一化` | `index.json` | 0.046s |
| `UnLuaNova.API.FVector2D.Set.设置X` | `index.json` | 0.048s |
| `UnLuaNova.API.FVector2D.Set.设置X/Y` | `index.json` | 0.057s |
| `UnLuaNova.API.FVector2D.Sub.向量相减` | `index.json` | 0.055s |
| `UnLuaNova.API.FVector2D.tostring().转为字符串` | `index.json` | 0.049s |
| `UnLuaNova.API.FVector2D.unm.取反向量` | `index.json` | 0.050s |
| `UnLuaNova.API.FVector2D.构造FVector2D.分别指定X/Y` | `index.json` | 0.053s |
| `UnLuaNova.API.FVector2D.构造FVector2D.同时指定X/Y` | `index.json` | 0.048s |
| `UnLuaNova.API.FVector2D.构造FVector2D.默认参数` | `index.json` | 0.053s |
| `UnLuaNova.API.FVector4.Add.向量相加` | `index.json` | 0.050s |
| `UnLuaNova.API.FVector4.Div.向量除` | `index.json` | 0.049s |
| `UnLuaNova.API.FVector4.Mul.向量乘` | `index.json` | 0.050s |
| `UnLuaNova.API.FVector4.Set.设置X` | `index.json` | 0.049s |
| `UnLuaNova.API.FVector4.Set.设置X/Y` | `index.json` | 0.051s |
| `UnLuaNova.API.FVector4.Set.设置X/Y/Z` | `index.json` | 0.048s |
| `UnLuaNova.API.FVector4.Set.设置X/Y/Z/W` | `index.json` | 0.046s |
| `UnLuaNova.API.FVector4.Sub.向量相减` | `index.json` | 0.053s |
| `UnLuaNova.API.FVector4.tostring().转为字符串` | `index.json` | 0.051s |
| `UnLuaNova.API.FVector4.unm.取反向量` | `index.json` | 0.052s |
| `UnLuaNova.API.FVector4.构造FVector4.指定X` | `index.json` | 0.050s |
| `UnLuaNova.API.FVector4.构造FVector4.指定X/Y` | `index.json` | 0.052s |
| `UnLuaNova.API.FVector4.构造FVector4.指定X/Y/Z` | `index.json` | 0.053s |
| `UnLuaNova.API.FVector4.构造FVector4.指定X/Y/Z/W` | `index.json` | 0.048s |
| `UnLuaNova.API.FVector4.构造FVector4.默认参数` | `index.json` | 0.050s |
| `UnLuaNova.API.GcScheduler.帧分摊.TimeSliced 下分帧步进能回收掉先建后弃的垃圾` | `index.json` | 0.058s |
| `UnLuaNova.API.GcScheduler.帧分摊.堆超过硬上限时改为整轮回收` | `index.json` | 0.054s |
| `UnLuaNova.API.GcScheduler.帧分摊.每帧预算会限制单次步进的量` | `index.json` | 0.052s |
| `UnLuaNova.API.GcScheduler.模式.Automatic 模式下 Tick 不做任何步进` | `index.json` | 0.049s |
| `UnLuaNova.API.GcScheduler.模式.默认是 Automatic，切换后可切回` | `index.json` | 0.051s |
| `UnLuaNova.API.GcScheduler.统计.帧数与峰值堆随 Tick 增长` | `index.json` | 0.054s |
| `UnLuaNova.API.Latent.主线程调用.在主线程里调用 latent 函数会报出契约里的那条消息` | `index.json` | 0.055s |
| `UnLuaNova.API.Latent.主线程调用.报错之后环境仍然可用` | `index.json` | 0.055s |
| `UnLuaNova.API.Lifetime.Smart pointer finalization.derived container arrays release their element type reference` | `index.json` | 0.052s |
| `UnLuaNova.API.Lifetime.Smart pointer finalization.padded smart pointer releases on Env close` | `index.json` | 0.051s |
| `UnLuaNova.API.Lifetime.Smart pointer finalization.registry shared pointer releases its owner at GC` | `index.json` | 0.053s |
| `UnLuaNova.API.Lifetime.Smart pointer finalization.released payload cannot be destroyed twice` | `index.json` | 0.049s |
| `UnLuaNova.API.Lifetime.手动引用.AddManualObjectReference 让对象不被 GC` | `index.json` | 0.210s |
| `UnLuaNova.API.Lifetime.手动引用.Ref proxy retains until Lua GC releases it` | `index.json` | 0.153s |
| `UnLuaNova.API.Lifetime.手动引用.RemoveManualObjectReference 后可以被 GC` | `index.json` | 0.106s |
| `UnLuaNova.API.Lifetime.手动引用.collecting an old proxy cannot release a replacement` | `index.json` | 0.173s |
| `UnLuaNova.API.Lifetime.手动引用.live manual proxy is released when Env closes` | `index.json` | 0.108s |
| `UnLuaNova.API.Lifetime.环境销毁.销毁环境不会顺手销毁被引用的对象` | `index.json` | 0.119s |
| `UnLuaNova.API.Lifetime.脚本持有的对象.只被 Lua 引用的对象会被引擎 GC 回收` | `index.json` | 0.106s |
| `UnLuaNova.API.Lifetime.脚本持有的对象.对象被标记销毁后，从 Lua 访问是受控错误` | `index.json` | 0.116s |
| `UnLuaNova.API.Lifetime.释放标记.IsReleasedPtr 只认那一个哨兵值` | `index.json` | 0.046s |
| `UnLuaNova.API.Locators.模块定位器.ByPackage 对原生类返回类名，与默认不同` | `index.json` | 0.047s |
| `UnLuaNova.API.Locators.模块定位器.换成自定义子类后用的是子类的结果` | `index.json` | 0.044s |
| `UnLuaNova.API.Locators.模块定位器.没有实现接口的对象得到空串` | `index.json` | 0.045s |
| `UnLuaNova.API.Locators.模块定位器.设置项切换后可写回原值` | `index.json` | 0.043s |
| `UnLuaNova.API.Locators.模块定位器.默认定位器对实现了接口的对象返回 GetModuleName 的结果` | `index.json` | 0.039s |
| `UnLuaNova.API.Locators.环境定位器.自定义子类把对象定位到指定环境` | `index.json` | 0.043s |
| `UnLuaNova.API.Locators.环境定位器.默认环境定位器返回的是非空环境` | `index.json` | 0.048s |
| `UnLuaNova.API.Locators.默认定位器（ByBinding）.什么都没配时返回空串` | `index.json` | 0.052s |
| `UnLuaNova.API.Locators.默认定位器（ByBinding）.命中之后缓存，改配置不影响已解析过的类` | `index.json` | 0.051s |
| `UnLuaNova.API.Locators.默认定位器（ByBinding）.接口优先于配置` | `index.json` | 0.048s |
| `UnLuaNova.API.Locators.默认定位器（ByBinding）.配置写完整路径能命中原生类` | `index.json` | 0.043s |
| `UnLuaNova.API.Locators.默认定位器（ByBinding）.配置只写类名也能命中，并沿继承链向上找` | `index.json` | 0.043s |
| `UnLuaNova.API.Locators.默认定位器（ByBinding）.配置默认值就是 ByBinding` | `index.json` | 0.040s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.HasFeature 与编译期能力一致` | `index.json` | 0.041s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.Luau 语法能编译并按预期求值` | `index.json` | 0.040s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.UnLuaNova.Vector 能把原生 vector 转成 FVector` | `index.json` | 0.045s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.buffer 能按类型读写` | `index.json` | 0.040s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.分配回调能收到分配事件` | `index.json` | 0.043s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.原生 vector 能进出 FVector` | `index.json` | 0.044s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.只读表能置位也能读回` | `index.json` | 0.042s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.断点原语可用` | `index.json` | 0.041s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.沙盒下 UE 命名空间仍能写入：类型注册不会被冻住` | `index.json` | 0.040s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.沙盒开启后全局表只读` | `index.json` | 0.043s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.编译期能力位与后端一致` | `index.json` | 0.045s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.脚本改写 UE 命名空间被拦截` | `index.json` | 0.043s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.脚本能问到当前后端` | `index.json` | 0.048s |
| `UnLuaNova.API.LuaBackend.Luau 专属能力.迭代元方法注册在后端认识的名字下` | `index.json` | 0.045s |
| `UnLuaNova.API.LuaBackend.Luau 示例目录.require 入口在两个后端下都安全返回` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.Profiler 与调试.VM hook 能安装、收到事件并按 owner 撤回` | `index.json` | 0.042s |
| `UnLuaNova.API.LuaBackend.Profiler 与调试.栈帧能取到源码行，Profiler 键稳定且唯一` | `index.json` | 0.046s |
| `UnLuaNova.API.LuaBackend.Profiler 与调试.调试通道原语可用` | `index.json` | 0.044s |
| `UnLuaNova.API.LuaBackend.协程.UNLUANOVA_IS_YIELDABLE 区分主线程与协程` | `index.json` | 0.046s |
| `UnLuaNova.API.LuaBackend.协程.UNLUANOVA_RESUME 能恢复协程并带回结果` | `index.json` | 0.047s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.HotReload 能加载模块，而不是静默降级` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.debug 提供 upvalue / local / registry 访问` | `index.json` | 0.047s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.debug.getupvalue 能读到闭包捕获的值` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.debug.setupvalue 能给 chunk 设环境` | `index.json` | 0.044s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.luaL_newmetatable 建的元表带 __name` | `index.json` | 0.046s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.luaL_newmetatable 第二次调用返回 0` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.luaL_ref / luaL_unref 往返一致` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.package 同时提供 loaded 和 searchers` | `index.json` | 0.048s |
| `UnLuaNova.API.LuaBackend.后端能力对齐.脚本依赖的标准库在两个后端都在` | `index.json` | 0.047s |
| `UnLuaNova.API.LuaBackend.后端适配层.64 位整数读写不截断` | `index.json` | 0.052s |
| `UnLuaNova.API.LuaBackend.后端适配层.GC 设置可用，且两参数的 lua_gc 能取到数值` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.后端适配层.Lua 侧的 64 位字面量能完整取回` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.后端适配层.UNLUANOVA_LOAD 能加载并执行 chunk` | `index.json` | 0.053s |
| `UnLuaNova.API.LuaBackend.后端适配层.两个环境互不影响` | `index.json` | 0.060s |
| `UnLuaNova.API.LuaBackend.整数类型.PUSHINT 推入的值能在脚本里参与算术` | `index.json` | 0.043s |
| `UnLuaNova.API.LuaBackend.整数类型.PUSHINT64 推入的值被认作整数且能完整读回` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.整数类型.UnLuaNovaIsIntegralNumber 区分整数与非整数` | `index.json` | 0.049s |
| `UnLuaNova.API.LuaBackend.整数类型.非整数读回为 0，而不是被截断` | `index.json` | 0.050s |
| `UnLuaNova.API.LuaBackend.环境销毁.销毁环境会跑完所有 __gc，且不破坏内存` | `index.json` | 0.059s |
| `UnLuaNova.API.LuaScriptCompat.collectgarbage.collect 返回 0，与 plain Lua 一致` | `index.json` | 0.051s |
| `UnLuaNova.API.LuaScriptCompat.collectgarbage.count 返回 KBytes 数字` | `index.json` | 0.058s |
| `UnLuaNova.API.LuaScriptCompat.collectgarbage.gc 之后 Lua 值仍然可用` | `index.json` | 0.064s |
| `UnLuaNova.API.LuaScriptCompat.collectgarbage.两个后端都存在` | `index.json` | 0.048s |
| `UnLuaNova.API.LuaScriptCompat.collectgarbage.未知选项报错而不是静默` | `index.json` | 0.054s |
| `UnLuaNova.API.LuaScriptCompat.load.两个后端都能从字符串加载 chunk` | `index.json` | 0.057s |
| `UnLuaNova.API.LuauIntegration.Buffer example preserves copy ownership` | `index.json` | 0.077s |
| `UnLuaNova.API.LuauIntegration.CRLF reads and iterators consume the full line ending` | `index.json` | 0.068s |
| `UnLuaNova.API.LuauIntegration.Invalid lengths and mutable paths cannot bypass validation` | `index.json` | 0.067s |
| `UnLuaNova.API.LuauIntegration.Native compilation survives coroutine and VM lifetimes` | `index.json` | 0.138s |
| `UnLuaNova.API.LuauIntegration.Native compiler command and enable variable coexist` | `index.json` | 0.056s |
| `UnLuaNova.API.Net.ClientServer Lua 声明的 RPC 在 C/S 会话里跨端执行` | `index.json` | 1.436s |
| `UnLuaNova.API.Net.DedicatedServer Two clients share a DS session and verify bidirectional RPCs and replication` | `index.json` | 38.408s |
| `UnLuaNova.API.Net.DedicatedServer.Report Connected peer verifies RPCs and replicated Lua fields` | `index.json` | 0.020s |
| `UnLuaNova.API.Net.Integer payloads.int64 fields round-trip across 32-bit, double and signed limits` | `index.json` | 0.050s |
| `UnLuaNova.API.Net.Integer payloads.invalid integers preserve the previous wire value` | `index.json` | 0.053s |
| `UnLuaNova.API.Net.RPC 声明.RPC registration is independent of table insertion order` | `index.json` | 0.051s |
| `UnLuaNova.API.Net.RPC 声明.RPCId 被分配，且重复声明时得到同一个值` | `index.json` | 0.061s |
| `UnLuaNova.API.Net.RPC 声明.Reliable 与参数表被采纳` | `index.json` | 0.048s |
| `UnLuaNova.API.Net.RPC 声明.RestoreOverrides 会把声明的 RPC 撤掉` | `index.json` | 0.049s |
| `UnLuaNova.API.Net.RPC 声明.invalid RPC parameters reject the whole function` | `index.json` | 0.059s |
| `UnLuaNova.API.Net.RPC 声明.三种 RPC 都注册上，并带上正确的 Net 标记` | `index.json` | 0.060s |
| `UnLuaNova.API.Net.RPC 声明.带 Net 标记的函数不会被当成普通覆写` | `index.json` | 0.055s |
| `UnLuaNova.API.Net.ReportReadiness` | `index.json` | 0.028s |
| `UnLuaNova.API.Net.分桶布局.Schema fingerprint includes replication conditions` | `index.json` | 0.052s |
| `UnLuaNova.API.Net.分桶布局.SchemaVersion 一致当且仅当布局一致` | `index.json` | 0.057s |
| `UnLuaNova.API.Net.分桶布局.字段按类型落进桶，同类型按声明顺序拿下标` | `index.json` | 0.050s |
| `UnLuaNova.API.Net.复制属性声明.GetLifetimeReplicatedProps 的返回值被读出来` | `index.json` | 0.059s |
| `UnLuaNova.API.Net.复制属性声明.empty first schema is valid and repeated declarations are idempotent` | `index.json` | 0.055s |
| `UnLuaNova.API.Net.复制属性声明.invalid schemas are rejected atomically` | `index.json` | 0.059s |
| `UnLuaNova.API.Net.复制属性声明.没有声明时返回空表而不是崩溃` | `index.json` | 0.061s |
| `UnLuaNova.API.Net.类型与条件解析.参数类型名解析，且大小写不敏感` | `index.json` | 0.054s |
| `UnLuaNova.API.Net.类型与条件解析.复制条件用引擎自己的常量` | `index.json` | 0.058s |
| `UnLuaNova.API.Observability.CPU 性能分析.开关可切换， detach 之后关闭` | `index.json` | 0.056s |
| `UnLuaNova.API.Observability.CPU 性能分析.开启后脚本仍能正常执行` | `index.json` | 0.054s |
| `UnLuaNova.API.Observability.Profiler records function source and definition line` | `index.json` | 0.041s |
| `UnLuaNova.API.Observability.内存统计.Reset preserves live allocations and clears interval counts` | `index.json` | 0.055s |
| `UnLuaNova.API.Observability.内存统计.关闭后不再记账` | `index.json` | 0.061s |
| `UnLuaNova.API.Observability.内存统计.开启后统计分配次数与字节数` | `index.json` | 0.056s |
| `UnLuaNova.API.Observability.调试服务.attach / detach` | `index.json` | 0.056s |
| `UnLuaNova.API.Observability.调试服务.单步能停下来并抓到调用栈` | `index.json` | 0.435s |
| `UnLuaNova.API.Observability.调试服务.断点登记` | `index.json` | 0.045s |
| `UnLuaNova.API.OutParam.Full 返回全部Out参数` | `index.json` | 0.028s |
| `UnLuaNova.API.OutParam.Partial 返回部分Out参数，剩余部分使用传入参数填充` | `index.json` | 0.021s |
| `UnLuaNova.API.Overrider.函数查找.未绑定的对象一律回答“不存在”而不是崩溃` | `index.json` | 0.044s |
| `UnLuaNova.API.Overrider.函数查找.能找到模块里的函数，找不到不存在的` | `index.json` | 0.045s |
| `UnLuaNova.API.Overrider.类型化调用.self 作为第一个参数传入` | `index.json` | 0.051s |
| `UnLuaNova.API.Overrider.类型化调用.void 调用不取返回值也不残留栈` | `index.json` | 0.048s |
| `UnLuaNova.API.Overrider.类型化调用.函数不存在时返回默认值` | `index.json` | 0.052s |
| `UnLuaNova.API.Overrider.类型化调用.栈在调用后是平衡的` | `index.json` | 0.054s |
| `UnLuaNova.API.Overrider.类型化调用.返回值按声明类型取回` | `index.json` | 0.048s |
| `UnLuaNova.API.Overrides.分发.Suspend 后走原实现，Resume 后又走 Lua` | `index.json` | 0.056s |
| `UnLuaNova.API.Overrides.分发.两个对象的覆写互不影响` | `index.json` | 0.059s |
| `UnLuaNova.API.Overrides.分发.覆写内部报错不会崩，之后对象仍可调用` | `index.json` | 0.056s |
| `UnLuaNova.API.Overrides.分发.覆写后调用走 Lua 实现` | `index.json` | 0.060s |
| `UnLuaNova.API.Overrides.可覆写性.BlueprintImplementableEvent 可覆写，BlueprintCallable 不可覆写` | `index.json` | 0.047s |
| `UnLuaNova.API.Overrides.可覆写性.GetOverridableFunctions 只列出可覆写的函数` | `index.json` | 0.051s |
| `UnLuaNova.API.Overrides.可覆写性.判定规则是两条分支，FUNC_Net 被第二条排除` | `index.json` | 0.049s |
| `UnLuaNova.API.Overrides.新增函数的路径（bAddNew）.outer 不是目标类时函数被挂到目标类上` | `index.json` | 0.047s |
| `UnLuaNova.API.Overrides.新增函数的路径（bAddNew）.同一个类的第二个对象不会二次覆写` | `index.json` | 0.049s |
| `UnLuaNova.API.Overrides.覆写与还原.RestoreOverrides 之后不再是覆写状态` | `index.json` | 0.055s |
| `UnLuaNova.API.Overrides.覆写与还原.绑定后 Overridable 被换成了 ULuaFunction` | `index.json` | 0.043s |
| `UnLuaNova.API.RuntimeModules.Buffer.HasBuffer 与 HasFeature('buffer') 一致` | `index.json` | 0.039s |
| `UnLuaNova.API.RuntimeModules.Buffer.可以 require 到模块表` | `index.json` | 0.042s |
| `UnLuaNova.API.RuntimeModules.Buffer.往返保留 0/255 边界值和空数组` | `index.json` | 0.047s |
| `UnLuaNova.API.RuntimeModules.Buffer.有 buffer 的后端下 TArray 往返保持内容` | `index.json` | 0.047s |
| `UnLuaNova.API.RuntimeModules.Event.HasFeature('event') 为真` | `index.json` | 0.047s |
| `UnLuaNova.API.RuntimeModules.Event.Once 只触发一次` | `index.json` | 0.049s |
| `UnLuaNova.API.RuntimeModules.Event.UnbindOwner 摘掉 owner 名下全部订阅` | `index.json` | 0.050s |
| `UnLuaNova.API.RuntimeModules.Event.一个监听者报错不影响其他监听者` | `index.json` | 0.049s |
| `UnLuaNova.API.RuntimeModules.Event.可以 require 到模块表` | `index.json` | 0.048s |
| `UnLuaNova.API.RuntimeModules.Event.广播中 Off 不会漏发也不会重发` | `index.json` | 0.041s |
| `UnLuaNova.API.RuntimeModules.Event.广播会把参数透传给每个监听者` | `index.json` | 0.045s |
| `UnLuaNova.API.RuntimeModules.OOP.Ctor 的参数会透传给每一层` | `index.json` | 0.045s |
| `UnLuaNova.API.RuntimeModules.OOP.HasFeature('oop') 为真` | `index.json` | 0.041s |
| `UnLuaNova.API.RuntimeModules.OOP.Is 沿继承链判断，且不会把实例当成类` | `index.json` | 0.040s |
| `UnLuaNova.API.RuntimeModules.OOP.Super 可以访问父类实现` | `index.json` | 0.046s |
| `UnLuaNova.API.RuntimeModules.OOP.构造链从基类到子类依次调用 Ctor` | `index.json` | 0.041s |
| `UnLuaNova.API.RuntimeModules.Timer.HasFeature('timer') 为真` | `index.json` | 0.039s |
| `UnLuaNova.API.RuntimeModules.Timer.require 得到内建模块` | `index.json` | 0.043s |
| `UnLuaNova.API.RuntimeModules.Vector.HasVector 与 HasFeature('vector') 一致` | `index.json` | 0.045s |
| `UnLuaNova.API.RuntimeModules.Vector.原生后端下可以往返 FVector` | `index.json` | 0.046s |
| `UnLuaNova.API.RuntimeModules.Vector.可以 require 到模块表` | `index.json` | 0.048s |
| `UnLuaNova.API.RuntimeModules.随插件发布.运行时模块都在插件的 Content/Script 下` | `index.json` | 0.051s |
| `UnLuaNova.API.StaticBinding.属性类型分类.常见 UProperty 映射到对应的 Kind` | `index.json` | 0.046s |
| `UnLuaNova.API.StaticBinding.注册表.取消注册后查不到` | `index.json` | 0.040s |
| `UnLuaNova.API.StaticBinding.注册表.注册后能按 USTRUCT 和名字查到同一个表` | `index.json` | 0.037s |
| `UnLuaNova.API.StaticBinding.解析.函数解析到真实的 UFunction 并填好 flag` | `index.json` | 0.040s |
| `UnLuaNova.API.StaticBinding.解析.属性解析到真实的 FProperty` | `index.json` | 0.034s |
| `UnLuaNova.API.StaticBinding.解析.表里写了不存在的成员时跳过而不是崩` | `index.json` | 0.039s |
| `UnLuaNova.API.StringEx.count.空的子串返回 0 而不是死循环` | `index.json` | 0.052s |
| `UnLuaNova.API.StringEx.count.统计不重叠出现次数` | `index.json` | 0.052s |
| `UnLuaNova.API.StringEx.join.用分隔符拼接并转字符串` | `index.json` | 0.058s |
| `UnLuaNova.API.StringEx.join.空表得到空串` | `index.json` | 0.042s |
| `UnLuaNova.API.StringEx.split.分隔符按字面量处理，不是模式` | `index.json` | 0.043s |
| `UnLuaNova.API.StringEx.split.没有分隔符时返回整串` | `index.json` | 0.037s |
| `UnLuaNova.API.StringEx.split.默认逗号分隔` | `index.json` | 0.040s |
| `UnLuaNova.API.StringEx.startswith / endswith.不匹配时为假` | `index.json` | 0.036s |
| `UnLuaNova.API.StringEx.startswith / endswith.前缀` | `index.json` | 0.038s |
| `UnLuaNova.API.StringEx.startswith / endswith.后缀` | `index.json` | 0.036s |
| `UnLuaNova.API.StringEx.startswith / endswith.空串恒为真` | `index.json` | 0.042s |
| `UnLuaNova.API.StringEx.strip.trim 是 strip 的别名` | `index.json` | 0.035s |
| `UnLuaNova.API.StringEx.strip.去掉首尾空白` | `index.json` | 0.043s |
| `UnLuaNova.API.StringEx.安装到 string 表.require 也能拿到模块表` | `index.json` | 0.041s |
| `UnLuaNova.API.StringEx.安装到 string 表.环境启动后即可直接用` | `index.json` | 0.036s |
| `UnLuaNova.API.TArray.Add.追加元素到数组末尾` | `index.json` | 0.041s |
| `UnLuaNova.API.TArray.AddUnique.只追加不重复的元素` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.Append.追加另外一个数组的所有元素到数组末尾` | `index.json` | 0.047s |
| `UnLuaNova.API.TArray.Clear.清除数组，移除所有元素` | `index.json` | 0.034s |
| `UnLuaNova.API.TArray.Contains.数组不包含指定元素返回false` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.Contains.数组包含指定元素返回true` | `index.json` | 0.037s |
| `UnLuaNova.API.TArray.Find.找到元素，返回索引位置` | `index.json` | 0.043s |
| `UnLuaNova.API.TArray.Find.没有找到元素，返回0` | `index.json` | 0.033s |
| `UnLuaNova.API.TArray.Get.使用[]获取` | `index.json` | 0.032s |
| `UnLuaNova.API.TArray.Get.拷贝指定索引位置的元素` | `index.json` | 0.034s |
| `UnLuaNova.API.TArray.GetData.获取数组分配的内存指针` | `index.json` | 0.038s |
| `UnLuaNova.API.TArray.GetRef.获取指定索引位置的元素引用` | `index.json` | 0.039s |
| `UnLuaNova.API.TArray.Insert.插入元素到指定位置` | `index.json` | 0.038s |
| `UnLuaNova.API.TArray.IsValidIndex.合法位置索引返回true` | `index.json` | 0.037s |
| `UnLuaNova.API.TArray.IsValidIndex.非法位置索引返回false` | `index.json` | 0.038s |
| `UnLuaNova.API.TArray.LastIndex.获取数组最后一个元素的索引位置` | `index.json` | 0.033s |
| `UnLuaNova.API.TArray.Length.Num` | `index.json` | 0.041s |
| `UnLuaNova.API.TArray.Length.获取数组长度` | `index.json` | 0.039s |
| `UnLuaNova.API.TArray.Remove.移除指定索引处的元素` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.RemoveItem.移除指定元素` | `index.json` | 0.037s |
| `UnLuaNova.API.TArray.Reserve.只能够对空数组进行预留空间操作` | `index.json` | 0.037s |
| `UnLuaNova.API.TArray.Reserve.预留N个数组元素的空间` | `index.json` | 0.037s |
| `UnLuaNova.API.TArray.Resize.扩容，使用默认元素填充` | `index.json` | 0.041s |
| `UnLuaNova.API.TArray.Resize.缩容，移除多余元素` | `index.json` | 0.036s |
| `UnLuaNova.API.TArray.Set.使用[]赋值` | `index.json` | 0.034s |
| `UnLuaNova.API.TArray.Set.设置指定索引位置的元素` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.Shuffle.随机打乱数组元素位置` | `index.json` | 0.033s |
| `UnLuaNova.API.TArray.Swap.交换两个索引位置的元素` | `index.json` | 0.039s |
| `UnLuaNova.API.TArray.ToTable.将数组内容转为LuaTable` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.pairs.按引用获取元素（原生）` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.pairs.按引用获取元素（蓝图）` | `index.json` | 0.036s |
| `UnLuaNova.API.TArray.pairs.迭代获取数组索引与元素` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.构造TArray.构造TArray<FString>` | `index.json` | 0.035s |
| `UnLuaNova.API.TArray.构造TArray.构造TArray<FVector>` | `index.json` | 0.041s |
| `UnLuaNova.API.TArray.构造TArray.构造TArray<UStruct>` | `index.json` | 0.048s |
| `UnLuaNova.API.TArray.构造TArray.构造TArray<bool>` | `index.json` | 0.037s |
| `UnLuaNova.API.TArray.构造TArray.构造TArray<int32>` | `index.json` | 0.041s |
| `UnLuaNova.API.TArray.迭代中修改.迭代中删除元素不崩，且删除真的生效` | `index.json` | 0.042s |
| `UnLuaNova.API.TArray.迭代中修改.迭代中追加元素不崩，且追加真的生效` | `index.json` | 0.039s |
| `UnLuaNova.API.TMap.Add.Key不存在，新增` | `index.json` | 0.045s |
| `UnLuaNova.API.TMap.Add.Key存在，覆盖` | `index.json` | 0.051s |
| `UnLuaNova.API.TMap.Clear.清除，移除所有键值对` | `index.json` | 0.053s |
| `UnLuaNova.API.TMap.Find.查找Key不存在，返回nil` | `index.json` | 0.053s |
| `UnLuaNova.API.TMap.Find.查找Key存在，返回Value的拷贝` | `index.json` | 0.050s |
| `UnLuaNova.API.TMap.FindRef.查找Key不存在，返回nil` | `index.json` | 0.050s |
| `UnLuaNova.API.TMap.FindRef.查找Key存在，返回Value的引用` | `index.json` | 0.043s |
| `UnLuaNova.API.TMap.Keys.获取所有Key的数组` | `index.json` | 0.043s |
| `UnLuaNova.API.TMap.Length.获取Map长度` | `index.json` | 0.045s |
| `UnLuaNova.API.TMap.Remove.Key不存在，返回false` | `index.json` | 0.043s |
| `UnLuaNova.API.TMap.Remove.Key存在，移除并返回true` | `index.json` | 0.041s |
| `UnLuaNova.API.TMap.ToTable.将Map内容转为LuaTable` | `index.json` | 0.047s |
| `UnLuaNova.API.TMap.Values.获取所有Value的数组` | `index.json` | 0.043s |
| `UnLuaNova.API.TMap.pairs.按引用获取元素（原生）` | `index.json` | 0.042s |
| `UnLuaNova.API.TMap.pairs.按引用获取元素（蓝图）` | `index.json` | 0.040s |
| `UnLuaNova.API.TMap.pairs.迭代获取Key与Value` | `index.json` | 0.042s |
| `UnLuaNova.API.TMap.构造TMap.构造TMap<FString,FString>` | `index.json` | 0.040s |
| `UnLuaNova.API.TMap.构造TMap.构造TMap<FVector,bool>` | `index.json` | 0.041s |
| `UnLuaNova.API.TMap.构造TMap.构造TMap<UStruct,UStruct>` | `index.json` | 0.044s |
| `UnLuaNova.API.TMap.构造TMap.构造TMap<int32,int32>` | `index.json` | 0.040s |
| `UnLuaNova.API.TMap.迭代中修改.迭代中删除当前键不崩，且删除真的生效` | `index.json` | 0.041s |
| `UnLuaNova.API.TMap.迭代中修改.迭代中新增键不崩，且新增真的生效` | `index.json` | 0.042s |
| `UnLuaNova.API.TSet.Add.不增加重复的元素` | `index.json` | 0.059s |
| `UnLuaNova.API.TSet.Add.元素不存在，新增` | `index.json` | 0.057s |
| `UnLuaNova.API.TSet.Clear.清除，移除所有元素` | `index.json` | 0.051s |
| `UnLuaNova.API.TSet.Contains.查找元素不存在，返回false` | `index.json` | 0.056s |
| `UnLuaNova.API.TSet.Contains.查找元素存在，返回true` | `index.json` | 0.049s |
| `UnLuaNova.API.TSet.Length.获取Set长度` | `index.json` | 0.050s |
| `UnLuaNova.API.TSet.Remove.元素不存在，返回false` | `index.json` | 0.055s |
| `UnLuaNova.API.TSet.Remove.元素存在，移除并返回true` | `index.json` | 0.063s |
| `UnLuaNova.API.TSet.ToArray.获取所有元素的数组` | `index.json` | 0.053s |
| `UnLuaNova.API.TSet.ToTable.将Set内容转为LuaTable` | `index.json` | 0.054s |
| `UnLuaNova.API.TSet.pairs.删除元素后迭代不会带回空洞` | `index.json` | 0.051s |
| `UnLuaNova.API.TSet.pairs.迭代中删除元素不崩，且删除真的生效` | `index.json` | 0.054s |
| `UnLuaNova.API.TSet.pairs.迭代中新增元素不崩，且新增真的生效` | `index.json` | 0.047s |
| `UnLuaNova.API.TSet.pairs.迭代拿到Set里的每个元素` | `index.json` | 0.057s |
| `UnLuaNova.API.TSet.构造TSet.构造TSet<FString>` | `index.json` | 0.047s |
| `UnLuaNova.API.TSet.构造TSet.构造TSet<FVector>` | `index.json` | 0.047s |
| `UnLuaNova.API.TSet.构造TSet.构造TSet<UScriptStruct>` | `index.json` | 0.044s |
| `UnLuaNova.API.TSet.构造TSet.构造TSet<int32>` | `index.json` | 0.046s |
| `UnLuaNova.API.Timer.cancelling another due callback prevents its invocation` | `index.json` | 0.061s |
| `UnLuaNova.API.Timer.closing Env rejects new callback registrations` | `index.json` | 0.060s |
| `UnLuaNova.API.Timer.collected caller coroutine cannot invalidate its callback` | `index.json` | 0.056s |
| `UnLuaNova.API.Timer.handles and ClearAll are isolated by environment` | `index.json` | 0.058s |
| `UnLuaNova.API.Timer.multiple worlds require explicit context` | `index.json` | 0.079s |
| `UnLuaNova.API.Timer.one-shot is removed before callback and nested tick does not reenter` | `index.json` | 0.069s |
| `UnLuaNova.API.Timer.pause preserves the remaining initial delay` | `index.json` | 0.060s |
| `UnLuaNova.API.Timer.rejects invalid timing and handles without allocating callbacks` | `index.json` | 0.066s |
| `UnLuaNova.API.Timer.world teardown invalidates handles while VM remains usable` | `index.json` | 0.067s |
| `UnLuaNova.API.UClass.GetDefaultObject.正确获取类默认对象` | `index.json` | 0.049s |
| `UnLuaNova.API.UClass.IsChildOf.正确判断子类` | `index.json` | 0.047s |
| `UnLuaNova.API.UClass.Load.正确加载蓝图类` | `index.json` | 0.049s |
| `UnLuaNova.API.UE.全局UE命名空间.访问反射枚举` | `index.json` | 0.046s |
| `UnLuaNova.API.UE.全局UE命名空间.访问反射类` | `index.json` | 0.047s |
| `UnLuaNova.API.UE.全局UE命名空间.访问导出类` | `index.json` | 0.039s |
| `UnLuaNova.API.UObject.GetClass.获取对象的类型` | `index.json` | 0.057s |
| `UnLuaNova.API.UObject.GetName.获取对象的名称` | `index.json` | 0.055s |
| `UnLuaNova.API.UObject.GetOuter.获取对象的外部对象` | `index.json` | 0.071s |
| `UnLuaNova.API.UObject.GetWorld.获取对象所在的World` | `index.json` | 0.085s |
| `UnLuaNova.API.UObject.IsA.判断对象是否为指定类型` | `index.json` | 0.068s |
| `UnLuaNova.API.UObject.IsValid.获取对象的有效状态` | `index.json` | 0.152s |
| `UnLuaNova.API.UObject.Load.加载一个对象` | `index.json` | 0.067s |
| `UnLuaNova.API.UObject.Release.释放对象在LuaVM的引用` | `index.json` | 0.058s |
| `UnLuaNova.API.World.SpawnActor.创建Actor，仅指定Actor类型` | `index.json` | 0.061s |
| `UnLuaNova.API.World.SpawnActor.创建Actor，指定初始Transform` | `index.json` | 0.060s |
| `UnLuaNova.API.World.SpawnActor.创建Actor，指定初始Transform/CollisionHanding/Owner` | `index.json` | 0.055s |
| `UnLuaNova.API.World.SpawnActor.创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator` | `index.json` | 0.058s |
| `UnLuaNova.API.World.SpawnActor.创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator/LuaModule` | `index.json` | 0.058s |
| `UnLuaNova.API.World.SpawnActor.创建Actor，指定初始Transform/CollisionHandling` | `index.json` | 0.058s |
| `UnLuaNova.API.World.SpawnActor.创建Actor，自定义构造参数` | `index.json` | 0.060s |
| `UnLuaNova.Binding.Enum Class.Get` | `index.json` | 0.042s |
| `UnLuaNova.Binding.Enum Class.Set` | `index.json` | 0.035s |
| `UnLuaNova.Binding.Scoped Enum.Get` | `index.json` | 0.039s |
| `UnLuaNova.Binding.Scoped Enum.Set` | `index.json` | 0.047s |
| `UnLuaNova.Editor.Commands.Lua 模板.脚本根目录指向工程的 Content/Script` | `index.json` | 0.035s |
| `UnLuaNova.Editor.Commands.Lua 模板.随插件发布且带 ClassName 占位符` | `index.json` | 0.035s |
| `UnLuaNova.Editor.Commands.控制台命令.lua.* 全部已注册` | `index.json` | 0.039s |
| `UnLuaNova.Editor.Commands.编辑器命令.编辑器模块可加载且命令上下文已注册` | `index.json` | 0.033s |
| `UnLuaNova.Editor.ConsoleCommands.lua.do.一段错误脚本不会让环境失效` | `index.json` | 0.044s |
| `UnLuaNova.Editor.ConsoleCommands.lua.do.执行的脚本真的生效` | `index.json` | 0.030s |
| `UnLuaNova.Editor.ConsoleCommands.lua.do.缺少参数时给用法提示而不是崩` | `index.json` | 0.033s |
| `UnLuaNova.Editor.ConsoleCommands.命令的可观测效果.lua.coroutines 可查且不影响脚本执行` | `index.json` | 0.033s |
| `UnLuaNova.Editor.ConsoleCommands.命令的可观测效果.lua.dbg 真的挂上调试器，断点登记后可查` | `index.json` | 0.036s |
| `UnLuaNova.Editor.ConsoleCommands.命令的可观测效果.lua.gc 真的回收了垃圾` | `index.json` | 0.039s |
| `UnLuaNova.Editor.ConsoleCommands.命令的可观测效果.lua.memstats on 之后开始记账，off 之后停` | `index.json` | 0.040s |
| `UnLuaNova.Editor.ConsoleCommands.命令的可观测效果.lua.profiler on/off 真的开关分析器` | `index.json` | 0.038s |
| `UnLuaNova.Editor.ConsoleCommands.开关类命令.lua.dbg 状态查询是幂等的` | `index.json` | 0.036s |
| `UnLuaNova.Editor.ConsoleCommands.开关类命令.lua.gc 执行后环境仍可用` | `index.json` | 0.031s |
| `UnLuaNova.Editor.ConsoleCommands.开关类命令.lua.memstats 各子命令都不崩` | `index.json` | 0.038s |
| `UnLuaNova.Editor.ConsoleCommands.开关类命令.lua.profiler 开关是可重复的` | `index.json` | 0.038s |
| `UnLuaNova.Editor.ConsoleCommands.沙盒.lua.sandbox 是一个可写的 CVar` | `index.json` | 0.036s |
| `UnLuaNova.Editor.IntelliSense.函数级辅助.GetTypeName 对属性给出非空类型名` | `index.json` | 0.036s |
| `UnLuaNova.Editor.IntelliSense.函数级辅助.IsValid 区分可导出与不可导出的函数` | `index.json` | 0.033s |
| `UnLuaNova.Editor.IntelliSense.类型声明生成.UCLASS 的声明包含它的名字` | `index.json` | 0.039s |
| `UnLuaNova.Editor.IntelliSense.类型声明生成.不同类得到不同的声明` | `index.json` | 0.035s |
| `UnLuaNova.Editor.IntelliSense.类型声明生成.声明包含真实的 UFUNCTION 与 UPROPERTY` | `index.json` | 0.032s |
| `UnLuaNova.Editor.StaticBindingGenerator.Generated thunk parameters use qualified enum values` | `index.json` | 0.103s |
| `UnLuaNova.Editor.StaticBindingGenerator.opt-out 的类型不会出现在输出里` | `index.json` | 0.096s |
| `UnLuaNova.Editor.StaticBindingGenerator.生成的是真实内容，而不只是创建了一个文件` | `index.json` | 0.105s |
| `UnLuaNova.Editor.StaticBindingGenerator.生成静态绑定表到 -OutputDir 指定的目录` | `index.json` | 0.112s |
| `UnLuaNova.Editor.StaticBindingGenerator.连续两次生成结果一致` | `index.json` | 0.161s |
| `UnLuaNova.Extensions.LuaProtobuf.protoc 加载内联 proto 后编解码一致` | `index.json` | 0.042s |
| `UnLuaNova.Extensions.LuaProtobuf.require 拿到可用的 pb 模块` | `index.json` | 0.043s |
| `UnLuaNova.Extensions.LuaRapidjson.decode 能解析出字段` | `index.json` | 0.040s |
| `UnLuaNova.Extensions.LuaRapidjson.encode/decode 往返一致` | `index.json` | 0.041s |
| `UnLuaNova.Extensions.LuaRapidjson.require 拿到带 decode/encode 的模块表` | `index.json` | 0.039s |
| `UnLuaNova.Extensions.LuaSocket.gettime 返回递增的数字，不依赖网络` | `index.json` | 0.066s |
| `UnLuaNova.Extensions.LuaSocket.require 拿到 socket 模块` | `index.json` | 0.056s |
| `UnLuaNova.Extensions.LuaSocket.socket.core 也能单独 require 到` | `index.json` | 0.050s |
| `UnLuaNova.Lua.Lua 侧用例全部通过` | `index.json` | 0.054s |
| `UnLuaNova.Memory.Accounting preserves live bytes across enable and reset` | `index.json` | 0.181s |
| `UnLuaNova.Memory.Closing a different environment preserves allocation callbacks` | `index.json` | 0.155s |
| `UnLuaNova.Memory.delegates` | `index.json` | 2.550s |
| `UnLuaNova.Memory.hot_reload` | `index.json` | 0.178s |
| `UnLuaNova.Memory.native_payloads` | `index.json` | 0.209s |
| `UnLuaNova.Memory.retained_reference` | `index.json` | 0.118s |
| `UnLuaNova.Memory.steady_heap` | `index.json` | 0.140s |
| `UnLuaNova.Memory.umg_resources` | `index.json` | 4.703s |
| `UnLuaNova.Memory.vm_cycles` | `index.json` | 0.241s |
| `UnLuaNova.Memory.world_env_first` | `index.json` | 1.614s |
| `UnLuaNova.Memory.world_world_first` | `index.json` | 1.552s |
| `UnLuaNova.Perf.Benchmark.把调用耗时写入 Saved/Benchmark` | `index.json` | 7.022s |
| `UnLuaNova.Perf.BenchmarkParity.把 slua 同名用例的耗时写入 Saved/Benchmark` | `index.json` | 1.632s |
| `UnLuaNova.Regression.Issue276 游戏世界暂停后，coroutine delay 功能不工作` | `index.json` | 1.942s |
| `UnLuaNova.Regression.Issue279 lua覆写蓝图的同名函数之后，无法将特定类型的函数参数保存到lua对象中` | `index.json` | 0.044s |
| `UnLuaNova.Regression.Issue284 require不存在的lib会崩` | `index.json` | 0.039s |
| `UnLuaNova.Regression.Issue286 蓝图 TMap FindRef 错误` | `index.json` | 0.560s |
| `UnLuaNova.Regression.Issue288 UMG里Image用到的Texture内存泄漏` | `index.json` | 0.197s |
| `UnLuaNova.Regression.Issue289 UClass在被UEGC后没有释放相应的绑定` | `index.json` | 0.211s |
| `UnLuaNova.Regression.Issue291 delegate绑定本身不在lua里有引用的object，在绑定后触发gc会导致无法调用绑定的函数` | `index.json` | 0.057s |
| `UnLuaNova.Regression.Issue292 actor调用K2_DestroyActor后，立刻调用IsValid会返回true` | `index.json` | 0.045s |
| `UnLuaNova.Regression.Issue293 关闭RPC会导致部分函数在非Editor模式下crash` | `index.json` | 0.041s |
| `UnLuaNova.Regression.Issue294 AutoCreateRefTerm标记的参数需要生成默认值` | `index.json` | 0.044s |
| `UnLuaNova.Regression.Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃` | `index.json` | 1.215s |
| `UnLuaNova.Regression.Issue300 LUA覆写的函数返回给蓝图的值错误` | `index.json` | 0.619s |
| `UnLuaNova.Regression.Issue303 部分情况下为TArray创建拷贝至lua时会产生崩溃` | `index.json` | 0.031s |
| `UnLuaNova.Regression.Issue304 Lua中监听delegate，接收TArray参数，使用Get获取其中元素导致崩溃` | `index.json` | 0.045s |
| `UnLuaNova.Regression.Issue314 访问过父类不存在的字段，后续在子类中也只能取到nil` | `index.json` | 0.068s |
| `UnLuaNova.Regression.Issue321 同名蓝图加载冲突` | `index.json` | 0.058s |
| `UnLuaNova.Regression.Issue322 绑定中文名字的蓝图会报错` | `index.json` | 0.060s |
| `UnLuaNova.Regression.Issue323 一些默认参数的写法没有分析出来` | `index.json` | 0.040s |
| `UnLuaNova.Regression.Issue328 C++调用LUA覆写带返回值的函数会崩溃` | `index.json` | 0.065s |
| `UnLuaNova.Regression.Issue331 UENUM的某一项为表达式时，默认值没有分析出来` | `index.json` | 0.046s |
| `UnLuaNova.Regression.Issue344 在Lua中对UClass_Load出来的对象写入属性后，GC的时候会Crash` | `index.json` | 0.055s |
| `UnLuaNova.Regression.Issue346 BlueprintFunctionLibrary无法静态绑定到lua文件` | `index.json` | 0.575s |
| `UnLuaNova.Regression.Issue366 使用ADD_SHARED_PTR_CONSTRUCTOR后启动直接崩溃` | `index.json` | 0.038s |
| `UnLuaNova.Regression.Issue367 类型元表被LuaGC时，可能意外导致正在使用的FClassDesc被错误delete` | `index.json` | 0.139s |
| `UnLuaNova.Regression.Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留` | `index.json` | 0.603s |
| `UnLuaNova.Regression.Issue375 子类被销毁时，可能会令父类的Lua绑定失效` | `index.json` | 0.122s |
| `UnLuaNova.Regression.Issue376 引用参数传递失败` | `index.json` | 0.050s |
| `UnLuaNova.Regression.Issue398 接口列表从lua传到c++/蓝图层会丢失一半信息` | `index.json` | 0.063s |
| `UnLuaNova.Regression.Issue400 不同实例的多播委托绑定相同实例函数，清理时崩溃` | `index.json` | 0.137s |
| `UnLuaNova.Regression.Issue401 LUA覆写导致数组传参错误` | `index.json` | 0.577s |
| `UnLuaNova.Regression.Issue407 无法在lua中将一个TMap调用Keys的结果传给C++` | `index.json` | 0.039s |
| `UnLuaNova.Regression.Issue430 动画通知调用组件Lua覆写函数崩溃` | `index.json` | 1.833s |
| `UnLuaNova.Regression.Issue432 Lua调用的函数返回蓝图结构体会check` | `index.json` | 0.040s |
| `UnLuaNova.Regression.Issue434 MulticastDelegate创建Handler时和self绑定` | `index.json` | 0.523s |
| `UnLuaNova.Regression.Issue443 在某些代码块内无法调用蓝图函数` | `index.json` | 1.546s |
| `UnLuaNova.Regression.Issue445 TSubclassOf作为函数返回值，从lua传回C++为空` | `index.json` | 0.061s |
| `UnLuaNova.Regression.Issue446 实现了FTickableGameObject的对象在Tick里调用自身被Lua覆写的方法会崩溃` | `index.json` | 1.191s |
| `UnLuaNova.Regression.Issue453 从Lua按引用传递到蓝图的TArray引用变成了空Array` | `index.json` | 0.661s |
| `UnLuaNova.Regression.Issue455 CacheScriptContainer导致的崩溃` | `index.json` | 2.851s |
| `UnLuaNova.Regression.Issue471 FScriptDelegate解绑无效` | `index.json` | 1.524s |
| `UnLuaNova.Regression.Issue473 FName类型参数中文乱码` | `index.json` | 0.039s |
| `UnLuaNova.Regression.Issue476 通过NewObject接口添加的UObject引用无法释放` | `index.json` | 0.126s |
| `UnLuaNova.Regression.Issue477 遍历蓝图结构体的DataTable会发生拷贝错误` | `index.json` | 0.350s |
| `UnLuaNova.Regression.Issue517 Actor的Struct成员变量在Lua里引用，释放后仍旧可以访问` | `index.json` | 0.115s |
| `UnLuaNova.Regression.Issue539 Lua覆写Out返回值时无法返回nil` | `index.json` | 0.044s |
| `UnLuaNova.Regression.Issue554 访问非TArray的结构体数组报错` | `index.json` | 0.038s |
| `UnLuaNova.Regression.Issue556 Lua覆写的函数接收数组参数时可能无效` | `index.json` | 0.039s |
| `UnLuaNova.Regression.Issue561 访问UStruct内部的委托会check` | `index.json` | 0.041s |
| `UnLuaNova.Regression.Issue562 容器元素为蓝图类型时，在蓝图Class被提前UE回收后，会导致容器在luagc时报错` | `index.json` | 0.129s |
| `UnLuaNova.Regression.Issue566 多次传递委托类型的参数到同一函数时，可能因为Owner失效而无法回调` | `index.json` | 0.090s |
| `UnLuaNova.Regression.Issue583 在Lua中遍历TMap字段的Values接口返回值时引起的崩溃` | `index.json` | 0.037s |
| `UnLuaNova.Regression.Issue585 蓝图枚举无法被UnLuaRef` | `index.json` | 0.146s |
| `UnLuaNova.Regression.Issue595 Lua无法调用Interface中函数` | `index.json` | 0.038s |
| `UnLuaNova.Regression.Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | `index.json` | 1.966s |
| `UnLuaNova.Regression.Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃` | `index.json` | 1.472s |
| `UnLuaNova.Regression.Issue660 Unlua函数绑定多个代理导致崩溃` | `index.json` | 1.967s |
| `UnLuaNova.Regression.Issue668 在Lua中调用TArray的Add接口时内存对齐引起的问题` | `index.json` | 0.028s |
| `UnLuaNova.Regression.IssueOverrides 覆写引起的问题` | `index.json` | 1.003s |
| `UnLuaNova.Regression.StructContainerLifetime` | `index.json` | 0.119s |
| `UnLuaNova.Settings.DeadLoopCheck.设置防止无限循环的超时时间` | `index.json` | 1.617s |
| `UnLuaNova.Settings.General.启动模块名称.为空时 Start 什么都不跑` | `index.json` | 0.047s |
| `UnLuaNova.Settings.General.启动模块名称.设置后 Start 会执行那个模块` | `index.json` | 0.049s |
| `UnLuaNova.Settings.General.定位器类.两个定位器类都能切换并还原` | `index.json` | 0.040s |
| `UnLuaNova.Settings.General.诊断开关.DanglingCheck 可设置且能还原` | `index.json` | 0.041s |
| `UnLuaNova.Settings.General.诊断开关.DeadLoopCheck 可设置且能还原` | `index.json` | 0.043s |
| `UnLuaNova.Showcase.Binding 静态绑定示例截图` | `index.json` | 3.475s |
| `UnLuaNova.Showcase.Container 容器访问示例截图` | `index.json` | 3.036s |
| `UnLuaNova.Showcase.Override 蓝图事件覆写示例截图` | `index.json` | 2.995s |
| `UnLuaNova.Showcase.UMG UMG 与 Lua 生命周期示例截图` | `index.json` | 3.489s |
| `UnLuaNova.Slow.Net.Multicast 服务器一次 Lua 声明的 multicast 调用到达两个客户端进程` | `index.json` | 26.418s |
| `UnLuaNova.Slow.Net.Multicast.Report 在 -UnLuaNovaNetRole 指定的联网进程里记录本端收到什么` | `index.json` | 0.013s |
## 最慢的用例

| 用例 | 耗时 |
|---|---|
| `UnLuaNova.API.Net.DedicatedServer Two clients share a DS session and verify bidirectional RPCs and replication` | 38.408s |
| `UnLuaNova.Slow.Net.Multicast 服务器一次 Lua 声明的 multicast 调用到达两个客户端进程` | 26.418s |
| `UnLuaNova.Perf.Benchmark.把调用耗时写入 Saved/Benchmark` | 7.022s |
| `UnLuaNova.Memory.umg_resources` | 4.703s |
| `UnLuaNova.Showcase.UMG UMG 与 Lua 生命周期示例截图` | 3.489s |
| `UnLuaNova.Showcase.Binding 静态绑定示例截图` | 3.475s |
| `UnLuaNova.Showcase.Container 容器访问示例截图` | 3.036s |
| `UnLuaNova.Showcase.Override 蓝图事件覆写示例截图` | 2.995s |
| `UnLuaNova.Regression.Issue455 CacheScriptContainer导致的崩溃` | 2.851s |
| `UnLuaNova.Memory.delegates` | 2.550s |

## 联网会话

这一轮跑过的跨进程拓扑。用例用 `FNetSession` 声明要几个 peer、各自什么角色，每个 peer 把自己的报告与日志写进 `Saved/UnLuaNovaNet/<会话>/`，`Tools/RunTests.ps1` 再归档到本轮目录的 `net/` 下。

headless 的独立服务器使用 `-NullRHI`，没有窗口；它的日志是它唯一的痕迹，所以每个 peer 都列了。

### DedicatedServer

| peer | 角色 | 窗口 | 报告 | 日志 |
|:--|:--|:--|:--|:--|
| Server | Server | headless | `net/DedicatedServer/Server.json` | `net/DedicatedServer/logs/Server.log` |
| Client1 | Client | 有视口 | `net/DedicatedServer/Client1.json` | `net/DedicatedServer/logs/Client1.log` |
| Client2 | Client | 有视口 | `net/DedicatedServer/Client2.json` | `net/DedicatedServer/logs/Client2.log` |

| peer | PID | NetMode | Connected | SessionId | Complete | 第一轮观测等待 ms |
|---|---:|---:|---|---|---|---:|
| Server | 54400 | 1 | True | F732231E49B1D018DE888AAE16B0E827 | True | 0 |
| Client1 | 31932 | 3 | True | F732231E49B1D018DE888AAE16B0E827 | True | 5008.152101188898 |
| Client2 | 40296 | 3 | True | F732231E49B1D018DE888AAE16B0E827 | True | 120.52150070667267 |

逐端原始观测：HP 从 Lua 表读取；调用日志是本端非复制字段。完整判定以严格证据门禁为准。

| peer | 轮次 | Actor | Owner | Challenge | Lua HP | 本端 RPC 日志 |
|---|---:|---:|---|---:|---:|---|
| Server | 1 | 1 | False | 855819 | 855820 | FromClient:855820&#124;ToAll:855822&#124; |
| Server | 1 | 2 | False | 855919 | 855920 | FromClient:855920&#124;ToAll:855922&#124; |
| Server | 2 | 1 | False | 855819 | 855830 | FromClient:855820&#124;ToAll:855822&#124;FromClient:855820&#124;FromClient:855830&#124;ToAll:855832&#124; |
| Server | 2 | 2 | False | 855919 | 855930 | FromClient:855920&#124;ToAll:855922&#124;FromClient:855920&#124;FromClient:855930&#124;ToAll:855932&#124; |
| Server | final | 1 | False | 855819 | 855830 | FromClient:855820&#124;ToAll:855822&#124;FromClient:855820&#124;FromClient:855830&#124;ToAll:855832&#124;FromClient:855830&#124; |
| Server | final | 2 | False | 855919 | 855930 | FromClient:855920&#124;ToAll:855922&#124;FromClient:855920&#124;FromClient:855930&#124;ToAll:855932&#124;FromClient:855930&#124; |
| Client1 | 1 | 1 | True | 855819 | 855820 | FromServer:855821&#124;ToAll:855822&#124; |
| Client1 | 1 | 2 | False | 855919 | 855920 | ToAll:855922&#124; |
| Client1 | 2 | 1 | True | 855819 | 855830 | FromServer:855821&#124;ToAll:855822&#124;FromServer:855823&#124;FromServer:855831&#124;ToAll:855832&#124; |
| Client1 | 2 | 2 | False | 855919 | 855930 | ToAll:855922&#124;ToAll:855932&#124; |
| Client1 | final | 1 | True | 855819 | 855830 | FromServer:855821&#124;ToAll:855822&#124;FromServer:855823&#124;FromServer:855831&#124;ToAll:855832&#124; |
| Client1 | final | 2 | False | 855919 | 855930 | ToAll:855922&#124;ToAll:855932&#124; |
| Client2 | 1 | 1 | False | 855819 | 855820 | ToAll:855822&#124; |
| Client2 | 1 | 2 | True | 855919 | 855920 | FromServer:855921&#124;ToAll:855922&#124; |
| Client2 | 2 | 1 | False | 855819 | 855830 | ToAll:855822&#124;ToAll:855832&#124; |
| Client2 | 2 | 2 | True | 855919 | 855930 | FromServer:855921&#124;ToAll:855922&#124;FromServer:855923&#124;FromServer:855931&#124;ToAll:855932&#124; |
| Client2 | final | 1 | False | 855819 | 855830 | ToAll:855822&#124;ToAll:855832&#124; |
| Client2 | final | 2 | True | 855919 | 855930 | FromServer:855921&#124;ToAll:855922&#124;FromServer:855923&#124;FromServer:855931&#124;ToAll:855932&#124; |

### Multicast

| peer | 角色 | 窗口 | 报告 | 日志 |
|:--|:--|:--|:--|:--|
| Server | Server | headless | `net/Multicast/Server.json` | `net/Multicast/logs/Server.log` |
| Client1 | Client | 有视口 | `net/Multicast/Client1.json` | `net/Multicast/logs/Client1.log` |
| Client2 | Client | 有视口 | `net/Multicast/Client2.json` | `net/Multicast/logs/Client2.log` |

| peer | PID | NetMode | Connected | SessionId | Complete | 第一轮观测等待 ms |
|---|---:|---:|---|---|---|---:|
| Server | 6316 | 1 | True | missing | missing | missing |
| Client1 | 44432 | 3 | True | missing | missing | missing |
| Client2 | 82128 | 3 | True | missing | missing | missing |

> 日志不随页面发布：它们在本轮输出目录的 `net/` 下（本机跑完就能打开）。

## 截图

### Showcase_Binding

![Showcase_Binding](Images/tests-luau/Showcase_Binding.png)

### Showcase_Container

![Showcase_Container](Images/tests-luau/Showcase_Container.png)

### Showcase_Override

![Showcase_Override](Images/tests-luau/Showcase_Override.png)

### Showcase_UMG

![Showcase_UMG](Images/tests-luau/Showcase_UMG.png)

## 按运行环境分类

每个用例需要什么环境（由 `Tools/ClassifyTests.py` 静态分析源码得出），以及这一类一共花了多少时间。看这一节是为了回答"把某一类挪走能省多少"——A 类连 `UWorld` 都不需要，B 类只要 world，两者都**不必**跑在编辑器进程里，而现在它们和真正需要编辑器的 D / E 一起挤在同一个 `UnrealEditor-Cmd` 里。

| 分类 | 需要什么 | 用例数 | 耗时 | 占比 |
|:--|:--|--:|--:|--:|
| C | PIE 且联网 | 6 | 66.3s | 43% |
| A | 纯逻辑（不需要 UWorld） | 531 | 40.0s | 26% |
| B | 需要 UWorld，不联网 | 56 | 35.1s | 23% |
| E | 截图 / 连拍 | 4 | 13.0s | 8% |
| D | 需要真实视口 / RHI | 1 | 0.5s | 0% |
| ? | 未分类 | 43 | 0.0s | 0% |

分类是**声明**出来的，不是实际触碰的：条件性开图的用例会被算进 B 类。宁可高估——它决定的是"哪些可以挪"，挪错了会红，不会静默通过。

## 静态类型检查（luau-lsp）

| 指标 | 数值 |
|---|---|
| 检查文件数 | 7 |
| 模式 | `strict` |
| 错误 | 0 |
| 警告 | 7 |

**没有类型错误 ✅**（声明文件由编辑器「导出智能提示」生成。）

## 内存与资源回收

| 用例 | 通过 | 已关闭 VM | 剩余分配字节 | 剩余分配块 | 保留资源 | 活跃堆范围（字节） |
|---|---|---|---|---|---|---|
| delegates | True | 24 | 0 | 0 | 0 | 0 - 0 |
| hot_reload | True | 1 | 0 | 0 | 0 | 158265 - 158889 |
| native_payloads | True | 24 | 0 | 0 | 0 | 0 - 0 |
| retained_reference | True | 1 | 0 | 0 | 0 | 840887 - 1560919 |
| steady_heap | True | 1 | 0 | 0 | 0 | 156172 - 156172 |
| umg_resources | True | 24 | 0 | 0 | 0 | 0 - 0 |
| vm_cycles | True | 24 | 0 | 0 | 0 | 0 - 0 |
| world_env_first | True | 12 | 0 | 0 | 0 | 0 - 0 |
| world_world_first | True | 12 | 0 | 0 | 0 | 0 - 0 |

稳态测试以 VM 活跃堆计数评估增长；物理分配还包含 Luau 页容量。VM 关闭后要求分配与资源残留均为零。
完整计数、采样和运行标识保存在 `metadata/memory/`；Release 另行核验工作量与所属运行。

## 性能基线对比

| 用例 | 本次（µs/次） | 基线（µs/次） | 变化 |
|---|---|---|---|
| Lua | 1.5590 | 1.5429 | +1.0% |
| Lua.Container.PointsAreCoplanar | 11.0599 | 10.8680 | +1.8% |
| Native | 0.0012 | 0.0017 | -26.2% |
| Reflection | 0.4681 | 0.4590 | +2.0% |
| VM.Arithmetic | 0.0065 | 0.0059 | +9.5% |
| VM.Call | 0.0233 | 0.0203 | +15.0% |
| VM.TableGrow | 0.0142 | 0.0154 | -8.0% |
| VM.TableRead | 0.0082 | 0.0076 | +7.2% |

> `VM.*` 四项不跨越语言边界，测的是虚拟机自身；其余三项测的是脚本进入 C++ 的那一跳，
> 开销在参数编组与函数解析上，两个后端因此落在同一量级。
> `VM.TableGrow` 是空表逐个增长（rehash），`VM.TableRead` 是同一张表的纯数组读取 ——
> 拆开是为了区分「建表贵」和「读表贵」，这两件事对脚本作者完全不是一回事。

> 本次数据来自报告目录的 `metadata/perf.json`，与同后端性能基线比较。超过 ±30% 会标 ⚠️。Release 独立检查同机器、引擎、用例和迭代数，并以耗时增长 25% 为失败门槛。

---

原始 JSON 留在本机对应的 `Saved/TestReports/`（Development）或 `Saved/ReleaseTests/`（Release）运行目录（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Profile Development -Backend Luau -Publish`（要两份就 `-Backend Both`）。


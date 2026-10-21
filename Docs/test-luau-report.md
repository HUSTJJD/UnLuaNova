---
title: "测试报告（Luau 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Luau 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-21 12:06:41。请勿手工编辑。

## 结论

**存在失败 ❌（36 项）** — 通过率 89.9%

| 指标 | 数值 |
|---|---|
| 用例总数 | 357 |
| 通过 | 321 |
| 失败 | 36 |
| 跳过 | 0 |
| 总耗时 | 38.89s |
| 截图 | 0 |
| Lua 后端 | `Luau` |

## 通过明细（321 项）

| 用例 | 来源 | 耗时 |
|---|---|---|
| `64 位整数读写不截断` | `index.json` | 0.025s |
| `F1` | `index.json` | 0.037s |
| `Full 返回全部Out参数` | `index.json` | 0.022s |
| `GC 设置可用，且两参数的 lua_gc 能取到数值` | `index.json` | 0.025s |
| `HasFeature 与编译期能力一致` | `index.json` | 0.027s |
| `HotReload 能加载模块，而不是静默降级` | `index.json` | 0.026s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | `index.json` | 1.473s |
| `Issue279 lua覆写蓝图的同名函数之后，无法将特定类型的函数参数保存到lua对象中` | `index.json` | 0.026s |
| `Issue284 require不存在的lib会崩` | `index.json` | 0.034s |
| `Issue289 UClass在被UEGC后没有释放相应的绑定` | `index.json` | 0.144s |
| `Issue292 actor调用K2_DestroyActor后，立刻调用IsValid会返回true` | `index.json` | 0.033s |
| `Issue293 关闭RPC会导致部分函数在非Editor模式下crash` | `index.json` | 0.030s |
| `Issue294 AutoCreateRefTerm标记的参数需要生成默认值` | `index.json` | 0.030s |
| `Issue303 部分情况下为TArray创建拷贝至lua时会产生崩溃` | `index.json` | 0.014s |
| `Issue304 Lua中监听delegate，接收TArray参数，使用Get获取其中元素导致崩溃` | `index.json` | 0.036s |
| `Issue321 同名蓝图加载冲突` | `index.json` | 0.054s |
| `Issue322 绑定中文名字的蓝图会报错` | `index.json` | 0.045s |
| `Issue323 一些默认参数的写法没有分析出来` | `index.json` | 0.031s |
| `Issue328 C++调用LUA覆写带返回值的函数会崩溃` | `index.json` | 0.047s |
| `Issue331 UENUM的某一项为表达式时，默认值没有分析出来` | `index.json` | 0.036s |
| `Issue344 在Lua中对UClass_Load出来的对象写入属性后，GC的时候会Crash` | `index.json` | 0.041s |
| `Issue346 BlueprintFunctionLibrary无法静态绑定到lua文件` | `index.json` | 0.248s |
| `Issue366 使用ADD_SHARED_PTR_CONSTRUCTOR后启动直接崩溃` | `index.json` | 0.038s |
| `Issue367 类型元表被LuaGC时，可能意外导致正在使用的FClassDesc被错误delete` | `index.json` | 0.150s |
| `Issue375 子类被销毁时，可能会令父类的Lua绑定失效` | `index.json` | 0.134s |
| `Issue376 引用参数传递失败` | `index.json` | 0.125s |
| `Issue400 不同实例的多播委托绑定相同实例函数，清理时崩溃` | `index.json` | 0.167s |
| `Issue407 无法在lua中将一个TMap调用Keys的结果传给C++` | `index.json` | 0.017s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | `index.json` | 2.007s |
| `Issue432 Lua调用的函数返回蓝图结构体会check` | `index.json` | 0.025s |
| `Issue445 TSubclassOf作为函数返回值，从lua传回C++为空` | `index.json` | 0.041s |
| `Issue446 实现了FTickableGameObject的对象在Tick里调用自身被Lua覆写的方法会崩溃` | `index.json` | 1.105s |
| `Issue455 CacheScriptContainer导致的崩溃` | `index.json` | 2.713s |
| `Issue471 FScriptDelegate解绑无效` | `index.json` | 1.571s |
| `Issue473 FName类型参数中文乱码` | `index.json` | 0.016s |
| `Issue476 通过NewObject接口添加的UObject引用无法释放` | `index.json` | 0.089s |
| `Issue477 遍历蓝图结构体的DataTable会发生拷贝错误` | `index.json` | 0.229s |
| `Issue517 Actor的Struct成员变量在Lua里引用，释放后仍旧可以访问` | `index.json` | 0.078s |
| `Issue539 Lua覆写Out返回值时无法返回nil` | `index.json` | 0.032s |
| `Issue554 访问非TArray的结构体数组报错` | `index.json` | 0.028s |
| `Issue556 Lua覆写的函数接收数组参数时可能无效` | `index.json` | 0.029s |
| `Issue561 访问UStruct内部的委托会check` | `index.json` | 0.027s |
| `Issue562 容器元素为蓝图类型时，在蓝图Class被提前UE回收后，会导致容器在luagc时报错` | `index.json` | 0.089s |
| `Issue566 多次传递委托类型的参数到同一函数时，可能因为Owner失效而无法回调` | `index.json` | 0.080s |
| `Issue595 Lua无法调用Interface中函数` | `index.json` | 0.028s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | `index.json` | 1.505s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | `index.json` | 1.542s |
| `Issue668 在Lua中调用TArray的Add接口时内存对齐引起的问题` | `index.json` | 0.013s |
| `IssueOverrides 覆写引起的问题` | `index.json` | 1.082s |
| `Key不存在，新增` | `index.json` | 0.030s |
| `Key不存在，返回false` | `index.json` | 0.028s |
| `Key存在，移除并返回true` | `index.json` | 0.029s |
| `Key存在，覆盖` | `index.json` | 0.031s |
| `Lua 侧的 64 位字面量能完整取回` | `index.json` | 0.026s |
| `Luau 语法能编译并按预期求值` | `index.json` | 0.028s |
| `Num` | `index.json` | 0.031s |
| `Partial 返回部分Out参数，剩余部分使用传入参数填充` | `index.json` | 0.023s |
| `UNLUANOVA_LOAD 能加载并执行 chunk` | `index.json` | 0.029s |
| `Vector 能把原生 vector 转成 FVector` | `index.json` | 0.028s |
| `buffer 能按类型读写` | `index.json` | 0.029s |
| `core 也能单独 require 到` | `index.json` | 0.070s |
| `debug 提供 upvalue / local / registry 访问` | `index.json` | 0.028s |
| `decode 能解析出字段` | `index.json` | 0.025s |
| `encode/decode 往返一致` | `index.json` | 0.028s |
| `getupvalue 能读到闭包捕获的值` | `index.json` | 0.026s |
| `luaL_newmetatable 建的元表带 __name` | `index.json` | 0.025s |
| `package 同时提供 loaded 和 searchers` | `index.json` | 0.028s |
| `protoc 加载内联 proto 后编解码一致` | `index.json` | 0.027s |
| `require 拿到可用的 pb 模块` | `index.json` | 0.024s |
| `require 拿到带 decode/encode 的模块表` | `index.json` | 0.273s |
| `setupvalue 能给 chunk 设环境` | `index.json` | 0.028s |
| `不增加重复的元素` | `index.json` | 0.038s |
| `不存在的枚举值返回nil` | `index.json` | 0.037s |
| `不存在的枚举值返回nil` | `index.json` | 0.035s |
| `不相等` | `index.json` | 0.034s |
| `两个环境互不影响` | `index.json` | 0.051s |
| `两点相乘` | `index.json` | 0.037s |
| `两点相减` | `index.json` | 0.035s |
| `两点相加` | `index.json` | 0.037s |
| `两点相除` | `index.json` | 0.035s |
| `交换两个索引位置的元素` | `index.json` | 0.052s |
| `使用[]获取` | `index.json` | 0.036s |
| `使用[]赋值` | `index.json` | 0.087s |
| `元素不存在，新增` | `index.json` | 0.039s |
| `元素不存在，返回false` | `index.json` | 0.040s |
| `元素存在，移除并返回true` | `index.json` | 0.039s |
| `分别指定Pitch/Yaw/Roll` | `index.json` | 0.050s |
| `分别指定X/Y` | `index.json` | 0.024s |
| `分别指定X/Y/Z` | `index.json` | 0.038s |
| `分别指定X/Y/Z` | `index.json` | 0.040s |
| `分别指定X/Y/Z/W` | `index.json` | 0.026s |
| `分别指定X和Y` | `index.json` | 0.037s |
| `分配回调能收到分配事件` | `index.json` | 0.027s |
| `创建Actor，仅指定Actor类型` | `index.json` | 0.041s |
| `创建Actor，指定初始Transform` | `index.json` | 0.043s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner` | `index.json` | 0.047s |
| `创建Actor，指定初始Transform/CollisionHandling` | `index.json` | 0.040s |
| `判断对象是否为指定类型` | `index.json` | 0.048s |
| `加载一个对象` | `index.json` | 0.040s |
| `原生 vector 能进出 FVector` | `index.json` | 0.027s |
| `取反向量` | `index.json` | 0.038s |
| `取反向量` | `index.json` | 0.043s |
| `取反向量` | `index.json` | 0.027s |
| `取反向量` | `index.json` | 0.031s |
| `取相反数` | `index.json` | 0.036s |
| `只能够对空数组进行预留空间操作` | `index.json` | 0.136s |
| `只追加不重复的元素` | `index.json` | 0.028s |
| `合法位置索引返回true` | `index.json` | 0.026s |
| `同时指定Pitch/Yaw/Roll` | `index.json` | 0.046s |
| `同时指定X/Y` | `index.json` | 0.027s |
| `同时指定X/Y/Z` | `index.json` | 0.036s |
| `同时指定X/Y/Z` | `index.json` | 0.245s |
| `同时指定X和Y` | `index.json` | 0.038s |
| `向量乘` | `index.json` | 0.028s |
| `向量乘以数字` | `index.json` | 0.226s |
| `向量乘以整数` | `index.json` | 0.035s |
| `向量乘以浮点数` | `index.json` | 0.069s |
| `向量相减` | `index.json` | 0.037s |
| `向量相减` | `index.json` | 0.039s |
| `向量相减` | `index.json` | 0.030s |
| `向量相减` | `index.json` | 0.026s |
| `向量相加` | `index.json` | 0.038s |
| `向量相加` | `index.json` | 0.040s |
| `向量相加` | `index.json` | 0.292s |
| `向量相加` | `index.json` | 0.030s |
| `向量除` | `index.json` | 0.030s |
| `向量除以数字` | `index.json` | 0.251s |
| `向量除以整数` | `index.json` | 0.037s |
| `向量除以浮点数` | `index.json` | 0.080s |
| `四元数乘以浮点数` | `index.json` | 0.032s |
| `大于等于容差，归一化` | `index.json` | 0.029s |
| `大于等于容差，归一化` | `index.json` | 0.048s |
| `大于等于容差，归一化并返回true` | `index.json` | 0.086s |
| `将Map内容转为LuaTable` | `index.json` | 0.030s |
| `将Set内容转为LuaTable` | `index.json` | 0.040s |
| `将当前Transform与目标Transform混合` | `index.json` | 0.038s |
| `将当前Transform与目标两个Transform混合` | `index.json` | 0.042s |
| `将数组内容转为LuaTable` | `index.json` | 0.030s |
| `小于容差，不归一化` | `index.json` | 0.029s |
| `小于容差，不归一化` | `index.json` | 0.030s |
| `小于容差，直接返回false` | `index.json` | 0.076s |
| `广播事件` | `index.json` | 0.042s |
| `执行委托` | `index.json` | 0.029s |
| `执行委托：从Lua返回值` | `index.json` | 0.028s |
| `执行委托：赋值` | `index.json` | 0.032s |
| `执行成功返回true` | `index.json` | 0.037s |
| `扩容，使用默认元素填充` | `index.json` | 0.051s |
| `找到元素，返回索引位置` | `index.json` | 0.025s |
| `拷贝指定索引位置的元素` | `index.json` | 0.035s |
| `指定ColorValue` | `index.json` | 0.037s |
| `指定RGB` | `index.json` | 0.040s |
| `指定RGB` | `index.json` | 0.039s |
| `指定RGBA` | `index.json` | 0.037s |
| `指定RGBA` | `index.json` | 0.040s |
| `指定Rotation(FQuat)` | `index.json` | 0.040s |
| `指定Rotation(FQuat)/Translation` | `index.json` | 0.045s |
| `指定Rotation(FQuat)/Translation/Scale` | `index.json` | 0.042s |
| `指定X` | `index.json` | 0.026s |
| `指定X/Y` | `index.json` | 0.028s |
| `指定X/Y/Z` | `index.json` | 0.024s |
| `指定X/Y/Z/W` | `index.json` | 0.027s |
| `指定轴与角度` | `index.json` | 0.026s |
| `按引用获取元素（原生）` | `index.json` | 0.032s |
| `按引用获取元素（原生）` | `index.json` | 0.027s |
| `按引用获取元素（蓝图）` | `index.json` | 0.035s |
| `按引用获取元素（蓝图）` | `index.json` | 0.026s |
| `插入元素到指定位置` | `index.json` | 0.032s |
| `支持启动脚本和参数` | `index.json` | 0.034s |
| `支持多个Lua环境` | `index.json` | 0.040s |
| `支持多参数传入和传出` | `index.json` | 0.033s |
| `数组不包含指定元素返回false` | `index.json` | 0.027s |
| `数组包含指定元素返回true` | `index.json` | 0.028s |
| `构造TArray<FString>` | `index.json` | 0.032s |
| `构造TArray<FVector>` | `index.json` | 0.030s |
| `构造TArray<UStruct>` | `index.json` | 0.030s |
| `构造TArray<bool>` | `index.json` | 0.044s |
| `构造TArray<int32>` | `index.json` | 0.037s |
| `构造TMap<FString,FString>` | `index.json` | 0.030s |
| `构造TMap<FVector,bool>` | `index.json` | 0.029s |
| `构造TMap<UStruct,UStruct>` | `index.json` | 0.028s |
| `构造TMap<int32,int32>` | `index.json` | 0.028s |
| `构造TSet<FString>` | `index.json` | 0.039s |
| `构造TSet<FVector>` | `index.json` | 0.041s |
| `构造TSet<UScriptStruct>` | `index.json` | 0.039s |
| `构造TSet<int32>` | `index.json` | 0.037s |
| `查找Key不存在，返回nil` | `index.json` | 0.026s |
| `查找Key不存在，返回nil` | `index.json` | 0.027s |
| `查找Key存在，返回Value的引用` | `index.json` | 0.026s |
| `查找Key存在，返回Value的拷贝` | `index.json` | 0.027s |
| `查找元素不存在，返回false` | `index.json` | 0.042s |
| `查找元素存在，返回true` | `index.json` | 0.044s |
| `正确传入FName到Lua堆栈` | `index.json` | 0.037s |
| `正确传入FString到Lua堆栈` | `index.json` | 0.036s |
| `正确传入UObject到Lua堆栈` | `index.json` | 0.036s |
| `正确传入bool到Lua堆栈` | `index.json` | 0.037s |
| `正确传入const char*到Lua堆栈` | `index.json` | 0.039s |
| `正确传入double到Lua堆栈` | `index.json` | 0.036s |
| `正确传入float到Lua堆栈` | `index.json` | 0.036s |
| `正确传入int16到Lua堆栈` | `index.json` | 0.037s |
| `正确传入int32到Lua堆栈` | `index.json` | 0.036s |
| `正确传入int64到Lua堆栈` | `index.json` | 0.036s |
| `正确传入int8到Lua堆栈` | `index.json` | 0.036s |
| `正确传入struct到Lua堆栈` | `index.json` | 0.036s |
| `正确传入uint16到Lua堆栈` | `index.json` | 0.035s |
| `正确传入uint32到Lua堆栈` | `index.json` | 0.040s |
| `正确传入uint64到Lua堆栈` | `index.json` | 0.035s |
| `正确传入uint8到Lua堆栈` | `index.json` | 0.036s |
| `正确传入void*到Lua堆栈` | `index.json` | 0.037s |
| `正确传入中文FString到Lua堆栈` | `index.json` | 0.036s |
| `正确判断子类` | `index.json` | 0.037s |
| `正确加载蓝图类` | `index.json` | 0.040s |
| `正确获取类默认对象` | `index.json` | 0.037s |
| `沙盒下 UE 命名空间仍能写入：类型注册不会被冻住` | `index.json` | 0.026s |
| `沙盒开启后全局表只读` | `index.json` | 0.025s |
| `没有找到元素，返回0` | `index.json` | 0.027s |
| `添加绑定` | `index.json` | 0.038s |
| `添加绑定：UFunction` | `index.json` | 0.036s |
| `添加绑定：赋值` | `index.json` | 0.038s |
| `清理所有的绑定` | `index.json` | 0.052s |
| `清除数组，移除所有元素` | `index.json` | 0.025s |
| `清除，移除所有元素` | `index.json` | 0.040s |
| `清除，移除所有键值对` | `index.json` | 0.031s |
| `相等` | `index.json` | 0.037s |
| `移除指定元素` | `index.json` | 0.129s |
| `移除指定索引处的元素` | `index.json` | 0.216s |
| `移除绑定` | `index.json` | 0.046s |
| `移除绑定：UFunction` | `index.json` | 0.039s |
| `绑定` | `index.json` | 0.031s |
| `绑定：UFunction` | `index.json` | 0.031s |
| `绑定：赋值` | `index.json` | 0.027s |
| `缩容，移除多余元素` | `index.json` | 0.069s |
| `脚本改写 UE 命名空间被拦截` | `index.json` | 0.024s |
| `脚本能问到当前后端` | `index.json` | 0.026s |
| `获取Map长度` | `index.json` | 0.028s |
| `获取Set长度` | `index.json` | 0.043s |
| `获取对象所在的World` | `index.json` | 0.043s |
| `获取对象的名称` | `index.json` | 0.043s |
| `获取对象的外部对象` | `index.json` | 0.042s |
| `获取对象的有效状态` | `index.json` | 0.156s |
| `获取对象的类型` | `index.json` | 0.047s |
| `获取平方后的长度` | `index.json` | 0.041s |
| `获取所有Key的数组` | `index.json` | 0.027s |
| `获取所有Value的数组` | `index.json` | 0.030s |
| `获取所有元素的数组` | `index.json` | 0.040s |
| `获取指定索引位置的元素引用` | `index.json` | 0.026s |
| `获取数据表中指定行的数据（蓝图）` | `index.json` | 0.044s |
| `获取数组分配的内存指针` | `index.json` | 0.029s |
| `获取数组最后一个元素的索引位置` | `index.json` | 0.028s |
| `获取数组长度` | `index.json` | 0.028s |
| `获取枚举的名称` | `index.json` | 0.036s |
| `获取枚举的本地化文本` | `index.json` | 0.037s |
| `获取枚举类型的最大值` | `index.json` | 0.037s |
| `解绑` | `index.json` | 0.029s |
| `设置Pitch` | `index.json` | 0.039s |
| `设置Pitch/Yaw` | `index.json` | 0.045s |
| `设置Pitch/Yaw/Roll` | `index.json` | 0.048s |
| `设置R` | `index.json` | 0.036s |
| `设置R` | `index.json` | 0.041s |
| `设置RG` | `index.json` | 0.038s |
| `设置RG` | `index.json` | 0.041s |
| `设置RGB` | `index.json` | 0.036s |
| `设置RGB` | `index.json` | 0.040s |
| `设置RGBA` | `index.json` | 0.036s |
| `设置RGBA` | `index.json` | 0.041s |
| `设置X` | `index.json` | 0.036s |
| `设置X` | `index.json` | 0.040s |
| `设置X` | `index.json` | 0.028s |
| `设置X` | `index.json` | 0.041s |
| `设置X` | `index.json` | 0.035s |
| `设置X` | `index.json` | 0.026s |
| `设置X/Y` | `index.json` | 0.040s |
| `设置X/Y` | `index.json` | 0.030s |
| `设置X/Y` | `index.json` | 0.045s |
| `设置X/Y` | `index.json` | 0.030s |
| `设置X/Y` | `index.json` | 0.027s |
| `设置X/Y/Z` | `index.json` | 0.031s |
| `设置X/Y/Z` | `index.json` | 0.035s |
| `设置X/Y/Z` | `index.json` | 0.026s |
| `设置X/Y/Z/W` | `index.json` | 0.035s |
| `设置X/Y/Z/W` | `index.json` | 0.028s |
| `设置XY` | `index.json` | 0.043s |
| `设置指定索引位置的元素` | `index.json` | 0.141s |
| `设置防止无限循环的超时时间` | `index.json` | 1.036s |
| `访问反射类` | `index.json` | 0.027s |
| `访问导出类` | `index.json` | 0.026s |
| `调用失败，返回值被标记为无效` | `index.json` | 0.035s |
| `调用失败，返回值被标记为无效` | `index.json` | 0.037s |
| `调用成功，返回正确的返回值` | `index.json` | 0.036s |
| `调用成功，返回正确的返回值` | `index.json` | 0.038s |
| `转为字符串` | `index.json` | 0.037s |
| `转为字符串` | `index.json` | 0.037s |
| `转为字符串` | `index.json` | 0.037s |
| `转为字符串` | `index.json` | 0.038s |
| `转为字符串` | `index.json` | 0.064s |
| `转为字符串` | `index.json` | 0.054s |
| `转为字符串` | `index.json` | 0.038s |
| `转为字符串` | `index.json` | 0.042s |
| `转为字符串` | `index.json` | 0.032s |
| `转为字符串` | `index.json` | 0.028s |
| `迭代获取Key与Value` | `index.json` | 0.027s |
| `迭代获取数组索引与元素` | `index.json` | 0.092s |
| `追加元素到数组末尾` | `index.json` | 0.026s |
| `追加另外一个数组的所有元素到数组末尾` | `index.json` | 0.026s |
| `随机打乱数组元素位置` | `index.json` | 0.048s |
| `非法位置索引返回false` | `index.json` | 0.031s |
| `预留N个数组元素的空间` | `index.json` | 0.073s |
| `颜色相乘` | `index.json` | 0.041s |
| `颜色相减` | `index.json` | 0.038s |
| `颜色相加` | `index.json` | 0.036s |
| `颜色相加` | `index.json` | 0.040s |
| `颜色相除` | `index.json` | 0.038s |
| `默认参数` | `index.json` | 0.037s |
| `默认参数` | `index.json` | 0.038s |
| `默认参数` | `index.json` | 0.038s |
| `默认参数` | `index.json` | 0.038s |
| `默认参数` | `index.json` | 0.027s |
| `默认参数` | `index.json` | 0.043s |
| `默认参数` | `index.json` | 0.039s |
| `默认参数` | `index.json` | 0.065s |
| `默认参数` | `index.json` | 0.031s |
| `默认参数` | `index.json` | 0.027s |

## 失败明细

### `执行失败返回false`

- 来源：`index.json`
- 耗时：0.037s
- 1 reported
- LogUnLuaNova: Lua error message: [string "invalid chunk"]:1: Incomplete statement: expected assignment or a function call


### `原生：调用的函数本身有返回值，先返回返回值再依次返回参数`

- 来源：`index.json`
- 耗时：0.158s
- 4 reported
- LogUnLuaNova: Lua error message: [string "..."]:5: attempt to index nil with 'TestForBaseSpec2'

- Ret: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:356]
- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:362]
- Expected 'C' to be 31, but it was 30.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `原生：调用的函数本身没有返回值，依次返回参数`

- 来源：`index.json`
- 耗时：0.045s
- 3 reported
- LogUnLuaNova: Lua error message: [string "..."]:6: attempt to index nil with 'TestForBaseSpec1'

- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:317]
- Expected 'D' to be "TestFlag", but it was "Test".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:319]

### `蓝图：调用的函数本身有返回值，先返回返回值再依次返回参数`

- 来源：`index.json`
- 耗时：0.050s
- 4 reported
- LogUnLuaNova: Lua error message: [string "..."]:5: attempt to index nil with 'TestForBaseSpec2'

- Ret: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:291]
- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:297]
- Expected 'C' to be 31, but it was 30.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `蓝图：调用的函数本身没有返回值，依次返回参数`

- 来源：`index.json`
- 耗时：0.046s
- 3 reported
- LogUnLuaNova: Lua error message: [string "..."]:6: attempt to index nil with 'TestForBaseSpec1'

- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:252]
- Expected 'D' to be "TestFlag", but it was "Test".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:254]

### `Dynamic 动态绑定，SpawnActor时指定脚本路径`

- 来源：`index.json`
- 耗时：0.053s
- 1 reported
- Expected 'Error' to be "", but it was "[string "Content/Script/Tests/Binding/BP_UnLuaTestActor.lua"]:37: invalid argument #4 to 'format' (string expected, got integer)".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:99]

### `Multiple 多重绑定：同一个Lua脚本绑定到不同类`

- 来源：`index.json`
- 耗时：0.036s
- 3 reported
- LogUnLuaNova: Lua error message: invalid actor class

- LogUnLuaNova: Lua error message: [string "..."]:2: attempt to index nil with 'RunTest'

- Expected 'Error' to be "", but it was "[string "..."]:2: attempt to index nil with 'RunTest'".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:152]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Binding/BP_UnLuaNovaTestActor_StaticBindingChild.BP_UnLuaTestActor_StaticBindingChild_C'

### `Overridden 覆写：同一个Lua脚本绑定到不同类`

- 来源：`index.json`
- 耗时：0.053s
- 2 reported
- LogUnLuaNova: Lua error message: invalid actor class

- Expected 'Actual' to be "BP ABC Lua", but it was "invalid actor class".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:174]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Binding/BP_UnLuaNovaTestActor_StaticBindingChild.BP_UnLuaTestActor_StaticBindingChild_C'

### `Priority 绑定冲突：静态绑定优先于动态绑定`

- 来源：`index.json`
- 耗时：0.059s
- 1 reported
- Expected 'Error' to be "", but it was "[string "Content/Script/Tests/Binding/BP_UnLuaTestActor.lua"]:37: invalid argument #4 to 'format' (string expected, got integer)".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:127]

### `Static 静态绑定，继承UnLuaInterface::GetModuleName指定脚本路径`

- 来源：`index.json`
- 耗时：0.047s
- 1 reported
- Expected 'Error' to be "", but it was "[string "Content/Script/Tests/Binding/BP_UnLuaTestActor.lua"]:37: invalid argument #4 to 'format' (string expected, got integer)".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:71]

### `获取数据表中指定行的数据（C++）`

- 来源：`index.json`
- 耗时：0.045s
- 2 reported
- LogDataTable: Missing RowStruct while emptying DataTable '/UnLuaNovaTestSuite/Tests/Misc/DataTable_CppTest.DataTable_CppTest', NeedLoad: 'false'!
- Expected 'lua_tostring(L, -1)' to be "Hello", but it was "".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_DataTable.spec.cpp:58]
- ⚠ 1 reported
- ⚠ LogDataTable: Missing RowStruct while loading DataTable '/UnLuaNovaTestSuite/Tests/Misc/DataTable_CppTest.DataTable_CppTest', NeedLoad: 'false'!

### `断点原语可用`

- 来源：`index.json`
- 耗时：0.031s
- 1 reported
- Expected '断点设置与清除都被接受' to be true.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaBackend.spec.cpp:396]

### `访问反射枚举`

- 来源：`index.json`
- 耗时：0.030s
- 1 reported
- Result2: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UELib.spec.cpp:67]

### `创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator`

- 来源：`index.json`
- 耗时：0.053s
- 2 reported
- Expected 'Transform.GetTranslation()' to be X=1.000 Y=1.000 Z=1.000, but it was X=98.451 Y=120.737 Z=1.000 and outside tolerance 0.000100.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_World.spec.cpp:143]
- Actor->SpawnCollisionHandlingMethod: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_World.spec.cpp:144]

### `Get`

- 来源：`index.json`
- 耗时：0.031s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:80]

### `Set`

- 来源：`index.json`
- 耗时：0.031s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:93]

### `Get`

- 来源：`index.json`
- 耗时：0.031s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:52]

### `Set`

- 来源：`index.json`
- 耗时：0.030s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:65]

### `gettime 返回递增的数字，不依赖网络`

- 来源：`index.json`
- 耗时：0.027s
- 1 reported
- LogUnLuaNova: module 'string' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:97 function load_error_handler
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "/Plugins/UnLuaNovaExtensions/LuaSocket/Content/Script/socket.lua"]:10
[string "Plugins/UnLu ...

### `require 拿到 socket 模块`

- 来源：`index.json`
- 耗时：0.028s
- 1 reported
- LogUnLuaNova: module 'string' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:97 function load_error_handler
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "/Plugins/UnLuaNovaExtensions/LuaSocket/Content/Script/socket.lua"]:10
[string "Plugins/UnLu ...

### `Issue286 蓝图 TMap FindRef 错误`

- 来源：`index.json`
- 耗时：0.566s
- 1 reported
- Expected 'Result' to be "passed", but it was "".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue286Test.cpp:27]

### `Issue288 UMG里Image用到的Texture内存泄漏`

- 来源：`index.json`
- 耗时：0.180s
- 3 reported
- LogUnLuaNova: Global_NewObject: Invalid class!
[string "..."]:3

- LogUnLuaNova: Lua error message: [string "..."]:2: attempt to index nil with 'Release'

- LogDataTable: Missing RowStruct while emptying DataTable '/UnLuaNovaTestSuite/Tests/Misc/DataTable_CppTest.DataTable_CppTest', NeedLoad: 'false'!
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue288/UnLuaNovaTestUMG_Issue288.UnLuaNovaTestUMG_Issue288_C'

### `Issue291 delegate绑定本身不在lua里有引用的object，在绑定后触发gc会导致无法调用绑定的函数`

- 来源：`index.json`
- 耗时：0.037s
- 1 reported
- LogUnLuaNova: Lua error message: [string "            local Flag = false            local function test(x, y, z)                Flag = true            end            local ActorClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue291/BP_UnLuaNovaTestActor_Issue291.BP_Un..."]:1: attempt to call a nil value


### `Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃`

- 来源：`index.json`
- 耗时：1.419s
- 4 reported
- LogUnLuaNova: Global_NewObject: Invalid class!
[string "..."]:5

- LogUnLuaNova: Lua error message: [string "..."]:6: attempt to index nil with 'AddToViewport'

- LogUnLuaNova: Global_NewObject: Invalid class!
[string "..."]:5

- LogUnLuaNova: Lua error message: [string "..."]:6: attempt to index nil with 'AddToViewport'

- ⚠ 2 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue295/UnLuaNovaTestUMG_Issue295.UnLuaNovaTestUMG_Issue295_C'
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue295/UnLuaNovaTestUMG_Issue295.UnLuaNovaTestUMG_Issue295_C'

### `Issue300 LUA覆写的函数返回给蓝图的值错误`

- 来源：`index.json`
- 耗时：0.626s
- 2 reported
- Expected 'Result1' to be 10, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue300Test.cpp:27]
- Expected 'Result2' to be 10, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue300Test.cpp:27]

### `Issue314 访问过父类不存在的字段，后续在子类中也只能取到nil`

- 来源：`index.json`
- 耗时：0.047s
- 2 reported
- LogUnLuaNova: Lua error message: invalid actor class

- Expected 'Result' to be true.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue314Test.cpp:52]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Child.BP_UnLuaTestActor_Issue314_Child_C'

### `Issue362 TArray引用做委托参数在lua中修改不生效`

- 来源：`index.json`
- 耗时：0.037s
- 3 reported
- LogUnLuaNova: Global_NewObject: Invalid class!
[string "..."]:2

- LogUnLuaNova: Lua error message: [string "..."]:3: attempt to index nil with 'Issue362Delegate'

- Expected 'Actual' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue362Test.cpp:47]

### `Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留`

- 来源：`index.json`
- 耗时：1.301s
- 1 reported
- Expected 'Actual' to be 7, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue372Test.cpp:27]

### `Issue398 接口列表从lua传到c++/蓝图层会丢失一半信息`

- 来源：`index.json`
- 耗时：0.061s
- 2 reported
- LogUnLuaNova: Lua error message: invalid actor class

- Expected 'Result' to be 4, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue398Test.cpp:60]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue398/BP_Character.BP_Character_C'

### `Issue401 LUA覆写导致数组传参错误`

- 来源：`index.json`
- 耗时：0.726s
- 2 reported
- Expected 'Result1' to be 2, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue401Test.cpp:27]
- Expected 'Result2' to be 2, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue401Test.cpp:27]

### `Issue434 MulticastDelegate创建Handler时和self绑定`

- 来源：`index.json`
- 耗时：1.115s
- 1 reported
- Expected 'Result1' to be true.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue434Test.cpp:28]

### `Issue443 在某些代码块内无法调用蓝图函数`

- 来源：`index.json`
- 耗时：1.560s
- 1 reported
- Expected 'Result' to be true.  [<UE>\Engine\Source\Runtime\Core\Public\Templates\Function.h:298]

### `Issue453 从Lua按引用传递到蓝图的TArray引用变成了空Array`

- 来源：`index.json`
- 耗时：0.562s
- 4 reported
- Expected 'Result1_BP' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]
- Expected 'Result1_Lua' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]
- Expected 'Result2_BP' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]
- Expected 'Result2_Lua' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]

### `Issue583 在Lua中遍历TMap字段的Values接口返回值时引起的崩溃`

- 来源：`index.json`
- 耗时：0.030s
- 1 reported
- LogDataTable: Missing RowStruct while emptying DataTable '/UnLuaNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583.DataTable_Issue583', NeedLoad: 'false'!
- ⚠ 1 reported
- ⚠ LogDataTable: Missing RowStruct while loading DataTable '/UnLuaNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583.DataTable_Issue583', NeedLoad: 'false'!

### `Issue585 蓝图枚举无法被UnLuaRef`

- 来源：`index.json`
- 耗时：0.127s
- 1 reported
- LogDataTable: Missing RowStruct while emptying DataTable '/UnLuaNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583.DataTable_Issue583', NeedLoad: 'false'!

### `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃`

- 来源：`index.json`
- 耗时：1.527s
- 3 reported
- LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] In use pin  Issue 634Struct  no longer exists on node  Make <unknown struct> . Please refresh node or break links to remove pin.
- LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] No Struct in  Make <unknown struct>
- LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] Failed to create property NewParam from  <None>  due to a bad or unknown type (Structure)
- ⚠ 5 reported
- ⚠ LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] Input pin  Buff ID  specifying non-default value no longer exists on node  Make <unknown struct> . Please refresh node or reset pin to default value to remove pin.
- ⚠ LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] Input pin  Over Time  specifying non-default value no longer exists on node  Make <unknown struct> . Please refresh node or reset pin to default value to remove pin.

## 最慢的用例

| 用例 | 耗时 |
|---|---|
| `Issue455 CacheScriptContainer导致的崩溃` | 2.713s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | 2.007s |
| `Issue471 FScriptDelegate解绑无效` | 1.571s |
| `Issue443 在某些代码块内无法调用蓝图函数` | 1.560s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | 1.542s |
| `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃` | 1.527s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | 1.505s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | 1.473s |
| `Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃` | 1.419s |
| `Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留` | 1.301s |

## 截图

没有截图。截图需要真实视口，跑的时候加 `-Screenshots`（脚本会关掉 `-NullRHI` 并把 `Saved/Screenshots` 收进报告目录）。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Publish`。


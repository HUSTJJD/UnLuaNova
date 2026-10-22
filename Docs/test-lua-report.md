---
title: "测试报告（Lua 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Lua 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-22 13:23:57。请勿手工编辑。

## 结论

**全部通过 ✅** — 通过率 100.0%

| 指标 | 数值 |
|---|---|
| 用例总数 | 368 |
| 通过 | 368 |
| 失败 | 0 |
| 跳过 | 0 |
| 总耗时 | 79.29s |
| 截图 | 0 |
| Lua 后端 | `Lua` |

## 通过明细（368 项）

| 用例 | 来源 | 耗时 |
|---|---|---|
| `64 位整数读写不截断` | `index.json` | 0.073s |
| `Dynamic 动态绑定，SpawnActor时指定脚本路径` | `index.json` | 0.089s |
| `F1` | `index.json` | 0.082s |
| `Full 返回全部Out参数` | `index.json` | 0.049s |
| `GC 设置可用，且两参数的 lua_gc 能取到数值` | `index.json` | 0.073s |
| `Get` | `index.json` | 0.089s |
| `Get` | `index.json` | 0.089s |
| `HasFeature 与编译期能力一致` | `index.json` | 0.147s |
| `HotReload 能加载模块，而不是静默降级` | `index.json` | 0.079s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | `index.json` | 2.063s |
| `Issue279 lua覆写蓝图的同名函数之后，无法将特定类型的函数参数保存到lua对象中` | `index.json` | 0.056s |
| `Issue284 require不存在的lib会崩` | `index.json` | 0.069s |
| `Issue286 蓝图 TMap FindRef 错误` | `index.json` | 0.883s |
| `Issue288 UMG里Image用到的Texture内存泄漏` | `index.json` | 0.247s |
| `Issue289 UClass在被UEGC后没有释放相应的绑定` | `index.json` | 0.208s |
| `Issue291 delegate绑定本身不在lua里有引用的object，在绑定后触发gc会导致无法调用绑定的函数` | `index.json` | 0.066s |
| `Issue292 actor调用K2_DestroyActor后，立刻调用IsValid会返回true` | `index.json` | 0.053s |
| `Issue293 关闭RPC会导致部分函数在非Editor模式下crash` | `index.json` | 0.057s |
| `Issue294 AutoCreateRefTerm标记的参数需要生成默认值` | `index.json` | 0.056s |
| `Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃` | `index.json` | 1.554s |
| `Issue300 LUA覆写的函数返回给蓝图的值错误` | `index.json` | 0.860s |
| `Issue303 部分情况下为TArray创建拷贝至lua时会产生崩溃` | `index.json` | 0.045s |
| `Issue304 Lua中监听delegate，接收TArray参数，使用Get获取其中元素导致崩溃` | `index.json` | 0.063s |
| `Issue314 访问过父类不存在的字段，后续在子类中也只能取到nil` | `index.json` | 0.092s |
| `Issue321 同名蓝图加载冲突` | `index.json` | 0.078s |
| `Issue322 绑定中文名字的蓝图会报错` | `index.json` | 0.080s |
| `Issue323 一些默认参数的写法没有分析出来` | `index.json` | 0.059s |
| `Issue328 C++调用LUA覆写带返回值的函数会崩溃` | `index.json` | 0.096s |
| `Issue331 UENUM的某一项为表达式时，默认值没有分析出来` | `index.json` | 0.108s |
| `Issue344 在Lua中对UClass_Load出来的对象写入属性后，GC的时候会Crash` | `index.json` | 0.073s |
| `Issue346 BlueprintFunctionLibrary无法静态绑定到lua文件` | `index.json` | 0.339s |
| `Issue362 TArray引用做委托参数在lua中修改不生效` | `index.json` | 0.069s |
| `Issue366 使用ADD_SHARED_PTR_CONSTRUCTOR后启动直接崩溃` | `index.json` | 0.068s |
| `Issue367 类型元表被LuaGC时，可能意外导致正在使用的FClassDesc被错误delete` | `index.json` | 0.188s |
| `Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留` | `index.json` | 0.948s |
| `Issue375 子类被销毁时，可能会令父类的Lua绑定失效` | `index.json` | 0.121s |
| `Issue376 引用参数传递失败` | `index.json` | 0.059s |
| `Issue398 接口列表从lua传到c++/蓝图层会丢失一半信息` | `index.json` | 0.071s |
| `Issue400 不同实例的多播委托绑定相同实例函数，清理时崩溃` | `index.json` | 0.149s |
| `Issue401 LUA覆写导致数组传参错误` | `index.json` | 0.855s |
| `Issue407 无法在lua中将一个TMap调用Keys的结果传给C++` | `index.json` | 0.037s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | `index.json` | 2.121s |
| `Issue432 Lua调用的函数返回蓝图结构体会check` | `index.json` | 0.050s |
| `Issue434 MulticastDelegate创建Handler时和self绑定` | `index.json` | 1.235s |
| `Issue443 在某些代码块内无法调用蓝图函数` | `index.json` | 1.773s |
| `Issue445 TSubclassOf作为函数返回值，从lua传回C++为空` | `index.json` | 0.073s |
| `Issue446 实现了FTickableGameObject的对象在Tick里调用自身被Lua覆写的方法会崩溃` | `index.json` | 1.360s |
| `Issue453 从Lua按引用传递到蓝图的TArray引用变成了空Array` | `index.json` | 0.787s |
| `Issue455 CacheScriptContainer导致的崩溃` | `index.json` | 3.086s |
| `Issue471 FScriptDelegate解绑无效` | `index.json` | 1.686s |
| `Issue473 FName类型参数中文乱码` | `index.json` | 0.036s |
| `Issue476 通过NewObject接口添加的UObject引用无法释放` | `index.json` | 0.113s |
| `Issue477 遍历蓝图结构体的DataTable会发生拷贝错误` | `index.json` | 0.337s |
| `Issue517 Actor的Struct成员变量在Lua里引用，释放后仍旧可以访问` | `index.json` | 0.122s |
| `Issue539 Lua覆写Out返回值时无法返回nil` | `index.json` | 0.060s |
| `Issue554 访问非TArray的结构体数组报错` | `index.json` | 0.056s |
| `Issue556 Lua覆写的函数接收数组参数时可能无效` | `index.json` | 0.059s |
| `Issue561 访问UStruct内部的委托会check` | `index.json` | 0.052s |
| `Issue562 容器元素为蓝图类型时，在蓝图Class被提前UE回收后，会导致容器在luagc时报错` | `index.json` | 0.151s |
| `Issue566 多次传递委托类型的参数到同一函数时，可能因为Owner失效而无法回调` | `index.json` | 0.124s |
| `Issue583 在Lua中遍历TMap字段的Values接口返回值时引起的崩溃` | `index.json` | 0.058s |
| `Issue585 蓝图枚举无法被UnLuaRef` | `index.json` | 0.205s |
| `Issue595 Lua无法调用Interface中函数` | `index.json` | 0.058s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | `index.json` | 1.794s |
| `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃` | `index.json` | 1.839s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | `index.json` | 1.732s |
| `Issue668 在Lua中调用TArray的Add接口时内存对齐引起的问题` | `index.json` | 0.036s |
| `IssueOverrides 覆写引起的问题` | `index.json` | 1.271s |
| `Key不存在，新增` | `index.json` | 0.091s |
| `Key不存在，返回false` | `index.json` | 0.096s |
| `Key存在，移除并返回true` | `index.json` | 0.089s |
| `Key存在，覆盖` | `index.json` | 0.066s |
| `Lua 侧的 64 位字面量能完整取回` | `index.json` | 0.065s |
| `Multiple 多重绑定：同一个Lua脚本绑定到不同类` | `index.json` | 0.086s |
| `Num` | `index.json` | 0.079s |
| `Overridden 覆写：同一个Lua脚本绑定到不同类` | `index.json` | 0.063s |
| `PUSHINT 推入的值能在脚本里参与算术` | `index.json` | 0.068s |
| `PUSHINT64 推入的值被认作整数且能完整读回` | `index.json` | 0.073s |
| `Partial 返回部分Out参数，剩余部分使用传入参数填充` | `index.json` | 0.049s |
| `Priority 绑定冲突：静态绑定优先于动态绑定` | `index.json` | 0.065s |
| `RepairTestAssets` | `index.json` | 12.216s |
| `Set` | `index.json` | 0.098s |
| `Set` | `index.json` | 0.090s |
| `Static 静态绑定，继承UnLuaInterface::GetModuleName指定脚本路径` | `index.json` | 0.063s |
| `UNLUANOVA_IS_YIELDABLE 区分主线程与协程` | `index.json` | 0.074s |
| `UNLUANOVA_LOAD 能加载并执行 chunk` | `index.json` | 0.070s |
| `UNLUANOVA_RESUME 能恢复协程并带回结果` | `index.json` | 0.079s |
| `UnLuaNovaIsIntegralNumber 区分整数与非整数` | `index.json` | 0.076s |
| `VM hook 能安装、收到事件并按 owner 撤回` | `index.json` | 0.082s |
| `core 也能单独 require 到` | `index.json` | 0.091s |
| `debug 提供 upvalue / local / registry 访问` | `index.json` | 0.066s |
| `decode 能解析出字段` | `index.json` | 0.131s |
| `encode/decode 往返一致` | `index.json` | 0.115s |
| `gettime 返回递增的数字，不依赖网络` | `index.json` | 0.086s |
| `getupvalue 能读到闭包捕获的值` | `index.json` | 0.063s |
| `luaL_newmetatable 建的元表带 __name` | `index.json` | 0.107s |
| `luaL_newmetatable 第二次调用返回 0` | `index.json` | 0.069s |
| `luaL_ref / luaL_unref 往返一致` | `index.json` | 0.083s |
| `package 同时提供 loaded 和 searchers` | `index.json` | 0.072s |
| `protoc 加载内联 proto 后编解码一致` | `index.json` | 0.143s |
| `require 入口在两个后端下都安全返回` | `index.json` | 0.107s |
| `require 拿到 socket 模块` | `index.json` | 0.082s |
| `require 拿到可用的 pb 模块` | `index.json` | 0.164s |
| `require 拿到带 decode/encode 的模块表` | `index.json` | 0.100s |
| `setupvalue 能给 chunk 设环境` | `index.json` | 0.076s |
| `不增加重复的元素` | `index.json` | 0.126s |
| `不存在的枚举值返回nil` | `index.json` | 0.115s |
| `不存在的枚举值返回nil` | `index.json` | 0.097s |
| `不相等` | `index.json` | 0.082s |
| `两个环境互不影响` | `index.json` | 0.083s |
| `两点相乘` | `index.json` | 0.097s |
| `两点相减` | `index.json` | 0.085s |
| `两点相加` | `index.json` | 0.076s |
| `两点相除` | `index.json` | 0.082s |
| `交换两个索引位置的元素` | `index.json` | 0.176s |
| `使用[]获取` | `index.json` | 0.084s |
| `使用[]赋值` | `index.json` | 0.120s |
| `元素不存在，新增` | `index.json` | 0.555s |
| `元素不存在，返回false` | `index.json` | 0.748s |
| `元素存在，移除并返回true` | `index.json` | 0.267s |
| `分别指定Pitch/Yaw/Roll` | `index.json` | 0.088s |
| `分别指定X/Y` | `index.json` | 0.074s |
| `分别指定X/Y/Z` | `index.json` | 0.087s |
| `分别指定X/Y/Z` | `index.json` | 0.083s |
| `分别指定X/Y/Z/W` | `index.json` | 0.088s |
| `分别指定X和Y` | `index.json` | 0.111s |
| `创建Actor，仅指定Actor类型` | `index.json` | 0.117s |
| `创建Actor，指定初始Transform` | `index.json` | 0.159s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner` | `index.json` | 0.460s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator` | `index.json` | 0.125s |
| `创建Actor，指定初始Transform/CollisionHandling` | `index.json` | 0.137s |
| `删除元素后迭代不会带回空洞` | `index.json` | 0.147s |
| `判断对象是否为指定类型` | `index.json` | 0.122s |
| `加载一个对象` | `index.json` | 0.137s |
| `原生：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.100s |
| `原生：调用的函数本身没有返回值，依次返回参数` | `index.json` | 0.081s |
| `取反向量` | `index.json` | 0.095s |
| `取反向量` | `index.json` | 0.075s |
| `取反向量` | `index.json` | 0.077s |
| `取反向量` | `index.json` | 0.064s |
| `取相反数` | `index.json` | 0.091s |
| `只能够对空数组进行预留空间操作` | `index.json` | 0.168s |
| `只追加不重复的元素` | `index.json` | 0.080s |
| `合法位置索引返回true` | `index.json` | 0.069s |
| `同时指定Pitch/Yaw/Roll` | `index.json` | 0.089s |
| `同时指定X/Y` | `index.json` | 0.067s |
| `同时指定X/Y/Z` | `index.json` | 0.090s |
| `同时指定X/Y/Z` | `index.json` | 0.088s |
| `同时指定X和Y` | `index.json` | 0.095s |
| `向量乘` | `index.json` | 0.078s |
| `向量乘以数字` | `index.json` | 0.070s |
| `向量乘以整数` | `index.json` | 0.081s |
| `向量乘以浮点数` | `index.json` | 0.138s |
| `向量相减` | `index.json` | 0.087s |
| `向量相减` | `index.json` | 0.095s |
| `向量相减` | `index.json` | 0.078s |
| `向量相减` | `index.json` | 0.100s |
| `向量相加` | `index.json` | 0.087s |
| `向量相加` | `index.json` | 0.094s |
| `向量相加` | `index.json` | 0.072s |
| `向量相加` | `index.json` | 0.073s |
| `向量除` | `index.json` | 0.065s |
| `向量除以数字` | `index.json` | 0.075s |
| `向量除以整数` | `index.json` | 0.082s |
| `向量除以浮点数` | `index.json` | 0.087s |
| `四元数乘以浮点数` | `index.json` | 0.186s |
| `大于等于容差，归一化` | `index.json` | 0.279s |
| `大于等于容差，归一化` | `index.json` | 0.071s |
| `大于等于容差，归一化并返回true` | `index.json` | 0.098s |
| `将Map内容转为LuaTable` | `index.json` | 0.095s |
| `将Set内容转为LuaTable` | `index.json` | 0.210s |
| `将当前Transform与目标Transform混合` | `index.json` | 0.093s |
| `将当前Transform与目标两个Transform混合` | `index.json` | 0.100s |
| `将数组内容转为LuaTable` | `index.json` | 0.079s |
| `小于容差，不归一化` | `index.json` | 0.235s |
| `小于容差，不归一化` | `index.json` | 0.069s |
| `小于容差，直接返回false` | `index.json` | 0.104s |
| `广播事件` | `index.json` | 0.082s |
| `执行失败返回false` | `index.json` | 0.100s |
| `执行委托` | `index.json` | 0.084s |
| `执行委托：从Lua返回值` | `index.json` | 0.097s |
| `执行委托：赋值` | `index.json` | 0.090s |
| `执行成功返回true` | `index.json` | 0.080s |
| `扩容，使用默认元素填充` | `index.json` | 0.199s |
| `找到元素，返回索引位置` | `index.json` | 0.069s |
| `拷贝指定索引位置的元素` | `index.json` | 0.080s |
| `指定ColorValue` | `index.json` | 0.099s |
| `指定RGB` | `index.json` | 0.092s |
| `指定RGB` | `index.json` | 0.077s |
| `指定RGBA` | `index.json` | 0.084s |
| `指定RGBA` | `index.json` | 0.080s |
| `指定Rotation(FQuat)` | `index.json` | 0.123s |
| `指定Rotation(FQuat)/Translation` | `index.json` | 0.089s |
| `指定Rotation(FQuat)/Translation/Scale` | `index.json` | 0.099s |
| `指定X` | `index.json` | 0.068s |
| `指定X/Y` | `index.json` | 0.067s |
| `指定X/Y/Z` | `index.json` | 0.077s |
| `指定X/Y/Z/W` | `index.json` | 0.088s |
| `指定轴与角度` | `index.json` | 0.082s |
| `按引用获取元素（原生）` | `index.json` | 0.093s |
| `按引用获取元素（原生）` | `index.json` | 0.136s |
| `按引用获取元素（蓝图）` | `index.json` | 0.438s |
| `按引用获取元素（蓝图）` | `index.json` | 0.112s |
| `插入元素到指定位置` | `index.json` | 0.076s |
| `支持启动脚本和参数` | `index.json` | 0.356s |
| `支持多个Lua环境` | `index.json` | 0.177s |
| `支持多参数传入和传出` | `index.json` | 0.078s |
| `数组不包含指定元素返回false` | `index.json` | 0.084s |
| `数组包含指定元素返回true` | `index.json` | 0.072s |
| `构造TArray<FString>` | `index.json` | 0.076s |
| `构造TArray<FVector>` | `index.json` | 0.062s |
| `构造TArray<UStruct>` | `index.json` | 0.068s |
| `构造TArray<bool>` | `index.json` | 0.072s |
| `构造TArray<int32>` | `index.json` | 0.080s |
| `构造TMap<FString,FString>` | `index.json` | 0.095s |
| `构造TMap<FVector,bool>` | `index.json` | 0.097s |
| `构造TMap<UStruct,UStruct>` | `index.json` | 0.100s |
| `构造TMap<int32,int32>` | `index.json` | 0.091s |
| `构造TSet<FString>` | `index.json` | 0.306s |
| `构造TSet<FVector>` | `index.json` | 0.141s |
| `构造TSet<UScriptStruct>` | `index.json` | 0.124s |
| `构造TSet<int32>` | `index.json` | 0.139s |
| `查找Key不存在，返回nil` | `index.json` | 0.066s |
| `查找Key不存在，返回nil` | `index.json` | 0.205s |
| `查找Key存在，返回Value的引用` | `index.json` | 0.124s |
| `查找Key存在，返回Value的拷贝` | `index.json` | 0.105s |
| `查找元素不存在，返回false` | `index.json` | 0.131s |
| `查找元素存在，返回true` | `index.json` | 0.134s |
| `栈帧能取到源码行，Profiler 键稳定且唯一` | `index.json` | 0.060s |
| `正确传入FName到Lua堆栈` | `index.json` | 0.088s |
| `正确传入FString到Lua堆栈` | `index.json` | 0.118s |
| `正确传入UObject到Lua堆栈` | `index.json` | 0.111s |
| `正确传入bool到Lua堆栈` | `index.json` | 0.094s |
| `正确传入const char*到Lua堆栈` | `index.json` | 0.089s |
| `正确传入double到Lua堆栈` | `index.json` | 0.089s |
| `正确传入float到Lua堆栈` | `index.json` | 0.094s |
| `正确传入int16到Lua堆栈` | `index.json` | 0.099s |
| `正确传入int32到Lua堆栈` | `index.json` | 0.094s |
| `正确传入int64到Lua堆栈` | `index.json` | 0.089s |
| `正确传入int8到Lua堆栈` | `index.json` | 0.083s |
| `正确传入struct到Lua堆栈` | `index.json` | 0.088s |
| `正确传入uint16到Lua堆栈` | `index.json` | 0.080s |
| `正确传入uint32到Lua堆栈` | `index.json` | 0.093s |
| `正确传入uint64到Lua堆栈` | `index.json` | 0.096s |
| `正确传入uint8到Lua堆栈` | `index.json` | 0.094s |
| `正确传入void*到Lua堆栈` | `index.json` | 0.087s |
| `正确传入中文FString到Lua堆栈` | `index.json` | 0.138s |
| `正确判断子类` | `index.json` | 0.113s |
| `正确加载蓝图类` | `index.json` | 0.130s |
| `正确获取类默认对象` | `index.json` | 0.225s |
| `没有原生 vector 时 ToVector 降级为 nil` | `index.json` | 0.069s |
| `没有找到元素，返回0` | `index.json` | 0.073s |
| `添加绑定` | `index.json` | 0.090s |
| `添加绑定：UFunction` | `index.json` | 0.084s |
| `添加绑定：赋值` | `index.json` | 0.086s |
| `清理所有的绑定` | `index.json` | 0.091s |
| `清除数组，移除所有元素` | `index.json` | 0.084s |
| `清除，移除所有元素` | `index.json` | 0.125s |
| `清除，移除所有键值对` | `index.json` | 0.079s |
| `相等` | `index.json` | 0.085s |
| `移除指定元素` | `index.json` | 0.116s |
| `移除指定索引处的元素` | `index.json` | 0.299s |
| `移除绑定` | `index.json` | 0.097s |
| `移除绑定：UFunction` | `index.json` | 0.112s |
| `绑定` | `index.json` | 0.082s |
| `绑定：UFunction` | `index.json` | 0.072s |
| `绑定：赋值` | `index.json` | 0.082s |
| `编译期能力位与后端一致` | `index.json` | 0.069s |
| `缩容，移除多余元素` | `index.json` | 0.146s |
| `脚本依赖的标准库在两个后端都在` | `index.json` | 0.074s |
| `脚本改写 UE 命名空间被拦截` | `index.json` | 0.115s |
| `脚本能问到当前后端` | `index.json` | 0.083s |
| `获取Map长度` | `index.json` | 0.969s |
| `获取Set长度` | `index.json` | 0.117s |
| `获取对象所在的World` | `index.json` | 0.148s |
| `获取对象的名称` | `index.json` | 0.132s |
| `获取对象的外部对象` | `index.json` | 0.147s |
| `获取对象的有效状态` | `index.json` | 0.401s |
| `获取对象的类型` | `index.json` | 0.142s |
| `获取平方后的长度` | `index.json` | 0.091s |
| `获取所有Key的数组` | `index.json` | 1.077s |
| `获取所有Value的数组` | `index.json` | 0.133s |
| `获取所有元素的数组` | `index.json` | 0.112s |
| `获取指定索引位置的元素引用` | `index.json` | 0.078s |
| `获取数据表中指定行的数据（C++）` | `index.json` | 0.079s |
| `获取数据表中指定行的数据（蓝图）` | `index.json` | 0.078s |
| `获取数组分配的内存指针` | `index.json` | 0.088s |
| `获取数组最后一个元素的索引位置` | `index.json` | 0.095s |
| `获取数组长度` | `index.json` | 0.090s |
| `获取枚举的名称` | `index.json` | 0.089s |
| `获取枚举的本地化文本` | `index.json` | 0.102s |
| `获取枚举类型的最大值` | `index.json` | 0.101s |
| `蓝图：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.086s |
| `蓝图：调用的函数本身没有返回值，依次返回参数` | `index.json` | 0.084s |
| `解绑` | `index.json` | 0.093s |
| `设置Pitch` | `index.json` | 0.087s |
| `设置Pitch/Yaw` | `index.json` | 0.086s |
| `设置Pitch/Yaw/Roll` | `index.json` | 0.086s |
| `设置R` | `index.json` | 0.100s |
| `设置R` | `index.json` | 0.092s |
| `设置RG` | `index.json` | 0.107s |
| `设置RG` | `index.json` | 0.078s |
| `设置RGB` | `index.json` | 0.101s |
| `设置RGB` | `index.json` | 0.080s |
| `设置RGBA` | `index.json` | 0.091s |
| `设置RGBA` | `index.json` | 0.077s |
| `设置X` | `index.json` | 0.085s |
| `设置X` | `index.json` | 0.078s |
| `设置X` | `index.json` | 0.194s |
| `设置X` | `index.json` | 0.113s |
| `设置X` | `index.json` | 0.074s |
| `设置X` | `index.json` | 0.067s |
| `设置X/Y` | `index.json` | 0.084s |
| `设置X/Y` | `index.json` | 0.128s |
| `设置X/Y` | `index.json` | 0.082s |
| `设置X/Y` | `index.json` | 0.075s |
| `设置X/Y` | `index.json` | 0.075s |
| `设置X/Y/Z` | `index.json` | 0.079s |
| `设置X/Y/Z` | `index.json` | 0.090s |
| `设置X/Y/Z` | `index.json` | 0.079s |
| `设置X/Y/Z/W` | `index.json` | 0.105s |
| `设置X/Y/Z/W` | `index.json` | 0.082s |
| `设置XY` | `index.json` | 0.089s |
| `设置指定索引位置的元素` | `index.json` | 0.091s |
| `设置防止无限循环的超时时间` | `index.json` | 1.093s |
| `访问反射枚举` | `index.json` | 0.095s |
| `访问反射类` | `index.json` | 0.091s |
| `访问导出类` | `index.json` | 0.099s |
| `调用失败，返回值被标记为无效` | `index.json` | 0.085s |
| `调用失败，返回值被标记为无效` | `index.json` | 0.098s |
| `调用成功，返回正确的返回值` | `index.json` | 0.082s |
| `调用成功，返回正确的返回值` | `index.json` | 0.104s |
| `转为字符串` | `index.json` | 0.093s |
| `转为字符串` | `index.json` | 0.094s |
| `转为字符串` | `index.json` | 0.091s |
| `转为字符串` | `index.json` | 0.076s |
| `转为字符串` | `index.json` | 0.096s |
| `转为字符串` | `index.json` | 0.092s |
| `转为字符串` | `index.json` | 0.096s |
| `转为字符串` | `index.json` | 0.074s |
| `转为字符串` | `index.json` | 0.081s |
| `转为字符串` | `index.json` | 0.067s |
| `迭代元方法注册在后端认识的名字下` | `index.json` | 0.081s |
| `迭代拿到Set里的每个元素` | `index.json` | 0.171s |
| `迭代获取Key与Value` | `index.json` | 0.131s |
| `迭代获取数组索引与元素` | `index.json` | 1.317s |
| `追加元素到数组末尾` | `index.json` | 0.079s |
| `追加另外一个数组的所有元素到数组末尾` | `index.json` | 0.077s |
| `销毁环境会跑完所有 __gc，且不破坏内存` | `index.json` | 0.111s |
| `随机打乱数组元素位置` | `index.json` | 0.079s |
| `非整数读回为 0，而不是被截断` | `index.json` | 0.102s |
| `非法位置索引返回false` | `index.json` | 0.079s |
| `预留N个数组元素的空间` | `index.json` | 0.454s |
| `颜色相乘` | `index.json` | 0.079s |
| `颜色相减` | `index.json` | 0.069s |
| `颜色相加` | `index.json` | 0.102s |
| `颜色相加` | `index.json` | 0.085s |
| `颜色相除` | `index.json` | 0.081s |
| `默认参数` | `index.json` | 0.087s |
| `默认参数` | `index.json` | 0.098s |
| `默认参数` | `index.json` | 0.085s |
| `默认参数` | `index.json` | 0.074s |
| `默认参数` | `index.json` | 0.088s |
| `默认参数` | `index.json` | 0.097s |
| `默认参数` | `index.json` | 0.100s |
| `默认参数` | `index.json` | 0.078s |
| `默认参数` | `index.json` | 0.088s |
| `默认参数` | `index.json` | 0.153s |
## 最慢的用例

| 用例 | 耗时 |
|---|---|
| `RepairTestAssets` | 12.216s |
| `Issue455 CacheScriptContainer导致的崩溃` | 3.086s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | 2.121s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | 2.063s |
| `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃` | 1.839s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | 1.794s |
| `Issue443 在某些代码块内无法调用蓝图函数` | 1.773s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | 1.732s |
| `Issue471 FScriptDelegate解绑无效` | 1.686s |
| `Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃` | 1.554s |

## 截图

没有截图。截图由 `UnLuaNova.Showcase.*` 这几条用例产出（默认就会收进报告目录），
加 `-NoScreenshots` 或跑 `-NullRHI`（无头）时不会有画面。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Backend Lua -Publish`（要两份就 `-Backend Both`）。


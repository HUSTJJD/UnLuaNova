---
title: "测试报告（Lua 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Lua 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-22 15:50:29。请勿手工编辑。

## 结论

**存在失败 ❌（2 项）** — 通过率 99.3%

| 指标 | 数值 |
|---|---|
| 用例总数 | 427 |
| 通过 | 424 |
| 失败 | 2 |
| 跳过 | 1 |
| 总耗时 | 191.21s |
| 截图 | 4 |
| Lua 后端 | `Lua` |

## 通过明细（424 项）

| 用例 | 来源 | 耗时 |
|---|---|---|
| `* 全部已注册` | `index.json` | 0.081s |
| `64 位整数读写不截断` | `index.json` | 0.103s |
| `Binding 静态绑定示例截图` | `index.json` | 6.208s |
| `Container 容器访问示例截图` | `index.json` | 5.616s |
| `Dynamic 动态绑定，SpawnActor时指定脚本路径` | `index.json` | 0.310s |
| `F1` | `index.json` | 1.749s |
| `Full 返回全部Out参数` | `index.json` | 0.072s |
| `GC 设置可用，且两参数的 lua_gc 能取到数值` | `index.json` | 0.117s |
| `Get` | `index.json` | 0.109s |
| `Get` | `index.json` | 0.103s |
| `HasBuffer 与 HasFeature('buffer') 一致` | `index.json` | 0.121s |
| `HasFeature 与编译期能力一致` | `index.json` | 0.115s |
| `HasVector 与 HasFeature('vector') 一致` | `index.json` | 0.119s |
| `HotReload 能加载模块，而不是静默降级` | `index.json` | 0.118s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | `index.json` | 3.827s |
| `Issue279 lua覆写蓝图的同名函数之后，无法将特定类型的函数参数保存到lua对象中` | `index.json` | 0.103s |
| `Issue284 require不存在的lib会崩` | `index.json` | 0.113s |
| `Issue286 蓝图 TMap FindRef 错误` | `index.json` | 1.752s |
| `Issue288 UMG里Image用到的Texture内存泄漏` | `index.json` | 0.358s |
| `Issue289 UClass在被UEGC后没有释放相应的绑定` | `index.json` | 0.349s |
| `Issue291 delegate绑定本身不在lua里有引用的object，在绑定后触发gc会导致无法调用绑定的函数` | `index.json` | 0.102s |
| `Issue292 actor调用K2_DestroyActor后，立刻调用IsValid会返回true` | `index.json` | 0.115s |
| `Issue293 关闭RPC会导致部分函数在非Editor模式下crash` | `index.json` | 0.213s |
| `Issue294 AutoCreateRefTerm标记的参数需要生成默认值` | `index.json` | 0.096s |
| `Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃` | `index.json` | 3.173s |
| `Issue300 LUA覆写的函数返回给蓝图的值错误` | `index.json` | 1.232s |
| `Issue303 部分情况下为TArray创建拷贝至lua时会产生崩溃` | `index.json` | 0.061s |
| `Issue304 Lua中监听delegate，接收TArray参数，使用Get获取其中元素导致崩溃` | `index.json` | 0.113s |
| `Issue314 访问过父类不存在的字段，后续在子类中也只能取到nil` | `index.json` | 0.147s |
| `Issue321 同名蓝图加载冲突` | `index.json` | 0.121s |
| `Issue322 绑定中文名字的蓝图会报错` | `index.json` | 0.134s |
| `Issue323 一些默认参数的写法没有分析出来` | `index.json` | 0.101s |
| `Issue328 C++调用LUA覆写带返回值的函数会崩溃` | `index.json` | 0.158s |
| `Issue331 UENUM的某一项为表达式时，默认值没有分析出来` | `index.json` | 0.121s |
| `Issue344 在Lua中对UClass_Load出来的对象写入属性后，GC的时候会Crash` | `index.json` | 0.123s |
| `Issue346 BlueprintFunctionLibrary无法静态绑定到lua文件` | `index.json` | 0.526s |
| `Issue362 TArray引用做委托参数在lua中修改不生效` | `index.json` | 0.098s |
| `Issue366 使用ADD_SHARED_PTR_CONSTRUCTOR后启动直接崩溃` | `index.json` | 0.101s |
| `Issue367 类型元表被LuaGC时，可能意外导致正在使用的FClassDesc被错误delete` | `index.json` | 0.314s |
| `Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留` | `index.json` | 1.381s |
| `Issue375 子类被销毁时，可能会令父类的Lua绑定失效` | `index.json` | 0.197s |
| `Issue376 引用参数传递失败` | `index.json` | 0.098s |
| `Issue398 接口列表从lua传到c++/蓝图层会丢失一半信息` | `index.json` | 0.113s |
| `Issue400 不同实例的多播委托绑定相同实例函数，清理时崩溃` | `index.json` | 0.220s |
| `Issue401 LUA覆写导致数组传参错误` | `index.json` | 1.366s |
| `Issue407 无法在lua中将一个TMap调用Keys的结果传给C++` | `index.json` | 0.049s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | `index.json` | 3.788s |
| `Issue432 Lua调用的函数返回蓝图结构体会check` | `index.json` | 0.225s |
| `Issue434 MulticastDelegate创建Handler时和self绑定` | `index.json` | 1.760s |
| `Issue443 在某些代码块内无法调用蓝图函数` | `index.json` | 2.390s |
| `Issue445 TSubclassOf作为函数返回值，从lua传回C++为空` | `index.json` | 0.132s |
| `Issue446 实现了FTickableGameObject的对象在Tick里调用自身被Lua覆写的方法会崩溃` | `index.json` | 1.907s |
| `Issue453 从Lua按引用传递到蓝图的TArray引用变成了空Array` | `index.json` | 1.167s |
| `Issue455 CacheScriptContainer导致的崩溃` | `index.json` | 3.189s |
| `Issue471 FScriptDelegate解绑无效` | `index.json` | 1.787s |
| `Issue473 FName类型参数中文乱码` | `index.json` | 0.039s |
| `Issue476 通过NewObject接口添加的UObject引用无法释放` | `index.json` | 0.112s |
| `Issue477 遍历蓝图结构体的DataTable会发生拷贝错误` | `index.json` | 0.298s |
| `Issue517 Actor的Struct成员变量在Lua里引用，释放后仍旧可以访问` | `index.json` | 0.108s |
| `Issue539 Lua覆写Out返回值时无法返回nil` | `index.json` | 0.049s |
| `Issue554 访问非TArray的结构体数组报错` | `index.json` | 0.076s |
| `Issue556 Lua覆写的函数接收数组参数时可能无效` | `index.json` | 0.070s |
| `Issue561 访问UStruct内部的委托会check` | `index.json` | 0.068s |
| `Issue562 容器元素为蓝图类型时，在蓝图Class被提前UE回收后，会导致容器在luagc时报错` | `index.json` | 0.132s |
| `Issue566 多次传递委托类型的参数到同一函数时，可能因为Owner失效而无法回调` | `index.json` | 0.112s |
| `Issue583 在Lua中遍历TMap字段的Values接口返回值时引起的崩溃` | `index.json` | 0.046s |
| `Issue585 蓝图枚举无法被UnLuaRef` | `index.json` | 0.134s |
| `Issue595 Lua无法调用Interface中函数` | `index.json` | 0.047s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | `index.json` | 2.501s |
| `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃` | `index.json` | 1.563s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | `index.json` | 2.447s |
| `Issue668 在Lua中调用TArray的Add接口时内存对齐引起的问题` | `index.json` | 0.029s |
| `IssueOverrides 覆写引起的问题` | `index.json` | 1.063s |
| `Key不存在，新增` | `index.json` | 0.119s |
| `Key不存在，返回false` | `index.json` | 0.123s |
| `Key存在，移除并返回true` | `index.json` | 0.126s |
| `Key存在，覆盖` | `index.json` | 0.094s |
| `Lua 侧的 64 位字面量能完整取回` | `index.json` | 0.101s |
| `Multiple 多重绑定：同一个Lua脚本绑定到不同类` | `index.json` | 0.267s |
| `Num` | `index.json` | 0.130s |
| `Overridden 覆写：同一个Lua脚本绑定到不同类` | `index.json` | 0.153s |
| `Override 蓝图事件覆写示例截图` | `index.json` | 6.291s |
| `PUSHINT 推入的值能在脚本里参与算术` | `index.json` | 0.112s |
| `PUSHINT64 推入的值被认作整数且能完整读回` | `index.json` | 0.106s |
| `Partial 返回部分Out参数，剩余部分使用传入参数填充` | `index.json` | 0.070s |
| `Priority 绑定冲突：静态绑定优先于动态绑定` | `index.json` | 0.610s |
| `Reset 清零` | `index.json` | 0.109s |
| `Set` | `index.json` | 0.111s |
| `Set` | `index.json` | 0.108s |
| `Static 静态绑定，继承UnLuaInterface::GetModuleName指定脚本路径` | `index.json` | 0.122s |
| `UMG UMG 与 Lua 生命周期示例截图` | `index.json` | 6.797s |
| `UNLUANOVA_IS_YIELDABLE 区分主线程与协程` | `index.json` | 0.120s |
| `UNLUANOVA_LOAD 能加载并执行 chunk` | `index.json` | 0.103s |
| `UNLUANOVA_RESUME 能恢复协程并带回结果` | `index.json` | 0.119s |
| `UnLuaNovaIsIntegralNumber 区分整数与非整数` | `index.json` | 0.115s |
| `VM hook 能安装、收到事件并按 owner 撤回` | `index.json` | 0.105s |
| `attach / detach` | `index.json` | 0.107s |
| `collect 返回 0，与 plain Lua 一致` | `index.json` | 0.134s |
| `core 也能单独 require 到` | `index.json` | 0.119s |
| `count 返回 KBytes 数字` | `index.json` | 0.121s |
| `debug 提供 upvalue / local / registry 访问` | `index.json` | 0.119s |
| `decode 能解析出字段` | `index.json` | 0.144s |
| `encode/decode 往返一致` | `index.json` | 0.120s |
| `gc 之后 Lua 值仍然可用` | `index.json` | 0.103s |
| `gettime 返回递增的数字，不依赖网络` | `index.json` | 0.141s |
| `getupvalue 能读到闭包捕获的值` | `index.json` | 0.111s |
| `luaL_newmetatable 建的元表带 __name` | `index.json` | 0.115s |
| `luaL_newmetatable 第二次调用返回 0` | `index.json` | 0.101s |
| `luaL_ref / luaL_unref 往返一致` | `index.json` | 0.092s |
| `package 同时提供 loaded 和 searchers` | `index.json` | 0.109s |
| `protoc 加载内联 proto 后编解码一致` | `index.json` | 0.169s |
| `require 也能拿到模块表` | `index.json` | 0.106s |
| `require 入口在两个后端下都安全返回` | `index.json` | 0.340s |
| `require 拿到 socket 模块` | `index.json` | 0.137s |
| `require 拿到可用的 pb 模块` | `index.json` | 0.132s |
| `require 拿到带 decode/encode 的模块表` | `index.json` | 0.131s |
| `setupvalue 能给 chunk 设环境` | `index.json` | 0.097s |
| `trim 是 strip 的别名` | `index.json` | 0.119s |
| `不匹配时为假` | `index.json` | 0.144s |
| `不增加重复的元素` | `index.json` | 0.130s |
| `不存在的枚举值返回nil` | `index.json` | 0.182s |
| `不存在的枚举值返回nil` | `index.json` | 0.198s |
| `不相等` | `index.json` | 0.159s |
| `两个后端都存在` | `index.json` | 0.118s |
| `两个后端都能从字符串加载 chunk` | `index.json` | 0.094s |
| `两个环境互不影响` | `index.json` | 0.164s |
| `两点相乘` | `index.json` | 0.200s |
| `两点相减` | `index.json` | 0.201s |
| `两点相加` | `index.json` | 1.634s |
| `两点相除` | `index.json` | 0.339s |
| `交换两个索引位置的元素` | `index.json` | 0.105s |
| `使用[]获取` | `index.json` | 0.103s |
| `使用[]赋值` | `index.json` | 0.103s |
| `元素不存在，新增` | `index.json` | 0.113s |
| `元素不存在，返回false` | `index.json` | 0.135s |
| `元素存在，移除并返回true` | `index.json` | 0.132s |
| `关闭后不再记账` | `index.json` | 0.107s |
| `函数解析到真实的 UFunction 并填好 flag` | `index.json` | 0.106s |
| `分别指定Pitch/Yaw/Roll` | `index.json` | 0.119s |
| `分别指定X/Y` | `index.json` | 0.599s |
| `分别指定X/Y/Z` | `index.json` | 1.688s |
| `分别指定X/Y/Z` | `index.json` | 0.140s |
| `分别指定X/Y/Z/W` | `index.json` | 0.265s |
| `分别指定X和Y` | `index.json` | 2.064s |
| `分发条件：非网络 / 非 latent / 非接口函数会走预解析路径` | `index.json` | 0.107s |
| `分隔符按字面量处理，不是模式` | `index.json` | 0.111s |
| `创建Actor，仅指定Actor类型` | `index.json` | 0.144s |
| `创建Actor，指定初始Transform` | `index.json` | 0.140s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner` | `index.json` | 0.131s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator` | `index.json` | 0.166s |
| `创建Actor，指定初始Transform/CollisionHandling` | `index.json` | 0.151s |
| `删除元素后迭代不会带回空洞` | `index.json` | 0.141s |
| `判断对象是否为指定类型` | `index.json` | 0.165s |
| `前缀` | `index.json` | 0.122s |
| `加载一个对象` | `index.json` | 0.175s |
| `原生后端下可以往返 FVector` | `index.json` | 0.112s |
| `原生：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.186s |
| `原生：调用的函数本身没有返回值，依次返回参数` | `index.json` | 0.155s |
| `去掉首尾空白` | `index.json` | 0.121s |
| `取反向量` | `index.json` | 0.502s |
| `取反向量` | `index.json` | 0.140s |
| `取反向量` | `index.json` | 0.148s |
| `取反向量` | `index.json` | 0.108s |
| `取消注册后查不到` | `index.json` | 0.118s |
| `取相反数` | `index.json` | 0.259s |
| `只能够对空数组进行预留空间操作` | `index.json` | 0.109s |
| `只追加不重复的元素` | `index.json` | 0.119s |
| `可以 require 到模块表` | `index.json` | 0.109s |
| `可以 require 到模块表` | `index.json` | 0.104s |
| `合法位置索引返回true` | `index.json` | 0.120s |
| `同时指定Pitch/Yaw/Roll` | `index.json` | 0.110s |
| `同时指定X/Y` | `index.json` | 0.150s |
| `同时指定X/Y/Z` | `index.json` | 0.282s |
| `同时指定X/Y/Z` | `index.json` | 0.124s |
| `同时指定X和Y` | `index.json` | 0.219s |
| `后缀` | `index.json` | 0.127s |
| `向量乘` | `index.json` | 0.102s |
| `向量乘以数字` | `index.json` | 0.101s |
| `向量乘以整数` | `index.json` | 0.161s |
| `向量乘以浮点数` | `index.json` | 0.124s |
| `向量相减` | `index.json` | 0.166s |
| `向量相减` | `index.json` | 0.219s |
| `向量相减` | `index.json` | 0.161s |
| `向量相减` | `index.json` | 0.106s |
| `向量相加` | `index.json` | 0.154s |
| `向量相加` | `index.json` | 0.172s |
| `向量相加` | `index.json` | 0.119s |
| `向量相加` | `index.json` | 0.135s |
| `向量除` | `index.json` | 0.133s |
| `向量除以数字` | `index.json` | 0.126s |
| `向量除以整数` | `index.json` | 0.144s |
| `向量除以浮点数` | `index.json` | 0.146s |
| `启动时已填充，且至少有一个函数带默认值` | `index.json` | 0.145s |
| `四元数乘以浮点数` | `index.json` | 0.186s |
| `大于等于容差，归一化` | `index.json` | 0.339s |
| `大于等于容差，归一化` | `index.json` | 0.120s |
| `大于等于容差，归一化并返回true` | `index.json` | 0.134s |
| `将Map内容转为LuaTable` | `index.json` | 0.118s |
| `将Set内容转为LuaTable` | `index.json` | 0.150s |
| `将当前Transform与目标Transform混合` | `index.json` | 0.129s |
| `将当前Transform与目标两个Transform混合` | `index.json` | 0.158s |
| `将数组内容转为LuaTable` | `index.json` | 0.095s |
| `小于容差，不归一化` | `index.json` | 2.496s |
| `小于容差，不归一化` | `index.json` | 0.116s |
| `小于容差，直接返回false` | `index.json` | 0.160s |
| `属性解析到真实的 FProperty` | `index.json` | 0.105s |
| `常见 UProperty 映射到对应的 Kind` | `index.json` | 0.091s |
| `广播事件` | `index.json` | 0.213s |
| `开关可切换， detach 之后关闭` | `index.json` | 0.095s |
| `开启后统计分配次数与字节数` | `index.json` | 0.103s |
| `开启后脚本仍能正常执行` | `index.json` | 0.102s |
| `执行失败返回false` | `index.json` | 0.144s |
| `执行委托` | `index.json` | 0.106s |
| `执行委托：从Lua返回值` | `index.json` | 0.132s |
| `执行委托：赋值` | `index.json` | 0.124s |
| `执行成功返回true` | `index.json` | 0.189s |
| `扩容，使用默认元素填充` | `index.json` | 0.111s |
| `找到元素，返回索引位置` | `index.json` | 0.107s |
| `拷贝指定索引位置的元素` | `index.json` | 0.094s |
| `指定ColorValue` | `index.json` | 0.435s |
| `指定RGB` | `index.json` | 0.223s |
| `指定RGB` | `index.json` | 0.337s |
| `指定RGBA` | `index.json` | 0.171s |
| `指定RGBA` | `index.json` | 0.173s |
| `指定Rotation(FQuat)` | `index.json` | 0.179s |
| `指定Rotation(FQuat)/Translation` | `index.json` | 0.131s |
| `指定Rotation(FQuat)/Translation/Scale` | `index.json` | 0.153s |
| `指定X` | `index.json` | 0.102s |
| `指定X/Y` | `index.json` | 0.098s |
| `指定X/Y/Z` | `index.json` | 0.104s |
| `指定X/Y/Z/W` | `index.json` | 0.134s |
| `指定轴与角度` | `index.json` | 0.326s |
| `按引用获取元素（原生）` | `index.json` | 0.150s |
| `按引用获取元素（原生）` | `index.json` | 0.116s |
| `按引用获取元素（蓝图）` | `index.json` | 0.128s |
| `按引用获取元素（蓝图）` | `index.json` | 0.109s |
| `插入元素到指定位置` | `index.json` | 0.111s |
| `支持启动脚本和参数` | `index.json` | 5.039s |
| `支持多个Lua环境` | `index.json` | 3.219s |
| `支持多参数传入和传出` | `index.json` | 1.640s |
| `数组不包含指定元素返回false` | `index.json` | 0.104s |
| `数组包含指定元素返回true` | `index.json` | 0.099s |
| `断点登记` | `index.json` | 0.174s |
| `显式传参会覆盖默认值` | `index.json` | 0.136s |
| `有 buffer 的后端下 TArray 往返保持内容` | `index.json` | 0.109s |
| `有静态绑定时出参与返回值都正确` | `index.json` | 0.127s |
| `有静态绑定时，Lua 调用结果正确` | `index.json` | 0.129s |
| `构造TArray<FString>` | `index.json` | 0.101s |
| `构造TArray<FVector>` | `index.json` | 0.105s |
| `构造TArray<UStruct>` | `index.json` | 0.105s |
| `构造TArray<bool>` | `index.json` | 0.100s |
| `构造TArray<int32>` | `index.json` | 0.102s |
| `构造TMap<FString,FString>` | `index.json` | 0.105s |
| `构造TMap<FVector,bool>` | `index.json` | 0.106s |
| `构造TMap<UStruct,UStruct>` | `index.json` | 0.115s |
| `构造TMap<int32,int32>` | `index.json` | 0.136s |
| `构造TSet<FString>` | `index.json` | 0.119s |
| `构造TSet<FVector>` | `index.json` | 0.136s |
| `构造TSet<UScriptStruct>` | `index.json` | 0.133s |
| `构造TSet<int32>` | `index.json` | 0.137s |
| `枚举默认值也生效` | `index.json` | 0.140s |
| `查找Key不存在，返回nil` | `index.json` | 0.105s |
| `查找Key不存在，返回nil` | `index.json` | 0.113s |
| `查找Key存在，返回Value的引用` | `index.json` | 0.113s |
| `查找Key存在，返回Value的拷贝` | `index.json` | 0.113s |
| `查找元素不存在，返回false` | `index.json` | 0.134s |
| `查找元素存在，返回true` | `index.json` | 0.121s |
| `栈帧能取到源码行，Profiler 键稳定且唯一` | `index.json` | 0.118s |
| `正确传入FName到Lua堆栈` | `index.json` | 0.304s |
| `正确传入FString到Lua堆栈` | `index.json` | 0.160s |
| `正确传入UObject到Lua堆栈` | `index.json` | 2.333s |
| `正确传入bool到Lua堆栈` | `index.json` | 0.829s |
| `正确传入const char*到Lua堆栈` | `index.json` | 0.215s |
| `正确传入double到Lua堆栈` | `index.json` | 0.242s |
| `正确传入float到Lua堆栈` | `index.json` | 0.222s |
| `正确传入int16到Lua堆栈` | `index.json` | 0.138s |
| `正确传入int32到Lua堆栈` | `index.json` | 1.708s |
| `正确传入int64到Lua堆栈` | `index.json` | 2.767s |
| `正确传入int8到Lua堆栈` | `index.json` | 0.170s |
| `正确传入struct到Lua堆栈` | `index.json` | 0.190s |
| `正确传入uint16到Lua堆栈` | `index.json` | 0.308s |
| `正确传入uint32到Lua堆栈` | `index.json` | 0.400s |
| `正确传入uint64到Lua堆栈` | `index.json` | 0.215s |
| `正确传入uint8到Lua堆栈` | `index.json` | 0.185s |
| `正确传入void*到Lua堆栈` | `index.json` | 0.665s |
| `正确传入中文FString到Lua堆栈` | `index.json` | 0.173s |
| `正确判断子类` | `index.json` | 0.112s |
| `正确加载蓝图类` | `index.json` | 0.119s |
| `正确获取类默认对象` | `index.json` | 0.139s |
| `没有分隔符时返回整串` | `index.json` | 0.116s |
| `没有原生 vector 时 ToVector 降级为 nil` | `index.json` | 0.113s |
| `没有找到元素，返回0` | `index.json` | 0.101s |
| `没有静态绑定时，同一个调用的结果一致` | `index.json` | 1.454s |
| `注册后能按 USTRUCT 和名字查到同一个表` | `index.json` | 0.085s |
| `添加绑定` | `index.json` | 0.863s |
| `添加绑定：UFunction` | `index.json` | 0.317s |
| `添加绑定：赋值` | `index.json` | 0.228s |
| `清理所有的绑定` | `index.json` | 2.005s |
| `清除数组，移除所有元素` | `index.json` | 0.104s |
| `清除，移除所有元素` | `index.json` | 0.116s |
| `清除，移除所有键值对` | `index.json` | 0.107s |
| `环境启动后即可直接用` | `index.json` | 0.100s |
| `生成静态绑定表到 -OutputDir 指定的目录` | `index.json` | 4.893s |
| `用分隔符拼接并转字符串` | `index.json` | 0.102s |
| `相等` | `index.json` | 0.444s |
| `省略的 C++ 默认参数照样生效` | `index.json` | 0.326s |
| `移除指定元素` | `index.json` | 0.111s |
| `移除指定索引处的元素` | `index.json` | 0.127s |
| `移除绑定` | `index.json` | 0.505s |
| `移除绑定：UFunction` | `index.json` | 0.184s |
| `空串恒为真` | `index.json` | 0.117s |
| `空的子串返回 0 而不是死循环` | `index.json` | 0.111s |
| `空表得到空串` | `index.json` | 0.118s |
| `绑定` | `index.json` | 0.106s |
| `绑定：UFunction` | `index.json` | 0.090s |
| `绑定：赋值` | `index.json` | 0.107s |
| `统计不重叠出现次数` | `index.json` | 0.113s |
| `编译期能力位与后端一致` | `index.json` | 0.132s |
| `编辑器模块可加载且命令上下文已注册` | `index.json` | 0.107s |
| `缩容，移除多余元素` | `index.json` | 0.101s |
| `脚本依赖的标准库在两个后端都在` | `index.json` | 0.104s |
| `脚本改写 UE 命名空间被拦截` | `index.json` | 0.088s |
| `脚本根目录指向工程的 Content/Script` | `index.json` | 0.100s |
| `脚本能问到当前后端` | `index.json` | 0.118s |
| `获取Map长度` | `index.json` | 0.108s |
| `获取Set长度` | `index.json` | 0.123s |
| `获取对象所在的World` | `index.json` | 0.182s |
| `获取对象的名称` | `index.json` | 0.192s |
| `获取对象的外部对象` | `index.json` | 0.180s |
| `获取对象的有效状态` | `index.json` | 0.417s |
| `获取对象的类型` | `index.json` | 0.196s |
| `获取平方后的长度` | `index.json` | 0.884s |
| `获取所有Key的数组` | `index.json` | 0.120s |
| `获取所有Value的数组` | `index.json` | 0.112s |
| `获取所有元素的数组` | `index.json` | 0.135s |
| `获取指定索引位置的元素引用` | `index.json` | 0.114s |
| `获取数据表中指定行的数据（C++）` | `index.json` | 2.072s |
| `获取数据表中指定行的数据（蓝图）` | `index.json` | 0.137s |
| `获取数组分配的内存指针` | `index.json` | 0.111s |
| `获取数组最后一个元素的索引位置` | `index.json` | 0.126s |
| `获取数组长度` | `index.json` | 0.124s |
| `获取枚举的名称` | `index.json` | 0.154s |
| `获取枚举的本地化文本` | `index.json` | 0.193s |
| `获取枚举类型的最大值` | `index.json` | 0.237s |
| `蓝图：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.169s |
| `蓝图：调用的函数本身没有返回值，依次返回参数` | `index.json` | 1.959s |
| `表里写了不存在的成员时跳过而不是崩` | `index.json` | 0.089s |
| `解绑` | `index.json` | 0.133s |
| `设置Pitch` | `index.json` | 0.209s |
| `设置Pitch/Yaw` | `index.json` | 1.587s |
| `设置Pitch/Yaw/Roll` | `index.json` | 0.140s |
| `设置R` | `index.json` | 0.492s |
| `设置R` | `index.json` | 0.246s |
| `设置RG` | `index.json` | 0.378s |
| `设置RG` | `index.json` | 0.205s |
| `设置RGB` | `index.json` | 0.150s |
| `设置RGB` | `index.json` | 0.787s |
| `设置RGBA` | `index.json` | 0.187s |
| `设置RGBA` | `index.json` | 1.699s |
| `设置X` | `index.json` | 0.595s |
| `设置X` | `index.json` | 0.129s |
| `设置X` | `index.json` | 0.165s |
| `设置X` | `index.json` | 0.128s |
| `设置X` | `index.json` | 0.124s |
| `设置X` | `index.json` | 0.131s |
| `设置X/Y` | `index.json` | 5.948s |
| `设置X/Y` | `index.json` | 0.149s |
| `设置X/Y` | `index.json` | 0.187s |
| `设置X/Y` | `index.json` | 0.139s |
| `设置X/Y` | `index.json` | 0.119s |
| `设置X/Y/Z` | `index.json` | 0.183s |
| `设置X/Y/Z` | `index.json` | 0.173s |
| `设置X/Y/Z` | `index.json` | 0.112s |
| `设置X/Y/Z/W` | `index.json` | 0.586s |
| `设置X/Y/Z/W` | `index.json` | 0.114s |
| `设置XY` | `index.json` | 0.474s |
| `设置指定索引位置的元素` | `index.json` | 0.106s |
| `设置防止无限循环的超时时间` | `index.json` | 1.093s |
| `访问反射枚举` | `index.json` | 0.100s |
| `访问反射类` | `index.json` | 0.098s |
| `访问导出类` | `index.json` | 0.106s |
| `调用失败，返回值被标记为无效` | `index.json` | 4.045s |
| `调用失败，返回值被标记为无效` | `index.json` | 0.231s |
| `调用成功，返回正确的返回值` | `index.json` | 0.234s |
| `调用成功，返回正确的返回值` | `index.json` | 0.621s |
| `转为字符串` | `index.json` | 0.241s |
| `转为字符串` | `index.json` | 0.248s |
| `转为字符串` | `index.json` | 0.203s |
| `转为字符串` | `index.json` | 0.567s |
| `转为字符串` | `index.json` | 0.247s |
| `转为字符串` | `index.json` | 0.141s |
| `转为字符串` | `index.json` | 0.175s |
| `转为字符串` | `index.json` | 0.965s |
| `转为字符串` | `index.json` | 0.139s |
| `转为字符串` | `index.json` | 0.108s |
| `运行时模块都在插件的 Content/Script 下` | `index.json` | 0.107s |
| `迭代元方法注册在后端认识的名字下` | `index.json` | 0.093s |
| `迭代拿到Set里的每个元素` | `index.json` | 0.123s |
| `迭代获取Key与Value` | `index.json` | 0.118s |
| `迭代获取数组索引与元素` | `index.json` | 0.140s |
| `追加元素到数组末尾` | `index.json` | 0.103s |
| `追加另外一个数组的所有元素到数组末尾` | `index.json` | 0.099s |
| `销毁环境会跑完所有 __gc，且不破坏内存` | `index.json` | 0.136s |
| `随插件发布且带 ClassName 占位符` | `index.json` | 0.097s |
| `随机打乱数组元素位置` | `index.json` | 0.104s |
| `非整数读回为 0，而不是被截断` | `index.json` | 0.116s |
| `非法位置索引返回false` | `index.json` | 0.130s |
| `预留N个数组元素的空间` | `index.json` | 0.105s |
| `颜色相乘` | `index.json` | 0.741s |
| `颜色相减` | `index.json` | 0.381s |
| `颜色相加` | `index.json` | 0.157s |
| `颜色相加` | `index.json` | 8.208s |
| `颜色相除` | `index.json` | 3.354s |
| `默认参数` | `index.json` | 0.412s |
| `默认参数` | `index.json` | 0.196s |
| `默认参数` | `index.json` | 0.464s |
| `默认参数` | `index.json` | 0.206s |
| `默认参数` | `index.json` | 0.162s |
| `默认参数` | `index.json` | 0.114s |
| `默认参数` | `index.json` | 0.157s |
| `默认参数` | `index.json` | 0.120s |
| `默认参数` | `index.json` | 0.108s |
| `默认参数` | `index.json` | 0.105s |
| `默认逗号分隔` | `index.json` | 0.117s |

## 跳过明细（1 项）

| 用例 | 来源 |
|---|---|
| `luau-analyze` | `luau-analyze.json` |

## 失败明细

### `未知选项报错而不是静默`

- 来源：`index.json`
- 耗时：0.450s
- 1 reported
- Expected 'lua_toboolean(L, -1)' to be 0, but it was 1.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `单步能停下来并抓到调用栈`

- 来源：`index.json`
- 耗时：0.114s
- 2 reported
- Debug.GetHitCount() > 0: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\Observability.spec.cpp:192]
- Debug.GetStackTrace().Num() > 0: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\Observability.spec.cpp:193]

## 最慢的用例

| 用例 | 耗时 |
|---|---|
| `颜色相加` | 8.208s |
| `UMG UMG 与 Lua 生命周期示例截图` | 6.797s |
| `Override 蓝图事件覆写示例截图` | 6.291s |
| `Binding 静态绑定示例截图` | 6.208s |
| `设置X/Y` | 5.948s |
| `Container 容器访问示例截图` | 5.616s |
| `支持启动脚本和参数` | 5.039s |
| `生成静态绑定表到 -OutputDir 指定的目录` | 4.893s |
| `调用失败，返回值被标记为无效` | 4.045s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | 3.827s |

## 截图

### Showcase_Binding

![Showcase_Binding](Images/tests-lua/Showcase_Binding.png)

### Showcase_Container

![Showcase_Container](Images/tests-lua/Showcase_Container.png)

### Showcase_Override

![Showcase_Override](Images/tests-lua/Showcase_Override.png)

### Showcase_UMG

![Showcase_UMG](Images/tests-lua/Showcase_UMG.png)

## 视频

### Showcase_Binding_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Binding_Clip.mp4"></video>

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Container_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Container_Clip.mp4"></video>

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Override_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Override_Clip.mp4"></video>

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_UMG_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_UMG_Clip.mp4"></video>

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Backend Lua -Publish`（要两份就 `-Backend Both`）。


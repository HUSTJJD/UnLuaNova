---
title: "测试报告（Lua 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Lua 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-23 11:28:37。请勿手工编辑。

## 结论

**存在失败 ❌（1 项）** — 通过率 99.8%

| 指标 | 数值 |
|---|---|
| 用例总数 | 509 |
| 通过 | 508 |
| 失败 | 1 |
| 跳过 | 0 |
| 总耗时 | 203.46s |
| 截图 | 4 |
| Lua 后端 | `Lua` |

## 通过明细（508 项）

| 用例 | 来源 | 耗时 |
|---|---|---|
| `* 全部已注册` | `index.json` | 0.160s |
| `64 位整数读写不截断` | `index.json` | 0.083s |
| `AddManualObjectReference 让对象不被 GC` | `index.json` | 0.267s |
| `Binding 静态绑定示例截图` | `index.json` | 19.787s |
| `BlueprintImplementableEvent 可覆写，BlueprintCallable 不可覆写` | `index.json` | 0.098s |
| `ByPackage 对原生类返回类名，与默认不同` | `index.json` | 0.065s |
| `Container 容器访问示例截图` | `index.json` | 18.400s |
| `DanglingCheck 可设置且能还原` | `index.json` | 0.105s |
| `DeadLoopCheck 可设置且能还原` | `index.json` | 0.084s |
| `DoString 返回 false 并把栈还原` | `index.json` | 0.095s |
| `DoString 返回 false，且之后的环境仍可用` | `index.json` | 0.093s |
| `Dynamic 动态绑定，SpawnActor时指定脚本路径` | `index.json` | 0.105s |
| `F1` | `index.json` | 0.108s |
| `Full 返回全部Out参数` | `index.json` | 0.049s |
| `GC 之后环境仍然可用` | `index.json` | 0.083s |
| `GC 设置可用，且两参数的 lua_gc 能取到数值` | `index.json` | 0.077s |
| `Get` | `index.json` | 0.094s |
| `Get` | `index.json` | 0.132s |
| `GetAll 与 FindEnv 能找到新环境` | `index.json` | 0.088s |
| `GetOverridableFunctions 只列出可覆写的函数` | `index.json` | 0.169s |
| `GetTypeName 对属性给出非空类型名` | `index.json` | 0.117s |
| `HasBuffer 与 HasFeature('buffer') 一致` | `index.json` | 0.082s |
| `HasFeature 与编译期能力一致` | `index.json` | 0.077s |
| `HasVector 与 HasFeature('vector') 一致` | `index.json` | 0.087s |
| `HotReload 之后环境仍然可用` | `index.json` | 0.121s |
| `HotReload 能加载模块，而不是静默降级` | `index.json` | 0.116s |
| `IsReleasedPtr 只认那一个哨兵值` | `index.json` | 0.061s |
| `IsValid 区分可导出与不可导出的函数` | `index.json` | 0.116s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | `index.json` | 6.625s |
| `Issue279 lua覆写蓝图的同名函数之后，无法将特定类型的函数参数保存到lua对象中` | `index.json` | 0.096s |
| `Issue284 require不存在的lib会崩` | `index.json` | 0.083s |
| `Issue286 蓝图 TMap FindRef 错误` | `index.json` | 0.978s |
| `Issue288 UMG里Image用到的Texture内存泄漏` | `index.json` | 0.326s |
| `Issue289 UClass在被UEGC后没有释放相应的绑定` | `index.json` | 0.269s |
| `Issue291 delegate绑定本身不在lua里有引用的object，在绑定后触发gc会导致无法调用绑定的函数` | `index.json` | 0.085s |
| `Issue292 actor调用K2_DestroyActor后，立刻调用IsValid会返回true` | `index.json` | 0.076s |
| `Issue293 关闭RPC会导致部分函数在非Editor模式下crash` | `index.json` | 0.078s |
| `Issue294 AutoCreateRefTerm标记的参数需要生成默认值` | `index.json` | 0.066s |
| `Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃` | `index.json` | 1.748s |
| `Issue300 LUA覆写的函数返回给蓝图的值错误` | `index.json` | 0.947s |
| `Issue303 部分情况下为TArray创建拷贝至lua时会产生崩溃` | `index.json` | 0.062s |
| `Issue304 Lua中监听delegate，接收TArray参数，使用Get获取其中元素导致崩溃` | `index.json` | 0.071s |
| `Issue314 访问过父类不存在的字段，后续在子类中也只能取到nil` | `index.json` | 0.116s |
| `Issue321 同名蓝图加载冲突` | `index.json` | 0.093s |
| `Issue322 绑定中文名字的蓝图会报错` | `index.json` | 0.107s |
| `Issue323 一些默认参数的写法没有分析出来` | `index.json` | 0.082s |
| `Issue328 C++调用LUA覆写带返回值的函数会崩溃` | `index.json` | 0.109s |
| `Issue331 UENUM的某一项为表达式时，默认值没有分析出来` | `index.json` | 0.088s |
| `Issue344 在Lua中对UClass_Load出来的对象写入属性后，GC的时候会Crash` | `index.json` | 0.096s |
| `Issue346 BlueprintFunctionLibrary无法静态绑定到lua文件` | `index.json` | 0.765s |
| `Issue362 TArray引用做委托参数在lua中修改不生效` | `index.json` | 0.106s |
| `Issue366 使用ADD_SHARED_PTR_CONSTRUCTOR后启动直接崩溃` | `index.json` | 0.107s |
| `Issue367 类型元表被LuaGC时，可能意外导致正在使用的FClassDesc被错误delete` | `index.json` | 0.254s |
| `Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留` | `index.json` | 0.921s |
| `Issue375 子类被销毁时，可能会令父类的Lua绑定失效` | `index.json` | 0.196s |
| `Issue376 引用参数传递失败` | `index.json` | 0.083s |
| `Issue398 接口列表从lua传到c++/蓝图层会丢失一半信息` | `index.json` | 0.095s |
| `Issue400 不同实例的多播委托绑定相同实例函数，清理时崩溃` | `index.json` | 0.181s |
| `Issue401 LUA覆写导致数组传参错误` | `index.json` | 0.814s |
| `Issue407 无法在lua中将一个TMap调用Keys的结果传给C++` | `index.json` | 0.046s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | `index.json` | 5.733s |
| `Issue432 Lua调用的函数返回蓝图结构体会check` | `index.json` | 0.239s |
| `Issue434 MulticastDelegate创建Handler时和self绑定` | `index.json` | 1.541s |
| `Issue443 在某些代码块内无法调用蓝图函数` | `index.json` | 1.920s |
| `Issue445 TSubclassOf作为函数返回值，从lua传回C++为空` | `index.json` | 0.079s |
| `Issue446 实现了FTickableGameObject的对象在Tick里调用自身被Lua覆写的方法会崩溃` | `index.json` | 1.455s |
| `Issue453 从Lua按引用传递到蓝图的TArray引用变成了空Array` | `index.json` | 0.869s |
| `Issue455 CacheScriptContainer导致的崩溃` | `index.json` | 3.269s |
| `Issue471 FScriptDelegate解绑无效` | `index.json` | 1.857s |
| `Issue473 FName类型参数中文乱码` | `index.json` | 0.068s |
| `Issue476 通过NewObject接口添加的UObject引用无法释放` | `index.json` | 0.172s |
| `Issue477 遍历蓝图结构体的DataTable会发生拷贝错误` | `index.json` | 0.470s |
| `Issue517 Actor的Struct成员变量在Lua里引用，释放后仍旧可以访问` | `index.json` | 0.192s |
| `Issue539 Lua覆写Out返回值时无法返回nil` | `index.json` | 0.081s |
| `Issue554 访问非TArray的结构体数组报错` | `index.json` | 0.086s |
| `Issue556 Lua覆写的函数接收数组参数时可能无效` | `index.json` | 0.087s |
| `Issue561 访问UStruct内部的委托会check` | `index.json` | 0.079s |
| `Issue562 容器元素为蓝图类型时，在蓝图Class被提前UE回收后，会导致容器在luagc时报错` | `index.json` | 0.180s |
| `Issue566 多次传递委托类型的参数到同一函数时，可能因为Owner失效而无法回调` | `index.json` | 0.152s |
| `Issue583 在Lua中遍历TMap字段的Values接口返回值时引起的崩溃` | `index.json` | 0.076s |
| `Issue585 蓝图枚举无法被UnLuaRef` | `index.json` | 0.341s |
| `Issue595 Lua无法调用Interface中函数` | `index.json` | 0.063s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | `index.json` | 6.715s |
| `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃` | `index.json` | 2.012s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | `index.json` | 6.443s |
| `Issue668 在Lua中调用TArray的Add接口时内存对齐引起的问题` | `index.json` | 0.056s |
| `IssueOverrides 覆写引起的问题` | `index.json` | 1.364s |
| `Key不存在，新增` | `index.json` | 0.114s |
| `Key不存在，返回false` | `index.json` | 0.102s |
| `Key存在，移除并返回true` | `index.json` | 0.116s |
| `Key存在，覆盖` | `index.json` | 0.081s |
| `Lua 侧用例全部通过` | `index.json` | 0.119s |
| `Lua 侧的 64 位字面量能完整取回` | `index.json` | 0.074s |
| `Multiple 多重绑定：同一个Lua脚本绑定到不同类` | `index.json` | 0.105s |
| `Num` | `index.json` | 0.083s |
| `OnCreated 与 OnDestroyed 各触发一次` | `index.json` | 0.090s |
| `Overridden 覆写：同一个Lua脚本绑定到不同类` | `index.json` | 0.066s |
| `Override 蓝图事件覆写示例截图` | `index.json` | 18.707s |
| `PUSHINT 推入的值能在脚本里参与算术` | `index.json` | 0.069s |
| `PUSHINT64 推入的值被认作整数且能完整读回` | `index.json` | 0.095s |
| `Partial 返回部分Out参数，剩余部分使用传入参数填充` | `index.json` | 0.051s |
| `Priority 绑定冲突：静态绑定优先于动态绑定` | `index.json` | 0.064s |
| `RemoveManualObjectReference 后可以被 GC` | `index.json` | 0.174s |
| `Reset 清零` | `index.json` | 0.073s |
| `RestoreOverrides 之后不再是覆写状态` | `index.json` | 0.098s |
| `Set` | `index.json` | 0.094s |
| `Set` | `index.json` | 0.130s |
| `Static 静态绑定，继承UnLuaInterface::GetModuleName指定脚本路径` | `index.json` | 0.072s |
| `Suspend 后走原实现，Resume 后又走 Lua` | `index.json` | 0.096s |
| `UCLASS 的声明包含它的名字` | `index.json` | 0.125s |
| `UMG UMG 与 Lua 生命周期示例截图` | `index.json` | 25.383s |
| `UNLUANOVA_IS_YIELDABLE 区分主线程与协程` | `index.json` | 0.073s |
| `UNLUANOVA_LOAD 能加载并执行 chunk` | `index.json` | 0.081s |
| `UNLUANOVA_RESUME 能恢复协程并带回结果` | `index.json` | 0.063s |
| `UnLuaNovaIsIntegralNumber 区分整数与非整数` | `index.json` | 0.077s |
| `VM hook 能安装、收到事件并按 owner 撤回` | `index.json` | 0.086s |
| `attach / detach` | `index.json` | 0.076s |
| `collect 返回 0，与 plain Lua 一致` | `index.json` | 0.078s |
| `core 也能单独 require 到` | `index.json` | 0.098s |
| `count 返回 KBytes 数字` | `index.json` | 0.067s |
| `dbg 状态查询是幂等的` | `index.json` | 0.122s |
| `debug 提供 upvalue / local / registry 访问` | `index.json` | 0.072s |
| `decode 能解析出字段` | `index.json` | 0.133s |
| `encode/decode 往返一致` | `index.json` | 0.118s |
| `gc 之后 Lua 值仍然可用` | `index.json` | 0.083s |
| `gc 执行后环境仍可用` | `index.json` | 0.115s |
| `gettime 返回递增的数字，不依赖网络` | `index.json` | 0.113s |
| `getupvalue 能读到闭包捕获的值` | `index.json` | 0.067s |
| `load 返回 nil 与错误信息，而不是抛出` | `index.json` | 0.100s |
| `luaL_newmetatable 建的元表带 __name` | `index.json` | 0.097s |
| `luaL_newmetatable 第二次调用返回 0` | `index.json` | 0.077s |
| `luaL_ref / luaL_unref 往返一致` | `index.json` | 0.079s |
| `memstats 各子命令都不崩` | `index.json` | 0.109s |
| `opt-out 的类型不会出现在输出里` | `index.json` | 5.024s |
| `package 同时提供 loaded 和 searchers` | `index.json` | 0.083s |
| `pcall 捕获后错误不逃逸，后续调用照常` | `index.json` | 0.087s |
| `profiler 开关是可重复的` | `index.json` | 0.121s |
| `protoc 加载内联 proto 后编解码一致` | `index.json` | 0.154s |
| `require 不存在的模块报错，而不是返回 nil` | `index.json` | 0.074s |
| `require 也能拿到模块表` | `index.json` | 0.082s |
| `require 拿到 socket 模块` | `index.json` | 0.124s |
| `require 拿到可用的 pb 模块` | `index.json` | 0.151s |
| `require 拿到带 decode/encode 的模块表` | `index.json` | 0.128s |
| `sandbox 是一个可写的 CVar` | `index.json` | 0.134s |
| `setupvalue 能给 chunk 设环境` | `index.json` | 0.072s |
| `trim 是 strip 的别名` | `index.json` | 0.083s |
| `一段错误脚本不会让环境失效` | `index.json` | 0.125s |
| `不匹配时为假` | `index.json` | 0.075s |
| `不同类得到不同的声明` | `index.json` | 0.108s |
| `不含机器绝对路径` | `index.json` | 0.096s |
| `不增加重复的元素` | `index.json` | 0.112s |
| `不存在的枚举值返回nil` | `index.json` | 0.090s |
| `不存在的枚举值返回nil` | `index.json` | 0.087s |
| `不相等` | `index.json` | 0.101s |
| `两个后端都存在` | `index.json` | 0.083s |
| `两个后端都能从字符串加载 chunk` | `index.json` | 0.075s |
| `两个定位器类都能切换并还原` | `index.json` | 0.104s |
| `两个对象的覆写互不影响` | `index.json` | 0.102s |
| `两个环境互不影响` | `index.json` | 0.092s |
| `两点相乘` | `index.json` | 0.084s |
| `两点相减` | `index.json` | 0.101s |
| `两点相加` | `index.json` | 0.097s |
| `两点相除` | `index.json` | 0.096s |
| `为空时 Start 什么都不跑` | `index.json` | 0.111s |
| `交换两个索引位置的元素` | `index.json` | 0.071s |
| `使用[]获取` | `index.json` | 0.080s |
| `使用[]赋值` | `index.json` | 0.070s |
| `元素不存在，新增` | `index.json` | 0.112s |
| `元素不存在，返回false` | `index.json` | 0.107s |
| `元素存在，移除并返回true` | `index.json` | 0.117s |
| `关闭后不再记账` | `index.json` | 0.078s |
| `内建 loader 能提供模块` | `index.json` | 0.088s |
| `函数解析到真实的 UFunction 并填好 flag` | `index.json` | 0.073s |
| `分别指定Pitch/Yaw/Roll` | `index.json` | 0.080s |
| `分别指定X/Y` | `index.json` | 0.089s |
| `分别指定X/Y/Z` | `index.json` | 0.089s |
| `分别指定X/Y/Z` | `index.json` | 0.101s |
| `分别指定X/Y/Z/W` | `index.json` | 0.081s |
| `分别指定X和Y` | `index.json` | 0.099s |
| `分发条件：非网络 / 非 latent / 非接口函数会走预解析路径` | `index.json` | 0.087s |
| `分隔符按字面量处理，不是模式` | `index.json` | 0.078s |
| `创建Actor，仅指定Actor类型` | `index.json` | 0.159s |
| `创建Actor，指定初始Transform` | `index.json` | 0.129s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner` | `index.json` | 0.164s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator` | `index.json` | 0.134s |
| `创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator/LuaModule` | `index.json` | 0.151s |
| `创建Actor，指定初始Transform/CollisionHandling` | `index.json` | 0.149s |
| `创建Actor，自定义构造参数` | `index.json` | 0.146s |
| `删除元素后迭代不会带回空洞` | `index.json` | 0.123s |
| `判断对象是否为指定类型` | `index.json` | 0.151s |
| `前缀` | `index.json` | 0.080s |
| `加载一个对象` | `index.json` | 0.180s |
| `单步能停下来并抓到调用栈` | `index.json` | 0.431s |
| `原生后端下可以往返 FVector` | `index.json` | 0.077s |
| `原生：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.098s |
| `原生：调用的函数本身没有返回值，依次返回参数` | `index.json` | 0.106s |
| `去掉首尾空白` | `index.json` | 0.078s |
| `参数类型错误是受控错误` | `index.json` | 0.083s |
| `取反向量` | `index.json` | 0.092s |
| `取反向量` | `index.json` | 0.098s |
| `取反向量` | `index.json` | 0.092s |
| `取反向量` | `index.json` | 0.087s |
| `取消注册后查不到` | `index.json` | 0.066s |
| `取相反数` | `index.json` | 0.092s |
| `只能够对空数组进行预留空间操作` | `index.json` | 0.084s |
| `只被 Lua 引用的对象会被引擎 GC 回收` | `index.json` | 0.166s |
| `只追加不重复的元素` | `index.json` | 0.081s |
| `可以 require 到模块表` | `index.json` | 0.071s |
| `可以 require 到模块表` | `index.json` | 0.074s |
| `合法位置索引返回true` | `index.json` | 0.079s |
| `同时指定Pitch/Yaw/Roll` | `index.json` | 0.122s |
| `同时指定X/Y` | `index.json` | 0.091s |
| `同时指定X/Y/Z` | `index.json` | 0.091s |
| `同时指定X/Y/Z` | `index.json` | 0.100s |
| `同时指定X和Y` | `index.json` | 0.073s |
| `后缀` | `index.json` | 0.078s |
| `向量乘` | `index.json` | 0.095s |
| `向量乘以数字` | `index.json` | 0.089s |
| `向量乘以整数` | `index.json` | 0.101s |
| `向量乘以浮点数` | `index.json` | 0.096s |
| `向量相减` | `index.json` | 0.088s |
| `向量相减` | `index.json` | 0.094s |
| `向量相减` | `index.json` | 0.069s |
| `向量相减` | `index.json` | 0.084s |
| `向量相加` | `index.json` | 0.098s |
| `向量相加` | `index.json` | 0.095s |
| `向量相加` | `index.json` | 0.081s |
| `向量相加` | `index.json` | 0.079s |
| `向量除` | `index.json` | 0.100s |
| `向量除以数字` | `index.json` | 0.085s |
| `向量除以整数` | `index.json` | 0.082s |
| `向量除以浮点数` | `index.json` | 0.100s |
| `启动时已填充，且至少有一个函数带默认值` | `index.json` | 0.088s |
| `启动模块不存在时不会被记成已加载` | `index.json` | 0.098s |
| `启动脚本能取到多个参数` | `index.json` | 0.102s |
| `四元数乘以浮点数` | `index.json` | 0.095s |
| `在主线程里调用 latent 函数会报出契约里的那条消息` | `index.json` | 0.069s |
| `声明包含真实的 UFUNCTION 与 UPROPERTY` | `index.json` | 0.141s |
| `多个环境之间的全局变量互不可见` | `index.json` | 0.087s |
| `大于等于容差，归一化` | `index.json` | 0.082s |
| `大于等于容差，归一化` | `index.json` | 0.078s |
| `大于等于容差，归一化并返回true` | `index.json` | 0.087s |
| `对 nil 引擎对象调用方法是受控错误` | `index.json` | 0.082s |
| `对象被标记销毁后，从 Lua 访问是受控错误` | `index.json` | 0.152s |
| `将Map内容转为LuaTable` | `index.json` | 0.101s |
| `将Set内容转为LuaTable` | `index.json` | 0.103s |
| `将当前Transform与目标Transform混合` | `index.json` | 0.122s |
| `将当前Transform与目标两个Transform混合` | `index.json` | 0.137s |
| `将数组内容转为LuaTable` | `index.json` | 0.070s |
| `小于容差，不归一化` | `index.json` | 0.090s |
| `小于容差，不归一化` | `index.json` | 0.092s |
| `小于容差，直接返回false` | `index.json` | 0.078s |
| `属性解析到真实的 FProperty` | `index.json` | 0.074s |
| `常见 UProperty 映射到对应的 Kind` | `index.json` | 0.066s |
| `广播事件` | `index.json` | 0.086s |
| `开关可切换， detach 之后关闭` | `index.json` | 0.080s |
| `开启后统计分配次数与字节数` | `index.json` | 0.079s |
| `开启后脚本仍能正常执行` | `index.json` | 0.080s |
| `执行失败返回false` | `index.json` | 0.105s |
| `执行委托` | `index.json` | 0.110s |
| `执行委托：从Lua返回值` | `index.json` | 0.109s |
| `执行委托：赋值` | `index.json` | 0.106s |
| `执行成功返回true` | `index.json` | 0.096s |
| `执行的脚本真的生效` | `index.json` | 0.137s |
| `扩容，使用默认元素填充` | `index.json` | 0.068s |
| `找到元素，返回索引位置` | `index.json` | 0.092s |
| `把调用耗时写入 Saved/Benchmark` | `index.json` | 0.171s |
| `报错之后环境仍然可用` | `index.json` | 0.065s |
| `拷贝指定索引位置的元素` | `index.json` | 0.085s |
| `指定ColorValue` | `index.json` | 0.097s |
| `指定RGB` | `index.json` | 0.099s |
| `指定RGB` | `index.json` | 0.107s |
| `指定RGBA` | `index.json` | 0.097s |
| `指定RGBA` | `index.json` | 0.122s |
| `指定Rotation(FQuat)` | `index.json` | 0.126s |
| `指定Rotation(FQuat)/Translation` | `index.json` | 0.108s |
| `指定Rotation(FQuat)/Translation/Scale` | `index.json` | 0.102s |
| `指定Rotation(FRotator)` | `index.json` | 0.108s |
| `指定Rotation(FRotator)/Translation` | `index.json` | 0.112s |
| `指定Rotation(FRotator)/Translation/Scale` | `index.json` | 0.105s |
| `指定Translation` | `index.json` | 0.100s |
| `指定X` | `index.json` | 0.080s |
| `指定X/Y` | `index.json` | 0.080s |
| `指定X/Y/Z` | `index.json` | 0.069s |
| `指定X/Y/Z/W` | `index.json` | 0.073s |
| `指定轴与角度` | `index.json` | 0.098s |
| `按引用获取元素（原生）` | `index.json` | 0.084s |
| `按引用获取元素（原生）` | `index.json` | 0.091s |
| `按引用获取元素（蓝图）` | `index.json` | 0.096s |
| `按引用获取元素（蓝图）` | `index.json` | 0.091s |
| `换成自定义子类后用的是子类的结果` | `index.json` | 0.083s |
| `插入元素到指定位置` | `index.json` | 0.081s |
| `搜索路径命中 Content/Script 下的真实模块` | `index.json` | 0.096s |
| `支持启动脚本和参数` | `index.json` | 0.113s |
| `支持多个Lua环境` | `index.json` | 0.091s |
| `支持多参数传入和传出` | `index.json` | 0.128s |
| `数组不包含指定元素返回false` | `index.json` | 0.095s |
| `数组包含指定元素返回true` | `index.json` | 0.097s |
| `文件系统优先于内建 loader` | `index.json` | 0.095s |
| `断点登记` | `index.json` | 0.082s |
| `显式传参会覆盖默认值` | `index.json` | 0.080s |
| `有 buffer 的后端下 TArray 往返保持内容` | `index.json` | 0.076s |
| `有静态绑定时出参与返回值都正确` | `index.json` | 0.088s |
| `有静态绑定时，Lua 调用结果正确` | `index.json` | 0.088s |
| `未知选项报错而不是静默` | `index.json` | 0.090s |
| `构造TArray<FString>` | `index.json` | 0.098s |
| `构造TArray<FVector>` | `index.json` | 0.113s |
| `构造TArray<UStruct>` | `index.json` | 0.096s |
| `构造TArray<bool>` | `index.json` | 0.098s |
| `构造TArray<int32>` | `index.json` | 0.113s |
| `构造TMap<FString,FString>` | `index.json` | 0.086s |
| `构造TMap<FVector,bool>` | `index.json` | 0.095s |
| `构造TMap<UStruct,UStruct>` | `index.json` | 0.102s |
| `构造TMap<int32,int32>` | `index.json` | 0.095s |
| `构造TSet<FString>` | `index.json` | 0.112s |
| `构造TSet<FVector>` | `index.json` | 0.112s |
| `构造TSet<UScriptStruct>` | `index.json` | 0.118s |
| `构造TSet<int32>` | `index.json` | 0.114s |
| `枚举默认值也生效` | `index.json` | 0.088s |
| `查找Key不存在，返回nil` | `index.json` | 0.100s |
| `查找Key不存在，返回nil` | `index.json` | 0.073s |
| `查找Key存在，返回Value的引用` | `index.json` | 0.078s |
| `查找Key存在，返回Value的拷贝` | `index.json` | 0.082s |
| `查找元素不存在，返回false` | `index.json` | 0.131s |
| `查找元素存在，返回true` | `index.json` | 0.121s |
| `栈帧能取到源码行，Profiler 键稳定且唯一` | `index.json` | 0.097s |
| `正确传入FName到Lua堆栈` | `index.json` | 0.110s |
| `正确传入FString到Lua堆栈` | `index.json` | 0.094s |
| `正确传入UObject到Lua堆栈` | `index.json` | 0.092s |
| `正确传入bool到Lua堆栈` | `index.json` | 0.085s |
| `正确传入const char*到Lua堆栈` | `index.json` | 0.088s |
| `正确传入double到Lua堆栈` | `index.json` | 0.099s |
| `正确传入float到Lua堆栈` | `index.json` | 0.085s |
| `正确传入int16到Lua堆栈` | `index.json` | 0.119s |
| `正确传入int32到Lua堆栈` | `index.json` | 0.104s |
| `正确传入int64到Lua堆栈` | `index.json` | 0.101s |
| `正确传入int8到Lua堆栈` | `index.json` | 0.117s |
| `正确传入struct到Lua堆栈` | `index.json` | 0.102s |
| `正确传入uint16到Lua堆栈` | `index.json` | 0.107s |
| `正确传入uint32到Lua堆栈` | `index.json` | 0.095s |
| `正确传入uint64到Lua堆栈` | `index.json` | 0.097s |
| `正确传入uint8到Lua堆栈` | `index.json` | 0.098s |
| `正确传入void*到Lua堆栈` | `index.json` | 0.093s |
| `正确传入中文FString到Lua堆栈` | `index.json` | 0.096s |
| `正确判断子类` | `index.json` | 0.114s |
| `正确加载蓝图类` | `index.json` | 0.117s |
| `正确获取类默认对象` | `index.json` | 0.108s |
| `没有分隔符时返回整串` | `index.json` | 0.096s |
| `没有原生 vector 时 ToVector 降级为 nil` | `index.json` | 0.079s |
| `没有实现接口的对象得到空串` | `index.json` | 0.075s |
| `没有找到元素，返回0` | `index.json` | 0.092s |
| `没有静态绑定时，同一个调用的结果一致` | `index.json` | 0.082s |
| `注册后能按 USTRUCT 和名字查到同一个表` | `index.json` | 0.063s |
| `添加绑定` | `index.json` | 0.097s |
| `添加绑定：UFunction` | `index.json` | 0.092s |
| `添加绑定：赋值` | `index.json` | 0.092s |
| `清理所有的绑定` | `index.json` | 0.107s |
| `清除数组，移除所有元素` | `index.json` | 0.097s |
| `清除，移除所有元素` | `index.json` | 0.123s |
| `清除，移除所有键值对` | `index.json` | 0.094s |
| `环境启动后即可直接用` | `index.json` | 0.092s |
| `生成的是真实内容，而不只是创建了一个文件` | `index.json` | 5.159s |
| `生成静态绑定表到 -OutputDir 指定的目录` | `index.json` | 5.191s |
| `用分隔符拼接并转字符串` | `index.json` | 0.114s |
| `相等` | `index.json` | 0.101s |
| `省略的 C++ 默认参数照样生效` | `index.json` | 0.095s |
| `移除指定元素` | `index.json` | 0.089s |
| `移除指定索引处的元素` | `index.json` | 0.081s |
| `移除绑定` | `index.json` | 0.097s |
| `移除绑定：UFunction` | `index.json` | 0.095s |
| `空串恒为真` | `index.json` | 0.074s |
| `空的子串返回 0 而不是死循环` | `index.json` | 0.104s |
| `空表得到空串` | `index.json` | 0.094s |
| `绑定` | `index.json` | 0.081s |
| `绑定后 Overridable 被换成了 ULuaFunction` | `index.json` | 0.090s |
| `绑定：UFunction` | `index.json` | 0.122s |
| `绑定：赋值` | `index.json` | 0.115s |
| `统计不重叠出现次数` | `index.json` | 0.097s |
| `编译期能力位与后端一致` | `index.json` | 0.071s |
| `编辑器模块可加载且命令上下文已注册` | `index.json` | 0.123s |
| `缩容，移除多余元素` | `index.json` | 0.075s |
| `缺少参数时给用法提示而不是崩` | `index.json` | 0.130s |
| `脚本依赖的标准库在两个后端都在` | `index.json` | 0.092s |
| `脚本改写 UE 命名空间被拦截` | `index.json` | 0.088s |
| `脚本根目录指向工程的 Content/Script` | `index.json` | 0.111s |
| `脚本能问到当前后端` | `index.json` | 0.080s |
| `自定义 loader 能提供文件系统里没有的模块` | `index.json` | 0.095s |
| `自定义子类把对象定位到指定环境` | `index.json` | 0.065s |
| `获取Map长度` | `index.json` | 0.084s |
| `获取Set长度` | `index.json` | 0.117s |
| `获取对象所在的World` | `index.json` | 0.166s |
| `获取对象的名称` | `index.json` | 0.163s |
| `获取对象的外部对象` | `index.json` | 0.145s |
| `获取对象的有效状态` | `index.json` | 0.307s |
| `获取对象的类型` | `index.json` | 0.152s |
| `获取平方后的长度` | `index.json` | 0.089s |
| `获取所有Key的数组` | `index.json` | 0.081s |
| `获取所有Value的数组` | `index.json` | 0.118s |
| `获取所有元素的数组` | `index.json` | 0.108s |
| `获取指定索引位置的元素引用` | `index.json` | 0.085s |
| `获取数据表中指定行的数据（C++）` | `index.json` | 0.126s |
| `获取数据表中指定行的数据（蓝图）` | `index.json` | 0.141s |
| `获取数组分配的内存指针` | `index.json` | 0.094s |
| `获取数组最后一个元素的索引位置` | `index.json` | 0.070s |
| `获取数组长度` | `index.json` | 0.094s |
| `获取枚举的名称` | `index.json` | 0.099s |
| `获取枚举的本地化文本` | `index.json` | 0.094s |
| `获取枚举类型的最大值` | `index.json` | 0.081s |
| `蓝图：调用的函数本身有返回值，先返回返回值再依次返回参数` | `index.json` | 0.096s |
| `蓝图：调用的函数本身没有返回值，依次返回参数` | `index.json` | 0.094s |
| `表里写了不存在的成员时跳过而不是崩` | `index.json` | 0.090s |
| `覆写内部报错不会崩，之后对象仍可调用` | `index.json` | 0.112s |
| `覆写后调用走 Lua 实现` | `index.json` | 0.105s |
| `解绑` | `index.json` | 0.107s |
| `设置Pitch` | `index.json` | 0.116s |
| `设置Pitch/Yaw` | `index.json` | 0.105s |
| `设置Pitch/Yaw/Roll` | `index.json` | 0.122s |
| `设置R` | `index.json` | 0.120s |
| `设置R` | `index.json` | 0.088s |
| `设置RG` | `index.json` | 0.090s |
| `设置RG` | `index.json` | 0.082s |
| `设置RGB` | `index.json` | 0.085s |
| `设置RGB` | `index.json` | 0.089s |
| `设置RGBA` | `index.json` | 0.105s |
| `设置RGBA` | `index.json` | 0.099s |
| `设置X` | `index.json` | 0.091s |
| `设置X` | `index.json` | 0.093s |
| `设置X` | `index.json` | 0.078s |
| `设置X` | `index.json` | 0.084s |
| `设置X` | `index.json` | 0.086s |
| `设置X` | `index.json` | 0.072s |
| `设置X/Y` | `index.json` | 0.093s |
| `设置X/Y` | `index.json` | 0.089s |
| `设置X/Y` | `index.json` | 0.089s |
| `设置X/Y` | `index.json` | 0.081s |
| `设置X/Y` | `index.json` | 0.088s |
| `设置X/Y/Z` | `index.json` | 0.097s |
| `设置X/Y/Z` | `index.json` | 0.097s |
| `设置X/Y/Z` | `index.json` | 0.077s |
| `设置X/Y/Z/W` | `index.json` | 0.083s |
| `设置X/Y/Z/W` | `index.json` | 0.087s |
| `设置XY` | `index.json` | 0.096s |
| `设置后 Start 会执行那个模块` | `index.json` | 0.105s |
| `设置指定索引位置的元素` | `index.json` | 0.070s |
| `设置防止无限循环的超时时间` | `index.json` | 1.150s |
| `设置项切换后可写回原值` | `index.json` | 0.073s |
| `访问反射枚举` | `index.json` | 0.131s |
| `访问反射类` | `index.json` | 0.097s |
| `访问导出类` | `index.json` | 0.106s |
| `语法错误返回 false，且之后仍可用` | `index.json` | 0.080s |
| `调用不存在的 UFunction 是受控错误` | `index.json` | 0.097s |
| `调用失败，返回值被标记为无效` | `index.json` | 0.102s |
| `调用失败，返回值被标记为无效` | `index.json` | 0.112s |
| `调用成功，返回正确的返回值` | `index.json` | 0.098s |
| `调用成功，返回正确的返回值` | `index.json` | 0.121s |
| `转为字符串` | `index.json` | 0.113s |
| `转为字符串` | `index.json` | 0.104s |
| `转为字符串` | `index.json` | 0.095s |
| `转为字符串` | `index.json` | 0.097s |
| `转为字符串` | `index.json` | 0.090s |
| `转为字符串` | `index.json` | 0.090s |
| `转为字符串` | `index.json` | 0.130s |
| `转为字符串` | `index.json` | 0.105s |
| `转为字符串` | `index.json` | 0.069s |
| `转为字符串` | `index.json` | 0.083s |
| `运行时模块都在插件的 Content/Script 下` | `index.json` | 0.078s |
| `运行时错误返回 false 且不污染栈顶` | `index.json` | 0.075s |
| `连续三次错误后栈顶不变` | `index.json` | 0.105s |
| `连续两次生成结果一致` | `index.json` | 9.475s |
| `迭代中删除元素不崩，且删除真的生效` | `index.json` | 0.075s |
| `迭代中删除元素不崩，且删除真的生效` | `index.json` | 0.118s |
| `迭代中删除当前键不崩，且删除真的生效` | `index.json` | 0.097s |
| `迭代中新增元素不崩，且新增真的生效` | `index.json` | 0.106s |
| `迭代中新增键不崩，且新增真的生效` | `index.json` | 0.101s |
| `迭代中追加元素不崩，且追加真的生效` | `index.json` | 0.108s |
| `迭代元方法注册在后端认识的名字下` | `index.json` | 0.072s |
| `迭代拿到Set里的每个元素` | `index.json` | 0.104s |
| `迭代获取Key与Value` | `index.json` | 0.092s |
| `迭代获取数组索引与元素` | `index.json` | 0.075s |
| `追加元素到数组末尾` | `index.json` | 0.094s |
| `追加另外一个数组的所有元素到数组末尾` | `index.json` | 0.093s |
| `释放对象在LuaVM的引用` | `index.json` | 0.140s |
| `销毁环境不会顺手销毁被引用的对象` | `index.json` | 0.155s |
| `销毁环境会跑完所有 __gc，且不破坏内存` | `index.json` | 0.093s |
| `错误信息带上 chunk 名与行号` | `index.json` | 0.076s |
| `随插件发布且带 ClassName 占位符` | `index.json` | 0.121s |
| `随机打乱数组元素位置` | `index.json` | 0.076s |
| `非整数读回为 0，而不是被截断` | `index.json` | 0.084s |
| `非法位置索引返回false` | `index.json` | 0.084s |
| `预留N个数组元素的空间` | `index.json` | 0.078s |
| `颜色相乘` | `index.json` | 0.086s |
| `颜色相减` | `index.json` | 0.105s |
| `颜色相加` | `index.json` | 0.111s |
| `颜色相加` | `index.json` | 0.076s |
| `颜色相除` | `index.json` | 0.080s |
| `默认参数` | `index.json` | 0.104s |
| `默认参数` | `index.json` | 0.084s |
| `默认参数` | `index.json` | 0.083s |
| `默认参数` | `index.json` | 0.093s |
| `默认参数` | `index.json` | 0.090s |
| `默认参数` | `index.json` | 0.105s |
| `默认参数` | `index.json` | 0.094s |
| `默认参数` | `index.json` | 0.091s |
| `默认参数` | `index.json` | 0.095s |
| `默认参数` | `index.json` | 0.069s |
| `默认定位器对实现了接口的对象返回 GetModuleName 的结果` | `index.json` | 0.071s |
| `默认环境定位器返回的是非空环境` | `index.json` | 0.078s |
| `默认逗号分隔` | `index.json` | 0.076s |

## 失败明细

### `require 入口在两个后端下都安全返回`

- 来源：`index.json`
- 耗时：0.409s
- 2 reported
- LogUnLuaNova: Lua error message: [string "Content/Script/Luau/Index.l..."]:20: syntax error near 'Index'
stack traceback:
	[string "Plugins/UnLuaNova/Content/S..."]:130: in upvalue 'load'
	[string "Plugins/UnLuaNova/Content/S..."]:163: in function 'UnLuaNova.HotReload.require'
	[C]: in global 'pcall'
	[string "chunk"]:1: in main chunk
- Expected 'require('Luau.Index') 返回与后端一致的结果' to be true.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaBackend.spec.cpp:873]

## 最慢的用例

| 用例 | 耗时 |
|---|---|
| `UMG UMG 与 Lua 生命周期示例截图` | 25.383s |
| `Binding 静态绑定示例截图` | 19.787s |
| `Override 蓝图事件覆写示例截图` | 18.707s |
| `Container 容器访问示例截图` | 18.400s |
| `连续两次生成结果一致` | 9.475s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | 6.715s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | 6.625s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | 6.443s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | 5.733s |
| `生成静态绑定表到 -OutputDir 指定的目录` | 5.191s |

## 整场录屏

不是 showcase 的剪辑——剪辑是用例自己录的，只覆盖 showcase 那几个测试；跑挂了的那一轮往往一个剪辑都没有。这一段是 `Tools/RunTests.ps1` 在启动编辑器的同时开始录、编辑器退出时结束，从第一条测试到最后一条都在里面（窗口出现前的几秒除外）。

<video controls muted playsinline width="640" src="Images/tests-lua/session.mp4"></video>

[打不开播放器的话：直接下载这个 mp4（标准 H.264）](Images/tests-lua/session.mp4)

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

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_Binding_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Container_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Container_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_Container_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Override_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Override_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_Override_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_UMG_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_UMG_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_UMG_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

## 静态类型检查（LuaLS）

| 指标 | 数值 |
|---|---|
| 检查文件数 | 88 |
| 模式 | `Warning` |
| 错误 | 0 |
| 警告 | 700 |

**没有类型错误 ✅**（声明文件由编辑器「导出智能提示」生成。）

## 性能环比

| 用例 | 本次（µs/次） | 上次（µs/次） | 变化 |
|---|---|---|---|
| Lua | 2.2216 | 1.8641 | +19.2% |
| Native | 0.0001 | — | 无上次数据 |
| Reflection | 0.6137 | 0.8480 | -27.6% |

> 超过 ±30% 会标 ⚠️，但**不会**让这轮失败：计时受机器负载影响，这里只做提示，不做判定。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Backend Lua -Publish`（要两份就 `-Backend Both`）。


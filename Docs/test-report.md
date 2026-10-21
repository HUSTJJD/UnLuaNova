---
title: "测试报告"
description: "自动化测试产出的通过率、失败明细与截图"
---

# 测试报告

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-21 11:28:59。请勿手工编辑。

## 结论

**存在失败 ❌（49 项）** — 通过率 86.3%

| 指标 | 数值 |
|---|---|
| 用例总数 | 357 |
| 通过 | 308 |
| 失败 | 49 |
| 跳过 | 0 |
| 总耗时 | 32.29s |
| 截图 | 0 |

## 失败明细

### `支持多参数传入和传出`

- 来源：`API.json`
- 耗时：0.140s
- 1 reported
- Expected 'RetValues[1].GetType()' to be 3, but it was 4.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:201]

### `执行失败返回false`

- 来源：`API.json`
- 耗时：0.036s
- 1 reported
- LogUnLuaNova: Lua error message: [string "invalid chunk"]:1: Incomplete statement: expected assignment or a function call


### `原生：调用的函数本身有返回值，先返回返回值再依次返回参数`

- 来源：`API.json`
- 耗时：0.040s
- 4 reported
- LogUnLuaNova: Lua error message: [string "..."]:5: attempt to index nil with 'TestForBaseSpec2'

- Ret: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:356]
- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:362]
- Expected 'C' to be 31, but it was 30.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `原生：调用的函数本身没有返回值，依次返回参数`

- 来源：`API.json`
- 耗时：0.039s
- 3 reported
- LogUnLuaNova: Lua error message: [string "..."]:6: attempt to index nil with 'TestForBaseSpec1'

- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:317]
- Expected 'D' to be "TestFlag", but it was "Test".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:319]

### `蓝图：调用的函数本身有返回值，先返回返回值再依次返回参数`

- 来源：`API.json`
- 耗时：0.038s
- 4 reported
- LogUnLuaNova: Lua error message: [string "..."]:5: attempt to index nil with 'TestForBaseSpec2'

- Ret: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:291]
- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:297]
- Expected 'C' to be 31, but it was 30.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `蓝图：调用的函数本身没有返回值，依次返回参数`

- 来源：`API.json`
- 耗时：0.039s
- 3 reported
- LogUnLuaNova: Lua error message: [string "..."]:6: attempt to index nil with 'TestForBaseSpec1'

- Expected 'B' to be 21, but it was 20.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:252]
- Expected 'D' to be "TestFlag", but it was "Test".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaBase.spec.cpp:254]

### `Dynamic 动态绑定，SpawnActor时指定脚本路径`

- 来源：`API.json`
- 耗时：0.045s
- 4 reported
- LogUnLuaNova: Lua error message: module 'Tests.Binding.BP_UnLuaTestActor' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "..."]:2

- LogUnLuaNova: Lua error message: module 'Tests.Binding.BP_UnLuaTestActor' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "..."]:4

- LogUnLuaNova: Lua error message: [string "..."]:2: attempt to call missing method 'RunTest' of userdata

- Expected 'Error' to be "", but it was "[string "..."]:2: attempt to call missing method 'RunTest' of userdata".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:99]
- ⚠ 2 reported
- ⚠ LogUnLuaNova: Failed to attach Tests.Binding.BP_UnLuaTestActor module for object Default__BP_UnLuaTestActor_DynamicBinding_C,0000019026110600!
invalid return value of require()
- ⚠ LogUnLuaNova: Failed to attach Tests.Binding.BP_UnLuaTestActor module for object BP_UnLuaTestActor_DynamicBinding_C_0,0000019026110000!
invalid return value of require()

### `Multiple 多重绑定：同一个Lua脚本绑定到不同类`

- 来源：`API.json`
- 耗时：0.029s
- 3 reported
- LogUnLuaNova: Lua error message: invalid actor class

- LogUnLuaNova: Lua error message: [string "..."]:2: attempt to index nil with 'RunTest'

- Expected 'Error' to be "", but it was "[string "..."]:2: attempt to index nil with 'RunTest'".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:152]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Binding/BP_UnLuaNovaTestActor_StaticBindingChild.BP_UnLuaTestActor_StaticBindingChild_C'

### `Overridden 覆写：同一个Lua脚本绑定到不同类`

- 来源：`API.json`
- 耗时：0.029s
- 2 reported
- LogUnLuaNova: Lua error message: invalid actor class

- Expected 'Actual' to be "BP ABC Lua", but it was "invalid actor class".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:174]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Binding/BP_UnLuaNovaTestActor_StaticBindingChild.BP_UnLuaTestActor_StaticBindingChild_C'

### `Priority 绑定冲突：静态绑定优先于动态绑定`

- 来源：`API.json`
- 耗时：0.042s
- 4 reported
- LogUnLuaNova: Lua error message: module 'Tests.Binding.BP_UnLuaTestActor' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "..."]:2

- LogUnLuaNova: Lua error message: module 'Tests.Binding.BP_UnLuaTestActor' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "..."]:4

- LogUnLuaNova: Lua error message: [string "..."]:2: attempt to call missing method 'RunTest' of userdata

- Expected 'Error' to be "", but it was "[string "..."]:2: attempt to call missing method 'RunTest' of userdata".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:127]
- ⚠ 2 reported
- ⚠ LogUnLuaNova: Failed to attach Tests.Binding.BP_UnLuaTestActor module for object Default__BP_UnLuaTestActor_StaticBinding_C,000001902611BA00!
invalid return value of require()
- ⚠ LogUnLuaNova: Failed to attach Tests.Binding.BP_UnLuaTestActor module for object BP_UnLuaTestActor_StaticBinding_C_0,000001902611B400!
invalid return value of require()

### `Static 静态绑定，继承UnLuaInterface::GetModuleName指定脚本路径`

- 来源：`API.json`
- 耗时：0.032s
- 3 reported
- LogUnLuaNova: Lua error message: module 'Tests.Binding.BP_UnLuaTestActor' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "..."]:3

- LogUnLuaNova: Lua error message: [string "..."]:2: attempt to call missing method 'RunTest' of userdata

- Expected 'Error' to be "", but it was "[string "..."]:2: attempt to call missing method 'RunTest' of userdata".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\BindingTest.cpp:71]
- ⚠ 1 reported
- ⚠ LogUnLuaNova: Failed to attach Tests.Binding.BP_UnLuaTestActor module for object BP_UnLuaTestActor_StaticBinding_C_0,000001902611DE00!
invalid return value of require()

### `获取数据表中指定行的数据（C++）`

- 来源：`API.json`
- 耗时：0.037s
- 2 reported
- LogDataTable: Missing RowStruct while emptying DataTable '/UnLuaNovaTestSuite/Tests/Misc/DataTable_CppTest.DataTable_CppTest', NeedLoad: 'false'!
- Expected 'lua_tostring(L, -1)' to be "Hello", but it was "".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_DataTable.spec.cpp:58]
- ⚠ 1 reported
- ⚠ LogDataTable: Missing RowStruct while loading DataTable '/UnLuaNovaTestSuite/Tests/Misc/DataTable_CppTest.DataTable_CppTest', NeedLoad: 'false'!

### `获取枚举类型的最大值`

- 来源：`API.json`
- 耗时：0.046s
- 1 reported
- Expected 'Actual' to be 3, but it was 0.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `断点原语可用`

- 来源：`API.json`
- 耗时：0.027s
- 1 reported
- Expected '断点设置与清除都被接受' to be true.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaBackend.spec.cpp:396]

### `按引用获取元素（原生）`

- 来源：`API.json`
- 耗时：0.028s
- 2 reported
- Expected 'Result1' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_Array.spec.cpp:698]
- Expected 'Result2' to be 2, but it was 0.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `按引用获取元素（蓝图）`

- 来源：`API.json`
- 耗时：0.028s
- 2 reported
- Expected 'Result1' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_Array.spec.cpp:725]
- Expected 'Result2' to be 2, but it was 0.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `按引用获取元素（原生）`

- 来源：`API.json`
- 耗时：0.028s
- 2 reported
- Expected 'Result1' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_Map.spec.cpp:359]
- Expected 'Result2' to be 2, but it was 0.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `按引用获取元素（蓝图）`

- 来源：`API.json`
- 耗时：0.027s
- 2 reported
- Expected 'Result1' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_Map.spec.cpp:386]
- Expected 'Result2' to be 2, but it was 0.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `将Map内容转为LuaTable`

- 来源：`API.json`
- 耗时：0.029s
- 2 reported
- Expected 'UNLUANOVA_TOINT64(L, -1)' to be 4, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_Map.spec.cpp:305]
- Expected 'UNLUANOVA_TOINT64(L, -2)' to be 3, but it was 0.  [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]

### `访问反射枚举`

- 来源：`API.json`
- 耗时：0.030s
- 1 reported
- Result2: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UELib.spec.cpp:67]

### `创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator`

- 来源：`API.json`
- 耗时：0.045s
- 2 reported
- Expected 'Transform.GetTranslation()' to be X=1.000 Y=1.000 Z=1.000, but it was X=98.451 Y=120.737 Z=1.000 and outside tolerance 0.000100.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_World.spec.cpp:143]
- Actor->SpawnCollisionHandlingMethod: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\LuaLib_World.spec.cpp:144]

### `Get`

- 来源：`Binding.json`
- 耗时：0.127s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:80]

### `Set`

- 来源：`Binding.json`
- 耗时：0.028s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:93]

### `Get`

- 来源：`Binding.json`
- 耗时：0.027s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:52]

### `Set`

- 来源：`Binding.json`
- 耗时：0.029s
- 1 reported
- Expected: The two values are not equal.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\UnLuaNovaEx.spec.cpp:65]

### `gettime 返回递增的数字，不依赖网络`

- 来源：`Extensions.json`
- 耗时：0.027s
- 1 reported
- LogUnLuaNova: module 'string' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:97 function load_error_handler
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "/Plugins/UnLuaNovaExtensions/LuaSocket/Content/Script/socket.lua"]:10
[string "Plugins/UnLu ...

### `require 拿到 socket 模块`

- 来源：`Extensions.json`
- 耗时：0.028s
- 1 reported
- LogUnLuaNova: module 'string' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:97 function load_error_handler
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "/Plugins/UnLuaNovaExtensions/LuaSocket/Content/Script/socket.lua"]:10
[string "Plugins/UnLu ...

### `Issue286 蓝图 TMap FindRef 错误`

- 来源：`Regression.json`
- 耗时：0.552s
- 1 reported
- Expected 'Result' to be "passed", but it was "".  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue286Test.cpp:27]

### `Issue288 UMG里Image用到的Texture内存泄漏`

- 来源：`Regression.json`
- 耗时：0.113s
- 2 reported
- LogUnLuaNova: Global_NewObject: Invalid class!
[string "..."]:3

- LogUnLuaNova: Lua error message: [string "..."]:2: attempt to index nil with 'Release'

- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue288/UnLuaNovaTestUMG_Issue288.UnLuaNovaTestUMG_Issue288_C'

### `Issue291 delegate绑定本身不在lua里有引用的object，在绑定后触发gc会导致无法调用绑定的函数`

- 来源：`Regression.json`
- 耗时：0.032s
- 1 reported
- LogUnLuaNova: Lua error message: [string "            local Flag = false            local function test(x, y, z)                Flag = true            end            local ActorClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue291/BP_UnLuaNovaTestActor_Issue291.BP_Un..."]:1: attempt to call a nil value


### `Issue294 AutoCreateRefTerm标记的参数需要生成默认值`

- 来源：`Regression.json`
- 耗时：0.029s
- 1 reported
- Expected 'G_Value1' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue294Test.cpp:54]

### `Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃`

- 来源：`Regression.json`
- 耗时：0.936s
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

- 来源：`Regression.json`
- 耗时：0.432s
- 2 reported
- Expected 'Result1' to be 10, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue300Test.cpp:27]
- Expected 'Result2' to be 10, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue300Test.cpp:27]

### `Issue314 访问过父类不存在的字段，后续在子类中也只能取到nil`

- 来源：`Regression.json`
- 耗时：0.039s
- 2 reported
- LogUnLuaNova: Lua error message: invalid actor class

- Expected 'Result' to be true.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue314Test.cpp:52]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Child.BP_UnLuaTestActor_Issue314_Child_C'

### `Issue362 TArray引用做委托参数在lua中修改不生效`

- 来源：`Regression.json`
- 耗时：0.030s
- 3 reported
- LogUnLuaNova: Global_NewObject: Invalid class!
[string "..."]:2

- LogUnLuaNova: Lua error message: [string "..."]:3: attempt to index nil with 'Issue362Delegate'

- Expected 'Actual' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue362Test.cpp:47]

### `Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留`

- 来源：`Regression.json`
- 耗时：0.526s
- 3 reported
- LogUnLuaNova: Lua error message: module 'Tests.Binding.BP_UnLuaTestActor' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "..."]:2

- LogUnLuaNova: Lua error message: module 'Tests.Binding.BP_UnLuaTestActor' not found
[string "Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua"]:174 function require
[string "..."]:3

- Expected 'Actual' to be 7, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue372Test.cpp:27]
- ⚠ 2 reported
- ⚠ LogUnLuaNova: Failed to attach Tests.Binding.BP_UnLuaTestActor module for object Default__BP_UnLuaTestActor_StaticBinding_C,0000023B1AFD6600!
invalid return value of require()
- ⚠ LogUnLuaNova: Failed to attach Tests.Binding.BP_UnLuaTestActor module for object BP_UnLuaTestActor_StaticBinding_C_0,0000023B1AFD5400!
invalid return value of require()

### `Issue376 引用参数传递失败`

- 来源：`Regression.json`
- 耗时：0.026s
- 1 reported
- Expected 'Actual' to be 100, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue376Test.cpp:40]

### `Issue398 接口列表从lua传到c++/蓝图层会丢失一半信息`

- 来源：`Regression.json`
- 耗时：0.030s
- 2 reported
- LogUnLuaNova: Lua error message: invalid actor class

- Expected 'Result' to be 4, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue398Test.cpp:60]
- ⚠ 1 reported
- ⚠ LogUObjectGlobals: Failed to find object 'Class /UnLuaNovaTestSuite/Tests/Regression/Issue398/BP_Character.BP_Character_C'

### `Issue401 LUA覆写导致数组传参错误`

- 来源：`Regression.json`
- 耗时：0.451s
- 2 reported
- Expected 'Result1' to be 2, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue401Test.cpp:27]
- Expected 'Result2' to be 2, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue401Test.cpp:27]

### `Issue407 无法在lua中将一个TMap调用Keys的结果传给C++`

- 来源：`Regression.json`
- 耗时：0.011s
- 1 reported
- Expected 'Result' to be 2, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue407Test.cpp:43]

### `Issue434 MulticastDelegate创建Handler时和self绑定`

- 来源：`Regression.json`
- 耗时：0.990s
- 1 reported
- Expected 'Result1' to be true.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue434Test.cpp:28]

### `Issue443 在某些代码块内无法调用蓝图函数`

- 来源：`Regression.json`
- 耗时：1.463s
- 1 reported
- Expected 'Result' to be true.  [<UE>\Engine\Source\Runtime\Core\Public\Templates\Function.h:298]

### `Issue453 从Lua按引用传递到蓝图的TArray引用变成了空Array`

- 来源：`Regression.json`
- 耗时：0.465s
- 4 reported
- Expected 'Result1_BP' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]
- Expected 'Result1_Lua' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]
- Expected 'Result2_BP' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]
- Expected 'Result2_Lua' to be 3, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue453Test.cpp:27]

### `Issue477 遍历蓝图结构体的DataTable会发生拷贝错误`

- 来源：`Regression.json`
- 耗时：0.235s
- 2 reported
- LogUnLuaNova: Lua error message: [string "..."]:10: attempt to perform arithmetic (add) on number and integer

- Expected 'Result' to be 6, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue477Test.cpp:51]

### `Issue583 在Lua中遍历TMap字段的Values接口返回值时引起的崩溃`

- 来源：`Regression.json`
- 耗时：0.027s
- 1 reported
- LogDataTable: Missing RowStruct while emptying DataTable '/UnLuaNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583.DataTable_Issue583', NeedLoad: 'false'!
- ⚠ 1 reported
- ⚠ LogDataTable: Missing RowStruct while loading DataTable '/UnLuaNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583.DataTable_Issue583', NeedLoad: 'false'!

### `Issue585 蓝图枚举无法被UnLuaRef`

- 来源：`Regression.json`
- 耗时：0.119s
- 1 reported
- LogDataTable: Missing RowStruct while emptying DataTable '/UnLuaNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583.DataTable_Issue583', NeedLoad: 'false'!

### `Issue595 Lua无法调用Interface中函数`

- 来源：`Regression.json`
- 耗时：0.027s
- 1 reported
- Expected '1' to be 0, but it was 1.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\Issue595Test.cpp:44]

### `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃`

- 来源：`Regression.json`
- 耗时：1.470s
- 3 reported
- LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] In use pin  Issue 634Struct  no longer exists on node  Make <unknown struct> . Please refresh node or break links to remove pin.
- LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] No Struct in  Make <unknown struct>
- LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] Failed to create property NewParam from  <None>  due to a bad or unknown type (Structure)
- ⚠ 5 reported
- ⚠ LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] Input pin  Buff ID  specifying non-default value no longer exists on node  Make <unknown struct> . Please refresh node or reset pin to default value to remove pin.
- ⚠ LogBlueprint: [AssetLog] Plugins\UnLuaNovaTestSuite\Content\Tests\Regression\Issue634\BP_Issue634Actor.uasset: [Compiler] Input pin  Over Time  specifying non-default value no longer exists on node  Make <unknown struct> . Please refresh node or reset pin to default value to remove pin.

### `IssueOverrides 覆写引起的问题`

- 来源：`Regression.json`
- 耗时：0.977s
- 3 reported
- LogUnLuaNova: Lua error message: [string "Content/Script/Tests/Regression/IssueOverrides/IssueOverridesObject.lua"]:4: attempt to perform arithmetic (add) on integer and number

- Expected 'Result1' to be 2, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\IssueOverridesTest.cpp:32]
- Expected 'Result2' to be 1, but it was 0.  [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Tests\IssueOverridesTest.cpp:32]

## 最慢的用例

| 用例 | 耗时 |
|---|---|
| `Issue455 CacheScriptContainer导致的崩溃` | 2.637s |
| `Issue430 动画通知调用组件Lua覆写函数崩溃` | 1.622s |
| `Issue603 在Lua监听的事件中再次触发事件会导致崩溃` | 1.499s |
| `Issue471 FScriptDelegate解绑无效` | 1.484s |
| `Issue276 游戏世界暂停后，coroutine delay 功能不工作` | 1.473s |
| `Issue634 覆写非ReturnValue的C++结构体返回值会导致崩溃` | 1.470s |
| `Issue443 在某些代码块内无法调用蓝图函数` | 1.463s |
| `Issue660 Unlua函数绑定多个代理导致崩溃` | 1.462s |
| `设置防止无限循环的超时时间` | 1.033s |
| `Issue446 实现了FTickableGameObject的对象在Tick里调用自身被Lua覆写的方法会崩溃` | 0.992s |

## 截图

没有截图。截图需要真实视口，跑的时候加 `-Screenshots`（脚本会关掉 `-NullRHI` 并把 `Saved/Screenshots` 收进报告目录）。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Publish`。


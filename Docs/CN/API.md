---
title: "API 参考"
description: "UnLuaNova 的 C++ API、Lua API 与构建期宏定义速查"
---

# UnLuaNova API

## C++ API

### 全局变量
* `UnLuaNova::GLuaSrcRelativePath` Lua源码文件相对于Content目录的相对路径

* `UnLuaNova::GLuaSrcFullPath` Lua源码文件所在的绝对路径

* `UnLuaNova::FLuaEnv` 一个独立的 Lua 运行环境（多环境下每个环境各自一个实例）；主环境通过
  `IUnLuaNovaModule::Get().GetEnv()` 获取

### 全局函数
命名空间：UnLuaNova
* `Call` 调用某个全局 `function`

* `CallTableFunc` 调用全局 `table` 里的某个 `function`

* `Get` 从Lua堆栈的某个索引位置上获取指定类型的数据

* `Push` / PushXXX 向Lua堆栈中压入一个指定类型的数据

* `IsType` 判断Lua堆栈上指定索引位置上的对象类型

* `CreateState` 创建一个 `lua_State`（单环境时代的遗留入口，新代码请用 `FLuaEnv`）

* `GetState` 获取主环境的 `lua_State`，等价于 `FLuaEnv::GetMainState()`

* `Startup` 启动UnLuaNova环境

* `Shutdown` 停止UnLuaNova环境

* `LoadFile` 从指定相对路径加载一个Lua文件，但不运行

* `RunFile` 从指定相对路径运行一个Lua文件

* `LoadChunk` 加载一个Lua Chunk，但不运行

* `RunChunk` 运行指定的Lua Chunk

* `ReportLuaCallError` 报告Lua错误，如果注册了自定义错误处理委托则会仅执行委托，否则使用UnLuaNova默认的 `UE_LOG` 输出

* `GetStackVariables` 获取当前堆栈上所有相关的变量、`upvalue` 信息

* `GetLuaCallStack` 获取当前Lua调用栈的字符串

### 全局委托
FUnLuaNovaDelegates
* `OnLuaStateCreated` 当一个Lua虚拟机被创建出来时候触发，可以拿到其对应的 `lua_State`

* `OnLuaContextInitialized` 当 `FLuaEnv` 初始化完成后触发（**注意**：当前构建里没有广播点，绑定后不会被调用）

* `OnPreLuaContextCleanup` 当 `FLuaEnv` 开始回收清理前触发（**注意**：当前构建里没有广播点，绑定后不会被调用）

* `OnPostLuaContextCleanup` 当 `FLuaEnv` 完成回收清理后触发（**注意**：当前构建里没有广播点，绑定后不会被调用）

* `OnPreStaticallyExport` 当 `FLuaEnv` 开始向 `lua_State` 注册静态导出的类型之前触发

* `OnObjectBinded` 当某个 `UObject` 和Lua绑定时触发

* `OnObjectUnbinded` 当某个 `UObject` 的Lua绑定被解绑时触发

* `ReportLuaCallError` 支持注册自定义Lua报错处理委托到UnLuaNova，当Lua报错时会调用这个委托

* `ConfigureLuaGC` 支持注册自定义LuaGC配置委托到UnLuaNova，覆盖UnLuaNova默认传递给 `lua_gc` 的参数

* `LoadLuaFile` 支持注册自定义的Lua文件加载器委托到UnLuaNova，实现自定义Lua文件加载机制

### 接口
* `UUnLuaNovaInterface` UnLuaNova的核心接口，用来标记和识别需要绑定到Lua的对象

### 类
* `FLuaEnv` 封装了一个 UnLuaNova 运行环境（主 `lua_State` 及其注册表、模块、线程），多环境下每个环境各自一个实例

* `FLuaIndex` 对Lua堆栈索引的简单封装

* `FLuaTable` 对Lua侧 `table` 数据类型的封装，提供类似Lua中"`[]`"的访问机制，也支持调用指定 `function`

* `FLuaValue` 对Lua各种值类型的封装

* `FLuaFunction` 对Lua侧 `function` 类型的封装，提供获取全局或指定 `table` 内的 `function` 并调用的能力

* `FLuaRetValues` 对从Lua侧的返回值的封装

* `FCheckStack` 用于辅助检测Lua堆栈平衡

* `FAutoStack` 用于自动恢复Lua堆栈索引

* `FExportedEnum` 提供向Lua侧注册导出枚举的接口

## Lua API

### 全局表
* `UE` 仅当 `WITH_UE4_NAMESPACE` 开关启用时存在，作为访问C++侧类的根对象。当开关禁用时，访问C++可以直接通过 `_G` 作为根对象来访问。类型是用到了才会注册到Lua，因此不必担心一次性加载过多导致的性能问题

* `UE.__newindex` `UE` 命名空间**只读**：引擎类型由绑定层用 `lua_rawset` 惰性注册进去，脚本
  往里写（比如 `UE.FVector = nil`）会直接报错，而不是静默覆盖掉别人的绑定。注册期除外——
  为了能让类型继续惰性注册，命名空间在注册完成前是可写的，之后才装上 `__newindex`

### 全局函数
* `RegisterEnum` 手动注册一个枚举到Lua

* `RegisterClass` 手动注册一个类到Lua

* `GetUProperty` 获取某个 `UObject` 上的 `UProperty`

* `SetUProperty` 设置某个 `UObject` 的 `UProperty` 为指定值

* `LoadObject` 加载一个 `UObject` ， 相当于：`UObject.Load("/Game/Core/Blueprints/AI/BehaviorTree_Enemy.BehaviorTree_Enemy")`

* `LoadClass` 加载一个 `UClass` ， 相当于： `UClass.Load("/Game/Core/Blueprints/AICharacter.AICharacter_C")`

* `NewObject` 根据指定的Class、Outer(可选)、Name(可选)， 创建一个 `UObject`

* `UEPrint` 是`UE_LOG` 的包装，也会使用 `UKismetSystemLibrary::PrintString` 输出

### 实例函数
* `Initialize` 当任意对象被绑定到Lua时，都会调用这个初始化函数

### 委托
* `Bind` 绑定一个回调到当前 `FScriptDelegate` 实例上。

* `Unbind` 从当前 `FScriptDelegate` 实例上解绑回调。

* `Execute` 手动执行 `FScriptDelegate` 所有回调。

### UnLuaNova.lua
一旦 `require "UnLuaNova"` 后，会产生如下全局函数：
* `Class` 创建一个"类"的table，模仿OOP的概念提供封装、继承的机制

* `print` 使用 `UEPrint` 覆盖Lua原生 `print`

### UnLuaNova 全局表（脚本侧）
`UnLuaNova` 既是 Lua 模块名（`require "UnLuaNova"`）也是运行期注入的全局表：

* `UnLuaNova.GetBackend()` 返回当前构建的 Lua 后端：`"Lua"` 或 `"Luau"`
* `UnLuaNova.HasFeature(Name)` 查询某项**特性**是否可用（大小写不敏感）。
  后端能力与配置开关走**同一个入口**，脚本不必知道这个值是从后端来的还是从 ini 来的：

  | 名字 | 含义 | 来源 |
  |---|---|---|
  | `sandbox` | 原生沙盒（`luaL_sandbox`） | 后端（Luau 有） |
  | `breakpoint` | VM 级断点/单步通道 | 后端（Luau 有） |
  | `allocCallbacks` | 分类内存回调（`lua.memstats` 的分类视图） | 后端（Luau 有） |
  | `vector` | 原生 `vector` 与 `FVector` 直接互换 | 后端（Luau 有） |
  | `buffer` | `buffer` 库 | 后端（Luau 有） |
  | `timer` | 内置库 `UnLuaNova.Timer` | 配置 `bTimer` |
  | `event` | 内置库 `UnLuaNova.Event` | 配置 `bEvent` |
  | `oop` | 内置库 `UnLuaNova.OOP` | 配置 `bOOP` |
  | `datadrivenbinding` | 数据驱动绑定 | 配置 `bDataDrivenBinding` |
  | `debugadapter` | `lua.dap` | 配置 `bDebugAdapter` |

  配置开关全在 `Config/DefaultUnLuaNova.ini` 的 `[/Script/UnLuaNova.UnLuaNovaSettings]`，
  键名就是 `b<CamelCase>`。详见[内置库](StdLib.md#插拔所有特性同一种开关格式)。

  未知名字返回 `false` 而不是报错，所以照着更新版本写的脚本在旧构建上是降级而不是崩。

按能力分支，而不是按后端名字分支：同一个后端的不同版本能力会变，名字不会。

### 字符串扩展
环境启动时自动把下列函数装到全局 `string` 表上，不需要 `require`
（实现见 `Plugins/UnLuaNova/Content/Script/UnLuaNova/StringEx.lua`）：

| 函数 | 说明 |
|---|---|
| `string.split(s, sep)` | 按**字面量**分隔（不是模式），`sep` 默认 `,` |
| `string.strip(s)` / `string.trim(s)` | 去掉首尾空白 |
| `string.startswith(s, prefix)` / `string.endswith(s, suffix)` | 前后缀判断，空串恒为真 |
| `string.count(s, sub)` | 统计不重叠出现次数 |

`join` 只在模块表里（`require("UnLuaNova.StringEx").join(sep, list)`），没有装到 `string` 上。

### 随插件发布的 Lua 模块

下面这些模块都在 `Plugins/UnLuaNova/Content/Script/UnLuaNova/` 下，**跟着插件走**，不是示例工程的
内容。它们通过 `UnLuaNova.<模块名>` / `Lua.<模块名>` 访问（`UnLuaNova` 是 `Lua` 的别名，
`__index` 会按需 `require`），也可以直接 `require("UnLuaNova.<模块名>")`：

| 模块 | 说明 |
|---|---|
| `StringEx` | 字符串扩展，环境启动时自动 `require`，会把函数装到 `string` 表上 |
| `Vector` | `vector` 与 `FVector` 互操作（`HasVector()` / `Create` / `ToFVector` / `FromFVector`） |
| `Buffer` | `buffer` 与 `TArray<uint8>` 互转（`HasBuffer()` / `FromArray` / `ToArray`） |
| `Input` | 输入绑定 |
| `EnhancedInput` | 增强输入绑定 |
| `HotReload` | 热重载：接管 `require`，使已加载模块可以被替换 |
| `Event` | 事件总线（快照广播、错误隔离、`UnbindOwner`），详见[内置库](StdLib.md#event纯-lua) |
| `OOP` | 脚本面向对象（单继承、构造链、`Super`），详见[内置库](StdLib.md#oop纯-lua) |
| `Timer` | 脚本计时器（**内建 native 模块**，走世界时间），详见[内置库](StdLib.md#timerc-驱动) |

!!! warning "别把这些模块放进工程目录"
    `package.path` **工程在前**（`Content/Script/?.lua`），插件脚本目录按插件实例解析在后。
    放在 `Content/Script/UnLuaNova/` 下的模块在本仓库能加载，
    但任何只装插件的工程都拿不到它 —— 而且本仓库不会有任何用例失败。
    `UnLuaNova.API.RuntimeModules` 这条用例就是断言"文件在插件目录里"，防止再犯。

!!! note "Timer 没有 .lua 文件"
    `Timer` 是内建模块，由 `FLuaEnv::AddBuiltInLoader` 提供加载器（`require` 的第四个 searcher），
    所以它在文件系统里找不到 —— 这是有意的：到期时间必须由 C++ 用世界时间算。

## 宏定义
以下宏定义可通过 `UnLuaNova.Build.cs` 修改：

* `UNLUANOVA_HAS_STDLIB_TIMER` / `UNLUANOVA_HAS_STDLIB_EVENT` / `UNLUANOVA_HAS_STDLIB_OOP`
内置库的**编译期**开关，由 `Config/DefaultUnLuaNova.ini` 的 `bTimer` / `bEvent` / `bOOP`
生成（与运行时设置同名同键，不是另起一套），默认全为 `1`。关掉后该库不注册、`HasFeature`
回答 false、Lua 库 require 时明确报错。详见[内置库](StdLib.md#插拔所有特性同一种开关格式)。
* `AUTO_UNLUA_STARTUP` 是否自动启动UnLuaNova环境，默认**启用**。

* `WITH_UE4_NAMESPACE` 是否提供一个统一的 `UE` 命名空间 `table` 作为对引擎侧类型的访问入口，默认**启用**。

* `ENABLE_TYPE_CHECK` 是否启用类型检测，帮助在 `UFunction` 调用时进行参数类型合法性检测，默认**启用**。检测到实参类型不合法时抛 Lua error（`pcall` 可捕获），而不是写入一个默认值后继续调用。

* `ENABLE_PERSISTENT_PARAM_BUFFER` 为每个 `UFunction` 复用一块参数缓存，默认**启用**。

* `ENABLE_CALL_OVERRIDDEN_FUNCTION` 是否支持在 Lua 里通过 `self.Overridden` 调用被覆写的原函数，默认**启用**。

* `UNLUANOVA_ENABLE_FTEXT` 是否以 C++ 对象方式传递 `FText`（否则按 `lua string`），默认**禁用**。

* `LUA_COMPILE_AS_CPP` 是否以 C++ 方式编译 Lua 源码，默认**禁用**。

* `UNLUANOVA_ENABLE_DEBUG` 是否输出UnLuaNova详细的调试日志，默认**禁用**。

* `UNLUANOVA_LUA_BACKEND` 由 `LuaBackend` 设置推导，**不要在代码里直接写**：两个后端导出同样的
  `lua_*` 符号，写死它等于让代码在另一个后端下编译不过。差异一律走 `LuaBackend.h` 的抽象。

---

## 延伸阅读

- [编程指南](UnLuaNova_Programming_Guide.md) — 绑定方式、双向调用与垃圾回收的核心概念
- [静态导出与静态绑定](StaticExportBinding.md) — 静态导出与构建期绑定代码生成
- [设置项](Settings.md) — 运行时与编辑器的全部设置项

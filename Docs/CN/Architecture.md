---
title: "架构"
description: "UnLuaNova 的目录结构、双后端抽象、绑定与覆写、静态绑定链路，以及新增类型与测试的写法"
---

# 架构

本文面向**要改 UnLuaNova 本身**的人：读完应该知道代码分成哪几块、一条 Lua 调用是怎么走到
引擎的、以及加一个新类型要碰哪些地方。只想用 UnLuaNova 写脚本的话，[编程指南](UnLuaNova_Programming_Guide.md)
更合适。

---

## 目录结构

```
Plugins/UnLuaNova/
├── Source/
│   ├── UnLuaNova/              运行时模块（游戏打包也带）
│   │   ├── Public/             27 个头文件 + StaticBinding/
│   │   ├── Private/
│   │   │   ├── ReflectionUtils/  UE 反射包装：ClassDesc / FunctionDesc / PropertyDesc / EnumDesc …
│   │   │   ├── Registries/       按类型分册的注册表：Class / Object / Function / Property / Enum / Delegate / Container
│   │   │   ├── BaseLib/          13 个 LuaLib_*：Array/Set/Map/Class/Object/Enum/Delegate/DataTable/Text/Key/File/World…
│   │   │   ├── MathLib/          10 个 LuaLib_F*（FVector/FQuat/FTransform…）+ 共用 LuaLib_Math.h
│   │   │   ├── Containers/       容器头（LuaArray/LuaMap/LuaSet）
│   │   │   └── StaticBinding/    静态绑定注册表的实现
│   │   └── UnLuaNova.Build.cs  后端选择在这里（读 Config/DefaultUnLuaNova.ini 的 LuaBackend）
│   ├── UnLuaNovaEditor/        编辑器模块：工具栏、IntelliSense、3 个 Commandlet、设置项
│   ├── UnLuaNovaDefaultParamCollector/  C# UBT 插件（非模块），收集 UFunction 的默认参数
│   └── ThirdParty/             Lua / Luau 两个 git 子模块 + 各自的 Build.cs 与生成脚本
├── UnLuaNovaExtensions/        LuaProtobuf / LuaRapidjson / LuaSocket，三个独立插件（可不带）
└── UnLuaNovaTestSuite/         测试：Private/Specs/（单元）+ Private/Tests/（回归、场景）
```

两个后端**都由 UBT 直接编译源码**，没有预构建步骤：Lua 走一个聚合的 `LuaUnity.c`，
Luau 走 `LuauLib/Generated/*.cpp`（每个 wrapper 只 include 一个 Luau 源文件，以避开 3 个
文件级 `static` 符号重名，所以 `bUseUnity = false` 是必需的）。

---

## 双后端抽象

`Public/LuaBackend.h` 是**唯一**吸收 VM 差异的地方。它自己的注释说得很直白：

> 除它以外的所有代码都必须走 `UNLUANOVA_*` 宏，不要直接调 VM；**只有这个文件**包含后端的头，
> 其他文件都不该 include `lua.h` / `lualib.h` / `lauxlib.h` / `luacode.h` —— 两个后端都占用这些名字。

**不要在功能代码里 `if (UNLUANOVA_LUA_BACKEND == ...)`。** 后端 id 只决定"编译哪个 VM"，
真正改变行为的是能力；所以分支要打在能力宏上，而不是后端 id 上。

| 能力宏 | Lua | Luau | 含义 |
|---|:---:|:---:|---|
| `UNLUANOVA_HAS_HOOK_EVENT` | ✅ | ❌ | `lua_Debug.event` 存在，Profiler 才能做 call/return 配对；Luau 改走 interrupt |
| `UNLUANOVA_HAS_NATIVE_VECTOR` | ❌ | ✅ | TValue 里直接放 3/4 个 float，不分配内存 |
| `UNLUANOVA_HAS_NATIVE_BUFFER` | ❌ | ✅ | Luau 的 buffer 类型 |
| `UNLUANOVA_HAS_READONLY_TABLES` | ❌ | ✅ | 只读表（`UE.<Type>` 写保护的底座） |
| `UNLUANOVA_HAS_SANDBOX` | ❌ | ✅ | `luaL_sandbox` / `luaL_sandboxthread` |
| `UNLUANOVA_HAS_BREAKPOINT` | ❌ | ✅ | 断点与单步回调 |
| `UNLUANOVA_HAS_ALLOC_CALLBACKS` | ❌ | ✅ | 带类型/标签的分配回调（内存统计靠它） |
| `UNLUANOVA_HAS_USERDATA_GC` | ✅ | ❌ | **Luau 根本没有 `__gc`**，dtor 尚未接线 |
| `UNLUANOVA_HAS_ITER_METAMETHOD` | ❌ | ✅ | Luau 用 `__iter`（Lua 用 `__pairs`），经 `UNLUANOVA_ITER_ENTRY` 注册 |

其余是"两边都有、但名字或签名不同"的适配宏：`UNLUANOVA_NEWSTATE`、`UNLUANOVA_LOAD`、
`UNLUANOVA_NEWDATA`、`UNLUANOVA_UDATA_LEN`、`UNLUANOVA_SETUP_GC`、`UNLUANOVA_RESUME`、
`UNLUANOVA_IS_YIELDABLE`、`UNLUANOVA_PUSHINT64/TOINT64` 等。

头文件里还有三道编译期护栏：两个后端同时被启用 → `#error`；实际编进来的 VM 与所选后端不符 →
`#error`；拿错头（`LUA_VECTOR_SIZE`、`sizeof(lua_Integer)` 不对）→ `#error` / `static_assert`。
这是"后端差异泄漏到别处"的最后一道防线。

---

## 绑定与覆写

### 绑定：把一个 Lua 模块挂到 UObject / UClass

1. `FLuaEnv::TryBind(UObject*)`（`Private/LuaEnv.cpp`）是入口；
2. `UUnLuaNovaManager::Bind()`（`Private/UnLuaNovaManager.cpp`）`require` 该模块，然后
   `BindClass()` 把模块表按类拷一份、记录 `BindInfo{Class, ModuleName, TableRef}`，
   用 `LowLevel::GetFunctionNames` 列出 Lua 侧函数名、用 `ULuaFunction::GetOverridableFunctions`
   列出可覆写的 UFunction，两者对得上的就逐个 `Override`；
3. `FObjectRegistry::Bind()` 完成对象侧登记。

脚本访问 `self.SomeProperty` 时走 `LuaCore.cpp` 的 `GetField` → `GetFieldInternal`
（读 `__name`、注册 `FClassDesc`/`FFieldDesc` 并**把结果缓存进 metatable**，所以第二次访问不再查反射）。
反向（引擎调 Lua）是 `PushFunction` / `CallFunction`。

### 覆写：thunk 替换 + opcode 注入

`FLuaOverrides::Override(UFunction*, UClass*, FName)`（`Private/LuaOverrides.cpp`）：

1. 清掉源函数的 `FUNC_Native` 标记；
2. 把原函数**复制**进一个 `ULuaOverridesClass`（承载所有运行时创建的 `ULuaFunction`），
   原实现因此还能被 `self.Overridden.Xxx(self)` 调回去；
3. `ULuaFunction::Override()`（`Private/LuaFunction.cpp`）做真正的关键一步：把原函数的
   native thunk 换成 `execScriptCallLua`，并把 `Function->Script` 覆写成一个可识别的头
   `{EX_StringConst, 'L','U','A','\0', EX_UInt64Const}` + `ULuaFunction*` 指针。

最后这一步就是"opcode 注入"：`ULuaFunction::Get(UFunction*)` 靠读回这段字节码来判断某个
UFunction 有没有被 Lua 覆写。两个 thunk `execCallLua` / `execScriptCallLua` 最终都进
`FFunctionRegistry::Invoke`。

还原/挂起/恢复：`ULuaFunction::RestoreOverrides`（退出 PIE 时）、`SuspendOverrides`
（PIE 保存 UPackage 前，避免 `ULuaFunction` 被序列化进磁盘）、`ResumeOverrides`（保存完成后）。

### Latent 函数

`LuaCore.cpp` 里的 `Class_CallLatentFunction` 是给 `FFunctionDesc::IsLatentFunction()` 为真的
UFunction 安装的闭包。它取（或新建）协程，然后：

- **在主线程调用** → 明确报错（"不能在 coroutine 外调用 latent 函数"这类文案）；
- **从引擎 C 回调进来的线程** → 先查 `UNLUANOVA_IS_YIELDABLE(L)`，给出可读的错误，
  而不是让 Luau 抛出那句很难懂的 *attempt to yield across metamethod/C-call boundary*；
- 正常路径 → `Function->CallUE(L, NumParams, &ThreadRef)` 后 `lua_yield`。

---

## 垃圾回收

`Private/LuaGcScheduler.cpp`（头文件在 `Public/LuaGcScheduler.h`）持有每个环境的堆调度。
`FLuaEnv` 在构造完 VM、跑完 `UNLUANOVA_SETUP_GC` 之后建它，并在
`FCoreDelegates::OnEndFrame` 上每帧 `TickGC()` 一次。

两种模式：

* **Automatic**（默认，也是推荐值）：VM 自己调度。Lua 5.5 跑 `LUA_GCGEN` 分代、Luau 跑
  增量，`TickGC()` 只更新基线。
* **TimeSliced**：`LUA_GCSTOP` 停掉 VM 自己的节奏，改为每帧按预算 `LUA_GCSTEP`。

**为什么默认不是 TimeSliced**：这里本来没有周期性 GC 尖峰要修。全仓库对
`FLuaEnv::GC()` 的调用方只有 `lua.gc` 控制台命令一处，没有任何自动调用；而
`UNLUANOVA_SETUP_GC` 已经在跑分代 / 增量。切到 `TimeSliced` 等于拿一个调过的分代回收器
换一个手写的步进器 —— 只在"项目于已知安全点（过场、加载界面）主动调 GC 并希望它摊开"
时才划算。

债务的定义是这套东西里唯一需要想清楚的地方：

> 债务 = 距上一次**完整周期**以来堆增长了多少，而不是这一帧分配了多少。

Lua 在跑完一轮之前不释放任何东西，所以"先建 4 MB 再整个丢掉"的脚本，这一帧分配量是 0、
待回收的垃圾是 4 MB。按每帧分配量记账永远不会去收它 —— 这是第一版实现的 bug，也是这里
用"基线（上次完整周期后的存活集）"而不是"上一帧堆大小"的原因。

后端差异只有一处：`lua_gc(LUA_GCSTEP)` 的 `data`，Luau 读作 KB、Lua 5.5 读作"工作量"
（且不承诺字节精度）。连同堆大小读取一起收在 `LuaBackend.h` 的
`UNLUANOVA_GC_STEP_BYTES` / `UNLUANOVA_GC_HEAP_BYTES` 里。

**为什么不是多线程 GC**：Lua 堆是单个 `lua_State`，判断可达性要遍历持有（也被持有于）
UObject 的 userdata —— 只能在游戏线程。把清扫搬去工作线程意味着改 VM 的回收器并给每次
分配加锁；摊到若干帧，延迟收益一样，风险小得多。

---

## 静态绑定：生成 → 注册 → `CallUEFast`

| 环节 | 位置 |
|---|---|
| **1. 生成** | `UnLuaNovaEditor/Private/Commandlets/UnLuaNovaStaticBindingGeneratorCommandlet.cpp`。默认输出到 `Source/<Project>/Generated/UnLuaNovaStaticBinding_Generated.cpp`。**选择是 opt-out**：跳过 `SKEL_/REINST_/TRASHCLASS_/HOTRELOADED_/PLACEHOLDER-CLASS`、deprecated/transient，以及带 `meta=(UnLuaNovaNoExport)` 的类型；祖先上有 `meta=(UnLuaNovaExport)` 的会被强制纳入 |
| **2. 注册** | 生成文件里每个类型带一句 `UNLUANOVA_REGISTER_STATIC_BINDING(TypeName)`，它声明一个静态 `FStaticBindingAutoRegistrar`，构造时调 `FStaticBindingRegistry::Get().Register(Type)`。注册本身不碰 UObject，所以**在 DLL 加载期跑是安全的** |
| **3. 解析** | `Register()` 只入队，`ResolvePending()` 把 `FProperty`/`UFunction` 指针真正解析出来，由读路径**惰性触发**——这样解析一定发生在 UObject 系统就绪之后。读路径加了 `FRWLock` + `PendingCount` 原子快判，避免每次 `FindProp`/`FindFunc` 都跑一遍 |
| **4. 消费** | `FFunctionDesc` 构造时查 `FStaticBindingRegistry::Get().Find(Owner)` 再 `FindFunc(...)`，结果缓存在 `FFunctionDesc::StaticBinding` |
| **5. 分发** | `FFunctionDesc::CallUE`：命中静态绑定且**不是**网络函数、不是 latent、不是接口函数 → `CallUEFast`；其余继续走反射 |
| **6. 快路径** | `CallUEFast` 仍然复用 `PreCall`/`PostCall`/`Buffer`，只省掉"每次查函数"的部分：不查 callspace，直接 `Object->UObject::ProcessEvent`；仅当 `Flags.bOverridable` 时才做覆写重定向。**可观测行为与反射路径完全一致** |

被排除的三类各有原因：网络函数要 `GetFunctionCallspace`/`CallRemoteFunction`、latent 要维护协程、
接口函数要按对象 `FindFunctionByName`。

---

## 如何新增一个 LuaLib 类型

照 `Private/MathLib/LuaLib_FIntPoint.cpp` 抄一遍：

1. **新建** `Private/MathLib/LuaLib_FMyType.cpp`，include `"UnLuaNovaEx.h"` 与 `"LuaLib_Math.h"`。
2. **构造函数** `FMyType_New(lua_State*)`：用 `NewTypedUserdata(L, FMyType)` + placement new，
   按 `lua_gettop` 分派不同重载。
3. **`luaL_Reg` 表**：`Set`、`Add/Sub/Mul/Div`、`__add/__sub/__mul/__div`、`__unm`、
   `__tostring`（`TMathUtils<T>::ToString`）、`__call` → `_New`。
4. **导出块**：`BEGIN_EXPORT_REFLECTED_CLASS(FMyType)` … `ADD_FUNCTION(...)` …
   `ADD_LIB(FMyTypeLib)` … `END_EXPORT_CLASS()` … `IMPLEMENT_EXPORTED_CLASS(FMyType)`。
   `IMPLEMENT_EXPORTED_CLASS` 造出那个静态 helper 实例，它的构造函数把自己注册进
   `UnLuaNova::ExportClass`（`Binding.cpp`）。
5. **Traits**：在 `LuaLib_Math.h` 里加 `template<> struct TMathTypeTraits<FMyType>`
   （`FieldType` / `ScalarType` / `NUM_FIELDS`）。运算符不统一的再加 `TMul<FMyType, float>` 这类特化。
6. **注册时机**：反射类型在 `ClassRegistry.cpp` 里 `FindExportedReflectedClass(...)->Register(L)`，
   紧挨着 `UELib::SetTableForClass`；非反射类型在 `LuaEnv.cpp`。metatable 由
   `TExportedClassBase<bIsReflected>::Register`（`UnLuaNovaEx.inl`）创建。
7. **编译**：什么都不用改。`UnLuaNova.Build.cs` 不列源文件，UBT 会 glob `Private/**`。
8. **测试**：`UnLuaNovaTestSuite/Private/Specs/LuaLib_FMyType.spec.cpp`，注册名
   `UnLuaNova.API.FMyType`，照 `LuaLib_FIntPoint.spec.cpp` 写（`BeforeEach` 里
   `UnLuaNova::Startup(); L = UnLuaNova::GetState();`，然后 `Describe`/`It` 跑 `RunChunk`）。
   每个 `MathLib/LuaLib_F*.cpp` 都有一个同名 spec。

---

## 延伸阅读

- [测试与自动化](Testing.md) — 用例怎么写、怎么跑、报告怎么看
- [C++ API 参考](CxxApi.md) — 公开 C++ API 逐项签名
- [如何实现覆写](How_To_Implement_Overriding.md) — 覆写机制的分步讲解
- [静态导出绑定](StaticExportBinding.md) — 手写导出（上面的宏体系）
- [Luau 后端](Luau.md) — 两个后端的语义差异
- [设置项](Settings.md) — 后端、沙盒、看门狗等开关

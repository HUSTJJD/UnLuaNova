---
title: "性能对标 slua：它为什么快，我们怎么超过"
description: "拆解 slua-unreal 的 7 个性能来源，逐条给出 UnLuaNova 的落地实现与验收口径"
---

# 性能对标 slua：它为什么快，我们怎么超过

这篇只回答一件事：**slua-unreal 的每一次跨语言调用到底省掉了什么，那几样东西在
UnLuaNova 里的对应位置在哪、现在省掉了没有。**

先给结论，再给证据：

* slua 的性能优势**不是"它写了更好的 C++"，是"它把编组从运行时搬到了编译期"**。
  三条绑定路径里只有反射那条在运行时做类型判断，CppBinding 与静态代码生成把
  "这个参数是什么类型"变成了模板实参，于是编组退化成几条 store。
  它 README 里的 0.541 s（反射）vs 0.090 s（CppBinding）是这唯一的差别造成的 ——
  **同一台机器、同一百万次调用，差 6 倍。**
* UnLuaNova 之前的处境比"没有 CppBinding"更糟：我们**连反射那条路的编组层都比
  slua 厚**。slua 的反射 0.54 µs ≈ `ProcessEvent` 裸开销，说明它的编组层几乎不额外
  收费；我们自己的数据是 `ProcessEvent` 0.459 µs、从 Lua 调 UE 函数 1.439 µs，
  **差 0.98 µs 全在我们的编组与校验层**。
* 本次把 slua 有的全部落地，并在三处做得比它更保守/更强（见[第四节](#四超越slua-没有的三处)）。
  核心是给静态绑定**生成真正的 thunk**，而不再只是生成描述表 —— 这正是
  [对标文档](Comparison_sluaunreal.md#g5--静态绑定生成-thunk回应-144-µs)里标记为"待做"的 G5。

> ⚠️ 数字口径：slua 的表是"10 万次调用耗时（秒）"，测于 Win10 / Ryzen 7 4700G / UE 4；
> 我们的数据来自 `UnLuaNova.Perf.BenchmarkParity` 与 `UnLuaNova.Perf.Benchmark`。
> **两台机器、两个引擎版本，不能直接比绝对值。** 本文所有"快 N 倍"都指**同一路径
> 优化前后的倍率**，或明确标注为对方自己的对照数据。

---

## 一、slua 快的七个来源

### 1. 编组在编译期定型（CppBinding / 静态代码生成）—— 唯一的量级差

slua 提供三条绑定：蓝图反射、**libclang 静态代码生成**、**CppBinding 模板展开**。
它自己说后两者"运行原理上没差别，只是一个自动生成、一个手写"。差别全在编译期：

```cpp
// CppBinding 的编组（示意）：类型是模板实参
int32 Arg0 = LuaObject::checkValue<int32>(L, 2);
FVector Arg1 = LuaObject::checkValue<FVector>(L, 3);
```

| 路径 | 100 万次空函数调用 | 相对反射 |
|:---|---:|---:|
| 蓝图反射 | 0.541507 s | 1.0x |
| CppBinding | 0.090571 s | **6.0x** |
| 传 FString 返回 int（反射） | 0.930042 s | — |
| 传 FString 返回 int（CppBinding） | 0.223207 s | **4.2x** |

**省掉的是**：`FProperty` 的虚调用、`UFunction` 参数帧的逐参数初始化、
按名字查找参数、运行时类型判断。剩下的只有 `ProcessEvent`。

### 2. 函数索引缓存 —— 2.0 相对 1.x 的"10 倍"

slua 2.0 的 release note：*"函数索引相比 1.x 版本有 10 倍速度提升"*。
对应它 README 表格里的 `TestIndex*` 组：

| 用例 | slua | UnLua | 倍率 |
|:---|---:|---:|---:|
| `TestIndexStaticCall` | 0.00125 | 0.00535 | 4.28 |
| `TestIndexEmptyCall` | 0.00126 | 0.00559 | 4.44 |
| `TestIndexGetBoolCall` | 0.00142 | 0.00572 | 4.03 |

本质：把"名字 → 可调用对象"的解析结果**写回 metatable**，之后一次 `rawget` 拿到。
UnLuaNova **本来就有这个机制**（`LuaCore.cpp` 的 `GetFieldInternal` 会把结果
`lua_rawset` 回 metatable），所以这一项我们不是差距 —— 值得记下来的是：
**它 1.x 没有，我们是带着这个优势出生的，没有理由在别处丢掉。**

### 3. 结构体引用式访问

slua 2.0 把 struct 从拷贝改成引用：

```lua
-- 老代码（读-改-写，两次编组）
local Position = Actor.Position
Position.X = 1
Actor.Position = Position
-- 新代码
Actor.Position.X = 1
```

UnLuaNova **同样有**：`FPropertyDesc` 的 `bCreateCopy=false` 走 `PushPointer`，
在弱表 `StructMap` 里按 lightuserdata 命中即零拷贝、零分配。平手。

### 4. 覆写路径缓存 —— 表上最大的那个 29 倍

| 用例 | slua | UnLua | 倍率 |
|:---|---:|---:|---:|
| `TestOverrideReceiveBeginPlay` | 0.00135 | 0.03956 | **29.30** |
| `TestOverrideTestFunc` | 0.01313 | 0.09633 | 7.34 |

`ReceiveBeginPlay` 是**无参事件**。UnLua 为它走了完整的 `FFrame` 参数解包
（`Stack.Step` 循环 + `FOutParmRec` 链构造），slua 对空签名直接派发。
**这是"参数帧准备"这一项被放大到极端的样子** —— 无参调用的合理成本应该接近
一次 `lua_pcall`，29 倍说明 UnLua 在无参时付了有参的钱。

### 5. 反射路径本身更薄

`TestEmptyCall` slua 0.0183 / UnLua 0.0335（1.83x）、
`TestGetIntCall` 0.02085 / 0.04239（2.03x）。同为反射，差 2 倍。
对照 slua 反射 0.54 µs ≈ `ProcessEvent` 裸开销，可以断定：**它的反射路径里没有
"每次调用都做的额外校验"**，而 UnLua 系有（`CheckObject`、`CheckPropertyType`、
`GetFunctionCallspace`）。

### 6. 属性访问不重复校验

`TestGetIntVar` 0.01396 / 0.01553、`TestGetVectorVar` 0.01109 / 0.02358（2.13x）、
`TestGetStructVar` 0.01085 / 0.02408（2.22x）。注意 **Get 比 Set 慢得多**
（GetVector 2.13x vs SetVector 0.91x）—— 说明差距集中在"读出来"这一步的
类型转换与分配上，而不是"写进去"。

### 7. GC 多线程化（影响帧率，不影响单次调用）

slua 声称支持多线程 GC。这一项在[对标文档](Comparison_sluaunreal.md#g3--gc-卡顿前提不成立本节记录为什么)
里已经论证过：**我们的前提不成立**（`FLuaEnv::GC()` 全仓库只有一个调用方，
Lua 5.5 跑分代、Luau 跑增量，本来就没有周期性尖峰要摊）。不吸收。

---

## 二、我们慢在哪：逐项开销清单

下面每一项都来自本仓库的实际代码，标了位置。这是本次动手的依据。

| # | 开销 | 位置 | 每次调用/访问发生 |
|:--|:---|:---|:--|
| 1 | `FPropertyDesc` 两层虚调用：`ReadValue_InContainer` → `GetValueInternal` | `ReflectionUtils/PropertyDesc.h:149` | 每次属性读写 |
| 2 | `TWeakFieldPtr::IsValid()` —— 弱句柄解析，不是指针判空 | `PropertyDesc.h:151,189` | 每次属性读写（**两遍**） |
| 3 | `LowLevel::CheckPropertyOwner` → `UObject::IsA()` 走类链 | `Public/LowLevel.cpp`（`ENABLE_TYPE_CHECK=1` 默认开） | 每次属性读写 |
| 4 | `FFunctionDesc::CheckObject` → `IsA` + `ULuaFunction::Get` | `ReflectionUtils/FunctionDesc.cpp:677` | 每次函数调用（WITH_EDITOR） |
| 5 | `PreCall` 逐参数 `InitializeValue` + `CheckPropertyType` 虚调用 | `FunctionDesc.cpp:387,415` | 每次调用 × 参数个数 |
| 6 | `Object->GetFunctionCallspace()` | `FunctionDesc.cpp:228` | 每次调用（非静态绑定时） |
| 7 | `FClassRegistry::Find(const char*)` 构造 `FName`（UTF8→TCHAR + 哈希） | `Registries/ClassRegistry.cpp:50` | 每次 push UObject |
| 8 | `PushObjectCore` 重建 metatable 名：`FString::Printf` 或 `GetPathName()` | `Private/LuaCore.cpp:467` | 每次读对象型属性 |
| 9 | 对象型属性每次新建 userdata（`PushObjectCore` 绕过 `ObjectMap` 缓存） | `LuaCore.cpp:551` | 每次读对象型属性 |
| 10 | `FStaticPropBinding` / `FStaticFuncBinding` 已生成，**属性侧完全没被消费** | 全仓对 StaticBinding 的消费点只有 `FunctionDesc.cpp` | 永久 |

第 10 项是最荒的：`Offset`、`EStaticPropKind` 早就生成好了，运行时没人读。

---

## 三、落地

### S1 — 静态绑定生成 thunk（对应 slua 的 CppBinding，G5 的核心）

**做法**：`FStaticFuncBinding` 新增 `lua_CFunction Thunk` 字段。
Commandlet 为签名可静态编组的函数生成模板实例，metatable 直接挂这个 C 函数，
**整个 `FFunctionDesc` / `FPropertyDesc` 链路不进入**。

```cpp
// 生成产物（示意）
{ TEXT("K2_SetActorLocation"), 0, MAX_uint16, NumFStaticParamBinding_Actor_...,
  FStaticFuncFlags(), FStaticParamBinding_..., nullptr,
  EStaticPropKind::Bool, nullptr,
  &UnLuaNova::StaticFuncThunk<EStaticPropKind::Bool, EStaticPropKind::Float, EStaticPropKind::Bool> },
```

关键设计：**类型是编译期常量，偏移是运行时读的。**

```cpp
template <EStaticPropKind Ret, EStaticPropKind... Args>
int32 StaticFuncThunk(lua_State* L);
```

slua 把偏移也烘进生成代码，快一点点，但**插一个 UPROPERTY 而没重新生成就会静默错位**。
我们的偏移来自 `FStaticPropBinding::Offset`（注册期从 `FProperty::GetOffset_ForGC()`
填的），永远不会陈旧；代价是每个参数一次间接取址 —— 换来的是"生成文件过期也安全"。

三道回退，任何一道触发都退回原路径：

1. **生成期** `CollectThunkKinds()` 只放行：非 static / 非 net / 非接口 / 非 latent /
   无 out 参数 / **无 C++ 默认实参**（`CPP_Default_*` 元数据）/ `ParmsSize ≤ 256` /
   参数 ≤ 8 个 / 所有参数的 kind 都在 `IsThunkableParamKind()` 里。

   默认实参那一条值得单独说：反射路径在脚本省略实参时会用收集到的默认值填槽，
   而 thunk 拿到的是清零的帧。与其"快一点但 `obj:Foo()` 的含义悄悄变了"，
   不如让这种函数留在反射路径上。
2. **注册期** `FStaticBindingRegistry::Resolve` 用真实 `FProperty` 复算 kind，
   与生成值不符就置空 `Thunk`（签名变了）；某个参数没解析到同样置空（避免两个
   未解析槽位都写到 offset 0）。
3. **运行期** `Func->Thunk` 为空则 `TryPushStaticBindingField` 返回 false，
   `FFunctionDesc` 照旧接管，并且仍然走 `CallUEFast`。

**验收** —— Commandlet 结尾打印 thunk 覆盖率：

```
[StaticBindingGen] thunk coverage: 8123/11044 functions (73.6%)
```

### S2 — 属性访问吃静态绑定

新增 `FStaticPropDesc`（`Private/ReflectionUtils/StaticPropDesc.{h,cpp}`）。
它**继承 `FPropertyDesc`**，所以 `Class_Index` / `Class_NewIndex` / 容器路径
一行都不用改，不处理的 kind 直接退回基类实现 —— 不可能悄悄改变行为。

省掉的是第二节的第 1、2 项：读变成"加偏移 + 按类型 push"，
`IsValid()` 从"switch + 若干 `IsUObjectValid`"降为一次 `PropertyPtr.IsValid()`
（活着的 `FProperty` 蕴含它的 `PropertyClass`/`Struct`/`SignatureFunction` 活着）。

落点：`GetFieldInternal` 在 `ClassDesc->RegisterField()` **之前**先查静态绑定，
命中就压入并写回 metatable 缓存 —— 于是连 `FFieldDesc` 都不建。

### S3 — 热路径去分配与去重复校验

| 改动 | 位置 | 省掉 |
|:---|:---|:---|
| metatable 命中时直接从 metatable 取 `ClassDesc` lightuserdata | `ClassRegistry.cpp:PushMetatable` | 第 7 项：每次构造 `FName` |
| `CheckPropertyOwner` 的 `IsA` 加 (class, property) memo | `LowLevel.cpp` | 第 3 项：类链遍历 |
| 对象型属性走 `PushUObject`（`ObjectMap` 身份缓存） | `LuaCore.cpp:PushObjectElement` / `PushInterfaceElement` | 第 8、9 项；顺带修好"同一个对象读两次不是同一个 Lua 值" |

memo 用 `thread_local` 而非 `static`：`IsA` 的结果是 (对象类, 属性) 的纯函数，
但多 Env 下 `static` 的撕裂读会让没验过的组合被判成 true。

`PushObjectElement` 对**已释放**对象保留原行为（仍然 `PushObjectCore`），
因为那是一条错误路径，不该在这次改动里顺手换掉。

### S4 — 尚未落地的

* `FFunctionDesc::CheckObject` 的 editor-only `IsA` + `ULuaFunction::Get`（第 4 项）：
  同样可以 memo，但它是 `WITH_EDITOR` 分支，benchmark 若跑 editor 会放大，
  需要先确认测量环境再动。
* `PreCall` 按 kind 批量直写（第 5 项）：静态绑定已有 `FStaticFuncBinding::Params`，
  可以再做一条 `CallUEStatic`，但**有 thunk 的类型已经不走这条路了**，
  收益面只剩"没跑 Commandlet 的项目"。
* 覆写空签名直接派发（对应 slua 的 29 倍）：`FunctionDesc::CallLua` 里
  `bUnpackParams` 对 `NumParms == 0` 短路。

---

## 四、超越：slua 没有的三处

对标不是为了追平。这三处是它给不了、我们能给的：

1. **thunk 覆盖属性，不只函数。** slua 的 CppBinding 是手写注册函数，
   现实项目里没有人会把几千个 UPROPERTY 手写一遍 —— 它的加速面天然只覆盖
   "被挑中的热点函数"。我们的 thunk 由 Commandlet 从反射数据批量生成，
   **属性与函数同批覆盖**，而且是 opt-out（`meta=(UnLuaNovaNoExport)`），
   默认全量。
2. **生成物过期是安全的。** 如上：偏移运行时读、kind 注册期复算、不符就退回。
   slua 的静态代码生成是 libclang 前向解析 + 烘死偏移，头文件改了没重新生成就是
   错位崩溃。我们不需要维护 clang 版本与 UE 宏展开（[对标文档](Comparison_sluaunreal.md#四明确不吸收的三项)
   里这条本来就在"不吸收"清单上）。
3. **Luau 后端的原生 vector。** `FPropertyDesc` 对 `FVector` 有直写快路径
   （Luau 的 `LUA_TVECTOR` 三个分量直接落到 `Dest->X/Y/Z`，不建 userdata）。
   slua 只有 Lua 5.4 一个后端，没有这层。

再加上本来就领先的部分（见[对标文档](Comparison_sluaunreal.md#二能力矩阵)）：
双语言后端、Luau 静态类型检查、沙盒、热重载、518 条自动化用例、UE 5.8。

---

## 五、验收口径

改完之后要拿到的不是"感觉快了"，是下面三个数：

1. **同机复测**：跑 `UnLuaNova.Perf.BenchmarkParity`（10 万次迭代，用例名与
   slua `TestPerf` 对齐），产出 `Saved/Benchmark/parity-latest-<Backend>.csv`。
   对比基准是同一次运行中 `Ref.TestEmptyCall` / `Ref.TestGetIntCall` 两行
   `ProcessEvent` 裸开销 —— **目标是"从 Lua 调 UE 函数"向裸开销收敛**，
   而不是向 slua 的绝对值收敛。
2. **thunk 覆盖率**：Commandlet 输出的 `thunk coverage: X/Y`。
   低于 60% 说明生成器的资格判定太严，值得回来放宽（先把 out 参数这一类做掉）。
3. **属性侧**：`TestGetIntVar` / `TestGetVectorVar` / `TestGetStrVar` 优化前后对比。
   这几项在 slua 的表里正好是 UnLua 输得最惨的（2.03x / 2.13x / 2.36x），
   也是我们这轮改动打中的地方。

**收益默认就是拿到的**：`bStaticBinding` 默认 `True`，产物由编辑器启动自动维护
（见[静态导出绑定](StaticExportBinding.md#默认就开着)），不用任何人记着去敲命令。
唯一遗留的人工动作是**重编译一次** —— 产物是 C++，这是编译期绑定的固有代价。

---

## 延伸阅读

- [对标 slua-unreal：差距与吸收](Comparison_sluaunreal.md) — 能力矩阵与 G1~G7
- [架构](Architecture.md) — 双后端抽象、覆写与静态绑定链路
- [静态导出绑定](StaticExportBinding.md) — Commandlet 的用法与 opt-out
- [控制台命令](ConsoleCommand.md) — `lua.profiler` / `lua.memstats`
- slua-unreal：<https://github.com/Tencent/sluaunreal>

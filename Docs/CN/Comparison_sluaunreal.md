---
title: "对标 slua-unreal：差距与吸收"
description: "逐项对照 Tencent/sluaunreal 的能力与实现路径，判断哪些该吸收、怎么落到 UnLuaNova 现有架构上、哪些不该抄"
---

# 对标 slua-unreal：差距与吸收

本文回答一个问题：**slua-unreal 做对了什么，UnLuaNova 还差什么，怎么补。**

结论先放在最前面：

* **真正的能力缺口只有一条** —— Lua 声明式网络（RPC / 复制属性）。
* **"Lua 侧一等公民 UObject"（G1）不是缺口**，是我一开始照搬 slua 时误判的。
  核对 `ULuaFunction::IsOverridable` 之后结论反转：它认 `FUNC_BlueprintEvent`，
  **原生类本来就能被覆写**，而"不建蓝图"的需求项目里本来就有 ini `ClassModuleBindings`、
  `IUnLuaNovaInterface`、`UUnLuaNovaBindingComponent` 三条路。造一组 `ALua*` 平行基类
  只会丢掉 `self.Overridden.Xxx(self)` 并多出一套并发生命周期。详见
  [Lua 驱动对象与 C++ 调 Lua](LuaDrivenObjects.md)。
* 本次一并落地的还有：从 C++ 类型化调用 Lua 成员函数（这个是真的缺）、
  DAP 的 `evaluate` / `pause`（G7）、Profiler 按函数聚合（G4）、
  以及与 slua `TestPerf` 同名的跨项目性能对照用例（P0）。
* GC 那一项（G3）**按错误的前提做过头了**：本来没有周期性尖峰要修，见下文 G3。
* **一个可疑的性能信号** —— 我们自己的 benchmark 里「从 Lua 调 UE 函数」1.44 µs，
  而 slua 的**蓝图反射**路径是 0.54 µs（同量级的机器、同为反射），我们的编组层
  相对 `ProcessEvent` 本身多花了约 1 µs。这需要同机复测，但它指向一个明确方向：
  静态绑定应该生成 **thunk**，而不只是生成**描述表**。
* **slua 有三处做法不该抄** —— 运行时给 UClass 动态加 UFunction、自研 Profiler 前端、
  libclang 前向分析。我们在这三处各有更好的底座。

> 本文问的是"对手有什么、我们缺什么"。**第二轮对标换了方向**，问的是
> "我们已经宣称拥有的东西成色如何" —— 结论是短板已经不在功能丰富度，而在
> 默认配置下的成色与工程化（Luau 上 userdata 永不回收、性能数字无产物、
> CI 不跑测试）。见[对标复盘：现状体检与改进方向](Benchmark_Review.md)。

---

## 一、slua-unreal 的形态

### 三种绑定并存

| | 机制 | slua 的做法 | UnLuaNova 的对应物 |
|:---|:---|:---|:---|
| 1 | 蓝图反射 | `LuaObject.cpp` 走 UE 反射 | 同（`ReflectionUtils/`） |
| 2 | 静态代码生成 | libclang 解析 C++ 头文件生成胶水 | `UnLuaNovaStaticBindingGeneratorCommandlet`（走反射，非 libclang） |
| 3 | CppBinding | `LuaCppBinding.h` 模板展开，编组在编译期定型 | **无对应物**（这是关键差距，见 G5） |

slua 的 README 给了 100 万次调用的对比：反射 0.54 s、CppBinding 0.09 s ——
**模板展开比反射快一个数量级**。而我们目前最快的路径（`CallUEFast`）仍属"反射 + 缓存"，
不是"编译期定型"。

### 2.0 的核心：Lua 类成为一等 UObject

这是 slua 与 UnLua 系最大的分野。`Plugins/slua_unreal/Source/slua_unreal/Public/` 下有一组
预置基类：

```
LuaActor.h  LuaPawn.h  LuaCharacter.h  LuaPlayerController.h
LuaGameMode.h  LuaGameState.h  LuaPlayerState.h  LuaUserWidget.h
LuaActorComponent.h  LuaLevelScriptActor.h
```

`ALuaActor` 的声明（已核对源码）：

```cpp
UCLASS(BlueprintType, Blueprintable)
class SLUA_UNREAL_API ALuaActor : public AActor, public ILuaOverriderInterface
{
    GENERATED_UCLASS_BODY()
    virtual FString GetLuaFilePath_Implementation() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated) FLuaNetSerialization LuaNetSerialization;
    ...
};
```

配套 `LuaOverriderInterface.h` 提供 C++ 主动调 Lua 的类型化入口：

```cpp
template<typename RET, typename... ARGS>
RET CallLuaFunction(const FString& FunctionName, ARGS&& ...Args);
// 另有 CallLuaFunctionIfExist / WithContext / IfExistWithContext 四个变体
// 内部 GetSelfTable() 取 Lua 表，TMap<FString, LuaVar> FuncMap 缓存函数
```

**这意味着：** slua 里一个 Lua 类不依附任何蓝图，它是真正的 `ALuaActor` 子类，
有 RPC、有复制属性、能被 C++ 用强类型签名直接调用。

UnLuaNova 目前做不到这件事 —— 见 G1 / G2。

### 网络：运行时增强 UClass

`LuaNet.h`（已核对）暴露：

```cpp
static void addLuaRPCType(const FString& rpcType, EFunctionFlags netFlag);
static ClassLuaReplicated* addClassReplicatedProps(lua_State* L, UObject* obj, const LuaVar& luaModule);
```

配合 `LuaNetSerialization.h/cpp` 的自定义 `NetDeltaSerialize`，把 Lua 表里声明的字段
扁平化后做增量复制。Lua 侧写法（README）：

```lua
LuaActor.ServerRPC.TestServerRPC = { Reliable = true, Params = { EPropertyClass.Int, ... } }
function LuaActor:GetLifetimeReplicatedProps()
    return { { "HP", ELifetimeCondition.COND_OwnerOnly, EPropertyClass.Float }, ... }
end
```

---

## 二、能力矩阵

图例：✅ 已有 ｜ ⚠️ 部分 ｜ ❌ 缺失

| 维度 | slua-unreal | UnLuaNova | 判定 |
|:---|:---|:---|:---|
| 蓝图反射绑定 | ✅ | ✅ | 平 |
| 静态代码生成 | ✅ libclang | ✅ Commandlet + opt-out | 平（路线不同） |
| CppBinding / 编译期 thunk | ✅ | ❌ | **差**（G5） |
| Lua 类成为一等 UObject | ✅ `ILuaOverriderInterface` + 9 个基类 | ✅ **本来就有**：ini `ClassModuleBindings` / `IUnLuaNovaInterface` / `UnLuaNovaBindingComponent` + 原生类可覆写 | **无需补**（见 G1） |
| C++ 类型化调用 Lua | ✅ `CallLuaFunction<RET>` | ✅ `CallLuaFunction<RET>`（本次新增，建立在既有 `PushFunction` 上） | **已补平** |
| Lua 声明 RPC | ✅ | ❌ **`IsOverridable` 明确排除 `FUNC_Net`** | **差**（G2） |
| Lua 声明复制属性 | ✅ 含条件复制 / 数组 / struct | ❌ | **差**（G2） |
| 覆写蓝图事件 | ✅ | ✅ opcode 注入 + `self.Overridden` | 我们更强 |
| Lua 类继承 / super | ✅ `Class(base)` + `__super` + `Super` | ✅ `Class(super_name)` + `Super` | 平 |
| 结构体引用式访问 | ✅ | ✅ `PropertyDesc.cpp` `bCreateCopy=false` | 平 |
| 运算符重载 | ✅ | ✅ `MathLib/LuaLib_F*` | 平 |
| out 参数 / 非 const 引用 | ✅ | ✅ `IsOutParameter` + `CopyBack` | 平 |
| 委托（单播/多播） | ✅ | ✅ `FDelegateRegistry` | 平 |
| 容器 TArray/TMap/TSet | ✅ | ✅ 引用式包装 + 元素缓存 | 平 |
| 扩展方法 | ✅ `LuaExtensionMethod.cpp` | ⚠️ 需写 C++（`UnLuaNovaEx.h`） | 小差（G6） |
| 多 Lua VM | ✅ | ✅ `FLuaEnv` + `ULuaEnvLocator_ByGameInstance` | 平 |
| **多 Lua 语言后端** | ❌ 仅 Lua 5.4 | ✅ **Lua 5.5 + Luau** | **我们更强** |
| 死循环检测 | ✅ | ✅ Hook 分发器共享槽位 | 平 |
| CPU Profiler | ✅ 自研 + 真机 | ✅ 写 Unreal Insights | 平（形态不同，G4） |
| 内存 Profile | ✅ `LuaMemoryProfile` | ✅ `lua.memstats` 分类统计 | 平 |
| 多线程 GC | ✅（README 声明） | ⚠️ 显式 `GC()` 可分片（默认仍是 VM 自管，见 G3） | **无需补**（见 G3） |
| 调试器 | ✅ 专属 VSCode 插件 | ⚠️ 标准 DAP，缺 `pause`/`evaluate` | **已补平**（G7） |
| 静态类型检查 | ⚠️ 靠调试器导出 | ✅ Luau `.d.lua` + LuaLS 注解 | **我们更强** |
| 沙盒 / 只读全局表 | ❌ | ✅ Luau 专属 | **我们更强** |
| 热重载 | ❌ | ✅ 云风 hotfix 方案 | **我们更强** |
| 自动化测试 | ❌ | ✅ 518 条双后端 | **我们更强** |
| 引擎版本 | UE 4.18 / 4.26 / 5.1 | **UE 5.8.3** | **我们更强** |
| protobuf | ✅ 内置 | ✅ `UnLuaNovaExtensions/LuaProtobuf` | 平 |
| 编辑器模拟 C/S | ✅ `LuaSimulate` | ✅ | 平 |

**净结论：** 核对之后 slua 只剩 2 项领先（CppBinding / 编译期 thunk、声明式网络），
我们在 9 项上领先（双后端、类型检查、沙盒、热重载、测试、引擎版本、覆写机制、
Profiler 聚合、无需平行基类即可绑定）。
剩余差距只有一处真正重要：**网络行为的声明能力**。

顺带记一笔教训：G1 与 G3 这两条一开始被我当成缺口，核对 `LuaFunction.cpp:74`（认
`FUNC_BlueprintEvent`）与"`GC()` 只有一个调用方"之后都不成立。**对标时先把对方的动机
翻译成我们自己代码里的事实，再决定要不要动手** —— 否则就是把别人的答案抄成我们的问题。

---

## 三、差距详解与落地设计

### G1 — Lua 侧一等公民 UObject：**不是缺口，本节记录为什么**

这一节原来的结论是"缺，要照 slua 补一组 `ALua*` 基类"。核对之后结论反转，保留推理过程
因为它比结论更有价值。

**被证伪的前提**

我说"原生类不能覆写，所以必须显式转发"。实际：

```cpp
// Private/LuaFunction.cpp:74
return Function->HasAnyFunctionFlags(FUNC_BlueprintEvent)
    || (Function->FunctionFlags & (FUNC_Native|FUNC_Event|FUNC_Net)) == (FUNC_Native|FUNC_Event);
```

认的是 `FUNC_BlueprintEvent`，**与类是不是蓝图无关**。`AActor::ReceiveBeginPlay` 是
`BlueprintImplementableEvent`，在纯 C++ 子类上一样可覆写（`LuaOverrides.cpp:36` 会把它
复制进 `ULuaOverridesClass` 并换 thunk）。

我说"需要一个 Lua 文件路径接口"。实际：`IUnLuaNovaInterface::GetModuleName` 已经干了
这件事，连注释都是同一句话。我造了个近乎重复的第二个接口。

我说"需要绑定后的 C++ 回调"。实际：`UUnLuaNovaManager::Bind` 已经会调模块里的
`Initialize`（`UnLuaNovaManager.cpp:97`）。我照抄了 slua 的 `PostLuaHook`。

**照搬的代价**

造 `ALua*` 平行基类会有三个净损失，所以不做：

1. **丢掉 `self.Overridden.Xxx(self)`** —— 这是 UnLuaNova 的标志性能力，显式转发绕开它；
2. 多一套并行的生命周期路径，触发时机与 tick 顺序要和覆写系统重新对齐；
3. "不建蓝图"本来就有 ini `ClassModuleBindings` / `IUnLuaNovaInterface` /
   `UUnLuaNovaBindingComponent` 三条路，第四条是重复。

**真正缺的那一半**

C++ 侧只能调**全局**函数（`Call` / `CallTableFunc`），没有"按对象调 Lua 方法并按类型取回
返回值"的入口。这个真缺，已补：

```cpp
bool bEnabled = UnLuaNova::CallLuaFunction<bool>(MyActor, TEXT("IsTestEnable"), 1, 2);
```

实现刻意做薄：查找直接复用 `PushFunction()`（覆写路径同一个函数），因此天然共享 self 表、
Super 链与 `ReportLuaCallError`，"C++ 能调"与"Lua 能调"在构造上等价。第一版手写了
metatable 遍历，立刻开始和 `PushFunction` 的语义脱节 —— 已改为复用。

**风险** —— `CallLuaFunction` 若在引擎回调（非游戏线程）里调用会踩 UObject 与 Lua 的线程约束。

---

### G2 — Lua 声明式网络（RPC + 复制属性）

**现状证据**

* `Private/ReflectionUtils/FunctionDesc.cpp:43`：
  `if (InFunction->HasAnyFunctionFlags(FUNC_Net)) LuaFunctionName = FString::Printf(TEXT("%s_RPC"), ...)`
  —— 只处理**引擎里已经存在**的 Net 函数，Lua 侧写 `Xxx_RPC` 只是换个名字覆盖它。
* 全模块搜不到 `GetLifetimeReplicatedProps` / `LifetimeCondition` / `COND_*`。
  Lua **不能声明**新的复制属性，也不能指定复制条件。

**落地设计（关键：不要抄 slua 的运行时改 UClass）**

slua 用 `LuaNet::addClassRPC` 在运行时给 UClass 加 UFunction，代价是一整套清理
（`onObjectDeleted` / `addedRPCClasses` / `removeObjectTable`）。我们有更好的两个现成底座：

* `ULuaOverridesClass` —— 已经是"承载运行时创建的 ULuaFunction"的容器，
  且 `ULuaFunction::RestoreOverrides` 在退出 PIE 时会自动还原。**直接复用。**
* `UnLuaNovaStaticBindingGeneratorCommandlet` —— 构建期生成。**能构建期做的不要留到运行时。**

分三期：

| 期 | 范围 | 做法 |
|:---|:---|:---|
| P1a | RPC：Server / Client / Multicast × Reliable / Unreliable | `TryBind` 阶段读 `M.ServerRPC / ClientRPC / MulticastRPC` 表 → 构造 `ULuaFunction` → 设 `FUNC_Net｜FUNC_NetServer｜FUNC_NetClient｜FUNC_NetMulticast｜FUNC_NetReliable` → 挂进 `ULuaOverridesClass`。参数类型由 `EPropertyClass` 风格声明映射到 `FProperty` 子类 |
| P1b | 复制属性：标量 / FVector / FString / UObject 引用 | 新增 `FLuaNetSerialization` UStruct（`TStructOpsTypeTraits` 带 `WithNetDeltaSerializer`），Lua 侧 `GetLifetimeReplicatedProps()` 返回声明表，绑定期扁平化成索引表，按索引增量序列化 |
| P1c | 条件复制（`ELifetimeCondition`）、`TArray`、嵌套 struct | 在 P1b 的扁平表里加 `Condition` 与嵌套 `FlatArrayPropInfo` |

Lua 侧 API 建议**与 slua 保持一致**（`ServerRPC` / `GetLifetimeReplicatedProps` 同名同形），
降低迁移成本，同时给 `.d.lua` 生成补上对应类型声明。

**验收** —— `UnLuaNovaTestSuite/Private/Tests/Net/`：
2 Client + 1 Server PIE，断言 Server RPC 在服务端执行、Client RPC 只在拥有者客户端执行、
Multicast 全端执行；`COND_OwnerOnly` 属性在非拥有客户端保持默认值。

**风险** —— 这是全表里工作量最大的一项（P1a 约等于一个中等 feature，P1b/c 各再加一倍）。
且动态生成的 UFunction 参与网络复制，必须确保 **cooked 包里能构建期固化**，
否则打包后 RPC 丢失。这一点要先做 spike 验证再全面铺开。

---

### G3 — GC 卡顿：**前提不成立，本节记录为什么**

我原来的表述是"把几秒一次的 40 ms 尖峰摊成每帧零点几毫秒"。那是照搬 slua 的动机，
不是这个项目的事实：

* **全仓库对 `FLuaEnv::GC()` 的调用方只有 `lua.gc` 控制台命令一处**（`->GC()` 全量搜索
  只有一命中）。没有任何代码自动调它，所以不存在周期性尖峰。
* `UNLUANOVA_SETUP_GC` 本来就在跑：Lua 5.5 是 `LUA_GCGEN` **分代**，Luau 是带 step size
  与 goal 的**增量**。两者都已经按分配节奏自行调度。

于是"帧分摊"的真实定位被我高估了。它现在只做一件事：**让显式的那个 `GC()` 可分片**，
服务于"在过场 / 加载界面主动调 GC"的项目。

**因此默认仍是 `Automatic`，也推荐保持。** 切到 `slice` 会 `LUA_GCSTOP`，在 Lua 5.5 上等于
放弃 `UNLUANOVA_SETUP_GC` 特意开启的分代模式 —— 拿一个调过的回收器换一个手写的步进器。
这一点在 `ELuaGcMode` 的注释、`ConsoleCommand.md` 与[架构](Architecture.md#垃圾回收)里
都写明了，避免被当成"默认更优"而误用。

**不做真多线程 GC**：Lua/Luau 的 GC 状态机与 `lua_State` 强绑定，UObject 可达性判定
又必须在游戏线程，真并行需要给 VM 改造成带锁的 GC，维护成本与崩溃风险都不可控。
slua 声称支持，但从源码结构看也没有独立的 GC 线程模块。

**验收** —— 需要的是"分片后单帧耗时显著低于整轮回收"，而不是"原本的尖峰消失了"
（因为本来就没有）。

---

### G4 — Profiler 的可用性（不是能力）

我们的数据已经能进 Unreal Insights，Insights 本身支持连真机 —— 所以"能不能连真机"
不是差距。真正的差距在**聚合**：Insights 的 timing view 面对几百个 Lua 函数不友好，
而 slua 的自研工具能做自底向上的热点聚合。

建议（低成本）：

* `lua.profiler` 增加自底向上（self-time）聚合导出：`lua.profiler dump <file>` 出 CSV/JSON；
* 增加 `lua.profiler record <file>` 落盘，让 CI 能跨提交对比（配合 `Tools/RunTests.ps1`）。

**不要**自研可视化前端 —— 那是一条没有尽头且我们不会做得比 Insights 好的路。

---

### G5 — 静态绑定生成 thunk（回应 1.44 µs）

**问题定位**

我们自己的 benchmark（README）：

| 路径 | µs/次 |
|---|---:|
| UE 反射 `ProcessEvent` | 0.4588 |
| 从 Lua 调用 UE 函数 | 1.4388 |

**差 0.98 µs 全在我们的编组与解析层。** slua 的反射路径 0.54 µs ≈ `ProcessEvent` 裸开销，
说明它的编组层几乎没有额外成本；而它的 CppBinding 是 0.09 µs（编组在编译期定型）。

> ⚠️ 两台机器、两个引擎版本、两套用例，**不能直接比数字**。下一步应先做同机复测：
> 把 slua 的 `TestPerf.lua` 那套用例移植进 `UnLuaNova.Perf.Benchmark`，
> 与我们的数据放同一张表。在拿到同机数据前，下面按"有明确优化空间"处理，不按"慢 2.6 倍"处理。

**落地设计**

当前 Commandlet 生成的是**描述表**（`StaticBindingTypes.h` 的
`FStaticPropBinding` / `FStaticFuncBinding`），读路径仍然是"查表 → 按 `EStaticPropKind`
分支 → 通用编组"。改成**同时生成 thunk**：

```cpp
// 生成产物（示意）
static int32 LuaThunk_AActor_K2_SetActorLocation(lua_State* L)
{
    AActor* Self = (AActor*)UnLuaNova::GetUObject(L, 1);
    FVector  Arg0 = UnLuaNova::GetValue<FVector>(L, 2);   // 编译期定型
    bool     Arg1 = UnLuaNova::GetValue<bool>(L, 3);
    // ... 直接调用，不走 FFunctionDesc
}
static const FStaticFuncBinding FuncBinding_AActor[] = {
    UNLUANOVA_STATIC_FUNC_BEGIN("K2_SetActorLocation", &LuaThunk_AActor_K2_SetActorLocation),
};
```

`FStaticFuncBinding` 增加 `lua_CFunction Thunk` 字段：

* 非空 → metatable 直接挂这个 C 函数，跳过整条反射路径；
* 为空 → 回退现有通用路径（out 参数、变长参数、蓝图实现的函数、Latent 函数）。

**代价** —— 生成代码体积与编译时间上升，需要给 `UnLuaNovaNoExport` 加同级 opt-out
（例如 `meta=(UnLuaNovaNoThunk)`），并且生成器要能判定"这个签名能不能静态编组"。

**验收** —— 同机复测下「从 Lua 调用 UE 函数」向 `ProcessEvent` 裸开销收敛；
`UnLuaNova.Perf.Benchmark` 输出 thunk 命中率（能静态编组的函数占比）。

---

### G6 — 扩展方法（低优先级）

slua 的 `LuaExtensionMethod.cpp` 能把非蓝图函数挂到 UClass 上（README 举例
`UUserWidget::GetWidgetFromName`）。我们靠 `UnLuaNovaEx.h` 的
`ADD_FUNCTION_EX` / `ADD_NAMED_FUNCTION` 也能做到，**只是要写 C++**。

真要补的话，补的是"不写 C++"那条路：
`UnLuaNova.AddExtensionMethod(UClass, Name, Fn)`，把 Lua 函数塞进该类 metatable。
风险是与静态绑定的 metatable 缓存顺序冲突（`GetFieldInternal` 会把结果**缓存进 metatable**，
`Architecture.md` 第 86 行），注册时机必须在首次访问之前。优先级最低。

---

### G7 — 调试器补两个请求

`Docs/CN/Debugging.md` 明说缺 `pause` 与 `evaluate`。这两个恰恰是断点调试体验的分水岭，
而且都不难：

* **`evaluate`** —— 断点命中时游戏线程正阻塞在 `Continue()` 自旋里，
  **这正好是唯一安全的求值时机**，把求值排进这个自旋即可（`FLuaDebugAdapter` 已有线程）。
* **`pause`** —— Luau 后端用 `lua_breakpoint` 设一个"下一行即停"的陷阱；
  Lua 5.5 后端退化成 hook count=1（接受它对性能的短暂影响，暂停本来就是调试态）。

另外可加一个低成本迁移友好项：提供 `import('Button')` 全局函数作为
`UE.UClass.Load` 的语法糖，让 slua 用户的脚本能少改就跑起来。
**但类型声明仍以 `.d.lua` 为准** —— 那是我们的优势，不为兼容让路。

---

## 四、明确不吸收的三项

| slua 的做法 | 为什么不抄 | 我们怎么做 |
|:---|:---|:---|
| 运行时 `LuaNet::addClassRPC` 给 UClass 加 UFunction | 污染 UClass，需要一整套 `onObjectDeleted` / `addedRPCClasses` 清理；热重载与 cooked 包都要特殊处理 | 构建期 Commandlet 生成 + 运行时挂进已有的 `ULuaOverridesClass`（`RestoreOverrides` 自动还原） |
| 自研 Profiler 前端 | 可视化是另一个产品，我们做不过 Insights | 数据继续写 Insights；只补聚合导出与落盘（G4） |
| libclang 前向解析 C++ 头文件 | 需要维护 clang 版本与 UE 宏展开，投入产出比差 | 走 UBT 反射数据 + 构建期 Commandlet（`UnLuaNovaDefaultParamCollector` 已经在 C# 侧做过类似的事） |

---

## 五、吸收计划

按"能独立交付、能独立验收"排序。每项都要求带测试用例进
`UnLuaNovaTestSuite`，否则不算完成。

| 期 | 项 | 工作量 | 依赖 | 验收 | 状态 |
|:---|:---|:---|:---|:---|:---|
| **P0** | 同机复测 benchmark（对齐 slua `TestPerf` 用例名与迭代次数） | 小 | 无 | `parity-latest-<Backend>.csv` | ✅ `UnLuaNova.Perf.BenchmarkParity` |
| **P0** | DAP 补 `evaluate` / `pause` | 小 | 无 | VSCode 里能求值、能暂停 | ✅ |
| **P0** | 显式 `GC()` 可分片 + `lua.gc slice/budget/mult/limit/stats` | 小 | 无 | 分片后单帧耗时显著低于整轮 | ✅ `UnLuaNova.API.GcScheduler`（默认仍是 `Automatic`） |
| **P1** | ~~G1 一等 UObject：`ALua*` 平行基类~~ | — | — | — | ❌ **不做**：前提证伪，见 G1 |
| **P1** | C++ 类型化调用 Lua：`CallLuaFunction<RET>` | 中 | 无 | 按对象调用并取回类型化返回值 | ✅ `UnLuaNova.API.Overrider` |
| **P2** | G2a Lua 声明 RPC | 大 | 无 | C/S PIE 下三类 RPC 行为正确 | ✅ 已落地并有自动 C/S 测试 `UnLuaNova.API.Net.ClientServer`（监听服务器 + 1 客户端 PIE，断言三种 RPC 各自在正确的端上执行）。**仍未覆盖：跨进程 `RPCId` 一致** —— 单进程里两端共用同一个 UClass，证明不了 |
| **P2** | G2b/c 复制属性 + 条件复制 | 大 | G2a | `COND_OwnerOnly` 等条件生效 | ⚠️ 已落地；payload 已从"字符串整体发送"改成**按类型分桶 + push model**，条件粒度是**按类型**而非逐字段。见 [Lua 声明式网络](LuaNetworking.md) |
| **P2** | G5 静态绑定生成 thunk | 中 | P0 benchmark | thunk 命中率 + 调用耗时收敛 | ⬜ 待做（先复测再决定） |
| **P3** | G4 Profiler 聚合导出与落盘 | 小 | 无 | `lua.profiler dump` 出 CSV | ✅ |
| **P3** | G6 扩展方法 ／ `import()` 语法糖 | 小 | 无 | 各一条用例 | ⬜ 待做 |

**G2 的前置本来就是现成的**：RPC 函数可以走 `ULuaFunction`（它本来就是 `UFunction` 子类、
已经有 `execScriptCallLua`），挂进现有的 `ULuaOverridesClass`，退出 PIE 时由
`RestoreOverrides` 一并还原 —— 不需要像 slua 那样在运行时给 UClass 加函数再配清理逻辑。
要跨过的只有 `IsOverridable` 里那句 `!FUNC_Net`。

G2a / G2b 已实现，见 [Lua 声明式网络](LuaNetworking.md)。

原计划里那个"构建期生成的 RPC UFunction 在 cooked 包里能不能保住 Net flag"的 spike
**不需要做了** —— 想清楚之后它问错了方向：我们不是把函数序列化进包里，而是每次启动时
运行时构建，和蓝图类加载时创建 UFunction 是同一回事，与 cook 无关。

真正剩下的未知数只有一个：**两端的 `RPCId` 是否一致**。引擎若已分配就用引擎的；若还是 0，
`EnsureRpcId()` 取"类上现有最大值 + 1" —— 两端跑同一个模块、按同样顺序添加，理论上落到
同样的 id。这需要在 C/S PIE 里确认，也是唯一可能推翻整条设计的点。

---

## 六、两边都缺、我们可以先做的

对标不是为了追平，是为了找空位。这几条 slua 也没有：

* **热更新补丁包与版本管理** —— 我们有 `HotReload.lua`（云风方案），
  但缺"补丁集 → 版本 → 校验 → 回滚"的那一层。
* **Lua 内存按来源归因** —— `lua.memstats` 有分类统计，
  但还不能回答"是哪个模块的哪个函数分配的"，而这才是对付泄漏要的答案。
* **协程泄漏检测** —— Latent 函数会建协程（`Class_CallLatentFunction`），
  悬挂协程没有检测手段。
* **多 Env 调试** —— `ULuaEnvLocator_ByGameInstance` 下可以有多个环境，
  但 `lua.dbg` / `lua.dap` 目前只对一个环境工作。

---

## 延伸阅读

- [架构](Architecture.md) — 目录结构、双后端抽象、覆写与静态绑定链路
- [C++ API 参考](CxxApi.md) — `FLuaEnv`、导出宏、静态绑定注册表、覆写类型
- [相对 UnLua：原理与优势](WhyUnLuaNova.md) — 我们每一个增量的取舍
- [控制台命令](ConsoleCommand.md) — `lua.profiler` / `lua.memstats` / `lua.dbg` / `lua.dap`
- [测试与自动化](Testing.md) — 用例约定（新能力必须带用例）
- slua-unreal：<https://github.com/Tencent/sluaunreal>

---
title: "相对 UnLua：原理与优势"
description: "UnLuaNova 相对 Tencent/UnLua v2.3.6 每个增量的痛点、原理、收益与边界"
---

# 相对 UnLua：原理与优势

[功能清单](Features.md)回答的是"有什么"，本文回答的是"**为什么这么做**"。

每条都按同一个结构写：

| 段 | 回答 |
|---|---|
| **痛点** | 上游 UnLua v2.3.6 在这里是什么形态、代价是什么 |
| **原理** | UnLuaNova 用什么机制解决（落到具体文件与符号） |
| **优势** | 换来了什么，怎么验证 |
| **边界** | 什么情况下不生效、要付什么代价 |

!!! note "增量与沿用要分清楚"
    UnLuaNova 是上游的续作，**不是重写**：覆写机制、容器绑定、反射调用路径、死循环检测、
    `ENABLE_PERSISTENT_PARAM_BUFFER` 参数缓存等都沿用自上游。
    本文只在明确标注"新增"的地方讲增量，沿用部分会说明它在整体链路里扮演什么角色 ——
    把上游的东西说成自己的，读者一旦去翻上游仓库就会失去对文档的信任。

---

## 速览

| 主题 | 一句话原理 | 上游形态 |
|---|---|---|
| [静态绑定](#一静态绑定把查类型从运行时挪到构建期) | 构建期生成 POD 绑定表，读路径从"查反射"变"查表" | 运行时反射 |
| [CallUEFast](#12-calluefast为什么只省一半) | 只省掉"每次查函数"，参数编组照旧 | 每次调用都查 callspace / 覆写 / 接口 |
| [双后端抽象](#二双后端把-vm-差异关进一个头文件) | 差异收敛在 `LuaBackend.h`，分支打在能力宏上 | 单一 Lua 版本 |
| [LuaScriptCompat](#21-luascriptcompat脚本不必为后端写两套) | 缺什么补什么，只补 Luau 少的那一半 | 无（只有一个后端） |
| [VM Hook 分发器](#31-先修根基vm-hook-分发器) | 多消费者 mask 按位或，只装一个原生 hook | 后装者覆盖先装者 |
| [CPU Profiler](#32-cpu-profiler写进-unreal-insights) | 复用分发器 + Insights trace，两侧由同一个运行时开关门控 | 编译期 `ENABLE_UNREAL_INSIGHTS` |
| [死循环看门狗](#33-死循环看门狗顺带修掉的线程模型) | 全进程一条轮询线程，只在可疑时挂 hook | 每 env 一条、且永不退出 |
| [内存统计](#34-内存统计两层数据两层答案) | 默认分配器管总量，Luau 分配回调管分类 | 只有总量 |
| [调试服务](#35-headless-调试服务) | Luau 原生断点单步包成无界面服务 | 需要 luasocket + 外部调试器 |
| [沙盒](#36-沙盒) | `luaL_sandbox` 加固全局表，再把必要的写权限还回去 | 无 |
| [脚本侧类型检查](#四把错误提前脚本侧类型检查) | 反射导出引擎类型签名，交给各后端的分析器 | 只有 EmmyLua 注解 |

---

## 一、静态绑定：把"查类型"从运行时挪到构建期

### 痛点

上游的绑定是**运行时反射**：Lua 访问 `self.SomeProperty`、`self:SomeFunc()` 时，
要经 `FClassDesc` / `FFieldDesc` 在反射系统里按名字找 `FProperty` / `UFunction`，
再构造出一套描述信息才能编组参数。

这条路径有两次缓解，但都缓解得不彻底：

1. **首次访问后缓存进 metatable** —— 缓存的是"描述符"，描述符**自身**的构造仍然是反射查找；
2. **调用期查找** —— 每次 `UFunction` 调用仍要查 `GetFunctionCallspace`、查有没有被 Lua 覆写、
   接口函数还要按对象 `FindFunctionByName`。

对中小项目这够用；对类型上千的项目，"查名字"这件事会被放大到每次属性访问和每次调用上。

### 原理

把"查名字"这件事挪到构建期，运行时只保留"查表"。四个环节：

| 环节 | 做法 | 关键点 |
|---|---|---|
| **生成** | `UnLuaNovaStaticBindingGenerator` Commandlet 为选中类型输出 POD 描述表 | 产物是 `static constexpr` 只读数据，不进运行时初始化路径 |
| **注册** | 每个类型一句 `UNLUANOVA_REGISTER_STATIC_BINDING(Type)`，展开出一个静态 `FStaticBindingAutoRegistrar` | 构造函数只做 `Pending.Add` + `PendingCount.store`，**不碰 UObject**，所以在 DLL 加载期跑是安全的 |
| **解析** | `ResolvePending()` 把 `FProperty*` / `UFunction*` 指针真正解析出来 | 由**读路径惰性触发**，因此一定发生在 UObject 系统就绪之后 |
| **消费** | `FFunctionDesc` 构造时查一次，结果快照在 `FFunctionDesc::StaticBinding` | 之后每次调用不再查注册表 |

两个容易被忽略的设计细节：

- **惰性解析不是"偷懒"，是正确性要求。** `FProperty*` 只有在 UObject 系统起来之后才有效；
  在静态初始化期解引用会拿到空指针。所以注册只入队，解析交给第一次真正的读。
- **读路径的两次快判。** `PendingCount` 是 `std::atomic<int32>`，读路径在**取锁之前**先看它；
  为 0 就直接跳过 resolve 分支 —— 启动结束后的稳态既不加写锁，也不走解析。
  真正要读数据时取的是 `FRWLock` 的**读锁**而不是独占锁
  （`Public/StaticBinding/StaticBindingRegistry.h` 的注释说明了原因：大项目注册几千个类型后，
  读路径会被每次属性/函数访问命中，独占锁会把它们全串行化）。

**类型选择是 opt-out**：默认导出全部类型（跳过 `SKEL_`/`REINST_`/`TRASHCLASS_`/`HOTRELOADED_`/
`PLACEHOLDER-CLASS` 前缀、deprecated / transient），项目只需要给不想要的类型标
`meta=(UnLuaNovaNoExport)`；反过来在基类上标一次 `meta=(UnLuaNovaExport)`，整个派生体系都被拉回。

### 优势

- **热路径变短**："遍历反射链找成员"变成"查表拿已解析好的指针"。
- **规模不再放大单次开销**：读锁并行 + 原子快判，类型数量增长影响的是内存而不是每次访问的耗时。
- **接入成本反过来了**：不需要先列一份"要导出什么"的白名单，而是只声明"不要什么"；
  漏标最坏的结果是"多导出了几个类型"，不是"某个类型在 Lua 里突然访问不了"。
- **生成物是 POD**：可以直接躺在只读段里，不参与静态初始化顺序问题。

### 边界

- **三类函数不走快路径**（`FunctionDesc.cpp` 的分发判断原样保留反射路径）：
  网络函数（要走 `GetFunctionCallspace` / `CallRemoteFunction`）、
  latent 函数（要维护协程）、接口函数（要按对象 `FindFunctionByName`）。
  这不是没做完，是它们确实需要那几步。
- **它是构建期步骤，不是运行期开关**：类型集合变了要重跑 Commandlet，并把生成物一起提交；
  生成物必须落在 `Source/<工程模块>/Generated/` 内才会被 UBT 编译 —— 放错地方等于没有优化。
- **生成的表不入库**，所以正常构建下静态绑定注册表是空的、快路径永远走不到。
  这一点直接决定了测试怎么写：`UnLuaNova.API.StaticBinding` 用两条探针函数分别覆盖
  "有绑定"与"无绑定"，因为 `FFunctionDesc` 在**构造时**就把绑定快照了，同一个函数在一个进程里
  走不了两条路。

---

### 1.1 参数缓存（沿用上游）

调用链上的第三笔开销是参数内存。上游的 `ENABLE_PERSISTENT_PARAM_BUFFER` 已经是答案：
`FParamBufferAllocator_Persistent` 为每个 `UFunction` 持有一组缓冲块，`Get()` 用计数器借出一块，
`Pop()` 只把计数器退回去 —— 省掉的是每次调用的 `FMemory::Malloc` + `Memzero` + `Free`。

UnLuaNova 沿用并默认保持开启（由 `UnLuaNova.Build.cs` 转成编译期宏）。
它是"调用链成本"这节的一部分，但**不是本次新增的能力**，这里写出来是为了让链路完整：
静态绑定省掉"查类型"，`CallUEFast` 省掉"查函数"，参数缓存省掉"分配内存"。

---

### 1.2 CallUEFast：为什么"只省一半"

### 痛点

有了预解析的描述符，最容易犯的错是"干脆绕开整个编组流程"。
绕开之后返回值、out 参数、容器和结构体的生命周期语义会和反射路径分叉，
表现出来就是"开了优化后某些函数的行为变了" —— 这类 bug 极难定位。

### 原理

`FFunctionDesc::CallUE` 的分发条件（`Private/ReflectionUtils/FunctionDesc.cpp`）：

```cpp
// Pre-resolved descriptor available: take the fast path.
if (StaticBinding && !StaticBinding->Flags.bIsNet && !StaticBinding->Flags.bLatent && !bInterfaceFunc)
    return CallUEFast(L, NumParams, FirstParamIndex, Object, Userdata);
```

`CallUEFast` **省掉的**：

- `GetFunctionCallspace()`（非网络函数时 `ProcessEvent` 自己会处理 `FUNC_Native`）
- 覆写重定向查找（只在 `Flags.bOverridable` 时才需要）
- 接口函数的 `FindFunctionByName`

`CallUEFast` **保留的**：`Buffer->Get()`、`PreCall`、`PostCall`、`DestroyParams`、`Buffer->Pop`。

### 优势

**可观测行为与反射路径完全一致** —— 这不是宣传语，而是设计约束：
参数编组和缓冲区管理共用同一份代码，所以"走快路径"和"走慢路径"的差别只在"怎么找到函数"，
不在"怎么调用它"。因此：

- 正确性可以由测试直接断言（`UnLuaNova.API.CallUEFast` 就是断言两条路径结果一致）；
- 出问题时可以整体关掉静态绑定退回反射路径，行为不会变，只变慢。

### 边界

只有命中静态绑定且不属于那三类的函数才会走快路径；
换句话说，**没跑过 Commandlet 的工程一条快路径都不会命中**。

---

## 二、双后端：把 VM 差异关进一个头文件

### 痛点

上游绑死在一个 Lua 版本上。想换 VM，代价是要么大面积改代码，要么放弃这个项目期待上游。
而"Lua 5.5 与 Luau"不是简单的版本升级：Luau 是 Lua 的方言，**没有** userdata `__gc`、
**没有** `load`/`collectgarbage`、迭代元方法叫 `__iter` 而不是 `__pairs`、整数是 32 位 ——
语义差异和 API 差异同时存在。

### 原理

`Public/LuaBackend.h` 是**唯一**吸收 VM 差异的地方，它自己的注释很直白：除它以外的所有代码
都必须走 `UNLUANOVA_*` 宏，且不应当 include `lua.h` / `lualib.h` / `luacode.h`
（两个后端都占用这些文件名）。

三个关键决定：

1. **分支打在能力上，不是后端 id 上。**

    ```cpp
    #if UNLUANOVA_HAS_SANDBOX      // 而不是 #if UNLUANOVA_LUA_BACKEND == LUAU
    ```

    后端 id 只决定"编译哪个 VM"；真正改变行为的是能力。这样第三个后端进来时，
    只需要回答"你有哪些能力"，不用去改每一处 `if`。

2. **能力宏清单就是能力台账。** `UNLUANOVA_HAS_HOOK_EVENT`、`UNLUANOVA_HAS_NATIVE_VECTOR`、
   `UNLUANOVA_HAS_READONLY_TABLES`、`UNLUANOVA_HAS_SANDBOX`、`UNLUANOVA_HAS_BREAKPOINT`、
   `UNLUANOVA_HAS_ALLOC_CALLBACKS`、`UNLUANOVA_HAS_USERDATA_GC`、`UNLUANOVA_HAS_ITER_METAMETHOD` ——
   新增一个 Luau 专属能力，先在这里登记，再由功能代码按宏落地。

3. **三道编译期护栏。** 两个后端同时启用 → `#error`；实际编进来的 VM 与所选后端不符 → `#error`；
   拿错头（`LUA_VECTOR_SIZE`、`sizeof(lua_Integer)` 不对）→ `#error` / `static_assert`。
   这是"差异泄漏到别处"的最后一道防线。

脚本侧的对应物是 `UnLuaNova.GetBackend()` / `UnLuaNova.HasFeature()`
（`Private/UnLuaNovaLib.cpp`）：

```lua
if UnLuaNova.HasFeature('sandbox') then
    -- 能力在，才走依赖它的分支
end
```

`HasFeature` 对**未知名字返回 `false` 而不是报错** —— 脚本因此能在新旧构建上安全降级。

### 优势

- **切换后端只改一个 ini 键**（`Config/DefaultUnLuaNovaEditor.ini` 的 `LuaBackend`）；
  后端是编译期选择，改完重编即可，业务代码一行不动。
- **两个 VM 都以 git 子模块引入**，源码归上游、包装（`*.Build.cs`、单一 TU 聚合文件、
  Luau 的生成包装）归本项目，升级 VM 是移动子模块指针。
- **能力差异对脚本可见**：脚本问的是"有没有沙盒"，不是"我是不是 Luau"。

### 边界

- **两个后端无法在同一个构建里共存**（同名符号 + 同名的 `lua.h`），所以是编译期选择而不是运行时开关。
- **部分能力单后端可用**：沙盒、原生断点、分配分类统计、原生 `vector`/`buffer` 只有 Luau 有。
  这些能力在 Lua 后端不是"降级实现"，而是**不存在**，文档与 `HasFeature` 都如实报告。

---

### 2.1 LuaScriptCompat：脚本不必为后端写两套

### 痛点

有了 Luau 后端，第一个现实问题是：Luau 的基础库和 `debug` 库是子集。
最典型的一条（Issue291）：Luau 没有 `collectgarbage`，脚本直接
`attempt to call a nil value`。热重载依赖的 `debug.getupvalue` / `setupvalue` / `upvalueid`
在 Luau 里也不全。

### 原理

`Private/LuaScriptCompat.cpp`（整文件包在 Luau 分支里），在 `UnLuaNovaLib::Open` **之前**
由 `UNLUANOVA_SETUP_SCRIPT_COMPAT(L)` 装好：

- `debug` 表打补丁：`getupvalue` / `setupvalue` / `upvalueid` / `getlocal` / `setlocal` /
  `getregistry` / `getuservalue` / `setuservalue` —— **只在缺失时插入**，不覆盖已有实现；
- 全局补 `load`（`LuaLoad`）与 `collectgarbage`（`LuaCollectGarbage`，支持 collect/step/count）；
- `pairs` 太窄就**替换**，并把原函数留作 upvalue：表走原生，非表查 `__iter`。
  这一条是必需的 —— Luau 的 `pairs` 只接受真表，会让 `pairs(TArray/TMap/TSet)` 直接报错。

### 优势

脚本作者按 Lua 的直觉写，`pairs(容器)`、`load`、`collectgarbage` 在两个后端都能跑；
两个后端共用一份业务脚本，**不需要维护两套代码**。

### 边界

它补的是"Luau 没有的那一半"，**补不了方向相反的差异**：
Luau 的 `continue`、复合赋值、类型注解在 Lua 5.5 下是语法错误，
这类只能靠脚本自己避开或做后端守卫（`Content/Script/Luau/Index.lua` 就是这么做的）。
另外 Luau **没有 userdata `__gc`**，容器 userdata 不会被终结器回收 —— 资源释放不要寄托在 `__gc` 上。

---

## 三、可观测性：让 Lua 不再是黑盒

上游在这一块并不是"什么都没有"：死循环检测（#428）、内存分配统计、
以及一个编译期开关的 `ENABLE_UNREAL_INSIGHTS` 都在。真正的问题是三件事：

1. 需要 Insights 数据时**要重新编译**（编译期宏）；
2. 多个特性抢**同一个 VM Hook 槽**，后装的把先装的覆盖掉，且失败是**静默**的；
3. 数据只到"总量"这一层，回答不了"内存去哪了"。

下面三节是对应的答案。

### 3.1 先修根基：VM Hook 分发器

### 痛点

一个 Lua VM 只暴露**一个** hook 槽：plain Lua 上是 `lua_sethook()`，Luau 上是
`lua_Callbacks::interrupt`。CPU Profiler 和死循环看门狗都要用它，于是后装者覆盖先装者。

这不是理论风险，`Private/LuaVmHooks.h` 的注释记录了它真实发生过：

> Two features that both need it - today the CPU profiler and the dead-loop watchdog -
> would otherwise overwrite each other's callback, and the loser silently stops working.
> That used to be real: the watchdog checked `lua_gethook()` first and quietly did nothing
> whenever the profiler had it.

"静默不工作"是最坏的一类 bug：看门狗开着，但它在 profiler 打开期间什么都不做，没有任何输出。

### 原理

`FLuaVmHooks`（`Private/LuaVmHooks.h`，header-only）：

- 每个 `lua_State*` 一个槽，槽里是一个 `TArray<FEntry>`，每项带 `Mask`、`Owner`（不透明的
  `const void*`，通常传 `this`）、回调；
- `Add` / `RemoveOwner` / `RemoveAll` 之后统一调 **`Refresh()`**：把所有监听者的 mask
  **按位或**成一个合成 mask，然后**只装一个原生 hook**
  （Lua：`lua_sethook(L, &Dispatch, Mask, 0)`；Luau：`lua_callbacks(L)->interrupt = &OnInterrupt`）；
- `Dispatch` 里用 `1 << Ar->event` 把事件还原回 mask 空间，逐个过滤后分发；
- 卸载是"**按 Owner 移除 + 重算**"，不是把 hook 置 null；
- Luau 下 `Ar` 为 `nullptr`（interrupt 只保证在安全点触发、不带 `lua_Debug`），
  需要帧信息的监听者自己调 `UNLUANOVA_GETINFO`。

### 优势

- **N 个消费者共存**：profiler 与看门狗可以同时开，新增第三个（比如将来的覆盖率统计）
  只要 `Add` 一次，不用知道别人装了什么。
- **只有一个写入点**：hook 槽只由 `Refresh` 写，所以"谁把我的 hook 覆盖了"这类问题在结构上不可能发生。
- **撤销安全**：`RemoveOwner` 用 owner 指针匹配，特性不需要自己保存句柄。

### 边界

合成 mask 是**并集**，所以任一消费者要逐行事件，大家都会收到逐行回调
（各自再按自己的 mask 过滤）。换句话说，分发器解决的是"别互相覆盖"，不是"零成本多路复用"。

---

### 3.2 CPU Profiler：写进 Unreal Insights

### 痛点

上游的 `ENABLE_UNREAL_INSIGHTS` 是**编译期宏**：要开分析就得重新编译，
而且它只覆盖 UE 侧，看不到 Lua 函数内部的耗时。"这段逻辑慢"只能靠猜。

### 原理

`Private/LuaProfiler.cpp` 的 `UnLuaNova::FLuaProfiler`：

1. **不自己调 `lua_sethook`**，而是 `FLuaVmHooks::Get().Add(...)`
   （Lua 后端传 `LUA_MASKCALL | LUA_MASKRET`；Luau 传 mask 0，走 interrupt）——
   这正是分发器的第一个消费者；
2. **事件名只注册一次**：`FCpuProfilerTrace::OutputEventType()` 把
   `Name [Source:linedefined]` 注册成 spec id 并缓存，之后每次命中只发缓存 id，
   用 `OutputBeginEvent` / `OutputEndEvent`；
3. **缓存键跨后端稳定**：`UNLUANOVA_DEBUG_PROTOID` 在 Luau 下取 `protoid`、
   在 Lua 下取 `source << 32 | linedefined`；
4. **UE 侧由同一个开关门控**：`FunctionDesc.cpp` 里 `CallLua`（两个重载）、`CallUE`、
   `CallUEFast` 各有一个
   `TRACE_CPUPROFILER_EVENT_SCOPE_TEXT_CONDITIONAL(*FuncName, FLuaProfiler::Get().IsEnabled())`。

### 优势

- **一次 `lua.profiler on` 同时点亮两侧**，代码注释写得很明确：
  "no rebuild, and no second setting to remember"。开与关都在运行时，不用重新编译。
- **跨语言调用链是完整的**：Insights 里能看到 `UFunction → Lua 函数 → UFunction` 的嵌套，
  而不是两段互不相干的采样。这是"把 UE 侧作用域和 Lua 侧事件放在同一条时间线上"直接换来的。
- **关掉时 UE 侧作用域也被门控掉**（`_CONDITIONAL` 变体），不是"照常采集只是不看"。

### 边界

- Luau 的 `lua_Debug` **没有 `event` 字段**，无法配对 call/return，所以退化为**零长度点事件**：
  Insights 里看到的是每个函数的"命中密度"，不是带时长的区间。
- 整块代码在 `#if CPUPROFILERTRACE_ENABLED` 内；Shipping 构建通常不带它。

---

### 3.3 死循环看门狗：顺带修掉的线程模型

### 痛点

死循环检测本身是上游就有的能力（#428）。但在我们这里它有两个真实的工程问题：

1. **和 profiler 抢 hook 槽，抢不到就静默失效** —— 已在 [3.1](#31-先修根基vm-hook-分发器) 解决；
2. **线程永不退出**：`FDeadLoopCheck` 构造时 `new FRunner()` 起一条每秒轮询的后台线程，
   而 `FRunner::Stop()` 全仓库没有调用点、`~FDeadLoopCheck` 也不存在。
   于是 `~FLuaEnv` 的 `delete DeadLoopCheck` 只删了壳：**每个 Lua 环境留一条线程，
   且那条线程还持有已经析构的 `Env`**。默认关闭时线程照样起，测试里每建一个 env 就漏一条。

### 原理

- **线程模型改为全进程共享一条**：`LuaDeadLoopCheck.cpp` 里
  `static TUniquePtr<FRunner> GDeadLoopRunner`，注释说明了原因 ——
  每 env 一条线程正是耗尽引擎 TLS 线程池、让编辑器在测试中途挂掉的原因；
  补上 `~FDeadLoopCheck` → `FRunner::Shutdown()`（`Stop()` + `WaitForCompletion()`）。
- **判定**：轮询线程每秒 `Sleep(1.0f)`，只有 `GuardCounter > 0`（确实有脚本在执行）时才累加
  `TimeoutCounter`，达到 `Timeout` 秒判定超时。
- **触发**：`FGuard::SetTimeout()` 通过 `FLuaVmHooks::Get().Add(...)` 装一个 LINE hook，
  回调里先 `RemoveOwner` 再 `luaL_error(InL, "lua script exec timeout")`。
- **关闭即零成本**：`Timeout == 0`（默认）时连线程都不创建。

### 优势

- **常态开销接近零**：轮询线程只在有脚本执行时计数，不装任何 hook；
  hook 只在"已经可疑"时才挂上去。这跟"常驻逐行 hook 来判超时"是数量级的差别。
- **超时是 Lua 错误而不是硬卡死**：走 `luaL_error`，可以被 `pcall` 捕获、能带上 Lua 调用栈，
  和脚本里其他错误的处理方式一致。
- **不再泄漏线程**，多环境（多 GameInstance）场景与自动化测试都不再越跑越慢。

### 边界

基于 VM Hook，因此只能拦**字节码层面**的死循环；卡在 C++ 调用里（比如引擎侧的死循环）依然会卡死。

---

### 3.4 内存统计：两层数据，两层答案

### 痛点

上游有"更准确的内存分配统计"，但只有总量。总量能回答"用了多少"，
回答不了"**内存去哪了**" —— 是脚本表、是字符串、还是某个类型刷出来的 userdata。

### 原理

`Private/LuaMemoryStats.cpp` 有两个数据来源，刻意分开：

| 层 | 来源 | 算什么 | 后端 |
|---|---|---|---|
| **总量** | `FLuaEnv::DefaultLuaAllocator`（所有后端都走它）→ `TrackAlloc/TrackFree` | 当前占用、峰值、分配/重分配/释放次数 | 两个后端 |
| **分类** | Luau 专属 `lua_callbacks->onallocate` | 按内存类别 / Lua 类型 / userdata 标签各出 Top N | 仅 Luau |

分类维度用**定长原子数组**（256 / 32 / 128 个桶）而不是哈希映射 —— 分配回调在热路径上，
热路径要用数组索引。峰值用 relaxed CAS 自旋更新，避免为了统计引入锁竞争。

### 优势

- **总量和分类回答不同的问题**：前者是容量（要不要扩容、GC 有没有跑），后者是归因（谁在刷内存）。
- **分类统计可以定位到具体类型**：userdata 标签维度能直接看出"是 `TArray` 还是 `FVector` 在涨"。

### 边界

- **分类统计的是分配量（churn），不是存活字节。** Luau 的 `onfree` 回调**不带大小**，
  所以分类维度减不掉；想知道存活与峰值，看总量部分。这一点文档和命令输出都会写明。
- Lua 5.5 后端没有分配回调（`UNLUANOVA_HAS_ALLOC_CALLBACKS == 0`），
  `lua.memstats` 会输出 `Categorized breakdown requires the Luau backend.` 后跳过 ——
  不假装统计过。

---

### 3.5 headless 调试服务

### 痛点

上游要断点调试得走外部调试器（LuaPanda / LuaHelper），依赖 `luasocket`，
而且要往脚本里塞 `require("LuaPanda").start(...)`。这意味着：
调试能力依赖一个**可选的扩展插件**，且脚本被调试代码污染。

### 原理

`Private/LuaDebugService.cpp` 把 Luau 的原生断点原语包成一个**不带界面**的服务：

- **断点 / 单步**走 Luau 的独立通道（`lua_callbacks->debugbreak` / `debugstep`），
  **不与 profiler、看门狗共享的 `interrupt` 槽冲突**；
- **调用栈 / 局部变量**按后端分别取：Luau 用 `lua_getinfo(L, Level, "sln")`，
  Lua 用 `lua_getstack` + `lua_getinfo("nSl")`，最多 128 层；局部变量遍历 `lua_getlocal` 索引 1..63；
- **"headless" 是刻意的分层**：无 socket、无 JSON、无自有 UI，只暴露 C++ API，
  供控制台命令、自动化测试和将来的 DAP 适配器驱动。

### 优势

- **零依赖**：不用装插件、不用 `luasocket`、不用改脚本。
- **可进自动化测试**：能力是 C++ API，测试可以直接断言"断点登记成功 / 单步命中并抓到调用栈"。
- **常态零开销**（Luau 后端）：`Attach()` 只在 `lua.dbg` 命令里调用；
  Luau 的单步是**模板参数**，`L->singlestep == false` 时走 `luau_execute<false>`，
  逐指令 hook 分支被编译期裁掉；`debugbreak` 只在 `LOP_BREAK` 指令上触发，
  而 BREAK 指令需要 `lua_breakpoint` 插入。没敲命令时回调永不执行。

### 边界

- **Lua 5.5 后端不是零开销**：它没有原生断点原语，服务退化为
  `FLuaVmHooks` + `LUA_MASKLINE`，那是真实的逐行 hook。
- `lua_breakpoint` 宏虽已封装，服务当前走的是"hook + 源码行匹配"路径（`MatchesBreakpoint`）。
- 还缺一个把它接到 IDE 的 **DAP 适配层**。

---

### 3.6 沙盒

### 痛点

热更新 / 下载的脚本与本地脚本跑在同一个全局表里。一条 `UE = nil` 或
`UE.UObject = somethingElse` 就能把引擎绑定改掉，而故障现场往往离这行代码很远。

### 原理

`Public/UnLuaNovaSandbox.h`：CVar `lua.sandbox` 在每个 Lua 环境创建时读取一次，
为真则调 **`luaL_sandbox(L)`**（Luau 原生）—— 它把全局表及其挂接的子表全部置只读。

紧接着有一个很容易被漏掉的步骤：`RestoreTablesAfterSandbox`
（`Private/UnLuaNovaLib.cpp`）把 `UE` / `UE4` / `Lua` / `UnLua` / `UnLuaNova` /
`package` 及 `package.loaded` 的**写权限还回去**。

原因是 UE 的类型是**懒注册**的：第一次访问 `UE.XXX` 时才写进去。
如果把 `UE` 一并冻住，下一个访问新类型的脚本、下一次 `require` 都会直接报错 ——
沙盒就变成"开了就没法用"。

脚本那一侧的写入仍然由 `UE` 命名空间自己的 `__newindex` 拦截
（`UELib::SetNamespaceWritable`），**这条在两个后端下都生效**。

### 优势

- **防误改，不防功能**：把"脚本改写全局"这条路堵上，同时保证懒注册和 `require` 照常工作。
- **保护强度没有下降**：还回的是 C++ 侧的写权限，脚本侧的写入仍被 `__newindex` 拦着。

### 边界

- **默认关闭**（`lua.sandbox=0`），因为这是对既有脚本的行为变更；要开就写进 ini 让它启动时生效。
- **它是启动期 CVar，不是运行时命令**：运行中再改不影响已存在的环境。
- **仅 Luau 支持**（`UNLUANOVA_HAS_SANDBOX`）。CVar 在两个后端都注册，
  所以不会"命令不存在"，但写入在 Lua 5.5 下不产生效果 —— 是否真的生效由
  `UnLuaNova.HasFeature('sandbox')` 回答。
- **类表尚未冻结**：只冻了全局表 + 拦截脚本写入 `UE`。类表是懒缓存的（首次访问会 `rawset`），
  冻住会打断缓存。

---

## 四、把错误提前：脚本侧类型检查

### 痛点

Lua 脚本里的 UE API 误用（函数名拼错、参数个数不对、取了不存在的属性）
在上游只能**运行时**发现，而且常常表现为"静默拿到 nil"，一路传到很远的地方才炸。

### 原理

引擎类型是反射可见的，所以签名可以被导出；脚本侧 API 是运行时注入的 Lua 表，
反射不到，只能手写。于是产物分成两份，各归各的分析器：

```
Plugins/UnLuaNova/IntelliSense/
├── EmmyLua/    Lua 方言     → lua-language-server（LuaLS）
└── Luau/       Luau 方言    → luau-lsp
    └── UnLuaNova.d.lua
```

- 反射类型由 `UUnLuaNovaIntelliSenseCommandlet` + `FUnLuaNovaIntelliSenseGenerator` 导出；
- 脚本侧 API 用签入仓库的手写声明 `Content/IntelliSense/UnLuaNova.d.lua` 拼接进去。

### 优势

- **一部分 UE API 误用从运行时提前到"编译期"**：`--!strict` + `luau-lsp` 能在写的时候报出来。
- **类型映射有明确规则**：整数各宽度与浮点统一为 `number`，`FString`/`FName`/`FText` 为 `string`，
  `TArray<T>` → `{T}`，`TMap<K,V>` → `{[K]: V}`，`TSet<T>` → `{[T]: boolean}`，
  委托等无法表达的类型退化为 `any`。
- **检查结果会进测试报告**：`Tools/RunTests.ps1` 按后端分派
  （Luau → `luau-lsp`，Lua → LuaLS），没工具时报告写"跳过 + 原因"，不假装查过。

### 边界

- **两个后端各用自己的分析器，这是语法事实不是偏好**：Luau 的类型注解 Lua 5.5 解析不了，
  Lua 后端的脚本也喂不进 `luau-lsp`，不存在"共用一个"。
- **`UE` 全局目前是不透明表**：它有 1.8 万个成员，会触发 Luau 的复杂度上限，
  且脚本写 `UE.FVector` 而声明叫 `Vector`。这一条还没打通。
- `luau-lsp` 是**本地步骤**，CI 上没有可执行文件。

---

## 五、小改动里的一致性原理

几个看起来很小的改动，背后都有具体的原因，值得单独说明 —— 它们体现的是同一条原则：
**错误信息要指向真正的原因，语义要和 C++ 侧对齐。**

### `pairs(TSet)` 产出 `(元素, true)`

`Private/BaseLib/LuaLib_Set.cpp` 里的 `TSet_Pairs`：为什么不交出槽位索引？
因为 `FScriptSet` 是**稀疏**的，元素删除后有空洞，槽位索引在删除发生后没有意义。
返回 `(value, true)` 让 `for k in pairs(Set)` 与 C++ 的 range-for 语义对齐。
跨后端的差异由 `UNLUANOVA_ITER_ENTRY` 吸收（Luau 是 `__iter`，Lua 是 `__pairs`，
两者协议相同，复用同一个 C 函数）。

### `TMap:Contains(Key)`（与 `TArray` / `TSet` 对齐）

缺一个方法时，`Map:Contains(2)` 会走到 `__index` 的自引用链上，报
`'__index' chain too long; possible loop` —— **看不出真正的原因是名字不存在**。
另外 `Find` 返回 `nil` 有歧义：既可能是"没有这个键"，也可能是"值本身是 nil"。

### 字符串扩展按字面量分隔

`Content/Script/UnLuaNova/StringEx.lua` 的 `split` / `count` 用
`s:find(sep, Start, true)` —— 第四个参数 `plain=true` 关掉模式匹配。
所以 `("a.b.c"):split(".")` 得到的是按点分隔的三段，而不是一个空表。
按 Lua 5.1 语义写，两个后端行为一致。

---

## 六、代价与已知边界

把边界集中列在这里，是为了让"要不要用"能一次判断完：

| 项 | 现状 |
|---|---|
| 静态绑定 | 构建期步骤，类型集合变了要重跑并提交生成物；网络 / latent / 接口函数不走快路径 |
| 后端切换 | 编译期选择，改完要重编；两个后端不能共存于同一构建 |
| Luau 专属能力 | 沙盒、原生断点、分配分类统计、`vector` / `buffer` 在 Lua 5.5 下**不存在**（不是降级） |
| Luau 语义差异 | 没有 userdata `__gc`；`lua_Integer` 是 32 位；yield 边界更严格 |
| Profiler | Luau 无 `event` 字段 → 零长度点事件，不是带时长的区间 |
| 内存分类统计 | 统计的是分配量（churn），不是存活字节 |
| 调试服务 | headless，缺 DAP 适配层；Lua 5.5 后端是真实逐行 hook，非零开销 |
| 类型检查 | 两个后端各用各的分析器；`UE` 全局尚为不透明表 |
| `buffer` ↔ `TArray<uint8>` | 目前是显式逐字节转换，零拷贝未做 |

---

## 延伸阅读

- [功能清单](Features.md) — "有什么"的清单版
- [架构](Architecture.md) — 代码分层、静态绑定全链路、双后端抽象
- [静态导出绑定](StaticExportBinding.md) — Commandlet 用法与 opt-out 规则
- [Luau 后端](Luau.md) — Luau 专属能力与行为差异
- [控制台命令](ConsoleCommand.md) — `lua.profiler` / `lua.memstats` / `lua.dbg` / `lua.sandbox`
- [测试与自动化](Testing.md) — 这些能力分别由哪些用例兜底

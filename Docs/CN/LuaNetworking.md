---
title: "Lua 声明式网络"
description: "在 Lua 模块里声明 Server/Client/Multicast RPC 与复制属性：语法、实现原理、以及需要联网验证的部分"
---

# Lua 声明式网络

## 为什么需要

缺口的位置很具体。`ULuaFunction::IsOverridable()`：

```cpp
// Private/LuaFunction.cpp:74
static constexpr uint32 FlagMask   = FUNC_Native | FUNC_Event | FUNC_Net;
static constexpr uint32 FlagResult = FUNC_Native | FUNC_Event;
return Function->HasAnyFunctionFlags(FUNC_BlueprintEvent) || (Function->FunctionFlags & FlagMask) == FlagResult;
```

`FUNC_Net` 被排除在第二个分支之外，而 `FunctionDesc.cpp:43` 只给**已存在**的 Net 函数改名成
`Xxx_RPC`。两条合起来的后果是：**Lua 只能重新实现引擎已经声明好的 RPC，不能自己声明一个。**

本文讲的就是补上这一点。

## 声明 RPC

```lua
local M = UnLuaNova.Class()

M.ServerRPC = {
    -- 简写：声明它，默认不可靠（与 UFUNCTION(Server) 一致）
    Ping = true,

    -- 完整写法
    Chat = {
        Reliable = true,
        Params = { "Int", "Str" },                     -- 简写，自动命名为 Arg0、Arg1
        -- 也可以写成 { Name = "Channel", Type = "Int" }
    },
}

M.ClientRPC    = { ShowDamage = true }
M.MulticastRPC = { Explode    = true }

function M:Chat(Channel, Text) end
function M:Ping() end
return M
```

参数类型名（大小写不敏感）：`Bool` `Int`/`Int32` `Int64` `Float` `Double` `String`/`FString`
`Name` `Vector` `Rotator` `Transform` `Object`。

**为什么用字符串而不是枚举**：声明层因此不需要 UHT 参与（没有 generated.h、没有反射依赖），
也不依赖任何枚举已注册进 Lua 注册表 —— 不会因为某个名字变更而失效，绑定期之前就能用。
代价是没有编译期类型检查，靠 `.d.lua` 与运行时警告补。

## 声明复制属性

```lua
function M:GetLifetimeReplicatedProps()
    return {
        { Name = "HP",   Type = "Float" },
        { Name = "Name", Type = "String", Condition = "OwnerOnly" },
    }
end
```

或者写成静态表 `M.ReplicatedProps = { ... }`，列表是常量时没必要付一次函数调用。

条件名用引擎自己的：`None` `InitialOnly` `OwnerOnly` `SkipOwner` `SimulatedOnly`
`AutonomousOnly` `SimulatedOrPhysics` `InitialOrOwner` `Custom` `ReplayOrOwner`
`ReplayOnly` `SimulatedOnlyNoReplay` `SimulatedOrPhysicsNoReplay` `SkipReplay`。

### 需要挂一个组件

复制属性**必须**有一个真正的 `UPROPERTY(Replicated)` 承载 —— 属性活在类的内存布局里，
不像函数那样能在运行时加进 `FuncMap`，这是覆写机制唯一造不出来的东西。

```lua
function M:Initialize()
    self.Replication = self:AddComponent(UE.UUnLuaNovaReplicationComponent)
end
```

也可以直接在蓝图里加。`BeginPlay` 时组件会读声明：没声明就不开 tick（一个"以防万一"加上去的
组件不该有成本）；有声明就按分桶布局分配槽位。

### 服务端怎么知道字段变了

两种模式，组件上的 `UpdateMode`：

| 模式 | 行为 | 成本 |
|:---|:---|:---|
| `Polled`（默认） | 每 `PollInterval`（默认 0.1s）扫一遍 Lua 表 | 每字段一次 Lua 读取 |
| `Explicit` | 完全不扫，脚本自己说 | **每帧零成本** |

`Explicit` 是生产环境该用的那个。改了字段就标一下：

```lua
function M:TakeDamage(Amount)
    self.HP = self.HP - Amount
    self.Replication:MarkFieldDirty("HP")
end
```

组件还给了 `MarkAllFieldsDirty()`（全量扫一次，适合初始化后）与 `FlushNetUpdate()`
（不等 `NetUpdateFrequency`，本帧就发）。

休眠的 actor 不会被扫描（`DORM_DormantAll` 直接跳过）；`MarkFieldDirty` 会先
`FlushNetDormancy()`，否则改动会一直躺着没人送。

### 客户端怎么知道哪个字段变了

组件在写回 Lua 表之前会先读出旧值比对，只对真正变了的字段派发：

```lua
function M:OnRep_HP()          -- 逐字段
    self:UpdateHealthBar()
end

function M:OnReplicatedProps(Names)   -- 兜底：{ "HP", "Mana" }
end
```

这个变更集是**在客户端算出来的，不占带宽**。

## 实现原理

**RPC 没有另起炉灶** —— `FLuaOverrides::Override()` 本来就有一条"新增函数"的路径：

```cpp
// LuaOverrides.cpp:41
const auto bAddNew = Function->GetOuter() != Class;
```

只要源 UFunction 的 outer 不是目标类就走这条，`ULuaFunction::SetActive()` 随后
`AddFunctionToFunctionMap()` + `AddNativeFunction()`，native func 设为 `execCallLua`。

所以"声明一个 RPC"= **造一个带 `FUNC_Net` 和类型化参数的 UFunction**，然后喂给
`ULuaFunction::Override()`。顺带白拿两样东西：

* **PIE 退出自动还原** —— 函数挂在 `ULuaOverridesClass` 上，`RestoreOverrides()` 会撤掉；
* **不走 `FUNC_BlueprintEvent`** —— RPC 不是给人覆写的事件，而 `IsOverridable()` 本来就拒绝
  带 `FUNC_Net` 的函数，加上反而是错的。

对比 slua：它是运行时 `LuaNet::addClassRPC` 给 UClass 加函数，再配一套
`onObjectDeleted` / `addedRPCClasses` 清理。我们这些都是现成的。

**复制属性**用"按类型分桶"：每种类型一条复制数组，字段按声明顺序落进桶里，地址是
`(桶, 下标)`，布局两端各自从同一份声明算出来，所以线上不带字段名、不带类型标签。

```cpp
// UUnLuaNovaReplicationComponent —— 十一条，一条一个类型
UPROPERTY(ReplicatedUsing = OnRep_Floats) TArray<float> Floats;   // [HP, Mana]
UPROPERTY(ReplicatedUsing = OnRep_Strings) TArray<FString> Strings; // [NickName]
```

一个 `float` 现在是 4 字节，而不是"FName + int32 + 字符串"那一套。代价与收益见下一节。

## 网络优化：push model 与 Iris

分桶不是为了"少发几个字节"，它是为了让后面这两件事**有可能**。

### push model

每个桶都用 `bIsPushBased` 注册，并单独标脏：

```cpp
FDoRepLifetimeParams Params;
Params.bIsPushBased = true;                                   // WITH_PUSH_MODEL 下
DOREPLIFETIME_WITH_PARAMS_FAST(UUnLuaNovaReplicationComponent, Floats, Params);
...
MARK_PROPERTY_DIRTY(this, Floats);   // 只脏这一个桶
```

于是复制系统不再轮询这些属性：改 `HP` 只让引擎看 `Floats`，`Strings` 一眼都不看。
改路径里**刻意没有** `MarkNetDirty()` / `ForceNetUpdate()` —— 那正是 push model 要去掉的
东西；要立刻发由脚本显式调 `FlushNetUpdate()`。

两条退化路径都是安全的：

* `WITH_PUSH_MODEL=0`（项目关掉）：宏不存在，桶变成普通属性由复制系统自己比对，功能不变。
* 编译进来了但运行时 `net.IsPushModelEnabled 0`：`MARK_PROPERTY_DIRTY` 是空操作，同样退回轮询。

组件启动时会打一行日志说明走的是哪条，别靠猜。

配合 `UpdateMode = Explicit` 才算完整：**扫描没了，轮询也没了**，一个字段的改动从 Lua 到
线上只有一次读、一次比较、一次标脏。

### Iris

兼容性是**结构性**的，不靠调 Iris 的任何 API。整个复制路径里有三样东西是刻意不写的：

* 没有 `NetDeltaSerialize` / `WithNetDeltaSerializer`
* 没有 `FFastArraySerializer`
* 没有自定义 `NetSerializer`

剩下的是最普通的、带类型的 `UPROPERTY`。legacy 和 Iris 都认这个，所以切到 Iris 不需要
改一行代码。而且 Iris 还会白送两样：

* **逐元素增量** —— `TArray` 在 Iris 里是动态状态，改 `Floats[0]` 只发 `[0]`；
  legacy 下则是整个数组（这是 legacy 唯一剩下的浪费，见已知边界 3）。
* **按类型量化** —— float / FVector / FRotator 走各自的 NetSerializer 压缩，
  而字符串载荷是拿不到这个的。

反过来说，前两版考虑过的"`FLuaNetSerialization` + 手写 `NetDeltaSerialize`"方案被放弃了，
两个原因：legacy 的 delta 序列化要自己维护 baseline，丢包时状态会发散（要重同步协议才救
得回来），而 `FNetDeltaSerializeInfo` 在版本间会变；更关键的是它在 Iris 下走不走得通是个
未知数。全量桶是自愈的：任何一次更新都包含完整状态，丢一次包不会错位。

### SchemaVersion

桶下标来自声明顺序，两端必须一致。组件会带一个 32 位指纹（`ComputeSchemaVersion`，
覆盖名字、顺序、类型），客户端收到后对不上就报错并拒绝应用 —— 热重载改了声明列表时，
你会看到一条明确的 Error，而不是 `HP` 被写进 `Mana`。

## 已知边界

1. **跨进程的 `RPCId` 一致仍未验证。** `UnLuaNova.API.Net.ClientServer` 已经能在
   C/S PIE 里断言：Server RPC 只在服务端执行、Client RPC 只在客户端执行、Multicast
   两端都执行、参数原样过线、Lua 声明的复制字段能到客户端。它证明不了的是**两个进程**
   的情况 —— RPC 在线上走的是 id 而不是名字，两端各建一份 UFunction 时必须落到同一个
   id。`EnsureRpcId()` 的处理是：引擎若已分配就用引擎的；若还是 0，就取"类上现有最大值
   + 1"。两端跑同一个模块、按同样顺序添加，因此会落到同样的 id —— 这是唯一剩下的
   未知数，并且它**不可能**被单进程测试证明：C/S PIE 里两端共用同一个 UClass，id 自然
   就是同一个。要证明它需要一次真正的双进程联机（独立服务器 / 打包客户端）。

2. **复制条件是"每类型一个"，不是"每字段一个"。** 同类型的字段共用一个条件（冲突时取第一个
   非 None 的并打警告）。比之前"整个 payload 共用一个"细了一档，但逐字段仍然做不到：那需要
   每个条件一个 UPROPERTY，运行时加不了。真要逐字段，出路是构建期代码生成，或者 Iris 的
   condition group。

3. **legacy 下改一个字段会重发整个桶。** 改 `HP` 会把该 actor 上所有 `Float` 一起发走。
   Iris 下没有这个问题（逐元素增量）。同一类型字段多、且只有一个在高频变化时，Iris 是答案，
   或者把那个字段在 C++ 侧声明成真正的 UPROPERTY。

4. **`Object` 类型按 `UObject*` 处理**，不区分子类。`Vector` / `Rotator` / `Transform`
   要求 Lua 侧放的是真正的 userdata（`UE.FVector(...)` 产出的那种）；给个别的东西会跳过
   并打日志，不会被当成向量重新解释。

5. **`Explicit` 模式下漏标就不发。** 改了字段不调 `MarkFieldDirty` 的话，服务端永远不知道。
   这是这个模式的代价，不是 bug —— 拿不准就先用 `Polled`。

6. **热重载后 RPC 会重新注册。** 若类上已有同名函数，声明会被跳过并打警告 —— 不会盖掉
   引擎或蓝图自己声明的 RPC。复制字段则会带上新的 `SchemaVersion`，对端会报不匹配。

## 验证清单

* `UnLuaNova.API.Net` —— 解析、标记、参数、还原（无需联网）
* `UnLuaNova.API.Net.ClientServer` —— 自动开一个监听服务器 + 1 客户端的 PIE 会话，断言
  Server RPC 在服务端执行且不在发起它的客户端执行、Client RPC 只在客户端执行、
  Multicast 两端都执行、参数原样过线、Lua 声明的复制字段到达客户端。
  会话里还带一个**引擎自己复制的 `int32` 作对照**：它没到就说明会话本身是坏的，
  后面关于 Lua 的断言都不用看了
* 仍未覆盖：跨进程 `RPCId` 一致（见已知边界 1）；`COND_OwnerOnly` 字段在非拥有客户端
  保持默认值（需要一个能区分拥有端的测试场景）

### push model 与 Iris 待验证项

源码不在这台机器上，下面几条**必须**在有引擎的地方过一遍，顺序即优先级：

1. `MARK_PROPERTY_DIRTY` / `FDoRepLifetimeParams::bIsPushBased` / `WITH_PUSH_MODEL`
   照当前名字能编译（5.x 稳定 API，但要确认这份源码没改）。
   编译不过的话 `UNLUANOVA_PUSH_MODEL` 会是 0，功能照常、只是退化成轮询 —— 启动日志会说明。
2. C/S PIE 里 `net.IsPushModelEnabled 1`：改动**能到**客户端（证明标脏生效）；
   随即改成 `0`：改动**仍然能到**（证明退化路径没写错）。
3. `Explicit` 模式：只 `MarkFieldDirty("HP")`，客户端只看到 `HP` 变，
   `OnRep_HP` 被调用一次且 `OnRep_Mana` 不被调用（证明逐字段派发是对的）。
4. Iris：按这份源码里 Iris 的启用方式（5.x 是 `Net.Iris.Enabled` 一类的 cvar，以源码为准）
   打开后跑同一组断言，逐字比对结果。重点看两件事：`TArray` 是否走了逐元素增量、
   `MARK_PROPERTY_DIRTY` 在 Iris 下是否被桥接（若否，桶会退回全量比对，功能仍正确）。
5. 休眠：actor 进 `DORM_DormantAll` 后改字段并 `MarkFieldDirty`，确认它被唤醒且数据到端。

---

## 延伸阅读

- [Lua 驱动对象与 C++ 调 Lua](LuaDrivenObjects.md) — 绑定与覆写的既有机制
- [C++ API 参考](CxxApi.md) — `ULuaFunction`、`FLuaEnv`
- [架构](Architecture.md) — 覆写链路
- [对标 slua-unreal](Comparison_sluaunreal.md) — 这条缺口是怎么定位的

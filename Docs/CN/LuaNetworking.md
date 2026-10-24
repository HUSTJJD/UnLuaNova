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
组件不该有成本）；有声明就按条件复制。

服务端每 tick 把 Lua 表里的字段推进 payload；客户端收到 `OnRep` 再推回自己的 Lua 表。

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

**复制属性**用 `TArray<FLuaRepField>`（`FName` + `int32` 类型 + `FString` 值）整体复制。
值走字符串是有意的取舍：纯 UPROPERTY 的 USTRUCT **不需要自定义序列化器**，于是不用实现
`NetDeltaSerialize`、不用碰版本间会变的 `FNetDeltaSerializeInfo`、也不用算逐字段变更掩码。
代价是带宽 —— payload 一变就全发。

## 已知边界

1. **RPC 的实际跨端行为仍需一次联网验证。** 单测能证明声明被解析、UFunction 带着正确的
   Net 标记进了 `FuncMap`、参数顺序与类型对。证明不了对端真的收到 —— 那取决于两端的
   `RPCId` 是否一致。`EnsureRpcId()` 的处理是：引擎若已分配就用引擎的；若还是 0，就取
   "类上现有最大值 + 1"。两端跑同一个模块、按同样顺序添加，因此会落到同样的 id ——
   **这正是需要在 C/S PIE 里确认的点**，也是唯一可能推翻整条设计的未知数。

2. **复制属性不支持逐字段条件。** 整个 payload 共用一个条件（声明冲突时取第一个非 None 的
   并打警告）。原因是结构性的：逐字段条件需要每个条件一个 UPROPERTY，而那不能运行时加。

3. **复制是整体发送，不是增量。** 见上文取舍。字段少、变化不频繁时完全够用；字段多或
   高频变化的场景，应该先在 C++ 侧声明真正的 UPROPERTY。

4. **`Object` 类型参数按 `UObject*` 处理**，不区分子类。

5. **热重载后 RPC 会重新注册。** 若类上已有同名函数，声明会被跳过并打警告 —— 不会盖掉
   引擎或蓝图自己声明的 RPC。

## 验证清单

* `UnLuaNova.API.Net` —— 解析、标记、参数、还原（无需联网）
* **需要补的**：C/S PIE 下断言 Server RPC 在服务端执行、Client RPC 只在拥有者客户端执行、
  Multicast 全端执行；`COND_OwnerOnly` 字段在非拥有客户端保持默认值

---

## 延伸阅读

- [Lua 驱动对象与 C++ 调 Lua](LuaDrivenObjects.md) — 绑定与覆写的既有机制
- [C++ API 参考](CxxApi.md) — `ULuaFunction`、`FLuaEnv`
- [架构](Architecture.md) — 覆写链路
- [对标 slua-unreal](Comparison_sluaunreal.md) — 这条缺口是怎么定位的

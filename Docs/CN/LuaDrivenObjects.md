---
title: "Lua 驱动对象与 C++ 调 Lua"
description: "用 UnLuaNova 既有的绑定与覆写机制让 Lua 模块成为游戏对象（不需要平行基类），从 C++ 类型化调用 Lua 成员函数，以及真正剩下的缺口：RPC 不可覆写"
---

# Lua 驱动对象与 C++ 调 Lua

## 先说结论：不需要平行基类

一个很自然的想法是"照 slua 那样提供 `ALuaActor` / `ULuaUserWidget` 一组基类，让 Lua 模块
不依赖蓝图就能成为游戏对象"。**这个项目里那是错的**，原因在 `ULuaFunction::IsOverridable`：

```cpp
// Private/LuaFunction.cpp:74
static constexpr uint32 FlagMask   = FUNC_Native | FUNC_Event | FUNC_Net;
static constexpr uint32 FlagResult = FUNC_Native | FUNC_Event;
return Function->HasAnyFunctionFlags(FUNC_BlueprintEvent) || (Function->FunctionFlags & FlagMask) == FlagResult;
```

它认的是 `FUNC_BlueprintEvent`，**与这个类是不是蓝图无关**。`AActor::ReceiveBeginPlay` 是
`BlueprintImplementableEvent`，所以哪怕在一个纯 C++ 子类上它也是可覆写的 —— 覆写系统会
把 UFunction 复制进 `ULuaOverridesClass` 并换掉 thunk（`LuaOverrides.cpp:36`）。

也就是说：**原生类一直都能被 Lua 覆写**。再造一组基类去"显式转发"生命周期，只会带来三个损失：

1. **丢掉 `self.Overridden.Xxx(self)`** —— 那是 UnLuaNova 的标志性能力，显式转发绕开了它；
2. **多一套并行的生命周期路径**，与覆写系统的触发时机、tick 顺序都要重新对齐；
3. **模块定位本来就已有三条路**（下表），再加第四条纯属重复。

## 让一个 Lua 模块驱动对象：既有的三条路

| 方式 | 粒度 | 适用场景 |
|:---|:---|:---|
| `IUnLuaNovaInterface::GetModuleName()` | 类（读 CDO） | 蓝图 / C++ 类自己声明，最常见的用法 |
| ini `ClassModuleBindings` | 类 | **不碰任何代码**，一行配置把既有类绑到模块 —— 包括原生类 |
| `UUnLuaNovaBindingComponent` | 实例 | 同一个类的不同实例指向不同模块 |

```lua
-- Content/Script/Gameplay/Pickup.lua
local M = UnLuaNova.Class()

function M:Initialize()          -- 绑定完成后由 UUnLuaNovaManager::Bind 调用
    self.Score = 10
end

function M:ReceiveBeginPlay()    -- 普通覆写，原生类也生效
    print("spawned")
end

function M:ReceiveTick(DeltaSeconds)
    self:AddActorLocalRotation(UE.FRotator(0, 90 * DeltaSeconds, 0))
end

return M
```

`/Script/UnLuaNova.UnLuaNovaSettings` 里加一行就够了 —— 不用建蓝图，不用写 C++，
也不用新增任何基类：

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+ClassModuleBindings=(Class="/Game/Blueprints/BP_Pickup.BP_Pickup_C",Module="Gameplay.Pickup")
```

**绑定完成后的钩子已经存在**：`UUnLuaNovaManager::Bind` 会调用模块里的 `Initialize`
（`UnLuaNovaManager.cpp:97`），不需要额外的 C++ 回调接口。

## 从 C++ 调用 Lua 成员函数

真正缺的是这一半。以前引擎侧只能调**全局**函数（`UnLuaNova::Call` / `CallTableFunc`），
没有"在这个对象上调用这个 Lua 方法、按 bool 取回结果"的入口。

```cpp
#include "UnLuaNovaOverrider.h"

bool   bEnabled = UnLuaNova::CallLuaFunction<bool>(MyActor, TEXT("IsTestEnable"), 1, 2);
double Total    = UnLuaNova::CallLuaFunction<double>(MyActor, TEXT("Sum"), 1.5, 2.5);

UnLuaNova::CallLuaFunctionVoid(MyActor, TEXT("Reset"));
UnLuaNova::CallLuaFunctionIfExist<void>(MyActor, TEXT("OnPickedUp"), Picker);
```

要点：

* `self` 是**第一个参数**，其余参数依次排在其后（标准 Lua 方法调用语义）。
* 返回值取第一个结果，按声明类型经 `FLuaValue::Value<Ret>()` 转换。
* 对象未绑定会先 `TryHookLua`。`NotifyUObjectCreated` 虽然对每个对象都试过一次，
  但它发生在实例属性从原型拷贝**之前**，靠实例属性指路的模块那时还读不到。
* 环境按对象取（`ULuaEnvLocator::Locate`），**不是**主环境 —— `ByGameInstance` 下拿错
  会拿到另一个世界的 self 表。
* 入口处装了 `FDeadLoopCheck` 与 `FDanglingCheck` 的 guard，与 `FLuaEnv::DoString` 一致。

**实现刻意做得很薄**：查找直接复用 `PushFunction()` —— 也就是覆写路径用的那个函数 ——
所以它天然共享同一套 self 表、Super 链与 `ReportLuaCallError`。"C++ 能调"和"Lua 能调"
因此在构造上等价，不会各自漂移。早先版本手写了 metatable 遍历，立刻就开始和
`PushFunction` 的语义脱节了。

底层两步入口（想把参数编组自己接过去时用）：

```cpp
UnLuaNova::FLuaMemberCall Call = UnLuaNova::BeginLuaMemberCall(Obj, TEXT("EchoSelf"), false);
if (Call.IsValid())
{
    // 栈此时是 [message handler][function][self]，PushArgs 压在其上
    auto Values = UnLuaNova::EndLuaMemberCall(Call, /*NumArgs=*/1);   // NumArgs 含 self
}
```

## 真正剩下的缺口：RPC 不可覆写

再看一遍 `IsOverridable` 的第二个分支：

```cpp
(Function->FunctionFlags & (FUNC_Native | FUNC_Event | FUNC_Net)) == (FUNC_Native | FUNC_Event)
```

**`FUNC_Net` 被明确排除**。这不是从 slua 抄来的判断，是我们自己代码里的事实：

* `FunctionDesc.cpp:43` 只给已存在的 Net 函数改名成 `Xxx_RPC`；
* 覆写系统根本不会碰带 `FUNC_Net` 的函数。

后果是：**Lua 只能覆盖引擎里已经声明好的 RPC，不能声明新的 RPC，也不能声明复制属性。**
这才是值得做的事，而且它比"再补十个基类"重要得多 —— 因为它决定了 Lua 能不能独立表达网络行为。

方向（尚未实现）：RPC 函数走 `ULuaFunction`（它本来就是 `UFunction` 子类、已有
`execScriptCallLua`），挂进现有的 `ULuaOverridesClass`，退出 PIE 时由 `RestoreOverrides`
一并还原 —— **不需要**像 slua 那样在运行时给 UClass 动态加函数再配一套清理逻辑。
动手前先验证一个未知数：构建期生成的 RPC UFunction 在 cooked 包里能不能保住 Net flag。

---

## 延伸阅读

- [如何实现覆写](How_To_Implement_Overriding.md) — 覆写系统与 `self.Overridden`
- [C++ API 参考](CxxApi.md) — `FLuaEnv`、覆写类型、`UnLuaNovaOverrider`
- [架构](Architecture.md) — 绑定、覆写与静态绑定的完整链路
- [对标 slua-unreal：差距与吸收](Comparison_sluaunreal.md) — 上面这些结论是怎么得出的

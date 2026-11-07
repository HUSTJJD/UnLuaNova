# Lua 声明式网络（实验）

支持声明 RPC 和带条件的复制字段。

**可用的条件**：`None`、`InitialOnly`、`OwnerOnly`、`SkipOwner`、`SimulatedOnly`、`AutonomousOnly`、`SimulatedOrPhysics`、`InitialOrOwner`。每种条件有独立的存储组，字段落进哪个组就按哪个条件发送。

**被拒绝的条件**：所有没有存储组的条件——`Custom` / `Dynamic`（引擎不支持子对象上的自定义条件）、`Never` / `NetGroup`，以及 Replay 系列（`ReplayOrOwner`、`ReplayOnly`、`SimulatedOnlyNoReplay`、`SimulatedOrPhysicsNoReplay`、`SkipReplay`）。这些名字能被解析（因此"拼错名字"和"条件不被支持"是两条不同的报错），但声明时整份拒绝，**不会降级成 None**——把一个 OwnerOnly 的值悄悄发给所有连接比拒绝复制更糟。

int64 已接入精确编组和输入校验。声明式网络是实验特性，不作为生产或 Iris 兼容保证。

## RPC 声明

```lua
local M = UnLuaNova.Class()
M.ServerRPC = {
    Ping = true,
    Chat = { Reliable = true, Params = { "Int", "String" } },
}
M.ClientRPC = { ShowDamage = true }
M.MulticastRPC = { Explode = true }
function M:Ping() end
function M:Chat(Channel, Text) end
return M
```

简写默认为不可靠。参数可写类型名，或 `{ Name = "Channel", Type = "Int" }`。内置类型仍是 Bool、Int/Int32、Int64、Float、Double、String/FString、Name、Vector、Rotator、Transform、Object；除此之外 `Type` 还能写**类型规格**：

| 写法 | 含义 |
|---|---|
| `Struct:/Script/Engine.Vector` 或 `/Script/Engine.Vector` | 任何可解析的 `UScriptStruct` |
| `Array:<spec>` | `TArray` |
| `Set:<spec>` | `TSet` |
| `Map:<keyspec>:<valuespec>` | `TMap`（键必须是可哈希规格） |

`UClass` 不能在运行时长出 `UPROPERTY`，但 RPC 的 `UFunction` 本来就是运行时构造的，`CreateParam` 可以直接挂上 `FStructProperty` / `FArrayProperty` / `FSetProperty` / `FMapProperty`，随后走 UnLua 自己的 `FFunctionDesc` 编组；复制字段也不需要新 `UPROPERTY`，它走一个共享 Generic 桶里的自描述文本 payload。规格参与 `SchemaVersion`，声明不一致的两端不会误解码。`UObject` 引用仍走专用 Object 桶——文本 payload 带不了 NetGUID。

规格在绑定期整份校验：未知关键字、缺失子规格、不可解析的 `UScriptStruct`、不可哈希的 Map 键、嵌套过深都会拒绝，不会留下无法解码的 payload。

参数列表必须连续，未知类型、重复参数名、错误 Params/Reliable 类型会拒绝整个 RPC。参数构造失败也不会注册一个缺参数的函数。

声明构造带网络 flags 的 UFunction，交给现有覆写链路。RPC 名称已按种类内排序注册，减少 Lua table 遍历顺序差异；继承、已有函数、NetCache、热重载及 cooked 环境中的 ID 一致性不在保证范围内。

### 连接前预绑定

动态 RPC 必须在两端的 NetDriver 首次缓存类之前注册。等到客户端收到 Actor、再由实例创建触发绑定会太晚：引擎可能已在解析类/模板的网络校验信息时缓存了不含 RPC 的字段表。将已加载的网络类加入项目 `Config/DefaultUnLuaNova.ini`：

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+PreBindClasses=/Script/MyGame.MyNetworkActor
```

预绑定在 Lua 启用时执行，并在引擎初始化完成后再次检查，覆盖 Default 阶段模块注册的原生类。配置只解析已加载类，不主动加载蓝图资产；蓝图网络类必须在连接前加载并完成绑定。服务端与客户端应使用相同声明，网络会话期间不能更改 RPC 布局。示例工程为 `UnLuaNovaTestNetActor` 配置了此项。

## 复制字段

```lua
function M:GetLifetimeReplicatedProps()
    return {
        { Name = "HP", Type = "Float" },
        { Name = "DisplayName", Type = "String" },
    }
end

function M:Initialize()
    self.Replication = self:AddComponent(UE.UUnLuaNovaReplicationComponent)
end

function M:TakeDamage(amount)
    self.HP = self.HP - amount
    self.Replication:MarkFieldDirty("HP")
end

function M:OnRep_HP()
    self:UpdateHealthBar()
end
```

也支持 `M.ReplicatedProps` 静态表。Actor 及组件的复制、所有权和网络生命周期必须正确配置。源码按 11 种内置类型分桶，另有第 12 个共享 `Generic` 桶承载类型规格字段，**字段下标在 (类型, 条件) 内按声明顺序分配**——不同条件的字段各从 0 开始。SchemaVersion 指纹包含字段名、顺序、类型、类型规格和条件，用于检测声明差异；32 位指纹不是网络安全校验或完整版本协商。

属性声明要求连续数组、唯一且非 None 的 FName、已知类型或合法类型规格；`Condition` 可省略（等于 `None`）或写上面列出的任一条件名。字段下标在**（类型, 条件）**内计数：两个 OwnerOnly 的 String 和一个无条件的 String 各自从下标 0 开始，因为它们落在不同组。首次空声明有效；缓存中的类 schema 不允许改写，完全相同的重复声明幂等。校验失败不会部分注册，也不会替换此前有效 schema。更改字段布局需要完整网络/环境重建，不能依靠热重载。全局声明缓存按**共同 owner 集合**释放：多个 Env 绑定同一份完全相同的声明时，各自登记为共同 owner，
`Cleanup(Env)` 只从集合中移除自己，最后一个关闭的 Env 才删除该 schema，任意关闭顺序都不会抹掉仍在用的
声明；无主条目仍由进程级 `Cleanup(nullptr)` 负责。

`UpdateMode=Polled` 默认每 0.1 秒读取 Lua 字段；`Explicit` 需要调用 `MarkFieldDirty`。还提供 `MarkAllFieldsDirty`、`FlushNetUpdate`、`OnRep_<Name>` 和 `OnReplicatedProps(Names)`。显式模式漏标不会发送改动，Flush 请求也不是同帧抵达保证。

## 必须知道的限制

- 组件上的 `Condition` 属性已废弃：它只用于识别旧资产，非 None 时打一条 Warning 并忽略，**不再禁用复制**；逐字段条件在 `GetLifetimeReplicatedProps` 里声明。
- 条件组的脏标记粒度是"整组"而不是"每个类型"：一个条件组是一个 UPROPERTY，改组内任一字段会重发整组。无条件字段仍走组件上按类型拆分的数组，保持按类型脏标记。owner / 非 owner 的联机行为需单独验收。
- int32/int64 读取拒绝小数、NaN、无穷、越界和数字字符串，失败保留旧值。Luau 在 ±2^53 内推入 number，范围外推入原生 integer64；脚本在 number 中已发生的舍入无法恢复，超出精确范围的值应来自 integer64/C++，不能以浮点字面量替代。
- 数学类型要求对应 userdata，Object 不提供任意子类约束；对象跨连接的失效行为与晚加入场景需单独验收。
- 类型规格字段走文本 payload，不是引擎的逐属性 delta；它证明的是"能声明、能往返、两端不会误解码"，不是带宽或压缩收益。`UObject` 引用不在规格语法内。
- 带类型的 UPROPERTY 和 push-model 宏不保证 Iris、逐元素 delta、压缩收益或运行时回退语义。Legacy / PushOff / Iris / Cooked 四种打包变体需单独验收，不宣称压缩收益。
- 条件组的脏标记粒度是"整组"而非"每个类型"：一个组是一个 UPROPERTY，组内任一字段变化都会重发整组；无条件字段仍保持按类型脏标记。
- 热重载与权限检查需单独验收；跨 peer 声明差异由 `RepSchemaVersion` 报告导出检测。

[测试架构](Testing_Architecture.md) · [对象绑定](LuaDrivenObjects.md)

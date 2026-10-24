# Lua 声明式网络（实验）

当前源码支持声明 RPC 和有限类型的复制字段，但**条件复制与 int64 路径存在正确性阻塞**。本页说明现有 API，不作为生产或 Iris 兼容保证。风险与验收统一见 [审计 NET-01 / NET-02](Benchmark_Review.md)。

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

简写默认为不可靠。参数可写类型名，或 `{ Name = "Channel", Type = "Int" }`。当前支持 Bool、Int/Int32、Int64、Float、Double、String/FString、Name、Vector、Rotator、Transform、Object；不是任意 USTRUCT/容器。

声明构造带网络 flags 的 UFunction，交给现有覆写链路。RPC 名称已按种类内排序注册，减少 Lua table 遍历顺序差异；这不证明继承、已有函数、NetCache、热重载及 cooked 环境中的 ID 一致。

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

也支持 `M.ReplicatedProps` 静态表。Actor 及组件的复制、所有权和网络生命周期必须正确配置。源码按 11 种类型分桶，字段顺序决定桶下标。SchemaVersion 指纹包含字段名、顺序、类型和条件，用于检测声明差异；32 位指纹不是网络安全校验或完整版本协商。

`UpdateMode=Polled` 默认每 0.1 秒读取 Lua 字段；`Explicit` 需要调用 `MarkFieldDirty`。还提供 `MarkAllFieldsDirty`、`FlushNetUpdate`、`OnRep_<Name>` 和 `OnReplicatedProps(Names)`。显式模式漏标不会发送改动，Flush 请求也不是同帧抵达保证。

## 必须知道的限制

- 条件不是逐字段实现：同类型字段共用条件，冲突取首个非 None 并警告。更严重的是，注册条件依赖实例 owner，而 UE 复制布局按类缓存。当前不能依靠 OwnerOnly 等条件保护私有字段。
- 未知条件回退为 None，尚需改为明确拒绝。
- Luau int64 网络读取使用 int32 整数 API，极值可能截断；专用 int64 绑定不能自动覆盖这条路径。
- 数学类型要求对应 userdata，Object 不提供任意子类约束；对象解析、失效和晚加入必须验收。
- 带类型的 UPROPERTY 和 push-model 宏存在，不等于已证明 Iris、逐元素 delta、压缩收益或运行时回退语义。
- 热重载、继承、重复声明、不同客户端 schema、权限检查仍需独立测试。

## 已有测试入口

| 用例 | 检查目标 | 证据限制 |
|---|---|---|
| UnLuaNova.API.Net | 声明解析、flags、还原、排序、schema 条件指纹 | 不验证实际传输 |
| UnLuaNova.API.Net.ClientServer | PIE 中 RPC / 字段传输 | 单进程共享类状态 |
| UnLuaNova.API.Net.DedicatedServer | 独立进程声明结果/ID 对照 | 不是相互连接的传输测试 |
| UnLuaNova.Slow.Net.Multicast | server + 两个连接的 client | 本轮未执行；Fast 层排除 |

需要补充 owner/非 owner、多 actor 混合条件、int64 边界、晚加入、断线重连、dormancy/relevancy、丢包及 legacy/Iris/push model 的打包矩阵。存在测试源码不等于测试已通过。

[测试架构](Testing_Architecture.md) · [对象绑定](LuaDrivenObjects.md) · [能力矩阵](../feature-matrix.md)

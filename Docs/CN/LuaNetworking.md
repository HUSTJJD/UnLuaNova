# Lua 声明式网络（实验）

当前源码支持声明 RPC 和有限类型的复制字段。**条件复制暂不支持，非 None 条件整份拒绝**；int64 已接入精确编组和输入校验，等待双后端编译、运行验证。本页不作为生产或 Iris 兼容保证。风险与验收统一见 [审计 NET-01 / NET-02](Benchmark_Review.md)。

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

参数列表必须连续，未知类型、重复参数名、错误 Params/Reliable 类型会拒绝整个 RPC。参数构造失败也不会注册一个缺参数的函数。

声明构造带网络 flags 的 UFunction，交给现有覆写链路。RPC 名称已按种类内排序注册，减少 Lua table 遍历顺序差异；这不证明继承、已有函数、NetCache、热重载及 cooked 环境中的 ID 一致。

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

也支持 `M.ReplicatedProps` 静态表。Actor 及组件的复制、所有权和网络生命周期必须正确配置。源码按 11 种类型分桶，字段顺序决定桶下标。SchemaVersion 指纹包含字段名、顺序、类型和条件，用于检测声明差异；32 位指纹不是网络安全校验或完整版本协商。

属性声明要求连续数组、唯一且非 None 的 FName、已知类型，条件只能省略或写 `"None"`。首次空声明有效；缓存中的类 schema 不允许改写，完全相同的重复声明幂等。校验失败不会部分注册，也不会替换此前有效 schema。更改字段布局需要完整网络/环境重建，不能依靠热重载。现有全局声明缓存的多 Env 清理归属仍需完善。

`UpdateMode=Polled` 默认每 0.1 秒读取 Lua 字段；`Explicit` 需要调用 `MarkFieldDirty`。还提供 `MarkAllFieldsDirty`、`FlushNetUpdate`、`OnRep_<Name>` 和 `OnReplicatedProps(Names)`。显式模式漏标不会发送改动，Flush 请求也不是同帧抵达保证。

## 必须知道的限制

- 组件复制布局固定为 None，不再从实例 owner 推导；OwnerOnly 等声明被拒绝。旧资产的组件 `Condition` 非 None 时，BeginPlay 禁用其复制。完整的逐字段条件布局及 owner/非 owner 联机验收仍是 P0。
- int32/int64 读取拒绝小数、NaN、无穷、越界和数字字符串，失败保留旧值。Luau 在 ±2^53 内推入 number，范围外推入原生 integer64；脚本在 number 中已发生的舍入无法恢复，超出精确范围的值应来自 integer64/C++，不能以浮点字面量替代。
- 数学类型要求对应 userdata，Object 不提供任意子类约束；对象解析、失效和晚加入必须验收。
- 带类型的 UPROPERTY 和 push-model 宏存在，不等于已证明 Iris、逐元素 delta、压缩收益或运行时回退语义。
- 热重载、继承、重复声明、不同客户端 schema、权限检查仍需独立测试。

## 已有测试入口

| 用例 | 检查目标 | 证据限制 |
|---|---|---|
| UnLuaNova.API.Net | 声明拒绝/幂等、flags、还原、排序、schema 指纹、实际桶编组的 int64 边界 | 双后端 Editor 已通过；不验证实际传输 |
| UnLuaNova.API.Net.ClientServer | PIE 中 RPC / 字段传输 | 单进程共享类状态 |
| UnLuaNova.API.Net.DedicatedServer | DS + 两个独立客户端、仅服务端生成两个 Actor 和设置 Owner、两轮 Server/Client/Multicast RPC 与 Lua HP 同步 | 新场景待运行；Release22 及以前仅为未连接的声明/ID 对照，不构成此项证据 |
| UnLuaNova.Slow.Net.Multicast | server + 两个连接的 client | 双后端 Editor 完整诊断轮已通过；Fast 层排除 |

以上执行状态对应 `FullFix-Complete-11`，打包环境与完整发布矩阵的最终结论见 [回归验证记录](Regression_20261024.md)。

新 DedicatedServer 场景的 SessionId 和每个 Actor 的随机 challenge 仅由服务端生成，经 Actor 复制到客户端。客户端不得本地生成测试 Actor 或设置 Owner；两个客户端分别从自己的 Actor 发出 Server RPC，服务端处理函数修改 Lua HP，再发送定向 Client RPC 和广播。两客户端分别经 Server RPC 确认第一轮的两个 Actor 均验证完成，服务端再经 Client RPC 允许第二轮开始；这样慢客户端不会因较快客户端提前改值而错过第一轮。Client1 固定延后至少 5 秒检查第一轮，原始等待时长进入门禁。严格比较逐端调用日志，非 Owner 不得收到 Client RPC。JSON 文件只用于结果和结束同步，不传递测试输入。

`ValidateRelease.py` 要求相同会话、真实连接、三个不同 PID、两个不同 Owner、两轮原始观测及准确的 RPC 参数/执行次数。仅编号一致或 Standalone 报告不能通过。开发诊断支持 `-UnLuaNovaNetFault=SkipServerRpc|SkipClientRpc|SkipReplication`，用于验证断开相应路径时确实失败；故障运行不得用于验收。

仍需要补充非 Owner 发起非法 Server RPC 的拒绝、多 actor 混合条件、int64 边界、晚加入、断线重连、dormancy/relevancy、丢包及 legacy/Iris/push model 的打包矩阵。存在测试源码不等于测试已通过。

[测试架构](Testing_Architecture.md) · [对象绑定](LuaDrivenObjects.md) · [能力矩阵](../feature-matrix.md)

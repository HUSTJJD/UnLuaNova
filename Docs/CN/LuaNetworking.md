# Lua 声明式网络（实验）

当前源码支持声明 RPC 和带条件的复制字段。

**可用的条件**：`None`、`InitialOnly`、`OwnerOnly`、`SkipOwner`、`SimulatedOnly`、`AutonomousOnly`、`SimulatedOrPhysics`、`InitialOrOwner`。每种条件有独立的存储组，字段落进哪个组就按哪个条件发送。

**被拒绝的条件**：所有没有存储组的条件——`Custom` / `Dynamic`（引擎不支持子对象上的自定义条件）、`Never` / `NetGroup`，以及 Replay 系列（`ReplayOrOwner`、`ReplayOnly`、`SimulatedOnlyNoReplay`、`SimulatedOrPhysicsNoReplay`、`SkipReplay`）。这些名字能被解析（因此"拼错名字"和"条件不被支持"是两条不同的报错），但声明时整份拒绝，**不会降级成 None**——把一个 OwnerOnly 的值悄悄发给所有连接比拒绝复制更糟。

int64 已接入精确编组和输入校验。本页不作为生产或 Iris 兼容保证。风险与验收统一见 [审计 NET-01 / NET-02](Benchmark_Review.md)。

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

之前把"任意 USTRUCT / 容器"判为架构边界是错的：`UClass` 确实不能在运行时长出 `UPROPERTY`，但 RPC 的 `UFunction` 本来就是运行时构造的，`CreateParam` 可以直接挂上 `FStructProperty` / `FArrayProperty` / `FSetProperty` / `FMapProperty`，随后走 UnLua 自己的 `FFunctionDesc` 编组；复制字段也不需要新 `UPROPERTY`，它走一个共享 Generic 桶里的自描述文本 payload。规格参与 `SchemaVersion`，声明不一致的两端不会误解码。`UObject` 引用仍走专用 Object 桶——文本 payload 带不了 NetGUID。

规格在绑定期整份校验：未知关键字、缺失子规格、不可解析的 `UScriptStruct`、不可哈希的 Map 键、嵌套过深都会拒绝，不会留下无法解码的 payload。

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

也支持 `M.ReplicatedProps` 静态表。Actor 及组件的复制、所有权和网络生命周期必须正确配置。源码按 11 种内置类型分桶，另有第 12 个共享 `Generic` 桶承载类型规格字段，**字段下标在 (类型, 条件) 内按声明顺序分配**——不同条件的字段各从 0 开始。SchemaVersion 指纹包含字段名、顺序、类型、类型规格和条件，用于检测声明差异；32 位指纹不是网络安全校验或完整版本协商。

属性声明要求连续数组、唯一且非 None 的 FName、已知类型或合法类型规格；`Condition` 可省略（等于 `None`）或写上面列出的任一条件名。字段下标在**（类型, 条件）**内计数：两个 OwnerOnly 的 String 和一个无条件的 String 各自从下标 0 开始，因为它们落在不同组。首次空声明有效；缓存中的类 schema 不允许改写，完全相同的重复声明幂等。校验失败不会部分注册，也不会替换此前有效 schema。更改字段布局需要完整网络/环境重建，不能依靠热重载。全局声明缓存按**共同 owner 集合**释放：多个 Env 绑定同一份完全相同的声明时，各自登记为共同 owner，
`Cleanup(Env)` 只从集合中移除自己，最后一个关闭的 Env 才删除该 schema，任意关闭顺序都不会抹掉仍在用的
声明；无主条目仍由进程级 `Cleanup(nullptr)` 负责。两个反向顺序已有单测，联机执行仍待补。

`UpdateMode=Polled` 默认每 0.1 秒读取 Lua 字段；`Explicit` 需要调用 `MarkFieldDirty`。还提供 `MarkAllFieldsDirty`、`FlushNetUpdate`、`OnRep_<Name>` 和 `OnReplicatedProps(Names)`。显式模式漏标不会发送改动，Flush 请求也不是同帧抵达保证。

## 必须知道的限制

- 组件上的 `Condition` 属性已废弃：它只用于识别旧资产，非 None 时打一条 Warning 并忽略，**不再禁用复制**；逐字段条件在 `GetLifetimeReplicatedProps` 里声明。
- 条件组的脏标记粒度是"整组"而不是"每个类型"：一个条件组是一个 UPROPERTY，改组内任一字段会重发整组。无条件字段仍走组件上按类型拆分的数组，保持按类型脏标记。**owner / 非 owner 的联机验收仍是 P0**（断言源码与门禁已补、尚未执行）——单进程用例只能证明分组与下标，证明不了"非属主确实没收到"。
- int32/int64 读取拒绝小数、NaN、无穷、越界和数字字符串，失败保留旧值。Luau 在 ±2^53 内推入 number，范围外推入原生 integer64；脚本在 number 中已发生的舍入无法恢复，超出精确范围的值应来自 integer64/C++，不能以浮点字面量替代。
- 数学类型要求对应 userdata，Object 不提供任意子类约束；对象解析与弱引用失效已有单测，
  对象跨连接失效的联机观测仍属于上面"尚未执行"的部分。晚加入已有场景源码与门禁，但尚未执行。
- 类型规格字段走文本 payload，不是引擎的逐属性 delta；它证明的是"能声明、能往返、两端不会误解码"，不是带宽或压缩收益。`UObject` 引用不在规格语法内。
- 带类型的 UPROPERTY 和 push-model 宏存在，不等于已证明 Iris、逐元素 delta、压缩收益或运行时回退语义。Legacy / PushOff / Iris / Cooked 四个变体的启动开关、真实连接字节/丢包计数与门禁已补源码，但没有任何一个变体跑过，也不宣称压缩收益。
- 条件组的脏标记粒度是"整组"而非"每个类型"：一个组是一个 UPROPERTY，组内任一字段变化都会重发整组；无条件字段仍保持按类型脏标记。
- 热重载与权限检查仍需独立验收；单进程已覆盖继承查找、重复声明幂等、多 Env 任意顺序销毁、弱对象失效与 schema 不匹配时不写 Lua 字段，**跨 peer** 声明差异已由 `RepSchemaVersion` 报告导出与门禁覆盖（尚未执行）。

## 已有测试入口

| 用例 | 检查目标 | 证据限制 |
|---|---|---|
| UnLuaNova.API.Net | 声明拒绝/幂等、flags、还原、排序、schema 指纹、实际桶编组的 int64 边界 | 双后端 Editor 已通过；不验证实际传输 |
| UnLuaNova.API.Net.ClientServer | PIE 中 RPC / 字段传输 | 单进程共享类状态 |
| UnLuaNova.API.Net.DedicatedServer | DS + 两个独立客户端、仅服务端生成两个 Actor 和设置 Owner、两轮 Server/Client/Multicast RPC 与 Lua HP 同步 | Release28 四个载体及完整 33 阶段均通过，含原始会话/PID/HP/逐端日志校验；Release22 及以前仅为未连接的声明/ID 对照 |
| UnLuaNova.Slow.Net.Multicast | server + 两个连接的 client | 双后端 Editor 完整诊断轮已通过；Fast 层排除 |
| UnLuaNova.Slow.Net.Scenarios.{LateJoin,Reconnect,Dormancy,Relevancy,JoinOrderSwap,PacketLoss,SeamlessReconnect,SchemaMismatch} | 八类连接场景各起 Server + Client1 + Client2 | 需 `-UnLuaNovaNetScenarios` 显式开启；源码与门禁已补、尚未执行 |
| UnLuaNova.API.Net.Scenarios.Report | 上述场景的 peer 侧报告 | 只由场景驱动拉起，不单独跑 |

API.Net、ClientServer、Multicast 的历史诊断见 `FullFix-Complete-11`；替换后的 DedicatedServer 及完整发布矩阵结论见 [回归验证记录](Regression_20261024.md)。

新 DedicatedServer 场景的 SessionId 和每个 Actor 的随机 challenge 仅由服务端生成，经 Actor 复制到客户端。客户端不得本地生成测试 Actor 或设置 Owner；两个客户端分别从自己的 Actor 发出 Server RPC，服务端处理函数修改 Lua HP，再发送定向 Client RPC 和广播。两客户端分别经 Server RPC 确认第一轮的两个 Actor 均验证完成，服务端再经 Client RPC 允许第二轮开始；这样慢客户端不会因较快客户端提前改值而错过第一轮。Client1 固定延后至少 5 秒检查第一轮，原始等待时长进入门禁。严格比较逐端调用日志，非 Owner 不得收到 Client RPC。JSON 文件只用于结果和结束同步，不传递测试输入。

`ValidateRelease.py` 要求相同会话、真实连接、三个不同 PID、两个不同 Owner、两轮原始观测及准确的 RPC 参数/执行次数。仅编号一致或 Standalone 报告不能通过。开发诊断支持 `-UnLuaNovaNetFault=SkipServerRpc|SkipClientRpc|SkipReplication`，用于验证断开相应路径时确实失败；故障运行不得用于验收。

**已补源码、尚未执行**：

- 非 Owner 发起 Server RPC 的联机断言（`NetActor.lua` / `LuaNetDedicatedServerTest.cpp` 用唯一 token 发起一次非属主调用，`ValidateRelease.py` 要求"尝试确实发生"且 token 不出现在任何 peer 的 `CallLog`）。
- 混合条件字段（`HP` 无条件 + `OwnerSecret` 为 `OwnerOnly`，逐端导出可见性由门禁断言）。
- 任意 `UStruct` / 容器：RPC 参数动态 FProperty、复制字段 Generic 桶的规范化文本 payload，以及解析 / 嵌套 / 往返单测。`NetActor.lua` 里另加了一个真实的 `Inventory`（`Array:Int`）字段和一个带
  `Struct:/Script/CoreUObject.Vector` 参数的 `SendPayload` RPC；声明报告按**真实 FProperty**导出参数规格，
  门禁要求三端都有 Generic 字段与 UStruct 参数。注册成功本身就是断言——`CreateParam` 失败的话这个 RPC
  根本不会出现。单测源码已补，但未执行。
- 跨 peer 声明一致性：每份 peer 报告导出实际绑定的 `RepSchemaVersion` 和一份故意不同的
  `VariantRepSchemaVersion`；门禁要求三端前者相同、后者与前者不同，否则"全都相等"可能只是因为指纹是常数。
- 八类连接场景（`LuaNetNetScenariosTest.cpp`）：晚加入、进程级重连、dormancy、relevancy、
  **加入顺序互换**（Client2 先连、Client1 后连，拓扑 `launch_serial` 必须反过来）、
  **丢包与乱序**（`NetEmulation.PktLoss/PktOrder/PktLag` 真实引擎模拟，reliable 复制必须收敛到最终值）、
  **`UNetConnection` 级重连**（服务端 `Connection->Close()`，同一进程用 `-UnLuaNovaNetReconnectUrl` 重开 URL；
  门禁要求 `launch_count=1` 且 `first_pid == pid`，与进程级重连明确区分）、
  **跨 peer 声明差异**（三端导出声明指纹并互相比对）。
- `Tools/RunNetMatrix.ps1` + `ValidateRelease.py --net-matrix`：Legacy / PushOff / Iris / **Cooked**
  四个变体。前三者传真实引擎开关（`-UseIrisReplication=1` 或 push-model cvar 的 ini 覆盖）；
  Cooked 把 `UNLUANOVA_TEST_PEER_EXE` / `_SERVER_EXE` 指向 staged `Windows` / `WindowsServer` 二进制，
  报告用编译期 `WITH_EDITOR` 回读。`AppendWorldFacts` 还导出 `UNetConnection` 的真实字节数、丢包数与
  cooked / Iris / push-model 状态，Iris 逐元素 delta 可从这些报告做变体间对比（**不宣称压缩收益**）。
- 多 Env 声明/销毁顺序：`FUnLuaNovaNet` 改为按**共同 owner 集合**释放——多个 Env 绑定同一份声明时，
  最后一个关闭才删除，任意关闭顺序都不会抹掉仍在用的 schema；已补两个反向顺序的单测。
- 对象失效：弱 `UObject` 引用失效后不会把野指针写回 Lua，已补单测。

**以上全部必须重新构建并跑真实 DS 才算证据——存在测试源码不等于测试已通过。** 入口是
`Tools/RunTests.ps1 -Profile Development -NetMatrix`（底层脚本 `Tools/RunNetMatrix.ps1`）。
每格都会起三个真实进程，因此它不进普通测试矩阵，也不进常规 Release 阶段。

**源码层面已无待实现项**。剩下的全部是执行：八类场景与四个变体一个都没跑过；
`release-status` 已由完整 Release 改为 `passed`，但 `NET-*` 仍是 `experimental`，
`verification` 未手改；常规矩阵通过不等于下面的联机验收完成。
seamless travel（`ServerTravel` / `TravelType`）本身不在实现范围内——这里交付的是
`UNetConnection` 级关闭 + 进程内重连，两者是不同的引擎路径，文档不把它们混为一谈。

## 什么时候才能摘掉"实验"标签

`NET-RPC` / `NET-PROPS` / `NET-TYPES` 在能力矩阵里仍是 `experimental`。按仓库自己的定义，
`experimental` 是"不是 release 保证"，`partial` 是"有已知缺口"，`implemented` 是"源码在既定边界内存在"。
`status` 允许人工填（只有 `verification` 禁止手改），但**只用证据换，不用文字换**。升级到 `partial`
需要同时满足：

1. 重新构建 Editor / Game / Server，跑一次完整 `Tools/RunTests.ps1`，产出新的
   `Saved/ReleaseTests/baseline/`（或等价的完整运行）。
2. `ValidateRelease.py` 的 DedicatedServer 门禁全绿，且包含上面两项新断言：非 Owner Server RPC 的
   token 未在任何 peer 执行、`OwnerOnly` 字段只对属主可见。
3. 用 `RecordEvidence.py --commit <该轮提交>` 把该轮 Editor / Game automation 报告归集进
   `Tools/evidence/`，且记录不带 `historical`。
4. `ValidateRelease.py --net-matrix` 全绿：八份场景拓扑 + 四份变体拓扑，每份都有三个真实
   peer 报告、非空日志、与拓扑一致的 PID、一致且可区分的声明指纹、Generic 字段 / UStruct 参数与
   非零连接字节计数；变体还要有从 cvar 读回的真实 Iris / push-model 状态，Cooked 还要有 staged 二进制路径
   与 `WITH_EDITOR` 编译期标志。
5. `release-status` marker 已由一次真实通过的完整 Release 运行更新为 `passed`。
   这只说明常规 Release 矩阵通过；第 4 项的八场景 / 四变体联机验收仍未执行，
   不能据此把 `NET-*` 升为 `partial`。

在 1–5 完成前，README 里的网络能力必须保留"（实验）"标签与硬边界。`status` 保持 `experimental`
不是遗漏，而是"源码已交付、执行未验收"的如实标注。

[测试架构](Testing_Architecture.md) · [对象绑定](LuaDrivenObjects.md) · [能力矩阵](../feature-matrix.md)

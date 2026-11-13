---
title: 声明式网络正式使用指南
description: Lua 声明式网络的完整示例、策略配置、RPC 鉴权、热重载 schema 迁移、Seamless Travel、错误码与验收清单。
---

# 声明式网络正式使用指南

> **状态说明**：本页描述正式发布后的目标契约与本轮新增的策略层用法。
> 当前该能力仍处于待晋升状态，最新 Release 为 `passed=false`，联网矩阵未跑全。
> 生产启用前必须先满足 [发布就绪度](Release_Readiness.md) 的 L2–L4。
> 参考语法见 [Lua 声明式网络](LuaNetworking.md)。

## 1. 完整示例

```lua
-- Content/Script/MyNetActor.lua
local M = UnLuaNova.Class()

M.ServerRPC = {
    RequestDamage = { Reliable = true, Params = { "Int", "String" } },
}
M.ClientRPC    = { ShowDamage = true }
M.MulticastRPC = { Explode = true }

function M:GetLifetimeReplicatedProps()
    return {
        { Name = "HP",     Type = "Float" },
        { Name = "Secret", Type = "String", Condition = "OwnerOnly" },
    }
end

function M:Initialize()
    self.Replication = self:AddComponent(UE.UUnLuaNovaReplicationComponent)
end

function M:RequestDamage(Amount, Source) end
function M:OnRep_HP() self:UpdateHealthBar() end

return M
```

工程侧 `Config/DefaultUnLuaNova.ini` 必须把网络类加入预绑定，否则 NetDriver 可能先缓存到
不含 RPC 的类：

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
+PreBindClasses=/Script/MyGame.MyNetActor
bDeclaredNetworking=True
```

启动期会自检这些路径：无法解析的类会打 `LogUnLuaNova` Warning，而不是等到联机时才发现 RPC 缺失。

## 2. 四个策略开关

| 键 | 默认 | 用途 |
|---|---|---|
| `bDeclaredNetworking` | `True` | 总开关。关掉后 `DeclareRPCs` / `DeclareReplicatedProps` 直接跳过，不再构造动态 `UFunction` |
| `bRequireRpcAuthorization` | `False` | 要求安装应用层鉴权处理器；开启但未安装是配置错误，策略层 **fail-closed** 拒绝 |
| `bAllowHotReloadSchemaChange` | `False` | 允许热重载改变复制布局，新布局作为「待协商迁移」暂存 |
| `bPreserveSchemaAcrossSeamlessTravel` | `True` | travel 期间保留已注册 schema 并递增代际，阻止旧世界 RPC ID 复用 |

脚本查询：`UnLuaNova.HasFeature('declarednetworking')`。

## 3. RPC 鉴权

引擎只挡「非 owner 调用 Server RPC」。应用层策略由宿主安装；`CanInvoke` 已接入
`FFunctionRegistry::Invoke` 的实际 RPC dispatch 路径（按 `FUNC_Net*` 判定种类），
声明式 RPC 到达 Lua 处理函数之前都会经过它：

```cpp
#include "UnLuaNovaNetPolicy.h"

UnLuaNova::FUnLuaNovaNetPolicy::Get().SetAuthorizationHandler(
    [](const UObject* Target, const FName& RpcName, UnLuaNova::ELuaRpcKind Kind)
        -> UnLuaNova::ELuaNetPolicyResult
    {
        // 示例：按项目自己的权限表判定；引擎的 owner 检查依然独立生效
        return MyProject::IsRpcAllowed(Target, RpcName, Kind)
            ? UnLuaNova::ELuaNetPolicyResult::Ok
            : UnLuaNova::ELuaNetPolicyResult::InvocationDenied;
    });
```

拒绝日志可通过 `SetDeniedLogger` 接管；默认写入 `LogUnLuaNova`，含结果码、原因、对象、
RPC 名、本地/远端 schema 与 generation。

## 4. 热重载下的 schema 变更

1. 设 `bAllowHotReloadSchemaChange=True`。
2. `FLuaEnv::HotReload()` 完成后会调用 `FUnLuaNovaNetPolicy::NotifyModulesReloaded()`：
   generation 递增、未批准的暂存迁移作废。
3. 下一次声明若改变布局，通过 `OnHotReload(Class, NewSchema, Reason)` 暂存为待协商迁移。
4. 所有 peer 必须经 `NegotiateSchema` 接受该迁移后，bucket 才会被应用；否则
   `OnRep_SchemaVersion` 保持拒绝并打 Error。
5. 关闭该开关时，布局变更会被明确拒绝并提示「重建网络环境」，不再静默误解码。

> 底层限制已修：`BeginPlay` 的一次性冻结已改为可重入的 `RefreshDeclaration()`，
> 热重载 / travel 完成时会自动调用 `RefreshAllDeclarations()` 重建槽位、SchemaVersion
> 与 RPC 布局指纹。该改动**尚未编译或运行验证**，见发布就绪度的缺陷清单。

## 5. Seamless Travel

`ServerTravel` 前后由宿主在自己的 travel 回调里各调用一次：

```cpp
UnLuaNova::FUnLuaNovaNetPolicy::Get().OnSeamlessTravel(GetWorld());
// ... travel 完成 ...
UnLuaNova::FUnLuaNovaNetPolicy::Get().CompleteSeamlessTravel();
```

策略层会：递增 generation、pin 住仍有效的类 schema、清理失效弱引用、在 travel 期间拒绝
新的声明（`TravelInProgress`），从而避免旧世界的 RPC ID / 字段下标被新世界复用；
`CompleteSeamlessTravel()` 还会自动调用 `RefreshAllDeclarations()` 为新世界重建声明。
这是**显式入口**而非自动注册的引擎委托——未经验证的委托签名不应进入发布分支；
若需要自动挂接，应在编译验证通过后于 `UGameInstance` 的 world 切换回调里调用上面两行。

## 6. 错误码

| `ELuaNetPolicyResult` | 含义 | 处理 |
|---|---|---|
| `Ok` | 通过 | — |
| `FeatureDisabled` | 总开关关闭 | 检查 `bDeclaredNetworking` |
| `DeclarationDenied` | 类为空或声明被拒 | 检查类路径与预绑定 |
| `InvocationDenied` | 应用层鉴权拒绝 | 检查宿主 handler |
| `SchemaIncompatible` | 两端布局不一致且无批准的迁移 | 统一脚本声明；或走热重载迁移流程 |
| `TravelInProgress` | travel 期间拒绝声明 | 等 `CompleteSeamlessTravel()` |
| `NotOwner` | 非 owner 调用 | 引擎已挡，策略层保留给宿主 |
| `Unauthorized` | 要求鉴权但未安装 handler | 安装 handler 或关闭该开关 |

## 7. 验收清单

- `Tools/CheckPromotion.py` 的 L2–L4 全绿；
- 每后端 8 个 Legacy 场景 + 4 个变体（两组独立，共 12 cell）拓扑与报告齐全；
- **真实故障注入已实现**（本轮）：场景内注入不同指纹并走真实协商路径，门禁强制要求
  `InjectedSchemaVersion` / `SchemaMismatchRejected` / `SchemaMismatchReason`；待运行验证；
- **RPC 布局不一致的显式拒绝已实现**（本轮）：新增 `RpcSchemaVersion` 副本与
  `OnRep_RpcSchemaVersion` / `PushSlots` 守卫；待编译与运行验证；
- 非 owner Server RPC 拒绝、PacketLoss 真实 NetEmulation 收敛、Cooked 使用 staged exe。

## 延伸阅读

[Lua 声明式网络](LuaNetworking.md) · [API 稳定性与版本策略](API_Stability.md) · [发布就绪度与判定标准](Release_Readiness.md) · [生产部署与运维](Production_Deployment.md) · [测试架构](Testing_Architecture.md)

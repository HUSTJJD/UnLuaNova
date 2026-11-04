# 实例级模块绑定（BIND-INSTANCE）

同一个 `UClass` 的不同 `UObject` 实例可以绑定不同的 Lua 模块。类级绑定仍然是默认：
没有显式覆盖的实例继续用 `ULuaModuleLocator` 为该类解析出的模块。

## API

```cpp
UUnLuaNovaManager* Manager = Env->GetManager();

// 类级绑定先建立（接口 / ini / TryBind），否则实例覆盖会拒绝：
Env->TryBind(First);

FString Error;
bool bOk = Manager->BindInstance(Second, TEXT("Weapons.Rifle"), LUA_NOREF, &Error);

// 查询与撤销
Manager->HasInstanceBinding(Second);
Manager->GetInstanceBoundRef(Second);          // 实例模块表 ref，未绑定为 LUA_NOREF
Manager->GetInstanceBoundModuleName(Second);
Manager->UnbindInstance(Second);               // 元表回落到类默认模块
```

蓝图入口：`BindObjectToModule(Object, ModuleName)` / `UnbindObjectFromModule(Object)`，
都返回是否成功；失败原因写在 `LogUnLuaNova`。

```lua
-- 覆盖模块就是普通 Lua 模块，函数按可覆写的 UFunction 名命名
local M = {}
function M:ReceiveBeginPlay() print("rifle instance") end
function M:Initialize(Initializer) end   -- 可选，与类级绑定同契约
return M
```

## 语义与实现要点

- `BindInstance` 会 `require` 模块并做一份**浅拷贝**作为该实例专属的 `REQUIRED_MODULE`。
  不能共用类模块表：`FObjectRegistry::Bind` 会原地给模块表设元表，共用会改掉类默认值。
- 引擎的覆写挂在 `UClass` 上，不是对象上。实例模块带来新的可覆写函数名时，会把这些名字
  并入该类的覆写集合（`ULuaFunction::Override` 对已覆写的函数会短路，蓝图重编译后重复调用
  也是幂等的）。某个实例没有定义该函数时，`FFunctionRegistry::Invoke` 找不到 Lua 函数就
  回落到原 `UFunction`，因此不会串线。
- `FFunctionRegistry` 的 `FuncRef` 缓存是按 `ULuaFunction*`（类级）建的。实例覆盖的对象
  改为**每次按自己的 `INSTANCE` 元表解析**，解析出的临时 ref 调用后立即释放；未覆盖的对象
  仍走原来的缓存路径，默认行为与性能不变。
- 已绑定对象用 `FObjectRegistry::RebindModule` 原地换元表，不 `Unbind`：已经 push 到 Lua 的
  userdata 与 `INSTANCE` 表继续有效。新模块表继承旧的 UObject 元表，元表链保持完整。
- `BindInstance` 会按类级绑定同样的契约调用模块的 `Initialize`，并可传
  `InitializerTableRef`。
- 清理：`NotifyUObjectDeleted` 与 `Cleanup` 会逐条 `luaL_unref` 实例表，避免多 Env 反复
  开关时泄漏。实例表保存在创建它的那个 `FLuaEnv` 的 Manager 里（分发 thunk 用哪个 Env，
  就必须绑到哪个 Env）。

## 约束

- 必须先完成类级绑定才能覆盖；`UClass` 本身不能作为实例绑定目标，模块名不能为空。
- `require` 必须返回 table；实例模块只做浅拷贝，模块级状态不共享。
- 这是 opt-in 能力，默认路径没有任何变化。`BindInstance` 不修改 `ULuaModuleLocator`
  的类级缓存，也不改变"静态绑定优先于动态绑定"的既有优先级。
- 类上新增的覆写是类级资源，`ULuaFunction::RestoreOverrides` 会把它们一起撤掉；
  重新绑定实例时会再次补齐。

## 测试

`Plugins/UnLuaNovaTestSuite/.../Private/Specs/InstanceBinding.spec.cpp`
（`UnLuaNova.API.InstanceBinding`）覆盖：实例覆盖与类默认共存、两个模块交替调用不串线、
重绑立即生效、未覆盖实例回落、解绑后回落与查询 API 复位、非法输入与缺少类级绑定时拒绝。
用例通过真实覆写 thunk（`Stub->Overridable()`）驱动，需要在 UE 里跑一次才会产生执行证据。

相关：[控制台命令](ConsoleCommand.md) · [能力矩阵](../feature-matrix.md) ·
[测试架构](Testing_Architecture.md)

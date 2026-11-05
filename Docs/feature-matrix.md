# Capability and Evidence Matrix

Generated from `Tools/feature-matrix.json`; edit that file and run `python Tools/AuditRepository.py --write-matrix`.

Audit: 2026-10-24. Source audit, not a claim of exhaustive feature or execution coverage.

Execution evidence is recorded per capability under `Tools/evidence/` (13 run(s), covering 36 of 40 capabilities). Those records include past runs: entries marked `historical` are real executions of an earlier tree and are never proof of the current one. `feature-matrix.json` itself never claims execution: its `verification` field stays `pending-runtime`, and a status edit there is rejected by this audit.

`implemented` means source exists within the stated boundary; it does not mean tested, complete parity or platform acceptance. `partial` has known gaps; `experimental` is not a release guarantee; `planned` is not delivered.

[Chinese audit](CN/Benchmark_Review.md) | [English audit](EN/Benchmark_Review.md)

## Upstream Pins

- [UnLua](https://github.com/Tencent/UnLua/tree/3f112e8689405eaad79bd1f8f0bc8d598c8f7fc1): `3f112e8689405eaad79bd1f8f0bc8d598c8f7fc1`
- [slua](https://github.com/Tencent/sluaunreal/tree/7970ab72fd59118e1b3f86aac2d016828b54b748): `7970ab72fd59118e1b3f86aac2d016828b54b748`
- [FUnLua](https://github.com/fcscript/FUnLua/tree/7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3): `7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3`
- [Luau](https://github.com/luau-lang/luau/tree/32c28089305f488f901aaf1dbf3fd1cb65fa3d11): `32c28089305f488f901aaf1dbf3fd1cb65fa3d11`
  Integrated revision: `6d36d483cb88973276af611198169f76f3ce05b0`.

## UE

### UE-REFLECT: Reflection and bidirectional calls

Status: **implemented**; priority: P1; references: UnLua, slua, FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 29/29 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 29/29 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 29/29 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 29/29 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 29/29 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 29/29 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 29/29 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 29/29 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 29/29 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 29/29 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 29/29 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 29/29 passed, 0 failed.

- Source: [UnLuaNovaBase.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaBase.cpp), [FunctionDesc.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/ReflectionUtils/FunctionDesc.cpp)
- Tests: [UnLuaNovaBase.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/UnLuaNovaBase.spec.cpp)
- Examples: [08_CppCallLua.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/08_CppCallLua.lua)

Boundary: Inherited from UnLua; no current execution evidence.

Acceptance: Test null, exception, out/ref, defaults and backend equivalence.

### UE-OVERRIDE: Blueprint overrides, inheritance and RPC overrides

Status: **partial**; priority: P1; references: UnLua, slua, FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 18/18 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 18/18 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 18/18 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 18/18 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 18/18 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 18/18 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 18/18 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 18/18 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 18/18 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 18/18 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 18/18 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 18/18 passed, 0 failed.

- Source: [UnLuaNovaOverrider.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaOverrider.cpp)
- Tests: [Overrides.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Overrides.spec.cpp), [LuaOverrider.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaOverrider.spec.cpp)
- Examples: [02_OverrideBlueprintEvents.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/02_OverrideBlueprintEvents.lua)

Boundary: Ordinary overrides exist; inheritance/net/reinstance combinations incomplete.

Acceptance: Run parent/child/super, interface, RPC and Blueprint recompile scenarios.

### UE-CONTAINER: Containers, structs and borrowed views

Status: **partial**; priority: P0; references: slua, FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 82/82 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 82/82 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 82/82 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 82/82 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 82/82 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 82/82 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 82/82 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 82/82 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 82/82 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 82/82 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 82/82 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 82/82 passed, 0 failed.

- Source: [ContainerRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/ContainerRegistry.cpp), [LuaLib_Array.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/BaseLib/LuaLib_Array.cpp)
- Tests: [LuaLib_Array.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Array.spec.cpp), [LuaLib_Map.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Map.spec.cpp), [LuaLib_Set.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Set.spec.cpp), [DanglingCheck.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/DanglingCheck.spec.cpp)
- Examples: [06_NativeContainers.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/06_NativeContainers.lua)

Boundary: Borrowed child/iterator invalidation and property metadata need lifetime audit.

Acceptance: Destroy parent, mutate/reallocate containers, reinstance Blueprint and force GC.

### UE-DELEGATE: Singlecast, multicast and callback ownership

Status: **partial**; priority: P1; references: slua, FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 14/14 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 14/14 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 14/14 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 14/14 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 14/14 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 14/14 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 14/14 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 14/14 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 14/14 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 14/14 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 14/14 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 14/14 passed, 0 failed.

- Source: [DelegateRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/DelegateRegistry.cpp)
- Tests: [LuaLib_Delegate.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Delegate.spec.cpp), [LuaLib_MulticastDelegate.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_MulticastDelegate.spec.cpp)
- Examples: [05_BindDelegates.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/05_BindDelegates.lua)

Boundary: No complete token/dedup/owner teardown acceptance; preserve singlecast semantics.

Acceptance: Test duplicate binding, reentrant removal, owner destruction, hot reload, multiple Env.

### UE-INPUT: Input, animation and UI workflows

Status: **partial**; priority: P1; references: UnLua, slua, FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 11/11 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 11/11 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 11/11 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 11/11 passed, 0 failed.

- Source: [UnLuaNovaManager.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaManager.cpp)
- Tests: [Overrides.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Overrides.spec.cpp)
- Examples: [03_BindInputs.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/03_BindInputs.lua), [AN_FootStep.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/AN_FootStep.lua), [11_ReleaseUMG.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/11_ReleaseUMG.lua)

Boundary: Examples exist; full EnhancedInput/animation/UMG lifecycle acceptance missing.

Acceptance: Run real RHI scenarios, input rebinding, UI release and animation callbacks on both backends.

## Binding

### BIND-TEMPLATE: Plain C++ template/static exports

Status: **implemented**; priority: P1; references: slua, FUnLua.

Evidence: `2026-11-03-editor-lua` Lua/Editor: 4/4 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 4/4 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 4/4 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 4/4 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 4/4 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 4/4 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 4/4 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 4/4 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 4/4 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 4/4 passed, 0 failed.

- Source: [UnLuaNovaEx.inl](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaEx.inl)
- Tests: [UnLuaNovaEx.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/UnLuaNovaEx.spec.cpp)
- Examples: [09_StaticExport.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/09_StaticExport.lua)

Boundary: Not arbitrary C++ parsing; destructor and ABI behavior require current execution.

Acceptance: Test constructors/destructors, overloads, inheritance, globals and non-reflected methods.

### BIND-THUNK: Generated marshalling thunks

Status: **partial**; priority: P1; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 6/6 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 6/6 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 6/6 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 6/6 passed, 0 failed.

- Source: [StaticBindingThunk.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/StaticBinding/StaticBindingThunk.h)
- Tests: [StaticBinding.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/StaticBinding.spec.cpp), [StaticBindingGenerator.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/StaticBindingGenerator.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Still calls ProcessEvent; unsupported/static/net/complex signatures fall back.

Acceptance: Prove generated/fallback equivalence and add a reproducible generation example.

### BIND-INSTANCE: Independent modules on same-class instances

Status: **implemented**; priority: P1; references: FUnLua.

Evidence: `2026-11-05-release-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 6/6 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 6/6 passed, 0 failed.

- Source: [UnLuaNovaManager.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaManager.h), [UnLuaNovaManager.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaManager.cpp), [ObjectRegistry.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/Registries/ObjectRegistry.h), [ObjectRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/ObjectRegistry.cpp), [FunctionRegistry.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/Registries/FunctionRegistry.h), [FunctionRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/FunctionRegistry.cpp)
- Tests: [InstanceBinding.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/InstanceBinding.spec.cpp)
- Examples: [InstanceBinding.md](https://github.com/HUSTJJD/UnLuaNova/blob/master/Docs/CN/InstanceBinding.md)

Boundary: Delivered: BindInstance/UnbindInstance keep per-instance shallow copies of the module table, RebindModule swaps the INSTANCE metatable without invalidating userdata, class overrides take the union of function names, and instance-bound objects resolve Lua functions per call instead of sharing the class-level FuncRef cache. Requires class-level binding first; runtime evidence still needs a real UE run.

Acceptance: InstanceBinding.spec.cpp covers two modules on one class, isolation across alternating calls, rebinding, class-default fallback, unbind, query reset and rejected inputs through the real override thunk.

## Lifetime

### LIFE-MANUAL: Owned, borrowed and manual userdata finalization

Status: **partial**; priority: P0; references: Luau, FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 13/13 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 13/13 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 13/13 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 13/13 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 13/13 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 13/13 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 13/13 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 13/13 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 13/13 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 13/13 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 13/13 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 13/13 passed, 0 failed.

- Source: [ObjectRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/ObjectRegistry.cpp), [UnLuaNovaEx.inl](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaEx.inl)
- Tests: [Lifetime.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Lifetime.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Manual proxies, known smart pointers and Map/Set-derived arrays now have native finalizers; invalidation is distinct from destruction. Dual-GC/teardown tests added, unexecuted; full allocation audit incomplete.

Acceptance: Force both collectors; prove release counts and safety under both teardown orders.

### LIFE-ENVS: Multiple VM/World environments

Status: **partial**; priority: P0; references: slua, FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 33/33 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 33/33 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 33/33 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 33/33 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 33/33 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 33/33 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 33/33 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 33/33 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 35/35 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 35/35 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 35/35 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 35/35 passed, 0 failed.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp), [LuaEnvLocator.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnvLocator.cpp)
- Tests: [LuaEnv.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaEnv.spec.cpp), [Locators.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Locators.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Timer handles enforce Env ownership with close cleanup, and the network RepProps cache is isolated by a joint owner set with Cleanup(Env) - the older global-cache gap is closed in source. Delivered this round: diagnostic attribution - DescribeEnv(lua_State*) gives a stable GetAllEnvs() index, name and main-state pointer; ReportLuaCallError and Timer callback errors carry it, and Timer entries record their registering script line. Multi-PIE execution evidence and process-level codegen/profiler counters remain outside per-Env attribution.

Acceptance: Create/destroy/recreate Env and World, reuse addresses, forbid cross-environment handles; ModuleContracts.spec.cpp asserts DescribeEnv distinguishes two live envs and answers for an unknown state, and Timer.spec.cpp asserts Diagnostics never leaks another Env's timers. A real multi-PIE run is still pending.

### LIFE-LATENT: Coroutines and latent calls

Status: **partial**; priority: P1; references: Luau, slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 2/2 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 2/2 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 2/2 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 2/2 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 2/2 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 2/2 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 2/2 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 2/2 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 2/2 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 2/2 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 2/2 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 2/2 passed, 0 failed.

- Source: [LuaCoroutineTracker.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaCoroutineTracker.cpp)
- Tests: [Latent.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Latent.spec.cpp)
- Examples: [07_CallLatentFunction.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/07_CallLatentFunction.lua)

Boundary: C boundary yield and abandoned coroutine teardown need coverage.

Acceptance: Test yield/resume/errors/cancellation with Env and World teardown.

## Runtime

### RUNTIME-TIMER: World timers

Status: **partial**; priority: P0; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 31/31 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 31/31 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 31/31 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 31/31 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 31/31 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 31/31 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 31/31 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 31/31 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 39/39 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 39/39 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 39/39 passed, 0 failed.

- Source: [UnLuaNovaTimerSubsystem.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaTimerSubsystem.cpp), [LuaLib_Timer.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaLib_Timer.cpp)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [Timer.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Timer.spec.cpp)
- Examples: [TimerLifecycle.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Examples/TimerLifecycle.lua)

Boundary: Main-state callbacks, teardown cleanup, explicit World, Env-scoped handles and reentry tests implemented. Pause / time dilation / unscaled clocks are implemented in source (Now(bool bUnscaled) selects GetTimeSeconds vs GetRealTimeSeconds, two IsPaused() checks in Tick, IsTickableWhenPaused) but still lack execution evidence. Delivered this round: registration-site diagnostics - FEntry records the Lua source/line captured at Set plus a DescribeEnv() label, Timer.Describe(handle) and Timer.Diagnostics(world) expose them for the calling Env only, and callback error logs name the site and owning VM. multi-PIE and packaged execution remain unverified.

Acceptance: Test due/pause/unscaled/reentry plus coroutine GC, Env-first/World-first destruction and multi-PIE; Timer.spec.cpp now asserts Describe/Diagnostics fields, cross-Env diagnostics isolation and both world clocks. A real paused / time-dilated / multi-PIE run is still pending.

### RUNTIME-EVENT: Event bus and Once reentrancy

Status: **partial**; priority: P1; references: FUnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 22/22 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 22/22 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 22/22 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 27/27 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 27/27 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 27/27 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 27/27 passed, 0 failed.

- Source: [Event.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/Event.lua)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [RuntimeContracts.spec.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Lua/RuntimeContracts.spec.lua)
- Examples: [RuntimeContracts.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Examples/RuntimeContracts.lua)

Boundary: Owner lifecycle cleanup is now automatic as well as explicit: before taking its broadcast snapshot, Emit prunes tokens whose owner reports IsValid()==false; SetOwnerValidator allows a project-accurate predicate and PruneDeadOwners is a manual entry point. UnbindOwner remains the recommended explicit path for hot reload. Runtime evidence for real UObject destruction, exception/reload scenarios and recursive Once is pending.

Acceptance: Run recursive Once and cleanup tests; RuntimeContracts.spec.lua now covers dead-owner pruning, live-owner retention and validator replacement; a real UObject destruction scenario still needs a live run.

### RUNTIME-OOP: Pure Lua classes

Status: **implemented**; priority: P2; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 22/22 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 22/22 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 22/22 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 27/27 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 27/27 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 27/27 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 27/27 passed, 0 failed.

- Source: [OOP.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/OOP.lua)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp)
- Examples: [RuntimeContracts.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Examples/RuntimeContracts.lua)

Boundary: Lua class helper is distinct from UE UClass and experimental Luau classes.

Acceptance: Validate constructor chain, Super, class/instance identity and both backend examples.

### RUNTIME-RELOAD: Hot reload and closure identity

Status: **partial**; priority: P1; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 28/28 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 28/28 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 28/28 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 28/28 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 28/28 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 28/28 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 28/28 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 28/28 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 34/34 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 34/34 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 36/36 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 36/36 passed, 0 failed.

- Source: [HotReload.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua), [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Luau upvalue identity is approximate (Luau has no lua_upvalueid, so shared scalar aliases cannot be proven - an explicit capability boundary, not a fixable bug). Delivered this round: failed reload rollback. reload_modules is a two-phase commit with a strong-reference shallow snapshot taken before any write, new modules computed in the sandbox without touching old objects, an xpcall-wrapped commit, and rollback of the module table / package.loaded / loaded_modules / loaded_module_times on failure; module_loaded hooks fire only after a successful commit; update_global's registry scan is pcall-guarded so the Luau debug.getregistry shim can no longer abort a whole reload. M.reload and reload_modules return ok, err, the no-argument path advances loaded_module_times only after success so a failed reload stays retryable, and Lua.HotReload() now surfaces that boolean. Rollback is explicitly best-effort, not a transaction: _G, live stack locals, arbitrary tables/upvalues and the registry are not snapshotted.

Acceptance: Test shared closures, table cycles, delegates, active coroutine and failed reload rollback; RuntimeModules.spec.cpp now drives success, load-time error, and syntax-error + no-argument retry paths entirely in memory through an injected package.searchers source switch on both backends. A real file-mtime / multi-module / active-coroutine reload run is still pending.

## Networking

### NET-RPC: Declared server/client/multicast RPC

Status: **experimental**; priority: P0; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 19/19 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 19/19 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 21/21 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 21/21 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 23/23 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 23/23 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 23/23 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 23/23 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 47/47 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 47/47 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 47/47 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 47/47 passed, 0 failed.

- Source: [UnLuaNovaNet.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaNet.cpp)
- Tests: [LuaNet.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaNet.spec.cpp), [LuaNetMulticastTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetMulticastTest.cpp), [LuaNetDedicatedServerTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetDedicatedServerTest.cpp), [LuaNetNetScenariosTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetNetScenariosTest.cpp)
- Examples: [NetActor.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Net/NetActor.lua)

Boundary: Sorted registration, whole-RPC parameter validation, reliable/unreliable flags on all three kinds, and parent-class schema lookup delivered. RPC parameters accept any resolvable UScriptStruct and Array/Set/Map nests: the runtime-built UFunction gains a real FStructProperty/FArrayProperty/FSetProperty/FMapProperty, and the peer declaration report exports the FProperty-derived spec of every parameter (not an echo of the Lua script), so a connected gate can prove the property was actually built. The engine already rejects a non-owner Server RPC (UNetDriver::ShouldCallRemoteFunction requires RepFlags.bNetOwner); the DS scenario and release gate assert this with a unique token, but that assertion has not been executed. Eight opt-in connected scenarios (late join, process-level reconnect, dormancy, relevancy, join-order swap, packet loss, UNetConnection-level reconnect, cross-peer schema fingerprint) and four delivery variants (Legacy/PushOff/Iris/Cooked) are delivered in source with fail-closed gates, but none has been executed. No further source gap is claimed; the remaining work is execution only.

Acceptance: The connected DS + 2 clients scenario includes an explicit non-owner Server RPC rejection assertion (unique token, exact CallLog comparison, and a ValidateRelease.py gate requiring the attempt itself). Late join needs a spawn marker plus topology launch order; the swapped-order scenario needs the reverse; reconnect needs a distinct replacement PID and launch_count=2; UNetConnection-level reconnect needs launch_count=1, a closed connection address and an in-process reopen; packet loss needs real NetEmulation plus convergence of the final field value; schema mismatch needs identical RepSchemaVersion on all three peers plus a discriminating VariantRepSchemaVersion. Cooked needs a staged WindowsServer/Windows peer exe and a WITH_EDITOR-derived flag. All of these must be executed via Tools/RunNetMatrix.ps1 before they count. Reliable/unreliable, declaration syntax, spec parsing and UStruct property construction are covered by unit cases.

### NET-PROPS: Property conditions and schemas

Status: **experimental**; priority: P0; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 20/20 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 20/20 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 20/20 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 20/20 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 21/21 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 21/21 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 46/46 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 45/45 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 46/46 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 45/45 passed, 0 failed.

- Source: [UnLuaNovaReplicationComponent.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaReplicationComponent.cpp), [UnLuaNovaNetPayload.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaNetPayload.cpp)
- Tests: [LuaNet.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaNet.spec.cpp), [LuaNetClientServerTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetClientServerTest.cpp), [LuaNetDedicatedServerTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetDedicatedServerTest.cpp), [LuaNetNetScenariosTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetNetScenariosTest.cpp)
- Examples: [NetActor.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Net/NetActor.lua)

Boundary: One storage group per supported condition (None/InitialOnly/OwnerOnly/SkipOwner/SimulatedOnly/AutonomousOnly/SimulatedOrPhysics/InitialOrOwner), plus a shared Generic group for arbitrary UStruct/container fields; malformed and unsupported declarations (Custom/Dynamic/Never/NetGroup/Replay*) are rejected atomically and cached schemas stay immutable. A subclass without its own declaration inherits the parent's schema. Multi-Env joint ownership is now fixed: identical declarations bound by several environments are released only when the last one closes, in any order (previously closing the first owner wiped a schema another Env still used), and weak UObject references that have gone invalid never write a stale pointer back to Lua. Both have dedicated unit cases. The connected declaration report exports TypeSpec, RepSchemaVersion and a deliberately different VariantRepSchemaVersion from every peer, so a mismatched declaration cannot decode into the wrong slot. Connected owner/non-owner visibility, dormancy, relevancy, join-order swaps and packet-loss convergence are asserted by scenario source and gates, but none has been executed. No further source gap is claimed.

Acceptance: The per-condition layout is delivered, and the DS scenario source mixes unconditional, OwnerOnly and Generic fields in one declaration list and exports per-peer visibility, TypeSpec, RPC parameter specs and declaration fingerprints for ValidateRelease.py to assert. Dormancy requires a client to observe a suppressed dormant value and then the flushed value; relevancy requires the non-owner to lose the owner-only actor; packet loss requires replication to converge under NetEmulation. All of these must be executed before they count.

### NET-TYPES: Replication types, UStruct/containers and Iris/push model

Status: **experimental**; priority: P0; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 19/19 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 19/19 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 19/19 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 19/19 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 21/21 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 21/21 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 21/21 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 21/21 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 45/45 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 45/45 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 45/45 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 45/45 passed, 0 failed.

- Source: [UnLuaNovaReplicationComponent.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaReplicationComponent.cpp), [UnLuaNovaNetTypeSpec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaNetTypeSpec.cpp), [UnLuaNovaNetTypeSpec.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaNetTypeSpec.h), [UnLuaNovaNetDecl.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaNetDecl.h), [UnLuaNovaNetBuckets.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaNetBuckets.h)
- Tests: [LuaNet.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaNet.spec.cpp), [LuaNetDedicatedServerTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetDedicatedServerTest.cpp), [LuaNetNetScenariosTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetNetScenariosTest.cpp), [test_release_validation.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_release_validation.py)
- Examples: [NetActor.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Net/NetActor.lua)

Boundary: Strict int32/int64 conversion, scalar and Object bucket round-trips, invalid weak objects, and per-condition storage covered by unit cases. The earlier claim that arbitrary UStruct/container fields are an architecture boundary was wrong: a UClass cannot gain a UPROPERTY at runtime, but a generic bucket carrying a canonical self-describing payload does not need one, and a runtime-built UFunction can own any FStructProperty/FArrayProperty/FSetProperty/FMapProperty. Both paths are delivered, with the spec folded into SchemaVersion so mismatched peers cannot misdecode. UObject references keep the dedicated Object bucket because a text payload cannot carry a NetGUID. The Legacy / PushOff / Iris / Cooked delivery matrix, eight connected scenarios and Tools/RunNetMatrix.ps1 are delivered in source; AppendWorldFacts exports real NetEmulation state, Iris/push-model cvars, WITH_EDITOR and UNetConnection byte/loss counters so an Iris element-level delta comparison can be made from the reports. No variant or scenario has been executed, and no compression win is claimed.

Acceptance: ValidateRelease.py --net-matrix must see four variant topologies (including a staged WindowsServer/Windows peer exe for Cooked) and eight scenario topologies, each with real engine switch state, cooked flag, declaration fingerprints, Generic field/RPC parameter specs and non-zero connection byte counters read back from every peer. Exact integer boundaries, Object resolution/invalidation, multi-Env teardown order, spec parsing/nesting and canonical Lua <-> payload round-trips are covered by unit cases.

## Luau

### LUAU-SYNTAX: Language/compiler surface

Status: **partial**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 30/30 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 39/39 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 32/32 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 32/32 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 41/41 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 41/41 passed, 0 failed.

- Source: [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp), [LuauConformance.spec.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Lua/LuauConformance.spec.lua)
- Examples: [04_LuauSyntax.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/04_LuauSyntax.lua), [01_StrictTyping.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/01_StrictTyping.lua)

Boundary: Pinned compiler supports syntax; not a full integrated conformance run.

Acceptance: Import pinned syntax/interpolation/if-expression/generics/iteration conformance with documented exclusions.

### LUAU-LIBS: base, math, string, table, utf8, bit32, coroutine

Status: **partial**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 30/30 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 39/39 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 32/32 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 32/32 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 41/41 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 41/41 passed, 0 failed.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp), [LuauConformance.spec.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Lua/LuauConformance.spec.lua)
- Examples: [04_LuauSyntax.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/04_LuauSyntax.lua)

Boundary: VM libraries exposed; tests sample behavior rather than all upstream conformance.

Acceptance: Run library/metatable/error/weak-table conformance and host-specific exclusions.

### LUAU-VECTOR: Native vector and FVector interoperability

Status: **implemented**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 53/53 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 44/44 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 44/44 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 53/53 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 44/44 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 53/53 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 44/44 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 53/53 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 46/46 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 46/46 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 55/55 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 55/55 passed, 0 failed.

- Source: [LuaLib_FVector.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/MathLib/LuaLib_FVector.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp), [LuaLib_FVector.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_FVector.spec.cpp), [LuauConformance.spec.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Lua/LuauConformance.spec.lua)
- Examples: [05_VectorInterop.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/05_VectorInterop.lua)

Boundary: float vector versus UE double FVector loses precision.

Acceptance: Test conversion/parameter/operator edges and document precision budget.

### LUAU-BUFFER: Native buffer and UE array copies

Status: **implemented**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 27/27 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 27/27 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 22/22 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 27/27 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 22/22 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 27/27 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 27/27 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 27/27 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 40/40 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 40/40 passed, 0 failed.

- Source: [UnLuaNovaLib.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaLib.cpp), [Buffer.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/Buffer.lua)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp), [LuauConformance.spec.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Lua/LuauConformance.spec.lua)
- Examples: [06_BufferInterop.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/06_BufferInterop.lua)

Boundary: Copies rather than external-memory aliases; output array type differs from uint8.

Acceptance: Bounds/type/value tests and verify source/buffer independence; measure copy cost.

### LUAU-CODEGEN: Native compilation

Status: **experimental**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 5/5 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 13/13 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 13/13 passed, 0 failed.

- Source: [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: VM-scoped initialization fixed. The counter is now explicitly a REQUEST count: GetNativeCodeGenRequestCount() plus a `lua.codegen` console line that states luau_codegen_compile() exposes no success signal, with the old getter kept as a documented alias. Platform acceptance, executable-memory policy and any per-chunk success signal remain absent (upstream API limitation, not a missing gate).

Acceptance: Test VM/coroutine lifecycle, supported architectures, debug attachment, failures and executable-memory policy.

### LUAU-GC: Incremental GC and budgets

Status: **partial**; priority: P1; references: Luau, slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 6/6 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 6/6 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 6/6 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 6/6 passed, 0 failed.

- Source: [LuaGcScheduler.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaGcScheduler.cpp), [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: [LuaGcScheduler.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaGcScheduler.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Not generational or multithreaded GC; budgets are not proven hard pause limits.

Acceptance: Measure pauses and allocation pressure; validate tuning, errors and destruction callbacks.

### LUAU-SANDBOX: Readonly globals, threads and class tables

Status: **partial**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 30/30 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 39/39 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 30/30 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 39/39 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 32/32 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 32/32 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 54/54 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 54/54 passed, 0 failed.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp), [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp), [UnLuaNovaLib.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaLib.cpp), [UnLuaNovaSandbox.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaSandbox.h)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp), [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp)
- Examples: [02_SandboxBoundary.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/02_SandboxBoundary.lua)

Boundary: Capability trimming delivered: lua.sandboxdeny removes named globals before luaL_sandbox freezes _G, protects host-written globals (UE.*/Lua/package.loaded/require) and returns the removal count so a spec can tell 'policy applied' from a no-op. Still default off, still not an untrusted-code permission model: host UE/debug/io/socket capabilities remain unless explicitly denied, and there is no per-script grant. Runtime evidence for writable escape paths, new threads and host limits pending.

Acceptance: Define capability policy and verify writable escape paths, new threads, imports and host limits; LuauIntegration.spec.cpp asserts that denied globals disappear while protected ones survive.

### LUAU-TYPES: Type inference, declarations and LSP

Status: **partial**; priority: P1; references: Luau.

Evidence: `2026-11-03-editor-lua` Lua/Editor: 5/5 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 5/5 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 5/5 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 5/5 passed, 0 failed.

- Source: [UnLuaNovaIntelliSenseGenerator.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNovaEditor/Private/UnLuaNovaIntelliSenseGenerator.cpp), [CheckLuauTypes.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/CheckLuauTypes.ps1)
- Tests: [IntelliSense.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/IntelliSense.spec.cpp)
- Examples: [01_StrictTyping.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/01_StrictTyping.lua)

Boundary: External checker; declarations approximate containers/delegates and skips may occur.

Acceptance: Require checker and fresh declarations; verify inference/generics/overloads/type-functions and negative fixtures.

### LUAU-REQUIRE: Host module loader versus upstream Require

Status: **partial**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 26/26 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 26/26 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 26/26 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 26/26 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 26/26 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 26/26 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 26/26 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 26/26 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 29/29 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 29/29 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 44/44 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 44/44 passed, 0 failed.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp), [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [LuaEnv.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaEnv.spec.cpp), [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp), [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp)
- Examples: [12_CustomLoader.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/12_CustomLoader.lua)

Boundary: Host searchers/preload still differ from upstream Require config semantics. Delivered: relative-path mangling fixed (LoadFromFileSystem only rewrites dots for dotted module names, so './Foo' / 'Foo.lua' / explicit paths keep their dots), package.aliases redirects require on both backends via a searcher placed before preload (alias and target share one cached module instance, self-aliases raise instead of recursing), and the Luau-side FLuaEnv::Require now detects cyclic dependencies with a dedicated 'loading' upvalue - it names the module in the error and clears the marks on success and failure so one failed load cannot poison later requires. Delivered this round: a require-wrapper cycle guard installed after HotReload replaces _G.require, so named cycle detection covers plain Lua's native require, FLuaEnv::Require and HotReload's replacement at once without changing loader signatures or the compiled chunks that HotReload applies setfenv/_ENV to (wrapping searchers would have defeated that); unrequire(name) clears package.loaded and HotReload's three private caches while keeping package.preload and package.aliases declarations, reloadmodule(name) is unrequire + require, and package.resolution() declares the case, cooked, @alias-config, cycle and unload contract in machine-readable form. A leading '@' now fails with an explicit unsupported-@alias-config-files error, and lua.requireresolution adds opt-in unresolved-path diagnostics. Upstream @alias config files remain deliberately unsupported; case-insensitive fallback is explicitly out of scope and a real cooked-build resolution run is still pending.

Acceptance: Document resolution/cycle/cache contract and add case/cook tests or explicit unsupported errors; LuaEnv.spec.cpp and LuauIntegration.spec.cpp cover alias redirection, self-alias rejection, cycle rejection and post-failure reuse; ModuleContracts.spec.cpp now covers the wrapped require cycle guard, argument/return pass-through, unrequire/reloadmodule, package.resolution and the @alias refusal on both backends. A real cooked-build and case-sensitive-filesystem resolution run is still pending.

### LUAU-COVERAGE: Coverage export and quality gates

Status: **implemented**; priority: P1; references: Luau.

Evidence: no execution recorded yet.

- Source: [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp), [LuaCoverage.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaCoverage.cpp), [UnLuaNovaConsoleCommands.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaConsoleCommands.cpp), [ValidateCoverage.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ValidateCoverage.py)
- Tests: [test_validate_coverage.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_validate_coverage.py)
- Examples: [LuauCoverage.md](https://github.com/HUSTJJD/UnLuaNova/blob/master/Docs/CN/LuauCoverage.md)

Boundary: Exported and gated: per-line Luau counters, declared exclusions, recomputed totals, multi-run aggregation and fail-closed coverage thresholds. Source names come from reachable global/module tables rather than a source map; C closures have no counters; chunks loaded before luau.coverage was set carry none, and only the Luau backend can export.

Acceptance: lua.coverage dump writes per-source line counters; Tools/ValidateCoverage.py recomputes totals, merges runs by max hits, requires named sources and a threshold, and fails on missing, empty, level-0 or mismatched reports. 15 tooling regressions pass without UE; execution evidence still requires a real run.

### LUAU-EXPERIMENTAL: Experimental integer and user-defined class libraries

Status: **experimental**; priority: P2; references: Luau.

Evidence: no execution recorded yet.

- Source: [Luau.Build.cs](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau.Build.cs)
- Tests: Missing dedicated evidence.
- Examples: Missing dedicated evidence.

Boundary: Upstream flag-controlled APIs are not stable integration promises.

Acceptance: Record flags, compatibility/ABI/type declarations and tests before exposing as stable.

## Compatibility

### COMPAT-DEBUG: debug, upvalues and uservalues

Status: **partial**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 6/6 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 6/6 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 6/6 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 6/6 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 7/7 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 7/7 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 9/9 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 9/9 passed, 0 failed.

- Source: [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Exact subset delivered and documented: debug.getregistry rejects explicitly instead of returning a misleading empty table; getuservalue is the documented nil-only subset HotReload.lua relies on; setuservalue returns false instead of a pretend success; upvalueid is value identity (exact for shared references, slot identity for scalars, never a scalar-aliasing proof). Backend-specific spec cases cover all four. Full Lua 5.x debug parity is not claimed.

Acceptance: Reject unsupported operations or implement exact subset; test closure identity and error cases. Both backends must keep their own exact semantics (Luau compat layer vs plain Lua native debug table).

### COMPAT-IO: Optional rooted buffered io

Status: **partial**; priority: P1; references: Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 5/5 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 5/5 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 13/13 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 13/13 passed, 0 failed.

- Source: [LuaLib_Io.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaLib_Io.cpp)
- Tests: [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Delivered: CRLF/path/read checks, strict mode validation (r/w/a/+/b, unknown or empty modes refused), strict seek whence ('set'/'cur'/'end', clamped to the in-memory buffer), format lists for file:lines() and io.lines() (shared reader for 'a'/'l'/'L'/'n' and numeric counts), a lua.iomaxbytes quota on open/write/flush, writes that overwrite at __pos and advance the position (append mode still forces the end), an in-memory io.tmpfile() whose flush never touches disk, and an explicit io.popen() refusal. Still not Lua io parity: there is no OS stream or sparse file, hard links / symlinks are outside the capability set, and binary/large-file comparisons need a real run.

Acceptance: Compare subset against Lua, including binary/large files, mutated handles, failure and root changes; LuauIntegration.spec.cpp covers mode/seek rejection, multi-format iterators, overwrite and append positions, tmpfile, popen refusal, the byte quota and sandbox capability trimming.

### COMPAT-LANGUAGE: Lua 5.x integer, close, goto and bytecode differences

Status: **partial**; priority: P1; references: Luau, UnLua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 45/45 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 36/36 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 36/36 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 45/45 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 36/36 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 45/45 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 36/36 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 45/45 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 50/50 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 50/50 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 61/61 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 61/61 passed, 0 failed.

- Source: [LuaBackend.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/LuaBackend.h), [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp), [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp), [ErrorHandling.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/ErrorHandling.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Delivered: source-only load is now an explicit contract - load() validates its mode (pure 'b', unknown letters and empty modes fail with a clear error), accepts 't' (including Lua's default 'bt') and always passes 't' to luaL_loadbufferx, so bytecode is never fed to Luau's source compiler nor accepted on plain Lua. Still no language-level equivalence: int64/close/goto differences, bytecode version policy and trusted-bytecode loading need an execution-backed migration contract.

Acceptance: Publish the int64/close/goto migration contract and verify trusted bytecode; ErrorHandling.spec.cpp, LuaBackend.spec.cpp and LuaScriptCompat.spec.cpp assert mode rejection, 't'/'bt' acceptance and the env argument on both backends.

## Tooling

### DEBUG-DAP: Debug service and DAP

Status: **partial**; priority: P1; references: slua, Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 14/14 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 14/14 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 14/14 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 14/14 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 14/14 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 14/14 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 8/8 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 8/8 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 23/23 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 14/14 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 23/23 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 14/14 passed, 0 failed.

- Source: [LuaDebugService.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/LuaDebugService.h), [LuaDebugService.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaDebugService.cpp), [UnLuaNovaBase.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaBase.cpp), [LuaDebugAdapter.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNovaEditor/Public/LuaDebugAdapter.h), [LuaDebugAdapter.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNovaEditor/Private/LuaDebugAdapter.cpp)
- Tests: [DebugService.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/DebugService.spec.cpp), [DebugAdapter.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/DebugAdapter.spec.cpp)
- Examples: [03_NativeDebug.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/03_NativeDebug.lua)

Boundary: Full command surface delivered and verified against real state: setBreakpoints verifies each line against the resolved script's real line count; exception filters (raised/uncaught) are wired into the shared ReportLuaCallError handler, arm the line hook, stop with reason=exception and answer exceptionInfo; variables expands nested tables through registry-backed handles with depth/cycle/count budgets and releases them at the next stop or Detach. setFunctionBreakpoints matches lua_getinfo('n') names in the line hook with explicit verified=false for empty names; modules is a real package.loaded snapshot taken on the VM thread at attach and every stop, with startModule/moduleCount paging. Delivered this round: capability declarations now match the implementation - supportsFunctionBreakpoints is true (implementation, dispatch and spec already existed while the bit said false, so IDEs never sent the request), and loadedSources is implemented from that same VM-thread snapshot, resolving module names through the shared script resolver and marking unresolved entries honestly instead of inventing paths, advertised via supportsLoadedSourcesRequest. A known-unsupported command table (restart, restartFrame, terminate, stepInTargets, goto, setExpression, completions, data/instruction breakpoints, readMemory, writeMemory, disassemble, cancel, breakpointLocations, runInTerminal) returns a reason with each refusal instead of a generic 'not implemented'. Conditional / hit-conditional breakpoints, stepInTargets and restart remain declared false. launch is still an explicit refusal. Remaining: packaged remote support, authorization, reconnect and a paused-frame multi-Env/coroutine run are unverified. Runtime evidence pending.

Acceptance: DebugAdapter.spec.cpp drives every command through a real DAP session and asserts verified breakpoints, exception capabilities/filters, function-breakpoint verification/rejection, modules paging consistency and nested-variable failure paths; it now also asserts the two newly advertised capability bits, an empty-but-successful loadedSources response while unattached, and a reasoned refusal (not a bare 'not implemented') for a known-unsupported command. DebugService.spec.cpp covers filters, last-error recording, stop reason, nested expansion refusal, function-name dedup and the package.loaded snapshot. Break/step behaviour still needs a paused-frame multi-Env/coroutine run; none of this round's C++ or automation was compiled or executed.

### PROFILE: CPU, self time, memory and flamegraphs

Status: **partial**; priority: P1; references: slua, Luau.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 9/9 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 9/9 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 9/9 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 9/9 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 9/9 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 9/9 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 9/9 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 9/9 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 9/9 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 9/9 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 9/9 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 9/9 passed, 0 failed.

- Source: [LuaProfiler.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaProfiler.cpp), [perfgraph.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/LuauPerfgraph/perfgraph.py)
- Tests: [Observability.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Observability.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Sampled/measured attribution still differs: plain Lua pairs call/return for exact self time, Luau charges the interval between safe points to whatever was running (the GC flag is now forwarded to the profiler instead of hardcoded to 0). Delivered: lua.profiler trace on/off/reset/dump writes Luau's profile.out (relative dump paths now land under Saved/Profiling), the perfgraph.py wrapper documents an end-to-end flow and fails with an actionable submodule hint, and CN/EN console docs carry the collect -> dump -> render example. The renderer itself is upstream pinned code, so it needs the Luau submodule; stack/yield/GC attribution, overhead and malformed dumps still need execution evidence.

Acceptance: Validate stack/yield/GC attribution, overhead and malformed dumps; follow the end-to-end example in Docs/{CN,EN}/ConsoleCommand.md and render with Tools/LuauPerfgraph/perfgraph.py once the pinned submodule is initialized.

### WATCHDOG: Dead-loop and coroutine diagnostics

Status: **partial**; priority: P1; references: slua.

Evidence: `2026-11-02-editor-luau` Luau/Editor: 10/10 passed, 0 failed; `2026-11-02-editor-lua` Lua/Editor: 10/10 passed, 0 failed; `2026-11-03-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 11/11 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 11/11 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 11/11 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 13/13 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 13/13 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 13/13 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 13/13 passed, 0 failed.

- Source: [LuaCoroutineTracker.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaCoroutineTracker.cpp), [LuaDeadLoopCheck.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaDeadLoopCheck.cpp), [LuaVmHooks.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/LuaVmHooks.h)
- Tests: [LuaDeadLoopCheck.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaDeadLoopCheck.spec.cpp), [ErrorHandling.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/ErrorHandling.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Delivered: cancellation is correct and observable (GuardLeave disarms the guard that actually finished, nested guards take over the armed slot, GetTimeoutCount() counts real interruptions), plus a deterministic safe-point budget lua.maxsafepoints counted through FLuaVmHooks - LUA_MASKCOUNT instructions on plain Lua, Luau interrupts on Luau - with GetSafePointInterruptCount() and a guard that exists even when the polling thread is off. GC interrupts are passed through to listeners: the profiler uses the flag (no longer hardcoded to 0) while the watchdog and budget skip them rather than longjmp out of the collector. Delivered this round: a per-Env memory quota (lua.maxmemorykb / lua.memquota) enforced in the default allocator before every allocation, with live/peak-request/rejection counters, one actionable Error log per env on the first refusal and a recoverable VM not-enough-memory error; frees are never refused so lua_close cannot wedge, a custom lua_Alloc stays outside the quota, and ModuleContracts.spec.cpp asserts a real refusal plus VM recovery on both backends. Still not a complete hostile-code control: native callback and abandoned-coroutine attribution unproven, collector scratch allocations count towards the live waterline, and a Luau safe point is not an instruction.

Acceptance: Test timeout/reset/teardown, nested guards, native callbacks and abandoned coroutine attribution; LuaDeadLoopCheck.spec.cpp now also asserts the safe-point budget interrupts a real loop and does not cap the next call with a stale counter. ModuleContracts.spec.cpp now asserts the quota fires, is observable and leaves the VM usable; a real dual-backend OOM/GC run is still pending.

## Product

### EXTENSIONS: Socket, protobuf and JSON

Status: **partial**; priority: P1; references: UnLua, slua.

Evidence: `2026-11-03-editor-lua` Lua/Editor: 8/8 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 8/8 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 8/8 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 8/8 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 8/8 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 8/8 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 8/8 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 8/8 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 8/8 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 8/8 passed, 0 failed.

- Source: [pb.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaExtensions/LuaProtobuf/Source/src/pb.cpp), [luasocket.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaExtensions/LuaSocket/Source/src/luasocket.cpp), [rapidjson.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaExtensions/LuaRapidjson/Source/src/rapidjson.cpp)
- Tests: [Extensions.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Extensions.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Backend lifetime/ABI/error and distribution notices need component-level verification.

Acceptance: Large/malformed payloads, socket close/failure, userdata GC and packaged linkage on both backends.

## Quality

### QA-PROVENANCE: Test framework, reports and build identity

Status: **partial**; priority: P1; references: UnLua, slua, FUnLua.

Evidence: `2026-11-03-editor-lua` Lua/Editor: 1/1 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 1/1 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 1/1 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 1/1 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 1/1 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 1/1 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 1/1 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 1/1 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 1/1 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 1/1 passed, 0 failed.

- Source: [RunTests.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/RunTests.ps1), [ReleasePipeline.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ReleasePipeline.ps1), [ValidateRelease.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ValidateRelease.py), [GenerateTestReport.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/GenerateTestReport.py), [ClassifyTests.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ClassifyTests.py), [AuditRepository.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/AuditRepository.py), [RecordEvidence.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/RecordEvidence.py), [ValidateProvenance.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ValidateProvenance.py)
- Tests: [LuaSpecs.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaSpecs.spec.cpp), [test_audit_repository.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_audit_repository.py), [test_validate_provenance.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_validate_provenance.py), [test_release_validation.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_release_validation.py)
- Examples: Missing dedicated evidence.

Boundary: Release orchestration, runtime manifests, strict evidence gates and provenance are implemented but unexecuted; source classification is not code coverage. Delivered this round (static only, no UE run): AuditRepository.py now validates inline AND HTML local links, Markdown anchors against MkDocs heading slugs, and plain-https remote URL syntax, and reports the historical/current-tree evidence split instead of silently counting historical runs as current coverage; ValidateRelease.py writes a schema/versioned release-gate.json carrying commit, run, backend, carrier, net_matrix and a UTC timestamp (a null commit when git is unavailable), and its showcase media gate now requires a real PNG signature and IHDR header; Tools/ValidateProvenance.py is a fail-closed static gate that keeps historical records out of current-tree coverage, checks that all four release gates (2 backends x 2 carriers) exist, are green, belong to one run and agree with release-summary.json, and can demand a non-historical record matching HEAD via --require-current. The four gate files already in the tree are legacy format with no provenance fields, so they still cannot prove they belong to the current checkout. None of this executes UE, fabricates runtime evidence or rewrites release-status.

Acceptance: Fail missing/empty/malformed runs; record commit/dirty/dependencies/flags/topology; require type tools. Also require: python -B Tools/AuditRepository.py (links, anchors, https syntax), python -B Tools/ValidateProvenance.py --run <run> --summary <summary> --require-current (fails with 'no current-tree execution evidence' while only historical runs exist), the full Tools/tests unittest suite (185 regressions), and four freshly generated gate files whose commit matches the released HEAD. A green static structure check must never be reported as 'the current tree was verified'.

### PERFORMANCE: Comparable benchmarks and regression gates

Status: **partial**; priority: P1; references: slua, FUnLua.

Evidence: `2026-11-03-editor-lua` Lua/Editor: 2/2 passed, 0 failed; `2026-11-03-editor-luau` Luau/Editor: 2/2 passed, 0 failed; `2026-11-03-baseline-editor-lua` Lua/Editor: 2/2 passed, 0 failed; `2026-11-03-baseline-editor-luau` Luau/Editor: 2/2 passed, 0 failed; `2026-11-03-baseline-game-lua` Lua/Game: 2/2 passed, 0 failed; `2026-11-03-baseline-game-luau` Luau/Game: 2/2 passed, 0 failed; `2026-11-05-release-editor-lua` Lua/Editor: 2/2 passed, 0 failed; `2026-11-05-release-game-lua` Lua/Game: 2/2 passed, 0 failed; `2026-11-05-release-editor-luau` Luau/Editor: 2/2 passed, 0 failed; `2026-11-05-release-game-luau` Luau/Game: 2/2 passed, 0 failed.

- Source: [RunTests.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/RunTests.ps1), [runner.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/CrossProject/runner.py), [metrics.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/CrossProject/metrics.py), [cases.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/CrossProject/workload/unluanova/cases.lua), [cases.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/CrossProject/workload/external/cases.lua)
- Tests: [Benchmark.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Benchmark.spec.cpp), [BenchmarkParity.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/BenchmarkParity.spec.cpp), [test_crossproject_runner.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_crossproject_runner.py), [test_crossproject_workloads.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_crossproject_workloads.py), [test_crossproject_metrics.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_crossproject_metrics.py)
- Examples: [BP_UnLuaNovaBenchmark.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Benchmark/BP_UnLuaNovaBenchmark.lua), [README.md](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/CrossProject/README.md)

Boundary: Workload contract (revision 1), UnLuaNova workload, adaptive slua-unreal/FUnLua adapter, fail-closed alignment gate and 22 static regressions delivered. Tools/CrossProject/metrics.py now produces raw multi-run distributions (min/median/mean/p95/max/stddev plus per-run medians) for one aligned project/backend, refusing mismatched hardware, project, backend, case sets or empty samples. Delivered this round: an independent #metric log line type (separate from the ten #case timing cases, so the static 10-case contract is untouched) carrying memory.peak_kb, startup.load_us, gc.pause_us and code_size.bytes; runner.py parses and aligns metric sets/units, both cases.lua emit them, metrics.py adds check_metric_alignment / summarize_metrics with min/median/mean/p95/max/stddev and fail-closed rejection of missing, unavailable-as-zero, negative, non-finite, unknown-unit or mismatched values behind an opt-in --require-metrics switch (default behaviour on old JSON is unchanged), and 30 new tool regressions pass (130 total). External adapters report code_size as explicit unavailable when it cannot be measured rather than emitting 0. Still absent: same-machine runs in the two external projects and any cross-project conclusion.

Acceptance: Same hardware/UE/config, identical 10-case sets and iteration counts (checked statically), raw multi-run distributions for timing and the four non-time metrics, and real logs from both external bindings before any comparison is published; the 130 tool regressions cover the metric gate statically, while a real dual-backend metric collection run is still pending.

## Product

### PACKAGING: Cooked targets, platforms and upgrades

Status: **partial**; priority: P1; references: slua, FUnLua.

Evidence: no execution recorded yet.

- Source: [UnLuaNova.Build.cs](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/UnLuaNova.Build.cs)
- Tests: Missing dedicated evidence.
- Examples: Missing dedicated evidence.

Boundary: Win64 Development cooked automation, Server build and Shipping packaging are orchestrated; execution, Shipping runtime and other platforms remain unverified. Delivered this round: shipped-notice reproducibility is statically checkable with GenerateNotices.py --check against staged NOTICES.txt, showcase media evidence must be a real PNG (signature + IHDR), and each release-gate.json carries commit/run/backend/carrier/timestamp so gates from different runs cannot be mistaken for one Release. The existing staged NOTICES.txt files are still incomplete and the current gates are legacy format; both need a fresh Release run.

Acceptance: Cooked scripts/assets/extensions, DS/client, feature-off, executable-memory targets and upgrade tests, plus a fresh four-gate set with matching commit provenance and a passing notice --check for every staged artifact.

## Release

### LEGAL: License scope and provenance

Status: **partial**; priority: P0; references: UnLua, slua, FUnLua, Luau.

Evidence: no execution recorded yet.

- Source: [LICENSE](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE), [LICENSE-THIRD-PARTY.md](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE-THIRD-PARTY.md), [GenerateNotices.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/GenerateNotices.py)
- Tests: [test_generate_notices.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/tests/test_generate_notices.py)
- Examples: Missing dedicated evidence.

Boundary: Root/file MIT scope is resolved and all in-tree version identifiers (LuaSocket 3.0-rc1, RapidJSON 1.1.0, serpent 0.30, lua-rapidjson 'scm') are recorded with file:line evidence. Delivered this round: Tools/GenerateNotices.py deterministically assembles a shipped NOTICES.txt from license information ALREADY present in the repository and offers --check against staged artifacts, so the two NOTICES.txt files under Saved/StagedBuilds are no longer unreproducible one-line leftovers. Every component whose notice cannot be found is emitted as UNRESOLVED and fails the gate (the real tree currently reports 11 UNRESOLVED and exits 1); no license text, rights holder or scope decision is invented, and the generated file states that explicitly. Still outstanding and not assertable from this tree: exact upstream revisions of copied extensions, sample asset redistribution rights, FUnLua's missing license file at the pinned commit, and the rights-holder scope decision itself. A reproducible manifest is not a legal conclusion.

Acceptance: Rights-holder scope decision, file/component provenance, complete notices in distributable artifact. Before any release claim, GenerateNotices.py --check must pass against every staged NOTICES.txt with zero UNRESOLVED entries; the staged files are currently known to be incomplete (one FreeType line each) and are deliberately not rewritten by this pass; default GenerateNotices.py is expected to stay FAIL-CLOSED until a rights holder resolves them.

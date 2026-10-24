# Capability and Evidence Matrix

Generated from `Tools/feature-matrix.json`; edit that file and run `python Tools/AuditRepository.py --write-matrix`.

Audit: 2026-10-24. Source audit, not a claim of exhaustive feature or execution coverage.

All runtime verification is pending. `implemented` means source exists within the stated boundary; it does not mean tested, complete parity or platform acceptance. `partial` has known gaps; `experimental` is not a release guarantee; `planned` is not delivered.

[Chinese audit](CN/Benchmark_Review.md) | [English audit](EN/Benchmark_Review.md)

## Upstream Pins

- [UnLua](https://github.com/Tencent/UnLua/tree/3f112e8689405eaad79bd1f8f0bc8d598c8f7fc1): `3f112e8689405eaad79bd1f8f0bc8d598c8f7fc1`
- [slua](https://github.com/Tencent/sluaunreal/tree/7970ab72fd59118e1b3f86aac2d016828b54b748): `7970ab72fd59118e1b3f86aac2d016828b54b748`
- [FUnLua](https://github.com/fcscript/FUnLua/tree/7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3): `7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3`
- [Luau](https://github.com/luau-lang/luau/tree/32c28089305f488f901aaf1dbf3fd1cb65fa3d11): `32c28089305f488f901aaf1dbf3fd1cb65fa3d11`
  Integrated revision: `6d36d483cb88973276af611198169f76f3ce05b0`.

## UE

### UE-REFLECT: Reflection and bidirectional calls

Status: **implemented**; priority: P1; references: UnLua, slua, FUnLua; execution: pending.

- Source: [UnLuaNovaBase.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaBase.cpp), [FunctionDesc.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/ReflectionUtils/FunctionDesc.cpp)
- Tests: [UnLuaNovaBase.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/UnLuaNovaBase.spec.cpp)
- Examples: [08_CppCallLua.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/08_CppCallLua.lua)

Boundary: Inherited from UnLua; no current execution evidence.

Acceptance: Test null, exception, out/ref, defaults and backend equivalence.

### UE-OVERRIDE: Blueprint overrides, inheritance and RPC overrides

Status: **partial**; priority: P1; references: UnLua, slua, FUnLua; execution: pending.

- Source: [UnLuaNovaOverrider.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaOverrider.cpp)
- Tests: [Overrides.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Overrides.spec.cpp), [LuaOverrider.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaOverrider.spec.cpp)
- Examples: [02_OverrideBlueprintEvents.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/02_OverrideBlueprintEvents.lua)

Boundary: Ordinary overrides exist; inheritance/net/reinstance combinations incomplete.

Acceptance: Run parent/child/super, interface, RPC and Blueprint recompile scenarios.

### UE-CONTAINER: Containers, structs and borrowed views

Status: **partial**; priority: P0; references: slua, FUnLua; execution: pending.

- Source: [ContainerRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/ContainerRegistry.cpp), [LuaLib_Array.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/BaseLib/LuaLib_Array.cpp)
- Tests: [LuaLib_Array.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Array.spec.cpp), [LuaLib_Map.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Map.spec.cpp), [LuaLib_Set.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Set.spec.cpp), [DanglingCheck.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/DanglingCheck.spec.cpp)
- Examples: [06_NativeContainers.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/06_NativeContainers.lua)

Boundary: Borrowed child/iterator invalidation and property metadata need lifetime audit.

Acceptance: Destroy parent, mutate/reallocate containers, reinstance Blueprint and force GC.

### UE-DELEGATE: Singlecast, multicast and callback ownership

Status: **partial**; priority: P1; references: slua, FUnLua; execution: pending.

- Source: [DelegateRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/DelegateRegistry.cpp)
- Tests: [LuaLib_Delegate.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_Delegate.spec.cpp), [LuaLib_MulticastDelegate.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_MulticastDelegate.spec.cpp)
- Examples: [05_BindDelegates.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/05_BindDelegates.lua)

Boundary: No complete token/dedup/owner teardown acceptance; preserve singlecast semantics.

Acceptance: Test duplicate binding, reentrant removal, owner destruction, hot reload, multiple Env.

### UE-INPUT: Input, animation and UI workflows

Status: **partial**; priority: P1; references: UnLua, slua, FUnLua; execution: pending.

- Source: [UnLuaNovaManager.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaManager.cpp)
- Tests: [Overrides.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Overrides.spec.cpp)
- Examples: [03_BindInputs.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/03_BindInputs.lua), [AN_FootStep.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/AN_FootStep.lua), [11_ReleaseUMG.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/11_ReleaseUMG.lua)

Boundary: Examples exist; full EnhancedInput/animation/UMG lifecycle acceptance missing.

Acceptance: Run real RHI scenarios, input rebinding, UI release and animation callbacks on both backends.

## Binding

### BIND-TEMPLATE: Plain C++ template/static exports

Status: **implemented**; priority: P1; references: slua, FUnLua; execution: pending.

- Source: [UnLuaNovaEx.inl](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaEx.inl)
- Tests: [UnLuaNovaEx.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/UnLuaNovaEx.spec.cpp)
- Examples: [09_StaticExport.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/09_StaticExport.lua)

Boundary: Not arbitrary C++ parsing; destructor and ABI behavior require current execution.

Acceptance: Test constructors/destructors, overloads, inheritance, globals and non-reflected methods.

### BIND-THUNK: Generated marshalling thunks

Status: **partial**; priority: P1; references: slua; execution: pending.

- Source: [StaticBindingThunk.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/StaticBinding/StaticBindingThunk.h)
- Tests: [StaticBinding.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/StaticBinding.spec.cpp), [StaticBindingGenerator.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/StaticBindingGenerator.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Still calls ProcessEvent; unsupported/static/net/complex signatures fall back.

Acceptance: Prove generated/fallback equivalence and add a reproducible generation example.

### BIND-INSTANCE: Independent modules on same-class instances

Status: **planned**; priority: P1; references: FUnLua; execution: pending.

- Source: [UnLuaNovaManager.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaManager.cpp)
- Tests: Missing dedicated evidence.
- Examples: Missing dedicated evidence.

Boundary: UClass-keyed cache short-circuits binding; not FUnLua-style per-instance binding.

Acceptance: Bind A/B modules to same class, verify isolation through inheritance/reload/unbind.

## Lifetime

### LIFE-MANUAL: Owned, borrowed and manual userdata finalization

Status: **partial**; priority: P0; references: Luau, FUnLua; execution: pending.

- Source: [ObjectRegistry.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/Registries/ObjectRegistry.cpp), [UnLuaNovaEx.inl](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaEx.inl)
- Tests: [Lifetime.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Lifetime.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Manual proxies, known smart pointers and Map/Set-derived arrays now have native finalizers; invalidation is distinct from destruction. Dual-GC/teardown tests added, unexecuted; full allocation audit incomplete.

Acceptance: Force both collectors; prove release counts and safety under both teardown orders.

### LIFE-ENVS: Multiple VM/World environments

Status: **partial**; priority: P0; references: slua, FUnLua; execution: pending.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp), [LuaEnvLocator.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnvLocator.cpp)
- Tests: [LuaEnv.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaEnv.spec.cpp), [Locators.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Locators.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Timer handles enforce Env ownership with close cleanup; global network cache and diagnostic attribution still need isolation work.

Acceptance: Create/destroy/recreate Env and World, reuse addresses, forbid cross-environment handles.

### LIFE-LATENT: Coroutines and latent calls

Status: **partial**; priority: P1; references: Luau, slua; execution: pending.

- Source: [LuaCoroutineTracker.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaCoroutineTracker.cpp)
- Tests: [Latent.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Latent.spec.cpp)
- Examples: [07_CallLatentFunction.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/07_CallLatentFunction.lua)

Boundary: C boundary yield and abandoned coroutine teardown need coverage.

Acceptance: Test yield/resume/errors/cancellation with Env and World teardown.

## Runtime

### RUNTIME-TIMER: World timers

Status: **partial**; priority: P0; references: slua; execution: pending.

- Source: [UnLuaNovaTimerSubsystem.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaTimerSubsystem.cpp), [LuaLib_Timer.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaLib_Timer.cpp)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [Timer.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Timer.spec.cpp)
- Examples: [TimerLifecycle.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Examples/TimerLifecycle.lua)

Boundary: Main-state callbacks, teardown cleanup, explicit World, Env-scoped handles and reentry tests implemented; pause/time dilation, multi-PIE and packaged execution still pending.

Acceptance: Test due/pause/unscaled/reentry plus coroutine GC, Env-first/World-first destruction and multi-PIE.

### RUNTIME-EVENT: Event bus and Once reentrancy

Status: **partial**; priority: P1; references: FUnLua; execution: pending.

- Source: [Event.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/Event.lua)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [RuntimeContracts.spec.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Lua/RuntimeContracts.spec.lua)
- Examples: [RuntimeContracts.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Examples/RuntimeContracts.lua)

Boundary: Tokens retain owner until explicit unsubscribe; not automatic UObject lifecycle cleanup.

Acceptance: Run recursive Once and cleanup tests; add exception/reload/owner destruction scenarios.

### RUNTIME-OOP: Pure Lua classes

Status: **implemented**; priority: P2; references: slua; execution: pending.

- Source: [OOP.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/OOP.lua)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp)
- Examples: [RuntimeContracts.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Examples/RuntimeContracts.lua)

Boundary: Lua class helper is distinct from UE UClass and experimental Luau classes.

Acceptance: Validate constructor chain, Super, class/instance identity and both backend examples.

### RUNTIME-RELOAD: Hot reload and closure identity

Status: **partial**; priority: P1; references: slua; execution: pending.

- Source: [HotReload.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/HotReload.lua), [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Luau upvalue identity is approximate; shared scalar aliases and rollback not proven.

Acceptance: Test shared closures, table cycles, delegates, active coroutine and failed reload rollback.

## Networking

### NET-RPC: Declared server/client/multicast RPC

Status: **experimental**; priority: P0; references: slua; execution: pending.

- Source: [UnLuaNovaNet.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaNet.cpp)
- Tests: [LuaNet.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaNet.spec.cpp), [LuaNetMulticastTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetMulticastTest.cpp)
- Examples: [NetActor.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Net/NetActor.lua)

Boundary: Sorted registration and whole-RPC parameter validation implemented; inheritance, class/net caches, security and cooked acceptance incomplete.

Acceptance: Connected DS + 2 clients, ownership rejection, reliable/unreliable, reconnect and schema mismatch.

### NET-PROPS: Property conditions and schemas

Status: **experimental**; priority: P0; references: slua; execution: pending.

- Source: [UnLuaNovaReplicationComponent.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaReplicationComponent.cpp), [UnLuaNovaNetPayload.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaNetPayload.cpp)
- Tests: [LuaNet.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaNet.spec.cpp), [LuaNetClientServerTest.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Tests/LuaNetClientServerTest.cpp)
- Examples: [NetActor.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Net/NetActor.lua)

Boundary: Fixed None layout; non-None/malformed declarations rejected atomically and cached schemas immutable. Full conditional layouts and multi-Env cache cleanup remain incomplete.

Acceptance: Fix layout design; test mixed conditions/types across actors, owner/nonowner, late join and dormancy.

### NET-TYPES: Replication types, int64 and Iris/push model

Status: **experimental**; priority: P0; references: slua; execution: pending.

- Source: [UnLuaNovaReplicationComponent.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaReplicationComponent.cpp), [UnLuaNovaNetDecl.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/UnLuaNovaNetDecl.h)
- Tests: [LuaNet.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaNet.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Strict int32/int64 conversion and real bucket round-trip regression sources added, unexecuted. No arbitrary struct/container support or Iris/push acceptance evidence.

Acceptance: Exact integer boundaries, object resolution, legacy/Iris/push combinations and packaged peers.

## Luau

### LUAU-SYNTAX: Language/compiler surface

Status: **partial**; priority: P1; references: Luau; execution: pending.

- Source: [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp)
- Examples: [04_LuauSyntax.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/04_LuauSyntax.lua), [01_StrictTyping.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/01_StrictTyping.lua)

Boundary: Pinned compiler supports syntax; not a full integrated conformance run.

Acceptance: Import pinned syntax/interpolation/if-expression/generics/iteration conformance with documented exclusions.

### LUAU-LIBS: base, math, string, table, utf8, bit32, coroutine

Status: **partial**; priority: P1; references: Luau; execution: pending.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp)
- Examples: [04_LuauSyntax.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/04_LuauSyntax.lua)

Boundary: VM libraries exposed; tests sample behavior rather than all upstream conformance.

Acceptance: Run library/metatable/error/weak-table conformance and host-specific exclusions.

### LUAU-VECTOR: Native vector and FVector interoperability

Status: **implemented**; priority: P1; references: Luau; execution: pending.

- Source: [LuaLib_FVector.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/MathLib/LuaLib_FVector.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp), [LuaLib_FVector.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaLib_FVector.spec.cpp)
- Examples: [05_VectorInterop.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/05_VectorInterop.lua)

Boundary: float vector versus UE double FVector loses precision.

Acceptance: Test conversion/parameter/operator edges and document precision budget.

### LUAU-BUFFER: Native buffer and UE array copies

Status: **implemented**; priority: P1; references: Luau; execution: pending.

- Source: [UnLuaNovaLib.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/UnLuaNovaLib.cpp), [Buffer.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Content/Script/UnLuaNova/Buffer.lua)
- Tests: [RuntimeModules.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/RuntimeModules.spec.cpp), [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp)
- Examples: [06_BufferInterop.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/06_BufferInterop.lua)

Boundary: Copies rather than external-memory aliases; output array type differs from uint8.

Acceptance: Bounds/type/value tests and verify source/buffer independence; measure copy cost.

### LUAU-CODEGEN: Native compilation

Status: **experimental**; priority: P1; references: Luau; execution: pending.

- Source: [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: VM-scoped initialization fixed; request counter is not success count; platform acceptance absent.

Acceptance: Test VM/coroutine lifecycle, supported architectures, debug attachment, failures and executable-memory policy.

### LUAU-GC: Incremental GC and budgets

Status: **partial**; priority: P1; references: Luau, slua; execution: pending.

- Source: [LuaGcScheduler.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaGcScheduler.cpp), [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: [LuaGcScheduler.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaGcScheduler.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Not generational or multithreaded GC; budgets are not proven hard pause limits.

Acceptance: Measure pauses and allocation pressure; validate tuning, errors and destruction callbacks.

### LUAU-SANDBOX: Readonly globals, threads and class tables

Status: **partial**; priority: P1; references: Luau; execution: pending.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp), [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp)
- Examples: [02_SandboxBoundary.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/02_SandboxBoundary.lua)

Boundary: Default off; host UE/debug/io/socket capabilities remain; not an untrusted-code permission model.

Acceptance: Define capability policy and verify writable escape paths, new threads, imports and host limits.

### LUAU-TYPES: Type inference, declarations and LSP

Status: **partial**; priority: P1; references: Luau; execution: pending.

- Source: [UnLuaNovaIntelliSenseGenerator.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNovaEditor/Private/UnLuaNovaIntelliSenseGenerator.cpp), [CheckLuauTypes.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/CheckLuauTypes.ps1)
- Tests: [IntelliSense.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/IntelliSense.spec.cpp)
- Examples: [01_StrictTyping.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/01_StrictTyping.lua)

Boundary: External checker; declarations approximate containers/delegates and skips may occur.

Acceptance: Require checker and fresh declarations; verify inference/generics/overloads/type-functions and negative fixtures.

### LUAU-REQUIRE: Host module loader versus upstream Require

Status: **partial**; priority: P1; references: Luau; execution: pending.

- Source: [LuaEnv.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaEnv.cpp), [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [LuaEnv.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaEnv.spec.cpp), [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp)
- Examples: [12_CustomLoader.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tutorials/12_CustomLoader.lua)

Boundary: Host searchers/preload differ from upstream Require relative paths/aliases/config.

Acceptance: Document resolution/cycle/cache contract and add alias/case/cook tests or explicit unsupported errors.

### LUAU-COVERAGE: Coverage export and quality gates

Status: **planned**; priority: P1; references: Luau; execution: pending.

- Source: [LuaBackend.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaBackend.cpp)
- Tests: Missing dedicated evidence.
- Examples: Missing dedicated evidence.

Boundary: Compiler option exists; complete result export, source mapping and gate are absent.

Acceptance: Produce per-source counts with exclusions, aggregate backend runs, fail on missing data.

### LUAU-EXPERIMENTAL: Experimental integer and user-defined class libraries

Status: **experimental**; priority: P2; references: Luau; execution: pending.

- Source: [Luau.Build.cs](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau.Build.cs)
- Tests: Missing dedicated evidence.
- Examples: Missing dedicated evidence.

Boundary: Upstream flag-controlled APIs are not stable integration promises.

Acceptance: Record flags, compatibility/ABI/type declarations and tests before exposing as stable.

## Compatibility

### COMPAT-DEBUG: debug, upvalues and uservalues

Status: **partial**; priority: P1; references: Luau; execution: pending.

- Source: [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Registry/uservalue placeholders and approximate upvalue identity.

Acceptance: Reject unsupported operations or implement exact subset; test closure identity and error cases.

### COMPAT-IO: Optional rooted buffered io

Status: **partial**; priority: P1; references: Luau; execution: pending.

- Source: [LuaLib_Io.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaLib_Io.cpp)
- Tests: [LuauIntegration.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuauIntegration.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: CRLF/path/read checks improved; modes/seek/errors/links/quotas remain incomplete; not Lua io parity.

Acceptance: Compare subset against Lua, including binary/large files, mutated handles, failure and root changes.

### COMPAT-LANGUAGE: Lua 5.x integer, close, goto and bytecode differences

Status: **partial**; priority: P1; references: Luau, UnLua; execution: pending.

- Source: [LuaBackend.h](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Public/LuaBackend.h), [LuaScriptCompat.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaScriptCompat.cpp)
- Tests: [LuaBackend.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaBackend.spec.cpp), [LuaScriptCompat.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaScriptCompat.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: No language-level equivalence; source-only load mode and bytecode/version policy incomplete.

Acceptance: Publish migration contracts and explicit failures; verify int64, close alternatives and trusted bytecode.

## Tooling

### DEBUG-DAP: Debug service and DAP

Status: **partial**; priority: P1; references: slua, Luau; execution: pending.

- Source: [LuaDebugService.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaDebugService.cpp), [LuaDebugAdapter.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNovaEditor/Private/LuaDebugAdapter.cpp)
- Tests: [DebugService.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/DebugService.spec.cpp), [DebugAdapter.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/DebugAdapter.spec.cpp)
- Examples: [03_NativeDebug.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Luau/03_NativeDebug.lua)

Boundary: Attach/evaluate/pause exist; packaged remote support, authorization and reconnect unverified.

Acceptance: Break/step/locals/upvalues across multiple Env and coroutine; malformed frames and disconnect cleanup.

### PROFILE: CPU, self time, memory and flamegraphs

Status: **partial**; priority: P1; references: slua, Luau; execution: pending.

- Source: [LuaProfiler.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaProfiler.cpp), [perfgraph.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/LuauPerfgraph/perfgraph.py)
- Tests: [Observability.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Observability.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Sampled/measured attribution differs; tool wrapper requires initialized pinned VM tools.

Acceptance: Validate stack/yield/GC attribution, overhead and malformed dumps; produce an end-to-end example.

### WATCHDOG: Dead-loop and coroutine diagnostics

Status: **partial**; priority: P1; references: slua; execution: pending.

- Source: [LuaCoroutineTracker.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/Private/LuaCoroutineTracker.cpp)
- Tests: [LuaDeadLoopCheck.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaDeadLoopCheck.spec.cpp), [ErrorHandling.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/ErrorHandling.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Detection exists; quotas and cancellation are not complete hostile-code controls.

Acceptance: Test timeout/reset/teardown, native callbacks and abandoned coroutine attribution.

## Product

### EXTENSIONS: Socket, protobuf and JSON

Status: **partial**; priority: P1; references: UnLua, slua; execution: pending.

- Source: [pb.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaExtensions/LuaProtobuf/Source/src/pb.cpp), [luasocket.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaExtensions/LuaSocket/Source/src/luasocket.cpp), [rapidjson.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaExtensions/LuaRapidjson/Source/src/rapidjson.cpp)
- Tests: [Extensions.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Extensions.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Backend lifetime/ABI/error and distribution notices need component-level verification.

Acceptance: Large/malformed payloads, socket close/failure, userdata GC and packaged linkage on both backends.

## Quality

### QA-PROVENANCE: Test framework, reports and build identity

Status: **partial**; priority: P1; references: UnLua, slua, FUnLua; execution: pending.

- Source: [RunTests.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/RunTests.ps1), [ReleasePipeline.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ReleasePipeline.ps1), [ValidateRelease.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ValidateRelease.py), [GenerateTestReport.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/GenerateTestReport.py), [ClassifyTests.py](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/ClassifyTests.py)
- Tests: [LuaSpecs.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/LuaSpecs.spec.cpp)
- Examples: Missing dedicated evidence.

Boundary: Release orchestration, runtime manifests, strict evidence gates and provenance are implemented but unexecuted; source classification is not code coverage.

Acceptance: Fail missing/empty/malformed runs; record commit/dirty/dependencies/flags/topology; require type tools.

### PERFORMANCE: Comparable benchmarks and regression gates

Status: **partial**; priority: P1; references: slua, FUnLua; execution: pending.

- Source: [RunTests.ps1](https://github.com/HUSTJJD/UnLuaNova/blob/master/Tools/RunTests.ps1)
- Tests: [Benchmark.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/Benchmark.spec.cpp), [BenchmarkParity.spec.cpp](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/BenchmarkParity.spec.cpp)
- Examples: [BP_UnLuaNovaBenchmark.lua](https://github.com/HUSTJJD/UnLuaNova/blob/master/Content/Script/Tests/Benchmark/BP_UnLuaNovaBenchmark.lua)

Boundary: Threshold code exists; committed perf baselines and controlled external results absent.

Acceptance: Same hardware/UE/config, raw multi-run distributions, memory/startup/GC/code-size metrics.

## Product

### PACKAGING: Cooked targets, platforms and upgrades

Status: **partial**; priority: P1; references: slua, FUnLua; execution: pending.

- Source: [UnLuaNova.Build.cs](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/UnLuaNova/UnLuaNova.Build.cs)
- Tests: Missing dedicated evidence.
- Examples: Missing dedicated evidence.

Boundary: Win64 Development cooked automation, Server build and Shipping packaging are orchestrated; execution, Shipping runtime and other platforms remain unverified.

Acceptance: Cooked scripts/assets/extensions, DS/client, feature-off, executable-memory targets and upgrade tests.

## Release

### LEGAL: License scope and provenance

Status: **partial**; priority: P0; references: UnLua, slua, FUnLua, Luau; execution: pending.

- Source: [LICENSE](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE), [LICENSE-THIRD-PARTY.md](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE-THIRD-PARTY.md)
- Tests: Missing dedicated evidence.
- Examples: Missing dedicated evidence.

Boundary: Root/file scope unresolved; extension notices incomplete; FUnLua license file missing.

Acceptance: Rights-holder scope decision, file/component provenance, complete notices in distributable artifact.

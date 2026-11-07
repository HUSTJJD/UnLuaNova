# Architecture

This page describes UnLuaNova's modules, call paths and ownership boundaries.

## Modules

- Plugins/UnLuaNova/Source/UnLuaNova: runtime Env, bindings, overrides, libraries, diagnostics and experimental networking.
- Plugins/UnLuaNova/Source/UnLuaNovaEditor: editor tools, declarations/binding generation and DAP adapter.
- Plugins/UnLuaNova/Source/UnLuaNovaDefaultParamCollector: UBT default-parameter collector.
- Plugins/UnLuaNova/Source/ThirdParty: pinned VM submodules and wrappers.
- Plugins/UnLuaNova/Content/Script/UnLuaNova: shipped script modules.
- Plugins/UnLuaNovaExtensions and Plugins/UnLuaNovaTestSuite: sibling extension and test plugins.
- Content/Script and Tools: demo scripts/Lua specs and test/report/typecheck tooling.

ReflectionUtils describes UE metadata; Registries maps classes, objects, functions, properties, delegates and containers; BaseLib/MathLib expose APIs. UBT builds VM source.

## Backend and Calls

LuaBackend.h/.cpp centralize capabilities, compilation, GC and CodeGen; LuaScriptCompat.cpp implements a partial compatibility layer. HAS_GC_METAMETHOD is 1 on Lua and 0 on Luau. HAS_USERDATA_FINALIZER is 1 on both via different mechanisms. Luau vector/buffer/readonly/debug primitives have no equivalent Lua API; integer C APIs also differ (64 versus 32 bits).

An Env locator selects the VM; TryBind and UnLuaNovaManager load a module, BindClass caches by UClass, and ObjectRegistry registers the instance. Separate instances of the same class do not receive independent modules. LuaCore and property descriptors resolve fields; writable class tables cache results, while frozen tables skip cache writes.

FLuaOverrides/ULuaFunction replace thunks and script entry points; self.Overridden reaches the original. Engine-to-Lua calls share lookup/marshalling/error paths; latent calls also need coroutine keepalive and legal yield/resume boundaries. Reflection, CallUEFast, template exports and generated thunks are distinct paths. Generated thunks still invoke ProcessEvent and unsupported signatures fall back.

## Ownership and Tools

Env owns the VM and registries; UE owns UObjects and Worlds. Borrowed struct/container views, callbacks and manual references need explicit lifetime contracts. Containers/FText/nontrivial exported constructors use finalizers, but manual proxies and Timer teardown require explicit lifetime management. Luau destructors must not touch the Lua stack.

Automatic GC delegates scheduling to the VM. TimeSliced requests steps under a per-frame budget; this is not a hard pause bound or multithreaded GC.

Editor-generated declarations feed external LuaLS/luau-lsp, not runtime type checking. Runtime debug service and editor DAP adapter have different deployment scope. Experimental networking creates UFunctions and uses typed replicated buckets. Unconditional fields live in per-type arrays so push model can dirty them individually; conditional fields live in one `FLuaNetBuckets` group per condition, because UE decides what to send from the property's class-cached registration and that cannot depend on an instance or a script value. A field's address is therefore (condition group, type bucket, index). A twelfth `Generic` bucket carries `UStruct` / container type specs as a canonical self-describing payload, with the spec folded into `SchemaVersion`; RPC parameters of those types are attached to the runtime-built `UFunction` as real `FStructProperty` / `FArrayProperty` / `FSetProperty` / `FMapProperty`. See [networking](LuaNetworking.md).

# Architecture

Current module responsibilities and call paths are described here; acceptance state lives in the [capability matrix](../feature-matrix.md).

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

An Env locator selects the VM; TryBind and UnLuaNovaManager load a module, BindClass caches by UClass, and ObjectRegistry registers the instance. This does not establish independent modules on same-class instances. LuaCore and property descriptors resolve fields; writable class tables cache results, while frozen tables skip cache writes.

FLuaOverrides/ULuaFunction replace thunks and script entry points; self.Overridden reaches the original. Engine-to-Lua calls share lookup/marshalling/error paths; latent calls also need coroutine keepalive and legal yield/resume boundaries. Reflection, CallUEFast, template exports and generated thunks are distinct paths. Generated thunks still invoke ProcessEvent and unsupported signatures fall back.

## Ownership and Tools

Env owns the VM and registries; UE owns UObjects and Worlds. Borrowed struct/container views, callbacks and manual references need explicit lifetime contracts. Containers/FText/nontrivial exported constructors use finalizers, but manual proxies and Timer teardown remain open. Luau destructors must not touch the Lua stack.

Automatic GC delegates scheduling to the VM. TimeSliced requests steps under a per-frame budget; this is not a hard pause bound or multithreaded GC and does not establish equal latency.

Editor-generated declarations feed external LuaLS/luau-lsp, not runtime type checking. Runtime debug service and editor DAP adapter have different deployment scope. Experimental networking creates UFunctions and uses typed replicated buckets; instance-derived conditions conflict with class-cached layouts. See [networking](LuaNetworking.md).

Changes must specify backend/error/ownership/platform contracts, update the matrix, and add meaningful tests/examples. Source existence and old screenshots are not current acceptance evidence.

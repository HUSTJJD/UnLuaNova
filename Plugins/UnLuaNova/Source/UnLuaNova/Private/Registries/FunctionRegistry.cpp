#include "Registries/FunctionRegistry.h"
#include "UnLuaNovaCompatibility.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaNetPolicy.h"
#include "UObject/UnrealType.h"
#include "LuaBackend.h"
#include "LuaEnv.h"
#include "UnLuaNovaManager.h"

namespace UnLuaNova
{
    FFunctionRegistry::FFunctionRegistry(FLuaEnv* Env)
        : Env(Env)
    {
    }

    void FFunctionRegistry::NotifyUObjectDeleted(UObject* Object)
    {
        const auto Function = (ULuaFunction*)Object;
        const auto Info = LuaFunctions.Find(Function);
        if (!Info)
            return;
        luaL_unref(Env->GetMainState(), LUA_REGISTRYINDEX, Info->LuaRef);
        LuaFunctions.Remove(Function);
    }

    // An override can be called before anything bound its owner: binding is lazy, and an
    // earlier attempt may simply have failed. Bind on the way in, then take whatever
    // reference the registry holds - which is the same value either way.
    static int32 GetOrBindSelfRef(FLuaEnv* Env, UObject* Context)
    {
        if (UNLIKELY(!Env->GetObjectRegistry()->IsBound(Context)))
            Env->TryBind(Context);

        const auto SelfRef = Env->GetObjectRegistry()->GetBoundRef(Context);
        check(SelfRef != LUA_NOREF);
        return SelfRef;
    }

    // Resolve the Lua function from the INSTANCE metatable (module table), following the
    // Super chain. The returned ref is owned by the caller.
    static lua_Integer ResolveFuncRef(lua_State* L, int32 SelfRef, const char* FunctionName)
    {
        lua_Integer FuncRef = LUA_NOREF;
        lua_rawgeti(L, LUA_REGISTRYINDEX, SelfRef);
        if (!lua_getmetatable(L, -1))
        {
            lua_pop(L, 1);
            return LUA_NOREF;
        }
        do
        {
            lua_pushstring(L, FunctionName);
            lua_rawget(L, -2);
            if (lua_isfunction(L, -1))
            {
                lua_pushvalue(L, -3);
                lua_remove(L, -3);
                lua_remove(L, -3);
                lua_pushvalue(L, -2);
                FuncRef = luaL_ref(L, LUA_REGISTRYINDEX);
                break;
            }
            lua_pop(L, 1);
            lua_pushstring(L, "Super");
            lua_rawget(L, -2);
            lua_remove(L, -2);
        }
        while (lua_istable(L, -1));
        lua_pop(L, 2);
        return FuncRef;
    }

    void FFunctionRegistry::Invoke(ULuaFunction* Function, UObject* Context, FFrame& Stack, RESULT_DECL)
    {
        // B4. The engine's own owner check only guards Server RPCs, and declared
        // RPCs are built at runtime, so no _Validate function exists. Every declared
        // RPC reaches this Lua thunk: apply the host's application-level policy here
        // before resolving or invoking the handler.
        if (Function && Function->IsLuaDeclaredRpc())
        {
            ELuaRpcKind Kind = ELuaRpcKind::Server;
            if (Function->HasAnyFunctionFlags(FUNC_NetMulticast))
                Kind = ELuaRpcKind::Multicast;
            else if (Function->HasAnyFunctionFlags(FUNC_NetClient))
                Kind = ELuaRpcKind::Client;

            FString Reason;
            const ELuaNetPolicyResult Verdict = FUnLuaNovaNetPolicy::Get().CanInvoke(
                Context, Function->GetFName(), Kind, Reason);
            if (Verdict != ELuaNetPolicyResult::Ok)
            {
                UNLUANOVA_LOGF(LogUnLuaNova, Warning,
                    TEXT("Declared RPC '%s' refused (%s): %s"),
                    *Function->GetName(), LexToString(Verdict), *Reason);
                // Consume the call through the overridden function so the VM stack
                // stays consistent. Newly declared RPCs normally have no override,
                // which means nothing is executed at all.
                const auto Overridden = Function->GetOverridden();
                if (Overridden && Stack.Code)
                    Overridden->Invoke(Context, Stack, RESULT_PARAM);
                return;
            }
        }

        const auto SelfRef = GetOrBindSelfRef(Env, Context);
        const auto L = Env->GetMainState();

        // Overrides live on the UClass, but an instance-bound object resolves functions
        // from its own module table. A class-level FuncRef cache would hand every instance
        // the closure resolved for the first one, so that path resolves per call instead.
        const bool bPerInstance = Env->GetManager()->HasInstanceBinding(Context);

        auto* Info = LuaFunctions.Find(Function);
        if (!Info)
        {
            FFunctionInfo NewInfo;
            NewInfo.Desc = TUniquePtr<FFunctionDesc>(new FFunctionDesc(Function, nullptr));
            Info = &LuaFunctions.Add(Function, MoveTemp(NewInfo));
        }

        FFunctionDesc* FuncDesc = Info->Desc.Get();
        lua_Integer FuncRef;
        bool bTemporaryRef = false;
        if (bPerInstance || !Info->bResolved)
        {
            FuncRef = ResolveFuncRef(L, SelfRef, FuncDesc->GetLuaFunctionName());
            bTemporaryRef = bPerInstance;
            if (!bPerInstance)
            {
                Info->LuaRef = FuncRef;
                Info->bResolved = true;
            }
        }
        else
        {
            FuncRef = Info->LuaRef;
        }

        if (FuncRef == LUA_NOREF)
        {
            if (Function->IsLuaDeclaredRpc())
                UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("Declared RPC handler missing: %s"), UTF8_TO_TCHAR(FuncDesc->GetLuaFunctionName()));
            // 可能因为Lua模块加载失败导致找不到对应的function，转发给原函数
            const auto Overridden = Function->GetOverridden();
            if (Overridden && Stack.Code)
                Overridden->Invoke(Context, Stack, RESULT_PARAM);
            return;
        }
        FuncDesc->CallLua(L, FuncRef, SelfRef, Stack, RESULT_PARAM);
        if (bTemporaryRef)
            luaL_unref(L, LUA_REGISTRYINDEX, FuncRef);
    }
}

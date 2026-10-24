// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

#include "UnLuaNovaOverrider.h"

#include "LuaCore.h"
#include "LuaDanglingCheck.h"
#include "LuaDeadLoopCheck.h"
#include "Registries/ObjectRegistry.h"
#include "UnLuaNovaManager.h"
#include "UnLuaNovaModule.h"

namespace UnLuaNova
{
    namespace
    {
        FLuaEnv* GetEnvFor(const UObject* Self)
        {
            if (!Self)
                return nullptr;

            IUnLuaNovaModule& Module = IUnLuaNovaModule::Get();
            if (!Module.IsActive())
                return nullptr;

            // Never the main env: ULuaEnvLocator_ByGameInstance hands out one env per
            // GameInstance, and asking the wrong one for this object's table finds
            // nothing - or worse, finds another world's table.
            return Module.GetEnv(const_cast<UObject*>(Self));
        }

        struct FLuaCallGuardPair
        {
            TUniquePtr<FDanglingCheck::FGuard> Dangling;
            TUniquePtr<FDeadLoopCheck::FGuard> DeadLoop;
        };
    }

    bool TryHookLua(UObject* Self)
    {
        FLuaEnv* Env = GetEnvFor(Self);
        if (!Env)
            return false;

        if (Env->GetObjectRegistry()->GetBoundRef(Self) != LUA_NOREF)
            return true;

        return Env->TryBind(Self);
    }

    bool IsLuaFunctionExist(const UObject* Self, const FString& FunctionName)
    {
        FLuaEnv* Env = GetEnvFor(Self);
        if (!Env)
            return false;

        lua_State* L = Env->GetMainState();
        const int32 Top = lua_gettop(L);

        const FTCHARToUTF8 Name(*FunctionName);

        // PushFunction is the lookup the override path uses, so "C++ can call it" and
        // "Lua can call it" cannot drift apart. It leaves [handler][function][self] and
        // hands back a registry ref we have to release - we only want the stack layout.
        const int32 Ref = PushFunction(L, const_cast<UObject*>(Self), Name.Get());
        if (Ref != LUA_NOREF)
            luaL_unref(L, LUA_REGISTRYINDEX, Ref);

        lua_settop(L, Top);
        return Ref != LUA_NOREF;
    }

    FLuaMemberCall BeginLuaMemberCall(const UObject* Self, const FString& FunctionName, bool bRequireExist)
    {
        FLuaMemberCall Call;

        FLuaEnv* Env = GetEnvFor(Self);
        if (!Env)
        {
            if (!bRequireExist)
                UE_LOG(LogUnLuaNova, Warning, TEXT("No lua env available for '%s' - cannot call Lua function '%s'."),
                    *GetNameSafe(Self), *FunctionName);
            return Call;
        }

        // Bind on demand. NotifyUObjectCreated already tries for every object, but it
        // fires before instance properties are copied from the archetype, so an object
        // whose module comes from an instance-held property is not bound yet here.
        if (Env->GetObjectRegistry()->GetBoundRef(Self) == LUA_NOREF)
        {
            if (!Env->TryBind(const_cast<UObject*>(Self)))
            {
                if (!bRequireExist)
                    UE_LOG(LogUnLuaNova, Warning, TEXT("Failed to bind a lua module to '%s' - cannot call Lua function '%s'."),
                        *GetNameSafe(Self), *FunctionName);
                return Call;
            }
        }

        lua_State* L = Env->GetMainState();
        Call.Top = lua_gettop(L);

        const FTCHARToUTF8 Name(*FunctionName);
        const int32 Ref = PushFunction(L, const_cast<UObject*>(Self), Name.Get());

        if (Ref == LUA_NOREF)
        {
            // PushFunction already restored the stack. Nothing to call.
            if (!bRequireExist)
                UE_LOG(LogUnLuaNova, Warning, TEXT("Lua function '%s' does not exist on '%s'."), *FunctionName, *GetNameSafe(Self));
            return Call;
        }

        // PushFunction refs the function so callers can hold onto it. This call is
        // immediate: the value is already on the stack, and keeping the ref would leak
        // one registry slot per call.
        luaL_unref(L, LUA_REGISTRYINDEX, Ref);

        Call.L = L;
        Call.Env = Env;
        return Call;
    }

    FLuaRetValues EndLuaMemberCall(const FLuaMemberCall& Call, int32 NumArgs)
    {
        if (!Call.IsValid())
            return FLuaRetValues(Call.L, INDEX_NONE);

        lua_State* L = Call.L;

        // CallFunction(L, NumArgs, LUA_MULTRET) pops the function and its arguments,
        // pushes everything the chunk returned, then removes the message handler - so
        // what is left above Call.Top is exactly the result list.
        if (!CallFunction(L, NumArgs, LUA_MULTRET))
        {
            lua_settop(L, Call.Top);
            return FLuaRetValues(L, INDEX_NONE);
        }

        return FLuaRetValues(L, lua_gettop(L) - Call.Top);
    }

    void* AcquireLuaCallGuard(FLuaEnv* Env)
    {
        if (!Env)
            return nullptr;

        // Same order as FLuaEnv::DoString(): the dangling check has to be armed before
        // the watchdog, or a script that trips the watchdog leaves the dangling guard
        // counting captures that never get cleared.
        auto* Pair = new FLuaCallGuardPair();
        Pair->Dangling = Env->GetDanglingCheck()->MakeGuard();
        Pair->DeadLoop = Env->GetDeadLoopCheck()->MakeGuard();
        return Pair;
    }

    void ReleaseLuaCallGuard(void* Guard)
    {
        delete static_cast<FLuaCallGuardPair*>(Guard);
    }
}

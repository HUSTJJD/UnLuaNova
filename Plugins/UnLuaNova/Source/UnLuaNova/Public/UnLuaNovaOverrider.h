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

#pragma once

#include "CoreMinimal.h"
#include "LuaEnv.h"
#include "LuaValue.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaLegacy.h"

#include <type_traits>

/**
 * C++ -> Lua member calls: the missing half of the binding.
 *
 * UnLuaNova has always been able to replace a UFunction with a Lua function, and it can
 * call a Lua *global* (UnLuaNova::Call / CallTableFunc). What it could not do is say
 * "call this Lua method on this object and give me the result back as a bool":
 *
 *     const bool bEnabled = UnLuaNova::CallLuaFunction<bool>(MyActor, TEXT("IsTestEnable"), 1, 2);
 *
 * This is deliberately a thin wrapper. The lookup is PushFunction() - the same function
 * the override path uses - so "C++ can call it" and "Lua can call it" mean the same thing
 * by construction: same self table, same Super chain, same ReportLuaCallError. An earlier
 * version of this file hand-rolled the metatable walk and immediately started drifting
 * from PushFunction's semantics. Reuse, don't reimplement.
 *
 * What this is NOT: it is not a replacement for the override system. If the engine is
 * supposed to be the caller, override a UFunction - that is the mechanism with
 * self.Overridden and tick ordering behind it. This is for C++ that genuinely wants to
 * reach into a Lua module.
 */
namespace UnLuaNova
{
    /**
     * A C++ -> Lua member call in progress.
     *
     * BeginLuaMemberCall leaves the stack as [message handler][function][self]; the caller
     * pushes arguments (self counts as the first one) and hands the struct back to
     * EndLuaMemberCall. Split in two so the variadic argument marshalling can stay a
     * template in the header while the VM-facing part stays in the .cpp.
     */
    struct FLuaMemberCall
    {
        lua_State* L = nullptr;
        FLuaEnv* Env = nullptr;

        /**
         * Stack depth before anything was pushed. Every path out restores to this, so a
         * failed call on the main state leaves no residue for the next caller to trip on.
         */
        int32 Top = 0;

        bool IsValid() const { return L != nullptr && Env != nullptr; }
    };

    /**
     * Make sure a Lua module is bound to this object. Idempotent: an object that is
     * already bound costs one map lookup. Returns false when no module could be located
     * or the module failed to load.
     */
    UNLUANOVA_API bool TryHookLua(UObject* Self);

    /** Whether a Lua function of this name is reachable from the object's Lua table. */
    UNLUANOVA_API bool IsLuaFunctionExist(const UObject* Self, const FString& FunctionName);

    /**
     * Resolve the Lua function and lay out the stack. Returns an invalid FLuaMemberCall
     * (and a restored stack) when there is nothing to call - see bRequireExist for
     * whether a missing function is logged as an error.
     */
    UNLUANOVA_API FLuaMemberCall BeginLuaMemberCall(const UObject* Self, const FString& FunctionName, bool bRequireExist);

    /** Execute the call. NumArgs includes self. Leaves the results on the stack. */
    UNLUANOVA_API FLuaRetValues EndLuaMemberCall(const FLuaMemberCall& Call, int32 NumArgs);

    /**
     * Per-call VM guards (dead loop watchdog + dangling pointer check) as an opaque
     * handle, so the header does not have to see FDeadLoopCheck's definition. Both are
     * RAII underneath; the pointer is only handed out between Begin and End.
     */
    UNLUANOVA_API void* AcquireLuaCallGuard(FLuaEnv* Env);
    UNLUANOVA_API void ReleaseLuaCallGuard(void* Guard);

    struct FLuaCallGuardScope
    {
        void* Handle;

        explicit FLuaCallGuardScope(FLuaEnv* Env)
            : Handle(Env ? AcquireLuaCallGuard(Env) : nullptr)
        {
        }

        ~FLuaCallGuardScope()
        {
            if (Handle)
                ReleaseLuaCallGuard(Handle);
        }

        FLuaCallGuardScope(const FLuaCallGuardScope&) = delete;
        FLuaCallGuardScope& operator=(const FLuaCallGuardScope&) = delete;
    };

    // Declared before the entry points that call it: CallLuaFunctionImpl<Ret>(...) spells
    // the template arguments explicitly, so the name has to be known to be a template at
    // that point - a later definition is not enough for the parser.
    template <typename Ret, typename... Args>
    Ret CallLuaFunctionImpl(UObject* Self, const FString& FunctionName, bool bRequireExist, Args&&... InArgs);

    /**
     * Call a Lua member function on a bound UObject and convert the first result.
     *
     * Logs and returns a default-constructed Ret when the object has no Lua module or the
     * function does not exist. Use CallLuaFunctionIfExist where "no such function" is a
     * legitimate state - optional hooks and handlers.
     */
    template <typename Ret, typename... Args>
    Ret CallLuaFunction(UObject* Self, const FString& FunctionName, Args&&... InArgs)
    {
        return CallLuaFunctionImpl<Ret>(Self, FunctionName, false, Forward<Args>(InArgs)...);
    }

    template <typename Ret, typename... Args>
    Ret CallLuaFunctionIfExist(UObject* Self, const FString& FunctionName, Args&&... InArgs)
    {
        return CallLuaFunctionImpl<Ret>(Self, FunctionName, true, Forward<Args>(InArgs)...);
    }

    /** Convenience for the void case; identical to CallLuaFunction<void>. */
    template <typename... Args>
    void CallLuaFunctionVoid(UObject* Self, const FString& FunctionName, Args&&... InArgs)
    {
        CallLuaFunctionImpl<void>(Self, FunctionName, false, Forward<Args>(InArgs)...);
    }

    // Shared implementation. The void branch is a separate if constexpr path because a
    // function returning void cannot spell "return Ret();".
    template <typename Ret, typename... Args>
    Ret CallLuaFunctionImpl(UObject* Self, const FString& FunctionName, bool bRequireExist, Args&&... InArgs)
    {
        const FLuaMemberCall Call = BeginLuaMemberCall(Self, FunctionName, bRequireExist);
        if (!Call.IsValid())
        {
            if constexpr (std::is_void_v<Ret>)
                return;
            else
                return Ret();
        }

        const FLuaCallGuardScope Guard(Call.Env);

        // self is argument 1 of every Lua method call; the rest follow it.
        const int32 NumArgs = PushArgs<false>(Call.L, Forward<Args>(InArgs)...) + 1;

        FLuaRetValues RetValues = EndLuaMemberCall(Call, NumArgs);

        if constexpr (std::is_void_v<Ret>)
        {
            return;
        }
        else
        {
            if (!RetValues.IsValid() || RetValues.Num() < 1)
                return Ret();
            return RetValues[0].Value<Ret>();
        }
    }
}

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
#include "UnLuaNovaNetDecl.h"

struct lua_State;
class UClass;
class UFunction;

namespace UnLuaNova
{
    /**
     * Lua-declared networking: RPCs and replicated fields.
     *
     * How RPCs are built, and why it goes through the existing override machinery:
     * FLuaOverrides::Override() already has an "add a brand new function" path - it is
     * the bAddNew branch, taken whenever the source UFunction's outer is not the target
     * class - and ULuaFunction::SetActive() then does AddFunctionToFunctionMap() plus
     * AddNativeFunction(). So declaring an RPC is a matter of *building the UFunction*
     * (flags + typed parameters) and handing it to ULuaFunction::Override(). That gets
     * us PIE teardown for free too: RestoreOverrides() removes what was added.
     *
     * What is NOT reused, and why: we do not set FUNC_BlueprintEvent. An RPC is not an
     * event anyone overrides, and ULuaFunction::IsOverridable() explicitly rejects
     * anything with FUNC_Net anyway.
     */
    class UNLUANOVA_API FUnLuaNovaNet
    {
    public:
        /**
         * Read the declarations off a bound module table and register them on the class.
         *
         * Recognised module fields, all optional:
         *
         *     M.ServerRPC     = { Foo = true, Bar = { Reliable = true, Params = { "Int", "Str" } } }
         *     M.ClientRPC     = { ... }
         *     M.MulticastRPC  = { ... }
         *
         * Idempotent: a name that already exists on the class is skipped with a warning
         * rather than overwritten, so re-binding after a hot reload cannot clobber an
         * RPC the engine already owns.
         */
        static void DeclareRPCs(lua_State* L, int32 ModuleTableIdx, UClass* Class);

        /**
         * Read the replicated-field declarations off a bound module table.
         *
         *     function M:GetLifetimeReplicatedProps()
         *         return { { Name = "HP", Type = "Float", Condition = "OwnerOnly" } }
         *     end
         *
         * A plain table field `M.ReplicatedProps` with the same shape is accepted too,
         * which is what you want when the list is static and there is no reason to pay
         * for a function call at bind time.
         */
        static void DeclareReplicatedProps(lua_State* L, int32 ModuleTableIdx, UClass* Class);

        /** Declarations registered for a class, empty when it declared none. */
        static const TArray<FLuaRepDecl>& GetReplicatedProps(const UClass* Class);

        /** Forget everything. Called from UUnLuaNovaManager::Cleanup(). */
        static void Cleanup();

    private:
        static TMap<TWeakObjectPtr<UClass>, TArray<FLuaRepDecl>> RepProps;
    };
}

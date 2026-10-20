// Tencent is pleased to support the open source community by making UnLuaNova available.
// 
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include "LowLevel.h"
#include "LuaCore.h"

namespace UnLuaNova
{
    namespace LowLevel
    {
        int EmptyIterator(lua_State* L)
        {
            lua_pushnil(L);
            return 1;
        }

        /**
         * Create weak key table
         */
        void CreateWeakKeyTable(lua_State* L)
        {
            lua_newtable(L);
            lua_newtable(L);
            lua_pushstring(L, "__mode");
            lua_pushstring(L, "k");
            lua_rawset(L, -3);
            lua_setmetatable(L, -2);
        }

        /**
         * Create weak value table
         */
        void CreateWeakValueTable(lua_State* L)
        {
            lua_newtable(L);
            lua_newtable(L);
            lua_pushstring(L, "__mode");
            lua_pushstring(L, "v");
            lua_rawset(L, -3);
            lua_setmetatable(L, -2);
        }

        FString GetMetatableName(const UObject* Object)
        {
            if (!Object)
                return "";

            if (UNLIKELY(Object->IsA<UEnum>()))
            {
                return Object->IsNative() ? Object->GetName() : Object->GetPathName();
            }

            const UStruct* Struct = Cast<UStruct>(Object);
            if (Struct)
                return GetMetatableName(Struct);

            Struct = Object->GetClass();
            return GetMetatableName(Struct);
        }

        FString GetMetatableName(const UStruct* Struct)
        {
            if (!Struct)
                return "";

            if (Struct->IsNative())
                return FString::Printf(TEXT("%s%s"), Struct->GetPrefixCPP(), *Struct->GetName());
            return Struct->GetPathName();
        }

        void GetFunctionNames(lua_State* L, const int TableRef, TSet<FName>& FunctionNames)
        {
            const auto Type = lua_rawgeti(L, LUA_REGISTRYINDEX, TableRef);
            if (Type == LUA_TNIL)
                return;

            static auto GetFunctionName = [](lua_State* L, void* Userdata)
            {
                const auto ValueType = lua_type(L, -1);
                if (ValueType != LUA_TFUNCTION)
                    return true;

                auto Names = (TSet<FName>*)Userdata;
                FString FuncName(lua_tostring(L, -2));
                if (FuncName.EndsWith(TEXT("_RPC")))
                    FuncName = FuncName.Left(FuncName.Len() - 4);
                Names->Add(FName(*FuncName));
                return true;
            };

            auto N = 1;
            bool bNext;
            do
            {
                bNext = TraverseTable(L, -1, &FunctionNames, GetFunctionName) > INDEX_NONE;
                if (bNext)
                {
                    lua_pushstring(L, "Super");
                    lua_rawget(L, -2);
                    ++N;
                    bNext = lua_istable(L, -1);
                }
            }
            while (bNext);
            lua_pop(L, N);
        }

        int GetLoadedModule(lua_State* L, const char* ModuleName)
        {
            if (!ModuleName)
            {
                UE_LOG(LogUnLuaNova, Warning, TEXT("%s, Invalid module name!"), ANSI_TO_TCHAR(__FUNCTION__));
                return LUA_TNIL;
            }

            lua_getglobal(L, "package");

            // Luau ships no package library, so "package" is nil here. Indexing nil raises
            // an index error, and this runs straight from C++ - with no errorJmp to unwind
            // to, Luau aborts the process. Bail out before touching it; the answer is
            // simply "module not loaded" on that backend.
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                return LUA_TNIL;
            }

            lua_getfield(L, -1, "loaded");
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 2);
                return LUA_TNIL;
            }

            int32 Type = lua_getfield(L, -1, ModuleName);
            lua_remove(L, -2);
            lua_remove(L, -2);
            return Type;
        }

        bool CheckPropertyOwner(lua_State* L, UnLuaNova::ITypeOps* InProperty, void* InContainerPtr)
        {
#if ENABLE_TYPE_CHECK == 1
            if (InProperty->StaticExported)
                return true;

            UnLuaNova::ITypeInterface* TypeInterface = (UnLuaNova::ITypeInterface*)InProperty;
            FProperty* Property = TypeInterface->GetUProperty();
            if (!Property)
                return true;

            UObject* Object = (UObject*)InContainerPtr;
            UClass* OwnerClass = Property->GetOwnerClass();
            if (!OwnerClass)
                return true;

            if (Object->IsA(OwnerClass))
                return true;

            luaL_error(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("Access property from invalid owner. %s should be a %s."), *Object->GetName(), *OwnerClass->GetName())));
            return false;
#else
            return true;
#endif
        }

        void* GetUserdata(lua_State* L, int32 Index, bool* OutTwoLvlPtr, bool *OutClassMetatable)
        {
            Index = AbsIndex(L, Index);

            void* Userdata = nullptr;
            bool bTwoLvlPtr = false, bClassMetatable = false;

            int32 Type = lua_type(L, Index);
            switch (Type)
            {
            case LUA_TTABLE:
                {
                    lua_pushstring(L, "Object");
                    Type = lua_rawget(L, Index);
                    if (Type == LUA_TUSERDATA)
                    {
                        // Not lua_touserdata(): that hands back the start of the block,
                        // which is the FUserdataDesc ([desc][padding][payload]), while
                        // every caller dereferences this as a two level pointer. Reading
                        // the block start instead of the payload made the "UObject" be
                        // the descriptor bytes themselves - magic 0x1688 plus the
                        // variant/two-level tag 0xA0 - and the first dereference of that
                        // pointer crashed. GetUserdataFast() walks past the descriptor.
                        Userdata = ::GetUserdataFast(L, -1, &bTwoLvlPtr); // get the raw UObject
                    }
                    else
                    {
                        lua_pop(L, 1);
                        lua_pushstring(L, "ClassDesc");
                        Type = lua_rawget(L, Index);
                        if (Type == LUA_TLIGHTUSERDATA)
                        {
                            Userdata = lua_touserdata(L, -1); // get the 'FClassDesc' pointer
                            bClassMetatable = true;
                        }
                    }
                    bTwoLvlPtr = true; // set two level pointer flag
                    lua_pop(L, 1);
                }
                break;
            case LUA_TUSERDATA:
                Userdata = GetUserdataFast(L, Index, &bTwoLvlPtr); // get the userdata pointer
                break;
            default:
                break;
            }

            if (OutTwoLvlPtr)
                *OutTwoLvlPtr = bTwoLvlPtr;

            if (OutClassMetatable)
                *OutClassMetatable = bClassMetatable;

            return Userdata;
        }

        uint8 CalculateUserdataPadding(UStruct* Struct)
        {
            const auto ScriptStruct = Cast<UScriptStruct>(Struct);
            if (!ScriptStruct)
                return 0;

            const auto CppStructOps = ScriptStruct->GetCppStructOps();
            const auto Alignment = CppStructOps ? CppStructOps->GetAlignment() : ScriptStruct->GetMinAlignment();
            return CalcUserdataPadding(Alignment);
        }
    }
}

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

#include "Registries/ObjectRegistry.h"
#include "LowLevel.h"
#include "LuaEnv.h"
#include "UnLuaNovaDelegates.h"

namespace UnLuaNova
{
    static const char* REGISTRY_KEY = "UNLUANOVA_ObjectMap";
    static const char* MANUAL_REF_PROXY_MAP = "UNLUANOVA_ManualRefProxyMap";

    static int ReleaseSharedPtr(lua_State* L)
    {
        // The smart pointer lives in the payload, not at the block start. NewSmartPointer()
        // lays the userdata out as [FUserdataDesc][padding][TSharedPtr], so lua_touserdata()
        // hands back the descriptor - magic 0x1688 plus a tag - and Reset() would tear those
        // bytes down instead of the smart pointer. FObjectRegistry::Push() uses a bare
        // lua_newuserdata(), where block start and payload are the same address;
        // GetSmartPointer() answers correctly for both, and for userdata already released.
        //
        // Only the plain-Lua backend ever reaches this: Luau has no __gc at all, so the
        // wrong address went unnoticed for as long as the backend existed - the first
        // lua_close() after static export ran Reset() over a descriptor instead of a
        // TSharedPtr, which is what took the whole Lua run down.
        const auto Ptr = (TSharedPtr<void>*)GetSmartPointer(L, 1);
        if (Ptr)
            Ptr->Reset();
        return 0;
    }

    static int ReleaseManualRef(lua_State* L)
    {
        const auto Proxy = (FManualRefProxy*)lua_touserdata(L, 1);
        if (Proxy)
            Proxy->Release();
        return 0;
    }

    void FManualRefProxy::Release()
    {
        if (Registry)
            Registry->ReleaseManualRefProxy(this);
    }

    void FObjectRegistry::ReleaseManualRefProxy(FManualRefProxy* Proxy)
    {
        // A weak-table entry may disappear before its finalizer runs. Only the current
        // proxy owns the reference; collecting an older proxy must not release its successor.
        if (ManualRefProxies.FindRef(Proxy->Object) == Proxy)
        {
            ManualRefProxies.Remove(Proxy->Object);
            if (UObject* Object = Proxy->Object.Get())
                Env->RemoveManualObjectReference(Object);
        }
        Proxy->Registry = nullptr;
    }

    FObjectRegistry::FObjectRegistry(FLuaEnv* Env)
        : Env(Env)
    {
        const auto L = Env->GetMainState();

        lua_pushstring(L, REGISTRY_KEY);
        LowLevel::CreateWeakValueTable(L);
        lua_rawset(L, LUA_REGISTRYINDEX);

        lua_pushstring(L, MANUAL_REF_PROXY_MAP);
        LowLevel::CreateWeakValueTable(L);
        lua_rawset(L, LUA_REGISTRYINDEX);
        
        luaL_newmetatable(L, "TSharedPtr");
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, ReleaseSharedPtr);
        lua_rawset(L, -3);

        luaL_newmetatable(L, "UnLuaNovaManualRefProxy");
        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, ReleaseManualRef);
        lua_rawset(L, -3);

        lua_pop(L, 2);
    }

    void FObjectRegistry::NotifyUObjectDeleted(UObject* Object)
    {
        RemoveManualRef(Object);
        Unbind(Object);
    }

    void FObjectRegistry::NotifyUObjectLuaGC(UObject* Object)
    {
        Env->AutoObjectReference.Remove(Object);
    }

    void FObjectRegistry::Push(lua_State* L, UObject* Object)
    {
        if (!Object)
        {
            lua_pushnil(L);
            return;
        }

        // avoid invalid ptrs in containers from lua
        if (!UnLuaNova::IsUObjectValid(Object))
        {
            luaL_error(L, "attempt to read invalid uobject ptr from lua, maybe from containers like TArray.");
            return;
        }

        lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_pushlightuserdata(L, Object);
        const auto Type = lua_rawget(L, -2);
        if (Type == LUA_TNIL)
        {
            lua_pop(L, 1);
            PushObjectCore(L, Object);
            lua_pushlightuserdata(L, Object);
            lua_pushvalue(L, -2);
            lua_rawset(L, -4);
            ObjectRefs.Add(Object, LUA_NOREF);
        }
        lua_remove(L, -2);
    }

    int FObjectRegistry::Bind(UObject* Object)
    {
        if (const auto Exists = ObjectRefs.Find(Object))
        {
            if (*Exists != LUA_NOREF)
                return *Exists;
        }

        const auto L = Env->GetMainState();

        int OldTop = lua_gettop(L);

        lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_pushlightuserdata(L, Object);
        lua_newtable(L); // create a Lua table ('INSTANCE')
        PushObjectCore(L, Object); // push UObject ('RAW_UOBJECT')
        lua_pushstring(L, "Object");
        lua_pushvalue(L, -2);
        lua_rawset(L, -4); // INSTANCE.Object = RAW_UOBJECT

        // in some case may occur module or object metatable can 
        // not be found problem
        const auto Class = Object->IsA<UClass>() ? static_cast<UClass*>(Object) : Object->GetClass();
        const auto ClassBoundRef = Env->GetManager()->GetBoundRef(Class);
        int32 TypeModule = lua_rawgeti(L, LUA_REGISTRYINDEX, ClassBoundRef); // push the required module/table ('REQUIRED_MODULE') to the top of the stack
        int32 TypeMetatable = lua_getmetatable(L, -2); // get the metatable ('METATABLE_UOBJECT') of 'RAW_UOBJECT' 
        if (TypeModule != LUA_TTABLE || TypeMetatable == LUA_TNIL)
        {
            lua_pop(L, lua_gettop(L) - OldTop);
            return LUA_REFNIL;
        }

#if ENABLE_CALL_OVERRIDDEN_FUNCTION
        lua_pushstring(L, "Overridden");
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
#endif
        lua_setmetatable(L, -2); // REQUIRED_MODULE.metatable = METATABLE_UOBJECT
        lua_setmetatable(L, -3); // INSTANCE.metatable = REQUIRED_MODULE
        lua_pop(L, 1);

        lua_pushvalue(L, -1);
        const auto Ret = luaL_ref(L, LUA_REGISTRYINDEX);
        ObjectRefs.Add(Object, Ret);

        FUnLuaNovaDelegates::OnObjectBinded.Broadcast(Object); // 'INSTANCE' is on the top of stack now

        lua_rawset(L, -3);
        lua_pop(L, 1);
        return Ret;
    }

    bool FObjectRegistry::IsBound(const UObject* Object) const
    {
        const auto Exists = ObjectRefs.Find(Object);
        return Exists && *Exists != LUA_NOREF;
    }

    int FObjectRegistry::GetBoundRef(const UObject* Object) const
    {
        const auto Ref = ObjectRefs.Find(Object);
        if (Ref)
            return *Ref;
        return LUA_NOREF;
    }

    void FObjectRegistry::Unbind(UObject* Object)
    {
        int32 Ref;
        if (!ObjectRefs.RemoveAndCopyValue(Object, Ref))
            return;

        const auto L = Env->GetMainState();
        const auto Top = lua_gettop(L);
        RemoveFromObjectMapAndPushToStack(Object);

        if (Ref == LUA_NOREF)
        {
            if (lua_isnil(L, -1))
            {
                lua_pop(L, 1);
                return;
            }
            check(lua_isuserdata(L, -1));
            bool bTwoLvlPtr;
            void* Userdata = GetUserdataFast(L, -1, &bTwoLvlPtr);
            check(bTwoLvlPtr)
            *((void**)Userdata) = (void*)LowLevel::ReleasedPtr;
            lua_settop(L, Top);
            return;
        }

        check(lua_istable(L, -1));
        luaL_unref(L, LUA_REGISTRYINDEX, Ref);
        FUnLuaNovaDelegates::OnObjectUnbinded.Broadcast(Object); // object instance ('INSTANCE') is on the top of stack now

        lua_pushstring(L, "Object");
        lua_rawget(L, -2);
        // Same rule as the branch above: lua_touserdata() is the FUserdataDesc, not the
        // payload. Writing ReleasedPtr there would overwrite the descriptor - the userdata
        // would stop being recognized as ours - instead of releasing the UObject slot.
        bool bTwoLvlPtr;
        void* Userdata = GetUserdataFast(L, -1, &bTwoLvlPtr);
        check(bTwoLvlPtr)
        *((void**)Userdata) = (void*)LowLevel::ReleasedPtr;

        lua_settop(L, Top);
    }

    void FObjectRegistry::AddManualRef(lua_State* L, UObject* Object)
    {
        lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
        lua_pushlightuserdata(L, Object);
        lua_rawget(L, -2);
        auto* Existing = static_cast<FManualRefProxy*>(lua_touserdata(L, -1));
        if (!Existing || Existing->Registry != this || Existing->Object.Get() != Object)
        {
            lua_pop(L, 1);
            auto Ptr = UNLUANOVA_NEWDATA_DTOR(L, sizeof(FManualRefProxy),
                [](lua_State*, void* Block) { static_cast<FManualRefProxy*>(Block)->~FManualRefProxy(); });
            auto Proxy = new(Ptr)FManualRefProxy;
            Proxy->Object = Object;
            Proxy->Registry = this;
            ManualRefProxies.Add(Proxy->Object, Proxy);
            Env->AddManualObjectReference(Object);
            luaL_getmetatable(L, "UnLuaNovaManualRefProxy");
            lua_setmetatable(L, -2);
            lua_pushlightuserdata(L, Object);
            lua_pushvalue(L, -2);
            lua_rawset(L, -4);
        }
        lua_remove(L, -2);
    }

    void FObjectRegistry::RemoveManualRef(UObject* Object)
    {
        FManualRefProxy* Proxy = nullptr;
        if (ManualRefProxies.RemoveAndCopyValue(TWeakObjectPtr<UObject>(Object), Proxy))
            Proxy->Registry = nullptr;
        const auto L = Env->GetMainState();
        lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
        lua_pushlightuserdata(L, Object);
        lua_pushnil(L);
        lua_rawset(L, -3);
        lua_pop(L, 1);
        Env->RemoveManualObjectReference(Object);
    }

    void FObjectRegistry::RemoveFromObjectMapAndPushToStack(UObject* Object)
    {
        const auto L = Env->GetMainState();
        lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
        lua_pushlightuserdata(L, Object);
        lua_rawget(L, -2);
        lua_pushlightuserdata(L, Object);
        lua_pushnil(L);
        lua_rawset(L, -4);
        lua_remove(L, -2);
    }
}

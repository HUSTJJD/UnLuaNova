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

#include "UELib.h"
#include "Binding.h"
#include "Registries/ClassRegistry.h"
#include "LuaCore.h"
#include "LuaDynamicBinding.h"
#include "LuaEnv.h"
#include "Registries/EnumRegistry.h"

static const char* REGISTRY_KEY = "UNLUANOVA_UELib";
static const char* NAMESPACE_NAME = "UE";

static int UE_Index(lua_State* L)
{
    const int32 Type = lua_type(L, 2);
    if (Type != LUA_TSTRING)
        return 0;

    const char* Name = lua_tostring(L, 2);
    const auto Exported = UnLuaNova::FindExportedNonReflectedClass(Name);
    if (Exported)
    {
        Exported->Register(L);
        lua_rawget(L, 1);
        return 1;
    }

    const char Prefix = Name[0];
    const auto& Env = UnLuaNova::FLuaEnv::FindEnvChecked(L);
    if (Prefix == 'U' || Prefix == 'A' || Prefix == 'F')
    {
        const auto ReflectedType = UnLuaNova::FClassRegistry::LoadReflectedType(Name + 1);
        if (!ReflectedType)
            return 0;

        if (ReflectedType->IsNative())
        {
            if (auto Struct = Cast<UStruct>(ReflectedType))
                Env.GetClassRegistry()->Register(Struct);
        }
        else
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("attempt to load a blueprint type %s with UE namespace, use UE.UClass.Load or UE.UObject.Load instead."), UTF8_TO_TCHAR(Name));
            return 0;
        }
    }
    else if (Prefix == 'E')
    {
        const auto ReflectedType = UnLuaNova::FClassRegistry::LoadReflectedType(Name);
        if (!ReflectedType)
            return 0;

        if (ReflectedType->IsNative())
        {
            if (auto Enum = Cast<UEnum>(ReflectedType))
                Env.GetEnumRegistry()->Register(Enum);
        }
        else
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("attempt to load a blueprint enum %s with UE namespace, use UE.UObject.Load instead."), UTF8_TO_TCHAR(Name));
            return 0;
        }
    }

    lua_rawget(L, 1);
    return 1;
}

// Scripts have no business writing into the UE namespace: every entry there is published
// by the binding layer with lua_rawset. A __newindex that raises both documents that and
// stops a hotfixed (or downloaded) script from swapping an engine binding out from under
// everything else - which is the thing the sandbox is supposed to prevent.
//
// Registered *after* luaL_setfuncs below, because luaL_setfuncs writes through
// lua_setfield, which goes through __newindex like any other assignment.
static int UE_NewIndex(lua_State* L)
{
    return luaL_error(L, "attempt to write into the UE namespace: engine types are registered by the binding layer, not by scripts");
}

extern int32 UObject_Load(lua_State *L);
extern int32 UClass_Load(lua_State *L);
static int32 Global_NewObject(lua_State *L)
{
    int32 NumParams = lua_gettop(L);
    if (NumParams < 1)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s: Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
        return 0;
    }

    UClass *Class = Cast<UClass>(UnLuaNova::GetUObject(L, 1));
    if (!Class)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s: Invalid class!"), ANSI_TO_TCHAR(__FUNCTION__));
        return 0;
    }

    UObject* Outer = UnLuaNova::GetUObject(L, 2);
    if (!Outer)
        Outer = GetTransientPackage();

    FName Name = NumParams > 2 ? FName(lua_tostring(L, 3)) : NAME_None;
    //EObjectFlags Flags = NumParams > 3 ? EObjectFlags(lua_tointeger(L, 4)) : RF_NoFlags;

    {
        const char *ModuleName = NumParams > 3 ? lua_tostring(L, 4) : nullptr;
        int32 TableRef = LUA_NOREF;
        if (NumParams > 4 && lua_type(L, 5) == LUA_TTABLE)
        {
            lua_pushvalue(L, 5);
            TableRef = luaL_ref(L, LUA_REGISTRYINDEX);
        }
        FScopedLuaDynamicBinding Binding(L, Class, UTF8_TO_TCHAR(ModuleName), TableRef);
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION < 26
        UObject* Object = StaticConstructObject_Internal(Class, Outer, Name);
#else
        FStaticConstructObjectParameters ObjParams(Class);
        ObjParams.Outer = Outer;
        ObjParams.Name = Name;
        UObject* Object = StaticConstructObject_Internal(ObjParams);
#endif
        if (Object)
        {
            UnLuaNova::PushUObject(L, Object);
        }
        else
        {
            UNLUANOVA_LOGERROR(L, TEXT("%s: Failed to new object for class %s!"), ANSI_TO_TCHAR(__FUNCTION__), *Class->GetName());
            return 0;
        }
    }

    return 1;
}

static constexpr luaL_Reg UE_Functions[] = {
    {"LoadObject", UObject_Load},
    {"LoadClass", UClass_Load},
    {"NewObject", Global_NewObject},
    {NULL, NULL}
};

int UnLuaNova::UELib::Open(lua_State* L)
{
    lua_newtable(L);
    lua_pushstring(L, "__index");
    lua_pushcfunction(L, UE_Index);
    lua_rawset(L, -3);

    lua_pushvalue(L, -1);
    lua_setmetatable(L, -2);

    lua_pushvalue(L, -1);
    lua_pushstring(L, REGISTRY_KEY);
    lua_rawset(L, LUA_REGISTRYINDEX);

    luaL_setfuncs(L, UE_Functions, 0);

    // Only now: everything the namespace owns has been written through lua_setfield, and
    // from here on a script writing into it is a mistake rather than a registration.
    lua_pushstring(L, "__newindex");
    lua_pushcfunction(L, UE_NewIndex);
    lua_rawset(L, -3);

    lua_setglobal(L, NAMESPACE_NAME);

    // global access for legacy support
    lua_getglobal(L, LUA_GNAME);
    luaL_setfuncs(L, UE_Functions, 0);
    lua_pop(L, 1);

#if WITH_UE4_NAMESPACE == 1
    // 兼容UE4访问
    lua_getglobal(L, NAMESPACE_NAME);
    lua_setglobal(L, "UE4");
#elif WITH_UE4_NAMESPACE == 0
    // 兼容无UE4全局访问
    lua_getglobal(L, LUA_GNAME);
    lua_newtable(L);
    lua_pushstring(L, "__index");
    lua_getglobal(L, NAMESPACE_NAME);
    lua_rawset(L, -3);
    lua_setmetatable(L, -2);
#endif

    return 1;
}

void UnLuaNova::UELib::SetTableForClass(lua_State* L, const char* Name)
{
    lua_getglobal(L, NAMESPACE_NAME);
    lua_pushstring(L, Name);
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);

    // Freeze the class table itself, not just the namespace holding it. luaL_sandbox only
    // hardens the globals, so `UE.MyClass.Member = ...` used to be fair game even with the
    // sandbox on. Read-only is what actually closes that - and it only became survivable
    // once the lazy field cache in GetFieldInternal learned to skip a write it cannot make.
    if (UnLuaNova::IsClassTableFreezeEnabled())
    {
        // -2 is the class table: [class table, namespace], the rawset above consumed the
        // key and the copy.
        UNLUANOVA_SET_READONLY(L, -2, true);
    }

    lua_pop(L, 1);
}

void UnLuaNova::UELib::SetNamespaceWritable(lua_State* L, bool bWritable)
{
    lua_getglobal(L, NAMESPACE_NAME);
    if (lua_istable(L, -1))
    {
        // No-op on backends without readonly tables, so this stays callable unconditionally.
        UNLUANOVA_SET_READONLY(L, -1, !bWritable);
    }
    lua_pop(L, 1);
}

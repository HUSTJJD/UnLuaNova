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

#include "CollisionHelper.h"
#include "LuaCore.h"
#include "LuaBackend.h"
#include "Binding.h"
#include "LuaDynamicBinding.h"
#include "UnLuaNova.h"
#include "LowLevel.h"
#include "Containers/LuaSet.h"
#include "Containers/LuaMap.h"
#include "ReflectionUtils/FieldDesc.h"
#include "ReflectionUtils/PropertyDesc.h"

#ifdef __cplusplus
#if !LUA_COMPILE_AS_CPP
extern "C" {
#endif
#endif

// No Lua internal headers (lstate.h / lobject.h / lfunc.h) from here on: everything
// below is built on the public API plus LuaBackend.h, so both supported backends
// (Lua 5.5 and Luau) compile the same way.

#ifdef __cplusplus
#if !LUA_COMPILE_AS_CPP
}
#endif
#endif

const FScriptContainerDesc FScriptContainerDesc::Array(sizeof(FLuaArray), "TArray");
const FScriptContainerDesc FScriptContainerDesc::Set(sizeof(FLuaSet), "TSet");
const FScriptContainerDesc FScriptContainerDesc::Map(sizeof(FLuaMap), "TMap");

/**
 * Get lua file full path from relative path
 */
FString GetFullPathFromRelativePath(const FString& RelativePath)
{
    FString FullFilePath = GLuaSrcFullPath + RelativePath;
    FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString ProjectPersistentDownloadDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectPersistentDownloadDir());
    if (!ProjectPersistentDownloadDir.EndsWith("/"))
    {
        ProjectPersistentDownloadDir.Append("/");
    }

    FString RealFullFilePath = FullFilePath.Replace(*ProjectDir, *ProjectPersistentDownloadDir);        // try to load the file from 'ProjectPersistentDownloadDir' first
    if (IFileManager::Get().FileExists(*RealFullFilePath))
    {
        FullFilePath = RealFullFilePath;
    }
    else
    {
        if (!IFileManager::Get().FileExists(*FullFilePath))
        {
            FullFilePath = "";
        }
    }
    return FullFilePath;
}

/**
 * Set the name for a Lua table which on the top of the stack
 */
void SetTableForClass(lua_State *L, const char *Name)
{
    lua_getglobal(L, "UE");
    lua_pushstring(L, Name);
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 2);
}

#define USERDATA_MAGIC  0x1688
#define BIT_VARIANT_TAG            (1 << 7)         // variant tag for userdata
#define BIT_RELEASED_TAG            (1 << 6)        // this userdata was released and should not use anywhere
#define BIT_TWOLEVEL_PTR        (1 << 5)            // two level pointer flag
#define BIT_SCRIPT_CONTAINER    (1 << 4)            // script container (TArray, TSet, TMap) flag

// The descriptor is allocated immediately BEFORE the payload, so it is reachable from a
// bare payload pointer as (Ptr - sizeof(FUserdataDesc)). That single property is what
// lets this file stay on the public API: locating it used to require walking back to
// 'Udata' and reading U->len, which is why lstate.h/lobject.h were needed at all.
// It is 8 bytes and 8-aligned so the payload keeps the alignment it had before.
struct alignas(8) FUserdataDesc
{
    uint16  magic;
    uint8   tag;
    uint8   padding;
};

/**
 * Read the descriptor that sits at the very start of a userdata block.
 *
 * 'Block' must be the raw pointer returned by lua_touserdata() - NOT the payload.
 * Layout is [FUserdataDesc][Padding][payload], so the descriptor is at a fixed offset
 * and this never reads outside the allocation. (Probing *behind* the payload instead
 * would read out of bounds for userdata UnLuaNova did not create.)
 *
 * Returns null for a null input, or for a userdata UnLuaNova did not create (no magic).
 */
static FUserdataDesc* GetUserdataDesc(void* Block)
{
    if (!Block)
        return nullptr;

    FUserdataDesc* UserdataDesc = (FUserdataDesc*)Block;
    return (USERDATA_MAGIC == UserdataDesc->magic) ? UserdataDesc : nullptr;
}

/**
 * Payload address of a block whose descriptor has already been validated.
 */
static uint8* GetUserdataPayload(void* Block, const FUserdataDesc* UserdataDesc)
{
    return (uint8*)Block + sizeof(FUserdataDesc) + UserdataDesc->padding;
}

/**
 * Offset, in bytes, from the start of a Lua userdata block to where the payload has to
 * begin so that it satisfies 'Alignment'.
 *
 * Measured once at runtime instead of derived from sizeof(UUdata): the old formula
 * hard-coded the 5.4 header size and needed lstate.h/lobject.h to read it back.
 */
uint8 CalcUserdataPadding(int32 Alignment)
{
    // Where a Lua userdata payload actually starts, relative to the widest alignment we
    // care about. Lua keeps a fixed offset from the allocation, so one probe represents
    // every later allocation.
    static uint8 PayloadBaseOffset = 0xFF;
    if (0xFF == PayloadBaseOffset)
    {
        lua_State* ProbeState = luaL_newstate();
        void* Probe = UNLUANOVA_NEWDATA(ProbeState, 1);
        PayloadBaseOffset = (uint8)((uintptr_t)Probe % 16);
        lua_close(ProbeState);
    }

    // payload starts at (block + Padding + sizeof(FUserdataDesc))
    const uint32 Base = (uint32)PayloadBaseOffset + (uint32)sizeof(FUserdataDesc);
    const uint32 Aligned = ((Base + (uint32)Alignment - 1) / (uint32)Alignment) * (uint32)Alignment;
    return (uint8)(Aligned - Base);
}

static void* NewUserdataWithDesc(lua_State* L, int Size, uint8 Tag, uint8 Padding)
{
    const uint32 DescSize = (uint32)sizeof(FUserdataDesc);

    // layout: [FUserdataDesc][Padding][payload]
    uint8* Block = (uint8*)UNLUANOVA_NEWDATA(L, DescSize + Padding + Size);

    FUserdataDesc* UserdataDesc = (FUserdataDesc*)Block;
    UserdataDesc->magic = USERDATA_MAGIC;
    UserdataDesc->tag = Tag;
    UserdataDesc->padding = Padding;

    return GetUserdataPayload(Block, UserdataDesc);
}

void* NewUserdataWithTwoLvPtrTag(lua_State* L, int Size, void* Object)
{
    void* Userdata = NewUserdataWithDesc(L, Size, (BIT_VARIANT_TAG | BIT_TWOLEVEL_PTR), 0);
    *(void**)Userdata = Object;
    return Userdata;
}

void* NewUserdataWithContainerTag(lua_State* L, int Size)
{
    return NewUserdataWithDesc(L, Size, (BIT_VARIANT_TAG | BIT_SCRIPT_CONTAINER), 0);
}

void* NewUserdataWithPaddingTag(lua_State* L, int Size, uint8 Padding)
{
    return NewUserdataWithDesc(L, Size, BIT_VARIANT_TAG, Padding);
}

void MarkUserdataTwoLvPtrTag(lua_State* L, int32 Index)
{
    // Takes the stack index rather than a bare payload pointer: both call sites have L
    // and the index handy, and going through lua_touserdata() avoids walking back to
    // 'Udata' - which is exactly what used to require lstate.h/lobject.h.
    FUserdataDesc* UserdataDesc = GetUserdataDesc(lua_touserdata(L, Index));
    if (UserdataDesc)
    {
        UserdataDesc->tag = (BIT_VARIANT_TAG | BIT_TWOLEVEL_PTR);
    }
}

void SetUserdataFlags(lua_State* L, int32 Index, uint8 Flags)
{
    FUserdataDesc* UserdataDesc = GetUserdataDesc(lua_touserdata(L, Index));
    if (UserdataDesc)
    {
        UserdataDesc->tag |= Flags;
    }
}

/**
 * Get the address of userdata
 *
 * @param Index - Lua stack index
 * @param[out] OutTwoLvlPtr - whether the address is a two level pointer
 * @param[out] OutClassMetatable - whether the userdata comes from a metatable
 * @return - the untyped dynamic array
 */
void* GetUserdata(lua_State *L, int32 Index, bool *OutTwoLvlPtr, bool *OutClassMetatable)
{
    return UnLuaNova::LowLevel::GetUserdata(L, Index, OutTwoLvlPtr, OutClassMetatable);
}

/**
 * Get the address of userdata, fast path
 */
void* GetUserdataFast(lua_State *L, int32 Index, bool *OutTwoLvlPtr)
{
    bool bTwoLvlPtr = false;
    void* Userdata = nullptr;

    const int32 Type = lua_type(L, Index);
    if (Type == LUA_TUSERDATA)
    {
        uint8* Block = (uint8*)lua_touserdata(L, Index);
        FUserdataDesc* UserdataDesc = GetUserdataDesc(Block);
        if ((UserdataDesc)
            && (UserdataDesc->tag & BIT_VARIANT_TAG))// if the userdata has a variant tag
        {
            bTwoLvlPtr = (UserdataDesc->tag & BIT_TWOLEVEL_PTR) != 0;        // test if the userdata is a two level pointer
            if (UserdataDesc->tag & BIT_RELEASED_TAG)
                Userdata = nullptr;
            else
                Userdata = GetUserdataPayload(Block, UserdataDesc);
        }
        else
        {
            Userdata = Block;                       // not created by us: hand back the block start
        }
    }
    else if (Type == LUA_TLIGHTUSERDATA)
    {
        Userdata = lua_touserdata(L, Index);                       // get the light userdata
    }

    if (OutTwoLvlPtr)
    {
        *OutTwoLvlPtr = bTwoLvlPtr;                                 // set two level pointer flag
    }

    return Userdata;
}

/**
 * Set metatable for the userdata/table on the top of the stack
 */
bool TryToSetMetatable(lua_State* L, const char* MetatableName, UObject* Object)
{
    const auto Registry = UnLuaNova::FLuaEnv::FindEnv(L)->GetClassRegistry();
    if (!Registry)
        return false;

    return Registry->TrySetMetatable(L, MetatableName);
}

/**
 * Create a new userdata with padding size
 */
void* NewUserdataWithPadding(lua_State *L, int32 Size, const char *MetatableName, uint8 PaddingSize)
{
    if (Size < 1)
    {
        // userdata size must be greater than 0
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid size!"), ANSI_TO_TCHAR(__FUNCTION__));
        return nullptr;
    }
    if ((PaddingSize & 0x07) != 0)          // 8 bytes padding at least..., 8, 24, 88
    {
        // padding size must be greater or equal to 8 bytes
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid padding size!"), ANSI_TO_TCHAR(__FUNCTION__));
        return nullptr;
    }

    void* Userdata = NewUserdataWithPaddingTag(L, Size, PaddingSize); // userdata size must add padding size
    if (MetatableName)
    {
        bool bSuccess = TryToSetMetatable(L, MetatableName);        // set metatable
        if (!bSuccess)
        {
            UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid metatable, metatable name: %s!"), ANSI_TO_TCHAR(__FUNCTION__), UTF8_TO_TCHAR(MetatableName));
            return nullptr;
        }
    }
    return (uint8*)Userdata + PaddingSize;                          // return 'valid' address (userdata memory address + padding size)
}

/**
 * Get a cpp instance's address
 */
void* GetCppInstance(lua_State *L, int32 Index)
{
    bool bTwoLvlPtr = false;
    void *Userdata = UnLuaNova::LowLevel::GetUserdata(L, Index, &bTwoLvlPtr);
    if (Userdata)
    {
        return bTwoLvlPtr ? *((void**)Userdata) : Userdata;         // return instance's address
    }
    return nullptr;
}

/**
 * Get a cpp instance's address, fast path
 */
void* GetCppInstanceFast(lua_State *L, int32 Index)
{
    bool bTwoLvlPtr = false;
    void *Userdata = GetUserdataFast(L, Index, &bTwoLvlPtr);
    if (Userdata)
    {
        return bTwoLvlPtr ? *((void**)Userdata) : Userdata;         // return instance's address
    }
    return nullptr;
}


/**
 * Create a new userdata for a script container
 */
void* NewScriptContainer(lua_State *L, const FScriptContainerDesc &Desc)
{
    void* Userdata = NewUserdataWithContainerTag(L, Desc.GetSize());
    luaL_setmetatable(L, Desc.GetName());   // set metatable
    return Userdata;
}

/**
 * Find a cached script container or create a new one
 *
 * @return - null if container is already cached, or the new created userdata otherwise
 */
void* CacheScriptContainer(lua_State *L, void *Key, const FScriptContainerDesc &Desc)
{
    if (!Key)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid key!"), ANSI_TO_TCHAR(__FUNCTION__));
        return nullptr;
    }

    // return null if container is already cached, or create/cache/return a new ud
    void *Userdata = nullptr;
    lua_getfield(L, LUA_REGISTRYINDEX, "ScriptContainerMap");
    lua_pushlightuserdata(L, Key);
    int32 Type = lua_rawget(L, -2);             
    if (Type == LUA_TNIL)
    {
        lua_pop(L, 1);

        Userdata = NewUserdataWithContainerTag(L, Desc.GetSize());      // create new userdata
        luaL_setmetatable(L, Desc.GetName());               // set metatable
        lua_pushlightuserdata(L, Key);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);                                  // cache it in 'ScriptContainerMap'
        UnLuaNova::FLuaEnv::FindEnv(L)->GetDanglingCheck()->CaptureContainer(L, Key);
    }
#if UE_BUILD_DEBUG
    else
    {
        check(Type == LUA_TUSERDATA);
    }
#endif
    lua_remove(L, -2);
    return Userdata;            // return null if container is already cached, or the new created userdata otherwise
}


void* CacheScriptContainer(lua_State *L, void *Key, const FScriptContainerDesc &Desc, const TFunctionRef<bool (void*)>& Validator)
{
    if (!Key)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid key!"), ANSI_TO_TCHAR(__FUNCTION__));
        return nullptr;
    }

    // return null if container is already cached, or create/cache/return a new ud
    void *Userdata = nullptr;
    lua_getfield(L, LUA_REGISTRYINDEX, "ScriptContainerMap");
    lua_pushlightuserdata(L, Key);
    int32 Type = lua_rawget(L, -2);
    if (Type == LUA_TNIL || !Validator(lua_touserdata(L, -1)))
    {
        lua_pop(L, 1);

        Userdata = NewUserdataWithContainerTag(L, Desc.GetSize());      // create new userdata
        luaL_setmetatable(L, Desc.GetName());               // set metatable
        lua_pushlightuserdata(L, Key);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);                                  // cache it in 'ScriptContainerMap'
        UnLuaNova::FLuaEnv::FindEnv(L)->GetDanglingCheck()->CaptureContainer(L, Key);
    }

    lua_remove(L, -2);
    return Userdata;            // return null if container is already cached, or the new created userdata otherwise
}

/**
 * Get a script container at the given stack index
 */
void* GetScriptContainer(lua_State *L, int32 Index)
{
    if (lua_type(L, Index) == LUA_TUSERDATA)
    {
        uint8 Flag = (BIT_VARIANT_TAG | BIT_SCRIPT_CONTAINER);              // variant tags

        void* Block = lua_touserdata(L, Index);
        FUserdataDesc* UserdataDesc = GetUserdataDesc(Block);
        if (UserdataDesc)
        {
            return (UserdataDesc->tag & Flag) == Flag ? *((void**)GetUserdataPayload(Block, UserdataDesc)) : nullptr;
        }
    }
    return nullptr;
}

/**
 * Remove a cached script container from 'ScriptContainerMap'
 */
void RemoveCachedScriptContainer(lua_State *L, void *Key)
{
    if (!L || !Key)
    {
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "ScriptContainerMap");
    lua_pushlightuserdata(L, Key);
    int32 Type = lua_rawget(L, -2);
    if (Type != LUA_TNIL)
    {
        lua_pushlightuserdata(L, Key);
        lua_pushnil(L);
        lua_rawset(L, -4);
    }
    lua_pop(L, 2);
}

/**
 * Push a UObject to Lua stack
 */
void PushObjectCore(lua_State *L, UObjectBaseUtility *Object)
{
    FString MetatableName = UnLuaNova::LowLevel::GetMetatableName((UObject*)Object);
    if (MetatableName.IsEmpty())
    {
		lua_pushnil(L);
		return;
    }
    
#if UNLUANOVA_ENABLE_DEBUG != 0
	UE_LOG(LogUnLuaNova, Log, TEXT("%s : %p,%s,%s"), ANSI_TO_TCHAR(__FUNCTION__), Object,*Object->GetName(), *MetatableName);
#endif

    NewUserdataWithTwoLvPtrTag(L, sizeof(void*), Object);  // create a userdata and store the UObject address
    bool bSuccess = TryToSetMetatable(L, TCHAR_TO_UTF8(*MetatableName), (UObject*)Object);
	if (!bSuccess)
	{
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid metatable,Name %s, Object %s,%p!"), ANSI_TO_TCHAR(__FUNCTION__), *MetatableName, *Object->GetName(), Object);
    }
}


/**
 * Push a integer
 */
static void PushIntegerElement(lua_State *L, FNumericProperty *Property, void *Value)
{
    UNLUANOVA_PUSHINT64(L, Property->GetUnsignedIntPropertyValue(Value));
}

/**
 * Push a float
 */
static void PushFloatElement(lua_State *L, FNumericProperty *Property, void *Value)
{
    lua_pushnumber(L, Property->GetFloatingPointPropertyValue(Value));
}

/**
 * Push a enum
 */
static void PushEnumElement(lua_State *L, FNumericProperty *Property, void *Value)
{
    UNLUANOVA_PUSHINT64(L, Property->GetSignedIntPropertyValue(Value));
}

/**
 * Push a FName
 */
static void PushFNameElement(lua_State *L, FNameProperty *Property, void *Value)
{
    lua_pushstring(L, TCHAR_TO_UTF8(*Property->GetPropertyValue(Value).ToString()));
}

/**
 * Push a FString
 */
static void PushFStringElement(lua_State *L, FStrProperty *Property, void *Value)
{
    lua_pushstring(L, TCHAR_TO_UTF8(*Property->GetPropertyValue(Value)));
}

/**
 * Push a FText
 */
static void PushFTextElement(lua_State *L, FTextProperty *Property, void *Value)
{
    lua_pushstring(L, TCHAR_TO_UTF8(*Property->GetPropertyValue(Value).ToString()));
}

/**
 * Push a UObject
 */
static void PushObjectElement(lua_State *L, FObjectPropertyBase *Property, void *Value)
{
    UObject *Object = Property->GetObjectPropertyValue(Value);
    PushObjectCore(L, Object);
}

/**
 * Push a Interface
 */
static void PushInterfaceElement(lua_State *L, FInterfaceProperty *Property, void *Value)
{
    const FScriptInterface &Interface = Property->GetPropertyValue(Value);
    UObject *Object = Interface.GetObject();
    PushObjectCore(L, Object);
}

/**
 * Push a ScriptStruct
 */
static void PushStructElement(lua_State *L, FProperty *Property, void *Value)
{
    NewUserdataWithTwoLvPtrTag(L, sizeof(void*), Value);
}

/**
 * Push a delegate
 */
static void PushDelegateElement(lua_State *L, FDelegateProperty *Property, void *Value)
{
    FScriptDelegate *ScriptDelegate = Property->GetPropertyValuePtr(Value);
    UnLuaNova::FLuaEnv::FindEnvChecked(L).GetDelegateRegistry()->Register(ScriptDelegate, Property, (UObject*)Value);
    NewUserdataWithTwoLvPtrTag(L, sizeof(void*), ScriptDelegate);
}

/**
 * Push a multicast delegate
 */
static void PushMCDelegateElement(lua_State *L, FMulticastDelegateProperty *Property, void *Value)
{
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION < 23
    FMulticastScriptDelegate *ScriptDelegate = Property->GetPropertyValuePtr(Value);
#else
    void *ScriptDelegate = Value;
#endif

    UnLuaNova::FLuaEnv::FindEnvChecked(L).GetDelegateRegistry()->Register(ScriptDelegate, Property, (UObject*)Value);
    NewUserdataWithTwoLvPtrTag(L, sizeof(void*), ScriptDelegate);
}

template <typename T, bool WithMetaTableName>
class TPropertyArrayPushPolicy
{
public:
    static bool CheckMetaTable(const char *MetatableName) { return true; }
    static void PrePushArray(lua_State *L, const char *MetatableName) {}
    static void PostPushArray(lua_State *L) {}
    static void PostPushSingleElement(lua_State *L) { lua_rawset(L, -3); }
};

template <typename T>
class TPropertyArrayPushPolicy<T, true>
{
public:
    static bool CheckMetaTable(const char *MetatableName) { return MetatableName != nullptr; }
    static void PrePushArray(lua_State *L, const char *MetatableName)
    {
        const auto& Env = UnLuaNova::FLuaEnv::FindEnvChecked(L);
        Env.GetClassRegistry()->PushMetatable(L, MetatableName);
    }
    static void PostPushArray(lua_State *L) { lua_pop(L, 1); }

    static void PostPushSingleElement(lua_State *L)
    {
        lua_pushvalue(L, -3);
        lua_setmetatable(L, -2);
        lua_rawset(L, -4);
    }
};

/**
 * Push static property array
 */
template <typename T, bool WithMetaTableName>
static void PushPropertyArray(lua_State *L, T *Property, void *Value, void(*PushFunc)(lua_State*, T*, void*), const char *MetatableName = nullptr)
{
#if !UE_BUILD_SHIPPING
    if (!Property || !Value || Property->ArrayDim < 2 || Property->GetElementSize() < 1)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
        return;
    }
#endif

    if (!TPropertyArrayPushPolicy<T, WithMetaTableName>::CheckMetaTable(MetatableName))
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid metatable name!"), ANSI_TO_TCHAR(__FUNCTION__));
        return;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "ArrayMap");         // get weak table 'ArrayMap'
    lua_pushlightuserdata(L, Value);
    int32 Type = lua_rawget(L, -2);
    if (Type != LUA_TTABLE)
    {
        check(Type == LUA_TNIL);
        lua_pop(L, 1);

        uint8 *ElementPtr = (uint8*)Value;
        lua_newtable(L);                                    // create a Lua table
        TPropertyArrayPushPolicy<T, WithMetaTableName>::PrePushArray(L, MetatableName);
        for (int32 i = 0; i < Property->ArrayDim; ++i)
        {
            lua_pushinteger(L, i + 1);
            PushFunc(L, Property, ElementPtr);
            ElementPtr += Property->GetElementSize();
            TPropertyArrayPushPolicy<T, WithMetaTableName>::PostPushSingleElement(L);
        }
        TPropertyArrayPushPolicy<T, WithMetaTableName>::PostPushArray(L);

        lua_pushlightuserdata(L, Value);                    // cache the Lua table in 'ArrayMap'
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
    }
    lua_remove(L, -2);
}


void PushIntegerArray(lua_State *L, FNumericProperty *Property, void *Value)
{
    PushPropertyArray<FNumericProperty, false>(L, Property, Value, PushIntegerElement);
}

void PushFloatArray(lua_State *L, FNumericProperty *Property, void *Value)
{
    PushPropertyArray<FNumericProperty, false>(L, Property, Value, PushFloatElement);
}

void PushEnumArray(lua_State *L, FNumericProperty *Property, void *Value)
{
    PushPropertyArray<FNumericProperty, false>(L, Property, Value, PushEnumElement);
}

void PushFNameArray(lua_State *L, FNameProperty *Property, void *Value)
{
    PushPropertyArray<FNameProperty, false>(L, Property, Value, PushFNameElement);
}

void PushFStringArray(lua_State *L, FStrProperty *Property, void *Value)
{
    PushPropertyArray<FStrProperty, false>(L, Property, Value, PushFStringElement);
}

void PushFTextArray(lua_State *L, FTextProperty *Property, void *Value)
{
    PushPropertyArray<FTextProperty, false>(L, Property, Value, PushFTextElement);
}

void PushObjectArray(lua_State *L, FObjectPropertyBase *Property, void *Value)
{
    PushPropertyArray<FObjectPropertyBase, false>(L, Property, Value, PushObjectElement);
}

void PushInterfaceArray(lua_State *L, FInterfaceProperty *Property, void *Value)
{
    PushPropertyArray<FInterfaceProperty, false>(L, Property, Value, PushInterfaceElement);
}

void PushDelegateArray(lua_State *L, FDelegateProperty *Property, void *Value)
{
    PushPropertyArray<FDelegateProperty, true>(L, Property, Value, PushDelegateElement, "FScriptDelegate");
}

void PushMCDelegateArray(lua_State *L, FMulticastDelegateProperty *Property, void *Value, const char *MetatableName)
{
    PushPropertyArray<FMulticastDelegateProperty, true>(L, Property, Value, PushMCDelegateElement, MetatableName);
}

void PushStructArray(lua_State *L, FProperty *Property, void *Value, const char *MetatableName)
{
    PushPropertyArray<FProperty, true>(L, Property, Value, PushStructElement, MetatableName);
}

/**
 * Get target UObject and Lua function pointer for a delegate
 */
int32 GetDelegateInfo(lua_State *L, int32 Index, UObject* &Object, const void* &Function)
{
    int32 Type = lua_type(L, Index);
    if (Type != LUA_TTABLE)
    {
        return INDEX_NONE;
    }

    Object = nullptr;
    Function = nullptr;
    int32 FuncIdxInTable = INDEX_NONE;
    for (int32 i = 1; i <= 2; ++i)
    {
        Type = lua_rawgeti(L, Index, i);
        if (Type == LUA_TFUNCTION)
        {
            Function = lua_topointer(L, -1);            // Lua function pointer
            FuncIdxInTable = i;
        }
        else
        {
            Object = UnLuaNova::GetUObject(L, -1);          // target UObject
        }
        lua_pop(L, 1);
    }
    return !Object || !Function ? INDEX_NONE : FuncIdxInTable;
}

/**
 * Get Lua instance for a UObject
 */
bool GetObjectMapping(lua_State *L, UObjectBaseUtility *Object)
{
    if (!Object)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s, Invalid object!"), ANSI_TO_TCHAR(__FUNCTION__));
        return false;
    }

    lua_getfield(L, LUA_REGISTRYINDEX, "UNLUANOVA_ObjectMap");
    lua_pushlightuserdata(L, Object);
    int32 Type = lua_rawget(L, -2);
    if (Type != LUA_TNIL)
    {
        lua_remove(L, -2);
        return true;
    }
    lua_pop(L, 2);
    return false;
}

/**
 * Push a Lua function (by a function name) and push a UObject instance as its first parameter
 */
int32 PushFunction(lua_State *L, UObjectBaseUtility *Object, const char *FunctionName)
{
    int32 N = lua_gettop(L);
    lua_pushcfunction(L, UnLuaNova::ReportLuaCallError);
    const auto& Env = UnLuaNova::FLuaEnv::FindEnv(L);
    const auto Ref = Env->GetObjectRegistry()->GetBoundRef((UObject*)Object);
    if (Ref != LUA_NOREF)
    {
        lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
        int32 Type = lua_type(L, -1);
        if (Type == LUA_TTABLE /*|| Type == LUA_TUSERDATA*/)
        {
            if (lua_getmetatable(L, -1) == 1)
            {
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
                        return luaL_ref(L, LUA_REGISTRYINDEX);
                    }
                    else
                    {
                        lua_pop(L, 1);
                        lua_pushstring(L, "Super");
                        lua_rawget(L, -2);
                        lua_remove(L, -2);
                    }
                } while (lua_istable(L, -1));
            }
        }
    }
    if (int32 NumToPop = lua_gettop(L) - N)
    {
        lua_pop(L, NumToPop);
    }
    return LUA_NOREF;
}

/**
 * Push a Lua function (by a function reference) and push a UObject instance as its first parameter
 */
bool PushFunction(lua_State *L, UObjectBaseUtility *Object, int32 FunctionRef)
{
    int32 N = lua_gettop(L);
    lua_pushcfunction(L, UnLuaNova::ReportLuaCallError);
    int32 Type = lua_rawgeti(L, LUA_REGISTRYINDEX, FunctionRef);
    if (Type == LUA_TFUNCTION)
    {
        UnLuaNova::PushUObject(L, Object);
        return true;
    }
    if (int32 NumToPop = lua_gettop(L) - N)
    {
        lua_pop(L, NumToPop);
    }
    return false;
}

/**
 * Call a Lua function
 */
bool CallFunction(lua_State *L, int32 NumArgs, int32 NumResults)
{
    int32 ErrorReporterIdx = lua_gettop(L) - NumArgs - 1;
    int32 Code = lua_pcall(L, NumArgs, NumResults, -(NumArgs + 2));
    if (Code == LUA_OK)
    {
        lua_remove(L, ErrorReporterIdx);
        return true;
    }
    int32 TopIdx = lua_gettop(L);
    lua_pop(L, TopIdx - ErrorReporterIdx + 1);
    return false;
}

/**
 * Push a field (property or function)
 */
static void PushField(lua_State *L, TSharedPtr<FFieldDesc> Field)
{
    const auto& Env = UnLuaNova::FLuaEnv::FindEnvChecked(L);
    check(Field && Field->IsValid());
    if (Field->IsProperty())
    {
        TSharedPtr<FPropertyDesc> Property = Field->AsProperty();
        Env.GetObjectRegistry()->Push(L, Property);
    }
    else
    {
        TSharedPtr<FFunctionDesc> Function = Field->AsFunction();
        Env.GetObjectRegistry()->Push(L, Function);
        if (Function->IsLatentFunction())
        {
            lua_pushcclosure(L, Class_CallLatentFunction, 1);   // closure
        }
        else
        {
            lua_pushcclosure(L, Class_CallUFunction, 1);        // closure
        }
    }
}

static void GetFieldInternal(lua_State* L) 
{
    lua_pop(L, 1);

    lua_pushstring(L, "__name");
    auto Type = lua_rawget(L, -2);
    check(Type == LUA_TSTRING);

    const char* ClassName = lua_tostring(L, -1);
    const char* FieldName = lua_tostring(L, 2);

    lua_pop(L, 1);

    // TODO: refactor
    const auto Registry = UnLuaNova::FLuaEnv::FindEnv(L)->GetClassRegistry();
    FClassDesc* ClassDesc = Registry->Register(ClassName);
    TSharedPtr<FFieldDesc> Field = ClassDesc->RegisterField(FieldName, ClassDesc);
    if (Field && Field->IsValid())
    {
        bool bCached = false;
        bool bInherited = Field->IsInherited();
        if (bInherited)
        {
            FString SuperStructName = Field->GetOuterName();
            const auto Pushed = Registry->PushMetatable(L, TCHAR_TO_UTF8(*SuperStructName));
            check(Pushed);
            lua_pushvalue(L, 2);
            Type = lua_rawget(L, -2);
            bCached = Type != LUA_TNIL;
            // check cached property from non-native class
            if (bCached && !Field->OuterClass->IsNative())
            {
                auto Ptr = lua_touserdata(L, -1);
                if (Ptr)
                {
                    auto Property = static_cast<TSharedPtr<UnLuaNova::ITypeOps>*>(Ptr);
                    if (Property && Property->IsValid())
                    {
                        auto PropertyDesc = static_cast<FPropertyDesc*>((*Property).Get());
                        if (!PropertyDesc->IsValid())
                        {
                            bCached = false;
                        }
                    }
                }
            }
            if (!bCached)
            {
                lua_pop(L, 1);
            }
        }

        if (!bCached)
        {
            PushField(L, Field);                // Property / closure
            lua_pushvalue(L, 2);                // key
            lua_pushvalue(L, -2);               // Property / closure
            lua_rawset(L, -4);
        }
        if (bInherited)
        {
            lua_remove(L, -2);
            lua_pushvalue(L, 2);                // key
            lua_pushvalue(L, -2);               // Property / closure
            lua_rawset(L, -4);
        }
    }
    else
    {
        if (ClassDesc->IsClass())
        {
            luaL_getmetatable(L, "UClass");
            lua_pushvalue(L, 2);                // push key
            lua_rawget(L, -2);
            lua_remove(L, -2);
        }
        else
        {
            lua_pushnil(L);
        }
    }
}

/**
 * Get a field (property or function)
 */
FORCEINLINE static int32 GetField(lua_State* L)
{
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    int32 Type = lua_rawget(L, -2);
    if (Type == LUA_TNIL)
        GetFieldInternal(L);
    lua_remove(L, -2);
    return 1;
}

/**
 * Debug only...
 */
bool PeekTableElement(lua_State *L, void*)
{
    int32 KeyType = lua_type(L, -2);
    switch (KeyType)
    {
    case LUA_TBOOLEAN:
        {
            int32 b = lua_toboolean(L, -2);
            check(b >= 0);
        }
        break;
    case LUA_TLIGHTUSERDATA:
        {
            const void *p = lua_topointer(L, -2);
            check(true);
        }
        break;
    case LUA_TNUMBER:
        {
            float f = lua_tonumber(L, -2);
            check(true);
        }
        break;
    case LUA_TSTRING:
        {
            const char *s = lua_tostring(L, -2);
            check(true);
        }
        break;
    case LUA_TUSERDATA:
        {
            const void *p = lua_topointer(L, -2);
            check(true);
        }
        break;
    }
    int32 ValueType = lua_type(L, -1);
    switch (ValueType)
    {
    case LUA_TBOOLEAN:
        {
            int32 b = lua_toboolean(L, -1);
            check(b >= 0);
        }
        break;
    case LUA_TLIGHTUSERDATA:
        {
            const void *p = lua_topointer(L, -1);
            check(true);
        }
        break;
    case LUA_TNUMBER:
        {
            float f = lua_tonumber(L, -1);
            check(true);
        }
        break;
    case LUA_TSTRING:
        {
            const char *s = lua_tostring(L, -1);
            check(true);
        }
        break;
    case LUA_TUSERDATA:
        {
            if (lua_checkstack(L, 2))
            {
                UObject *p = UnLuaNova::GetUObject(L, -1);
                UStruct *Struct = Cast<UStruct>(p);
                if (Struct && Struct->IsNative())
                {
                    return false;
                }
                check(true);
            }
        }
        break;
    }
    return true;
}

/**
 * Traverse a Lua table
 */
int32 TraverseTable(lua_State *L, int32 Index, void *Userdata, bool(*TraverseWorker)(lua_State*, void*))
{
    if (Index < 0 && Index > LUA_REGISTRYINDEX)
    {
        int32 Top = lua_gettop(L);
        Index = Top + Index + 1;
    }
    int32 Type = lua_type(L, Index);
    if (Type == LUA_TTABLE)
    {
        if (!lua_checkstack(L, 2))
        {
            return INDEX_NONE;
        }

        int32 NumElements = 0;
        lua_pushnil(L);
        while (lua_next(L, Index) != 0)
        {
            if (TraverseWorker)
            {
                bool b = TraverseWorker(L, Userdata);
                if (b)
                {
                    ++NumElements;
                }
            }
            lua_pop(L, 1);
        }
        return NumElements;
    }
    return INDEX_NONE;
}

/**
 * __index meta methods for class
 */
int32 Class_Index(lua_State *L)
{
    GetField(L);

    auto Ptr = lua_touserdata(L, -1);
    if (!Ptr)
        return 1;

    auto Property = static_cast<TSharedPtr<UnLuaNova::ITypeOps>*>(Ptr);
    if (!Property->IsValid())
        return 0;
    
    auto Self = GetCppInstance(L, 1);
    if (!Self)
        return 1;

    if (UnLuaNova::LowLevel::IsReleasedPtr(Self))
        return luaL_error(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("attempt to read property '%s' on released object"), *(*Property)->GetName())));

    if (!UnLuaNova::LowLevel::CheckPropertyOwner(L, (*Property).Get(), Self))
        return 0;

    (*Property)->ReadValue_InContainer(L, Self, false);
    lua_remove(L, -2);
    return 1;
}

/**
 * __newindex meta methods for class
 */
int32 Class_NewIndex(lua_State *L)
{
    GetField(L);

    auto Ptr = lua_touserdata(L, -1);
    if (Ptr)
    {
        auto Property = static_cast<TSharedPtr<UnLuaNova::ITypeOps>*>(Ptr);
        if (Property->IsValid())
        {
            void* Self = GetCppInstance(L, 1);
            if (Self)
            {
                if (UnLuaNova::LowLevel::IsReleasedPtr(Self))
                    return luaL_error(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("attempt to write property '%s' on released object"), *(*Property)->GetName())));

                if (!UnLuaNova::LowLevel::CheckPropertyOwner(L, (*Property).Get(), Self))
                    return 0;

                (*Property)->WriteValue_InContainer(L, Self, 3);
            }
        }
    }
    else
    {
        int32 Type = lua_type(L, 1);
        if (Type == LUA_TTABLE)
        {
            lua_pushvalue(L, 2);
            lua_pushvalue(L, 3);
            lua_rawset(L, 1);

            //UE_LOG(LogUnLuaNova, Warning, TEXT("%s: You are modifying metatable! Please make sure you know what you are doing!"), ANSI_TO_TCHAR(__FUNCTION__));
        }
    }
    lua_pop(L, 1);
    return 0;
}

/**
 * Generic closure to call a UFunction
 */
int32 Class_CallUFunction(lua_State *L)
{
    //!!!Fix!!!
    //delete desc when is not valid
    auto& Env = UnLuaNova::FLuaEnv::FindEnvChecked(L);
    auto Function = Env.GetObjectRegistry()->Get<FFunctionDesc>(L, lua_upvalueindex(1));
    if (!Function->IsValid())
    {
        UE_LOG(LogUnLuaNova, Log, TEXT("%s: Invalid function descriptor!"), ANSI_TO_TCHAR(__FUNCTION__));
        return 0;
    }
    int32 NumParams = lua_gettop(L);
    int32 NumResults = Function->CallUE(L, NumParams);
    return NumResults;
}

/**
 * Generic closure to call a latent function
 */
int32 Class_CallLatentFunction(lua_State *L)
{
    auto& Env = UnLuaNova::FLuaEnv::FindEnvChecked(L);
    auto Function = Env.GetObjectRegistry()->Get<FFunctionDesc>(L, lua_upvalueindex(1));
	if (!Function->IsValid())
    {
        UE_LOG(LogUnLuaNova, Log, TEXT("%s: Invalid function descriptor!"), ANSI_TO_TCHAR(__FUNCTION__));
        return 0;
    }

    auto ThreadRef = Env.FindOrAddThread(L);
    if (ThreadRef == LUA_REFNIL)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s: Can't call latent action in main lua thread!"), ANSI_TO_TCHAR(__FUNCTION__));
        return 0;
    }

    // Luau only allows a C function to yield while no other C boundary has been crossed;
    // otherwise lua_yield raises the opaque
    // "attempt to yield across metamethod/C-call boundary". Reaching here through an
    // engine callback - UE calling into Lua with lua_pcall - is exactly that case, so
    // report it as what it is instead of letting the VM error surface.
    if (!UNLUANOVA_IS_YIELDABLE(L))
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s: cannot start a latent action here. This thread was entered from C (an engine callback), and the Luau backend only allows yielding from a thread Lua resumed directly - start the latent action from script instead."), ANSI_TO_TCHAR(__FUNCTION__));
        return 0;
    }

    int32 NumParams = lua_gettop(L);
    int32 NumResults = Function->CallUE(L, NumParams, &ThreadRef);
    return lua_yield(L, NumResults);
}

FClassDesc* Class_CheckParam(lua_State *L)
{
    FClassDesc *ClassDesc = (FClassDesc*)lua_touserdata(L, lua_upvalueindex(1));
    if (!ClassDesc)
    {
        UE_LOG(LogUnLuaNova, Log, TEXT("Class : Invalid FClassDesc!"));
        return NULL;
    }

    UClass *Class = ClassDesc->AsClass();
    if (!Class)
    {
        UE_LOG(LogUnLuaNova, Log, TEXT("Class : ClassDesc type is not class(Name : %s, Address : %p)"), *ClassDesc->GetName(),ClassDesc);
        return NULL;
    }

    return ClassDesc;
}

/**
 * Generic closure to get UClass for a type
 */
int32 Class_StaticClass(lua_State *L)
{
    FClassDesc *ClassDesc = Class_CheckParam(L);
	if (!ClassDesc)
	{
        return 0;
    }

    UClass *Class = ClassDesc->AsClass();
    UnLuaNova::PushUObject(L, Class);
    return 1;
}

/**
 * Cast a UObject
 */
int32 Class_Cast(lua_State* L)
{
    int32 NumParams = lua_gettop(L);
    if (NumParams < 2)
    {
        UNLUANOVA_LOGERROR(L, TEXT("%s: Invalid parameters!"), ANSI_TO_TCHAR(__FUNCTION__));
        return 0;
    }

    UObject *Object = UnLuaNova::GetUObject(L, 1);
    if (!Object)
    {
        return 0;
    }

    UClass *Class = Cast<UClass>(UnLuaNova::GetUObject(L, 2));
    if (Class && (Object->IsA(Class) || (Class->HasAnyClassFlags(CLASS_Interface) && Class != UInterface::StaticClass() && Object->GetClass()->ImplementsInterface(Class))))
    {
        lua_pushvalue(L, 1);
    }
    else
    {
        lua_pushnil(L);
    }
    return 1;
}

/**
 * __index meta methods for struct
 */
int32 ScriptStruct_Index(lua_State *L)
{
    GetField(L);
    if (lua_type(L, -1) != LUA_TUSERDATA)
        return 1;

    const auto Registry = UnLuaNova::FLuaEnv::FindEnvChecked(L).GetObjectRegistry();
    const auto Property = Registry->Get<UnLuaNova::ITypeOps>(L, -1);
    if (!Property.IsValid())
        return 0;

    void* Self = GetCppInstanceFast(L, 1);
    if (!Self)
        return luaL_error(L, TCHAR_TO_UTF8(*FString::Printf(TEXT("attempt to read property '%s' on released struct"), *Property->GetName())));

    Property->ReadValue_InContainer(L, Self, false);
    lua_remove(L, -2);
    return 1;
}

FClassDesc* ScriptStruct_CheckParam(lua_State *L)
{
    FClassDesc *ClassDesc = (FClassDesc*)lua_touserdata(L, lua_upvalueindex(1));
    if (!ClassDesc)
    {
        UE_LOG(LogUnLuaNova, Log, TEXT("ScriptStruct : Invalid FClassDesc!"));
        return NULL;
    }

    UScriptStruct *ScriptStruct = ClassDesc->AsScriptStruct();
    if (!ScriptStruct)
    {
        // UE_LOG(LogUnLuaNova, Log, TEXT("ScriptStruct : ClassDesc type is not script struct(Name : %s, Address : %p)"), *ClassDesc->GetName(),ClassDesc);
        return NULL;
    }

    return ClassDesc;
}

/**
 * Generic closure to create a UScriptStruct instance
 */
int32 ScriptStruct_New(lua_State *L)
{
    FClassDesc *ClassDesc = ScriptStruct_CheckParam(L);
    if (!ClassDesc)
    {
        return 0;
    }

    UScriptStruct *ScriptStruct = ClassDesc->AsScriptStruct();
    void *Userdata = NewUserdataWithPadding(L, ClassDesc->GetSize(), TCHAR_TO_UTF8(*ClassDesc->GetName()), ClassDesc->GetUserdataPadding());
    ScriptStruct->InitializeStruct(Userdata);

    return 1;
}

/**
 * Generic GC function for UScriptStruct
 */
int32 ScriptStruct_Delete(lua_State *L)
{
    FClassDesc *ClassDesc = ScriptStruct_CheckParam(L);
    if (!ClassDesc)
    {
        return 0;
    }

    UScriptStruct *ScriptStruct = ClassDesc->AsScriptStruct();

    bool bTwoLvlPtr = false;
    void * Userdata = GetUserdataFast(L, 1, &bTwoLvlPtr);
    if (Userdata)
    {   
        //struct in userdata memory
        if (!bTwoLvlPtr)
        {
            if (!(ScriptStruct->StructFlags & (STRUCT_IsPlainOldData | STRUCT_NoDestructor)))
            {
                ScriptStruct->DestroyStruct(Userdata);
            }
        }
    }
    return 0;
}

/**
 * Generic closure to copy a UScriptStruct
 */
int32 ScriptStruct_CopyFrom(lua_State *L)
{
    FClassDesc *ClassDesc = ScriptStruct_CheckParam(L);
    if (!ClassDesc)
    {
        return 0;
    }

    UScriptStruct *ScriptStruct = ClassDesc->AsScriptStruct();

	void *Src = GetCppInstanceFast(L, 1);
	void *Userdata = nullptr;
	if (lua_gettop(L) > 1)
	{
		Userdata = GetCppInstanceFast(L, 2);
		lua_pushvalue(L, 2);
	}
	else
	{
		Userdata = NewUserdataWithPadding(L, ClassDesc->GetSize(), TCHAR_TO_UTF8(*ClassDesc->GetName()), ClassDesc->GetUserdataPadding());
		ScriptStruct->InitializeStruct(Userdata);
	}
	ScriptStruct->CopyScriptStruct(Src,Userdata);
	return 1;
}


/**
 * Generic closure to copy a UScriptStruct
 */
int32 ScriptStruct_Copy(lua_State *L)
{
    FClassDesc *ClassDesc = ScriptStruct_CheckParam(L);
    if (!ClassDesc)
    {
        return 0;
    }

    UScriptStruct *ScriptStruct = ClassDesc->AsScriptStruct();

    void *Src = GetCppInstanceFast(L, 1);
    void *Userdata = nullptr;
    if (lua_gettop(L) > 1)
    {
        Userdata = GetCppInstanceFast(L, 2);
        lua_pushvalue(L, 2);
    }
    else
    {
        Userdata = NewUserdataWithPadding(L, ClassDesc->GetSize(), TCHAR_TO_UTF8(*ClassDesc->GetName()), ClassDesc->GetUserdataPadding());
        ScriptStruct->InitializeStruct(Userdata);
    }
    ScriptStruct->CopyScriptStruct(Userdata, Src);
    return 1;
}

/**
 * Generic closure to compare two UScriptStructs
 */
int32 ScriptStruct_Compare(lua_State *L)
{
    FClassDesc *ClassDesc = ScriptStruct_CheckParam(L);
    if (!ClassDesc)
    {
        return 0;
    }

    UScriptStruct *ScriptStruct = ClassDesc->AsScriptStruct();

    void *A = GetCppInstanceFast(L, 1);
    void *B = GetCppInstanceFast(L, 2);
    bool bResult = A && B ? ScriptStruct->CompareScriptStruct(A, B, /*PPF_None*/0) : false;
    lua_pushboolean(L, bResult);
    return 1;
}

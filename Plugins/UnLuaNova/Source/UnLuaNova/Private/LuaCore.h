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

#pragma once

#include "LuaBackend.h"
#include "UnLuaNovaPrivate.h"
#include "UnLuaNovaCompatibility.h"

struct FScriptContainerDesc
{
    FORCEINLINE int32 GetSize() const { return Size; }
    FORCEINLINE const char* GetName() const { return Name; }

    static const FScriptContainerDesc Array;
    static const FScriptContainerDesc Set;
    static const FScriptContainerDesc Map;

private:
    FORCEINLINE FScriptContainerDesc(int32 InSize, const char *InName)
        : Size(InSize), Name(InName)
    {}

    int32 Size;
    const char *Name;
};

FString GetFullPathFromRelativePath(const FString& RelativePath);
void SetTableForClass(lua_State *L, const char *Name);

/**
 * Set metatable for the userdata/table on the top of the stack
 */
bool TryToSetMetatable(lua_State *L, const char *MetatableName, UObject* Object = nullptr);

/**
 * The tag byte carried by every userdata descriptor. It lives here rather than in
 * LuaCore.cpp because the dangling check marks a released container from its own
 * translation unit, and a bit spelled out there as 1 << 6 is a bit nobody can check.
 */
#define USERDATA_MAGIC          0x1688
#define BIT_VARIANT_TAG         (1 << 7)    // variant tag for userdata
#define BIT_RELEASED_TAG        (1 << 6)    // this userdata was released and should not use anywhere
#define BIT_TWOLEVEL_PTR        (1 << 5)    // two level pointer flag
#define BIT_SCRIPT_CONTAINER    (1 << 4)    // script container (TArray, TSet, TMap) flag

/**
 * Functions to handle Lua userdata
 */
void* NewUserdataWithTwoLvPtrTag(lua_State* L, int Size, void* Object);
void* NewUserdataWithContainerTag(lua_State* L, int Size);
void MarkUserdataTwoLvPtrTag(lua_State* L, int32 Index);
void SetUserdataFlags(lua_State* L, int32 Index, uint8 Flags);
UNLUANOVA_API uint8 CalcUserdataPadding(int32 Alignment);
template <typename T> uint8 CalcUserdataPadding() { return CalcUserdataPadding(alignof(T)); }
UNLUANOVA_API void* GetUserdata(lua_State *L, int32 Index, bool *OutTwoLvlPtr = nullptr, bool *OutClassMetatable = nullptr);
UNLUANOVA_API void* GetUserdataFast(lua_State *L, int32 Index, bool *OutTwoLvlPtr = nullptr);
UNLUANOVA_API void* NewUserdataWithPadding(lua_State *L, int32 Size, const char *MetatableName, uint8 PaddingSize = 0);
#define NewTypedUserdata(L, Type) NewUserdataWithPadding(L, sizeof(Type), #Type, CalcUserdataPadding<Type>())
UNLUANOVA_API void* GetCppInstance(lua_State *L, int32 Index);
UNLUANOVA_API void* GetCppInstanceFast(lua_State *L, int32 Index);

/**
 * Functions to handle script containers
 */
void* NewScriptContainer(lua_State *L, const FScriptContainerDesc &Desc);
void* CacheScriptContainer(lua_State *L, void *Key, const FScriptContainerDesc &Desc);
void* CacheScriptContainer(lua_State* L, void* Key, const FScriptContainerDesc& Desc, const TFunctionRef<bool (void*)>& Validator);
void* GetScriptContainer(lua_State *L, int32 Index);
void RemoveCachedScriptContainer(lua_State *L, void *Key);

/**
 * Functions to push FProperty array
 */
void PushIntegerArray(lua_State *L, FNumericProperty *Property, void *Value);
void PushFloatArray(lua_State *L, FNumericProperty *Property, void *Value);
void PushEnumArray(lua_State *L, FNumericProperty *Property, void *Value);
void PushFNameArray(lua_State *L, FNameProperty *Property, void *Value);
void PushFStringArray(lua_State *L, FStrProperty *Property, void *Value);
void PushFTextArray(lua_State *L, FTextProperty *Property, void *Value);
void PushObjectArray(lua_State *L, FObjectPropertyBase *Property, void *Value);
void PushInterfaceArray(lua_State *L, FInterfaceProperty *Property, void *Value);
void PushDelegateArray(lua_State *L, FDelegateProperty *Property, void *Value);
void PushMCDelegateArray(lua_State *L, FMulticastDelegateProperty *Property, void *Value, const char *MetatableName);
void PushStructArray(lua_State *L, FProperty *Property, void *Value, const char *MetatableName);

/**
 * Push a UObject to Lua stack
 */
void PushObjectCore(lua_State *L, UObjectBaseUtility *Object);

/**
 * Get UObject and Lua function address for delegate
 */
int32 GetDelegateInfo(lua_State *L, int32 Index, UObject* &Object, const void* &Function);

int32 PushFunction(lua_State *L, UObjectBaseUtility *Object, const char *FunctionName);
bool PushFunction(lua_State *L, UObjectBaseUtility *Object, int32 FunctionRef);
bool CallFunction(lua_State *L, int32 NumArgs, int32 NumResults);

/**
 * Get corresponding Lua instance for a UObjectBaseUtility
 */
UNLUANOVA_API bool GetObjectMapping(lua_State *L, UObjectBaseUtility *Object);

/**
 * Add Lua package path
 */
UNLUANOVA_API void AddPackagePath(lua_State *L, const char *Path);

/**
 * Functions to create weak table
 */
void CreateWeakKeyTable(lua_State *L);
void CreateWeakValueTable(lua_State *L);

int32 TraverseTable(lua_State *L, int32 Index, void *Userdata, bool (*TraverseWorker)(lua_State*, void*));
bool PeekTableElement(lua_State *L, void *Userdata);

/**
 * Functions to handle UClass
 */
int32 Class_Index(lua_State* L);
int32 Class_NewIndex(lua_State* L);
int32 Class_CallUFunction(lua_State *L);
int32 Class_CallLatentFunction(lua_State *L);
int32 Class_StaticClass(lua_State *L);
int32 Class_Cast(lua_State* L);

/**
 * Functions to handle UScriptStruct
 */
int32 ScriptStruct_Index(lua_State *L);
int32 ScriptStruct_New(lua_State *L);
int32 ScriptStruct_Delete(lua_State *L);
int32 ScriptStruct_Copy(lua_State *L);
int32 ScriptStruct_CopyFrom(lua_State *L);
int32 ScriptStruct_Compare(lua_State *L);

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

#include "Containers/StaticBitArray.h"
#include "LuaBackend.h"
#include "ParamBufferAllocator.h"
#include "Registries/FunctionRegistry.h"
#include "ReflectionUtils/PropertyDesc.h"

struct FParameterCollection;
struct FStaticFuncBinding;

/**
 * Function descriptor
 */
class FFunctionDesc
{
public:
    FFunctionDesc(UFunction *InFunction, FParameterCollection *InDefaultParams);

    /**
     * Check the validity of this function
     *
     * @return - true if the function is valid, false otherwise
     */
    FORCEINLINE bool IsValid() const { return Function.IsValid(); }

    /**
     * Test if this function has return property
     *
     * @return - true if the function has return property, false otherwise
     */
    FORCEINLINE bool HasReturnProperty() const { return ReturnPropertyIndex > INDEX_NONE; }

    /**
     * Test if this function is a latent function
     *
     * @return - true if the function is a latent function, false otherwise
     */
    FORCEINLINE bool IsLatentFunction() const { return LatentPropertyIndex > INDEX_NONE; }

    /**
     * Get the number of out properties
     *
     * @return - the number of out properties. out properties means return property or non-const reference properties
     */
    FORCEINLINE uint8 GetNumOutProperties() const { return ReturnPropertyIndex > INDEX_NONE ? OutPropertyIndices.Num() + 1 : OutPropertyIndices.Num(); }

    /**
     * Get the number of non-const reference properties
     *
     * @return - the number of non-const reference properties.
     */
    FORCEINLINE uint8 GetNumNoConstRefProperties() const { return OutPropertyIndices.Num(); }

    /**
     * Get the 'true' function
     *
     * @return - the UFunction
     */
    FORCEINLINE UFunction* GetFunction() const { return Function.Get(); }

    FORCEINLINE const char* GetLuaFunctionName() const { return LuaFunctionName->Get(); }
 
    void CallLua(lua_State* L, lua_Integer FunctionRef, lua_Integer SelfRef, FFrame& Stack, RESULT_DECL);
 
    bool CallLua(lua_State* L, int32 LuaRef, void* Params, UObject* Self);
 
    /**
     * Call this UFunction
     *
     * @param NumParams - the number of parameters
     * @param Userdata - user data, now it's only used for latent function and it must be a 'int32'
     * @return - the number of return values pushed on the stack
     */
    int32 CallUE(lua_State *L, int32 NumParams, void *Userdata = nullptr);

    /**
     * Fire the delegate
     *
     * @param NumParams - the number of parameters
     * @param FirstParamIndex - Lua index of the first parameter
     * @param ScriptDelegate - the delegate
     * @return - the number of return values pushed on the stack
     */
    int32 ExecuteDelegate(lua_State *L, int32 NumParams, int32 FirstParamIndex, FScriptDelegate *ScriptDelegate);

    /**
     * Fire the multicast delegate
     *
     * @param NumParams - the number of parameters
     * @param FirstParamIndex - Lua index of the first parameter
     * @param ScriptDelegate - the multicast delegate
     */
    void BroadcastMulticastDelegate(lua_State *L, int32 NumParams, int32 FirstParamIndex, FMulticastScriptDelegate *ScriptDelegate);

private:
    typedef TStaticBitArray<64U> FFlagArray;

    /**
     * Prepare values of properties for the UFunction.
     *
     * @param OutError - optional; filled with the message when the return value is false
     * @return - false when a supplied argument fails its type check. Nothing has been
     *           marshalled past that point, but the parameter buffer is still held: the
     *           caller has to release it before raising, because raising does not return.
     */
    bool PreCall(lua_State* L, int32 NumParams, int32 FirstParamIndex, FFlagArray& CleanupFlags, void* Params, void* Userdata = nullptr, FString* OutError = nullptr);

    int32 PostCall(lua_State* L, int32 NumParams, int32 FirstParamIndex, void* Params, const FFlagArray& CleanupFlags);

    /** Destroy the values PostCall would destroy, without touching the Lua stack. */
    void DestroyParams(void* Params, const FFlagArray& CleanupFlags) const;

    bool CallLuaInternal(lua_State *L, void *InParams, FOutParmRec *OutParams, void *RetValueAddress) const;

    FORCEINLINE bool CheckObject(UObject* Object, FString& Error) const;

    /**
     * Static-binding fast path for CallUE.
     *
     * Skips the work that a pre-resolved FStaticFuncBinding makes unnecessary:
     *   - GetFunctionCallspace(): UObject::ProcessEvent already queries it for
     *     FUNC_Native functions, and never needs it otherwise. UnLuaNova calls it for
     *     every call, including pure Blueprint functions.
     *   - ULuaFunction::Get(): the override redirect can only hit overridable functions.
     *   - FindFunctionByName(): only needed for interface functions.
     * Everything else (buffer, PreCall/PostCall) is shared with the reflective path,
     * so behaviour is identical.
     */
    int32 CallUEFast(lua_State* L, int32 NumParams, int32 FirstParamIndex, UObject* Object, void* Userdata);

    TWeakObjectPtr<UFunction> Function;
    FString FuncName;
    TSharedPtr<FParamBufferAllocator> Buffer;
    TArray<TUniquePtr<FPropertyDesc>> Properties;
    TArray<int32> OutPropertyIndices;
    FParameterCollection *DefaultParams;
    int32 ReturnPropertyIndex;
    int32 LatentPropertyIndex;
    uint8 bStaticFunc : 1;
    uint8 bInterfaceFunc : 1;
    int32 ParmsSize;
    TUniquePtr<FTCHARToUTF8> LuaFunctionName;

    /** Non-null when this function belongs to a type with a static binding. */
    const FStaticFuncBinding* StaticBinding = nullptr;
};

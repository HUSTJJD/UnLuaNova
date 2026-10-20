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

#include "CoreUObject.h"

namespace UnLuaNova
{
    class FLuaEnv;
}

class FPropertyDesc;
class FFunctionDesc;
class FFieldDesc;

/**
 * Class descriptor
 */
class FClassDesc
{
public:
    FClassDesc(UnLuaNova::FLuaEnv *Env, UStruct *InStruct, const FString &InName);

    FORCEINLINE bool IsValid() const { return true; }

    FORCEINLINE bool IsStructValid() const { return Struct.IsValid(); }

    FORCEINLINE bool IsScriptStruct() const { return bIsScriptStruct; }

    FORCEINLINE bool IsClass() const { return bIsClass; }

    FORCEINLINE bool IsInterface() const { return bIsInterface; }

    FORCEINLINE bool IsNative() const { return bIsNative; }

    FORCEINLINE UStruct* AsStruct()
    {
        Load();
        return Struct.Get();
    }

    FORCEINLINE UScriptStruct* AsScriptStruct()
    {
        if (!bIsScriptStruct)
            return nullptr;
        Load();
        return static_cast<UScriptStruct*>(Struct.Get());
    }

    FORCEINLINE UClass* AsClass()
    {
        if (!bIsClass)
            return nullptr;
        Load();
        return static_cast<UClass*>(Struct.Get());
    }

    FORCEINLINE FString GetName() const { return ClassName; }

    FORCEINLINE int32 GetSize() const { return Size; }

    FORCEINLINE uint8 GetUserdataPadding() const { return UserdataPadding; }

    FORCEINLINE TSharedPtr<FPropertyDesc> GetProperty(int32 Index) { return Index > INDEX_NONE && Index < Properties.Num() ? Properties[Index] : nullptr; }

    FORCEINLINE TSharedPtr<FFunctionDesc> GetFunction(int32 Index) { return Index > INDEX_NONE && Index < Functions.Num() ? Functions[Index] : nullptr; }

    TSharedPtr<FFieldDesc> RegisterField(FName FieldName, FClassDesc *QueryClass = nullptr);

    void GetInheritanceChain(TArray<FClassDesc*>& Chain);

    void Load();
    
    void UnLoad();

private:
    // Not decoration and not dead state - this is part of the ABI.
    //
    // A class table (UE.UMyClass) is what a script hands over where a UObject is expected:
    // NewObject(UE.UMyClass, self), any UObject* parameter filled from UE.UMyClass. For such
    // a table LowLevel::GetUserdata() answers with the FClassDesc pointer (it reads the
    // table's "ClassDesc" light userdata) and flags it a two level pointer, so
    // GetCppInstance() then dereferences it: *(void**)Desc. What sits at that address has to
    // *be* the UObject/UStruct pointer - anything else gets read as one, and the weak
    // pointer's object index + serial number read as an address crashes on first use.
    //
    // Kept in step with Struct below, and nulled by UnLoad() so a stale class reads as null
    // rather than as a pointer to something that is gone.
    UStruct* StructPtr = nullptr;

    TWeakObjectPtr<UStruct> Struct;

    FString ClassName;

    uint8 bIsScriptStruct : 1;
    uint8 bIsClass : 1;
    uint8 bIsInterface : 1;
    uint8 bIsNative : 1;

    int32 UserdataPadding : 8;            // only used for UScriptStruct
    int32 Size : 24;

    TMap<FName, TSharedPtr<FFieldDesc>> Fields;
    TArray<TSharedPtr<FPropertyDesc>> Properties;
    TArray<TSharedPtr<FFunctionDesc>> Functions;
    TArray<FClassDesc*> SuperClasses;
    UnLuaNova::FLuaEnv* Env;

    struct FFunctionCollection *FunctionCollection;
};


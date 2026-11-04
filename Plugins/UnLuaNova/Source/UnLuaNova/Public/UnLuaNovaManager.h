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

#include "InputCoreTypes.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "LuaBackend.h"
#include "UnLuaNovaCompatibility.h"
#include "UnLuaNovaManager.generated.h"

namespace UnLuaNova
{
    class FLuaEnv;
}

UCLASS()
class UNLUANOVA_API UUnLuaNovaManager : public UObject
{
    GENERATED_BODY()

public:

    UnLuaNova::FLuaEnv* Env;

    UUnLuaNovaManager();

    bool Bind(UObject *Object, const TCHAR *InModuleName, int32 InitializerTableRef = LUA_NOREF);

    /**
     * 实例级绑定：让同一个 UClass 的单个实例使用与类默认不同的 Lua 模块。
     *
     * 类级绑定必须先建立（Bind/TryBind），因为未显式绑定的实例仍要回落到类模块表。
     * 类上可覆写的 UFunction 是类级资源，所以实例模块带来的新函数名会并入该类的覆写集合；
     * 未定义该函数的实例由调用分发回落到原 UFunction，不会串线。
     */
    bool BindInstance(UObject* Object, const TCHAR* InModuleName,
        int32 InitializerTableRef = LUA_NOREF, FString* OutError = nullptr);

    /** 撤销实例级绑定，元表回落到类默认模块表。 */
    bool UnbindInstance(UObject* Object);

    bool HasInstanceBinding(const UObject* Object) const;
    int GetInstanceBoundRef(const UObject* Object) const;
    FString GetInstanceBoundModuleName(const UObject* Object) const;

    /** 蓝图入口：把某个对象绑到指定 Lua 模块（返回是否成功）。 */
    UFUNCTION(BlueprintCallable, Category = "UnLuaNova|Binding")
    bool BindObjectToModule(UObject* Object, const FString& ModuleName);

    UFUNCTION(BlueprintCallable, Category = "UnLuaNova|Binding")
    bool UnbindObjectFromModule(UObject* Object);

    void NotifyUObjectDeleted(const UObjectBase *Object);

    void Cleanup();

    int GetBoundRef(const UClass* Class);

    void GetDefaultInputs();

    void CleanupDefaultInputs();

    bool ReplaceInputs(AActor *Actor, class UInputComponent *InputComponent);

    void OnMapLoaded(UWorld *World);

    UFUNCTION(BlueprintCallable)
    UDynamicBlueprintBinding* GetOrAddBindingObject(UClass* Class, UClass* BindingClass);

    UFUNCTION(BlueprintCallable)
    void Override(UClass* Class, FName FunctionName, FName LuaFunctionName);

    UFUNCTION()
    void OnLatentActionCompleted(int32 LinkID);

    UFUNCTION(BlueprintImplementableEvent)
    void EnhancedInputActionDigital(bool ActionValue, float ElapsedSeconds, float TriggeredSeconds);

    UFUNCTION(BlueprintImplementableEvent)
    void EnhancedInputActionAxis1D(float ActionValue, float ElapsedSeconds, float TriggeredSeconds);

    UFUNCTION(BlueprintImplementableEvent)
    void EnhancedInputActionAxis2D(const FVector2D& ActionValue, float ElapsedSeconds, float TriggeredSeconds);

    UFUNCTION(BlueprintImplementableEvent)
    void EnhancedInputActionAxis3D(const FVector& ActionValue, float ElapsedSeconds, float TriggeredSeconds);
    
    UFUNCTION(BlueprintImplementableEvent)
    void InputAction(FKey Key);

    UFUNCTION(BlueprintImplementableEvent)
    void InputAxis(float AxisValue);

    UFUNCTION(BlueprintImplementableEvent)
    void InputTouch(ETouchIndex::Type FingerIndex, const FVector &Location);

    UFUNCTION(BlueprintImplementableEvent)
    void InputVectorAxis(const FVector &AxisValue);

    UFUNCTION(BlueprintImplementableEvent)
    void InputGesture(float Value);

    UFUNCTION(BlueprintImplementableEvent)
    void TriggerAnimNotify();

private:
    /* 将一个UClass绑定到Lua模块，根据这个模块定义的函数列表来覆盖上面的UFunction */
    bool BindClass(UClass *Class, const FString &InModuleName, FString &Error);

    void ReplaceActionInputs(AActor *Actor, UInputComponent *InputComponent, TSet<FName> &LuaFunctions);
    void ReplaceKeyInputs(AActor *Actor, UInputComponent *InputComponent, TSet<FName> &LuaFunctions);
    void ReplaceAxisInputs(AActor *Actor, UInputComponent *InputComponent, TSet<FName> &LuaFunctions);
    void ReplaceTouchInputs(AActor *Actor, UInputComponent *InputComponent, TSet<FName> &LuaFunctions);
    void ReplaceAxisKeyInputs(AActor *Actor, UInputComponent *InputComponent, TSet<FName> &LuaFunctions);
    void ReplaceVectorAxisInputs(AActor *Actor, UInputComponent *InputComponent, TSet<FName> &LuaFunctions);
    void ReplaceGestureInputs(AActor *Actor, UInputComponent *InputComponent, TSet<FName> &LuaFunctions);

    struct FClassBindInfo
    {
        UClass* Class;
        FString ModuleName;
        int TableRef;
        TSet<FName> LuaFunctions;
        TMap<FName, UFunction*> UEFunctions;
    };

    TMap<UClass*, FClassBindInfo> Classes;

    struct FInstanceBindInfo
    {
        FString ModuleName;
        int TableRef = LUA_NOREF;
        TSet<FName> LuaFunctions;
    };

    // UObject* 是裸键，与 FObjectRegistry::ObjectRefs 一致；NotifyUObjectDeleted 与
    // Cleanup 必须逐条 luaL_unref 并移除，否则对象销毁后 ref 会永久泄漏。
    TMap<UObject*, FInstanceBindInfo> Instances;

    TSet<FName> DefaultAxisNames;
    TSet<FName> DefaultActionNames;
    TArray<FKey> AllKeys;

    UFunction *InputActionFunc;
    UFunction *InputAxisFunc;
    UFunction *InputTouchFunc;
    UFunction *InputVectorAxisFunc;
    UFunction *InputGestureFunc;
    UFunction *AnimNotifyFunc;
};

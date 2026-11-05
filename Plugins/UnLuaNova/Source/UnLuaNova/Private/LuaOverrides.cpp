#include "LuaOverrides.h"
#include "LuaFunction.h"
#include "UnLuaNovaBase.h"
#include "LuaOverridesClass.h"
#include "UObject/MetaData.h"

namespace UnLuaNova
{
    FLuaOverrides& FLuaOverrides::Get()
    {
        static FLuaOverrides Override;
        return Override;
    }

    FLuaOverrides::FLuaOverrides()
    {
        GUObjectArray.AddUObjectDeleteListener(this);
    }

    void FLuaOverrides::NotifyUObjectDeleted(const UObjectBase* Object, int32 Index)
    {
        TWeakObjectPtr<ULuaOverridesClass> OverridesClass;
        if (Overrides.RemoveAndCopyValue((UClass*)Object, OverridesClass) )
        {
            if ( OverridesClass.IsValid( ) )
            {
                OverridesClass->Restore();
             }
         }
    }

    void FLuaOverrides::OnUObjectArrayShutdown()
    {
        GUObjectArray.RemoveUObjectDeleteListener(this);
    }

    void FLuaOverrides::Override(UFunction* Function, UClass* Class, FName NewName)
    {
        const auto OverridesClass = GetOrAddOverridesClass(Class);

        ULuaFunction* LuaFunction;
        const auto bAddNew = Function->GetOuter() != Class;

        // Overrides live on the UClass and the function registry resolves the Lua module
        // per object at call time, so binding another object - or rebinding an instance to
        // a different module - never needs a second thunk on the same UFunction. An
        // already-patched UFunction keeps its original Script/native func only inside the
        // ULuaFunction recorded in its script data; stacking another override there used to
        // lose that chain and trip a fatal assertion in ULuaFunction::Override.
        if (ULuaFunction::IsScriptOverrideThunk(Function))
        {
            const ULuaFunction* Existing = ULuaFunction::Get(Function);
            // bAddNew is the legitimate case: a derived class overrides a function that
            // its parent already patched, and reuses the recorded original implementation.
            // It still requires a readable header and a recoverable Overridden.
            if (!bAddNew || !Existing || !Existing->GetOverridden())
            {
                if (!bAddNew && Existing)
                {
                    // Idempotent rebind: nothing to do, the live thunk stays in place.
                    return;
                }
                UE_LOG(LogUnLuaNova, Warning,
                    TEXT("FLuaOverrides::Override: refusing to stack an override on '%s' (patched=%d, existing=%d)"),
                    *Function->GetName(), 1, Existing != nullptr ? 1 : 0);
                return;
            }
        }

        if (bAddNew)
        {
            const auto Exists = Class->FindFunctionByName(NewName, EIncludeSuperFlag::ExcludeSuper);
            if (Exists && Exists->GetSuperStruct() == Function)
                return;
        }
        else
        {
            LuaFunction = Cast<ULuaFunction>(Function);
            if (LuaFunction)
            {
                LuaFunction->Initialize();
                return;
            }
        }

        const auto OriginalFunctionFlags = Function->FunctionFlags;
        Function->FunctionFlags &= (~EFunctionFlags::FUNC_Native);

        FObjectDuplicationParameters DuplicationParams(Function, OverridesClass);
        DuplicationParams.InternalFlagMask &= ~EInternalObjectFlags::Native;
        DuplicationParams.DestName = NewName;
        DuplicationParams.DestClass = ULuaFunction::StaticClass();
        LuaFunction = static_cast<ULuaFunction*>(StaticDuplicateObjectEx(DuplicationParams));

        Function->FunctionFlags = OriginalFunctionFlags;
        LuaFunction->FunctionFlags = OriginalFunctionFlags;

        LuaFunction->Next = OverridesClass->Children;
        OverridesClass->Children = LuaFunction;

        LuaFunction->StaticLink(true);
        LuaFunction->Initialize();
        LuaFunction->Override(Function, Class, bAddNew);
        LuaFunction->Bind();

        if (Class->IsRooted() || GUObjectArray.IsDisregardForGC(Class))
            LuaFunction->AddToRoot();
        else
            LuaFunction->AddToCluster(Class);
    }

    void FLuaOverrides::Restore(UClass* Class)
    {
        TWeakObjectPtr<ULuaOverridesClass> OverridesClass;
        if ( !Overrides.RemoveAndCopyValue( Class, OverridesClass) )
            return;
            
        if ( !OverridesClass.IsValid() )
            return;

        OverridesClass->Restore();
    }

    void FLuaOverrides::RestoreAll()
    {
        TArray<UClass*> Classes;
        Overrides.GenerateKeyArray(Classes);
        for (const auto& Class : Classes)
            Restore(Class);
    }

    void FLuaOverrides::Suspend(UClass* Class)
    {
        if (const auto Exists = Overrides.Find(Class))
        {
            if ( Exists != nullptr && Exists->IsValid() )
            {
                Exists->Get()->SetActive(false);
            }
        }
    }

    void FLuaOverrides::Resume(UClass* Class)
    {
        if (const auto Exists = Overrides.Find(Class))
        {
            if ( Exists != nullptr && Exists->IsValid() )
            {
                Exists->Get()->SetActive(true);
            }
        }
    }

    UClass* FLuaOverrides::GetOrAddOverridesClass(UClass* Class)
    {
        const auto Exists = Overrides.Find(Class);
        if ( Exists != nullptr && Exists->IsValid() )
            return Exists->Get();

        const auto OverridesClass = ULuaOverridesClass::Create(Class);
        Overrides.Add(Class, OverridesClass);
        return OverridesClass;
    }
}

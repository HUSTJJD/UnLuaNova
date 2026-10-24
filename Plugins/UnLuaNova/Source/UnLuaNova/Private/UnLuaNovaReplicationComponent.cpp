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

#include "UnLuaNovaReplicationComponent.h"

#include "Net/UnrealNetwork.h"
#include "Registries/ObjectRegistry.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaNet.h"

namespace
{
    /** Read a field off the Lua table bound to Object. False when it is not there. */
    bool ReadLuaField(lua_State* L, int32 TableIdx, FName Name, FString& OutValue)
    {
        const FTCHARToUTF8 Key(*Name.ToString());
        lua_getfield(L, TableIdx, Key.Get());

        const int32 Type = lua_type(L, -1);
        if (Type == LUA_TNIL || Type == LUA_TNONE)
        {
            lua_pop(L, 1);
            return false;
        }

        // Explicit type checks rather than lua_tostring throughout: on both backends
        // lua_tostring on a number rewrites the value in place, which would silently turn
        // the script's number into a string.
        if (lua_isnumber(L, -1))
            OutValue = FString::SanitizeFloat(lua_tonumber(L, -1));
        else if (lua_isstring(L, -1))
            OutValue = UTF8_TO_TCHAR(lua_tostring(L, -1));
        else if (lua_isboolean(L, -1))
            OutValue = lua_toboolean(L, -1) ? TEXT("true") : TEXT("false");
        else
            OutValue = FString::Printf(TEXT("<%s>"), UTF8_TO_TCHAR(lua_typename(L, lua_type(L, -1))));

        lua_pop(L, 1);
        return true;
    }

    /** Write a field back into the Lua table bound to Object. */
    void WriteLuaField(lua_State* L, int32 TableIdx, FName Name, int32 Type, const FString& Value)
    {
        const FTCHARToUTF8 Key(*Name.ToString());

        switch (UnLuaNova::ELuaNetParamType(Type))
        {
        case UnLuaNova::ELuaNetParamType::Int:
        case UnLuaNova::ELuaNetParamType::Int64:
            lua_pushinteger(L, FCString::Atoi64(*Value));
            break;
        case UnLuaNova::ELuaNetParamType::Float:
        case UnLuaNova::ELuaNetParamType::Double:
            lua_pushnumber(L, FCString::Atod(*Value));
            break;
        case UnLuaNova::ELuaNetParamType::Bool:
            lua_pushboolean(L, Value.ToBool());
            break;
        default:
        {
            const FTCHARToUTF8 Text(*Value);
            lua_pushstring(L, Text.Get());
            break;
        }
        }

        lua_setfield(L, TableIdx, Key.Get());
    }
}

UUnLuaNovaReplicationComponent::UUnLuaNovaReplicationComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    Condition = COND_None;
}

void UUnLuaNovaReplicationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UUnLuaNovaReplicationComponent, Payload, Condition);
}

void UUnLuaNovaReplicationComponent::BeginPlay()
{
    Super::BeginPlay();

    const AActor* Owner = GetOwner();
    if (!Owner)
        return;

    const TArray<UnLuaNova::FLuaRepDecl>& Decls = UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Owner->GetClass());
    if (Decls.Num() == 0)
    {
        // Nothing declared means nothing to replicate; leave ticking off so a component
        // someone added "just in case" costs nothing.
        SetComponentTickEnabled(false);
        return;
    }

    bDeclared = true;

    // The payload has one condition for every field. Honour the declarations when they
    // agree, otherwise take the first non-None and say so - silently picking one would be
    // a replication bug that looks like a gameplay bug.
    uint8 First = COND_None;
    for (const UnLuaNova::FLuaRepDecl& Decl : Decls)
    {
        if (Decl.Condition == COND_None)
            continue;
        if (First == COND_None)
        {
            First = Decl.Condition;
        }
        else if (First != Decl.Condition)
        {
            UE_LOG(LogUnLuaNova, Warning,
                TEXT("'%s' declares conflicting replication conditions; '%s' will use the first one. "
                     "Per-field conditions need one UPROPERTY per condition and cannot be added at runtime."),
                *Owner->GetClass()->GetName(), *Decl.Name.ToString());
        }
    }

    if (First != COND_None)
        Condition = (ELifetimeCondition)First;

    // Clients never push; they only receive.
    SetComponentTickEnabled(GetOwnerRole() == ROLE_Authority);

    PullFromLua();
}

void UUnLuaNovaReplicationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bDeclared = false;
    Super::EndPlay(EndPlayReason);
}

void UUnLuaNovaReplicationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bDeclared)
        PullFromLua();
}

void UUnLuaNovaReplicationComponent::OnRep_Payload()
{
    if (bDeclared)
        PushToLua();
}

void UUnLuaNovaReplicationComponent::PullFromLua()
{
    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    IUnLuaNovaModule& Module = IUnLuaNovaModule::Get();
    if (!Module.IsActive())
        return;

    UnLuaNova::FLuaEnv* Env = Module.GetEnv(Owner);
    if (!Env)
        return;

    lua_State* L = Env->GetMainState();
    const int32 Ref = Env->GetObjectRegistry()->GetBoundRef(Owner);
    if (Ref == LUA_NOREF)
        return;

    const int32 Top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
    const int32 TableIdx = lua_absindex(L, -1);

    const TArray<UnLuaNova::FLuaRepDecl>& Decls = UnLuaNova::FUnLuaNovaNet::GetReplicatedProps(Owner->GetClass());

    bool bChanged = Payload.Fields.Num() != Decls.Num();
    if (!bChanged)
    {
        for (int32 Index = 0; Index < Decls.Num(); ++Index)
        {
            if (Payload.Fields[Index].Name != Decls[Index].Name)
            {
                bChanged = true;
                break;
            }
        }
    }

    Payload.Fields.SetNum(Decls.Num());
    for (int32 Index = 0; Index < Decls.Num(); ++Index)
    {
        FLuaRepField& Field = Payload.Fields[Index];
        Field.Name = Decls[Index].Name;
        Field.Type = int32(Decls[Index].Type);

        FString Value;
        if (ReadLuaField(L, TableIdx, Field.Name, Value) && Field.Value != Value)
        {
            Field.Value = Value;
            bChanged = true;
        }
    }

    lua_settop(L, Top);

    // Assigning into a UPROPERTY is what marks the subobject dirty for replication; the
    // engine compares against its own shadow state, so an unchanged payload costs nothing.
    if (bChanged)
        MarkNetDirty();
}

void UUnLuaNovaReplicationComponent::PushToLua()
{
    AActor* Owner = GetOwner();
    if (!Owner)
        return;

    IUnLuaNovaModule& Module = IUnLuaNovaModule::Get();
    if (!Module.IsActive())
        return;

    UnLuaNova::FLuaEnv* Env = Module.GetEnv(Owner);
    if (!Env)
        return;

    lua_State* L = Env->GetMainState();
    const int32 Ref = Env->GetObjectRegistry()->GetBoundRef(Owner);
    if (Ref == LUA_NOREF)
        return;

    const int32 Top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
    const int32 TableIdx = lua_absindex(L, -1);

    for (const FLuaRepField& Field : Payload.Fields)
        WriteLuaField(L, TableIdx, Field.Name, Field.Type, Field.Value);

    lua_settop(L, Top);
}

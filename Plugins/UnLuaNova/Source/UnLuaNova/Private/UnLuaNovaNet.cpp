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

#include "UnLuaNovaNet.h"

#include "Engine/EngineBaseTypes.h"
#include "LuaFunction.h"
#include "UnLuaNovaBase.h"
#include "UObject/UnrealType.h"

namespace UnLuaNova
{
    TMap<TWeakObjectPtr<UClass>, TArray<FLuaRepDecl>> FUnLuaNovaNet::RepProps;

    // ------------------------------------------------------------------ parsing

    bool ParseLuaNetParamType(const FString& Text, ELuaNetParamType& OutType)
    {
        // Case-insensitive on purpose: these names come out of a script, and a capitalisation
        // slip silently producing an Int when a Float was meant is worse than an error.
        const FString Lower = Text.ToLower();

        if (Lower == TEXT("bool"))       { OutType = ELuaNetParamType::Bool;       return true; }
        if (Lower == TEXT("int") || Lower == TEXT("int32"))
                                         { OutType = ELuaNetParamType::Int;        return true; }
        if (Lower == TEXT("int64"))      { OutType = ELuaNetParamType::Int64;      return true; }
        if (Lower == TEXT("float"))      { OutType = ELuaNetParamType::Float;      return true; }
        if (Lower == TEXT("double"))     { OutType = ELuaNetParamType::Double;     return true; }
        if (Lower == TEXT("string") || Lower == TEXT("fstring") || Lower == TEXT("str"))
                                         { OutType = ELuaNetParamType::String;     return true; }
        if (Lower == TEXT("name") || Lower == TEXT("fname"))
                                         { OutType = ELuaNetParamType::Name;       return true; }
        if (Lower == TEXT("vector") || Lower == TEXT("fvector"))
                                         { OutType = ELuaNetParamType::Vector;     return true; }
        if (Lower == TEXT("rotator") || Lower == TEXT("frotator"))
                                         { OutType = ELuaNetParamType::Rotator;    return true; }
        if (Lower == TEXT("transform") || Lower == TEXT("ftransform"))
                                         { OutType = ELuaNetParamType::Transform;  return true; }
        if (Lower == TEXT("object") || Lower == TEXT("uobject"))
                                         { OutType = ELuaNetParamType::Object;     return true; }

        return false;
    }

    bool ParseLifetimeCondition(const FString& Text, uint8& OutCondition)
    {
        // UE's own constants. There is no reason to own a parallel enum for these.
        const FString Lower = Text.ToLower();

        if (Lower == TEXT("none"))                        { OutCondition = COND_None;                      return true; }
        if (Lower == TEXT("initialonly"))                 { OutCondition = COND_InitialOnly;               return true; }
        if (Lower == TEXT("owneronly"))                   { OutCondition = COND_OwnerOnly;                 return true; }
        if (Lower == TEXT("skipowner"))                   { OutCondition = COND_SkipOwner;                 return true; }
        if (Lower == TEXT("simulatedonly"))               { OutCondition = COND_SimulatedOnly;             return true; }
        if (Lower == TEXT("autonomousonly"))              { OutCondition = COND_AutonomousOnly;            return true; }
        if (Lower == TEXT("simulatedorphysics"))          { OutCondition = COND_SimulatedOrPhysics;        return true; }
        if (Lower == TEXT("initialorowner"))              { OutCondition = COND_InitialOrOwner;            return true; }
        if (Lower == TEXT("custom"))                      { OutCondition = COND_Custom;                    return true; }
        if (Lower == TEXT("replayorowner"))               { OutCondition = COND_ReplayOrOwner;             return true; }
        if (Lower == TEXT("replayonly"))                  { OutCondition = COND_ReplayOnly;                return true; }
        if (Lower == TEXT("simulatedonlynoreplay"))       { OutCondition = COND_SimulatedOnlyNoReplay;     return true; }
        if (Lower == TEXT("simulatedorphysicsnoreplay"))  { OutCondition = COND_SimulatedOrPhysicsNoReplay; return true; }
        if (Lower == TEXT("skipreplay"))                  { OutCondition = COND_SkipReplay;                return true; }

        return false;
    }

    namespace
    {
        /** Read a string field off the table at TableIdx, or the empty string. */
        FString GetStringField(lua_State* L, int32 TableIdx, const char* Field)
        {
            if (lua_getfield(L, TableIdx, Field) != LUA_TSTRING)
            {
                lua_pop(L, 1);
                return FString();
            }
            const FString Value = UTF8_TO_TCHAR(lua_tostring(L, -1));
            lua_pop(L, 1);
            return Value;
        }

        /**
         * Parameters: an array of type names ("Int"), or of tables
         * ({ Name = "HP", Type = "Float" }). Unnamed ones become Arg0, Arg1, ...
         */
        void ParseParams(lua_State* L, int32 TableIdx, FLuaRpcDecl& Decl)
        {
            if (lua_getfield(L, TableIdx, "Params") != LUA_TTABLE)
            {
                lua_pop(L, 1);
                return;
            }

            const int32 ParamsIdx = lua_absindex(L, -1);
            const int32 Count = int32(lua_objlen(L, ParamsIdx));

            for (int32 Index = 1; Index <= Count; ++Index)
            {
                lua_rawgeti(L, ParamsIdx, Index);

                FLuaRpcParam Param;
                bool bOk = false;

                if (lua_isstring(L, -1))
                {
                    bOk = ParseLuaNetParamType(UTF8_TO_TCHAR(lua_tostring(L, -1)), Param.Type);
                }
                else if (lua_istable(L, -1))
                {
                    const int32 EntryIdx = lua_absindex(L, -1);
                    const FString TypeName = GetStringField(L, EntryIdx, "Type");
                    const FString NameText = GetStringField(L, EntryIdx, "Name");
                    if (!TypeName.IsEmpty())
                    {
                        bOk = ParseLuaNetParamType(TypeName, Param.Type);
                        if (!NameText.IsEmpty())
                            Param.Name = FName(*NameText);
                    }
                }

                if (bOk)
                {
                    if (Param.Name.IsNone())
                        Param.Name = FName(*FString::Printf(TEXT("Arg%d"), Decl.Params.Num()));
                    Decl.Params.Add(Param);
                }
                else
                {
                    UE_LOG(LogUnLuaNova, Warning, TEXT("Ignoring unrecognised parameter #%d of RPC '%s'."),
                        Index, *Decl.Name.ToString());
                }

                lua_pop(L, 1); // the entry
            }

            lua_pop(L, 1); // the Params table
        }

        void ParseRpcDecl(lua_State* L, int32 ValueIdx, FLuaRpcDecl& Decl)
        {
            const int32 Abs = lua_absindex(L, ValueIdx);

            // `Foo = true` declares it with default flags - i.e. unreliable, matching
            // UFUNCTION(Server) in C++. Reliability is opt-in through the table form.
            if (!lua_istable(L, Abs))
                return;

            if (lua_getfield(L, Abs, "Reliable") != LUA_TNIL)
                Decl.bReliable = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);

            ParseParams(L, Abs, Decl);
        }

        FProperty* CreateStructParam(UFunction* Outer, FName Name, const TCHAR* StructPath)
        {
            // FindObject by path rather than TBaseStructure<FVector>::Get(): no header to
            // guess at, and it cannot drift when the struct moves packages.
            UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, StructPath);
            if (!Struct)
                return nullptr;

            auto* Property = NewObject<FStructProperty>(Outer, Name, RF_Transient);
            Property->Struct = Struct;
            return Property;
        }

        FProperty* CreateParam(UFunction* Outer, const FLuaRpcParam& Param)
        {
            switch (Param.Type)
            {
            case ELuaNetParamType::Bool:
            {
                auto* Property = NewObject<FBoolProperty>(Outer, Param.Name, RF_Transient);
                // Without this the element size stays at whatever the default ctor left,
                // and ParmsSize - hence the whole call frame - comes out wrong.
                Property->SetBoolSize(sizeof(bool), true);
                return Property;
            }
            case ELuaNetParamType::Int:       return NewObject<FIntProperty>(Outer, Param.Name, RF_Transient);
            case ELuaNetParamType::Int64:     return NewObject<FInt64Property>(Outer, Param.Name, RF_Transient);
            case ELuaNetParamType::Float:     return NewObject<FFloatProperty>(Outer, Param.Name, RF_Transient);
            case ELuaNetParamType::Double:    return NewObject<FDoubleProperty>(Outer, Param.Name, RF_Transient);
            case ELuaNetParamType::String:    return NewObject<FStrProperty>(Outer, Param.Name, RF_Transient);
            case ELuaNetParamType::Name:      return NewObject<FNameProperty>(Outer, Param.Name, RF_Transient);
            case ELuaNetParamType::Vector:    return CreateStructParam(Outer, Param.Name, TEXT("/Script/CoreUObject.Vector"));
            case ELuaNetParamType::Rotator:   return CreateStructParam(Outer, Param.Name, TEXT("/Script/CoreUObject.Rotator"));
            case ELuaNetParamType::Transform: return CreateStructParam(Outer, Param.Name, TEXT("/Script/CoreUObject.Transform"));
            case ELuaNetParamType::Object:
            {
                auto* Property = NewObject<FObjectProperty>(Outer, Param.Name, RF_Transient);
                Property->SetPropertyClass(UObject::StaticClass());
                return Property;
            }
            }

            return nullptr;
        }

        /**
         * Hand the fresh UFunction an RPC id if the engine did not.
         *
         * Blueprint RPCs get theirs when the blueprint class is linked. A function we add
         * at runtime may or may not, depending on whether AddFunctionToFunctionMap assigns
         * it, so: if it is still 0, take the next free one above everything already on the
         * class. DeclareRPCs sorts names before registration; Lua table traversal order
         * is not a network protocol. Class binding order still requires peer validation.
         *
         * If the engine did assign it, this is a no-op and the engine's value wins.
         */
        void EnsureRpcId(UClass* Class, UFunction* Function)
        {
            if (!Function->HasAnyFunctionFlags(FUNC_Net) || Function->RPCId != 0)
                return;

            int32 Max = 0;
            for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
                Max = FMath::Max(Max, int32(It->RPCId));

            Function->RPCId = int16(Max + 1);
        }

        UFunction* BuildRpcTemplate(const FLuaRpcDecl& Decl)
        {
            auto* Function = NewObject<UFunction>(GetTransientPackage(), Decl.Name, RF_Transient);

            uint32 Flags = FUNC_Public | FUNC_Net;
            switch (Decl.Kind)
            {
            case ELuaRpcKind::Server:    Flags |= FUNC_NetServer;    break;
            case ELuaRpcKind::Client:    Flags |= FUNC_NetClient;    break;
            case ELuaRpcKind::Multicast: Flags |= FUNC_NetMulticast; break;
            }
            if (Decl.bReliable)
                Flags |= FUNC_NetReliable;

            Function->FunctionFlags = static_cast<EFunctionFlags>(Flags);

            FProperty* Previous = nullptr;
            for (const FLuaRpcParam& Param : Decl.Params)
            {
                FProperty* Property = CreateParam(Function, Param);
                if (!Property)
                {
                    UE_LOG(LogUnLuaNova, Warning, TEXT("Could not build parameter '%s' of RPC '%s'; skipping it."),
                        *Param.Name.ToString(), *Decl.Name.ToString());
                    continue;
                }

                Property->SetPropertyFlags(CPF_Parm);

                if (Previous)
                    Previous->Next = Property;
                else
                    Function->ChildProperties = Property;
                Previous = Property;
            }

            // Builds PropertyLink, NumParms and ParmsSize from ChildProperties. Mirrors what
            // LuaFunction::SetActive() does for functions it duplicates.
            Function->StaticLink(true);
            return Function;
        }
    }

    // ------------------------------------------------------------------ RPC

    void FUnLuaNovaNet::DeclareRPCs(lua_State* L, int32 ModuleTableIdx, UClass* Class)
    {
        if (!L || !Class)
            return;

        const int32 ModuleIdx = lua_absindex(L, ModuleTableIdx);

        static const struct { const char* Field; ELuaRpcKind Kind; } Kinds[] =
        {
            { "ServerRPC",    ELuaRpcKind::Server    },
            { "ClientRPC",    ELuaRpcKind::Client    },
            { "MulticastRPC", ELuaRpcKind::Multicast },
        };

        for (const auto& Entry : Kinds)
        {
            if (lua_getfield(L, ModuleIdx, Entry.Field) != LUA_TTABLE)
            {
                lua_pop(L, 1);
                continue;
            }

            const int32 KindIdx = lua_absindex(L, -1);
            TArray<FString> Names;
            lua_pushnil(L);
            while (lua_next(L, KindIdx) != 0)
            {
                if (lua_type(L, -2) == LUA_TSTRING)
                    Names.Add(UTF8_TO_TCHAR(lua_tostring(L, -2)));
                lua_pop(L, 1);
            }
            Names.Sort();
            for (const FString& Name : Names)
            {
                lua_getfield(L, KindIdx, TCHAR_TO_UTF8(*Name));
                {
                    FLuaRpcDecl Decl;
                    Decl.Name = FName(*Name);
                    Decl.Kind = Entry.Kind;
                    ParseRpcDecl(L, -1, Decl);

                    if (Class->FindFunctionByName(Decl.Name, EIncludeSuperFlag::ExcludeSuper))
                    {
                        // Re-binding after a hot reload must not replace an RPC the engine
                        // (or a blueprint) already owns; ULuaFunction::SetActive() checks
                        // this too and would fail the check.
                        UE_LOG(LogUnLuaNova, Warning,
                            TEXT("'%s' already declares a function named '%s'; the Lua RPC declaration is ignored."),
                            *Class->GetName(), *Decl.Name.ToString());
                    }
                    else
                    {
                        UFunction* Template = BuildRpcTemplate(Decl);
                        ULuaFunction::Override(Template, Class, Decl.Name);

                        if (auto* Added = Cast<ULuaFunction>(Class->FindFunctionByName(Decl.Name, EIncludeSuperFlag::ExcludeSuper)))
                            EnsureRpcId(Class, Added);
                        else
                            UE_LOG(LogUnLuaNova, Warning, TEXT("Failed to register Lua RPC '%s' on '%s'."),
                                *Decl.Name.ToString(), *Class->GetName());
                    }
                }

                lua_pop(L, 1);
            }

            lua_pop(L, 1); // the kind table
        }
    }

    // ------------------------------------------------------------------ replication

    void FUnLuaNovaNet::DeclareReplicatedProps(lua_State* L, int32 ModuleTableIdx, UClass* Class)
    {
        if (!L || !Class)
            return;

        const int32 ModuleIdx = lua_absindex(L, ModuleTableIdx);

        // Either a function returning the list, or the list itself. The function form is
        // what slua spells, so it is what a migrating script already has.
        bool bHaveList = false;
        if (lua_getfield(L, ModuleIdx, "GetLifetimeReplicatedProps") == LUA_TFUNCTION)
        {
            lua_pushvalue(L, ModuleIdx); // self
            if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_istable(L, -1))
                bHaveList = true;
            else
                lua_pop(L, 1);
        }
        else
        {
            lua_pop(L, 1);

            if (lua_getfield(L, ModuleIdx, "ReplicatedProps") == LUA_TTABLE)
                bHaveList = true;
            else
                lua_pop(L, 1);
        }

        if (!bHaveList)
            return;

        const int32 ListIdx = lua_absindex(L, -1);

        TArray<FLuaRepDecl> Decls;
        const int32 Count = int32(lua_objlen(L, ListIdx));
        for (int32 Index = 1; Index <= Count; ++Index)
        {
            lua_rawgeti(L, ListIdx, Index);
            if (!lua_istable(L, -1))
            {
                lua_pop(L, 1);
                continue;
            }

            const int32 EntryIdx = lua_absindex(L, -1);
            const FString Name = GetStringField(L, EntryIdx, "Name");
            const FString TypeName = GetStringField(L, EntryIdx, "Type");
            const FString ConditionText = GetStringField(L, EntryIdx, "Condition");

            FLuaRepDecl Decl;
            if (Name.IsEmpty() || !ParseLuaNetParamType(TypeName, Decl.Type))
            {
                UE_LOG(LogUnLuaNova, Warning,
                    TEXT("Replicated property #%d on '%s' needs both Name and a known Type; skipping."),
                    Index, *Class->GetName());
                lua_pop(L, 1);
                continue;
            }

            Decl.Name = FName(*Name);
            Decl.Condition = COND_None;
            if (!ConditionText.IsEmpty() && !ParseLifetimeCondition(ConditionText, Decl.Condition))
            {
                UE_LOG(LogUnLuaNova, Warning, TEXT("Unknown replication condition '%s' for '%s'; using COND_None."),
                    *ConditionText, *Name);
                Decl.Condition = COND_None;
            }

            Decls.Add(Decl);
            lua_pop(L, 1); // the entry
        }

        lua_pop(L, 1); // the list

        if (Decls.Num() > 0)
            RepProps.Add(Class, MoveTemp(Decls));
    }

    const TArray<FLuaRepDecl>& FUnLuaNovaNet::GetReplicatedProps(const UClass* Class)
    {
        static const TArray<FLuaRepDecl> Empty;
        if (!Class)
            return Empty;

        if (const auto* Found = RepProps.Find(Class))
            return *Found;

        return Empty;
    }

    void FUnLuaNovaNet::Cleanup()
    {
        RepProps.Empty();
    }
}

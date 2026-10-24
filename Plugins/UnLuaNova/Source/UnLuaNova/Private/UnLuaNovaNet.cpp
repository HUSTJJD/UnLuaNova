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
#include <initializer_list>

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
        bool HasOnlyFields(lua_State* L, int32 TableIdx, std::initializer_list<const char*> Allowed)
        {
            const int32 Abs = lua_absindex(L, TableIdx);
            bool bValid = true;
            lua_pushnil(L);
            while (lua_next(L, Abs) != 0)
            {
                bool bKnown = false;
                if (lua_type(L, -2) == LUA_TSTRING)
                {
                    size_t Length = 0;
                    const char* Key = lua_tolstring(L, -2, &Length);
                    for (const char* Field : Allowed)
                        bKnown |= Length == FCStringAnsi::Strlen(Field) && FMemory::Memcmp(Key, Field, Length) == 0;
                }
                bValid &= bKnown;
                lua_pop(L, 1);
            }
            return bValid;
        }

        int32 DenseArrayCount(lua_State* L, int32 TableIdx)
        {
            const int32 Abs = lua_absindex(L, TableIdx);
            // lua_objlen() is size_t. Narrow it once here so every comparison below stays
            // signed - MSVC treats a signed/unsigned mismatch as an error, not a warning.
            const auto RawLength = lua_objlen(L, Abs);
            if (RawLength > MAX_int32)
                return INDEX_NONE;
            const int64 Length = (int64)RawLength;
            int32 Seen = 0;
            bool bValid = true;
            lua_pushnil(L);
            while (lua_next(L, Abs) != 0)
            {
                int64 Key;
                bValid &= UnLuaNovaTryToInt64(L, -2, Key) && Key >= 1 && Key <= Length;
                ++Seen;
                lua_pop(L, 1);
            }
            return bValid && Seen == Length ? Seen : INDEX_NONE;
        }

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
        bool ParseParams(lua_State* L, int32 TableIdx, FLuaRpcDecl& Decl)
        {
            const int32 Type = lua_getfield(L, TableIdx, "Params");
            if (Type != LUA_TTABLE)
            {
                lua_pop(L, 1);
                return Type == LUA_TNIL;
            }

            const int32 ParamsIdx = lua_absindex(L, -1);
            const int32 Count = DenseArrayCount(L, ParamsIdx);
            if (Count == INDEX_NONE)
            {
                lua_pop(L, 1);
                return false;
            }
            TSet<FName> Names;

            for (int32 Index = 1; Index <= Count; ++Index)
            {
                lua_rawgeti(L, ParamsIdx, Index);

                FLuaRpcParam Param;
                bool bOk = false;

                if (lua_type(L, -1) == LUA_TSTRING)
                {
                    bOk = ParseLuaNetParamType(UTF8_TO_TCHAR(lua_tostring(L, -1)), Param.Type);
                }
                else if (lua_istable(L, -1))
                {
                    const int32 EntryIdx = lua_absindex(L, -1);
                    const FString TypeName = GetStringField(L, EntryIdx, "Type");
                    const FString NameText = GetStringField(L, EntryIdx, "Name");
                    const int32 NameType = lua_getfield(L, EntryIdx, "Name");
                    lua_pop(L, 1);
                    if (!TypeName.IsEmpty() && HasOnlyFields(L, EntryIdx, { "Name", "Type" })
                        && (NameType == LUA_TNIL || (NameType == LUA_TSTRING && !NameText.IsEmpty() && !FName(*NameText).IsNone())))
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
                    bOk = !Names.Contains(Param.Name);
                }
                if (!bOk)
                {
                    lua_pop(L, 2);
                    return false;
                }
                Names.Add(Param.Name);
                Decl.Params.Add(Param);

                lua_pop(L, 1); // the entry
            }

            lua_pop(L, 1); // the Params table
            return true;
        }

        bool ParseRpcDecl(lua_State* L, int32 ValueIdx, FLuaRpcDecl& Decl)
        {
            const int32 Abs = lua_absindex(L, ValueIdx);

            // `Foo = true` declares it with default flags - i.e. unreliable, matching
            // UFUNCTION(Server) in C++. Reliability is opt-in through the table form.
            if (!lua_istable(L, Abs))
                return lua_type(L, Abs) == LUA_TBOOLEAN && lua_toboolean(L, Abs);
            if (!HasOnlyFields(L, Abs, { "Reliable", "Params" }))
                return false;

            const int32 Type = lua_getfield(L, Abs, "Reliable");
            Decl.bReliable = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            return (Type == LUA_TNIL || Type == LUA_TBOOLEAN) && ParseParams(L, Abs, Decl);
        }

        /**
         * A property is an FField, not a UObject, so it is allocated with new rather than
         * NewObject<T>() (which goes through StaticConstructObject and cannot make one).
         * Lifetime is the owner's: the UFunction it is created under holds it, and that is
         * also what makes it visible to the garbage collector.
         */
        FProperty* CreateStructParam(UFunction* Outer, FName Name, const TCHAR* StructPath)
        {
            // FindObject by path rather than TBaseStructure<FVector>::Get(): no header to
            // guess at, and it cannot drift when the struct moves packages.
            UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, StructPath);
            if (!Struct)
                return nullptr;

            auto* Property = new FStructProperty(Outer, Name);
            Property->Struct = Struct;
            return Property;
        }

        FProperty* CreateParam(UFunction* Outer, const FLuaRpcParam& Param)
        {
            switch (Param.Type)
            {
            case ELuaNetParamType::Bool:
            {
                auto* Property = new FBoolProperty(Outer, Param.Name);
                // Without this the element size stays at whatever the default ctor left,
                // and ParmsSize - hence the whole call frame - comes out wrong.
                Property->SetBoolSize(sizeof(bool), true);
                return Property;
            }
            case ELuaNetParamType::Int:       return new FIntProperty(Outer, Param.Name);
            case ELuaNetParamType::Int64:     return new FInt64Property(Outer, Param.Name);
            case ELuaNetParamType::Float:     return new FFloatProperty(Outer, Param.Name);
            case ELuaNetParamType::Double:    return new FDoubleProperty(Outer, Param.Name);
            case ELuaNetParamType::String:    return new FStrProperty(Outer, Param.Name);
            case ELuaNetParamType::Name:      return new FNameProperty(Outer, Param.Name);
            case ELuaNetParamType::Vector:    return CreateStructParam(Outer, Param.Name, TEXT("/Script/CoreUObject.Vector"));
            case ELuaNetParamType::Rotator:   return CreateStructParam(Outer, Param.Name, TEXT("/Script/CoreUObject.Rotator"));
            case ELuaNetParamType::Transform: return CreateStructParam(Outer, Param.Name, TEXT("/Script/CoreUObject.Transform"));
            case ELuaNetParamType::Object:
            {
                auto* Property = new FObjectProperty(Outer, Param.Name);
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
                    UE_LOG(LogUnLuaNova, Error, TEXT("Could not build parameter '%s' of RPC '%s'; rejecting RPC."),
                        *Param.Name.ToString(), *Decl.Name.ToString());
                    return nullptr;
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
                    if (Decl.Name.IsNone() || !ParseRpcDecl(L, -1, Decl))
                    {
                        UE_LOG(LogUnLuaNova, Error, TEXT("Invalid RPC declaration '%s'; rejecting RPC without changing its signature."), *Name);
                        lua_pop(L, 1);
                        continue;
                    }

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
                        if (!Template)
                        {
                            lua_pop(L, 1);
                            continue;
                        }
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

    bool FUnLuaNovaNet::DeclareReplicatedProps(lua_State* L, int32 ModuleTableIdx, UClass* Class)
    {
        if (!L || !Class)
            return false;

        const int32 Top = lua_gettop(L);
        const int32 ModuleIdx = lua_absindex(L, ModuleTableIdx);
        auto Reject = [&](const TCHAR* Reason)
        {
            lua_settop(L, Top);
            UE_LOG(LogUnLuaNova, Error, TEXT("Rejected replication declaration on '%s': %s"), *Class->GetName(), Reason);
            return false;
        };

        const int32 GetterType = lua_getfield(L, ModuleIdx, "GetLifetimeReplicatedProps");
        if (GetterType == LUA_TFUNCTION)
        {
            lua_pushvalue(L, ModuleIdx); // self
            if (lua_pcall(L, 1, 1, 0) != LUA_OK || !lua_istable(L, -1))
                return Reject(TEXT("GetLifetimeReplicatedProps must return an array without raising an error."));
        }
        else if (GetterType == LUA_TNIL)
        {
            lua_pop(L, 1);
            const int32 ListType = lua_getfield(L, ModuleIdx, "ReplicatedProps");
            if (ListType == LUA_TNIL)
            {
                lua_pop(L, 1);
                lua_newtable(L);
            }
            else if (ListType != LUA_TTABLE)
                return Reject(TEXT("ReplicatedProps must be an array."));
        }
        else
            return Reject(TEXT("GetLifetimeReplicatedProps must be a function."));

        const int32 ListIdx = lua_absindex(L, -1);
        TArray<FLuaRepDecl> Decls;
        TSet<FName> Names;
        const int32 Count = DenseArrayCount(L, ListIdx);
        if (Count == INDEX_NONE)
            return Reject(TEXT("Property list must be a dense array starting at 1."));
        for (int32 Index = 1; Index <= Count; ++Index)
        {
            lua_rawgeti(L, ListIdx, Index);
            if (!lua_istable(L, -1))
                return Reject(TEXT("Every property must be a table."));

            const int32 EntryIdx = lua_absindex(L, -1);
            if (!HasOnlyFields(L, EntryIdx, { "Name", "Type", "Condition" }))
                return Reject(TEXT("Unknown property declaration field."));
            const FString Name = GetStringField(L, EntryIdx, "Name");
            const FString TypeName = GetStringField(L, EntryIdx, "Type");
            const FString ConditionText = GetStringField(L, EntryIdx, "Condition");
            const int32 ConditionType = lua_getfield(L, EntryIdx, "Condition");
            lua_pop(L, 1);

            FLuaRepDecl Decl;
            if (Name.IsEmpty() || !ParseLuaNetParamType(TypeName, Decl.Type))
                return Reject(TEXT("Every property requires a nonempty Name and known Type."));

            Decl.Name = FName(*Name);
            if (Decl.Name.IsNone() || Names.Contains(Decl.Name))
                return Reject(TEXT("Property names must be unique, non-None FNames."));
            Names.Add(Decl.Name);
            Decl.Condition = COND_None;
            if (ConditionType != LUA_TNIL && (ConditionType != LUA_TSTRING
                || !ParseLifetimeCondition(ConditionText, Decl.Condition)))
                return Reject(TEXT("Unknown or malformed replication condition."));
            if (Decl.Condition != COND_None)
                return Reject(TEXT("Conditional fields require a stable class layout; only None is currently supported."));

            Decls.Add(Decl);
            lua_pop(L, 1); // the entry
        }

        lua_pop(L, 1); // the list

        // Existing components already use this layout. Rebinding cannot mutate it.
        if (const auto* Existing = RepProps.Find(Class))
        {
            if (Existing->Num() != Decls.Num())
                return Reject(TEXT("Class schema is immutable until network teardown."));
            for (int32 Index = 0; Index < Decls.Num(); ++Index)
                if ((*Existing)[Index].Name != Decls[Index].Name || (*Existing)[Index].Type != Decls[Index].Type
                    || (*Existing)[Index].Condition != Decls[Index].Condition)
                    return Reject(TEXT("Class schema is immutable until network teardown."));
        }
        else
            RepProps.Add(Class, MoveTemp(Decls));
        return true;
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

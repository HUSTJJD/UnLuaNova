// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations
// under the License.

#include "UnLuaNovaNetPolicy.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaCompatibility.h"
#include "UnLuaNovaSettings.h"
#include "UnLuaNovaReplicationComponent.h"

#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

namespace UnLuaNova
{
    const TCHAR* LexToString(ELuaNetPolicyResult Result)
    {
        switch (Result)
        {
        case ELuaNetPolicyResult::Ok:                   return TEXT("Ok");
        case ELuaNetPolicyResult::FeatureDisabled:      return TEXT("FeatureDisabled");
        case ELuaNetPolicyResult::DeclarationDenied:    return TEXT("DeclarationDenied");
        case ELuaNetPolicyResult::InvocationDenied:     return TEXT("InvocationDenied");
        case ELuaNetPolicyResult::SchemaIncompatible:   return TEXT("SchemaIncompatible");
        case ELuaNetPolicyResult::TravelInProgress:     return TEXT("TravelInProgress");
        case ELuaNetPolicyResult::NotOwner:             return TEXT("NotOwner");
        case ELuaNetPolicyResult::Unauthorized:         return TEXT("Unauthorized");
        default:                                        return TEXT("Unknown");
        }
    }

    FUnLuaNovaNetPolicy& FUnLuaNovaNetPolicy::Get()
    {
        static FUnLuaNovaNetPolicy Instance;
        return Instance;
    }

    void FUnLuaNovaNetPolicy::Initialize()
    {
        if (bInitialized)
        {
            return;
        }
        bInitialized = true;
        bTravelInProgress = false;
        Generation = 1;
    }

    void FUnLuaNovaNetPolicy::Shutdown()
    {
        Schemas.Reset();
        PendingSchemas.Reset();
        AuthorizationHandler = FAuthorizationHandler();
        DeniedLogger = FDeniedLogger();
        bHasAuthorizationHandler = false;
        bHasDeniedLogger = false;
        bTravelInProgress = false;
        bInitialized = false;
    }

    bool FUnLuaNovaNetPolicy::IsDeclaredNetworkingEnabled() const
    {
        const UUnLuaNovaSettings* Settings = GetDefault<UUnLuaNovaSettings>();
        return Settings != nullptr && Settings->bDeclaredNetworking;
    }

    bool FUnLuaNovaNetPolicy::IsAuthorizationRequired() const
    {
        const UUnLuaNovaSettings* Settings = GetDefault<UUnLuaNovaSettings>();
        return Settings != nullptr && Settings->bRequireRpcAuthorization;
    }

    bool FUnLuaNovaNetPolicy::IsHotReloadSchemaChangeAllowed() const
    {
        const UUnLuaNovaSettings* Settings = GetDefault<UUnLuaNovaSettings>();
        return Settings != nullptr && Settings->bAllowHotReloadSchemaChange;
    }

    bool FUnLuaNovaNetPolicy::IsSeamlessTravelSchemaPreserved() const
    {
        const UUnLuaNovaSettings* Settings = GetDefault<UUnLuaNovaSettings>();
        return Settings != nullptr && Settings->bPreserveSchemaAcrossSeamlessTravel;
    }

    FString FUnLuaNovaNetPolicy::ObjectPath(const UObject* Object)
    {
        return Object != nullptr ? Object->GetPathName() : FString(TEXT("<none>"));
    }

    void FUnLuaNovaNetPolicy::Deny(const FLuaNetPolicyEvent& Event) const
    {
        if (bHasDeniedLogger && DeniedLogger)
        {
            DeniedLogger(Event);
            return;
        }
        // A denied declaration / RPC / schema is a policy decision that was handled, not
        // an engine failure. Logging it at Error makes UE's automation framework mark
        // every spec that exercises the rejection path as failed (it counts Error logs
        // emitted during a test). Warning keeps it visible without corrupting the result.
        UNLUANOVA_LOGF(LogUnLuaNova, Warning,
            TEXT("UnLuaNova.Net policy denied: result=%s reason='%s' object='%s' rpc='%s' "
                 "schema local=0x%08X remote=0x%08X generation=%u"),
            LexToString(Event.Result), *Event.Reason, *Event.ObjectPath,
            *Event.RpcName.ToString(), Event.LocalSchema, Event.RemoteSchema,
            Event.Generation);
    }

    ELuaNetPolicyResult FUnLuaNovaNetPolicy::CanDeclare(
        UClass* Class, const TArray<FLuaRpcDecl>& Rpcs,
        const TArray<FLuaRepDecl>& Fields, FString& OutReason) const
    {
        if (!IsDeclaredNetworkingEnabled())
        {
            OutReason = TEXT("bDeclaredNetworking is false; no dynamic UFunction will be constructed");
            FLuaNetPolicyEvent Event;
            Event.Result = ELuaNetPolicyResult::FeatureDisabled;
            Event.Reason = OutReason;
            Event.ObjectPath = ObjectPath(Class);
            Event.Generation = Generation;
            Deny(Event);
            return Event.Result;
        }
        if (Class == nullptr)
        {
            OutReason = TEXT("cannot declare networking on a null class");
            FLuaNetPolicyEvent Event;
            Event.Result = ELuaNetPolicyResult::DeclarationDenied;
            Event.Reason = OutReason;
            Event.Generation = Generation;
            Deny(Event);
            return Event.Result;
        }
        if (bTravelInProgress)
        {
            OutReason = TEXT("a seamless travel is in progress; declarations are deferred until CompleteSeamlessTravel()");
            FLuaNetPolicyEvent Event;
            Event.Result = ELuaNetPolicyResult::TravelInProgress;
            Event.Reason = OutReason;
            Event.ObjectPath = ObjectPath(Class);
            Event.Generation = Generation;
            Deny(Event);
            return Event.Result;
        }
        if (Rpcs.Num() == 0 && Fields.Num() == 0)
        {
            OutReason.Empty();
            return ELuaNetPolicyResult::Ok;
        }
        // Fail closed: requiring authorization without a policy is a configuration
        // error, not a permissive default.
        if (IsAuthorizationRequired() && !bHasAuthorizationHandler)
        {
            OutReason = TEXT("bRequireRpcAuthorization is enabled but no authorization handler is installed");
            FLuaNetPolicyEvent Event;
            Event.Result = ELuaNetPolicyResult::Unauthorized;
            Event.Reason = OutReason;
            Event.ObjectPath = ObjectPath(Class);
            Event.Generation = Generation;
            Deny(Event);
            return Event.Result;
        }
        OutReason.Empty();
        return ELuaNetPolicyResult::Ok;
    }

    void FUnLuaNovaNetPolicy::SetAuthorizationHandler(FAuthorizationHandler Handler)
    {
        AuthorizationHandler = MoveTemp(Handler);
        bHasAuthorizationHandler = true;
    }

    void FUnLuaNovaNetPolicy::ClearAuthorizationHandler()
    {
        AuthorizationHandler = FAuthorizationHandler();
        bHasAuthorizationHandler = false;
    }

    ELuaNetPolicyResult FUnLuaNovaNetPolicy::CanInvoke(
        const UObject* Target, const FName& RpcName, ELuaRpcKind Kind,
        FString& OutReason) const
    {
        if (!IsDeclaredNetworkingEnabled())
        {
            OutReason = TEXT("declared networking is disabled");
            return ELuaNetPolicyResult::FeatureDisabled;
        }
        // The engine's own UNetDriver::ShouldCallRemoteFunction owner check is always
        // in force. This policy is an additional, opt-in application-level check.
        if (!IsAuthorizationRequired())
        {
            OutReason.Empty();
            return ELuaNetPolicyResult::Ok;
        }
        if (!bHasAuthorizationHandler)
        {
            OutReason = TEXT("authorization required but no handler installed");
            FLuaNetPolicyEvent Event;
            Event.Result = ELuaNetPolicyResult::Unauthorized;
            Event.Reason = OutReason;
            Event.RpcName = RpcName;
            Event.ObjectPath = ObjectPath(Target);
            Event.Generation = Generation;
            Deny(Event);
            return Event.Result;
        }
        const ELuaNetPolicyResult Result = AuthorizationHandler(Target, RpcName, Kind);
        if (Result != ELuaNetPolicyResult::Ok)
        {
            OutReason = FString::Printf(TEXT("RPC '%s' (%d) refused by the installed authorization handler: %s"),
                *RpcName.ToString(), static_cast<int32>(Kind), LexToString(Result));
            FLuaNetPolicyEvent Event;
            Event.Result = Result;
            Event.Reason = OutReason;
            Event.RpcName = RpcName;
            Event.ObjectPath = ObjectPath(Target);
            Event.Generation = Generation;
            Deny(Event);
            return Result;
        }
        OutReason.Empty();
        return ELuaNetPolicyResult::Ok;
    }

    ELuaNetPolicyResult FUnLuaNovaNetPolicy::NegotiateSchema(
        const UObject* Owner, uint32 LocalSchema, uint32 RemoteSchema, FString& OutReason)
    {
        if (LocalSchema == RemoteSchema)
        {
            OutReason.Empty();
            return ELuaNetPolicyResult::Ok;
        }
        UClass* Class = Owner != nullptr ? Owner->GetClass() : nullptr;
        if (Class != nullptr)
        {
            const TWeakObjectPtr<UClass> Key(Class);
            if (const uint32* Pending = PendingSchemas.Find(Key))
            {
                if (*Pending == LocalSchema || *Pending == RemoteSchema)
                {
                    PendingSchemas.Remove(Key);
                    RegisterSchema(Class, LocalSchema);
                    ++Generation;
                    OutReason = FString::Printf(
                        TEXT("accepted approved schema migration 0x%08X -> 0x%08X"),
                        RemoteSchema, LocalSchema);
                    return ELuaNetPolicyResult::Ok;
                }
            }
        }
        OutReason = FString::Printf(
            TEXT("peer declaration fingerprint 0x%08X does not match local 0x%08X and no hot-reload migration was approved; buckets will not be applied"),
            RemoteSchema, LocalSchema);
        FLuaNetPolicyEvent Event;
        Event.Result = ELuaNetPolicyResult::SchemaIncompatible;
        Event.Reason = OutReason;
        Event.ObjectPath = ObjectPath(Owner);
        Event.LocalSchema = LocalSchema;
        Event.RemoteSchema = RemoteSchema;
        Event.Generation = Generation;
        Deny(Event);
        return Event.Result;
    }

    ELuaNetPolicyResult FUnLuaNovaNetPolicy::NegotiateRpcLayout(
        const UObject* Owner, uint32 LocalRpc, uint32 RemoteRpc, FString& OutReason)
    {
        if (LocalRpc == RemoteRpc)
        {
            OutReason.Empty();
            return ELuaNetPolicyResult::Ok;
        }
        UClass* Class = Owner != nullptr ? Owner->GetClass() : nullptr;
        if (IsHotReloadSchemaChangeAllowed() && HasPendingMigration(Class))
        {
            OutReason = FString::Printf(
                TEXT("accepted approved RPC layout migration 0x%08X -> 0x%08X"),
                RemoteRpc, LocalRpc);
            return ELuaNetPolicyResult::Ok;
        }
        OutReason = FString::Printf(
            TEXT("peer RPC layout 0x%08X differs from local 0x%08X; RPC declarations are not compatible"),
            RemoteRpc, LocalRpc);
        FLuaNetPolicyEvent Event;
        Event.Result = ELuaNetPolicyResult::SchemaIncompatible;
        Event.Reason = OutReason;
        Event.ObjectPath = ObjectPath(Owner);
        Event.LocalSchema = LocalRpc;
        Event.RemoteSchema = RemoteRpc;
        Event.Generation = Generation;
        Deny(Event);
        return Event.Result;
    }

    void FUnLuaNovaNetPolicy::RegisterSchema(UClass* Class, uint32 Schema)
    {
        if (Class != nullptr)
        {
            Schemas.Add(TWeakObjectPtr<UClass>(Class), Schema);
        }
    }

    uint32 FUnLuaNovaNetPolicy::GetSchema(UClass* Class) const
    {
        if (Class == nullptr)
        {
            return 0;
        }
        if (const uint32* Found = Schemas.Find(TWeakObjectPtr<UClass>(Class)))
        {
            return *Found;
        }
        return 0;
    }

    ELuaNetPolicyResult FUnLuaNovaNetPolicy::OnHotReload(
        UClass* Class, uint32 NewSchema, FString& OutReason)
    {
        if (bTravelInProgress)
        {
            OutReason = TEXT("cannot migrate a schema during a seamless travel");
            return ELuaNetPolicyResult::TravelInProgress;
        }
        if (!IsHotReloadSchemaChangeAllowed())
        {
            OutReason = TEXT("the reload changes the replicated layout; set bAllowHotReloadSchemaChange=True to stage a migration, otherwise rebuild the network environment");
            FLuaNetPolicyEvent Event;
            Event.Result = ELuaNetPolicyResult::SchemaIncompatible;
            Event.Reason = OutReason;
            Event.ObjectPath = ObjectPath(Class);
            Event.LocalSchema = GetSchema(Class);
            Event.RemoteSchema = NewSchema;
            Event.Generation = Generation;
            Deny(Event);
            return Event.Result;
        }
        if (Class != nullptr)
        {
            PendingSchemas.Add(TWeakObjectPtr<UClass>(Class), NewSchema);
        }
        OutReason = FString::Printf(
            TEXT("staged schema migration to 0x%08X; peers must negotiate it before buckets are applied"),
            NewSchema);
        return ELuaNetPolicyResult::Ok;
    }

    void FUnLuaNovaNetPolicy::ApproveHotReloadMigration(UClass* Class, uint32 NewSchema)
    {
        if (Class == nullptr)
        {
            return;
        }
        const TWeakObjectPtr<UClass> Key(Class);
        PendingSchemas.Remove(Key);
        RegisterSchema(Class, NewSchema);
        ++Generation;
    }

    bool FUnLuaNovaNetPolicy::HasPendingMigration(UClass* Class) const
    {
        return Class != nullptr && PendingSchemas.Contains(TWeakObjectPtr<UClass>(Class));
    }

    void FUnLuaNovaNetPolicy::NotifyModulesReloaded()
    {
        ++Generation;
        // A reload invalidates every unapproved pending migration: the script may have
        // changed again, so an older staged layout must never be applied silently.
        PendingSchemas.Reset();
        // Rebuild every live replication component so a module bound after BeginPlay
        // or a declaration changed by the reload is picked up. This is what removes
        // the "schema frozen at BeginPlay" limitation.
        UUnLuaNovaReplicationComponent::RefreshAllDeclarations();
        UNLUANOVA_LOGF(LogUnLuaNova, Log,
            TEXT("UnLuaNova.Net: modules reloaded, generation=%u; declarations re-parsed"),
            Generation);
    }

    void FUnLuaNovaNetPolicy::OnSeamlessTravel(const UObject* Context)
    {
        bTravelInProgress = true;
        ++Generation;
        if (!IsSeamlessTravelSchemaPreserved())
        {
            Schemas.Reset();
            PendingSchemas.Reset();
        }
        else
        {
            // UClasses are process-wide and survive a seamless travel; only stale
            // weak references are dropped. RPC IDs and field indices are recomputed
            // against the pinned schemas in the new world.
            for (auto It = Schemas.CreateIterator(); It; ++It)
            {
                if (!It.Key().IsValid())
                {
                    It.RemoveCurrent();
                }
            }
        }
        UNLUANOVA_LOGF(LogUnLuaNova, Log,
            TEXT("UnLuaNova.Net: seamless travel started (context='%s', generation=%u, schemas pinned=%s)"),
            *ObjectPath(Context), Generation,
            IsSeamlessTravelSchemaPreserved() ? TEXT("true") : TEXT("false"));
    }

    void FUnLuaNovaNetPolicy::CompleteSeamlessTravel()
    {
        if (!bTravelInProgress)
        {
            return;
        }
        bTravelInProgress = false;
        // The new world needs its own declaration parse: RPC IDs and field indices
        // computed for the old world must not be reused.
        UUnLuaNovaReplicationComponent::RefreshAllDeclarations();
        UNLUANOVA_LOGF(LogUnLuaNova, Log,
            TEXT("UnLuaNova.Net: seamless travel completed (generation=%u, %d pinned schema(s))"),
            Generation, Schemas.Num());
    }

    void FUnLuaNovaNetPolicy::SetDeniedLogger(FDeniedLogger Logger)
    {
        DeniedLogger = MoveTemp(Logger);
        bHasDeniedLogger = true;
    }

    void FUnLuaNovaNetPolicy::ClearDeniedLogger()
    {
        DeniedLogger = FDeniedLogger();
        bHasDeniedLogger = false;
    }
}

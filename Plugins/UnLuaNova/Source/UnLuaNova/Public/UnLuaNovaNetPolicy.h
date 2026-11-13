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

#pragma once

// Stable API. Lua-declared networking is a supported feature; the three capabilities
// that used to be out of scope (authorization, hot-reload schema migration and
// seamless travel) are implemented here as one policy layer. See
// Docs/CN/Release_Readiness.md for the promotion criteria and
// Docs/CN/API_Stability.md for the stability/deprecation contract.
//
// Deliberately no UCLASS/USTRUCT/UENUM: this layer needs no reflection, no generated
// header and no blueprint surface. That keeps it compilable on every carrier
// (Editor / Game / Server / cooked) without UHT involvement.

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UnLuaNovaNetDecl.h"

class UObject;

namespace UnLuaNova
{
    /** Explicit, loggable outcome of every policy decision. */
    enum class ELuaNetPolicyResult : uint8
    {
        Ok,
        /** bDeclaredNetworking is false - no dynamic UFunction may be constructed. */
        FeatureDisabled,
        /** A declaration was refused before any UFunction was built. */
        DeclarationDenied,
        /** An RPC invocation was refused by the authorization policy. */
        InvocationDenied,
        /** Two peers disagree on the replicated layout and no migration was approved. */
        SchemaIncompatible,
        /** A seamless travel is in progress; re-declaration is deferred. */
        TravelInProgress,
        /** The caller is not the owner of the target. */
        NotOwner,
        /** Authorization is required but no handler was installed (fail-closed). */
        Unauthorized,
    };

    UNLUANOVA_API const TCHAR* LexToString(ELuaNetPolicyResult Result);

    /** One structured diagnostic, handed to the host logger on every denial. */
    struct FLuaNetPolicyEvent
    {
        ELuaNetPolicyResult Result = ELuaNetPolicyResult::Ok;
        FString Reason;
        FName RpcName;
        FString ObjectPath;
        uint32 LocalSchema = 0;
        uint32 RemoteSchema = 0;
        uint32 Generation = 0;
    };

    /**
     * Central policy layer for Lua-declared networking.
     *
     * Everything here is pure state + decisions; it does not construct UFunctions,
     * replicate properties or register engine delegates. Callers stay in the existing
     * FUnLuaNovaNet / UUnLuaNovaReplicationComponent paths and ask this layer before
     * acting. That keeps the three formerly out-of-scope capabilities testable without
     * a network session and avoids introducing an unverified engine delegate signature.
     */
    class UNLUANOVA_API FUnLuaNovaNetPolicy
    {
    public:
        static FUnLuaNovaNetPolicy& Get();

        void Initialize();
        void Shutdown();
        bool IsInitialized() const { return bInitialized; }

        // ---- Configuration (UUnLuaNovaSettings) --------------------------------
        bool IsDeclaredNetworkingEnabled() const;
        bool IsAuthorizationRequired() const;
        bool IsHotReloadSchemaChangeAllowed() const;
        bool IsSeamlessTravelSchemaPreserved() const;

        // ---- Declaration authorization ---------------------------------------
        /** Called once before FUnLuaNovaNet::DeclareRPCs builds any UFunction. */
        ELuaNetPolicyResult CanDeclare(UClass* Class, const TArray<FLuaRpcDecl>& Rpcs,
                                       const TArray<FLuaRepDecl>& Fields, FString& OutReason) const;

        // ---- Invocation authorization ----------------------------------------
        /** Host-installed policy; when required and absent the answer is Unauthorized. */
        typedef TFunction<ELuaNetPolicyResult(const UObject*, const FName&, ELuaRpcKind)> FAuthorizationHandler;
        void SetAuthorizationHandler(FAuthorizationHandler Handler);
        void ClearAuthorizationHandler();
        ELuaNetPolicyResult CanInvoke(const UObject* Target, const FName& RpcName,
                                      ELuaRpcKind Kind, FString& OutReason) const;

        // ---- Schema negotiation and migration --------------------------------
        /**
         * Compare the local declaration fingerprint with a peer's. A mismatch is
         * accepted only when an explicit migration was approved by OnHotReload or
         * ApproveHotReloadMigration for the same generation.
         */
        ELuaNetPolicyResult NegotiateSchema(const UObject* Owner, uint32 LocalSchema,
                                            uint32 RemoteSchema, FString& OutReason);
        /**
         * Same contract for the RPC layout fingerprint. An approved hot-reload
         * migration may accept one mismatch; otherwise the peer is rejected with an
         * explicit reason instead of calling into the wrong RPC signature.
         */
        ELuaNetPolicyResult NegotiateRpcLayout(const UObject* Owner, uint32 LocalRpc,
                                               uint32 RemoteRpc, FString& OutReason);
        void RegisterSchema(UClass* Class, uint32 Schema);
        uint32 GetSchema(UClass* Class) const;

        // ---- Hot reload -------------------------------------------------------
        /**
         * A module reload may change the replicated layout. With
         * bAllowHotReloadSchemaChange the new layout is staged as a pending migration
         * and peers must negotiate it before any bucket is applied; otherwise the
         * reload is refused with a diagnostic instead of silently mis-decoding.
         */
        ELuaNetPolicyResult OnHotReload(UClass* Class, uint32 NewSchema, FString& OutReason);
        void ApproveHotReloadMigration(UClass* Class, uint32 NewSchema);
        bool HasPendingMigration(UClass* Class) const;
        /**
         * Called by FLuaEnv::HotReload() after the script reload completes. Advances the
         * generation so any RPC ID or field index computed before the reload is invalid,
         * and records that the next declaration must be re-negotiated.
         */
        void NotifyModulesReloaded();
        bool WasReloadedSince(uint32 ObservedGeneration) const { return Generation != ObservedGeneration; }

        // ---- Seamless travel --------------------------------------------------
        /**
         * Call at the start of a seamless travel and again on completion. Registered
         * schemas are pinned across the travel and the generation counter advances, so
         * an RPC ID or field index computed for the old world cannot be reused for the
         * new one without an explicit re-declaration.
         */
        void OnSeamlessTravel(const UObject* Context);
        void CompleteSeamlessTravel();
        bool IsTravelInProgress() const { return bTravelInProgress; }
        uint32 CurrentGeneration() const { return Generation; }

        // ---- Diagnostics ------------------------------------------------------
        typedef TFunction<void(const FLuaNetPolicyEvent&)> FDeniedLogger;
        void SetDeniedLogger(FDeniedLogger Logger);
        void ClearDeniedLogger();

    private:
        void Deny(const FLuaNetPolicyEvent& Event) const;
        static FString ObjectPath(const UObject* Object);

        FAuthorizationHandler AuthorizationHandler;
        FDeniedLogger DeniedLogger;
        /** Explicit flags: do not rely on TFunction's bool conversion across UE versions. */
        bool bHasAuthorizationHandler = false;
        bool bHasDeniedLogger = false;

        /** Live layout per class, pinned across a seamless travel. */
        TMap<TWeakObjectPtr<UClass>, uint32> Schemas;
        /** Layouts approved by a hot reload but not yet negotiated by every peer. */
        TMap<TWeakObjectPtr<UClass>, uint32> PendingSchemas;
        /** Advances on every accepted schema migration and every seamless travel. */
        uint32 Generation = 1;
        bool bInitialized = false;
        bool bTravelInProgress = false;
    };
}

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

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineBaseTypes.h"
#include "UnLuaNovaNetPayload.h"
#include "UnLuaNovaReplicationComponent.generated.h"

/**
 * Carries the replicated fields a Lua module declared.
 *
 * Why a component and not a base class: replication needs a real UPROPERTY(Replicated)
 * to hang off, and there is no way to conjure one onto an arbitrary UClass at runtime -
 * that is the one thing the override machinery cannot fake, because properties live in
 * the class's memory layout rather than in its function map. A component is the UE-idiomatic
 * way to add a capability to an actor, it works on a blueprint or a native class alike,
 * and it needs exactly one of them rather than one per actor flavour.
 *
 * Usage: add it to the actor (from a blueprint, from C++, or at runtime), declare the
 * fields in the Lua module, and the component does the rest:
 *
 *     function M:GetLifetimeReplicatedProps()
 *         return { { Name = "HP", Type = "Float" } }
 *     end
 *
 * Server pushes the Lua table into the payload every tick; clients get OnRep and push
 * the payload back into their own Lua table.
 */
UCLASS(ClassGroup = (UnLuaNova), meta = (BlueprintSpawnableComponent))
class UNLUANOVA_API UUnLuaNovaReplicationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnLuaNovaReplicationComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Replication condition for the payload as a whole.
     *
     * One condition for all fields is a real limitation, and it is structural: per-field
     * conditions would need one UPROPERTY per condition, which cannot be added at runtime.
     * It is applied rather than COND_None when the Lua declaration asks for one; when the
     * declarations disagree, the first non-None one wins and a warning is logged.
     */
    UPROPERTY(EditAnywhere, Category = "UnLuaNova")
    TEnumAsByte<ELifetimeCondition> Condition;

protected:
    UFUNCTION()
    void OnRep_Payload();

private:
    /** True when the owner's Lua module actually declared any replicated fields. */
    bool bDeclared = false;

    void PushToLua();
    void PullFromLua();

    UPROPERTY(ReplicatedUsing = OnRep_Payload)
    FLuaNetPayload Payload;
};

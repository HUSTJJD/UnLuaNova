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

#include "UnLuaNovaTestHelpers.h"
#include "UnLuaNovaEx.h"

#include "Net/UnrealNetwork.h"

BEGIN_EXPORT_CLASS(FUnLuaNovaTestLib)
ADD_STATIC_FUNCTION(TestForBaseSpec1)
ADD_STATIC_FUNCTION(TestForBaseSpec2)
ADD_SHARED_PTR_CONSTRUCTOR(ESPMode::NotThreadSafe)
END_EXPORT_CLASS()
IMPLEMENT_EXPORTED_CLASS(FUnLuaNovaTestLib)

BEGIN_EXPORT_ENUM_EX(EScopedEnum::Type, EScopedEnum)
ADD_SCOPED_ENUM_VALUE(Value1)
ADD_SCOPED_ENUM_VALUE(Value2)
END_EXPORT_ENUM(EScopedEnum)

BEGIN_EXPORT_ENUM(EEnumClass)
ADD_SCOPED_ENUM_VALUE(Value1)
ADD_SCOPED_ENUM_VALUE(Value2)
END_EXPORT_ENUM(EEnumClass)

BEGIN_EXPORT_REFLECTED_CLASS(UUnLuaNovaTestStub)
ADD_PROPERTY(ScopedEnum)
ADD_PROPERTY(EnumClass)
END_EXPORT_CLASS()
IMPLEMENT_EXPORTED_CLASS(UUnLuaNovaTestStub)

AUnLuaNovaTestNetActor::AUnLuaNovaTestNetActor()
{
    bReplicates = true;

    // Spawned at run time into an empty map, so there is no guarantee the client's view
    // point is anywhere near it. Without this the relevance check can leave the actor - and
    // everything the test wants to assert on - on the server only.
    bAlwaysRelevant = true;

    // Created unconditionally rather than only when the Lua module declares fields: the
    // component decides for itself at BeginPlay whether there is anything to do, and a
    // component added on demand would have to be added from inside a bind that may never
    // happen on the client.
    Replication = CreateDefaultSubobject<UUnLuaNovaReplicationComponent>(TEXT("LuaReplication"));
}

void AUnLuaNovaTestNetActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AUnLuaNovaTestNetActor, EngineReplicatedInt);
    DOREPLIFETIME(AUnLuaNovaTestNetActor, NetTestSessionId);
    DOREPLIFETIME(AUnLuaNovaTestNetActor, NetTestActorId);
}

int32 UUnLuaNovaTestStub::TestForIssue407(TArray<int32> Array)
{
    // Issue407: a TArray that Lua produced itself (TMap:Keys()/:Values()) handed back to
    // C++. The count is the whole assertion - 2 for the stub's MapForIssue407 - so the
    // body is deliberately trivial; what is under test is the marshalling, not this.
    return Array.Num();
}

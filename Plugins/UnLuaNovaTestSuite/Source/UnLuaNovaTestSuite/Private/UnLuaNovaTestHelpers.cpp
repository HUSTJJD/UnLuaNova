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

#include "Net/UnrealNetwork.h"

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
}

int32 UUnLuaNovaTestStub::TestForIssue407(TArray<int32> Array)
{
    // Issue407: a TArray that Lua produced itself (TMap:Keys()/:Values()) handed back to
    // C++. The count is the whole assertion - 2 for the stub's MapForIssue407 - so the
    // body is deliberately trivial; what is under test is the marshalling, not this.
    return Array.Num();
}

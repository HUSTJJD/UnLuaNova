// Tencent is pleased to support the open source community by making UnLuauNova available.
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

#include "UnLuauNovaBase.h"
#include "UnLuauNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuauNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuauNovaLibMulticastDelegateSpec, "UnLuauNova.API.FMulticastScriptDelegate", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
    UUnLuauNovaTestStub* Stub;
END_DEFINE_SPEC(FUnLuauNovaLibMulticastDelegateSpec)

void FUnLuauNovaLibMulticastDelegateSpec::Define()
{
    BeforeEach([this]
    {
        UnLuauNova::Startup();
        L = UnLuauNova::GetState();
        Stub = NewObject<UUnLuauNovaTestStub>();
        UnLuauNova::PushUObject(L, Stub);
        lua_setglobal(L, "Stub");
    });

    Describe(TEXT("Add"), [this]()
    {
        It(TEXT("添加绑定"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            Flag = false
            Stub.SimpleEvent:Add(Stub, function() Flag = true end)
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_TRUE(Stub->SimpleEvent.IsBound());
            Stub->SimpleEvent.Broadcast();
            lua_getglobal(L, "Flag");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("添加绑定：赋值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            Flag = false
            Stub.SimpleEvent = { Stub, function() Flag = true end }
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_TRUE(Stub->SimpleEvent.IsBound());
            Stub->SimpleEvent.Broadcast();
            lua_getglobal(L, "Flag");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("添加绑定：UFunction"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            Stub.SimpleEvent:Add(Stub, Stub.AddCount)
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_TRUE(Stub->SimpleEvent.IsBound());
            Stub->SimpleEvent.Broadcast();
            TEST_EQUAL(Stub->Counter, 1);
        });
    });

    Describe(TEXT("Remove"), [this]()
    {
        It(TEXT("移除绑定"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            local Callback = function() end
            Stub.SimpleEvent:Add(Stub, Callback)
            Stub.SimpleEvent:Remove(Stub, Callback)
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_FALSE(Stub->SimpleEvent.IsBound());
        });

        It(TEXT("移除绑定：UFunction"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            Stub.SimpleEvent:Add(Stub, Stub.AddCount)
            Stub.SimpleEvent:Remove(Stub, Stub.AddCount)
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_FALSE(Stub->SimpleEvent.IsBound());
        });
    });

    Describe(TEXT("Clear"), [this]()
    {
        It(TEXT("清理所有的绑定"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            Stub.SimpleEvent:Add(Stub, function() end)
            Stub.SimpleEvent:Clear()
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_FALSE(Stub->SimpleEvent.IsBound());
        });
    });

    Describe(TEXT("Broadcast"), [this]()
    {
        It(TEXT("广播事件"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            local Counter1 = 0
            local Counter2 = 0
            Stub.SimpleEvent:Add(Stub, function() Counter1 = Counter1 + 1 end)
            Stub.SimpleEvent:Add(Stub, function() Counter2 = Counter2 + 1 end)
            Stub.SimpleEvent:Broadcast()
            return Counter1, Counter2
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_EQUAL(lua_tointeger(L, -1), 1LL);
            TEST_EQUAL(lua_tointeger(L, -2), 1LL);
        });
    });

    AfterEach([this]
    {
        UnLuauNova::Shutdown();
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS

// Tencent is pleased to support the open source community by making UnLuaNova available.
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

// Covers the pre-resolved call path (FFunctionDesc::CallUEFast).
//
// It cannot be called directly - it is a private member of an unexported class - and it is not
// observable from outside either: it deliberately skips only the work the static binding makes
// unnecessary (callspace query, interface lookup, override redirection), and shares
// marshalling with the reflective path. So what is tested here is the contract that matters:
//
//   * the dispatch conditions actually hold for a plain, non-net, non-latent, non-interface
//     function (otherwise the fast path is dead code that no test reaches), and
//   * calling that function from Lua returns exactly what the reflective path returns.
//
// The two probes exist because FFunctionDesc captures whether a binding exists at
// construction time: one function cannot be driven through both paths in one process.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#include "StaticBinding/StaticBindingRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    FStaticParamBinding GFastPathParams[] = {
        {TEXT("In"), 0, EStaticPropKind::Int, nullptr},
    };

    // No function is listed for ProbeWithoutStaticBinding - that is the whole point.
    FStaticFuncBinding GFastPathFuncs[] = {
        {TEXT("ProbeWithStaticBinding"), 0, 0, 1, FStaticFuncFlags(), GFastPathParams, nullptr},
    };

    FStaticTypeBinding GFastPathType = {
        TEXT("UUnLuaNovaTestStub"), nullptr, 0,
        GFastPathFuncs, UE_ARRAY_COUNT(GFastPathFuncs), nullptr, false
    };
}

BEGIN_DEFINE_SPEC(FCallUEFastSpec, "UnLuaNova.API.CallUEFast", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FCallUEFastSpec)

void FCallUEFastSpec::Define()
{
    AfterEach([this]
    {
        FStaticBindingRegistry::Get().Unregister(&GFastPathType);
        Env.Reset();
        L = nullptr;
    });

    It(TEXT("分发条件：非网络 / 非 latent / 非接口函数会走预解析路径"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        GFastPathType.Class = UUnLuaNovaTestStub::StaticClass();
        FStaticBindingRegistry::Get().Register(&GFastPathType);

        const FStaticTypeBinding* Type = FStaticBindingRegistry::Get().Find(UUnLuaNovaTestStub::StaticClass());
        TEST_TRUE(Type != nullptr);
        if (!Type)
            return;

        const FStaticFuncBinding* Func = FStaticBindingRegistry::Get().FindFunc(Type, TEXT("ProbeWithStaticBinding"));
        TEST_TRUE(Func != nullptr);
        if (!Func)
            return;

        TEST_FALSE(Func->Flags.bIsNet);
        TEST_FALSE(Func->Flags.bLatent);
        TEST_FALSE(Func->Flags.bInterface);
        TEST_TRUE(Func->Function != nullptr);
    });

    It(TEXT("有静态绑定时，Lua 调用结果正确"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        GFastPathType.Class = UUnLuaNovaTestStub::StaticClass();
        FStaticBindingRegistry::Get().Register(&GFastPathType);

        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();

        UnLuaNova::RunChunk(L, "local Stub = NewObject(UE.UUnLuaNovaTestStub) return Stub:ProbeWithStaticBinding(41)");
        TEST_EQUAL((int32)lua_tointeger(L, -1), 42);
    });

    It(TEXT("没有静态绑定时，同一个调用的结果一致"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        // No registration: this probe stays on the reflective path for the whole process.
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();

        UnLuaNova::RunChunk(L, "local Stub = NewObject(UE.UUnLuaNovaTestStub) return Stub:ProbeWithoutStaticBinding(41)");
        TEST_EQUAL((int32)lua_tointeger(L, -1), 42);
    });

    It(TEXT("有静态绑定时出参与返回值都正确"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        GFastPathType.Class = UUnLuaNovaTestStub::StaticClass();
        FStaticBindingRegistry::Get().Register(&GFastPathType);

        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();

        // Both probes, so one chunk compares the two paths side by side.
        UnLuaNova::RunChunk(L, R"(
            local Stub = NewObject(UE.UUnLuaNovaTestStub)
            return Stub:ProbeWithStaticBinding(1), Stub:ProbeWithoutStaticBinding(1)
        )");
        TEST_EQUAL((int32)lua_tointeger(L, -2), 2);
        TEST_EQUAL((int32)lua_tointeger(L, -1), 2);
    });
}

#endif

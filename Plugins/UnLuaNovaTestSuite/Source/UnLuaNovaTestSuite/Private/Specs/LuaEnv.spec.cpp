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

// FLuaEnv is the entry point to everything else: how an environment is created, how a
// chunk is run, how a module is found, and what happens when any of those fail.
//
// Two things make these cases worth having. First, the loaders are ordered, and that
// order is a decision - a custom loader beats the file system, and the file system beats
// a builtin - so it is asserted rather than left to chance. Second, a failed chunk has to
// leave the environment usable: a script that dies once and then behaves differently for
// the rest of the process is far harder to diagnose than one that fails outright.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Returns a table carrying a marker, so a case can tell whether require() handed back
// this loader's value or something a different searcher produced.
static int SpecBuiltinLoader(lua_State* InL)
{
    lua_newtable(InL);
    lua_pushboolean(InL, 1);
    lua_setfield(InL, -2, "FromBuiltin");
    return 1;
}

BEGIN_DEFINE_SPEC(FLuaEnvSpec, "UnLuaNova.API.FLuaEnv", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FLuaEnvSpec)

void FLuaEnvSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    Describe(TEXT("创建Lua环境"), [this]()
    {
        It(TEXT("支持多个Lua环境"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::FLuaEnv Env1;
            UnLuaNova::FLuaEnv Env2;

            Env1.DoString("return 1");
            Env2.DoString("return 2");

            const auto A = (int32)lua_tointeger(Env1.GetMainState(), -1);
            const auto B = (int32)lua_tointeger(Env2.GetMainState(), -1);

            TEST_EQUAL(A, 1);
            TEST_EQUAL(B, 2);
        });

        It(TEXT("多个环境之间的全局变量互不可见"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // "Two environments" is only true if the second one cannot read what the
            // first one wrote - a shared global table would make every multi-env feature
            // silently cross-talk.
            UnLuaNova::FLuaEnv Env1;
            UnLuaNova::FLuaEnv Env2;

            TEST_TRUE(Env1.DoString(TEXT("X = 41")));
            lua_getglobal(Env2.GetMainState(), "X");

            TEST_TRUE(lua_isnil(Env2.GetMainState(), -1));
        });

        It(TEXT("支持启动脚本和参数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::FLuaEnv Env1;
            const auto L1 = Env1.GetMainState();
            TMap<FString, UObject*> Args;
            Args.Add("GWorld", GWorld);
            Env1.Start(TEXT("Tests.Specs.LuaEnv.支持启动脚本和参数"), Args);
            lua_getglobal(L1, "GWorld");
            const auto Actual = (UWorld*)UnLuaNova::GetUObject(L1, -1);
            const auto Expected = (UWorld*)GWorld;
            TEST_EQUAL(Actual, Expected);
        });

        It(TEXT("启动脚本能取到多个参数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // One parameter happening to work says nothing about the table being built
            // correctly - two does.
            UnLuaNova::FLuaEnv Env1;
            const auto L1 = Env1.GetMainState();
            const auto First = NewObject<UUnLuaNovaTestStub>();
            const auto Second = NewObject<UUnLuaNovaTestStub>();

            TMap<FString, UObject*> Args;
            Args.Add(TEXT("First"), First);
            Args.Add(TEXT("Second"), Second);
            Env1.Start(TEXT("Tests.Specs.LuaEnv.支持多个启动参数"), Args);

            lua_getglobal(L1, "StartArgFirst");
            TEST_EQUAL((UObject*)UnLuaNova::GetUObject(L1, -1), (UObject*)First);

            lua_getglobal(L1, "StartArgSecond");
            TEST_EQUAL((UObject*)UnLuaNova::GetUObject(L1, -1), (UObject*)Second);
        });

        It(TEXT("启动模块不存在时报错，而不是静默跳过"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Start() swallows the pcall result, so the log is the only signal a caller
            // gets - and "the startup script never ran" is otherwise indistinguishable
            // from "it ran and did nothing". AddExpectedError is the assertion here: if
            // the failure stops being reported, this case fails.
            UnLuaNova::FLuaEnv Env1;
            TMap<FString, UObject*> Args;
            AddExpectedError(TEXT("SpecNoSuchStartupModule"), EAutomationExpectedErrorFlags::Contains);
            Env1.Start(TEXT("SpecNoSuchStartupModule"), Args);
        });
    });

    Describe(TEXT("DoString"), [this]()
    {
        It(TEXT("语法错误返回 false，且之后仍可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Luau says "Incomplete statement ..." where plain Lua says "syntax error";
            // the expectation is whatever this backend produces.
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
            AddExpectedError(TEXT("Incomplete statement"), EAutomationExpectedErrorFlags::Contains);
#else
            AddExpectedError(TEXT("syntax error"), EAutomationExpectedErrorFlags::Contains);
#endif
            TEST_FALSE(Env->DoString(TEXT("invalid chunk")));
            TEST_TRUE(Env->DoString(TEXT("return 1")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
        });

        It(TEXT("运行时错误返回 false 且不污染栈顶"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const int32 TopBefore = lua_gettop(L);
            AddExpectedError(TEXT("boom"), EAutomationExpectedErrorFlags::Contains);
            TEST_FALSE(Env->DoString(TEXT("error('boom')")));
            TEST_EQUAL(lua_gettop(L), TopBefore);
            TEST_TRUE(Env->DoString(TEXT("return 7")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 7);
        });
    });

    Describe(TEXT("GC 与热重载"), [this]()
    {
        It(TEXT("GC 之后环境仍然可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TEST_TRUE(Env->DoString(TEXT("t = { v = 42 }")));
            Env->GC();
            TEST_TRUE(Env->DoString(TEXT("return t.v")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 42);
        });

        It(TEXT("HotReload 之后环境仍然可用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Only "does not break the environment" is asserted. The semantics of what
            // hot reload should do to a live module are their own subject.
            Env->HotReload();
            TEST_TRUE(Env->DoString(TEXT("return 1")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
        });
    });

    Describe(TEXT("模块加载"), [this]()
    {
        It(TEXT("自定义 loader 能提供文件系统里没有的模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // pcall keeps a failure from becoming an Error-level log; the case is about
            // the loader being consulted, not about the error text.
            UnLuaNova::FLuaEnv::FLuaFileLoader Loader = UnLuaNova::FLuaEnv::FLuaFileLoader::CreateLambda(
                [](const UnLuaNova::FLuaEnv& InEnv, const FString& FileName, TArray<uint8>& Data, FString& ChunkName) -> bool
                {
                    if (FileName != TEXT("SpecOnlyModule"))
                        return false;

                    const FTCHARToUTF8 Bytes(TEXT("return { ok = true }"));
                    Data.Append((const uint8*)Bytes.Get(), Bytes.Length());
                    ChunkName = TEXT("SpecOnlyModule");
                    return true;
                });
            Env->AddLoader(Loader);

            Env->DoString(TEXT("local Ok, M = pcall(require, 'SpecOnlyModule') _G.SpecLoaded = Ok and M ~= nil and M.ok == true"));
            lua_getglobal(L, "SpecLoaded");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("内建 loader 能提供模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->AddBuiltInLoader(TEXT("SpecBuiltinModule"), SpecBuiltinLoader);

            Env->DoString(TEXT("local Ok, M = pcall(require, 'SpecBuiltinModule') _G.SpecBuiltin = Ok and type(M) == 'table' and M.FromBuiltin == true"));
            lua_getglobal(L, "SpecBuiltin");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("文件系统优先于内建 loader"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The order is custom -> file system -> builtin, which means a builtin can
            // never shadow a real file. Registering one under a name that exists on disk
            // and still getting the file back is what pins that.
            Env->AddBuiltInLoader(TEXT("UnLuaNova.StringEx"), SpecBuiltinLoader);

            Env->DoString(TEXT("local Ok, M = pcall(require, 'UnLuaNova.StringEx') _G.SpecPriority = Ok and type(M) == 'table' and M.FromBuiltin == nil"));
            lua_getglobal(L, "SpecPriority");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("搜索路径命中 Content/Script 下的真实模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            Env->DoString(TEXT("local Ok, M = pcall(require, 'UnLuaNova.StringEx') _G.SpecPath = Ok and M ~= nil"));
            lua_getglobal(L, "SpecPath");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("require 不存在的模块报错，而不是返回 nil"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // A silent nil would turn into "attempt to index a nil value" three frames
            // later; an error at the require names the module that was missing.
            Env->DoString(TEXT("local Ok, E = pcall(require, 'Spec.NoSuchModule') _G.SpecMissing = Ok"));
            lua_getglobal(L, "SpecMissing");
            TEST_FALSE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("环境的生命周期"), [this]()
    {
        It(TEXT("OnCreated 与 OnDestroyed 各触发一次"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            int32 Created = 0;
            int32 Destroyed = 0;
            const auto HandleCreated = UnLuaNova::FLuaEnv::OnCreated.AddLambda([&Created](UnLuaNova::FLuaEnv&)
            {
                Created++;
            });
            const auto HandleDestroyed = UnLuaNova::FLuaEnv::OnDestroyed.AddLambda([&Destroyed](UnLuaNova::FLuaEnv&)
            {
                Destroyed++;
            });

            {
                UnLuaNova::FLuaEnv Local;
                TEST_EQUAL(Created, 1);
                TEST_EQUAL(Destroyed, 0);
            }

            // Order matters as much as count: anything still holding the lua_State after
            // OnDestroyed is holding a closed VM.
            TEST_EQUAL(Destroyed, 1);

            UnLuaNova::FLuaEnv::OnCreated.Remove(HandleCreated);
            UnLuaNova::FLuaEnv::OnDestroyed.Remove(HandleDestroyed);
        });

        It(TEXT("GetAll 与 FindEnv 能找到新环境"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const int32 Before = UnLuaNova::FLuaEnv::GetAll().Num();
            UnLuaNova::FLuaEnv Local;
            TEST_EQUAL(UnLuaNova::FLuaEnv::GetAll().Num(), Before + 1);
            TEST_EQUAL(UnLuaNova::FLuaEnv::FindEnv(Local.GetMainState()), &Local);
        });
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });
}

#endif

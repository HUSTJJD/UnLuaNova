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

// Covers the Lua modules the runtime ships (Plugins/UnLuaNova/Content/Script/UnLuaNova/).
//
// The reason this file exists: package.path is
// "Content/Script/?.lua;Plugins/UnLuaNova/Content/Script/?.lua" - the *project* comes first.
// A module parked in the demo project therefore loads fine here and is simply missing for
// anyone who installs the plugin, and nothing in this repo notices. StringEx, Vector and
// Buffer have all been in that state: StringEx existed in both places with different
// behaviour, Vector and Buffer existed only in the project while the docs handed out
// `UnLuaNova.Buffer.FromArray(...)` as a feature.
//
// So this asserts the file is where the plugin ships it, not merely that a require succeeds.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FRuntimeModulesSpec, "UnLuaNova.API.RuntimeModules", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FRuntimeModulesSpec)

void FRuntimeModulesSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });

    Describe(TEXT("随插件发布"), [this]()
    {
        It(TEXT("运行时模块都在插件的 Content/Script 下"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Every one of these is loaded by the runtime itself or documented as a
            // feature, so all of them have to travel with the plugin.
            const TCHAR* Modules[] = {
                TEXT("StringEx"),       // 环境启动时自动 require，装 string 扩展
                TEXT("Input"),          // 输入绑定
                TEXT("EnhancedInput"),  // 增强输入绑定
                TEXT("HotReload"),      // UNLUANOVA_WITH_HOT_RELOAD 时接管 require
                TEXT("Vector"),         // vector <-> FVector
                TEXT("Buffer"),         // buffer <-> TArray
            };

            const FString Root = FPaths::ProjectPluginsDir() / TEXT("UnLuaNova") / TEXT("Content") / TEXT("Script") / TEXT("UnLuaNova");
            for (const TCHAR* Module : Modules)
            {
                const FString Path = Root / (FString(Module) + TEXT(".lua"));
                // The path is the assertion text, so a failure says which module went
                // missing instead of just "false".
                TestTrue(*Path, IFileManager::Get().FileExists(*Path));
            }
        });
    });

    Describe(TEXT("Vector"), [this]()
    {
        It(TEXT("可以 require 到模块表"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Vector')
                return type(M) == 'table'
                    and type(M.HasVector) == 'function'
                    and type(M.Create) == 'function'
                    and type(M.ToFVector) == 'function'
                    and type(M.FromFVector) == 'function'
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("HasVector 与 HasFeature('vector') 一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Two answers to the same question, and a script branching on the wrong one
            // gets a nil where it expected a vector.
            UnLuaNova::RunChunk(L, "return require('UnLuaNova.Vector').HasVector() == UnLuaNova.HasFeature('vector')");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("原生后端下可以往返 FVector"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Vector')
                if not M.HasVector() then
                    return true    -- plain Lua 没有 vector，Create 返回 nil 才是正确行为
                end
                local Native = M.Create(1, 2, 3)
                local Vec = M.ToFVector(Native)
                return Native ~= nil and Vec ~= nil and M.FromFVector(Vec) ~= nil
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("Buffer"), [this]()
    {
        It(TEXT("可以 require 到模块表"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Buffer')
                return type(M) == 'table'
                    and type(M.HasBuffer) == 'function'
                    and type(M.FromArray) == 'function'
                    and type(M.ToArray) == 'function'
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("HasBuffer 与 HasFeature('buffer') 一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return require('UnLuaNova.Buffer').HasBuffer() == UnLuaNova.HasFeature('buffer')");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("有 buffer 的后端下 TArray 往返保持内容"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Buffer')
                if not M.HasBuffer() then
                    return true
                end
                local A = UE.TArray(0)
                A:Add(1) A:Add(2) A:Add(3)
                local Buf = M.FromArray(A)
                if Buf == nil then
                    return false
                end
                local Back = M.ToArray(Buf)
                return Back:Length() == 3 and Back:Get(1) == 1 and Back:Get(2) == 2 and Back:Get(3) == 3
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });
}

#endif

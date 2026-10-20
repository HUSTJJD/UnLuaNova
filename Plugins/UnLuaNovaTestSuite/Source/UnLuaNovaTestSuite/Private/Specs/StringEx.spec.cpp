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

// Covers the string helpers (Plugins/UnLuaNova/Content/Script/UnLuaNova/StringEx.lua).
//
// Two things about them are worth a test and were not covered by one:
//
//   1. They are installed onto the global `string` table when an environment starts, so a
//      script can call string.split without requiring anything. That is the contract the
//      docs promise, and there used to be a second copy of this file under the project's
//      Content/Script - which comes first in package.path and did *not* install anything -
//      so whether the helpers existed at all depended on which copy won. Asserting on
//      `string.split` is what catches a regression of that shape.
//   2. The point of the file is cross-backend parity: Luau ships string.split, plain Lua
//      does not. Every helper is asserted on both backends, because "works on the backend
//      I happened to build" is the failure mode this file exists to prevent.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FStringExSpec, "UnLuaNova.API.StringEx", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FStringExSpec)

void FStringExSpec::Define()
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

    Describe(TEXT("安装到 string 表"), [this]()
    {
        It(TEXT("环境启动后即可直接用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                return type(string.split) == 'function'
                    and type(string.strip) == 'function'
                    and type(string.startswith) == 'function'
                    and type(string.endswith) == 'function'
                    and type(string.count) == 'function'
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("require 也能拿到模块表"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local StringEx = require('UnLuaNova.StringEx')
                return type(StringEx) == 'table' and type(StringEx.join) == 'function'
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("split"), [this]()
    {
        It(TEXT("默认逗号分隔"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "local R = string.split('a,b,c') return #R .. ',' .. R[1] .. R[2] .. R[3]");
            TEST_EQUAL(FString(ANSI_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("3,abc")));
        });

        It(TEXT("分隔符按字面量处理，不是模式"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // '.' as a Lua pattern matches every character, which would yield an empty
            // table. Game code splitting paths and version strings hands us '.' all the
            // time, so this is the case that has to be right.
            UnLuaNova::RunChunk(L, "local R = string.split('1.2.3', '.') return #R .. R[2]");
            TEST_EQUAL(FString(ANSI_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("32")));
        });

        It(TEXT("没有分隔符时返回整串"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "local R = string.split('abc', ',') return #R .. R[1]");
            TEST_EQUAL(FString(ANSI_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("1abc")));
        });
    });

    Describe(TEXT("strip"), [this]()
    {
        It(TEXT("去掉首尾空白"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.strip('  hi  ')");
            TEST_EQUAL(FString(ANSI_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("hi")));
        });

        It(TEXT("trim 是 strip 的别名"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.trim('  hi  ')");
            TEST_EQUAL(FString(ANSI_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("hi")));
        });
    });

    Describe(TEXT("startswith / endswith"), [this]()
    {
        It(TEXT("前缀"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.startswith('Content/Script', 'Content/')");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("后缀"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.endswith('Foo.lua', '.lua')");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("空串恒为真"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.startswith('abc', '') and string.endswith('abc', '')");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("不匹配时为假"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.startswith('abc', 'x') or string.endswith('abc', 'x')");
            TEST_FALSE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("count"), [this]()
    {
        It(TEXT("统计不重叠出现次数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.count('a.b.c', '.')");
            TEST_EQUAL((int32)lua_tointeger(L, -1), 2);
        });

        It(TEXT("空的子串返回 0 而不是死循环"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return string.count('abc', '')");
            TEST_EQUAL((int32)lua_tointeger(L, -1), 0);
        });
    });

    Describe(TEXT("join"), [this]()
    {
        It(TEXT("用分隔符拼接并转字符串"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "local StringEx = require('UnLuaNova.StringEx') return StringEx.join(', ', {1, 2, 3})");
            TEST_EQUAL(FString(ANSI_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("1, 2, 3")));
        });

        It(TEXT("空表得到空串"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "local StringEx = require('UnLuaNova.StringEx') return StringEx.join(',', {})");
            TEST_EQUAL(FString(ANSI_TO_TCHAR(lua_tostring(L, -1))), FString(TEXT("")));
        });
    });
}

#endif

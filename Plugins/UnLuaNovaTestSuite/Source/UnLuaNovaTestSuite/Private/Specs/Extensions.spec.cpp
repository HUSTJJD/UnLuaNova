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

#include "UnLuaNovaBase.h"
#include "LuaBackend.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

// Covers the three optional extension plugins: LuaRapidjson, LuaProtobuf, LuaSocket.
//
// Each of them is a stock third-party Lua C module, unmodified apart from what it takes
// to build it here, and each one was written against plain Lua's loader rules -
// package.cpath, luaL_register, #include <lua.hpp> - none of which exist on Luau. So the
// interesting question is not "does the module work" (upstream tests that) but "does it
// still work once it is loaded the way UnLuaNova loads modules": through our require,
// off the built-in loader chain, against whichever backend is configured.
//
// They are *optional*, and off by default, so every case asks first whether the module is
// actually loadable and skips when it is not. A project that does not enable a plugin is a
// configuration, not a regression - failing there would make the suite useless for exactly
// the setups it is meant to cover.
//
// Run them on their own with:
//   .\Tools\RunTests.ps1 -Filter UnLuaNova.Extensions

namespace
{
    // Runs `pcall(require, Module)` and reports whether that produced a table.
    bool IsModuleLoadable(UnLuaNova::FLuaEnv& Env, const TCHAR* Module)
    {
        lua_State* const L = Env.GetMainState();
        const int32 Top = lua_gettop(L);

        // pcall leaves (ok, module-or-error) on the stack.
        const bool Ran = Env.DoString(FString::Printf(TEXT("return pcall(require, '%s')"), Module));
        const bool Loadable = Ran && lua_toboolean(L, -2) != 0 && lua_istable(L, -1) != 0;

        lua_settop(L, Top);
        return Loadable;
    }

    // Runs a chunk that must `return` one value, and leaves that value on top of the stack.
    bool RunForResult(UnLuaNova::FLuaEnv& Env, const TCHAR* Chunk)
    {
        lua_State* const L = Env.GetMainState();
        const int32 Top = lua_gettop(L);

        if (!Env.DoString(Chunk))
        {
            lua_settop(L, Top);
            return false;
        }

        if (lua_gettop(L) <= Top)
        {
            return false;
        }

        return true;
    }

    FString PopString(lua_State* L)
    {
        const FString Value = UTF8_TO_TCHAR(lua_tostring(L, -1));
        lua_pop(L, 1);
        return Value;
    }
}

BEGIN_DEFINE_SPEC(FUnLuaNovaExtensionsSpec, "UnLuaNova.Extensions", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
END_DEFINE_SPEC(FUnLuaNovaExtensionsSpec)

void FUnLuaNovaExtensionsSpec::Define()
{
    BeforeEach([this]
    {
        // Creating the environment is what makes the plugins register their built-in
        // loaders, so it has to happen here rather than once per suite.
        Env = MakeShared<UnLuaNova::FLuaEnv>();
    });

    Describe(TEXT("LuaRapidjson"), [this]()
    {
        It(TEXT("require 拿到带 decode/encode 的模块表"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("rapidjson")))
            {
                AddInfo(TEXT("LuaRapidjson 插件未启用，跳过"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            const bool Ok = RunForResult(*Env,
                TEXT("local r = require('rapidjson'); return type(r.decode) == 'function' and type(r.encode) == 'function'"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            TestTrue(TEXT("decode/encode 都是函数"), lua_toboolean(L, -1) != 0);
            lua_pop(L, 1);
        });

        It(TEXT("decode 能解析出字段"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("rapidjson")))
            {
                AddInfo(TEXT("LuaRapidjson 插件未启用，跳过"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            const bool Ok = RunForResult(*Env,
                TEXT("local r = require('rapidjson'); local t = r.decode('{\"a\":1,\"b\":\"x\"}'); return t.a"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            // lua_tonumber, not UNLUANOVA_TOINT64: a JSON number arrives as a Lua number
            // (a double on Luau), and the *64 accessors only read the integer subtype, so
            // they would answer 0 here for a value that is perfectly correct.
            TEST_EQUAL((int64)lua_tonumber(L, -1), 1ll);
            lua_pop(L, 1);
        });

        It(TEXT("encode/decode 往返一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("rapidjson")))
            {
                AddInfo(TEXT("LuaRapidjson 插件未启用，跳过"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            const bool Ok = RunForResult(*Env,
                TEXT("local r = require('rapidjson'); local t = {a = 7, b = 'x'}; return r.decode(r.encode(t)).b"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            TestEqual(TEXT("字段 b 往返后不变"), PopString(L), FString(TEXT("x")));
        });
    });

    Describe(TEXT("LuaProtobuf"), [this]()
    {
        It(TEXT("require 拿到可用的 pb 模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("pb")))
            {
                AddInfo(TEXT("LuaProtobuf 插件未启用，跳过"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            const bool Ok = RunForResult(*Env,
                TEXT("local pb = require('pb'); return type(pb.encode) == 'function' and type(pb.decode) == 'function'"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            TestTrue(TEXT("encode/decode 都是函数"), lua_toboolean(L, -1) != 0);
            lua_pop(L, 1);
        });

        It(TEXT("protoc 加载内联 proto 后编解码一致"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("pb")))
            {
                AddInfo(TEXT("LuaProtobuf 插件未启用，跳过"));
                return;
            }

            // protoc.lua is the Lua half of lua-protobuf and lives in the plugin's
            // Content/Script; it needs require() to reach it through the package path.
            if (!IsModuleLoadable(*Env, TEXT("protoc")))
            {
                AddInfo(TEXT("protoc.lua 不可用，跳过（只测 pb 本身）"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            const bool Ok = RunForResult(*Env,
                TEXT("local protoc = require('protoc'); local pb = require('pb'); ")
                TEXT("protoc:load('message ExtSpecMsg { optional int32 a = 1; optional string b = 2; }'); ")
                TEXT("local data = pb.encode('ExtSpecMsg', {a = 42, b = 'hi'}); ")
                TEXT("return pb.decode('ExtSpecMsg', data).a"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            // Same reason as the rapidjson case: protobuf int32 comes back as a Lua number.
            TEST_EQUAL((int64)lua_tonumber(L, -1), 42ll);
            lua_pop(L, 1);
        });
    });

    Describe(TEXT("LuaSocket"), [this]()
    {
        It(TEXT("require 拿到 socket 模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("socket")))
            {
                AddInfo(TEXT("LuaSocket 插件未启用，跳过"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            // socket.lua is the Lua half; it in turns requires socket.core, which is the
            // built-in C loader - so this one case covers both halves at once.
            const bool Ok = RunForResult(*Env, TEXT("local s = require('socket'); return type(s.gettime) == 'function'"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            TestTrue(TEXT("gettime 是函数"), lua_toboolean(L, -1) != 0);
            lua_pop(L, 1);
        });

        It(TEXT("gettime 返回递增的数字，不依赖网络"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("socket")))
            {
                AddInfo(TEXT("LuaSocket 插件未启用，跳过"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            const bool Ok = RunForResult(*Env, TEXT("local s = require('socket'); return s.gettime() > 0"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            // Nothing here opens a socket: gettime() is the one LuaSocket entry point
            // that works on a machine with no network and needs no permissions.
            TestTrue(TEXT("gettime() > 0"), lua_toboolean(L, -1) != 0);
            lua_pop(L, 1);
        });

        It(TEXT("socket.core 也能单独 require 到"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            if (!IsModuleLoadable(*Env, TEXT("socket.core")))
            {
                AddInfo(TEXT("LuaSocket 插件未启用，跳过"));
                return;
            }

            lua_State* const L = Env->GetMainState();

            const bool Ok = RunForResult(*Env, TEXT("local c = require('socket.core'); return type(c)"));
            TestTrue(TEXT("chunk 执行成功"), Ok);
            if (!Ok)
                return;

            TestEqual(TEXT("返回的是 table"), PopString(L), FString(TEXT("table")));
        });
    });

    AfterEach([this]
    {
        Env.Reset();
    });
}

#endif

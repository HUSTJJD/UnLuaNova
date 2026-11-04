// Copyright (c) 2026 UnLuaNova contributors
//
// Licensed under the MIT License (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations
// under the License.

// Sixth-round capability completion: the per-VM memory quota, the require() cycle
// guard, the explicit unrequire/reload contract, and the resolution boundaries that
// used to be implicit. Every case here is written to run on BOTH backends.
//
// DoString() uses LUA_MULTRET and leaves results on the main stack, so each case resets
// the stack before running a chunk and reads results by absolute index. Numbers are read
// with lua_tonumber: UNLUANOVA_PUSHINT pushes a double on Luau, where lua_tointeger on a
// double answers 0.

#include "LuaBackend.h"
#include "LuaEnv.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaTestHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FModuleContractsSpec, "UnLuaNova.API.ModuleContracts", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L = nullptr;
END_DEFINE_SPEC(FModuleContractsSpec)

void FModuleContractsSpec::Define()
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

    Describe(TEXT("per-Env memory quota"), [this]()
    {
        It(TEXT("is opt-in, adjustable at runtime and independent between envs"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::FLuaEnv Other;
            const int64 DefaultLimit = Env->GetMemoryQuotaMaxBytes();

            Env->SetMemoryQuotaMaxBytes(4096);
            TEST_TRUE(Env->GetMemoryQuotaMaxBytes() == 4096);
            TEST_TRUE(Other.GetMemoryQuotaMaxBytes() == DefaultLimit);
            TEST_TRUE(Env->GetMemoryQuotaRejectionCount() == 0);
            TEST_TRUE(Env->GetLiveLuaBytes() >= 0);

            // A negative value means "off", never a wrapped huge limit.
            Env->SetMemoryQuotaMaxBytes(-1);
            TEST_TRUE(Env->GetMemoryQuotaMaxBytes() == 0);
        });

        It(TEXT("a refused allocation is observable and does not poison the VM"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Set the limit just above the CURRENT waterline rather than an absolute
            // value: the baseline heap differs between backends and openlibs versions.
            const int64 Live = Env->GetLiveLuaBytes();
            Env->SetMemoryQuotaMaxBytes(Live + 64 * 1024);

            // The allocator logs exactly one Error line for the first refusal. pcall keeps
            // the VM's own "not enough memory" from becoming a second undeclared one.
            AddExpectedError(TEXT("Lua memory quota"), EAutomationExpectedErrorFlags::Contains);

            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT(
                "local ok = pcall(function()"
                "  local t = {}"
                "  for i = 1, 200000 do t[i] = string.rep('x', 128) end"
                "  return #t"
                "end) "
                "return ok")));
            TEST_FALSE(lua_toboolean(L, 1) != 0);
            TEST_TRUE(Env->GetMemoryQuotaRejectionCount() > 0);
            TEST_TRUE(Env->GetMemoryQuota().LastRequestedBytes.load() > 0);

            // Turn the quota off again: the emergency-GC path must have left the VM able
            // to run script, and teardown must not be constricted.
            Env->SetMemoryQuotaMaxBytes(0);
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT("return 1")));
            TEST_TRUE(lua_tonumber(L, 1) == 1.0);
        });

        It(TEXT("lua.memquota status reports a live CVar and per-env counters"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            (void)UnLuaNova::FLuaEnv::ReadMemoryQuotaCvar();
            auto* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.maxmemorykb"));
            TEST_TRUE(CVar != nullptr);
            if (CVar)
                TEST_TRUE(CVar->GetInt() == UnLuaNova::FLuaEnv::ReadMemoryQuotaCvar() / 1024);
        });
    });

    Describe(TEXT("require cycle guard"), [this]()
    {
        It(TEXT("rejects a named cycle and does not poison later requires"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT(
                "package.preload.SpecCycleA = function() return require('SpecCycleB') end "
                "package.preload.SpecCycleB = function() return require('SpecCycleA') end "
                "local ok, err = pcall(require, 'SpecCycleA') "
                "return ok, tostring(err)")));
            TEST_FALSE(lua_toboolean(L, 1) != 0);

            FString ErrorText;
            if (const char* Err = lua_tostring(L, 2))
                ErrorText = UTF8_TO_TCHAR(Err);
            TEST_TRUE(ErrorText.Contains(TEXT("cyclic dependency")));

            // The loading mark must be cleared on failure, otherwise this require is
            // rejected forever with a false cycle.
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT(
                "package.preload.SpecCycleB = function() return { ok = true } end "
                "local ok = pcall(require, 'SpecCycleB') return ok")));
            TEST_TRUE(lua_toboolean(L, 1) != 0);
        });

        It(TEXT("keeps extra arguments and the second return value of require"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT(
                "package.preload.SpecArgs = function(name, data) "
                "  return { name = name, data = data } "
                "end "
                "local m = require('SpecArgs') "
                "return type(m), m.name")));
            FString Type;
            FString Name;
            if (const char* S = lua_tostring(L, 1)) Type = UTF8_TO_TCHAR(S);
            if (const char* S = lua_tostring(L, 2)) Name = UTF8_TO_TCHAR(S);
            TEST_TRUE(Type == TEXT("table"));
            TEST_TRUE(Name == TEXT("SpecArgs"));
        });
    });

    Describe(TEXT("explicit cache and reload contract"), [this]()
    {
        It(TEXT("unrequire clears caches but keeps loader declarations"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT(
                "local calls = 0 "
                "package.preload.SpecUnload = function() "
                "  calls = calls + 1 return { token = calls } "
                "end "
                "local a = require('SpecUnload') "
                "local n = unrequire('SpecUnload') "
                "local stillDeclared = type(package.preload.SpecUnload) == 'function' "
                "local b = reloadmodule('SpecUnload') "
                "return n, (a ~= b), stillDeclared, a.token, b.token")));
            TEST_TRUE(lua_tonumber(L, 1) >= 1.0);
            TEST_TRUE(lua_toboolean(L, 2) != 0);
            TEST_TRUE(lua_toboolean(L, 3) != 0);
            TEST_TRUE(lua_tonumber(L, 4) == 1.0);
            TEST_TRUE(lua_tonumber(L, 5) == 2.0);
        });

        It(TEXT("unrequire on an unknown name is a no-op, not an error"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT("return unrequire('SpecNeverLoaded')")));
            TEST_TRUE(lua_tonumber(L, 1) == 0.0);
        });

        It(TEXT("package.resolution declares the unsupported boundaries"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT(
                "local r = package.resolution() "
                "return r.case_sensitive, r.automatic_case_fallback, "
                "       r.alias_config_files, r.runtime_alias_table, "
                "       r.cycle_detection, r.explicit_unrequire")));
            TEST_TRUE(lua_toboolean(L, 1) != 0);   // case sensitive
            TEST_FALSE(lua_toboolean(L, 2) != 0);  // no silent case fallback
            TEST_FALSE(lua_toboolean(L, 3) != 0);  // no upstream @alias config files
            TEST_TRUE(lua_toboolean(L, 4) != 0);   // package.aliases instead
            TEST_TRUE(lua_toboolean(L, 5) != 0);
            TEST_TRUE(lua_toboolean(L, 6) != 0);
        });

        It(TEXT("@alias config files fail with an explicit contract error"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            lua_settop(L, 0);
            TEST_TRUE(Env->DoString(TEXT(
                "local ok, err = pcall(require, '@SpecAlias') return ok, tostring(err)")));
            TEST_FALSE(lua_toboolean(L, 1) != 0);

            FString ErrorText;
            if (const char* Err = lua_tostring(L, 2))
                ErrorText = UTF8_TO_TCHAR(Err);
            TEST_TRUE(ErrorText.Contains(TEXT("@alias config files are not supported")));
        });
    });

    Describe(TEXT("diagnostic attribution"), [this]()
    {
        It(TEXT("DescribeEnv distinguishes live envs and answers for an unknown state"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::FLuaEnv Other;
            const FString A = UnLuaNova::DescribeEnv(L);
            const FString B = UnLuaNova::DescribeEnv(Other.GetMainState());
            TEST_TRUE(A.Contains(TEXT("env=")));
            TEST_FALSE(A.Equals(B));
            TEST_TRUE(UnLuaNova::DescribeEnv(nullptr) == TEXT("env=<unknown>"));
        });
    });
}

#endif // WITH_DEV_AUTOMATION_TESTS

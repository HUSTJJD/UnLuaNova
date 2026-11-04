// Copyright (c) 2026 UnLuaNova contributors
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
// The reason this file exists: package.path puts the *project* first, and the plugin's own
// script directory is now resolved from the plugin instance (see UnLuaNovaLib::BuildPackagePath)
// rather than from a hardcoded "Plugins/UnLuaNova/Content/Script".
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
                TEXT("Event"),          // 事件总线（纯 Lua）
                TEXT("OOP"),            // 脚本面向对象（纯 Lua）
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

        // 字节是 0..255 而不是 0..127，空数组也是一条独立的路径（C++ 版在 Num()==0 时直接返回）
        It(TEXT("往返保留 0/255 边界值和空数组"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Buffer')
                if not M.HasBuffer() then
                    return true
                end
                local Empty = M.ToArray(M.FromArray(UE.TArray(0)))
                if Empty:Length() ~= 0 then
                    return false
                end
                local A = UE.TArray(0)
                A:Add(0) A:Add(127) A:Add(255)
                local B = M.ToArray(M.FromArray(A))
                return B:Length() == 3 and B:Get(1) == 0 and B:Get(2) == 127 and B:Get(3) == 255
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("Event"), [this]()
    {
        // 这几条断言的不是"能存表"，是手写版本最容易错的四件事：
        // 广播中增删、单个监听者报错、owner 摘除、Once 语义。
        It(TEXT("可以 require 到模块表"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Event')
                return type(M) == 'table'
                    and type(M.On) == 'function'
                    and type(M.Once) == 'function'
                    and type(M.Off) == 'function'
                    and type(M.Emit) == 'function'
                    and type(M.UnbindOwner) == 'function'
                    and type(M.Clear) == 'function'
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("HasFeature('event') 为真"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return UnLuaNova.HasFeature('event') == true");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("广播会把参数透传给每个监听者"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Event')
                M.Clear()
                local Got = 0
                M.On("T", function(a, b) Got = a + b end)
                M.Emit("T", 2, 3)
                M.Clear()
                return Got == 5
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("Once 只触发一次"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Event')
                M.Clear()
                local N = 0
                M.Once("T", function() N = N + 1 end)
                M.Emit("T")
                M.Emit("T")
                M.Clear()
                return N == 1
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("广播中 Off 不会漏发也不会重发"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // 快照语义：本次广播里摘掉自己，其余监听者照常收到一次
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Event')
                M.Clear()
                local A, B = 0, 0
                local TokenA
                TokenA = M.On("T", function()
                    A = A + 1
                    M.Off(TokenA)
                end)
                M.On("T", function() B = B + 1 end)
                M.Emit("T")
                M.Emit("T")
                M.Clear()
                return A == 1 and B == 2
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("一个监听者报错不影响其他监听者"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            AddExpectedError(TEXT("boom"), EAutomationExpectedErrorFlags::Contains, 1);
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Event')
                M.Clear()
                local Reached = false
                M.On("T", function() error("boom") end)
                M.On("T", function() Reached = true end)
                local Fired = M.Emit("T")
                M.Clear()
                return Reached and Fired == 2
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("UnbindOwner 摘掉 owner 名下全部订阅"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Event')
                M.Clear()
                local Owner = {}
                local N = 0
                M.On("A", function() N = N + 1 end, Owner)
                M.On("B", function() N = N + 1 end, Owner)
                M.Emit("A") M.Emit("B")
                local Before = N
                M.UnbindOwner(Owner)
                M.Emit("A") M.Emit("B")
                local After = N
                M.Clear()
                return Before == 2 and After == 2
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("失效 owner 在广播前自动摘除，不必显式 UnbindOwner"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Event')
                M.Clear()
                local Owner = { Alive = true }
                local N = 0
                M.On("A", function() N = N + 1 end, Owner)
                M.SetOwnerValidator(function(o) return o.Alive end)
                M.Emit("A")
                Owner.Alive = false
                M.Emit("A")
                local Result = N == 1 and M.Count("A") == 0
                M.SetOwnerValidator(nil)
                M.Clear()
                return Result
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("OOP"), [this]()
    {
        // 手写 class 最容易错的两处：构造链漏掉基类、Super 访问把父类当 self。
        // 两条都"不报错、只是行为不对"，所以必须有用例钉住。
        It(TEXT("构造链从基类到子类依次调用 Ctor"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local OOP = require('UnLuaNova.OOP')
                local Order = {}
                local A = OOP.Class("A")
                function A:Ctor() Order[#Order + 1] = "A" end
                local B = OOP.Class("B", A)
                function B:Ctor() Order[#Order + 1] = "B" end
                local C = OOP.Class("C", B)      -- 没有 Ctor，不应打断构造链
                local Inst = C:New()
                return Order[1] == "A" and Order[2] == "B" and #Order == 2
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("Ctor 的参数会透传给每一层"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local OOP = require('UnLuaNova.OOP')
                local A = OOP.Class("A")
                function A:Ctor(X) self.Base = X end
                local B = OOP.Class("B", A)
                function B:Ctor(X) self.Derived = X end
                local Inst = B:New(7)
                return Inst.Base == 7 and Inst.Derived == 7
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("Super 可以访问父类实现"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local OOP = require('UnLuaNova.OOP')
                local A = OOP.Class("A")
                function A:Tag() return "A" end
                local B = OOP.Class("B", A)
                function B:Tag() return "B+" .. B.Super.Tag(self) end
                return B:New():Tag() == "B+A"
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("Is 沿继承链判断，且不会把实例当成类"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local OOP = require('UnLuaNova.OOP')
                local A = OOP.Class("A")
                local B = OOP.Class("B", A)
                local Inst = B:New()
                return OOP.Is(Inst, B) and OOP.Is(Inst, A)
                    and not OOP.Is(Inst, OOP.Class("C"))
                    and OOP.IsClass(B) and not OOP.IsClass(Inst)
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("HasFeature('oop') 为真"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return UnLuaNova.HasFeature('oop') == true");
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("Timer"), [this]()
    {
        // 内建（native）模块：没有 .lua 文件，走 FLuaEnv::AddBuiltInLoader。
        // 这里只验"require 得到、API 齐全、能力查询一致" —— 到期与暂停语义要真世界，
        // 放在需要 World 的用例里，不在纯 VM 环境里假装能测。
        It(TEXT("require 得到内建模块"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local M = require('UnLuaNova.Timer')
                return type(M) == 'table'
                    and type(M.Set) == 'function'
                    and type(M.Clear) == 'function'
                    and type(M.Pause) == 'function'
                    and type(M.IsValid) == 'function'
                    and type(M.ClearAll) == 'function'
                    and type(M.Time) == 'function'
                    and type(M.Count) == 'function'
            )");
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("HasFeature('timer') 为真"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, "return UnLuaNova.HasFeature('timer') == true");
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });

    // RUNTIME-RELOAD / failed reload rollback。
    // 往 package.searchers 注入一个「虚拟模块」，用全局 SpecRollbackSource 切换
    // 好 / 加载期抛错 / 语法错误 三份源码：全程不落盘、不依赖文件时间戳精度，
    // Luau 与 plain Lua 两个后端走的是同一段 Lua。每个 It 有独立的 FLuaEnv，
    // 所以 searcher 注入与全局变量都不会跨用例泄漏。
    Describe(TEXT("HotReload 失败重载回滚"), [this]()
    {
        It(TEXT("成功路径 reload 返回 true"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local Hot = require('UnLuaNova.HotReload')
                local Name = 'SpecRollbackModule'
                SpecRollbackSource = "local M = {}; M.Version = 1; function M.Get() return 1 end; return M"
                table.insert(package.searchers, 1, function(name)
                    if name == Name then return assert(load(SpecRollbackSource, name)) end
                end)
                local Mod = Hot.require(Name)
                if type(Mod) ~= 'table' or Mod.Get() ~= 1 then return false end
                SpecRollbackSource = "local M = {}; M.Version = 2; function M.Get() return 2 end; return M"
                local ok, err = Hot.reload({Name})
                return ok == true and err == nil and Mod.Get() == 2
            )");
            TestTrue(TEXT("回滚用例 chunk 无未捕获错误"), lua_type(L, -1) == LUA_TBOOLEAN);
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("加载期抛错：返回 false、旧状态不变、修好后能成功"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            AddExpectedError(TEXT("SpecRollbackBoom"), EAutomationExpectedErrorFlags::Contains, 1);
            UnLuaNova::RunChunk(L, R"(
                local Hot = require('UnLuaNova.HotReload')
                local Name = 'SpecRollbackModule'
                SpecRollbackSource = "local M = {}; M.Version = 1; function M.Get() return 1 end; return M"
                table.insert(package.searchers, 1, function(name)
                    if name == Name then return assert(load(SpecRollbackSource, name)) end
                end)
                local Mod = Hot.require(Name)
                if type(Mod) ~= 'table' then return false end
                local OldLoaded = package.loaded[Name]
                -- 加载期 error()：由 xpcall + load_error_handler 捕获，计算阶段即失败
                SpecRollbackSource = "error('SpecRollbackBoom') return {}"
                local ok, err = Hot.reload({Name})
                local failed = (ok == false and type(err) == 'string')
                local unchanged = (Mod.Get() == 1 and Mod.Version == 1
                                   and package.loaded[Name] == OldLoaded)
                -- 修好后再 reload 能成功，新函数生效
                SpecRollbackSource = "local M = {}; M.Version = 2; function M.Get() return 2 end; return M"
                local ok2 = Hot.reload({Name})
                local retried = (ok2 == true and Mod.Get() == 2
                                 and package.loaded[Name] == OldLoaded)
                return failed and unchanged and retried
            )");
            TestTrue(TEXT("回滚用例 chunk 无未捕获错误"), lua_type(L, -1) == LUA_TBOOLEAN);
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("语法错误 + 无参 reload：失败不推进 mtime、修好后自动重试"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local Hot = require('UnLuaNova.HotReload')
                local Name = 'SpecRollbackModule'
                SpecRollbackSource = "local M = {}; M.Version = 1; function M.Get() return 1 end; return M"
                table.insert(package.searchers, 1, function(name)
                    if name == Name then return assert(load(SpecRollbackSource, name)) end
                end)
                local Mod = Hot.require(Name)
                if type(Mod) ~= 'table' or Mod.Get() ~= 1 then return false end
                -- 白盒：M.reload 的 upvalue 里，loaded_module_times 是唯一一个
                -- [Name] 非 nil 且不等于模块表的 table（ignore_modules/config 均无此键）
                local times
                local i = 1
                while true do
                    local n, v = debug.getupvalue(Hot.reload, i)
                    if not n then break end
                    if type(v) == 'table' and v[Name] ~= nil and v[Name] ~= Mod then
                        times = v
                        break
                    end
                    i = i + 1
                end
                if not times then return false end
                -- 模拟「文件在上次记录之后被改动」：换成必然 != 数值时间戳的哨兵
                times[Name] = false
                local OldGet = Mod.Get
                -- 坏版本（语法错误）：编译期失败，由 pcall(sandbox.load) 捕获
                SpecRollbackSource = "local M = { this is not valid lua !!!"
                local ok, err = Hot.reload()
                local failed = (ok == false and type(err) == 'string')
                local notAdvanced = (times[Name] == false)   -- 核心：失败不得推进时间戳
                local unchanged = (Mod.Get == OldGet and Mod.Get() == 1
                                   and package.loaded[Name] == Mod and Mod.Version == 1)
                -- 修好；旧实现失败时已把时间戳推进到当前值，这次无参 reload 会静默跳过
                SpecRollbackSource = "local M = {}; M.Version = 2; function M.Get() return 2 end; return M"
                local ok2 = Hot.reload()
                local retried = (ok2 == true and Mod.Get() == 2)
                local advanced = (times[Name] ~= false)      -- 成功后才推进为当前时间戳
                return failed and notAdvanced and unchanged and retried and advanced
            )");
            TestTrue(TEXT("回滚用例 chunk 无未捕获错误"), lua_type(L, -1) == LUA_TBOOLEAN);
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });

    // M.unrequire 契约：幂等、不抛错、清掉 HotReload 私有记账与 package.loaded，
    // 返回明确计数。对 package.preload 不做假设（当前实现刻意不清它，以免破坏
    // reloadmodule 对声明式模块的重载），故只断言「至少清掉 3 项」与关键表为 nil。
    Describe(TEXT("HotReload unrequire"), [this]()
    {
        It(TEXT("卸载模块缓存：幂等、计数、非法参数不抛错"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            UnLuaNova::RunChunk(L, R"(
                local Hot = require('UnLuaNova.HotReload')
                local Name = 'SpecUnrequireModule'
                SpecUnrequireSource = "local M = {}; function M.Get() return 1 end; return M"
                table.insert(package.searchers, 1, function(name)
                    if name == Name then return assert(load(SpecUnrequireSource, name)) end
                end)
                local Mod = Hot.require(Name)
                if type(Mod) ~= 'table' or package.loaded[Name] ~= Mod then return false end
                Hot.config.ignore_modules[Name] = true
                local n = Hot.unrequire(Name)
                local okCount = (type(n) == 'number' and n >= 3)
                local cleared = (package.loaded[Name] == nil
                                 and rawget(Hot.config.ignore_modules, Name) == nil)
                local again = Hot.unrequire(Name)
                local b1 = Hot.unrequire(nil)
                local b2 = Hot.unrequire(123)
                local b3 = Hot.unrequire('')
                return okCount and cleared and again == 0
                       and b1 == 0 and b2 == 0 and b3 == 0
            )");
            TestTrue(TEXT("unrequire 用例 chunk 无未捕获错误"), lua_type(L, -1) == LUA_TBOOLEAN);
            TEST_TRUE(lua_toboolean(L, -1));
        });
    });
}

#endif

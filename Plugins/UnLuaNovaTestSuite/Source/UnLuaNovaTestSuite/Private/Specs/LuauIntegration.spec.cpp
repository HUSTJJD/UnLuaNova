#include "LuaBackend.h"
#include "LuaEnv.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaSandbox.h"
#include "UnLuaNovaTestHelpers.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/OutputDeviceNull.h"

#if WITH_DEV_AUTOMATION_TESTS && UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU

BEGIN_DEFINE_SPEC(FLuauIntegrationSpec, "UnLuaNova.API.LuauIntegration", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L = nullptr;
    FString Root;
    FString PreviousRoot;
    int32 PreviousIo = 0;
END_DEFINE_SPEC(FLuauIntegrationSpec)

void FLuauIntegrationSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
        auto& Console = IConsoleManager::Get();
        auto* Enabled = Console.FindConsoleVariable(TEXT("lua.io"));
        PreviousIo = Enabled->GetInt();
        Enabled->Set(1, ECVF_SetByCode);
        Env.Reset();
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
        // ResolvePath registers lua.ioroot on its first call; this does not create a file.
        UnLuaNova::RunChunk(L, "return io.open('Saved/UnLuaNovaIoProbeMissing', 'r')");
        lua_settop(L, 0);
        auto* RootVar = Console.FindConsoleVariable(TEXT("lua.ioroot"));
        PreviousRoot = RootVar->GetString();
        Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/LuauIo"));
        RootVar->Set(*Root, ECVF_SetByCode);
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
        IConsoleManager::Get().FindConsoleVariable(TEXT("lua.io"))->Set(PreviousIo, ECVF_SetByCode);
        IConsoleManager::Get().FindConsoleVariable(TEXT("lua.ioroot"))->Set(*PreviousRoot, ECVF_SetByCode);
        IFileManager::Get().Delete(*(Root / TEXT("lines.txt")));
    });

    It(TEXT("Buffer example preserves copy ownership"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, "assert(require('Luau.Index').BufferInterop:Run())"));
    });

    It(TEXT("CRLF reads and iterators consume the full line ending"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            local f = assert(io.open('lines.txt', 'w'))
            f:write('first\r\n\r\nlast') assert(f:close())
            f = assert(io.open('lines.txt', 'r'))
            assert(f:read() == 'first')
            assert(f:read('*l') == '')
            assert(f:read('l') == 'last')
            assert(f:read() == nil) f:close()
            local lines = {}
            for line in io.lines('lines.txt') do lines[#lines + 1] = line end
            assert(#lines == 3 and lines[1] == 'first' and lines[2] == '' and lines[3] == 'last')
        )"));
    });

    It(TEXT("Invalid lengths and mutable paths cannot bypass validation"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            local f = assert(io.open('lines.txt', 'w'))
            f:write('123') assert(f:close())
            f = assert(io.open('lines.txt', 'r'))
            assert(not pcall(f.read, f, -1))
            assert(not pcall(f.read, f, 'invalid'))
            assert(f:read('*n') == 123)
            f.__pos = -1
            assert(not pcall(f.read, f, 1))
            f.__pos = 0 f:close()
            assert(not pcall(io.open, '../escape.txt', 'w'))
            assert(not pcall(io.open, 'C:/escape.txt', 'w'))
            f = assert(io.open('lines.txt', 'w'))
            f.__path = '../escape.txt'
            assert(not pcall(f.flush, f))
            f.__path = 'lines.txt' assert(f:close())
        )"));
    });

    It(TEXT("io.open modes and file:seek options are validated"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            local f = assert(io.open('lines.txt', 'w'))
            f:write('abc\ndef') assert(f:close())

            -- 'b' is a no-op here, but an unknown letter, an empty mode or 'b' alone
            -- must be refused instead of quietly opening a readable handle.
            assert(not pcall(io.open, 'lines.txt', 'q'))
            assert(not pcall(io.open, 'lines.txt', ''))
            assert(not pcall(io.open, 'lines.txt', 'b'))

            f = assert(io.open('lines.txt', 'rb'))
            assert(f:seek('set', 4) == 4)
            assert(f:read(3) == 'def')
            assert(f:seek('end', -3) == 4)
            assert(f:seek('cur', -100) == 0)
            assert(not pcall(f.seek, f, 'nonsense'))
            assert(f:close())
        )"));
    });

    It(TEXT("file:lines and io.lines honour format lists"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            local f = assert(io.open('lines.txt', 'w'))
            f:write('1 first\n2 second\n3 third') assert(f:close())

            local count = 0
            for n, word in io.lines('lines.txt', 'n', 'l') do
                count = count + 1
                assert(n == count)
                assert(word == ' first' or word == ' second' or word == ' third')
            end
            assert(count == 3)

            local g = assert(io.open('lines.txt', 'r'))
            local lines = {}
            for line in g:lines('l') do lines[#lines + 1] = line end
            assert(#lines == 3 and lines[1] == '1 first' and lines[3] == '3 third')
            assert(g:close())
        )"));
    });

    It(TEXT("lua.iomaxbytes stops an unbounded write before it is buffered"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        // Register the CVar with a harmless call first, then tighten it for this case.
        UnLuaNova::ApplyCapabilityPolicy(L);
        auto* MaxVar = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.iomaxbytes"));
        if (!MaxVar)
            return;
        const int32 Saved = MaxVar->GetInt();
        MaxVar->Set(8, ECVF_SetByCode);

        const bool bOk = UnLuaNova::RunChunk(L, R"(
            local f = assert(io.open('lines.txt', 'w'))
            assert(not pcall(f.write, f, string.rep('x', 32)))
            assert(f:close())
        )");

        MaxVar->Set(Saved, ECVF_SetByCode);
        TEST_TRUE(bOk);
    });

    It(TEXT("sandbox capability policy trims globals but protects host tables"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        UnLuaNova::ApplyCapabilityPolicy(L); // registers lua.sandboxdeny
        auto* Deny = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.sandboxdeny"));
        if (!Deny)
            return;
        const FString Saved(Deny->GetString());
        Deny->Set(TEXT("debug,io,require,UE"), ECVF_SetByCode);

        const int32 Removed = UnLuaNova::ApplyCapabilityPolicy(L);
        const bool bState = UnLuaNova::RunChunk(L,
            "return debug == nil and io == nil and require ~= nil");

        Deny->Set(*Saved, ECVF_SetByCode);
        TEST_TRUE(Removed == 2);
        TEST_TRUE(bState && lua_toboolean(L, -1) != 0);
    });

    It(TEXT("file:write overwrites at __pos and append mode still writes at the end"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            local f = assert(io.open('lines.txt', 'w'))
            f:write('abcdef') assert(f:close())

            f = assert(io.open('lines.txt', 'r+'))
            assert(f:seek('set', 2) == 2)
            f:write('XY')
            assert(f.__data == 'abXYef')
            assert(f.__pos == 4)
            assert(f:seek('set', 0) == 0)
            assert(f:read('a') == 'abXYef')
            assert(f:close())

            -- Append mode ignores a seek: the write always lands at the end.
            f = assert(io.open('lines.txt', 'a'))
            assert(f:seek('set', 0) == 0)
            f:write('Z')
            assert(f.__data == 'abXYefZ')
            assert(f:close())
        )"));
    });

    It(TEXT("io.tmpfile is an in-memory handle and io.popen is an explicit refusal"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            local t = assert(io.tmpfile())
            t:write('scratch')
            assert(t:seek('set', 0) == 0)
            assert(t:read('a') == 'scratch')
            assert(io.type(t) == 'file')
            assert(t:close())
            assert(io.type(t) == 'closed file')
            assert(not pcall(io.popen, 'dir'))
        )"));
    });

    It(TEXT("package.aliases redirects require and rejects a self-alias"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            package.preload['Spec.AliasTarget'] = function() return { tag = 'target' } end
            package.aliases['Spec.Alias'] = 'Spec.AliasTarget'
            local m = require('Spec.Alias')
            assert(m.tag == 'target')
            assert(require('Spec.AliasTarget') == m)
            assert(not pcall(function()
                package.aliases['Spec.Loop'] = 'Spec.Loop'
                return require('Spec.Loop')
            end))

            package.aliases['Spec.Alias'] = nil
            package.aliases['Spec.Loop'] = nil
            package.preload['Spec.AliasTarget'] = nil
            package.loaded['Spec.Alias'] = nil
            package.loaded['Spec.AliasTarget'] = nil
        )"));
    });

    It(TEXT("require reports a cyclic dependency and a failed load does not poison the name"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(UnLuaNova::RunChunk(L, R"(
            package.preload['Spec.CycleA'] = function() return require('Spec.CycleB') end
            package.preload['Spec.CycleB'] = function() return require('Spec.CycleA') end
            local ok, err = pcall(require, 'Spec.CycleA')
            assert(not ok)
            assert(tostring(err):find('cyclic dependency', 1, true))

            -- The loading marks must be cleared on error, otherwise this name can
            -- never be required again even after the module is fixed.
            package.preload['Spec.CycleA'] = function() return { ok = true } end
            package.preload['Spec.CycleB'] = nil
            assert(require('Spec.CycleA').ok == true)

            package.loaded['Spec.CycleA'] = nil
            package.loaded['Spec.CycleB'] = nil
            package.preload['Spec.CycleA'] = nil
        )"));
    });

    It(TEXT("Native compilation survives coroutine and VM lifetimes"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        if (!UnLuaNova::IsNativeCodeGenAvailable())
        {
            AddWarning(TEXT("Native compilation is unavailable on this target; requires a supported target for acceptance."));
            return;
        }
        TEST_EQUAL(luaL_loadstring(L, "return 1"), LUA_OK);
        UnLuaNova::MaybeCompileNativeChunk(L, "NativeLifecycle");
        lua_pop(L, 1);
        auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.codegen.enabled"));
        auto* Modules = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.codegen.modules"));
        const int32 PreviousEnabled = Enabled->GetInt();
        const FString PreviousModules = Modules->GetString();
        Enabled->Set(1, ECVF_SetByCode);
        Modules->Set(TEXT("NativeLifecycle"), ECVF_SetByCode);
        for (int32 Iteration = 0; Iteration < 16; ++Iteration)
        {
            auto Vm = MakeShared<UnLuaNova::FLuaEnv>();
            lua_State* Main = Vm->GetMainState();
            for (int32 ThreadIndex = 0; ThreadIndex < 3; ++ThreadIndex)
            {
                lua_State* Thread = ThreadIndex == 0 ? Main : lua_newthread(Main);
                TEST_EQUAL(luaL_loadstring(Thread, "return 21 * 2"), LUA_OK);
                UnLuaNova::MaybeCompileNativeChunk(Thread, "NativeLifecycle");
                TEST_EQUAL(lua_pcall(Thread, 0, 1, 0), LUA_OK);
                TEST_EQUAL(int32(lua_tointeger(Thread, -1)), 42);
                lua_settop(Thread, 0);
                if (Thread != Main)
                    lua_pop(Main, 1);
            }
        }
        Modules->Set(*PreviousModules, ECVF_SetByCode);
        Enabled->Set(PreviousEnabled, ECVF_SetByCode);
    });

    It(TEXT("Native compiler command and enable variable coexist"), EAsyncExecution::TaskGraphMainThread, [this]
    {
#if UNLUANOVA_HAS_NATIVE_CODEGEN
        auto& Console = IConsoleManager::Get();
        auto* Command = Console.FindConsoleObject(TEXT("lua.codegen"));
        auto* Enabled = Console.FindConsoleVariable(TEXT("lua.codegen.enabled"));
        TEST_TRUE(Command != nullptr && Command->AsCommand() != nullptr);
        TEST_TRUE(Enabled != nullptr);
        if (!Command || !Command->AsCommand() || !Enabled)
            return;

        const int32 PreviousEnabled = Enabled->GetInt();
        const EConsoleVariableFlags PreviousFlags = Enabled->GetFlags();
        FOutputDeviceNull Output;
        TEST_TRUE(Console.ProcessUserConsoleInput(TEXT("lua.codegen on"), Output, nullptr));
        TEST_EQUAL(Enabled->GetInt(), 1);
        TEST_TRUE(Console.ProcessUserConsoleInput(TEXT("lua.codegen off"), Output, nullptr));
        TEST_EQUAL(Enabled->GetInt(), 0);
        Enabled->Set(PreviousEnabled, ECVF_SetByConsole);
        Enabled->SetFlags(PreviousFlags);
#endif
    });
}

#endif

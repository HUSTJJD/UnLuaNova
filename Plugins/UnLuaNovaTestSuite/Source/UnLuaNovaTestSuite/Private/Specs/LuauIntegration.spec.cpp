#include "LuaBackend.h"
#include "LuaEnv.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaTestHelpers.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

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
        auto* Enabled = IConsoleManager::Get().FindConsoleVariable(TEXT("lua.codegen"));
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
}

#endif

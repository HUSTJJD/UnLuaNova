#include "LuaEnv.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaTimerSubsystem.h"
#include "UnLuaNovaTestHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && UNLUANOVA_HAS_STDLIB_TIMER

BEGIN_DEFINE_SPEC(FTimerSpec, "UnLuaNova.API.Timer", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    UWorld* World = nullptr;
    UUnLuaNovaTimerSubsystem* Timer = nullptr;
    lua_State* L = nullptr;
END_DEFINE_SPEC(FTimerSpec)

void FTimerSpec::Define()
{
    BeforeEach([this]
    {
        World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TimerSpec"));
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
        World->InitializeActorsForPlay(FURL());
        World->BeginPlay();
        Timer = World->GetSubsystem<UUnLuaNovaTimerSubsystem>();
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
        UnLuaNova::PushUObject(L, World);
        lua_setglobal(L, "World");
        TEST_TRUE(Env->DoString(TEXT("Timer = require('UnLuaNova.Timer')")));
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
        if (World)
        {
            GEngine->DestroyWorldContext(World);
            World->DestroyWorld(false);
        }
        World = nullptr;
        Timer = nullptr;
    });

    It(TEXT("collected caller coroutine cannot invalidate its callback"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(Env->DoString(TEXT(R"(
            Hits = 0
            local co = coroutine.create(function()
                Timer.Set(function() Hits = Hits + 1 end, 0, false, 0, false, World)
            end)
            assert(coroutine.resume(co))
        )")));
        lua_gc(L, LUA_GCCOLLECT, 0);
        const int32 Top = lua_gettop(L);
        Timer->Tick(0);
        TEST_EQUAL(lua_gettop(L), Top);
        TEST_EQUAL(Timer->Num(), 0);
        TEST_TRUE(Env->DoString(TEXT("assert(Hits == 1)")));
    });

    It(TEXT("one-shot is removed before callback and nested tick does not reenter"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        lua_pushlightuserdata(L, Timer);
        lua_pushcclosure(L, [](lua_State* Inner) -> int
        {
            static_cast<UUnLuaNovaTimerSubsystem*>(lua_touserdata(Inner, lua_upvalueindex(1)))->Tick(0);
            return 0;
        }, 1);
        lua_setglobal(L, "Reenter");
        TEST_TRUE(Env->DoString(TEXT(R"(
            Hits = 0
            Handle = Timer.Set(function()
                assert(not Timer.IsValid(Handle))
                Hits = Hits + 1
                Reenter()
                Timer.Set(function() Hits = Hits + 10 end, 0, false, 0, false, World)
            end, 0, false, 0, false, World)
        )")));
        Timer->Tick(0);
        TEST_TRUE(Env->DoString(TEXT("assert(Hits == 1)")));
        Timer->Tick(0);
        TEST_TRUE(Env->DoString(TEXT("assert(Hits == 11)")));
    });

    It(TEXT("cancelling another due callback prevents its invocation"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(Env->DoString(TEXT(R"(
            Hits = 0
            Timer.Set(function() assert(Timer.Clear(Other)) end, 0, false, 0, false, World)
            Other = Timer.Set(function() Hits = Hits + 1 end, 0, false, 0, false, World)
        )")));
        Timer->Tick(0);
        TEST_TRUE(Env->DoString(TEXT("assert(Hits == 0 and Timer.Count(World) == 0)")));
    });

    It(TEXT("pause preserves the remaining initial delay"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(Env->DoString(TEXT(R"(
            Hits = 0
            Handle = Timer.Set(function() Hits = Hits + 1 end, 0, false, 100, false, World)
            assert(Timer.Pause(Handle, true))
            assert(Timer.Pause(Handle, false))
        )")));
        Timer->Tick(0);
        TEST_TRUE(Env->DoString(TEXT("assert(Hits == 0 and Timer.IsValid(Handle))")));
    });

    It(TEXT("handles and ClearAll are isolated by environment"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(Env->DoString(TEXT("Handle = Timer.Set(function() end, 0, true, 100, false, World)")));
        lua_getglobal(L, "Handle");
        const int32 Handle = int32(lua_tointeger(L, -1));
        lua_pop(L, 1);
        auto OtherEnv = MakeShared<UnLuaNova::FLuaEnv>();
        auto* Other = OtherEnv->GetMainState();
        UnLuaNova::PushUObject(Other, World);
        lua_setglobal(Other, "World");
        UNLUANOVA_PUSHINT(Other, Handle);
        lua_setglobal(Other, "Foreign");
        TEST_TRUE(OtherEnv->DoString(TEXT(R"(
            local t = require('UnLuaNova.Timer')
            assert(not t.Clear(Foreign) and not t.Pause(Foreign, true) and not t.IsValid(Foreign))
            t.Set(function() end, 0, true, 100, false, World)
            assert(t.Count(World) == 1)
            t.ClearAll(World)
            assert(t.Count(World) == 0)
        )")));
        TEST_TRUE(Timer->IsValid(Handle));
        TEST_EQUAL(Timer->Num(), 1);
        Env.Reset();
        L = nullptr;
        TEST_EQUAL(Timer->Num(), 0);
    });

    It(TEXT("world teardown invalidates handles while VM remains usable"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(Env->DoString(TEXT("Handle = Timer.Set(function() end, 0, true, 100, false, World)")));
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
        World = nullptr;
        Timer = nullptr;
        TEST_TRUE(Env->DoString(TEXT("assert(not Timer.IsValid(Handle) and not Timer.Clear(Handle))")));
    });

    It(TEXT("closing Env rejects new callback registrations"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        bool bObserved = false;
        const FDelegateHandle Delegate = UnLuaNova::FLuaEnv::OnDestroyed.AddLambda([this, &bObserved](UnLuaNova::FLuaEnv& Closing)
        {
            if (Closing.GetMainState() != L)
                return;
            bObserved = true;
            lua_pushcfunction(L, [](lua_State*) -> int { return 0; });
            const int32 Ref = luaL_ref(L, LUA_REGISTRYINDEX);
            TEST_EQUAL(Timer->Add(L, Ref, 0, false, 0, false), INDEX_NONE);
            luaL_unref(L, LUA_REGISTRYINDEX, Ref);
        });
        Env.Reset();
        UnLuaNova::FLuaEnv::OnDestroyed.Remove(Delegate);
        L = nullptr;
        TEST_TRUE(bObserved);
        TEST_EQUAL(Timer->Num(), 0);
    });

    It(TEXT("multiple worlds require explicit context"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("TimerOtherWorld"));
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(OtherWorld);
        OtherWorld->InitializeActorsForPlay(FURL());
        OtherWorld->BeginPlay();
        const bool bPassed = Env->DoString(TEXT(R"(
            assert(not pcall(function() Timer.Set(function() end) end))
            local h = Timer.Set(function() end, 0, false, 100, false, World)
            assert(Timer.Clear(h))
        )"));
        GEngine->DestroyWorldContext(OtherWorld);
        OtherWorld->DestroyWorld(false);
        TEST_TRUE(bPassed);
    });

    It(TEXT("rejects invalid timing and handles without allocating callbacks"), EAsyncExecution::TaskGraphMainThread, [this]
    {
        TEST_TRUE(Env->DoString(TEXT(R"(
            for _, value in ipairs({-1, math.huge, -math.huge, 0/0, '1', {}}) do
                assert(not pcall(function() Timer.Set(function() end, value, false, 0, false, World) end))
                assert(not pcall(function() Timer.Set(function() end, 0, false, value, false, World) end))
                assert(not Timer.Clear(value))
            end
            assert(not Timer.Clear(1.5))
            assert(Timer.Count(World) == 0)
        )")));
    });
}

#endif

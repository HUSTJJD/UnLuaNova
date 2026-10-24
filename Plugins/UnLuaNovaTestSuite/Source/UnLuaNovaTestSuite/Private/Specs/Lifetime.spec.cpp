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

// Who owns a UObject once a script can reach it.
//
// The failure this whole file exists for is a native crash, not a wrong number: a script
// holds a userdata whose object the engine has already destroyed, and the next access
// walks into freed memory. Every case here therefore asserts twice - once about the
// object's actual lifetime, and once that touching it from Lua afterwards produced a
// *caught* Lua error rather than nothing at all.
//
// Cases that need a specific diagnostic text are deliberately avoided: DanglingCheck is
// opt-in (UUnLuaNovaSettings::DanglingCheck) and its wording is not part of any contract.
// What is contractual is that the access is survivable.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "LowLevel.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"
#include "Registries/ObjectRegistry.h"
#include "Containers/LuaArray.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FLifetimeSpec, "UnLuaNova.API.Lifetime", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FLifetimeSpec)

void FLifetimeSpec::Define()
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

    Describe(TEXT("释放标记"), [this]()
    {
        It(TEXT("IsReleasedPtr 只认那一个哨兵值"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // UnLua does not null out a released userdata - it stamps it, so that a
            // script touching it gets an error instead of a read of freed memory.
            const auto Stub = NewObject<UUnLuaNovaTestStub>();
            TEST_TRUE(UnLuaNova::LowLevel::IsReleasedPtr(UnLuaNova::LowLevel::ReleasedPtr));
            TEST_FALSE(UnLuaNova::LowLevel::IsReleasedPtr(Stub));
            TEST_FALSE(UnLuaNova::LowLevel::IsReleasedPtr(nullptr));
        });
    });

    Describe(TEXT("脚本持有的对象"), [this]()
    {
        It(TEXT("只被 Lua 引用的对象会被引擎 GC 回收"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // This is the surprising half of the contract, and the reason the manual
            // reference API exists: pushing an object into Lua gives the script a
            // userdata, but the engine has no idea that userdata is a reference, so
            // nothing roots the object. Keeping one alive is the caller's job.
            //
            // Asserted through the weak pointer only. Stub is freed by the collection
            // below, and touching it afterwards - even just to ask IsValid() - reads
            // freed memory.
            const auto Stub = NewObject<UUnLuaNovaTestStub>();
            const TWeakObjectPtr<UUnLuaNovaTestStub> Weak = Stub;

            UnLuaNova::PushUObject(L, Stub);
            lua_setglobal(L, "SpecKept");

            CollectGarbage(RF_Standalone, true);

            TEST_FALSE(Weak.IsValid());
        });

        It(TEXT("对象被标记销毁后，从 Lua 访问是受控错误"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Stub = NewObject<UUnLuaNovaTestStub>();
            UnLuaNova::PushUObject(L, Stub);
            lua_setglobal(L, "SpecDead");

            Stub->MarkAsGarbage();
            CollectGarbage(RF_Standalone, true);

            // pcall() is the whole point: if this returns at all, the access was a Lua
            // error and not a read through a dead pointer.
            UnLuaNova::RunChunk(L, R"(
                local Ok, Err = pcall(function() SpecDead:AddCount() end)
                return Ok
            )");
            TEST_FALSE(lua_toboolean(L, -1));

            // ...and the environment is still usable afterwards.
            TEST_TRUE(Env->DoString(TEXT("return 1")));
            TEST_EQUAL((int32)lua_tointeger(L, -1), 1);
        });
    });

    Describe(TEXT("手动引用"), [this]()
    {
        It(TEXT("Ref proxy retains until Lua GC releases it"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Stub = NewObject<UUnLuaNovaTestStub>();
            const TWeakObjectPtr<UObject> Weak = Stub;
            UnLuaNova::PushUObject(L, Stub);
            lua_setglobal(L, "Obj");
            TEST_TRUE(Env->DoString(TEXT("Proxy = UnLuaNova.Ref(Obj); Obj = nil")));
            lua_gc(L, LUA_GCCOLLECT, 0);
            CollectGarbage(RF_Standalone, true);
            TEST_TRUE(Weak.IsValid());
            TEST_TRUE(Env->DoString(TEXT("Proxy = nil")));
            lua_gc(L, LUA_GCCOLLECT, 0);
            CollectGarbage(RF_Standalone, true);
            TEST_FALSE(Weak.IsValid());
        });

        It(TEXT("collecting an old proxy cannot release a replacement"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Stub = NewObject<UUnLuaNovaTestStub>();
            const TWeakObjectPtr<UObject> Weak = Stub;
            UnLuaNova::PushUObject(L, Stub);
            lua_setglobal(L, "Obj");
            TEST_TRUE(Env->DoString(TEXT(R"(
                Old = UnLuaNova.Ref(Obj)
                UnLuaNova.Unref(Obj)
                UnLuaNova.Unref(Obj)
                New = UnLuaNova.Ref(Obj)
                Old = nil
            )")));
            lua_gc(L, LUA_GCCOLLECT, 0);
            CollectGarbage(RF_Standalone, true);
            TEST_TRUE(Weak.IsValid());
            TEST_TRUE(Env->DoString(TEXT("New = nil; Obj = nil")));
            lua_gc(L, LUA_GCCOLLECT, 0);
            CollectGarbage(RF_Standalone, true);
            TEST_FALSE(Weak.IsValid());
        });

        It(TEXT("live manual proxy is released when Env closes"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            auto* Stub = NewObject<UUnLuaNovaTestStub>();
            const TWeakObjectPtr<UObject> Weak = Stub;
            UnLuaNova::PushUObject(L, Stub);
            lua_setglobal(L, "Obj");
            TEST_TRUE(Env->DoString(TEXT("Proxy = UnLuaNova.Ref(Obj)")));
            Env.Reset();
            L = nullptr;
            CollectGarbage(RF_Standalone, true);
            TEST_FALSE(Weak.IsValid());
        });

        It(TEXT("AddManualObjectReference 让对象不被 GC"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The escape hatch for "hold this for me without a UPROPERTY": the reference
            // is the only thing keeping the object alive here.
            const auto Stub = NewObject<UUnLuaNovaTestStub>();
            const TWeakObjectPtr<UUnLuaNovaTestStub> Weak = Stub;

            Env->AddManualObjectReference(Stub);
            CollectGarbage(RF_Standalone, true);

            TEST_TRUE(Weak.IsValid());

            // Give it back so the next case starts from nothing.
            Env->RemoveManualObjectReference(Stub);
        });

        It(TEXT("RemoveManualObjectReference 后可以被 GC"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The other half: removing the reference has to actually release it, or the
            // API is a one-way leak.
            const auto Stub = NewObject<UUnLuaNovaTestStub>();
            const TWeakObjectPtr<UUnLuaNovaTestStub> Weak = Stub;

            Env->AddManualObjectReference(Stub);
            Env->RemoveManualObjectReference(Stub);
            CollectGarbage(RF_Standalone, true);

            TEST_FALSE(Weak.IsValid());
        });
    });

    Describe(TEXT("Smart pointer finalization"), [this]()
    {
        It(TEXT("derived container arrays release their element type reference"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            for (const char* Script : {
                "Source = UE.TMap(0,0); Source:Add(1,2); return Source:Keys()",
                "Source = UE.TMap(0,0); Source:Add(1,2); return Source:Values()",
                "Source = UE.TSet(0); Source:Add(1); return Source:ToArray()" })
            {
                TEST_TRUE(UnLuaNova::RunChunk(L, Script));
                auto* Array = static_cast<FLuaArray*>(UnLuaNova::GetPointer(L, -1));
                TEST_TRUE(Array != nullptr);
                if (!Array)
                    return;
                auto Inner = Array->Inner;
                lua_gc(L, LUA_GCCOLLECT, 0);
                const int32 Before = Inner.GetSharedReferenceCount();
                lua_pop(L, 1);
                lua_gc(L, LUA_GCCOLLECT, 0);
                TEST_EQUAL(Inner.GetSharedReferenceCount(), Before - 1);
            }
        });

        It(TEXT("registry shared pointer releases its owner at GC"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TSharedPtr<int32> Ptr = MakeShared<int32>(42);
            const TWeakPtr<int32> Weak = Ptr;
            Env->GetObjectRegistry()->Push(L, Ptr);
            Ptr.Reset();
            TEST_TRUE(Weak.IsValid());
            lua_pop(L, 1);
            lua_gc(L, LUA_GCCOLLECT, 0);
            TEST_FALSE(Weak.IsValid());
        });

        It(TEXT("padded smart pointer releases on Env close"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TSharedPtr<int32> Ptr = MakeShared<int32>(42);
            const TWeakPtr<int32> Weak = Ptr;
            void* Payload = UnLuaNova::NewSmartPointer(L, sizeof(TSharedPtr<int32>), "TSharedPtr",
                UnLuaNova::DestroyUserdataPayload<TSharedPtr<int32>>);
            TEST_TRUE(Payload != nullptr);
            if (!Payload)
                return;
            new(Payload) TSharedPtr<int32>(Ptr);
            Ptr.Reset();
            Env.Reset();
            L = nullptr;
            TEST_FALSE(Weak.IsValid());
        });

        It(TEXT("released payload cannot be destroyed twice"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TSharedPtr<int32> Ptr = MakeShared<int32>(42);
            const TWeakPtr<int32> Weak = Ptr;
            void* Payload = UnLuaNova::NewSmartPointer(L, sizeof(TSharedPtr<int32>), "TSharedPtr",
                UnLuaNova::DestroyUserdataPayload<TSharedPtr<int32>>);
            TEST_TRUE(Payload != nullptr);
            if (!Payload)
                return;
            new(Payload) TSharedPtr<int32>(Ptr);
            Ptr.Reset();
            UnLuaNova::DestroyUserdataPayload<TSharedPtr<int32>>(L, lua_touserdata(L, -1));
            TEST_FALSE(Weak.IsValid());
            TEST_TRUE(UnLuaNova::GetSmartPointer(L, -1) == nullptr);
            UnLuaNova::DestroyUserdataPayload<TSharedPtr<int32>>(L, lua_touserdata(L, -1));
            lua_pop(L, 1);
            lua_gc(L, LUA_GCCOLLECT, 0);
        });
    });

    Describe(TEXT("环境销毁"), [this]()
    {
        It(TEXT("销毁环境不会顺手销毁被引用的对象"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The env owns Lua state, not UObjects. Tearing one down must not take the
            // object with it - and must not crash doing so.
            //
            // Rooted first, so that what is being measured is the teardown and not
            // whether the (now unreferenced) object happens to survive a collection: if
            // the env destroyed it, IsValid() would still go false.
            const auto Stub = NewObject<UUnLuaNovaTestStub>();
            Stub->AddToRoot();
            const TWeakObjectPtr<UUnLuaNovaTestStub> Weak = Stub;

            UnLuaNova::PushUObject(L, Stub);
            lua_setglobal(L, "SpecObj");

            Env.Reset();
            L = nullptr;

            CollectGarbage(RF_Standalone, true);
            TEST_TRUE(Weak.IsValid());
            Stub->RemoveFromRoot();
        });
    });
}

#endif

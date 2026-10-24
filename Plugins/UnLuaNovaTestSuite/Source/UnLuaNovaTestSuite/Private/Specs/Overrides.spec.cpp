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

// Overriding: what makes a UFunction replaceable, and what a script's implementation
// actually does once it is in place.
//
// The contract this file pins down has three halves, and the middle one is the one that
// breaks silently:
//   1. which functions may be overridden (events yes, plain callables no);
//   2. that after binding, a call really reaches the Lua implementation - and after
//      RestoreOverrides really does not;
//   3. that suspend/resume round-trips.
//
// Overriding mutates the UClass, not the object, so every case must undo it. AfterEach
// restores; without that, whichever case runs next inherits someone else's override and
// the suite starts reporting results that depend on ordering.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "LuaFunction.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuaNovaOverridesSpec, "UnLuaNova.API.Overrides", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    UnLuaNova::FLuaEnv* Env;
    lua_State* L;

    // Reads a global the fixture writes, so a case can tell whether Lua ran at all
    // rather than inferring it from a return value that might coincidentally match.
    int32 GlobalCalls()
    {
        lua_getglobal(L, "OverridesStubGlobalCalls");
        const int32 Value = (int32)lua_tointeger(L, -1);
        lua_pop(L, 1);
        return Value;
    }
END_DEFINE_SPEC(FUnLuaNovaOverridesSpec)

void FUnLuaNovaOverridesSpec::Define()
{
    BeforeEach([this]
    {
        // Not an env of our own. The dispatch thunk (ULuaFunction::execCallLua) resolves
        // the env with IUnLuaNovaModule::Get().GetEnv(Context) - the env locator's answer,
        // not the one TryBind was called on. Building a private FLuaEnv here therefore
        // binds the object where nothing will ever call it: the override is installed,
        // the call silently reaches the original implementation, and every case below
        // measures zero. Taking the env the thunk will take is the only way both halves
        // of the contract are observed in the same VM.
        UnLuaNova::Startup();
        Env = IUnLuaNovaModule::Get().GetEnv();
        L = Env->GetMainState();

        // The fixture counts calls in a global, so a reused env (Startup() early-returns
        // when a preceding case left the module active) would hand a case someone else's
        // total. Zeroing it here keeps every count below a count of this case.
        Env->DoString(TEXT("_G.OverridesStubGlobalCalls = 0"));
        Env->DoString(TEXT("_G.OverridesStubShouldError = false"));
    });

    AfterEach([this]
    {
        // Before the env goes away: restoring walks the overridden UFunctions, and the
        // override's Lua side is held by this env.
        ULuaFunction::RestoreOverrides(UUnLuaNovaTestStubForOverrides::StaticClass());
        UnLuaNova::Shutdown();
        Env = nullptr;
        L = nullptr;
    });

    Describe(TEXT("可覆写性"), [this]()
    {
        It(TEXT("BlueprintImplementableEvent 可覆写，BlueprintCallable 不可覆写"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The gate IsOverridable is the only thing standing between a script and an
            // arbitrary UFunction; if it admitted plain callables, a typo in a Lua module
            // would silently replace engine behaviour.
            const UFunction* Event = UUnLuaNovaTestStubForOverrides::StaticClass()->FindFunctionByName(TEXT("Overridable"));
            const UFunction* Plain = UUnLuaNovaTestStubForOverrides::StaticClass()->FindFunctionByName(TEXT("NotOverridable"));

            TEST_TRUE(Event != nullptr);
            TEST_TRUE(Plain != nullptr);
            TEST_TRUE(ULuaFunction::IsOverridable(Event));
            TEST_FALSE(ULuaFunction::IsOverridable(Plain));
        });

        It(TEXT("GetOverridableFunctions 只列出可覆写的函数"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            TMap<FName, UFunction*> Functions;
            ULuaFunction::GetOverridableFunctions(UUnLuaNovaTestStubForOverrides::StaticClass(), Functions);

            TEST_TRUE(Functions.Contains(FName(TEXT("Overridable"))));
            TEST_FALSE(Functions.Contains(FName(TEXT("NotOverridable"))));
        });

        It(TEXT("判定规则是两条分支，FUNC_Net 被第二条排除"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // IsOverridable is `FUNC_BlueprintEvent || (flags & (Native|Event|Net)) == (Native|Event)`.
            // The fixture only reaches the first branch, so the second one - the one that
            // made Lua-declared RPCs impossible until they got their own path - had no
            // coverage at all. Built by hand here: no UHT, no fixture change.
            const auto MakeFunction = [](uint32 Flags) -> UFunction*
            {
                auto* Function = NewObject<UFunction>(GetTransientPackage(), TEXT("SpecFlagProbe"), RF_Transient);
                Function->FunctionFlags = static_cast<EFunctionFlags>(Flags);
                Function->StaticLink(true);
                return Function;
            };

            const uint32 NativeEvent = FUNC_Public | FUNC_Native | FUNC_Event;
            UFunction* A = MakeFunction(NativeEvent);
            UFunction* B = MakeFunction(NativeEvent | FUNC_Net);
            UFunction* C = MakeFunction(FUNC_Public | FUNC_BlueprintEvent | FUNC_Net);

            TEST_TRUE(ULuaFunction::IsOverridable(A));
            // Same flags plus FUNC_Net: rejected. This is why a script could only re-implement
            // an RPC the engine had already declared, never declare one of its own.
            TEST_FALSE(ULuaFunction::IsOverridable(B));
            // The first branch does not look at FUNC_Net at all, so it still wins.
            TEST_TRUE(ULuaFunction::IsOverridable(C));
        });
    });

    Describe(TEXT("新增函数的路径（bAddNew）"), [this]()
    {
        It(TEXT("outer 不是目标类时函数被挂到目标类上"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // FLuaOverrides::Override takes the bAddNew branch when the source function's
            // outer is not the class - which is the whole mechanism behind Lua-declared
            // RPCs: build a UFunction, then hand it to the same override path. Nothing in
            // this suite drove that branch; every case above overrides a function the class
            // already owns.
            auto* Function = NewObject<UFunction>(GetTransientPackage(), TEXT("SpecAddedFunction"), RF_Transient);
            Function->FunctionFlags = FUNC_Public | FUNC_BlueprintEvent;
            Function->StaticLink(true);
            TEST_TRUE(Function->GetOuter() != UUnLuaNovaTestStubForOverrides::StaticClass());

            TEST_TRUE(ULuaFunction::Override(Function, UUnLuaNovaTestStubForOverrides::StaticClass(), FName(TEXT("SpecAddedFunction"))));

            UFunction* Added = UUnLuaNovaTestStubForOverrides::StaticClass()->FindFunctionByName(TEXT("SpecAddedFunction"));
            TEST_TRUE(Added != nullptr);
            TEST_TRUE(ULuaFunction::Get(Added) != nullptr);

            // ...and it comes back off: RestoreOverrides is what makes PIE exit - and this
            // spec's AfterEach - leave no trace on the UClass.
            ULuaFunction::RestoreOverrides(UUnLuaNovaTestStubForOverrides::StaticClass());
            TEST_TRUE(UUnLuaNovaTestStubForOverrides::StaticClass()->FindFunctionByName(TEXT("SpecAddedFunction")) == nullptr);
        });

        It(TEXT("同一个类的第二个对象不会二次覆写"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto First = NewObject<UUnLuaNovaTestStubForOverrides>();
            First->AddToRoot();
            TEST_TRUE(Env->TryBind(First));

            UFunction* Func = First->GetClass()->FindFunctionByName(TEXT("Overridable"));
            ULuaFunction* After = ULuaFunction::Get(Func);
            TEST_TRUE(After != nullptr);

            const auto Second = NewObject<UUnLuaNovaTestStubForOverrides>();
            Second->AddToRoot();
            TEST_TRUE(Env->TryBind(Second));

            // The override is on the UClass, so binding a second object must find the
            // existing ULuaFunction and re-initialise it, not add another one. Two entries
            // would mean whichever was installed last wins and the other leaks.
            TEST_EQUAL(ULuaFunction::Get(Func), After);

            First->RemoveFromRoot();
            Second->RemoveFromRoot();
        });
    });

    Describe(TEXT("覆写与还原"), [this]()
    {
        It(TEXT("绑定后 Overridable 被换成了 ULuaFunction"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();
            Stub->AddToRoot();

            TEST_TRUE(Env->TryBind(Stub));

            UFunction* Func = Stub->GetClass()->FindFunctionByName(TEXT("Overridable"));
            const ULuaFunction* Overridden = ULuaFunction::Get(Func);

            TEST_TRUE(Overridden != nullptr);
            // The original has to survive somewhere, or self.Overridden.X(self) has
            // nothing to call and "keep the Blueprint implementation" is a lie.
            TEST_TRUE(Overridden != nullptr && Overridden->GetOverridden() != nullptr);

            Stub->RemoveFromRoot();
        });

        It(TEXT("RestoreOverrides 之后不再是覆写状态"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();
            Stub->AddToRoot();
            TEST_TRUE(Env->TryBind(Stub));

            UFunction* Func = Stub->GetClass()->FindFunctionByName(TEXT("Overridable"));
            TEST_TRUE(ULuaFunction::Get(Func) != nullptr);

            ULuaFunction::RestoreOverrides(UUnLuaNovaTestStubForOverrides::StaticClass());
            TEST_TRUE(ULuaFunction::Get(Func) == nullptr);

            Stub->RemoveFromRoot();
        });
    });

    Describe(TEXT("分发"), [this]()
    {
        It(TEXT("覆写后调用走 Lua 实现"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();
            Stub->AddToRoot();
            TEST_TRUE(Env->TryBind(Stub));

            const int32 Result = Stub->Overridable();

            // The fixture returns the per-object counter after incrementing it, so both
            // "Lua ran" and "it ran with this object as self" are visible at once.
            TEST_EQUAL(Result, 1);
            TEST_EQUAL(Stub->Counter, 1);
            TEST_EQUAL(GlobalCalls(), 1);

            Stub->RemoveFromRoot();
        });

        It(TEXT("Suspend 后走原实现，Resume 后又走 Lua"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();
            Stub->AddToRoot();
            TEST_TRUE(Env->TryBind(Stub));

            TEST_EQUAL(Stub->Overridable(), 1);

            ULuaFunction::SuspendOverrides(UUnLuaNovaTestStubForOverrides::StaticClass());
            // The original is an empty BlueprintImplementableEvent: it returns the default
            // value and must not have reached Lua.
            TEST_EQUAL(Stub->Overridable(), 0);
            TEST_EQUAL(GlobalCalls(), 1);

            ULuaFunction::ResumeOverrides(UUnLuaNovaTestStubForOverrides::StaticClass());
            TEST_EQUAL(Stub->Overridable(), 2);
            TEST_EQUAL(GlobalCalls(), 2);

            Stub->RemoveFromRoot();
        });

        It(TEXT("两个对象的覆写互不影响"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The override is on the UClass, so this is really "self is the right object":
            // one shared Lua implementation, two separate pieces of state.
            const auto First = NewObject<UUnLuaNovaTestStubForOverrides>();
            const auto Second = NewObject<UUnLuaNovaTestStubForOverrides>();
            First->AddToRoot();
            Second->AddToRoot();

            TEST_TRUE(Env->TryBind(First));
            TEST_TRUE(Env->TryBind(Second));

            TEST_EQUAL(First->Overridable(), 1);
            TEST_EQUAL(Second->Overridable(), 1);
            TEST_EQUAL(First->Overridable(), 2);

            TEST_EQUAL(First->Counter, 2);
            TEST_EQUAL(Second->Counter, 1);
            TEST_EQUAL(GlobalCalls(), 3);

            First->RemoveFromRoot();
            Second->RemoveFromRoot();
        });

        It(TEXT("覆写内部报错不会崩，之后对象仍可调用"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // This is the branch that most easily becomes a crash: a raise inside a thunk
            // that the engine called. What matters is that it is survivable and reported.
            const auto Stub = NewObject<UUnLuaNovaTestStubForOverrides>();
            Stub->AddToRoot();
            TEST_TRUE(Env->TryBind(Stub));

            AddExpectedError(TEXT("override boom"), EAutomationExpectedErrorFlags::Contains);

            Env->DoString(TEXT("_G.OverridesStubShouldError = true"));
            const int32 Result = Stub->Overridable();
            TEST_EQUAL(Result, 0);
            TEST_EQUAL(Stub->Counter, 0);

            // ...and the object is not poisoned by having raised once.
            Env->DoString(TEXT("_G.OverridesStubShouldError = false"));
            TEST_EQUAL(Stub->Overridable(), 1);

            Stub->RemoveFromRoot();
        });
    });
}

#endif

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

// FDanglingCheck had no cases at all. Writing them meant first pinning down what it
// actually does, because the name suggests something it is not:
//
//   It is NOT a scanner that reports a count of dangling pointers. There is no Run() and
//   no number to assert on.
//
//   What it is: a per-call guard. Every C++-driven entry into Lua (RunChunk, DoString,
//   CallLuaFunction, an override dispatch) holds an FGuard. While one is held, every
//   container or struct handed to Lua for the *first* time is recorded (CaptureContainer /
//   CaptureStruct - see the call sites in LuaCore.cpp and UnLuaNovaBase.cpp). When the
//   outermost guard drops, each recorded address is looked up in the registry's
//   ScriptContainerMap / StructMap, tagged as released (or its two-level pointer nulled)
//   and removed from the map. A script still holding that userdata then gets a clean error
//   instead of reading freed memory.
//
// So the observable, assertable effect is: **the registry entry is gone once the call
// ends** - and only when the check is enabled. That is what the cases below read, from
// C++, straight out of Lua's registry. The paired control (disabled) is what proves the
// removal came from the check rather than from something else.
//
// `DanglingCheck` defaults to false in UUnLuaNovaSettings, so this is an opt-in mode. It
// is off by default precisely because it is strict: anything Lua first touched during a
// call is treated as transient and released when the call returns.

#include "UnLuaNovaBase.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"
#include "UnLuaNovaSettings.h"

#include "LuaEnv.h"

namespace
{
	bool IsContainerCached(lua_State* L, void* Key)
	{
		if (!L || !Key)
			return false;

		lua_getfield(L, LUA_REGISTRYINDEX, "ScriptContainerMap");
		lua_pushlightuserdata(L, Key);
		lua_rawget(L, -2);
		const bool bPresent = !lua_isnil(L, -1);
		lua_pop(L, 2);
		return bPresent;
	}
}

BEGIN_DEFINE_SPEC(FDanglingCheckSpec, "UnLuaNova.API.DanglingCheck", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
	TSharedPtr<UnLuaNova::FLuaEnv> Env;
	lua_State* L;
	bool bSavedEnabled = false;

	/** The container address a case touched, so AfterEach can leave the registry as it was. */
	void* TouchedContainer = nullptr;
END_DEFINE_SPEC(FDanglingCheckSpec)

void FDanglingCheckSpec::Define()
{
	BeforeEach([this]
	{
		Env = MakeShared<UnLuaNova::FLuaEnv>();
		L = Env->GetMainState();
		bSavedEnabled = UnLuaNova::FDanglingCheck::Enabled;
		TouchedContainer = nullptr;
	});

	AfterEach([this]
	{
		UnLuaNova::FDanglingCheck::Enabled = bSavedEnabled;
		Env.Reset();
		L = nullptr;
		TouchedContainer = nullptr;
	});

	Describe(TEXT("开关"), [this]()
	{
		It(TEXT("默认关闭，关闭时 guard 是空的"), EAsyncExecution::TaskGraphMainThread, [this]()
		{
			// The switch matters because the mechanism is strict: it releases anything Lua
			// first touched during a call. Nobody gets that by accident, so the default is
			// asserted on the settings object - the source of truth - rather than on the
			// static flag, which another spec could have flipped.
			TEST_FALSE(GetDefault<UUnLuaNovaSettings>()->DanglingCheck);

			UnLuaNova::FDanglingCheck::Enabled = false;
			TUniquePtr<UnLuaNova::FDanglingCheck::FGuard> Off = Env->GetDanglingCheck()->MakeGuard();
			TEST_FALSE(Off.IsValid());

			UnLuaNova::FDanglingCheck::Enabled = true;
			TUniquePtr<UnLuaNova::FDanglingCheck::FGuard> On = Env->GetDanglingCheck()->MakeGuard();
			TEST_TRUE(On.IsValid());
		});
	});

	Describe(TEXT("调用结束时释放"), [this]()
	{
		It(TEXT("开启后，容器在调用返回时从 ScriptContainerMap 摘掉"), EAsyncExecution::TaskGraphMainThread, [this]()
		{
			UnLuaNova::FDanglingCheck::Enabled = true;

			const auto Stub = NewObject<UUnLuaNovaTestStub>();
			Stub->AddToRoot();

			FProperty* Property = UUnLuaNovaTestStub::StaticClass()->FindPropertyByName(TEXT("MapForIssue407"));
			TEST_TRUE(Property != nullptr);
			if (!Property)
			{
				Stub->RemoveFromRoot();
				return;
			}
			TouchedContainer = Property->ContainerPtrToValuePtr<void>(Stub);

			UnLuaNova::PushUObject(L, Stub, false);
			lua_setglobal(L, "SpecStub");

			// RunChunk holds a guard, so the first touch of this container during it is
			// captured - and released when RunChunk returns.
			TEST_TRUE(UnLuaNova::RunChunk(L, "SpecHeldMap = SpecStub.MapForIssue407"));

			TEST_FALSE(IsContainerCached(L, TouchedContainer));

			Stub->RemoveFromRoot();
		});

		It(TEXT("关闭时留在缓存里（对照）"), EAsyncExecution::TaskGraphMainThread, [this]()
		{
			// The control for the case above: same steps, switch off. If this entry also
			// disappeared then the previous case proved nothing - something else would be
			// clearing the map.
			UnLuaNova::FDanglingCheck::Enabled = false;

			const auto Stub = NewObject<UUnLuaNovaTestStub>();
			Stub->AddToRoot();

			FProperty* Property = UUnLuaNovaTestStub::StaticClass()->FindPropertyByName(TEXT("MapForIssue407"));
			TEST_TRUE(Property != nullptr);
			if (!Property)
			{
				Stub->RemoveFromRoot();
				return;
			}
			TouchedContainer = Property->ContainerPtrToValuePtr<void>(Stub);

			UnLuaNova::PushUObject(L, Stub, false);
			lua_setglobal(L, "SpecStub");

			TEST_TRUE(UnLuaNova::RunChunk(L, "SpecHeldMap = SpecStub.MapForIssue407"));

			TEST_TRUE(IsContainerCached(L, TouchedContainer));

			Stub->RemoveFromRoot();
		});
	});
}

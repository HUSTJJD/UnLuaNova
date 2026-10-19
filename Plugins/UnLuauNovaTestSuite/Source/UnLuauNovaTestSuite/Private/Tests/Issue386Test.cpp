// Tencent is pleased to support the open source community by making UnLuauNova available.
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

#include "UnLuauNovaDelegates.h"
#include "UnLuauNovaTestCommon.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#if LUA_COMPILE_AS_CPP

static bool bTestClassDestructorCalled = false;

class FTestClass
{
public:
	~FTestClass()
	{
		bTestClassDestructorCalled = true;
	}
};

static int TestLuaError(lua_State* L)
{
	FTestClass Test;
	luaL_error(L, "foo");
	return 0;
}


struct FUnLuauNovaTest_Issue386 : FUnLuauNovaTestBase
{
	virtual bool InstantTest() override
	{
		return true;
	}

	virtual bool SetUp() override
	{
		bTestClassDestructorCalled = false;

		FUnLuauNovaDelegates::OnLuaStateCreated.AddLambda([](lua_State* InL)
		{
			lua_register(InL, "TestLuaError", TestLuaError);
		});

		FUnLuauNovaTestBase::SetUp();

		GetTestRunner().AddExpectedError("foo");
		UnLuauNova::RunChunk(L, "TestLuaError()");
		RUNNER_TEST_TRUE(bTestClassDestructorCalled);
		return true;
	}
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuauNovaTest_Issue386, TEXT("UnLuauNova.Regression.Issue386 C++类的析构会在luaerror后被跳过"))

#endif

#endif //WITH_DEV_AUTOMATION_TESTS

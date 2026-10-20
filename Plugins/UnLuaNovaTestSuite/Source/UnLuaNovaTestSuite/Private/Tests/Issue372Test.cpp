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

#include "TestCommands.h"
#include "UnLuaNovaTestCommon.h"
#include "LuaBackend.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

    // Walks the registry for the entry that is the object at ObjIdx - what the chunk used
    // to do with pairs(debug.getregistry()). Luau has no debug.getregistry() and no C API
    // that hands the registry to a script, so there the chunk died on its first line and
    // the case never ran. LUA_REGISTRYINDEX is reachable from C++ on both backends.
    //
    // lua_compare rather than lua_rawequal: the entry is the object's Lua table while
    // `Actor` is the userdata bound to it, and it is the __eq on that userdata that says
    // the two are the same object - the same comparison `v == Actor` used to make.
    static int32 FindRegistryRef(lua_State* L, int32 ObjIdx)
    {
        lua_pushnil(L);                                 // [k]
        while (lua_next(L, LUA_REGISTRYINDEX) != 0)     // [k, v]
        {
            if (lua_compare(L, -1, ObjIdx, LUA_OPEQ) != 0)
            {
                const int32 Ref = (int32)UNLUANOVA_TOINT64(L, -2);
                lua_pop(L, 2);                          // [k, v] -> []
                return Ref;
            }
            lua_pop(L, 1);                              // [k] - the key lua_next needs
        }
        return LUA_NOREF;                               // lua_next already took the key
    }

BEGIN_TESTSUITE(FIssue372Test, TEXT("UnLuaNova.Regression.Issue372 Actor调用K2_DestroyActor销毁时，lua部分会在注册表残留"));

    bool FIssue372Test::RunTest(const FString& Parameters)
    {
        const auto MapName = TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue372/Issue372");
        ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(MapName))
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this] {
            const auto L = UnLuaNova::GetState();
            UnLuaNova::PushUObject(L, GWorld);
            lua_setglobal(L, "World");

            UnLuaNova::PushUObject(L, GWorld);

            const auto Chunk = R"(
            local ActorClass = UE.UClass.Load("/UnLuaNovaTestSuite/Tests/Binding/BP_UnLuaNovaTestActor_StaticBinding.BP_UnLuaTestActor_StaticBinding_C")
            Actor = World:SpawnActor(ActorClass)
            )";
            UnLuaNova::RunChunk(L, Chunk);

            const int32 Top = lua_gettop(L);
            lua_getglobal(L, "Actor");
            auto Ref = FindRegistryRef(L, lua_gettop(L));
            lua_settop(L, Top);

            TestNotEqual(TEXT("Ref"), (double)Ref, (double)LUA_NOREF);

            lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
            auto Actual = lua_type(L, -1);
            TestEqual(TEXT("Actual"), Actual, LUA_TTABLE);
            lua_pop(L, 1);

            UnLuaNova::RunChunk(L, "Actor:K2_DestroyActor()");
            CollectGarbage(RF_NoFlags, true);

            lua_rawgeti(L, LUA_REGISTRYINDEX, Ref);
            Actual = lua_type(L, -1);
            TestNotEqual(TEXT("Actual"), (double)Actual, (double)LUA_TTABLE);
            lua_pop(L, 1);
            return true;
            }));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
        return true;
    }

END_TESTSUITE(FRegression_Issue276)

#endif

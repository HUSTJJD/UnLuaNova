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
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_TESTSUITE(FIssue295Test, TEXT("UnLuaNova.Regression.Issue295 动态绑定的UMG对象，在切换地图回调Destruct导致崩溃"))

    bool FIssue295Test::RunTest(const FString& Parameters)
    {
        const auto MapName1 = TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue295/Issue295_1");
        const auto MapName2 = TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue295/Issue295_2");

        ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(MapName1))
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this] {
            const auto L = UnLuaNova::GetState();
            UnLuaNova::PushUObject(L, GWorld);
            lua_setglobal(L, "GWorld");

            const char* Chunk = R"(
            -- The file was renamed to UnLuaNovaTestUMG_Issue295, but the class *inside* the
            -- asset is still UnLuaTestUMG_Issue295_C: renaming a .uasset on disk does not
            -- rename the BlueprintGeneratedClass it holds, and that object is what Load()
            -- has to find.
            local UMGClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue295/UnLuaNovaTestUMG_Issue295.UnLuaTestUMG_Issue295_C') 
            local Outer = GWorld
            
            local UMG1 = NewObject(UMGClass, Outer, 'UMG1', 'Tests.Regression.Issue295.TestUMG')
            UMG1:AddToViewport()
            
            local UMG2 = NewObject(UMGClass, Outer, 'UMG2', 'Tests.Regression.Issue295.TestUMG')
            UMG2:AddToViewport()
            )";
            UnLuaNova::RunChunk(L, Chunk);

            lua_gc(L, LUA_GCCOLLECT, 0);
            CollectGarbage(RF_NoFlags, true);
            return true;
            }));
        ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(MapName2))
        ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this] {
            const auto L = UnLuaNova::GetState();
            UnLuaNova::PushUObject(L, GWorld);
            lua_setglobal(L, "GWorld");

            const char* Chunk = R"(
            -- The file was renamed to UnLuaNovaTestUMG_Issue295, but the class *inside* the
            -- asset is still UnLuaTestUMG_Issue295_C: renaming a .uasset on disk does not
            -- rename the BlueprintGeneratedClass it holds, and that object is what Load()
            -- has to find.
            local UMGClass = UE.UClass.Load('/UnLuaNovaTestSuite/Tests/Regression/Issue295/UnLuaNovaTestUMG_Issue295.UnLuaTestUMG_Issue295_C') 
            local Outer = GWorld
            
            local UMG1 = NewObject(UMGClass, Outer, 'UMG1', 'Tests.Regression.Issue295.TestUMG')
            UMG1:AddToViewport()
            
            local UMG2 = NewObject(UMGClass, Outer, 'UMG2', 'Tests.Regression.Issue295.TestUMG')
            UMG2:AddToViewport()
            )";
            UnLuaNova::RunChunk(L, Chunk);
            return true;
            }));
        ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
        return true;
    }

END_TESTSUITE(FRegression_Issue276)

#endif

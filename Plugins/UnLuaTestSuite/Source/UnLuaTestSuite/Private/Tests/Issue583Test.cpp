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


#include "Issue583Test.h"
#include "LowLevel.h"
#include "UnLuauNovaSettings.h"
#include "UnLuauNovaTestCommon.h"
#include "UnLuauNovaTestHelpers.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

struct FUnLuauNovaTest_Issue583 : FUnLuauNovaTestBase
{
    virtual bool InstantTest() override
    {
        return true;
    }

    virtual bool SetUp() override
    {
        FUnLuauNovaTestBase::SetUp();

        const auto Chunk = R"(
            local DataTable = UE.UObject.Load('/UnLuauNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583.DataTable_Issue583')
            local Row = UE.FIssue583Row()
            local Result = UE.UDataTableFunctionLibrary.GetDataTableRowFromName(DataTable, 'Row_1', Row)
            local Records = Row.Records:Values()
            for i=1, Records:Length() do
                local Record = Records:Get(i)
                print(Record.Description)
            end
        )";
        UnLuauNova::RunChunk(L, Chunk);
        return true;
    }
};

IMPLEMENT_UNLUA_INSTANT_TEST(FUnLuauNovaTest_Issue583, TEXT("UnLuauNova.Regression.Issue583 在Lua中遍历TMap字段的Values接口返回值时引起的崩溃"))

#endif

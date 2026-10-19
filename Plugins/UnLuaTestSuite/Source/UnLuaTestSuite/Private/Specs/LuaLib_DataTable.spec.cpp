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

#include "UnLuauNovaBase.h"
#include "UnLuauNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuauNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuauNovaLibDataTableSpec, "UnLuauNova.API.DataTable", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
END_DEFINE_SPEC(FUnLuauNovaLibDataTableSpec)

void FUnLuauNovaLibDataTableSpec::Define()
{
    BeforeEach([this]
    {
        UnLuauNova::Startup();
        L = UnLuauNova::GetState();
    });

    Describe(TEXT("GetTableDataRowFromName"), [this]()
    {
        It(TEXT("获取数据表中指定行的数据（蓝图）"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Chunk = R"(
            local DataTable = UE.UObject.Load('/UnLuauNovaTestSuite/Tests/Misc/DataTable_BPTest.DataTable_BPTest')
            local RowStruct = UE.UObject.Load('/UnLuauNovaTestSuite/Tests/Misc/Struct_TableRow.Struct_TableRow')
            local Row = RowStruct()
            local Result = UE.UDataTableFunctionLibrary.GetDataTableRowFromName(DataTable, 'Row_1', Row)
            return Row.TestString
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_EQUAL(lua_tostring(L, -1), FString("A"));
        });

        It(TEXT("获取数据表中指定行的数据（C++）"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const auto Chunk = R"(
            local DataTable = UE.UObject.Load('/UnLuauNovaTestSuite/Tests/Misc/DataTable_CppTest.DataTable_CppTest')
            local Row = UE.FUnLuauNovaTestTableRow()
            local Result = UE.UDataTableFunctionLibrary.GetDataTableRowFromName(DataTable, 'Row_1', Row)
            return Row.Title
            )";
            UnLuauNova::RunChunk(L, Chunk);
            TEST_EQUAL(lua_tostring(L, -1), FString("Hello"));
        });
    });

    AfterEach([this]
    {
        UnLuauNova::Shutdown();
    });
}

#endif

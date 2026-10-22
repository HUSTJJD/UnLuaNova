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

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FUnLuaNovaLibSetSpec, "UnLuaNova.API.TSet", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    lua_State* L;
END_DEFINE_SPEC(FUnLuaNovaLibSetSpec)

void FUnLuaNovaLibSetSpec::Define()
{
    BeforeEach([this]
    {
        UnLuaNova::Startup();
        L = UnLuaNova::GetState();
    });

    Describe(TEXT("构造TSet"), [this]()
    {
        It(TEXT("构造TSet<int32>"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            return Set\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Set = (TSet<int32>*)UnLuaNova::GetSet(L, -1);
            TEST_EQUAL(Set->Num(), 2);
            TEST_TRUE(Set->Contains(1));
            TEST_TRUE(Set->Contains(2));
        });

        It(TEXT("构造TSet<FString>"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet('')\
            Set:Add('A')\
            Set:Add('B')\
            return Set\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Set = (TSet<FString>*)UnLuaNova::GetSet(L, -1);
            TEST_EQUAL(Set->Num(), 2);
            TEST_TRUE(Set->Contains("A"));
            TEST_TRUE(Set->Contains("B"));
        });

        It(TEXT("构造TSet<FVector>"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(UE.FVector)\
            Set:Add(UE.FVector(1,1,1))\
            Set:Add(UE.FVector(2,2,2))\
            return Set\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Set = (TSet<FVector>*)UnLuaNova::GetSet(L, -1);
            TEST_EQUAL(Set->Num(), 2);
            TEST_TRUE(Set->Contains(FVector(1,1,1)));
            TEST_TRUE(Set->Contains(FVector(2,2,2)));
        });

        It(TEXT("构造TSet<UScriptStruct>"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = R"(
            local Struct_TableRow = UE.UObject.Load("/UnLuaNovaTestSuite/Tests/Misc/Struct_TableRow.Struct_TableRow")
            local Set = UE.TSet(Struct_TableRow)

            local Item1 = Struct_TableRow()
            Item1.TestFloat = 1
            local Item2 = Struct_TableRow()
            Item2.TestFloat = 2

            Set:Add(Item1)
            Set:Add(Item2)

            return Set
            )";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Set = UnLuaNova::GetSet(L, -1);
            TEST_EQUAL(Set->Num(), 2);
        });
    });

    Describe(TEXT("Length"), [this]()
    {
        It(TEXT("获取Set长度"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            return Set:Length()\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 2LL);
        });
    });

    Describe(TEXT("Add"), [this]()
    {
        It(TEXT("元素不存在，新增"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            return Set\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Set = (TSet<int32>*)UnLuaNova::GetSet(L, -1);
            TEST_EQUAL(Set->Num(), 2);
            TEST_TRUE(Set->Contains(1));
            TEST_TRUE(Set->Contains(2));
        });

        It(TEXT("不增加重复的元素"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(1)\
            return Set\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Set = (TSet<int32>*)UnLuaNova::GetSet(L, -1);
            TEST_EQUAL(Set->Num(), 1);
            TEST_TRUE(Set->Contains(1));
        });
    });

    Describe(TEXT("Remove"), [this]()
    {
        It(TEXT("元素不存在，返回false"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            return Set:Remove(2)\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_FALSE(lua_toboolean(L, -1));
        });

        It(TEXT("元素存在，移除并返回true"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            return Set:Remove(1), Set\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Set = (TSet<int32>*)UnLuaNova::GetSet(L, -1);
            TEST_EQUAL(Set->Num(), 0);
            TEST_TRUE(lua_toboolean(L, -2));
        });
    });

    Describe(TEXT("Contains"), [this]()
    {
        It(TEXT("查找元素存在，返回true"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            return Set:Contains(1)\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("查找元素不存在，返回false"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            return Set:Contains(2)\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_FALSE(lua_toboolean(L, -1));
        });
    });

    Describe(TEXT("Clear"), [this]()
    {
        It(TEXT("清除，移除所有元素"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            Set:Clear()\
            return Set:Length()\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 0LL);
        });
    });

    Describe(TEXT("ToArray"), [this]()
    {
        It(TEXT("获取所有元素的数组"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            local Ret = UE.TArray(0)\
            Ret:Append(Set:ToArray())\
            return Ret\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            const auto Array = (TArray<int32>*)UnLuaNova::GetArray(L, -1);
            TEST_EQUAL(Array->Num(), 2);
            TEST_TRUE(Array->Contains(1));
            TEST_TRUE(Array->Contains(2));
        });
    });

    Describe(TEXT("ToTable"), [this]()
    {
        It(TEXT("将Set内容转为LuaTable"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            local Table = Set:ToTable()\
            return Table[1], Table[2]\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 2LL);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -2), 1LL);
        });
    });

    Describe(TEXT("pairs"), [this]()
    {
        It(TEXT("迭代拿到Set里的每个元素"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // pairs(Set) yields (element, true): a set has no key of its own, so the element
            // is what a script wants - the same value C++ range-for hands out, and the same
            // value ToTable() would have given it. TSet registered no iteration entry at all
            // until now (__iter on Luau, __pairs on Lua 5.x), so this raised on both backends
            // and ToArray / ToTable were the only way to walk one.
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            local Count = 0\
            local Sum = 0\
            for Element, Flag in pairs(Set) do\
                Count = Count + 1\
                Sum = Sum + Element\
            end\
            return Count, Sum\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -2), 2LL);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 3LL);
        });

        It(TEXT("删除元素后迭代不会带回空洞"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // A TSet leaves holes where elements were removed, so an iterator that simply
            // increments the index either hands back dead slots or stops early. Both are
            // silent - the element just never shows up.
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            Set:Add(3)\
            Set:Remove(2)\
            local Count = 0\
            local Sum = 0\
            for Element in pairs(Set) do\
                Count = Count + 1\
                Sum = Sum + Element\
            end\
            return Count, Sum\
            ";
            UnLuaNova::RunChunk(L, Chunk);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -2), 2LL);
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 4LL);
        });

        It(TEXT("迭代中删除元素不崩，且删除真的生效"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Mutating a container while an iterator is walking it is where a naive
            // iterator either crashes or quietly skips an element. What is asserted is
            // that the walk survives and the container ends up consistent - how many
            // elements the walk happens to visit is not a contract anyone should rely on.
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            Set:Add(3)\
            for Element in pairs(Set) do\
                if Element == 2 then Set:Remove(2) end\
            end\
            return Set:Length(), Set:Contains(2), Set:Contains(1)\
            ";
            TEST_TRUE(UnLuaNova::RunChunk(L, Chunk));
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -3), 2LL);
            TEST_FALSE(lua_toboolean(L, -2));
            TEST_TRUE(lua_toboolean(L, -1));
        });

        It(TEXT("迭代中新增元素不崩，且新增真的生效"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The other half: an Add can reallocate the set's storage under the iterator.
            const char* Chunk = "\
            local Set = UE.TSet(0)\
            Set:Add(1)\
            Set:Add(2)\
            for Element in pairs(Set) do\
                if Element == 1 then Set:Add(99) end\
            end\
            return Set:Contains(99), Set:Length()\
            ";
            TEST_TRUE(UnLuaNova::RunChunk(L, Chunk));
            TEST_TRUE(lua_toboolean(L, -2));
            TEST_EQUAL(UNLUANOVA_TOINT64(L, -1), 3LL);
        });
    });

    AfterEach([this]
    {
        UnLuaNova::Shutdown();
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS

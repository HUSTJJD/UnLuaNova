// Tencent is pleased to support the open source community by making UnLuaNova available.
//
// Copyright (C) 2019 Tencent. All rights reserved.
//
// Licensed under the MIT License (the "License");
// you may obtain a copy of the License at
//
// http://opensource.org/MIT
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and limitations under the License.

// Apples-to-apples rows against slua-unreal's published TestPerf set.
//
// Benchmark.spec.cpp measures what UnLuaNova cares about and names the rows after that.
// This file is the other half: the same case names slua-unreal publishes in its README
// (TestEmptyCall, TestGetIntCall, TestSetStrVar, ...), same iteration count, same
// machine - so the two projects' numbers can finally be put in one table.
//
// The point is not to win that table. It is that until now the only cross-project claim
// available was "slua's reflection is 0.54 us/call and ours is 1.44 us/call", taken from
// two different machines, two engine versions and two test harnesses. That difference is
// either a 2.6x marshalling overhead worth fixing or an artifact of the comparison, and
// nothing in the repository could tell you which. These rows can.
//
// Read them with GenerateTestReport.py next to latest-<Backend>.csv; the case asserts
// nothing about duration, for the same reason Benchmark.spec.cpp does not.

#include "UnLuaNovaBase.h"
#include "LuaBackend.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Perfs/UnLuaNovaBenchmarkProxy.h"

#if WITH_DEV_AUTOMATION_TESTS

// 100000 calls per row, matching slua's published table, so the per-op column can be
// read straight across without rescaling.
static constexpr int32 ParityIterations = 100000;

// Container rows grow the container every iteration, so they get the smaller budget the
// main benchmark spec already uses for the same reason.
static constexpr int32 ParityContainerIterations = 20000;

static const TCHAR* ParityBackendTag()
{
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
    return TEXT("Luau");
#else
    return TEXT("Lua");
#endif
}

BEGIN_DEFINE_SPEC(FBenchmarkParitySpec, "UnLuaNova.Perf.BenchmarkParity", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;

    struct FRow
    {
        FString Case;
        int32 Iterations;
        double TotalMs;
    };

    TArray<FRow> Rows;

    void Measure(const FString& Case, int32 Iterations, TFunctionRef<void()> Body)
    {
        const double Start = FPlatformTime::Seconds();
        Body();
        Rows.Add({Case, Iterations, (FPlatformTime::Seconds() - Start) * 1000.0});
    }

    // One chunk, one loop, inside the script - the same rule as the main benchmark: the
    // loop has to live in Lua or the timing measures compilation.
    void MeasureChunk(const FString& Case, int32 Iterations, const TCHAR* Chunk)
    {
        Measure(Case, Iterations, [this, Chunk]()
        {
            Env->DoString(Chunk);
        });
    }
END_DEFINE_SPEC(FBenchmarkParitySpec)

void FBenchmarkParitySpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
        Rows.Reset();
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });

    It(TEXT("把 slua 同名用例的耗时写入 Saved/Benchmark"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        const auto Proxy = NewObject<AUnLuaBenchmarkProxy>();
        Proxy->AddToRoot();

        UnLuaNova::PushUObject(L, Proxy);
        lua_setglobal(L, "SpecProxy");
        lua_newtable(L);
        lua_setglobal(L, "SpecResult");

        // ---- function calls --------------------------------------------------
        // Named after slua's TestPerf set: TestEmptyCall, TestGetIntCall, ...
        MeasureChunk(TEXT("TestEmptyCall"), ParityIterations,
            TEXT("for i = 1, 100000 do SpecProxy:NOP() end"));

        MeasureChunk(TEXT("TestGetIntCall"), ParityIterations,
            TEXT("local s = 0 for i = 1, 100000 do s = s + SpecProxy:GetMeshID() end SpecResult[1] = s"));

        MeasureChunk(TEXT("TestSetIntCall"), ParityIterations,
            TEXT("for i = 1, 100000 do SpecProxy:UpdateMeshID(i) end"));

        MeasureChunk(TEXT("TestGetStrCall"), ParityIterations,
            TEXT("local n for i = 1, 100000 do n = SpecProxy:GetMeshName() end SpecResult[2] = n"));

        MeasureChunk(TEXT("TestSetStrCall"), ParityIterations,
            TEXT("for i = 1, 100000 do SpecProxy:UpdateMeshName(\"benchmark\") end"));

        MeasureChunk(TEXT("TestGetVectorCall"), ParityIterations,
            TEXT("local v for i = 1, 100000 do v = SpecProxy:GetCOM() end SpecResult[3] = v"));

        MeasureChunk(TEXT("TestGetBoolCall"), ParityIterations,
            TEXT("local b = false for i = 1, 100000 do b = SpecProxy:Raycast(UE.FVector(0,0,0), UE.FVector(0,0,1)) end SpecResult[4] = b"));

        // ---- property access -------------------------------------------------
        MeasureChunk(TEXT("TestSetIntVar"), ParityIterations,
            TEXT("for i = 1, 100000 do SpecProxy.MeshID = i end"));

        MeasureChunk(TEXT("TestGetIntVar"), ParityIterations,
            TEXT("local s = 0 for i = 1, 100000 do s = s + SpecProxy.MeshID end SpecResult[5] = s"));

        MeasureChunk(TEXT("TestSetStrVar"), ParityIterations,
            TEXT("for i = 1, 100000 do SpecProxy.MeshName = \"benchmark\" end"));

        MeasureChunk(TEXT("TestGetStrVar"), ParityIterations,
            TEXT("local n for i = 1, 100000 do n = SpecProxy.MeshName end SpecResult[6] = n"));

        MeasureChunk(TEXT("TestSetVectorVar"), ParityIterations,
            TEXT("for i = 1, 100000 do SpecProxy.COM = UE.FVector(1, 2, 3) end"));

        MeasureChunk(TEXT("TestGetVectorVar"), ParityIterations,
            TEXT("local v for i = 1, 100000 do v = SpecProxy.COM end SpecResult[7] = v"));

        // ---- containers ------------------------------------------------------
        MeasureChunk(TEXT("TestAddArrayElement"), ParityContainerIterations,
            TEXT("for i = 1, 20000 do local t = SpecProxy.Indices t:Add(i) end"));

        MeasureChunk(TEXT("TestGetArrayElement"), ParityContainerIterations,
            TEXT("local s = 0 local t = SpecProxy.Indices for i = 1, 20000 do s = s + (t[i] or 0) end SpecResult[8] = s"));

        Proxy->RemoveFromRoot();

        // ---- two reference rows ----------------------------------------------
        // slua's table is all reflection. These two repeat TestEmptyCall / TestGetIntCall
        // through ProcessEvent directly, so the CSV can separate "cost of the UE call"
        // from "cost of UnLuaNova's marshalling on top of it" - the split that decides
        // whether there is anything to optimise.
        {
            UFunction* Nop = Proxy->GetClass()->FindFunctionByName(TEXT("NOP"));
            Measure(TEXT("Ref.TestEmptyCall"), ParityIterations, [Proxy, Nop]()
            {
                for (int32 i = 0; i < ParityIterations; ++i)
                    if (Nop) Proxy->ProcessEvent(Nop, nullptr);
            });

            UFunction* GetMeshID = Proxy->GetClass()->FindFunctionByName(TEXT("GetMeshID"));
            Measure(TEXT("Ref.TestGetIntCall"), ParityIterations, [Proxy, GetMeshID]()
            {
                struct { int32 ReturnValue; } Params;
                for (int32 i = 0; i < ParityIterations; ++i)
                {
                    Params.ReturnValue = 0;
                    if (GetMeshID) Proxy->ProcessEvent(GetMeshID, &Params);
                }
            });
        }

        // ---- CSV ---------------------------------------------------------------
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("Benchmark");
        IFileManager::Get().MakeDirectory(*Dir, true);

        const FString Latest = Dir / FString::Printf(TEXT("parity-latest-%s.csv"), ParityBackendTag());

        TArray<FString> Lines;
        Lines.Add(TEXT("Case,Iterations,TotalMs,PerOpUs"));
        for (const FRow& Row : Rows)
        {
            const double PerOpUs = Row.Iterations > 0 ? (Row.TotalMs * 1000.0) / Row.Iterations : 0.0;
            Lines.Add(FString::Printf(TEXT("%s,%d,%.3f,%.4f"), *Row.Case, Row.Iterations, Row.TotalMs, PerOpUs));
        }

        const bool bWritten = FFileHelper::SaveStringToFile(
            FString::Join(Lines, TEXT("\n")) + TEXT("\n"), *Latest,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        TEST_TRUE(bWritten);

        // 15 slua 同名行（13 调用 / 属性 + 2 容器）加 2 行 ProcessEvent 裸开销参照。
        TEST_EQUAL(Rows.Num(), 17);
        for (const FRow& Row : Rows)
        {
            TestTrue(*FString::Printf(TEXT("%s 跑了 %d 次"), *Row.Case, Row.Iterations), Row.Iterations > 0);
            TestTrue(*FString::Printf(TEXT("%s 耗时非零"), *Row.Case), Row.TotalMs > 0.0);
        }
    });
}

#endif //WITH_DEV_AUTOMATION_TESTS

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

// Turns the benchmark into an automation case that leaves a CSV behind.
//
// The benchmark used to be a level you opened by hand and read off the screen. Numbers
// nobody commits are not a baseline, they are a screenshot, so the point of this file is
// the CSV: GenerateTestReport.py reads Saved/Benchmark/ and compares against the previous
// run, which is what makes "this change made calls slower" visible at all.
//
// What it asserts is deliberately weak - that work happened, took time, and was recorded.
// Asserting an absolute duration would make the suite red on a loaded CI machine, and a
// flaky case is worse than no case. The comparison lives in the report, where a 30%
// regression is a warning and not a failure.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Perfs/UnLuaNovaBenchmarkProxy.h"

#if WITH_DEV_AUTOMATION_TESTS

// Small enough that the case stays a few seconds; large enough that a single outlier
// iteration cannot move the average.
static constexpr int32 SpecIterations = 20000;

BEGIN_DEFINE_SPEC(FBenchmarkSpec, "UnLuaNova.Perf.Benchmark", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;

    struct FRow
    {
        FString Case;
        int32 Iterations;
        double TotalMs;
    };

    TArray<FRow> Rows;

    // Body does its own looping and Iterations only labels how many calls it made. That
    // matters for the Lua row: the loop has to live inside the chunk, or the timing
    // measures chunk compilation rather than the call - which would make the per-op number
    // meaningless and the run-to-run comparison pure noise.
    void Measure(const FString& Case, int32 Iterations, TFunctionRef<void()> Body)
    {
        const double Start = FPlatformTime::Seconds();
        Body();
        Rows.Add({Case, Iterations, (FPlatformTime::Seconds() - Start) * 1000.0});
    }
END_DEFINE_SPEC(FBenchmarkSpec)

void FBenchmarkSpec::Define()
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

    It(TEXT("把调用耗时写入 Saved/Benchmark"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        const auto Proxy = NewObject<AUnLuaBenchmarkProxy>();
        Proxy->AddToRoot();

        Measure(TEXT("Native"), SpecIterations, [Proxy, this]()
        {
            for (int32 i = 0; i < SpecIterations; ++i)
                Proxy->UpdateMeshID(i);
        });

        Measure(TEXT("Reflection"), SpecIterations, [Proxy]()
        {
            // Same call, resolved through UFunction every time: the gap between this and
            // the row above is what static binding exists to close.
            struct FParams { int32 NewID; int32 ReturnValue; } Params;
            UFunction* Func = Proxy->GetClass()->FindFunctionByName(TEXT("UpdateMeshID"));
            for (int32 i = 0; i < SpecIterations; ++i)
            {
                Params.NewID = i;
                Params.ReturnValue = 0;
                if (Func)
                    Proxy->ProcessEvent(Func, &Params);
            }
        });

        UnLuaNova::PushUObject(L, Proxy);
        lua_setglobal(L, "SpecProxy");
        Measure(TEXT("Lua"), SpecIterations, [this]()
        {
            Env->DoString(FString::Printf(TEXT(
                "for i = 1, %d do SpecProxy:UpdateMeshID(i) end"), SpecIterations));
        });

        Proxy->RemoveFromRoot();

        // ---- write the CSV -------------------------------------------------
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("Benchmark");
        IFileManager::Get().MakeDirectory(*Dir, true);

        const FString Latest = Dir / TEXT("latest.csv");
        const FString Previous = Dir / TEXT("previous.csv");

        // latest -> previous before overwriting, so the report always has something to
        // compare against.
        IFileManager::Get().Delete(*Previous, false, true);
        IFileManager::Get().Move(*Previous, *Latest);

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

        // Sanity: work happened and was measured. Not an absolute duration - see the file
        // comment.
        TEST_EQUAL(Rows.Num(), 3);
        for (const FRow& Row : Rows)
        {
            TestTrue(*FString::Printf(TEXT("%s 跑了 %d 次"), *Row.Case, Row.Iterations), Row.Iterations > 0);
            TestTrue(*FString::Printf(TEXT("%s 耗时非零"), *Row.Case), Row.TotalMs > 0.0);
        }
    });
}

#endif

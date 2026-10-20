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

// Record per-backend timings and raw samples in the current run directory. The committed
// independent reference supplies the 25% gate; short VM loops are repeated within each
// timed sample so a single millisecond-scale interruption cannot dominate its average.

#include "UnLuaNovaBase.h"
// LuaBackend.h is where UNLUANOVA_LUA_BACKEND is defined, and it is not reachable from
// UnLuaNovaBase.h. Without this include the #if below silently evaluates an undefined macro
// as 0, matches UNLUANOVA_BACKEND_LUAU and tags a Lua 5.5 build "Luau" - which is exactly
// the mix-up this tag exists to prevent.
#include "LuaBackend.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/App.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Dom/JsonObject.h"
#include "Perfs/UnLuaNovaBenchmarkProxy.h"

#if WITH_DEV_AUTOMATION_TESTS

// Small enough that the case stays a few seconds; large enough that a single outlier
// iteration cannot move the average.
static constexpr int32 SpecIterations = 20000;

// The VM rows are pure script work, two orders of magnitude cheaper per iteration than a
// cross-language call, so they need a proportionally longer run to clear timer noise.
static constexpr int32 VmIterations = 200000;

static constexpr int32 ContainerIterations = 20000;
static constexpr int32 PerfSampleCount = 5;
static constexpr double PerfWarmupMs = 250.0;
static constexpr double PerfSampleMs = 100.0;

static FORCENOINLINE int32 CallNativeBenchmark(AUnLuaBenchmarkProxy* Proxy, int32 Value)
{
    return Proxy->UpdateMeshID(Value);
}

// One container iteration is "build a TArray<FVector>, push 8 FVectors, hand the whole thing
// to a UFunction" - roughly an order of magnitude more work than the single call the
// SpecIterations rows measure, so it gets its own count to stay in the same few-seconds
// budget. (The hand-run version of this case in the benchmark level uses 1024*1024; that is
// fine when a human is waiting for a number on screen and not when CI runs it every build.)

// Which backend this build compiled, used to name the CSV. A run that cannot say which VM
// produced its numbers is not a baseline - see the file comment.
static const TCHAR* BackendTag()
{
#if UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU
    return TEXT("Luau");
#else
    return TEXT("Lua");
#endif
}

// How far a run may drift from the committed baseline before it counts as a regression.
//
// 25%: wide enough that a loaded machine or a different CPU does not make the suite red,
// narrow enough that "this change made calls ten times slower" is a failure here instead of
// a warning in a report nobody reads. Deliberately not 1.0 - a gate that fires on noise is
// a gate people learn to ignore, which is worse than no gate.
static constexpr double PerfBaselineTolerance = 1.25;

BEGIN_DEFINE_SPEC(FBenchmarkSpec, "UnLuaNova.Perf.Benchmark", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;

    struct FSample
    {
        double ElapsedMs;
        int32 Batches;
    };

    struct FRow
    {
        FString Case;
        int32 Iterations;
        double TotalMs;
        int32 WarmupBatches = 0;
        double WarmupMs = 0.0;
        TArray<FSample> Samples;
    };

    TArray<FRow> Rows;

    double PerOpUs(const FRow& Row) const
    {
        return Row.Iterations > 0 ? (Row.TotalMs * 1000.0) / Row.Iterations : 0.0;
    }

    bool WriteJson(const FString& Path) const;
    void CheckAgainstBaseline();

    // Body does its own looping and Iterations only labels how many calls it made. That
    // matters for the Lua row: the loop has to live inside the chunk, or the timing
    // measures chunk compilation rather than the call - which would make the per-op number
    // meaningless and the run-to-run comparison pure noise.
    void Measure(const FString& Case, int32 Iterations, TFunctionRef<void()> Body)
    {
        FRow Row;
        Row.Case = Case;
        Row.Iterations = Iterations;
        const double WarmupStart = FPlatformTime::Seconds();
        do
        {
            Body();
            ++Row.WarmupBatches;
            Row.WarmupMs = (FPlatformTime::Seconds() - WarmupStart) * 1000.0;
        } while (Row.WarmupMs < PerfWarmupMs);

        TArray<double, TInlineAllocator<PerfSampleCount>> BatchMeans;
        for (int32 Sample = 0; Sample < PerfSampleCount; ++Sample)
        {
            const double Start = FPlatformTime::Seconds();
            FSample Timing{0.0, 0};
            do
            {
                Body();
                ++Timing.Batches;
                Timing.ElapsedMs = (FPlatformTime::Seconds() - Start) * 1000.0;
            } while (Timing.ElapsedMs < PerfSampleMs);
            Row.Samples.Add(Timing);
            BatchMeans.Add(Timing.ElapsedMs / Timing.Batches);
        }
        BatchMeans.Sort();
        Row.TotalMs = BatchMeans[PerfSampleCount / 2];
        Rows.Add(MoveTemp(Row));
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
            volatile int32 Result = 0;
            for (int32 i = 0; i < SpecIterations; ++i)
                Result = CallNativeBenchmark(Proxy, i);
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

        // ---- pure VM work ----------------------------------------------------
        // The three rows above measure the crossing into C++: parameter marshalling and
        // UFunction resolution dominate, so both backends land in the same ballpark and the
        // numbers say nothing about the interpreter. These rows stay inside the chunk - same
        // rule as the Lua row, the loop has to be in the script or the timing is compile
        // time - and they are written in the syntax both VMs accept (no `continue`, no
        // compound assignment, no type annotations), so one chunk compiles either way.
        //
        // Each result is written into SpecResult instead of being left in a local: a value
        // nobody reads is dead code, and a compiler is free to delete the loop with it.
        lua_newtable(L);
        lua_setglobal(L, "SpecResult");

        Measure(TEXT("VM.Arithmetic"), VmIterations, [this]()
        {
            Env->DoString(FString::Printf(TEXT(
                "local s = 0.0\n"
                "for i = 1, %d do s = s + i * 0.5 end\n"
                "SpecResult[1] = s"), VmIterations));
        });

        Measure(TEXT("VM.Call"), VmIterations, [this]()
        {
            Env->DoString(FString::Printf(TEXT(
                "local function Add(a, b) return a + b end\n"
                "local s = 0.0\n"
                "for i = 1, %d do s = s + Add(i, 0.5) end\n"
                "SpecResult[2] = s"), VmIterations));
        });

        // Table work splits in two on purpose. Growing an empty table one key at a time is
        // rehash, not lookup, and the two have completely different consequences for a
        // script author: a VM that costs 40% more to *build* a table may cost nothing extra
        // to *read* one, and only the split says which it is. (Luau scripts can dodge the
        // growth cost entirely with table.create(n); there is no equivalent on Lua 5.5, so
        // preallocating is not an option in a chunk both backends have to compile.)
        Measure(TEXT("VM.TableGrow"), VmIterations, [this]()
        {
            Env->DoString(FString::Printf(TEXT(
                "local t = {}\n"
                "for i = 1, %d do t[i] = i end\n"
                "SpecResult.t = t"), VmIterations));
        });

        Measure(TEXT("VM.TableRead"), VmIterations, [this]()
        {
            Env->DoString(FString::Printf(TEXT(
                "local t = SpecResult.t\n"
                "if t == nil then SpecResult[3] = 0 return end\n"
                "local s = 0.0\n"
                "for i = 1, %d do s = s + t[i] end\n"
                "SpecResult[3] = s"), VmIterations));
        });

        // ---- container marshalling ----------------------------------------
        // TArray<FVector> built in Lua and passed to a UFunction as a whole: the cost is
        // split between constructing the container and marshalling it back across the
        // boundary, and only a row that does both per iteration says what a real script
        // pays. Same "no dead code" rule as the VM rows - the result is stored, not dropped.
        //
        // Written in the syntax both VMs accept (no `continue`, no compound assignment, no
        // type annotations) so one chunk compiles either way.
        Measure(TEXT("Lua.Container.PointsAreCoplanar"), ContainerIterations, [this]()
        {
            Env->DoString(FString::Printf(TEXT(
                "for i = 1, %d do\n"
                "  local TestVec = UE.TArray(UE.FVector)\n"
                "  TestVec:Add(UE.FVector(0, 0, 0))\n"
                "  TestVec:Add(UE.FVector(0, 0, 1))\n"
                "  TestVec:Add(UE.FVector(0, 1, 0))\n"
                "  TestVec:Add(UE.FVector(1, 0, 0))\n"
                "  TestVec:Add(UE.FVector(1, 1, 0))\n"
                "  TestVec:Add(UE.FVector(1, 0, 1))\n"
                "  TestVec:Add(UE.FVector(0, 1, 1))\n"
                "  TestVec:Add(UE.FVector(1, 1, 1))\n"
                "  SpecResult[4] = UE.UKismetMathLibrary.PointsAreCoplanar(TestVec, 0.1)\n"
                "end"), ContainerIterations));
        });

        Proxy->RemoveFromRoot();

        // ---- write the CSV -------------------------------------------------
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("Benchmark");
        IFileManager::Get().MakeDirectory(*Dir, true);

        // Per backend - see the file comment. Sharing one pair of files across backends
        // compares two different VMs and calls the difference a regression.
        const FString Latest = Dir / FString::Printf(TEXT("latest-%s.csv"), BackendTag());
        const FString Previous = Dir / FString::Printf(TEXT("previous-%s.csv"), BackendTag());

        // latest -> previous before overwriting, so the report always has something to
        // compare against. Only when there is a latest: Move() logs an error when the
        // source does not exist - which is the normal state the first time this runs -
        // and the automation framework turns a logged error into a failed case.
        if (IFileManager::Get().FileExists(*Latest))
        {
            IFileManager::Get().Delete(*Previous, false, true);
            IFileManager::Get().Move(*Previous, *Latest);
        }

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

        // ---- write the machine-readable run --------------------------------
        // The CSV is what the report reads; this is what a *committed* baseline can be
        // compared against. Written every run so `RunTests.ps1 -UpdateBaseline` has
        // something to copy - until it is copied in, the numbers this case produces live
        // only on this machine and no one else can check them.
        const FString JsonPath = Dir / FString::Printf(TEXT("perf-%s.json"), BackendTag());
        TEST_TRUE(WriteJson(JsonPath));
        FString ReportDirectory;
        if (FParse::Value(FCommandLine::Get(), TEXT("ReportExportPath="), ReportDirectory))
        {
            const FString Metadata = ReportDirectory / TEXT("metadata");
            IFileManager::Get().MakeDirectory(*Metadata, true);
            TEST_TRUE(WriteJson(Metadata / TEXT("perf.json")));
        }

        // ---- and hold it against the committed baseline ---------------------
        CheckAgainstBaseline();

        // Sanity: work happened and was measured. Not an absolute duration - see the file
        // comment. Eight rows: three crossing into C++, four staying inside the VM, one
        // building a container and marshalling it across.
        TEST_EQUAL(Rows.Num(), 8);
        for (const FRow& Row : Rows)
        {
            TestTrue(*FString::Printf(TEXT("%s 跑了 %d 次"), *Row.Case, Row.Iterations), Row.Iterations > 0);
            TestTrue(*FString::Printf(TEXT("%s 耗时非零"), *Row.Case), Row.TotalMs > 0.0);
        }
    });
}

bool FBenchmarkSpec::WriteJson(const FString& Path) const
{
    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

    // Recorded so a number can always be traced to where it came from. A baseline without
    // these is exactly what made the documented figures unverifiable: a duration with no
    // VM, no machine and no engine version behind it says nothing.
    Root->SetStringField(TEXT("Backend"), BackendTag());
    Root->SetStringField(TEXT("EngineVersion"), FEngineVersion::Current().ToString());
    Root->SetStringField(TEXT("MeasuredUtc"), FDateTime::UtcNow().ToIso8601());
    Root->SetStringField(TEXT("Machine"), FPlatformProcess::ComputerName());
    Root->SetNumberField(TEXT("Tolerance"), PerfBaselineTolerance);
    const TSharedRef<FJsonObject> Measurement = MakeShared<FJsonObject>();
    Measurement->SetStringField(TEXT("Method"), TEXT("median-of-5-batch-means"));
    Measurement->SetNumberField(TEXT("WarmupMinMs"), PerfWarmupMs);
    Measurement->SetNumberField(TEXT("SampleMinMs"), PerfSampleMs);
    Root->SetObjectField(TEXT("Measurement"), Measurement);

    TArray<TSharedPtr<FJsonValue>> Cases;
    for (const FRow& Row : Rows)
    {
        const TSharedRef<FJsonObject> Case = MakeShared<FJsonObject>();
        Case->SetStringField(TEXT("Case"), Row.Case);
        Case->SetNumberField(TEXT("Iterations"), Row.Iterations);
        Case->SetNumberField(TEXT("PerOpUs"), PerOpUs(Row));
        Case->SetNumberField(TEXT("WarmupBatches"), Row.WarmupBatches);
        Case->SetNumberField(TEXT("WarmupMs"), Row.WarmupMs);
        TArray<TSharedPtr<FJsonValue>> Samples;
        for (const FSample& Timing : Row.Samples)
        {
            const TSharedRef<FJsonObject> Sample = MakeShared<FJsonObject>();
            Sample->SetNumberField(TEXT("ElapsedMs"), Timing.ElapsedMs);
            Sample->SetNumberField(TEXT("Batches"), Timing.Batches);
            Samples.Add(MakeShared<FJsonValueObject>(Sample));
        }
        Case->SetArrayField(TEXT("Samples"), Samples);
        Cases.Add(MakeShared<FJsonValueObject>(Case));
    }
    Root->SetArrayField(TEXT("Cases"), Cases);

    FString Output;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
    if (!FJsonSerializer::Serialize(Root, Writer))
        return false;

    return FFileHelper::SaveStringToFile(Output, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FBenchmarkSpec::CheckAgainstBaseline()
{
    const FString Path = FPaths::ProjectDir() / TEXT("Tools") / TEXT("baseline")
        / FString::Printf(TEXT("perf-%s.json"), BackendTag());

    FString Text;
    if (!FPaths::FileExists(*Path) || !FFileHelper::LoadFileToString(Text, *Path))
    {
        // Not a failure on purpose: a baseline cannot exist before someone records one, and
        // failing outright would make the suite red on every clean checkout. It is a warning
        // because what it means is "this case currently proves nothing about performance".
        AddWarning(FString::Printf(
            TEXT("没有提交的性能基线（%s），这条用例现在只验证「跑过」，验证不了「没变慢」。跑一次 RunTests.ps1 -UpdateBaseline 并把产物提交进来。"),
            *Path));
        return;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        AddError(FString::Printf(TEXT("性能基线 %s 不是合法 JSON，无法比对。"), *Path));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Cases = nullptr;
    if (!Root->TryGetArrayField(TEXT("Cases"), Cases) || !Cases)
    {
        AddError(FString::Printf(TEXT("性能基线 %s 里没有 Cases 数组。"), *Path));
        return;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Cases)
    {
        const TSharedPtr<FJsonObject> Case = Value.IsValid() ? Value->AsObject() : nullptr;
        if (!Case.IsValid())
            continue;

        FString Name;
        double BaselineUs = 0.0;
        if (!Case->TryGetStringField(TEXT("Case"), Name) || !Case->TryGetNumberField(TEXT("PerOpUs"), BaselineUs))
            continue;

        const FRow* Row = Rows.FindByPredicate([&Name](const FRow& R) { return R.Case == Name; });
        if (!Row)
            continue;   // a case this run did not measure is not this run's problem

        const double ActualUs = PerOpUs(*Row);
        const double Limit = BaselineUs * PerfBaselineTolerance;
        TestTrue(*FString::Printf(TEXT("%s 每操作 %.4f µs，未超过基线 %.4f µs 的 %.0f%%"),
            *Name, ActualUs, BaselineUs, (PerfBaselineTolerance - 1.0) * 100.0),
            ActualUs <= Limit);
    }
}

#endif

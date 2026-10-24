// Licensed under the MIT License. See LICENSE in the repository root.

#include "LuaEnv.h"
#include "LuaMemoryStats.h"
#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTimerSubsystem.h"
#include "UnLuaNovaTestHelpers.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    // Only audited test environments pay for a pointer map. The ledger outlives lua_close.
    struct FVmLedger
    {
        TMap<UPTRINT, SIZE_T> Live;
        int64 Bytes = 0;
        int64 Allocations = 0;
        int32 Errors = 0;
    };

    class FTrackedEnv : public UnLuaNova::FLuaEnv
    {
    public:
        explicit FTrackedEnv(FVmLedger& Ledger) : FLuaEnv(&Allocate, &Ledger) {}

        static void* Allocate(void* Userdata, void* Pointer, size_t OldSize, size_t NewSize)
        {
            auto& Ledger = *static_cast<FVmLedger*>(Userdata);
            const UPTRINT OldAddress = reinterpret_cast<UPTRINT>(Pointer);
            void* Result = DefaultLuaAllocator(nullptr, Pointer, OldSize, NewSize);
            if (!Result && NewSize != 0) return nullptr;
            if (OldAddress)
            {
                SIZE_T RecordedSize = 0;
                if (!Ledger.Live.RemoveAndCopyValue(OldAddress, RecordedSize) || RecordedSize != OldSize)
                    ++Ledger.Errors;
                Ledger.Bytes -= static_cast<int64>(RecordedSize);
            }
            if (Result)
            {
                const UPTRINT Address = reinterpret_cast<UPTRINT>(Result);
                if (Ledger.Live.Contains(Address)) ++Ledger.Errors;
                Ledger.Live.Add(Address, NewSize);
                Ledger.Bytes += static_cast<int64>(NewSize);
                ++Ledger.Allocations;
            }
            return Result;
        }
    };

    struct FLeakResult
    {
        explicit FLeakResult(FAutomationTestBase& InTest, const TCHAR* InCase)
            : Test(InTest), Case(InCase), InitialEnvs(UnLuaNova::FLuaEnv::GetAll().Num()) {}

        void Check(bool Condition, const TCHAR* What)
        {
            Passed &= Test.TestTrue(What, Condition);
        }

        void Run(FTrackedEnv& Env, const TCHAR* Script)
        {
            Check(Env.DoString(Script, TEXT("MemoryLeakWorkload")), TEXT("Memory workload executed"));
            lua_settop(Env.GetMainState(), 0);
        }

        void Sample(const FVmLedger& Ledger)
        {
            auto Row = MakeShared<FJsonObject>();
            Row->SetNumberField(TEXT("bytes"), Ledger.Bytes);
            Row->SetNumberField(TEXT("blocks"), Ledger.Live.Num());
            Samples.Add(MakeShared<FJsonValueObject>(Row));
        }

        void Closed(const FVmLedger& Ledger)
        {
            ++ClosedEnvs;
            Allocations += Ledger.Allocations;
            RemainingBytes += Ledger.Bytes;
            RemainingBlocks += Ledger.Live.Num();
            AccountingErrors += Ledger.Errors;
            Check(Ledger.Allocations > 0, TEXT("Allocator observed real VM work"));
            Check(Ledger.Bytes == 0 && Ledger.Live.Num() == 0 && Ledger.Errors == 0,
                TEXT("Every VM allocation was released exactly once at close"));
        }

        void Resource(bool Released)
        {
            ++CheckedResources;
            if (!Released) ++RetainedResources;
            Check(Released, TEXT("Owned resource released"));
        }

        void Save()
        {
            const int32 FinalEnvs = UnLuaNova::FLuaEnv::GetAll().Num();
            Check(FinalEnvs == InitialEnvs, TEXT("Environment registry returned to its initial size"));
            auto Root = MakeShared<FJsonObject>();
            Root->SetNumberField(TEXT("schema_version"), 1);
            Root->SetStringField(TEXT("case"), Case);
            Root->SetStringField(TEXT("backend"), UNLUANOVA_LUA_BACKEND == UNLUANOVA_BACKEND_LUAU ? TEXT("Luau") : TEXT("Lua"));
            Root->SetBoolField(TEXT("editor"), GIsEditor);
            Root->SetBoolField(TEXT("cooked"), FPlatformProperties::RequiresCookedData());
            Root->SetStringField(TEXT("run_id"), FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_RELEASE_TOKEN")));
            Root->SetNumberField(TEXT("closed_envs"), ClosedEnvs);
            Root->SetNumberField(TEXT("allocation_events"), Allocations);
            Root->SetNumberField(TEXT("remaining_bytes"), RemainingBytes);
            Root->SetNumberField(TEXT("remaining_blocks"), RemainingBlocks);
            Root->SetNumberField(TEXT("accounting_errors"), AccountingErrors);
            Root->SetNumberField(TEXT("checked_resources"), CheckedResources);
            Root->SetNumberField(TEXT("retained_resources"), RetainedResources);
            Root->SetNumberField(TEXT("initial_envs"), InitialEnvs);
            Root->SetNumberField(TEXT("final_envs"), FinalEnvs);
            Root->SetNumberField(TEXT("warmup"), Warmup);
            Root->SetBoolField(TEXT("positive_control_detected"), PositiveControlDetected);
            Root->SetArrayField(TEXT("samples"), Samples);
            Root->SetBoolField(TEXT("passed"), Passed && !Test.HasAnyErrors());
            const FString Manifest = FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_TEST_MANIFEST"));
            const FString Directory = Manifest.IsEmpty()
                ? FPaths::ProjectSavedDir() / TEXT("MemoryTests")
                : FPaths::GetPath(Manifest) / TEXT("memory");
            IFileManager::Get().MakeDirectory(*Directory, true);
            FString Json;
            const bool Serialized = FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
            Check(Serialized && FFileHelper::SaveStringToFile(Json, *(Directory / (Case + TEXT(".json")))),
                TEXT("Memory evidence written"));
        }

        FAutomationTestBase& Test;
        FString Case;
        int32 InitialEnvs;
        int32 ClosedEnvs = 0;
        int64 Allocations = 0;
        int64 RemainingBytes = 0;
        int32 RemainingBlocks = 0;
        int32 AccountingErrors = 0;
        int32 CheckedResources = 0;
        int32 RetainedResources = 0;
        int32 Warmup = 0;
        bool PositiveControlDetected = false;
        bool Passed = true;
        TArray<TSharedPtr<FJsonValue>> Samples;
    };

    void CollectObjects()
    {
        CollectGarbage(RF_NoFlags, true);
        CollectGarbage(RF_NoFlags, true);
    }

    const TCHAR* HeapWorkload = TEXT(R"(
        local values = {}
        for i = 1, 256 do
            values[i] = {text = tostring(i), fn = function() return i end}
            local co = coroutine.create(function() coroutine.yield(i) end)
            assert(coroutine.resume(co))
        end
        local a, m, s = UE.TArray(0), UE.TMap(0, 0), UE.TSet(0)
        for i = 1, 64 do a:Add(i); m:Add(i, i); s:Add(i) end
        local keys, vals, entries = m:Keys(), m:Values(), s:ToArray()
        assert(keys:Length() == 64 and vals:Length() == 64 and entries:Length() == 64)
        local ok = pcall(function()
            local temporary = {string.rep('x', 4096)}
            error(temporary)
        end)
        assert(not ok)
    )");

    void SteadyWorkload(FLeakResult& Result, bool Reload)
    {
        FVmLedger Ledger;
        {
            FTrackedEnv Env(Ledger);
            if (Reload)
            {
                Result.Run(Env, TEXT(R"(
                    Hot = require('UnLuaNova.HotReload')
                    ReloadCount = 0
                    Hot.hook.module_loaded = function(_, name, reloading)
                        if name == 'MemoryReloadModule' and reloading then ReloadCount = ReloadCount + 1 end
                    end
                    table.insert(package.searchers, 1, function(name)
                        if name == 'MemoryReloadModule' then
                            return assert(load('local M = { HOT_RELOAD = true }; function M.Read() return 1 end; return M', name))
                        end
                    end)
                    Reloaded = Hot.require('MemoryReloadModule')
                    assert(Reloaded.Read() == 1)
                )"));
            }
            Result.Warmup = 8;
            int64 BaselineBytes = 0;
            int32 BaselineBlocks = 0;
            for (int32 Iteration = 0; Iteration < Result.Warmup + 32; ++Iteration)
            {
                Result.Run(Env, HeapWorkload);
                if (Reload)
                {
                    Result.Run(Env, TEXT(R"(
                        local old = Reloaded.Read
                        local count = ReloadCount
                        Hot.reload({'MemoryReloadModule'})
                        assert(ReloadCount == count + 1 and Reloaded.Read ~= old and Reloaded.Read() == 1)
                    )"));
                }
                lua_settop(Env.GetMainState(), 0);
                Env.GC();
                if (Iteration == Result.Warmup - 1)
                {
                    BaselineBytes = Ledger.Bytes;
                    BaselineBlocks = Ledger.Live.Num();
                }
                if (Iteration >= Result.Warmup)
                {
                    Result.Sample(Ledger);
                    Result.Check(Ledger.Bytes <= BaselineBytes + 8192 && Ledger.Live.Num() <= BaselineBlocks + 16,
                        TEXT("Post-GC heap remains within the warmed budget"));
                }
            }
        }
        Result.Closed(Ledger);
    }
}

BEGIN_DEFINE_SPEC(FMemoryLeaksSpec, "UnLuaNova.Memory", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
END_DEFINE_SPEC(FMemoryLeaksSpec)

void FMemoryLeaksSpec::Define()
{
    BeforeEach([] { CollectObjects(); });

    It(TEXT("Accounting preserves live bytes across enable and reset"), [this]
    {
        UnLuaNova::FLuaMemoryStats Stats;
        Stats.TrackAlloc(0, 128);
        Stats.bEnabled = true;
        Stats.Reset();
        TestEqual(TEXT("Existing allocations survive reset"), Stats.GetCurrentBytes(), uint64(128));
        Stats.TrackAlloc(128, 256);
        Stats.bEnabled = false;
        Stats.TrackFree(256);
        TestEqual(TEXT("Free while disabled balances bytes"), Stats.GetCurrentBytes(), uint64(0));
        TestEqual(TEXT("Enabled realloc counted"), Stats.GetReallocCount(), uint64(1));
        FVmLedger Ledger;
        auto& Global = UnLuaNova::FLuaMemoryStats::Get();
        const uint64 Before = Global.GetCurrentBytes();
        void* Block = FTrackedEnv::Allocate(&Ledger, nullptr, LUA_TTABLE, 128);
        TestEqual(TEXT("Fresh allocation ignores the Lua type tag"), Global.GetCurrentBytes(), Before + 128);
        FTrackedEnv::Allocate(&Ledger, Block, 128, 0);
        TestEqual(TEXT("Tagged allocation/free balances"), Global.GetCurrentBytes(), Before);
    });

    It(TEXT("vm_cycles"), [this]
    {
        FLeakResult Result(*this, TEXT("vm_cycles"));
        for (int32 Cycle = 0; Cycle < 24; ++Cycle)
        {
            FVmLedger Ledger;
            { FTrackedEnv Env(Ledger); Result.Run(Env, HeapWorkload); }
            Result.Closed(Ledger);
            Result.Sample(Ledger);
        }
        Result.Save();
    });

    It(TEXT("steady_heap"), [this]
    {
        FLeakResult Result(*this, TEXT("steady_heap"));
        SteadyWorkload(Result, false);
        Result.Save();
    });

    It(TEXT("hot_reload"), [this]
    {
        FLeakResult Result(*this, TEXT("hot_reload"));
        SteadyWorkload(Result, true);
        Result.Save();
    });

    It(TEXT("retained_reference"), [this]
    {
        FLeakResult Result(*this, TEXT("retained_reference"));
        FVmLedger Ledger;
        {
            FTrackedEnv Env(Ledger);
            Env.GC();
            Result.Sample(Ledger);
            const int64 Before = Ledger.Bytes;
            Result.Run(Env, TEXT("Retained = {}; for i = 1, 8192 do Retained[i] = {i} end"));
            Env.GC();
            Result.Sample(Ledger);
            Result.PositiveControlDetected = Ledger.Bytes > Before + 65536;
            Result.Check(Result.PositiveControlDetected, TEXT("Deliberately retained graph is detected after GC"));
            Result.Run(Env, TEXT("Retained = nil"));
            Env.GC();
            Result.Sample(Ledger);
            Result.Check(Ledger.Bytes <= Before + 8192, TEXT("Releasing the control graph restores the heap"));
        }
        Result.Closed(Ledger);
        Result.Save();
    });

    It(TEXT("native_payloads"), [this]
    {
        FLeakResult Result(*this, TEXT("native_payloads"));
        for (int32 Cycle = 0; Cycle < 24; ++Cycle)
        {
            FVmLedger Ledger;
            TWeakPtr<TArray<uint8>> Weak;
            {
                FTrackedEnv Env(Ledger);
                auto* L = Env.GetMainState();
                {
                    auto Payload = MakeShared<TArray<uint8>>();
                    Payload->SetNumUninitialized(65536);
                    Weak = Payload;
                    using FPayload = TSharedPtr<TArray<uint8>>;
                    void* Storage = UnLuaNova::NewSmartPointer(L, sizeof(FPayload), "TSharedPtr",
                        UnLuaNova::DestroyUserdataPayload<FPayload>);
                    if (Storage) new(Storage) FPayload(Payload);
                    Result.Check(Storage != nullptr, TEXT("Native payload registered"));
                }
                Result.Check(Weak.IsValid(), TEXT("VM owns the native payload before cleanup"));
                if (Cycle % 2 == 0)
                {
                    lua_settop(L, 0);
                    Env.GC();
                    Result.Resource(!Weak.IsValid());
                }
                // Container userdata owns native type descriptors as well as VM memory.
                // Keep the descriptor alive independently so its reference count is observable.
                const TCHAR* Sources[] = {
                    TEXT("Source = UE.TMap(0,0); Source:Add(1,2); return Source:Keys()"),
                    TEXT("Source = UE.TMap(0,0); Source:Add(1,2); return Source:Values()"),
                    TEXT("Source = UE.TSet(0); Source:Add(1); return Source:ToArray()")
                };
                const bool Created = Env.DoString(Sources[Cycle % 3]);
                Result.Check(Created, TEXT("Derived native container created"));
                auto* Array = Created ? static_cast<FLuaArray*>(UnLuaNova::GetPointer(L, -1)) : nullptr;
                Result.Check(Array != nullptr, TEXT("Container has a native payload"));
                if (Array)
                {
                    auto Inner = Array->Inner;
                    Env.GC();
                    const int32 References = Inner.GetSharedReferenceCount();
                    lua_pop(L, 1);
                    Env.GC();
                    Result.Resource(Inner.GetSharedReferenceCount() == References - 1);
                }
            }
            Result.Resource(!Weak.IsValid());
            Result.Closed(Ledger);
            Result.Sample(Ledger);
        }
        Result.Save();
    });

    It(TEXT("delegates"), [this]
    {
        FLeakResult Result(*this, TEXT("delegates"));
        for (int32 Cycle = 0; Cycle < 24; ++Cycle)
        {
            FVmLedger Ledger;
            TWeakObjectPtr<UObject> Weak;
            {
                FTrackedEnv Env(Ledger);
                auto* Stub = NewObject<UUnLuaNovaTestStub>();
                Weak = Stub;
                Env.AddManualObjectReference(Stub);
                UnLuaNova::PushUObject(Env.GetMainState(), Stub);
                lua_setglobal(Env.GetMainState(), "Stub");
                Result.Run(Env, TEXT(R"(
                    Stub.SimpleHandler:Bind(Stub, function() end)
                    Stub.SimpleEvent:Add(Stub, function() end)
                )"));
                TArray<TWeakObjectPtr<UObject>> Handlers;
                Handlers.Add(Stub->SimpleHandler.GetUObject());
                for (auto* Handler : Stub->SimpleEvent.GetAllObjects()) Handlers.Add(Handler);
                Result.Check(Handlers.Num() == 2 && Handlers[0].IsValid() && Handlers[1].IsValid(),
                    TEXT("Both delegate handlers were created"));
                Result.Run(Env, TEXT(R"(
                    Stub.SimpleHandler:Unbind()
                    Stub.SimpleEvent:Clear()
                    Stub = nil
                )"));
                Env.RemoveManualObjectReference(Stub);
                Env.GC();
                CollectObjects();
                Result.Resource(!Weak.IsValid());
                for (const auto& Handler : Handlers) Result.Resource(!Handler.IsValid());
            }
            Result.Closed(Ledger);
            Result.Sample(Ledger);
        }
        Result.Save();
    });

    It(TEXT("umg_resources"), [this]
    {
        FLeakResult Result(*this, TEXT("umg_resources"));
        for (int32 Cycle = 0; Cycle < 24; ++Cycle)
        {
            FVmLedger Ledger;
            TWeakObjectPtr<UObject> WeakImage, WeakTexture;
            {
                FTrackedEnv Env(Ledger);
                auto* Image = NewObject<UImage>();
                auto* Texture = NewObject<UTexture2D>();
                WeakImage = Image;
                WeakTexture = Texture;
                Image->SetBrushFromTexture(Texture);
                UnLuaNova::PushUObject(Env.GetMainState(), Image);
                lua_setglobal(Env.GetMainState(), "Image");
                Result.Run(Env, TEXT("ImageProxy = UnLuaNova.Ref(Image); Image = nil"));
                CollectObjects();
                Result.Check(WeakImage.IsValid() && WeakTexture.IsValid(), TEXT("Live image proxy retains its texture"));
                Result.Run(Env, TEXT("ImageProxy = nil"));
                Env.GC();
                CollectObjects();
                Result.Resource(!WeakImage.IsValid());
                Result.Resource(!WeakTexture.IsValid());
            }
            Result.Closed(Ledger);
            Result.Sample(Ledger);
        }
        Result.Save();
    });

#if UNLUANOVA_HAS_STDLIB_TIMER && UNLUANOVA_HAS_STDLIB_EVENT
    for (bool WorldFirst : {false, true})
    {
        const FString Name = WorldFirst ? TEXT("world_world_first") : TEXT("world_env_first");
        It(Name, [this, Name, WorldFirst]
        {
            FLeakResult Result(*this, *Name);
            for (int32 Cycle = 0; Cycle < 12; ++Cycle)
            {
                FVmLedger Ledger;
                UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("MemoryWorld"));
                if (!World)
                {
                    Result.Check(false, TEXT("Memory test world created"));
                    break;
                }
                GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
                World->InitializeActorsForPlay(FURL());
                World->BeginPlay();
                auto* Timer = World->GetSubsystem<UUnLuaNovaTimerSubsystem>();
                TWeakObjectPtr<UObject> WeakWorld = World;
                TWeakObjectPtr<UObject> WeakTimer = Timer;
                auto Env = MakeUnique<FTrackedEnv>(Ledger);
                UnLuaNova::PushUObject(Env->GetMainState(), World);
                lua_setglobal(Env->GetMainState(), "World");
                Result.Run(*Env, TEXT(R"(
                    local timer = require('UnLuaNova.Timer')
                    local event = require('UnLuaNova.Event')
                    Weak = setmetatable({}, {__mode = 'v'})
                    do
                        local owner = {payload = string.rep('x', 65536)}
                        Weak[1] = owner
                        local token = event.On('Memory.Event', function() return owner end, owner)
                        assert(event.Off(token))
                        timer.Set(function() return owner end, 0, true, 100, false, World)
                    end
                )"));
                Result.Check(Timer && Timer->Num() == 1, TEXT("Timer callback was registered"));
                Env->GC();
                Result.Run(*Env, TEXT("assert(Weak[1] ~= nil)"));
                if (WorldFirst)
                {
                    GEngine->DestroyWorldContext(World);
                    World->DestroyWorld(false);
                    Env->GC();
                    Result.Run(*Env, TEXT("assert(Weak[1] == nil)"));
                    Result.Resource(Timer && Timer->Num() == 0);
                    Env.Reset();
                }
                else
                {
                    Env.Reset();
                    Result.Resource(Timer && Timer->Num() == 0);
                    GEngine->DestroyWorldContext(World);
                    World->DestroyWorld(false);
                }
                CollectObjects();
                Result.Resource(!WeakWorld.IsValid());
                Result.Resource(!WeakTimer.IsValid());
                Result.Closed(Ledger);
                Result.Sample(Ledger);
            }
            Result.Save();
        });
    }
#endif

#if UNLUANOVA_HAS_ALLOC_CALLBACKS
    It(TEXT("Closing a different environment preserves allocation callbacks"), [this]
    {
        auto& Stats = UnLuaNova::FLuaMemoryStats::Get();
        FVmLedger FirstLedger, OtherLedger;
        auto First = MakeUnique<FTrackedEnv>(FirstLedger);
        Stats.Attach(First->GetMainState());
        { FTrackedEnv Other(OtherLedger); }
        TestFalse(TEXT("Other environment did not detach the owner"), Stats.BuildCategoryReport().Contains(TEXT("Not attached")));
        First.Reset();
        TestTrue(TEXT("Closing the owner detaches before freeing its state"), Stats.BuildCategoryReport().Contains(TEXT("Not attached")));
        Stats.Detach();
    });
#endif
}
#endif

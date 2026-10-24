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

#if WITH_DEV_AUTOMATION_TESTS

#include "TestCommands.h"

#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UnrealClient.h"

#if WITH_EDITOR
#include "FileHelpers.h"
#include "LevelEditor.h"
#if ENGINE_MAJOR_VERSION >= 5
#include "Tests/AutomationEditorCommon.h"
#endif
#endif

namespace UnLuaNovaTestSuite
{
    // A PIE session is "running" for as long as any PIE world is still around, which is
    // what tells us whether a previous test's session has really finished ending or is only
    // being torn down.
    static bool IsPIERunning()
    {
        if (!GEngine)
            return false;

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World())
                return true;
        }

        return false;
    }

    FOpenMapLatentCommand::FOpenMapLatentCommand(FString MapName, bool bForceReload, int32 InNumClients, EPlayNetMode InPlayNetMode)
        : MapName(MapName), bForceReload(bForceReload)
        , NumClients(InNumClients), PlayNetMode(InPlayNetMode)
    {
        // Server world + one per client. Standalone is just the one.
        ExpectedWorlds = (PlayNetMode == EPlayNetMode::PIE_Standalone) ? 1 : (InNumClients + 1);
    }

    FPieNetWorlds GetPieNetWorlds()
    {
        FPieNetWorlds Result;
        if (!GEngine)
            return Result;

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            UWorld* World = Context.World();
            if (!World || Context.WorldType != EWorldType::PIE)
                continue;

            const ENetMode Mode = World->GetNetMode();
            if (Mode == NM_ListenServer || Mode == NM_DedicatedServer)
            {
                if (!Result.Server)
                    Result.Server = World;
            }
            else if (Mode == NM_Client)
            {
                Result.Clients.Add(World);
            }
        }

        return Result;
    }

    static int32 CountPieWorlds()
    {
        int32 Count = 0;
        if (!GEngine)
            return Count;

        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World())
                ++Count;
        }

        return Count;
    }

    bool FOpenMapLatentCommand::Update()
    {
#if !WITH_EDITOR
        return true;
#else
        // Phase 1: an earlier test left a session running (see FEndPlayMapCommand). End it
        // and wait until it is gone - starting the new one on top of a teardown is what
        // loses the request, and it is also the moment the old session's audio device comes
        // apart inside this frame.
        if (bEndingPreviousSession)
        {
            if (IsPIERunning())
                return false;

            bEndingPreviousSession = false;
        }

        if (!WaitForMapToLoad)
        {
            if (IsPIERunning())
            {
                GEditor->EndPlayMap();
                bEndingPreviousSession = true;
                return false;
            }

            LoadMap();
            return false;
        }

        // Phase 2: wait for this map to be up. Matched by name, not just "some game
        // world", so a session that is still shutting down cannot stand in for it.
        //
        // For a C/S session this is necessary but not sufficient: every peer loads the same
        // map, so the first one to finish satisfies the name wait while the clients are still
        // coming up. Assertions made against a client that has not joined yet fail for a
        // reason that has nothing to do with what is being tested, so wait for them too.
        if (WaitForMapToLoad->Update())
        {
            if (CountPieWorlds() >= ExpectedWorlds)
                return true;
        }

        if (FPlatformTime::Seconds() > WaitDeadline)
        {
            // Give up rather than wait forever: the test that asked for this map will fail
            // on its own assertions with a clear reason here in the log, and the rest of the
            // suite still runs. Ending the session keeps the next test from inheriting a
            // half-started one.
            UE_LOG(LogTemp, Error, TEXT("PIE never came up for '%s' after %.0fs - ending the session and moving on. Check the log above for why the play session did not start (a lost request, or PIE being ended mid-start)."),
                *MapName, WaitTimeoutSeconds);
            if (IsPIERunning())
                GEditor->EndPlayMap();
            return true;
        }

        return false;
#endif
    }

    void FOpenMapLatentCommand::LoadMap()
    {
#if WITH_EDITOR
        struct FFailedGameStartHandler
        {
            bool bCanProceed;

            FFailedGameStartHandler()
            {
                bCanProceed = true;
                FEditorDelegates::EndPIE.AddRaw(this, &FFailedGameStartHandler::OnEndPIE);
            }

            ~FFailedGameStartHandler()
            {
                FEditorDelegates::EndPIE.RemoveAll(this);
            }

            bool CanProceed() const { return bCanProceed; }

            void OnEndPIE(const bool bInSimulateInEditor)
            {
                bCanProceed = false;
            }
        };

        bool bNeedLoadEditorMap = true;
        bool bPieRunning = false;

        //check existing worlds
        const TIndirectArray<FWorldContext> WorldContexts = GEngine->GetWorldContexts();
        for (auto& Context : WorldContexts)
        {
            if (Context.World())
            {
                FString WorldPackage = Context.World()->GetOutermost()->GetName();

                if (Context.WorldType == EWorldType::PIE)
                {
                    //don't quit!  This was triggered while pie was already running!
                    bPieRunning = true;
                    break;
                }
                else if (Context.WorldType == EWorldType::Editor)
                {
                    bNeedLoadEditorMap = MapName != WorldPackage;
                }
            }
        }

        if (bNeedLoadEditorMap || bForceReload)
        {
            if (bPieRunning)
                GEditor->EndPlayMap();
            FEditorFileUtils::LoadMap(*MapName, false, true);
        }

        FFailedGameStartHandler FailHandler;

        FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
        TSharedPtr<IAssetViewport> ActiveLevelViewport = LevelEditorModule.GetFirstActiveViewport();

        FRequestPlaySessionParams RequestParams;
        ULevelEditorPlaySettings* EditorPlaySettings = NewObject<ULevelEditorPlaySettings>();
        EditorPlaySettings->SetPlayNumberOfClients(NumClients);
        // Separate servers run in another process, and a test can only reach peers that
        // share its process - which is the whole point of a listen server here.
        EditorPlaySettings->bLaunchSeparateServer = false;
        EditorPlaySettings->SetPlayNetMode(PlayNetMode);
        RequestParams.EditorPlaySettings = EditorPlaySettings;
        RequestParams.DestinationSlateViewport = ActiveLevelViewport;

#if ENGINE_MAJOR_VERSION >= 5
        // Make sure the player start location is a valid location.
        if (GEditor->CheckForPlayerStart() == nullptr)
        {
            FAutomationEditorCommonUtils::SetPlaySessionStartToActiveViewport(RequestParams);
        }
#endif

        GEditor->RequestPlaySession(RequestParams);

        // Immediately launch the session 
        GEditor->StartQueuedPlaySessionRequest();

        check(FailHandler.CanProceed());

        // Waited for by name: FWaitForMapToLoadCommand answers for any game world, and a
        // session that is still ending would otherwise be accepted as "the map is loaded".
        WaitForMapToLoad = MakeUnique<FWaitForSpecifiedMapToLoadCommand>(MapName);
        WaitDeadline = FPlatformTime::Seconds() + WaitTimeoutSeconds;
#endif
    }

    bool FEndPlayMapCommand::Update()
    {
#if WITH_EDITOR
#if ENGINE_MAJOR_VERSION < 5
        if (GEditor && GEditor->PlayWorld)
            GEditor->EndPlayMap();
#endif
#endif
        return true;
    }

    FScreenshotLatentCommand::FScreenshotLatentCommand(FString ShotName)
        : Name(MoveTemp(ShotName))
    {
    }

    bool FScreenshotLatentCommand::Update()
    {
        const FString Path = FPaths::ScreenShotDir() / (Name + TEXT(".png"));

        if (!bRequested)
        {
            bRequested = true;
            // No suffix: the report generator matches screenshots by name, so the file has
            // to be exactly <Name>.png.
            //
            // bShowUI=true, restricted to the game viewport. The tutorial levels' visible
            // content is PrintString on-screen text and UMG widgets - both live in the UI
            // layer. With bShowUI=false this captured the bare scene render target only, and
            // the engine additionally disables on-screen debug messages for that path
            // (UnrealClient.cpp: GAreScreenMessagesEnabled = false), so every showcase shot
            // came out as a photo of an empty, unlit world: pure black. Restricting to the
            // game viewport keeps the editor chrome out when PIE is embedded in the level
            // editor viewport.
            FScreenshotRequest::RequestScreenshot(Name, true, false, false, FIntRect(), true);
            return false;
        }

        if (IFileManager::Get().FileExists(*Path))
            return true;

        if (++FramesWaited > 120)
        {
            // Not a failure: whether a PNG can be produced at all depends on the run (a
            // viewport has to exist), and a missing picture must not read as "the showcase
            // broke". Say it out loud anyway - otherwise the only symptom is an empty
            // gallery in the report and nobody knows why.
            UE_LOG(LogTemp, Warning, TEXT("Screenshot '%s' was never written to %s - no viewport, or rendering is off (-NullRHI)."), *Name, *Path);
            return true;
        }

        return false;
    }

    FCaptureFramesLatentCommand::FCaptureFramesLatentCommand(FString ClipName, int32 NumFrames, int32 WarmupFrames)
        : Name(MoveTemp(ClipName)), Frames(NumFrames), WarmupFrames(WarmupFrames)
    {
    }

    bool FCaptureFramesLatentCommand::Update()
    {
        if (Captured >= Frames)
            return true;

        if (WarmedUp < WarmupFrames)
        {
            // Let the PIE viewport settle before the first frame is captured, otherwise frame 1
            // is a picture of the level editor viewport instead of the running demo.
            ++WarmedUp;
            return false;
        }

        const FString FrameName = FString::Printf(TEXT("%s_%04d"), *Name, Captured + 1);
        const FString Path = FPaths::ScreenShotDir() / (FrameName + TEXT(".png"));

        if (!bPending)
        {
            bPending = true;
            FramesWaited = 0;
            // Same bShowUI/bRestrictToGameViewport reasoning as FScreenshotLatentCommand:
            // without UI the clips record an empty unlit world instead of the demo.
            FScreenshotRequest::RequestScreenshot(FrameName, true, false, false, FIntRect(), true);
            return false;
        }

        if (IFileManager::Get().FileExists(*Path))
        {
            ++Captured;
            bPending = false;
            return Captured >= Frames;
        }

        // Give up on this frame rather than on the whole clip: a dropped frame leaves a
        // jump in the video, a hang leaves the suite stuck and the report empty.
        if (++FramesWaited > 120)
        {
            ++Captured;
            bPending = false;
            return Captured >= Frames;
        }

        return false;
    }

    bool IsFrameRecordingEnabled()
    {
        const FString Value = FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_RECORD_FRAMES"));
        return !Value.IsEmpty() && Value != TEXT("0") && !Value.Equals(TEXT("false"), ESearchCase::IgnoreCase);
    }

    int32 GetFrameRecordingCount()
    {
        const FString Value = FPlatformMisc::GetEnvironmentVariable(TEXT("UNLUANOVA_RECORD_FRAMES"));
        if (Value.IsEmpty())
            return 0;
        const int32 Count = FCString::Atoi(*Value);
        // A number means "this many frames"; anything else ("1", "on", "true") is the
        // default clip length.
        return Count > 0 ? Count : 60;
    }

    // Lets a run grab pictures without writing a test for it, e.g.
    //   -ExecCmds="UnLuaNovaTest.Screenshot Showcase_Binding; Quit"
    // which is how the showcase pages are captured.
    static void TakeScreenshotCommand(const TArray<FString>& Args)
    {
        const FString ShotName = Args.Num() > 0 ? Args[0] : TEXT("UnLuaNovaTest");
        // Same as FScreenshotLatentCommand: capture the game viewport with UI, not the bare
        // (and here, empty) scene render target.
        FScreenshotRequest::RequestScreenshot(ShotName, true, false, false, FIntRect(), true);
    }

    static FAutoConsoleCommand GScreenshotConsoleCommand(
        TEXT("UnLuaNovaTest.Screenshot"),
        TEXT("Requests a screenshot into Saved/Screenshots. Usage: UnLuaNovaTest.Screenshot <name>"),
        FConsoleCommandWithArgsDelegate::CreateStatic(&TakeScreenshotCommand));

    FString DescribeUniformScreenshot(const FString& PngPath)
    {
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
        if (!ImageWrapper.IsValid())
        {
            return FString();
        }

        TArray<uint8> FileData;
        if (!FFileHelper::LoadFileToArray(FileData, *PngPath))
        {
            return FString();
        }
        if (!ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
        {
            return FString();
        }

        TArray64<uint8> Raw;
        if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, Raw))
        {
            return FString();
        }

        const int32 Width = ImageWrapper->GetWidth();
        const int32 Height = ImageWrapper->GetHeight();
        const int64 NumPixels = Raw.Num() / 4;
        if (NumPixels <= 0)
        {
            return FString();
        }

        // Uniform = min/max luminance over the whole image within a small tolerance. A real
        // scene shot has far more spread than that, so this only fires on photos of nothing.
        uint8 MinLuma = 255;
        uint8 MaxLuma = 0;
        const uint8* Pixels = Raw.GetData();
        for (int64 i = 0; i < NumPixels; ++i)
        {
            const uint8* Pixel = Pixels + (i << 2); // BGRA
            const uint8 Luma = static_cast<uint8>((Pixel[0] * 299u + Pixel[1] * 587u + Pixel[2] * 114u) >> 8);
            MinLuma = FMath::Min(MinLuma, Luma);
            MaxLuma = FMath::Max(MaxLuma, Luma);
            if (MaxLuma - MinLuma > 2)
            {
                return FString();
            }
        }

        return FString::Printf(TEXT("是一张 %dx%d 的纯色图（亮度范围 %d..%d，四舍五入就是什么都看不见）"),
            Width, Height, MinLuma, MaxLuma);
    }

    FVerifyScreenshotNotBlankLatentCommand::FVerifyScreenshotNotBlankLatentCommand(FString ShotName, FAutomationTestBase& InTest)
        : Name(MoveTemp(ShotName)), Test(InTest)
    {
    }

    bool FVerifyScreenshotNotBlankLatentCommand::Update()
    {
        const FString Path = FPaths::ScreenShotDir() / (Name + TEXT(".png"));

        if (!IFileManager::Get().FileExists(*Path))
        {
            // The screenshot command upstream already gave up after 120 frames with a log
            // line; this only catches the "PNG appeared late" race.
            if (++FramesWaited <= 120)
            {
                return false;
            }
            Test.AddWarning(FString::Printf(TEXT("截图 '%s' 没有落盘（%s），无法检查是否为空图。"), *Name, *Path));
            return true;
        }

        const FString Uniform = DescribeUniformScreenshot(Path);
        if (!Uniform.IsEmpty())
        {
            Test.AddError(FString::Printf(TEXT("截图 '%s' %s —— 大概率截到了空关卡（没有灯光/网格体，可见内容全在 UI 层）或无效的一帧。"),
                *Name, *Uniform));
        }

        return true;
    }
}

#endif

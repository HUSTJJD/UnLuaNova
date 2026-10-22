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
#include "Misc/Paths.h"
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
    FOpenMapLatentCommand::FOpenMapLatentCommand(FString MapName, bool bForceReload)
        : MapName(MapName), bForceReload(bForceReload)
    {
    }

    bool FOpenMapLatentCommand::Update()
    {
        if (WaitForMapToLoad)
            return WaitForMapToLoad->Update();

        LoadMap();
        return false;
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
        EditorPlaySettings->SetPlayNumberOfClients(1);
        EditorPlaySettings->bLaunchSeparateServer = false;
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
        WaitForMapToLoad = MakeUnique<FWaitForMapToLoadCommand>();
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
            FScreenshotRequest::RequestScreenshot(Name, false, false);
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

    FCaptureFramesLatentCommand::FCaptureFramesLatentCommand(FString ClipName, int32 NumFrames)
        : Name(MoveTemp(ClipName)), Frames(NumFrames)
    {
    }

    bool FCaptureFramesLatentCommand::Update()
    {
        if (Captured >= Frames)
            return true;

        const FString FrameName = FString::Printf(TEXT("%s_%04d"), *Name, Captured + 1);
        const FString Path = FPaths::ScreenShotDir() / (FrameName + TEXT(".png"));

        if (!bPending)
        {
            bPending = true;
            FramesWaited = 0;
            FScreenshotRequest::RequestScreenshot(FrameName, false, false);
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
        FScreenshotRequest::RequestScreenshot(ShotName, false, false);
    }

    static FAutoConsoleCommand GScreenshotConsoleCommand(
        TEXT("UnLuaNovaTest.Screenshot"),
        TEXT("Requests a screenshot into Saved/Screenshots. Usage: UnLuaNovaTest.Screenshot <name>"),
        FConsoleCommandWithArgsDelegate::CreateStatic(&TakeScreenshotCommand));
}

#endif

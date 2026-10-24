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

#pragma once

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
// EPlayNetMode (PIE_Standalone / PIE_ListenServer / PIE_Client) lives in
// Settings/LevelEditorPlaySettings.h - an *editor* header, in a module that
// UnLuaNovaTestSuite.Build.cs only depends on when Target.bBuildEditor. Including it here
// used to be harmless right up until the suite also had to compile for the -game carrier:
// every one of the ~20 files that include this header then failed on an include that is not
// on the include path at all. So the enum is mirrored below and mapped back in
// TestCommands.cpp, which is where the editor-only call actually is.

#if WITH_DEV_AUTOMATION_TESTS

namespace UnLuaNovaTestSuite
{
    /** Mirror of EPlayNetMode; see the note above the includes. */
    enum class EUnLuaNovaPlayNetMode : uint8
    {
        Standalone,
        ListenServer,
        Client,
    };
    // Opens a map in PIE and waits until that map - not whatever was running before - is
    // actually playing.
    //
    // Two things about PIE inside a long suite make this more than "request and wait":
    //
    //  - An earlier test's session can still be alive (FEndPlayMapCommand leaves PIE running
    //    on UE5). Requesting a new one while the old one is still tearing down is the bad
    //    case: the engine ends the old session inside that very call - audio device and all -
    //    and the new request can be dropped with it, so no session ever comes up.
    //  - The engine's "wait for a map" helper answers for *any* game world, so a stale
    //    session can satisfy the wait for a map that has not been opened yet.
    //
    // So: end whatever is running and wait until it is really gone, then start, then wait by
    // map name - and that wait is bounded, because a suite that never finishes is worse than
    // a test that fails.
    class FOpenMapLatentCommand : public IAutomationLatentCommand
    {
    public:
        /**
         * @param NumClients  How many client windows. 1 = standalone (the default, what
         *                    every existing test wants). >1 only makes sense with a net mode.
         * @param PlayNetMode Standalone (default) or ListenServer for a C/S session.
         *                    The server runs in-process - bLaunchSeparateServer is left off - so
         *                    one editor process holds every peer, which is what lets a single
         *                    test reach into all of them.
         */
        explicit FOpenMapLatentCommand(FString MapName, bool bForceReload = false,
            int32 NumClients = 1, EUnLuaNovaPlayNetMode PlayNetMode = EUnLuaNovaPlayNetMode::Standalone);

        virtual bool Update() override;

        void LoadMap();

    private:
        // How long a session is allowed to take before this gives up on it. Generous on
        // purpose: starting PIE compiles blueprints and can take a second or two, and a
        // false alarm here turns into a failed test rather than a slow one.
        static constexpr double WaitTimeoutSeconds = 60.0;

        FString MapName;
        bool bForceReload;
        int32 NumClients = 1;
        EPlayNetMode PlayNetMode = EPlayNetMode::PIE_Standalone;
        bool bEndingPreviousSession = false;
        double WaitDeadline = 0.0;
#if WITH_EDITOR
        TUniquePtr<FWaitForSpecifiedMapToLoadCommand> WaitForMapToLoad;
#endif
    };

    // Ends a PIE session and waits until it is really gone.
    //
    // FEndPlayMapCommand is a no-op on UE5 - it exists for the UE4 code path - so a test
    // that wants PIE to be over when it finishes has nothing to call. Ending here rather
    // than leaving it to the next test matters for anything that cleans up per-session
    // state: the Lua overrides are torn down when the session ends, so a test that restores
    // them wants the session gone first, not merely requested to stop.
    class FEndPieSessionCommand : public IAutomationLatentCommand
    {
    public:
        virtual bool Update() override;

    private:
        bool bRequested = false;
        double WaitDeadline = 0.0;
        // Same order of magnitude as FOpenMapLatentCommand's: tearing a session down is
        // normally a frame or two, and the bound is only here so a stuck teardown cannot
        // wedge the whole suite.
        static constexpr double WaitTimeoutSeconds = 60.0;
    };

    class FEndPlayMapCommand : public IAutomationLatentCommand
    {
    public:
        virtual bool Update() override;
    };

    /**
     * The peers of the PIE session currently running, split by role.
     *
     * Classified by GetNetMode() rather than by PIE instance index: the index is an
     * editor-side bookkeeping number that says nothing about who is the authority, and
     * every assertion a networked test wants to make is about authority.
     */
    struct FPieNetWorlds
    {
        /** The listen server (NM_ListenServer) or dedicated server (NM_DedicatedServer). */
        UWorld* Server = nullptr;
        /** One entry per connected client, in world-context order. */
        TArray<UWorld*> Clients;
    };

    /** Collect the PIE worlds of the running session. Empty when PIE is not up. */
    FPieNetWorlds GetPieNetWorlds();

    // Takes a screenshot and waits until the PNG is actually on disk.
    //
    // Screenshots are written asynchronously, so a test that screenshots and immediately
    // moves on leaves the report generator with an empty gallery. The wait is bounded on
    // purpose: a -NullRHI run never produces a PNG, and hanging the whole suite is worse
    // than missing a picture.
    class FScreenshotLatentCommand : public IAutomationLatentCommand
    {
    public:
        explicit FScreenshotLatentCommand(FString ShotName);

        virtual bool Update() override;

    private:
        FString Name;
        bool bRequested = false;
        int32 FramesWaited = 0;
    };

    // Decodes the PNG at PngPath and returns a non-empty description when it is a uniform
    // (all-one-color) image - "a photo of nothing". Returns an empty string when the PNG is
    // missing, undecodable, or has actual content, so a broken check never masks a real shot.
    FString DescribeUniformScreenshot(const FString& PngPath);

    // Runs after FScreenshotLatentCommand and turns a uniform screenshot into a test failure.
    // A showcase image has exactly one job - show the level - and an all-black PNG means the
    // shot captured nothing (empty level, occluded window, ...). Before this check such shots
    // passed silently and got published to the docs.
    class FVerifyScreenshotNotBlankLatentCommand : public IAutomationLatentCommand
    {
    public:
        FVerifyScreenshotNotBlankLatentCommand(FString ShotName, FAutomationTestBase& InTest);

        virtual bool Update() override;

    private:
        FString Name;
        FAutomationTestBase& Test;
        int32 FramesWaited = 0;
    };

    // Captures a burst of consecutive frames as <Name>_0001.png, <Name>_0002.png, ... so an
    // offline encoder (-Video in Tools/RunTests.ps1) can turn them into a clip. Still images
    // are what the docs site shows; a clip is what shows something moving.
    //
    // One screenshot per update, each waited for: FScreenshotRequest has no completion
    // callback, and asking for the next frame before the previous PNG exists produces a
    // sequence with holes in it. The wait is bounded on purpose for the same reason the
    // single-shot command's is - a -NullRHI run never writes a PNG.
    //
    // The first couple of frames are dropped rather than captured: the very first one can still
    // show the level editor viewport (grid and axis gizmo) instead of the PIE view, which leaves
    // a jarring frame of the wrong thing at the head of every clip.
    class FCaptureFramesLatentCommand : public IAutomationLatentCommand
    {
    public:
        FCaptureFramesLatentCommand(FString ClipName, int32 NumFrames, int32 WarmupFrames = 3);

        virtual bool Update() override;

    private:
        FString Name;
        int32 Frames;
        int32 WarmupFrames;
        int32 WarmedUp = 0;
        int32 Captured = 0;
        int32 FramesWaited = 0;
        bool bPending = false;
    };

    // Whether this run was asked to record clips as well as stills. Read from the
    // environment because the switch lives on RunTests.ps1, not on the test.
    bool IsFrameRecordingEnabled();
    int32 GetFrameRecordingCount();
}

#endif

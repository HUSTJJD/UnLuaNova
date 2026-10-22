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

#if WITH_DEV_AUTOMATION_TESTS

namespace UnLuaNovaTestSuite
{
    class FOpenMapLatentCommand : public IAutomationLatentCommand
    {
    public:
        explicit FOpenMapLatentCommand(FString MapName, bool bForceReload = false);

        virtual bool Update() override;

        void LoadMap();

    private:
        FString MapName;
        bool bForceReload;
        TUniquePtr<FWaitForMapToLoadCommand> WaitForMapToLoad;
    };

    class FEndPlayMapCommand : public IAutomationLatentCommand
    {
    public:
        virtual bool Update() override;
    };

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

    // Captures a burst of consecutive frames as <Name>_0001.png, <Name>_0002.png, ... so an
    // offline encoder (-Video in Tools/RunTests.ps1) can turn them into a clip. Still images
    // are what the docs site shows; a clip is what shows something moving.
    //
    // One screenshot per update, each waited for: FScreenshotRequest has no completion
    // callback, and asking for the next frame before the previous PNG exists produces a
    // sequence with holes in it. The wait is bounded on purpose for the same reason the
    // single-shot command's is - a -NullRHI run never writes a PNG.
    class FCaptureFramesLatentCommand : public IAutomationLatentCommand
    {
    public:
        FCaptureFramesLatentCommand(FString ClipName, int32 NumFrames);

        virtual bool Update() override;

    private:
        FString Name;
        int32 Frames;
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

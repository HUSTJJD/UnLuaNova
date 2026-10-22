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

// Takes the pictures the docs site shows: Docs/CN/Showcase.md and the gallery at the bottom
// of every test report.
//
// They are taken from the tutorial levels (Content/Tutorials/) on purpose. A screenshot of a
// test fixture is a picture of something the reader will never open; a screenshot of a level
// that ships with the project is a picture of the thing the page is describing, and it goes
// stale in the same commit that changes the level instead of quietly years later.
//
// Each shot is its own test so a failure names the missing picture, and all of them are
// registered with BEGIN_VIEWPORT_TESTSUITE: without a viewport there is no picture to take,
// and "no screenshot" must not be reported as "the showcase broke".
//
// Stills are always taken. Clips (a burst of frames, encoded to video by Tools/RunTests.ps1
// -Video) only when the run asked for them, because they cost a second or two each and most
// runs only want the stills:
//
//   Tools\RunTests.ps1 -Filter UnLuaNova.Showcase -Video -Publish

#include "TestCommands.h"
#include "UnLuaNovaTestCommon.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

#if WITH_DEV_AUTOMATION_TESTS

// Opens the level, lets PIE settle, takes the still, verifies it is not a uniform (blank)
// image, and - if this run is recording - captures a clip of the same scene. The wait is not
// cosmetic: PIE needs a few ticks before there is anything to photograph, and a shot taken at
// frame 0 is a picture of a loading frame.
#define SHOWCASE_SHOT(MapName, ShotName) \
    ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(MapName)) \
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f)) \
    ADD_LATENT_AUTOMATION_COMMAND(FScreenshotLatentCommand(ShotName)) \
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyScreenshotNotBlankLatentCommand(ShotName, *this)) \
    if (IsFrameRecordingEnabled()) \
    { \
        ADD_LATENT_AUTOMATION_COMMAND(FCaptureFramesLatentCommand(FString(ShotName) + TEXT("_Clip"), GetFrameRecordingCount())) \
    } \
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());

BEGIN_VIEWPORT_TESTSUITE(FShowcaseBindingTest, TEXT("UnLuaNova.Showcase.Binding 静态绑定示例截图"))

    bool FShowcaseBindingTest::RunTest(const FString& Parameters)
    {
        SHOWCASE_SHOT(TEXT("/Game/Tutorials/01_HelloWorld/HelloWorld"), TEXT("Showcase_Binding"))
        return true;
    }

END_TESTSUITE(FShowcaseBindingTest)

BEGIN_VIEWPORT_TESTSUITE(FShowcaseOverrideTest, TEXT("UnLuaNova.Showcase.Override 蓝图事件覆写示例截图"))

    bool FShowcaseOverrideTest::RunTest(const FString& Parameters)
    {
        SHOWCASE_SHOT(TEXT("/Game/Tutorials/02_OverrideBlueprintEvents/OverrideBlueprintEvents"), TEXT("Showcase_Override"))
        return true;
    }

END_TESTSUITE(FShowcaseOverrideTest)

BEGIN_VIEWPORT_TESTSUITE(FShowcaseContainerTest, TEXT("UnLuaNova.Showcase.Container 容器访问示例截图"))

    bool FShowcaseContainerTest::RunTest(const FString& Parameters)
    {
        SHOWCASE_SHOT(TEXT("/Game/Tutorials/06_NativeContainers/NativeContainers"), TEXT("Showcase_Container"))
        return true;
    }

END_TESTSUITE(FShowcaseContainerTest)

BEGIN_VIEWPORT_TESTSUITE(FShowcaseUmgTest, TEXT("UnLuaNova.Showcase.UMG UMG 与 Lua 生命周期示例截图"))

    bool FShowcaseUmgTest::RunTest(const FString& Parameters)
    {
        SHOWCASE_SHOT(TEXT("/Game/Tutorials/11_ReleaseUMG/ReleaseUMG"), TEXT("Showcase_UMG"))
        return true;
    }

END_TESTSUITE(FShowcaseUmgTest)

#endif

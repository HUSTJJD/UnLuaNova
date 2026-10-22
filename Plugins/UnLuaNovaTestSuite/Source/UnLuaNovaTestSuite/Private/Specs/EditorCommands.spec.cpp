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

// Covers the parts of the editor surface that can break without any test noticing:
//
//   * the lua.* console commands - registered by the runtime module at startup; if a rename
//     lands in the command name but not in the docs, nothing fails until a user types it
//   * the Lua templates - "create Lua template" reads them from disk and substitutes
//     ClassName; a missing file or a renamed placeholder produces an empty or unsubstituted
//     script rather than an error
//   * the editor command context - proves the editor module's commands are registered, which
//     is as far as this can go without linking the editor module (its headers are private
//     and its classes are unexported)
//
// Everything here goes through public UE APIs, so the test suite needs no new module
// dependency.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "UnLuaNovaTestHelpers.h"

#if WITH_EDITOR
#include "Modules/ModuleManager.h"
#include "Framework/Commands/InputBindingManager.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS

// Declared here rather than including UnLuaNovaPrivate.h: the plugin's private PCH pulls in
// far more than this test needs.
UNLUANOVA_API extern FString GLuaSrcFullPath;

BEGIN_DEFINE_SPEC(FEditorCommandsSpec, "UnLuaNova.Editor.Commands", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
END_DEFINE_SPEC(FEditorCommandsSpec)

void FEditorCommandsSpec::Define()
{
    Describe(TEXT("控制台命令"), [this]()
    {
        It(TEXT("lua.* 全部已注册"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            static const TCHAR* Commands[] = {
                TEXT("lua.do"),
                TEXT("lua.dofile"),
                TEXT("lua.gc"),
                TEXT("lua.memstats"),
                TEXT("lua.profiler"),
                TEXT("lua.dbg"),
            };

            for (const TCHAR* Command : Commands)
            {
                // The name is the assertion text: a failure says which command vanished.
                TestTrue(Command, IConsoleManager::Get().FindConsoleObject(Command) != nullptr);
            }
        });
    });

    Describe(TEXT("Lua 模板"), [this]()
    {
        It(TEXT("随插件发布且带 ClassName 占位符"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // CreateLuaTemplate_Executed looks these up by class name under
            // Config/LuaTemplates, project dir first and the plugin's as fallback. Only the
            // plugin's are guaranteed to exist for every user.
            static const TCHAR* Templates[] = {
                TEXT("Actor"),
                TEXT("ActorComponent"),
                TEXT("AnimInstance"),
                TEXT("AnimNotifyState"),
                TEXT("Object"),
                TEXT("UserWidget"),
            };

            const FString Dir = FPaths::ProjectPluginsDir() / TEXT("UnLuaNova") / TEXT("Config") / TEXT("LuaTemplates");
            for (const TCHAR* Template : Templates)
            {
                const FString Path = Dir / (FString(Template) + TEXT(".lua"));
                TestTrue(*Path, IFileManager::Get().FileExists(*Path));

                FString Content;
                FFileHelper::LoadFileToString(Content, *Path);
                // Substituted during generation; a template without it produces a script
                // that still says "ClassName".
                TestTrue(*Path, Content.Contains(TEXT("ClassName")));
            }
        });

        It(TEXT("脚本根目录指向工程的 Content/Script"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // The template is written to GLuaSrcFullPath + relative module path, so a wrong
            // root means templates land somewhere nobody looks.
            TEST_FALSE(GLuaSrcFullPath.IsEmpty());
            TEST_TRUE(GLuaSrcFullPath.EndsWith(TEXT("Script/")));
            TEST_TRUE(FPaths::DirectoryExists(*GLuaSrcFullPath));
        });
    });

#if WITH_EDITOR
    Describe(TEXT("编辑器命令"), [this]()
    {
        It(TEXT("编辑器模块可加载且命令上下文已注册"), EAsyncExecution::TaskGraphMainThread, [this]()
        {
            // Loading by name rather than linking: the editor module's headers are private
            // and its command class is not exported, and this test lives in a module that
            // must also build for game targets.
            TEST_TRUE(FModuleManager::Get().LoadModulePtr(TEXT("UnLuaNovaEditor")) != nullptr);

            const TSharedPtr<FBindingContext> Context = FInputBindingManager::Get().GetContextByName(TEXT("UnLuaNovaEditor"));
            TEST_TRUE(Context.IsValid());
        });
    });
#endif
}

#endif

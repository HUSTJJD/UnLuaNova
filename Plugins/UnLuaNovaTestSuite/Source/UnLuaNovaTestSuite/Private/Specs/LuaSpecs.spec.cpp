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

// Runs the Lua-side suites (Content/Script/Tests/Lua/*.spec.lua) and lifts every result
// into a real automation case.
//
// The modules the plugin ships are pure Lua - StringEx, Vector, Buffer - and until now the
// only way to exercise them was a C++ case calling into them, which means a bug in a
// five-line Lua helper needs a C++ file to be noticed at all.
//
// Two things make this worth the indirection:
//   * each Lua case becomes its own TestTrue, so a single broken helper does not hide the
//     other thirty behind one red case;
//   * the failure text carries the Lua file and line, because "a Lua helper is wrong" is
//     useless without knowing which line.
//
// "No spec files found" is a failure, not a pass: a renamed directory would otherwise
// leave this case green while running nothing.

#include "UnLuaNovaBase.h"
#include "UnLuaNovaTemplate.h"
#include "UnLuaNovaTestHelpers.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS

BEGIN_DEFINE_SPEC(FLuaSpecsSpec, "UnLuaNova.Lua", EAutomationTestFlags::ProductFilter | ApplicationContextMask)
    TSharedPtr<UnLuaNova::FLuaEnv> Env;
    lua_State* L;
END_DEFINE_SPEC(FLuaSpecsSpec)

void FLuaSpecsSpec::Define()
{
    BeforeEach([this]
    {
        Env = MakeShared<UnLuaNova::FLuaEnv>();
        L = Env->GetMainState();
    });

    AfterEach([this]
    {
        Env.Reset();
        L = nullptr;
    });

    It(TEXT("Lua 侧用例全部通过"), EAsyncExecution::TaskGraphMainThread, [this]()
    {
        const FString Dir = FPaths::ProjectContentDir() / TEXT("Script") / TEXT("Tests") / TEXT("Lua");

        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *(Dir / TEXT("*.spec.lua")), true, false);
        Files.Sort();

        // A missing or renamed directory must not look like success.
        TestTrue(*FString::Printf(TEXT("在 %s 下找到 *.spec.lua"), *Dir), Files.Num() > 0);
        if (Files.Num() == 0)
            return;

        for (const FString& File : Files)
        {
            // Relative to the script root (Content/Script), which is what LoadFile takes.
            const FString Relative = FString(TEXT("Tests/Lua/")) + File;
            const bool bLoaded = UnLuaNova::LoadFile(L, Relative);
            TestTrue(*Relative, bLoaded);
            if (!bLoaded)
                continue;

            // Registering the cases is the point of loading the file; a raise here means
            // the file itself is broken and no case inside it will run.
            const int32 Status = lua_pcall(L, 0, 0, 0);
            if (Status != 0)
            {
                const char* Err = lua_tostring(L, -1);
                AddError(FString::Printf(TEXT("%s: %s"), *Relative, UTF8_TO_TCHAR(Err ? Err : "")), 0);
                lua_pop(L, 1);
            }
        }

        // Run and encode in one chunk: the results table never has to cross the boundary
        // as a table, which would need backend-specific accessors.
        TEST_TRUE(Env->DoString(TEXT(
            "local S = require('Tests.Lua.Spec') "
            "_G.SpecLuaResults = S.encode(S.run())")));

        lua_getglobal(L, "SpecLuaResults");
        const ANSICHAR* Encoded = lua_tostring(L, -1);
        TestTrue(TEXT("取回结果"), Encoded != nullptr);
        if (!Encoded)
        {
            lua_pop(L, 1);
            return;
        }

        TArray<FString> Lines;
        FString(UTF8_TO_TCHAR(Encoded)).ParseIntoArray(Lines, TEXT("\n"), false);

        int32 Failed = 0;
        int32 Executed = 0;
        for (const FString& Line : Lines)
        {
            if (Line.IsEmpty())
                continue;

            TArray<FString> Fields;
            Line.ParseIntoArray(Fields, TEXT("\t"), false);
            if (Fields.Num() < 4 || (Fields[0] != TEXT("PASS") && Fields[0] != TEXT("FAIL")))
            {
                AddError(FString::Printf(TEXT("Malformed Lua spec result: %s"), *Line));
                continue;
            }

            ++Executed;

            const bool bPassed = Fields[0] == TEXT("PASS");
            const FString& File = Fields[1];
            const FString& LineNo = Fields[2];
            const FString& Name = Fields[3];
            const FString Message = Fields.Num() > 4 ? Fields[4] : FString();

            const FString What = FString::Printf(TEXT("%s:%s %s%s%s"),
                *File, *LineNo, *Name,
                Message.IsEmpty() ? TEXT("") : TEXT(" - "), *Message);

            TestTrue(*What, bPassed);
            if (!bPassed)
                Failed++;
        }

        lua_pop(L, 1);

        // Redundant with the per-case assertions, but it keeps "everything passed because
        // nothing ran" from being a green result.
        TestTrue(TEXT("At least one Lua spec executed"), Executed > 0);
        TEST_EQUAL(Failed, 0);
    });
}

#endif

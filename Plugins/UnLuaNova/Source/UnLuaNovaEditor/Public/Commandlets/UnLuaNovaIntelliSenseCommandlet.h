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

// Writes the IntelliSense declarations without an editor window.
//
//   UnrealEditor-Cmd.exe <Project>.uproject -run=UnLuaNovaIntelliSense [-BP=1]
//       -unattended -NoSplash -stdout
//
// Why this exists: UnLuaNova.d.lua (Luau, read by luau-analyze) and the EmmyLua annotations
// (read by LuaLS) are the only thing that stops the static type check from degrading into
// "Unknown global 'UnLuaNova'" on every file. They used to be reachable only from the editor
// toolbar, so on any machine that never had a human click that button the check ran against
// nothing and the run still looked green. Tools/RunTests.ps1 runs this before the type check.
//
// -BP=1 is what turns on the second half: without it only the static exports under
// IntelliSense/StaticallyExports are refreshed, and no .d.lua is written at all.

#pragma once

#include "Commandlets/Commandlet.h"
#include "UnLuaNovaIntelliSenseCommandlet.generated.h"

UCLASS()
class UNLUANOVAEDITOR_API UUnLuaNovaIntelliSenseCommandlet : public UCommandlet
{
    GENERATED_UCLASS_BODY()

public:
    virtual int32 Main(const FString& Params) override;

private:
    void SaveFile(const FString &ModuleName, const FString &FileName, const FString &GeneratedFileContent);

    FString GeneratedDir;
};

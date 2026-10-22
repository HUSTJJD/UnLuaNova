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

#include "Commandlets/UnLuaNovaIntelliSenseCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Binding.h"
#include "Misc/FileHelper.h"
#include "UnLuaNovaIntelliSenseGenerator.h"

UUnLuaNovaIntelliSenseCommandlet::UUnLuaNovaIntelliSenseCommandlet(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Same root the generator uses (see FUnLuaNovaIntelliSenseGenerator::Initialize): the
    // declarations are IDE/type-checker input, not build scratch, so they go to the plugin's
    // own IntelliSense/ directory. SaveFile() appends "IntelliSense/<module>" to this.
    GeneratedDir = FPaths::ProjectPluginsDir();
    GeneratedDir += TEXT("UnLuaNova/");

    // The flags the other commandlets in here set, and which this one needs too: it loads the
    // asset registry and touches blueprint assets - editor-only territory - and it is meant to
    // be driven from a command line (Tools/RunTests.ps1 runs it unattended, no window).
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UUnLuaNovaIntelliSenseCommandlet::Main(const FString &Params)
{
    // class black list
    TArray<FString> ClassBlackList;
    ClassBlackList.Add("int8");
    ClassBlackList.Add("int16");
    ClassBlackList.Add("int32");
    ClassBlackList.Add("int64");
    ClassBlackList.Add("uint8");
    ClassBlackList.Add("uint16");
    ClassBlackList.Add("uint32");
    ClassBlackList.Add("uint64");
    ClassBlackList.Add("float");
    ClassBlackList.Add("double");
    ClassBlackList.Add("bool");
    ClassBlackList.Add("FName");
    ClassBlackList.Add("FString");
    ClassBlackList.Add("FText");

    // enum black list
    TArray<FString> EnumBlackList;

    // function black list
    TArray<FString> FuncBlackList;
    FuncBlackList.Add("OnModuleHotfixed");

    const auto ExportedReflectedClasses = UnLuaNova::GetExportedReflectedClasses();
    const auto ExportedNonReflectedClasses = UnLuaNova::GetExportedNonReflectedClasses();
    const auto ExportedEnums = UnLuaNova::GetExportedEnums();
    const auto ExportedFunctions = UnLuaNova::GetExportedFunctions();
    
    FString GeneratedFileContent;
    FString ModuleName(TEXT("StaticallyExports"));
    
    // reflected classes
    for (auto Pair : ExportedReflectedClasses)
    {
        if (ClassBlackList.Contains(Pair.Key))
            continue;

        Pair.Value->GenerateIntelliSense(GeneratedFileContent);
        SaveFile(ModuleName, Pair.Key, GeneratedFileContent);
    }

    for (auto Pair : ExportedNonReflectedClasses)
    {
        if (ClassBlackList.Contains(Pair.Key))
            continue;

        Pair.Value->GenerateIntelliSense(GeneratedFileContent);
        SaveFile(ModuleName, Pair.Key, GeneratedFileContent);
    }

    for (auto Enum : ExportedEnums)
    {
        if (EnumBlackList.Contains(Enum->GetName()))
            continue;

        Enum->GenerateIntelliSense(GeneratedFileContent);
        SaveFile(ModuleName, Enum->GetName(), GeneratedFileContent);
    }

    for (auto Function : ExportedFunctions)
    {
        if (FuncBlackList.Contains(Function->GetName()))
            continue;
    
        Function->GenerateIntelliSense(GeneratedFileContent);
    }
    
    SaveFile(ModuleName, TEXT("GlobalFunctions"), GeneratedFileContent);

    // generate blueprint intellisense if needed
    TArray<FString> Tokens;
    TArray<FString> Switches;
    TMap<FString, FString> ParamsMap;
    ParseCommandLine(*Params, Tokens, Switches, ParamsMap);
    FString BPKey = TEXT("BP");
    // -BP=1 is the documented form; a bare -BP asks for the same thing and is accepted so a
    // caller that forgets the "=1" does not silently get a run with no .d.lua at the end.
    const bool bExportBlueprints = (ParamsMap.Contains(BPKey) && ParamsMap[BPKey] == TEXT("1"))
        || Switches.Contains(BPKey);

    if (bExportBlueprints)
    {
        // The scan a commandlet starts with is still running when Main() is entered. Without
        // this GetAssets() returns whatever the cache held, and the blueprint half of the
        // export comes out empty on a cold cache. Native types are collected by reflection,
        // so UnLuaNova.d.lua would still be written - just without anything that exists only
        // as a blueprint.
        FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        AssetRegistryModule.Get().WaitForCompletion();

        auto Generator = FUnLuaNovaIntelliSenseGenerator::Get();
        Generator->Initialize();
        Generator->UpdateAll();

        // The caller cannot see inside UpdateAll(), and "did it write anything" is the whole
        // question when the type check comes up empty.
        UE_LOG(LogTemp, Display, TEXT("[IntelliSense] exported to %s"), *Generator->GetOutputDir());
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("[IntelliSense] static exports only - pass -BP=1 to write UnLuaNova.d.lua."));
    }

    return 0;
}

void UUnLuaNovaIntelliSenseCommandlet::SaveFile(const FString &ModuleName, const FString &FileName, const FString &GeneratedFileContent)
{
    IFileManager &FileManager = IFileManager::Get();
    FString Directory = FString::Printf(TEXT("%sIntelliSense/%s"), *GeneratedDir, *ModuleName);
    if (!FileManager.DirectoryExists(*Directory))
    {
        FileManager.MakeDirectory(*Directory);
    }

    const FString FilePath = FString::Printf(TEXT("%s/%s.lua"), *Directory, *FileName);
    FString FileContent;
    FFileHelper::LoadFileToString(FileContent, *FilePath);
    if (FileContent != GeneratedFileContent)
    {
        bool bResult = FFileHelper::SaveStringToFile(GeneratedFileContent, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
        check(bResult);
    }
}

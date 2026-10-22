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

#include "Misc/EngineVersionComparison.h"
#include "UnLuaNovaIntelliSenseGenerator.h"
#include "UnLuaNovaLuauDecls.h"
#if UE_VERSION_NEWER_THAN(5, 1, 0)
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif
#include "CoreUObject.h"
#include "UnLuaNova.h"
#include "UnLuaNovaEditorSettings.h"
#include "UnLuaNovaIntelliSense.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Blueprint.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "UnLuaNovaIntelliSenseGenerator"

TSharedPtr<FUnLuaNovaIntelliSenseGenerator> FUnLuaNovaIntelliSenseGenerator::Singleton;

TSharedRef<FUnLuaNovaIntelliSenseGenerator> FUnLuaNovaIntelliSenseGenerator::Get()
{
    if (!Singleton.IsValid())
        Singleton = MakeShareable(new FUnLuaNovaIntelliSenseGenerator);
    return Singleton.ToSharedRef();
}

void FUnLuaNovaIntelliSenseGenerator::Initialize()
{
    if (bInitialized)
        return;

    OutputDir = IPluginManager::Get().FindPlugin("UnLuaNova")->GetBaseDir() + "/Intermediate/IntelliSense";

    // Two dialects, two directories. The EmmyLua annotations are Lua and are read by LuaLS;
    // UnLuaNova.d.lua is Luau and is read by luau-analyze. Neither analyzer can parse the
    // other's dialect, so a single flat directory forced every consumer to know which files
    // to skip - and LuaLS would try to parse `declare class`, which is not valid Lua.
    EmmyLuaDir = OutputDir / TEXT("EmmyLua");
    LuauDir = OutputDir / TEXT("Luau");

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    AssetRegistryModule.Get().OnAssetAdded().AddRaw(this, &FUnLuaNovaIntelliSenseGenerator::OnAssetAdded);
    AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FUnLuaNovaIntelliSenseGenerator::OnAssetRemoved);
    AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FUnLuaNovaIntelliSenseGenerator::OnAssetRenamed);
    AssetRegistryModule.Get().OnAssetUpdated().AddRaw(this, &FUnLuaNovaIntelliSenseGenerator::OnAssetUpdated);

    bInitialized = true;
}

void FUnLuaNovaIntelliSenseGenerator::UpdateAll()
{
    const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    FARFilter Filter;
#if UE_VERSION_OLDER_THAN(5, 1, 0)
    Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
    Filter.ClassNames.Add(UWidgetBlueprint::StaticClass()->GetFName());
#else
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.ClassPaths.Add(UWidgetBlueprint::StaticClass()->GetClassPathName());
#endif

    TArray<FAssetData> BlueprintAssets;
    TArray<const UField*> NativeTypes;
    AssetRegistryModule.Get().GetAssets(Filter, BlueprintAssets);
    CollectTypes(NativeTypes);

    auto TotalCount = BlueprintAssets.Num() + NativeTypes.Num();
    if (TotalCount == 0)
        return;

    TotalCount++;

    FScopedSlowTask SlowTask(TotalCount, LOCTEXT("GeneratingBlueprintsIntelliSense", "Generating Blueprints InstelliSense"));
    SlowTask.MakeDialog();

    for (int32 i = 0; i < BlueprintAssets.Num(); i++)
    {
        if (SlowTask.ShouldCancel())
            break;
        OnAssetUpdated(BlueprintAssets[i]);
        SlowTask.EnterProgressFrame();
    }

    for (const auto Type : NativeTypes)
    {
        if (SlowTask.ShouldCancel())
            break;

        Export(Type);
        SlowTask.EnterProgressFrame();
    }

    if (SlowTask.ShouldCancel())
        return;
    ExportUE(NativeTypes);
    ExportUnLua();
    ExportLuauDecls(NativeTypes);
    SlowTask.EnterProgressFrame();
}

bool FUnLuaNovaIntelliSenseGenerator::IsBlueprint(const FAssetData& AssetData)
{
#if UE_VERSION_OLDER_THAN(5, 1, 0)
    const FName AssetClass = AssetData.AssetClass;
    return AssetClass == UBlueprint::StaticClass()->GetFName() || AssetClass == UWidgetBlueprint::StaticClass()->GetFName();
#else
    const auto AssetClassPath = AssetData.AssetClassPath.ToString();
    return AssetClassPath == UBlueprint::StaticClass()->GetName() || AssetClassPath == UWidgetBlueprint::StaticClass()->GetName();
#endif
}

bool FUnLuaNovaIntelliSenseGenerator::ShouldExport(const FAssetData& AssetData, bool bLoad)
{
    const auto& Settings = *GetDefault<UUnLuaNovaEditorSettings>();
    if (!Settings.bGenerateIntelliSense)
        return false;

    if (!IsBlueprint(AssetData))
        return false;

    const auto Asset = AssetData.FastGetAsset(bLoad);
    if (!Asset)
        return false;

    const auto Blueprint = Cast<UBlueprint>(Asset);
    if (!Blueprint)
        return false;

    if (Blueprint->SkeletonGeneratedClass || Blueprint->GeneratedClass)
        return true;

    return false;
}

void FUnLuaNovaIntelliSenseGenerator::Export(const UBlueprint* Blueprint)
{
    Export(Blueprint->GeneratedClass);
}

void FUnLuaNovaIntelliSenseGenerator::Export(const UField* Field)
{
#if ENGINE_MAJOR_VERSION > 4 || (ENGINE_MAJOR_VERSION == 4 && ENGINE_MINOR_VERSION >= 26)
    const UPackage* Package = Field->GetPackage();
#else
    const UPackage* Package = (UPackage*)Field->GetTypedOuter(UPackage::StaticClass());
#endif
    auto ModuleName = Package->GetName();
    if (!Field->IsNative())
    {
        int32 LastSlashIndex;
        if (ModuleName.FindLastChar('/', LastSlashIndex))
            ModuleName.LeftInline(LastSlashIndex);
    }
    FString FileName = UnLuaNova::IntelliSense::GetTypeName(Field);
    if (FileName.EndsWith("_C"))
        FileName.LeftChopInline(2);
    const FString Content = UnLuaNova::IntelliSense::Get(Field);
    SaveFile(ModuleName, FileName, Content);
}

void FUnLuaNovaIntelliSenseGenerator::ExportUE(const TArray<const UField*> Types)
{
    const FString Content = UnLuaNova::IntelliSense::GetUE(Types);
    SaveFile("", "UE", Content);
}

void FUnLuaNovaIntelliSenseGenerator::ExportUnLua()
{
    const auto ContentDir = IPluginManager::Get().FindPlugin(TEXT("UnLuaNova"))->GetContentDir();
    const auto SrcDir = ContentDir / "IntelliSense";
    const auto DstDir = EmmyLuaDir;

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    if (!PlatformFile.DirectoryExists(*SrcDir))
        return;

    PlatformFile.CopyDirectoryTree(*DstDir, *SrcDir, true);
}

void FUnLuaNovaIntelliSenseGenerator::ExportLuauDecls(const TArray<const UField*> Types)
{
    // LuauDecl lives inside namespace UnLuaNova (see UnLuaNovaLuauDecls.h); this
    // function is defined at global scope, so the name has to be qualified.
    const FString Content = UnLuaNova::LuauDecl::Generate(Types);

    IFileManager& FileManager = IFileManager::Get();
    if (!FileManager.DirectoryExists(*LuauDir))
    {
        FileManager.MakeDirectory(*LuauDir);
    }

    // .d.lua rather than .lua: these are declarations for luau-analyze, not something
    // a script is meant to require at runtime. It lives under Luau/ so LuaLS - which
    // points at EmmyLua/ - never sees Luau syntax.
    const FString FilePath = LuauDir / TEXT("UnLuaNova.d.lua");

    FString Existing;
    FFileHelper::LoadFileToString(Existing, *FilePath);
    if (Existing != Content)
    {
        FFileHelper::SaveStringToFile(Content, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    }
}

void FUnLuaNovaIntelliSenseGenerator::CollectTypes(TArray<const UField*>& Types)
{
    for (TObjectIterator<UClass> It; It; ++It)
    {
        const UClass* Class = *It;
        const FString ClassName = Class->GetName();
        // ReSharper disable StringLiteralTypo
        if (ClassName.StartsWith("SKEL_")
            || ClassName.StartsWith("PLACEHOLDER-CLASS")
            || ClassName.StartsWith("REINST_")
            || ClassName.StartsWith("TRASHCLASS_")
            || ClassName.StartsWith("HOTRELOADED_")
        )
        {
            // skip nonsense types
            continue;
        }
        // ReSharper restore StringLiteralTypo
        Types.Add(Class);
    }

    for (TObjectIterator<UScriptStruct> It; It; ++It)
    {
        const UScriptStruct* ScriptStruct = *It;
        Types.Add(ScriptStruct);
    }

    for (TObjectIterator<UEnum> It; It; ++It)
    {
        const UEnum* Enum = *It;
        Types.Add(Enum);
    }
}

void FUnLuaNovaIntelliSenseGenerator::SaveFile(const FString& ModuleName, const FString& FileName, const FString& GeneratedFileContent)
{
    IFileManager& FileManager = IFileManager::Get();
    const FString Directory = EmmyLuaDir / ModuleName;
    if (!FileManager.DirectoryExists(*Directory))
        FileManager.MakeDirectory(*Directory);

    const FString FilePath = FString::Printf(TEXT("%s/%s.lua"), *Directory, *FileName);
    FString FileContent;
    FFileHelper::LoadFileToString(FileContent, *FilePath);
    if (FileContent != GeneratedFileContent)
        FFileHelper::SaveStringToFile(GeneratedFileContent, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

void FUnLuaNovaIntelliSenseGenerator::DeleteFile(const FString& ModuleName, const FString& FileName)
{
    IFileManager& FileManager = IFileManager::Get();
    const FString Directory = EmmyLuaDir / ModuleName;
    if (!FileManager.DirectoryExists(*Directory))
        FileManager.MakeDirectory(*Directory);

    const FString FilePath = FString::Printf(TEXT("%s/%s.lua"), *Directory, *FileName);
    if (FileManager.FileExists(*FilePath))
        FileManager.Delete(*FilePath);
}

void FUnLuaNovaIntelliSenseGenerator::OnAssetAdded(const FAssetData& AssetData)
{
    if (!ShouldExport(AssetData))
        return;

    OnAssetUpdated(AssetData);

    TArray<const UField*> Types;
    CollectTypes(Types);
    ExportUE(Types);
}

void FUnLuaNovaIntelliSenseGenerator::OnAssetRemoved(const FAssetData& AssetData)
{
    if (!ShouldExport(AssetData))
        return;

    DeleteFile(FString("/Game"), AssetData.AssetName.ToString());
}

void FUnLuaNovaIntelliSenseGenerator::OnAssetRenamed(const FAssetData& AssetData, const FString& OldPath)
{
    if (!ShouldExport(AssetData))
        return;

    //remove old Blueprint name
    const FString OldPackageName = FPackageName::GetShortName(OldPath);
    DeleteFile("/Game", OldPackageName);

    //update new name 
    OnAssetUpdated(AssetData);
}

void FUnLuaNovaIntelliSenseGenerator::OnAssetUpdated(const FAssetData& AssetData)
{
    if (!ShouldExport(AssetData, true))
        return;

    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetData.GetSoftObjectPath().ToString());
    if (!Blueprint)
        return;

    Export(Blueprint);
}

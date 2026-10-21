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

// Repairs the test assets that the UnLua -> UnLuaNova rename left behind, so the suite can
// be run without anybody opening the editor.
//
// The rename touched text - sources, .lua, ini - and could not touch the .uasset binaries.
// What that left is three kinds of damage, all of which need an editor process, because
// they need the Blueprint compiler and the package saver:
//
//   1. Blueprints whose ParentClass import resolves to nothing, so the compiler answers
//      "has missing or NULL parent class" and there is nothing to compile. The parent
//      cannot be recovered from the asset - that is the part that is gone - so it is
//      listed here and put back before compiling.
//   2. Blueprints saved without being compiled: no SKEL_*, no Default__*, no
//      BlueprintGeneratedClass export in the package, so `Class ..._C` cannot be found and
//      SpawnActor/NewObject get a nil class. Compiling and saving is the whole fix, and it
//      takes more than one pass: a Blueprint that calls a function in another Blueprint
//      cannot compile until that one has.
//   3. DataTables whose RowStruct is gone, so the engine logs "Missing RowStruct" at
//      *Error* verbosity and the automation framework counts that as a failure for
//      whichever test happens to be running.
//
// Issue634 is deliberately not touched: its graph references a struct asset that no longer
// exists at all, and deleting a pin from a graph here would be guess work. It is reported.
//
// It is an automation test rather than a commandlet because it then runs through the
// harness that is already wired up (Tools/RunTests.ps1) and reports what it did and what is
// still broken into the same place everything else lands. It is deliberately *not* under
// the UnLuaNova.API filter the suite runs by default - it rewrites assets, so it is opt-in:
//
//   Tools\RunTests.ps1 -Backend Luau -Filter UnLuaNova.Editor
//
// Note that it can come back "failed" while doing its job: the Blueprint compiler logs
// errors verbosely, and an Error logged while a test runs is enough for the automation
// framework to mark it red. The counts in the Info line are what tell you what got fixed.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "UnLuaNovaTestHelpers.h" // FUnLuaNovaTestTableRow
#include "Tests/Issue583Test.h"   // FIssue583Row / FIssue583Record

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"

namespace
{
    // --- 1. Blueprints that lost their parent -----------------------------------------
    // Package -> the class it is supposed to derive from. Written down because the asset
    // no longer says, and nothing else in the repository does either.
    struct FParentRepair
    {
        const TCHAR* PackagePath;
        const TCHAR* ParentPath;
    };

    const FParentRepair GParentRepairs[] = {
        // Binding: the child of the static-binding actor, used by the "multiple bindings"
        // and "overridden" tests.
        {TEXT("/UnLuaNovaTestSuite/Tests/Binding/BP_UnLuaNovaTestActor_StaticBindingChild"),
         TEXT("/UnLuaNovaTestSuite/Tests/Binding/BP_UnLuaNovaTestActor_StaticBinding.BP_UnLuaTestActor_StaticBinding_C")},

        // Issue314: the child of the actor that reads a field the parent does not have.
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Child"),
         TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Parent.BP_UnLuaTestActor_Issue314_Parent_C")},

        // Issue398: spawned as a character and passed around as an AActor; the folder holds
        // no other actor Blueprint for it to derive from.
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue398/BP_Character"),
         TEXT("/Script/Engine.Character")},
    };

    // --- 3. DataTables that lost their RowStruct ---------------------------------------
    struct FTableRepair
    {
        const TCHAR* PackagePath;
        UScriptStruct* (*RowStruct)();
        void (*Fill)(UDataTable*);
    };

    void FillCppTestTable(UDataTable* Table)
    {
        FUnLuaNovaTestTableRow Row;
        Row.Title = TEXT("Hello"); // what LuaLib_DataTable.spec.cpp asks for
        Row.Level = 1;
        Table->AddRow(TEXT("Row_1"), Row);
    }

    void FillIssue583Table(UDataTable* Table)
    {
        FIssue583Record Record;
        Record.Description = FText::FromString(TEXT("Record_1"));
        Record.Flag = 200;

        FIssue583Row Row;
        Row.Records.Add(TEXT("Record_1"), Record);
        Table->AddRow(TEXT("Row_1"), Row);
    }

    const FTableRepair GTableRepairs[] = {
        {TEXT("/UnLuaNovaTestSuite/Tests/Misc/DataTable_CppTest"),
         &FUnLuaNovaTestTableRow::StaticStruct,
         &FillCppTestTable},
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue583/DataTable_Issue583"),
         &FIssue583Row::StaticStruct,
         &FillIssue583Table},
    };

    UObject* LoadByPath(const TCHAR* Path)
    {
        return StaticLoadObject(UObject::StaticClass(), nullptr, Path);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUnLuaNovaRepairTestAssets,
    "UnLuaNova.Editor.RepairTestAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FUnLuaNovaRepairTestAssets::RunTest(const FString& Parameters)
{
    const FString Root = TEXT("/UnLuaNovaTestSuite/");
    TArray<UPackage*> DirtyPackages;
    int32 NumReparented = 0;
    int32 NumTablesRepaired = 0;

    // --- 1. put the missing parents back ---------------------------------------------
    for (const FParentRepair& Repair : GParentRepairs)
    {
        UBlueprint* Blueprint = Cast<UBlueprint>(LoadByPath(Repair.PackagePath));
        if (!Blueprint)
        {
            AddWarning(FString::Printf(TEXT("%s is not a Blueprint - parent not repaired."), Repair.PackagePath));
            continue;
        }

        if (Blueprint->ParentClass != nullptr)
        {
            continue; // nothing to put back
        }

        UClass* Parent = LoadClass<UObject>(nullptr, Repair.ParentPath);
        if (!Parent)
        {
            AddWarning(FString::Printf(TEXT("%s: intended parent %s could not be loaded."),
                Repair.PackagePath, Repair.ParentPath));
            continue;
        }

        Blueprint->ParentClass = Parent;
        ++NumReparented;
        DirtyPackages.AddUnique(Blueprint->GetOutermost());
    }

    // --- 2. compile every Blueprint in the test content --------------------------------
    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

    TArray<FAssetData> Assets;
    AssetRegistryModule.Get().GetAssetsByPath(FName(*Root), Assets, /*bRecursive=*/true);

    if (Assets.Num() == 0)
    {
        // Nothing found usually means the registry had not finished its initial scan when
        // automation started; ask for a synchronous one rather than quietly repairing
        // nothing and calling it a pass.
        AssetRegistryModule.Get().SearchAllAssets(/*bSynchronous=*/true);
        AssetRegistryModule.Get().GetAssetsByPath(FName(*Root), Assets, /*bRecursive=*/true);
    }

    TArray<UBlueprint*> Blueprints;
    for (const FAssetData& Asset : Assets)
    {
        if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset()))
        {
            Blueprints.Add(Blueprint);
        }
    }

    // Two passes: a Blueprint that calls a function defined in another one fails until
    // that one has been compiled, and one pass over an unordered list cannot guarantee it.
    for (int32 Pass = 0; Pass < 2; ++Pass)
    {
        for (UBlueprint* Blueprint : Blueprints)
        {
            FKismetEditorUtilities::CompileBlueprint(Blueprint);
        }
    }

    int32 NumCompiled = 0;
    TArray<FString> StillBroken;
    for (UBlueprint* Blueprint : Blueprints)
    {
        if (Blueprint->Status == BS_Error || Blueprint->Status == BS_Unknown)
        {
            StillBroken.Add(Blueprint->GetPathName());
            continue;
        }

        ++NumCompiled;
        DirtyPackages.AddUnique(Blueprint->GetOutermost());
    }

    // --- 3. the DataTables that lost their RowStruct -----------------------------------
    for (const FTableRepair& Repair : GTableRepairs)
    {
        UDataTable* Table = Cast<UDataTable>(LoadByPath(Repair.PackagePath));
        if (!Table)
        {
            AddWarning(FString::Printf(TEXT("%s is not a DataTable - nothing to repair."), Repair.PackagePath));
            continue;
        }

        if (Table->RowStruct == Repair.RowStruct())
        {
            continue; // already right: keep its rows
        }

        // Empty *before* swapping the struct in, and only when there is something to empty.
        //
        // UDataTable::EmptyTable() destroys each row with GetEmptyUsingStruct(), which is
        // the table's RowStruct - so calling it after the swap runs the *new* struct's
        // destructor over bytes that were never laid out for it. That was an
        // EXCEPTION_ACCESS_VIOLATION inside UScriptStruct::DestroyStruct. Before the swap
        // it falls back to FTableRowBase, which has nothing to destroy.
        //
        // Skipped when there are no rows because GetEmptyUsingStruct() reports
        // "Missing RowStruct" at Error verbosity on the way in, and an Error logged while a
        // test runs is enough to fail it.
        if (Table->GetRowNames().Num() > 0)
        {
            Table->EmptyTable();
        }

        Table->RowStruct = Repair.RowStruct();
        Repair.Fill(Table);
        ++NumTablesRepaired;
        DirtyPackages.AddUnique(Table->GetOutermost());
    }

    // --- 4. write it all back ---------------------------------------------------------
    if (DirtyPackages.Num() > 0)
    {
        UEditorLoadingAndSavingUtils::SavePackages(DirtyPackages, /*bOnlyDirty=*/false);
    }

    AddInfo(FString::Printf(
        TEXT("Repaired: %d parent class(es), %d DataTable(s). Compiled %d of %d Blueprint(s); saved %d package(s)."),
        NumReparented, NumTablesRepaired, NumCompiled, Blueprints.Num(), DirtyPackages.Num()));

    for (const FString& Path : StillBroken)
    {
        // Reported rather than failed on: a Blueprint that still will not compile needs a
        // human to look at its graph (Issue634 is the known one), and naming the asset is
        // the useful half.
        AddWarning(FString::Printf(TEXT("Still does not compile: %s"), *Path));
    }

    return true;
}

#endif

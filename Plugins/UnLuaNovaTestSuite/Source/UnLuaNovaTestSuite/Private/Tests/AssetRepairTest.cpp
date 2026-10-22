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
//   4. Levels that lost actors outright. An actor whose class was a renamed Blueprint could
//      not resolve, so the level was saved back without it: no actor, no BeginPlay, and the
//      Lua module bound to that class never runs - which is why the Issue286/300/401/443/
//      453 regressions read their result globals as nil. The class is still on disk under
//      its new path, so the actor is spawned back in.
//   5. Interface implementations that lost the graph list mapping each interface function
//      to the graph implementing it (BP_BlueprintBenchmark kept one graph out of fifteen).
//
// Four assets are deliberately left alone and skipped instead of compiled:
//
//   - The three Benchmark Blueprints. Performance fixtures no automation test loads; the
//     resave took variables and function graphs out of them that cannot be reconstructed
//     from what is left, so they can only be rebuilt by hand in the editor.
//   - The Benchmark level, which exists only to hold the three above.
//
// They are named rather than detected, because "does not compile" is not a good enough
// test: it is exactly the state this test exists to get assets out of.
//
// Two more kinds of damage are deliberately left alone, for the opposite reason - they
// belong to assets nothing loads unless this test asks for it, and asking for them is what
// puts their compiler errors in *this* test's log:
//
//   - Levels (Issue500, Issue634) are not loaded by the scan below. Loading one compiles
//     its level Blueprint, and both of those have nodes the rename left pointing at types
//     that no longer exist - a Blueprint function library reference in Issue500, and in
//     Issue634 the actor this test does repair, whose level calls it. Nothing in the scan
//     needs a World: the levels it writes to are loaded by name further down.
//   - Issue494 has no test at all - no .cpp, no Lua module - so nothing loads its level
//     either, and its level Blueprint (a SpawnActor node with no class, an interface call
//     whose cast lost its type) is left as it is.
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
#include "Tests/Issue634Test.h"   // FIssue634Struct
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "K2Node_MakeStruct.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/UObjectHash.h"

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

    // --- 1c. Assets past the point of repair ---------------------------------------------
    // See the note at the top of the file. These are left out of the compile pass: compiling
    // them cannot succeed and only fills the log with errors, which the automation framework
    // counts against this test.
    const TCHAR* const GUnrepairablePackages[] = {
        TEXT("/UnLuaNovaTestSuite/Tests/Benchmark/Benchmark"), // the level: loads the three below
        TEXT("/UnLuaNovaTestSuite/Tests/Benchmark/BP_BlueprintBenchmark"),
        TEXT("/UnLuaNovaTestSuite/Tests/Benchmark/BP_BlueprintToBlueprintBenchmark"),
        TEXT("/UnLuaNovaTestSuite/Tests/Benchmark/BP_NativizedBlueprintBenchmark"),
    };

    static bool IsUnrepairable(const FString& PackagePath)
    {
        for (const TCHAR* Package : GUnrepairablePackages)
        {
            if (PackagePath == Package)
            {
                return true;
            }
        }
        return false;
    }

    // --- 2b. Actors that the rename took out of the levels ------------------------------
    // Every level below had an actor of a Blueprint whose *file* was renamed
    // (BP_UnLuaTestActor_IssueNNN -> BP_UnLuaNovaTestActor_IssueNNN). The actor kept a
    // reference to the old package, which no longer exists, so the next time the level was
    // saved the actor was written back out as nothing at all - the class is gone from the
    // level, BeginPlay never runs, and the Lua module that was bound to it never executes.
    // The class itself is still there under its new path, so the actor can be put back; the
    // tests only need it to exist and tick, not to be at any particular transform.
    //
    // The object name inside the package is still the pre-rename one, hence
    // <new file name>.<old object name>_C.
    struct FActorRestore
    {
        const TCHAR* MapPath;   // <package>.<world>, so LoadObject gets the UWorld
        const TCHAR* ClassPath; // full path of the class to place
    };

    // The Benchmark level is not in here: it is one of the assets past repair (see below),
    // and loading it would compile the three broken Benchmark Blueprints placed in it.
    //
    // Issue494 is not in here either, although it lost its actor the same way: there is no
    // Issue494 test (no .cpp, no Lua module - only the interface its level Blueprint calls
    // into), so nothing needs the actor back, while loading the level compiles a level
    // Blueprint with a SpawnActor node that has no class and a cast that lost its type,
    // and those errors land on this test.
    const FActorRestore GActorRestores[] = {
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue286/Issue286.Issue286"),
         TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue286/BP_UnLuaNovaTestActor_Issue286.BP_UnLuaTestActor_Issue286_C")},
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue300/Issue300.Issue300"),
         TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue300/BP_UnLuaNovaTestActor_Issue300.BP_UnLuaTestActor_Issue300_C")},
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue343/Map3.Map3"),
         TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue343/BP_UnLuaNovaTestActor_Issue343.BP_UnLuaTestActor_Issue343_C")},
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue401/Issue401.Issue401"),
         TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue401/BP_UnLuaNovaTestActor_Issue401.BP_UnLuaTestActor_Issue401_C")},
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue443/Issue443.Issue443"),
         TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue443/BP_UnLuaNovaTestActor_Issue443.BP_UnLuaTestActor_Issue443_C")},
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue453/Issue453.Issue453"),
         TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue453/BP_UnLuaNovaTestActor_Issue453.BP_UnLuaTestActor_Issue453_C")},
    };

    // --- 2c. Components the resave dropped as well ---------------------------------------
    // Losing the parent class took the component templates with it: a component whose owner
    // class cannot be resolved has nothing valid to attach to, so the resave left the
    // SimpleConstructionScript node out. Issue314 is the one a test looks at - its child
    // class is supposed to have a 'Cube' that the parent does not.
    struct FComponentRestore
    {
        const TCHAR* PackagePath;
        const TCHAR* ComponentName;
        const TCHAR* StaticMeshPath; // nullptr for a plain scene component
    };

    const FComponentRestore GComponentRestores[] = {
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Child"),
         TEXT("Cube"), TEXT("/Engine/BasicShapes/Cube.Cube")},
    };

    // --- 2d. Struct types the rename took out of the graphs -------------------------------
    // A K2Node_MakeStruct names its struct through the module it was compiled against. The
    // struct is a C++ one, so renaming the module moved it: the reference in the asset
    // resolves to nothing, the node has no output pin, the function result pin fed by it has
    // no type, and the compiler answers "No Struct in Make <unknown struct>" - which is
    // enough for every test that loads the asset to be marked failed.
    //
    // The struct itself is still in the test module, so it can be put back: on the node
    // (UK2Node_StructOperation::StructType), and on every pin that says "struct" without
    // saying which one, which is what the function result pin looks like once the node
    // feeding it lost its type.
    struct FStructRepair
    {
        const TCHAR* PackagePath;
        UScriptStruct* (*Struct)();
    };

    const FStructRepair GStructRepairs[] = {
        {TEXT("/UnLuaNovaTestSuite/Tests/Regression/Issue634/BP_Issue634Actor"),
         &FIssue634Struct::StaticStruct},
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

    // A Blueprint implementing an interface keeps, per interface, the list of graphs that
    // implement its functions (FBPInterfaceDescription::Graphs). Re-saving while the
    // interface reference was unresolved dropped that list: BP_BlueprintBenchmark kept one
    // graph out of fifteen, so the compiler stopped seeing those functions and every call
    // to them failed with "Could not find a function named ...".
    //
    // The graphs themselves survived, and they are named after the function they override,
    // so the mapping can be rebuilt by name.
    int32 RelinkInterfaceGraphs(UBlueprint* Blueprint)
    {
        int32 NumRelinked = 0;

        for (FBPInterfaceDescription& Description : Blueprint->ImplementedInterfaces)
        {
            const UClass* InterfaceClass = Description.Interface;
            if (!InterfaceClass)
            {
                continue;
            }

            for (TFieldIterator<UFunction> It(InterfaceClass, EFieldIteratorFlags::IncludeSuper); It; ++It)
            {
                const FName FunctionName = (*It)->GetFName();

                for (UEdGraph* Graph : Blueprint->FunctionGraphs)
                {
                    if (Graph && Graph->GetFName() == FunctionName && Description.Graphs.AddUnique(Graph) != INDEX_NONE)
                    {
                        ++NumRelinked;
                    }
                }
            }
        }

        if (NumRelinked > 0)
        {
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        }

        return NumRelinked;
    }

    int32 RestoreMissingComponents(const TMap<FString, UBlueprint*>& BlueprintsByPackage,
        TArray<FString>& Warnings, TArray<FString>& Diagnostics)
    {
        int32 NumRestored = 0;

        for (const FComponentRestore& Restore : GComponentRestores)
        {
            UBlueprint* const* Found = BlueprintsByPackage.Find(Restore.PackagePath);
            UBlueprint* Blueprint = Found ? *Found : nullptr;
            if (!Blueprint)
            {
                Warnings.Add(FString::Printf(TEXT("%s is not a Blueprint - component not restored."), Restore.PackagePath));
                continue;
            }

            USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;
            if (!SCS)
            {
                // The resave dropped the construction script along with the nodes in it.
                SCS = NewObject<USimpleConstructionScript>(Blueprint);
                Blueprint->SimpleConstructionScript = SCS;
                Diagnostics.Add(FString::Printf(TEXT("%s: re-created the construction script"), *Blueprint->GetName()));
            }
            SCS->ValidateSceneRootNodes();

            const FName VariableName(Restore.ComponentName);
            bool bAlreadyThere = false;
            for (USCS_Node* Node : SCS->GetAllNodes())
            {
                if (Node && Node->GetVariableName() == VariableName)
                {
                    bAlreadyThere = true;
                    break;
                }
            }
            if (bAlreadyThere)
            {
                continue;
            }

            USCS_Node* Node = SCS->CreateNode(UStaticMeshComponent::StaticClass(), VariableName);
            if (!Node)
            {
                Warnings.Add(FString::Printf(TEXT("%s: could not create component %s."), Restore.PackagePath, Restore.ComponentName));
                continue;
            }

            if (UStaticMeshComponent* Template = Cast<UStaticMeshComponent>(Node->ComponentTemplate))
            {
                if (Restore.StaticMeshPath)
                {
                    Template->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Restore.StaticMeshPath));
                }
            }

            SCS->AddNode(Node);
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            ++NumRestored;

            Diagnostics.Add(FString::Printf(TEXT("%s: re-added component '%s'"), *Blueprint->GetName(), Restore.ComponentName));
        }

        return NumRestored;
    }

    int32 RestoreMissingStructs(const TMap<FString, UBlueprint*>& BlueprintsByPackage,
        TArray<FString>& Warnings, TArray<FString>& Diagnostics)
    {
        int32 NumRepaired = 0;

        for (const FStructRepair& Repair : GStructRepairs)
        {
            UBlueprint* const* Found = BlueprintsByPackage.Find(Repair.PackagePath);
            UBlueprint* Blueprint = Found ? *Found : nullptr;
            if (!Blueprint)
            {
                Warnings.Add(FString::Printf(TEXT("%s is not a Blueprint - struct not restored."), Repair.PackagePath));
                continue;
            }

            UScriptStruct* const Struct = Repair.Struct();
            TArray<UEdGraph*> Graphs;
            Blueprint->GetAllGraphs(Graphs);

            int32 NumNodes = 0;
            int32 NumPins = 0;
            for (UEdGraph* Graph : Graphs)
            {
                if (!Graph)
                {
                    continue;
                }

                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (!Node)
                    {
                        continue;
                    }

                    if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(Node))
                    {
                        if (!StructNode->StructType)
                        {
                            StructNode->StructType = Struct;
                            ++NumNodes;
                        }
                    }

                    for (UEdGraphPin* Pin : Node->Pins)
                    {
                        // "struct" with no struct: the pin kept its category and lost the
                        // type it was carrying, which is what a pin fed by a struct node
                        // that lost its struct looks like.
                        if (Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct
                            && !Pin->PinType.PinSubCategoryObject.IsValid())
                        {
                            Pin->PinType.PinSubCategoryObject = Struct;
                            ++NumPins;
                        }
                    }
                }
            }

            if (NumNodes == 0 && NumPins == 0)
            {
                continue; // already right, or never had a struct to lose
            }

            // The pins have to be rebuilt from the struct: a MakeStruct node with no struct
            // has an empty pin list, so retyping it is only half the repair - the member
            // pins come back when the node is reconstructed. Links survive it, because the
            // node recognises its own pins by name (DoPinsMatchForReconstruction).
            for (UEdGraph* Graph : Graphs)
            {
                if (!Graph)
                {
                    continue;
                }

                for (UEdGraphNode* Node : Graph->Nodes)
                {
                    if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(Node))
                    {
                        if (StructNode->StructType == Struct)
                        {
                            StructNode->ReconstructNode();
                        }
                    }
                }
            }

            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
            ++NumRepaired;

            Diagnostics.Add(FString::Printf(TEXT("%s: re-typed %d struct node(s) and %d pin(s) to %s"),
                *Blueprint->GetName(), NumNodes, NumPins, *Struct->GetName()));
        }

        return NumRepaired;
    }

    int32 RestoreMissingActors(TArray<UPackage*>& DirtyPackages, TArray<FString>& Warnings)
    {
        int32 NumRestored = 0;

        for (const FActorRestore& Restore : GActorRestores)
        {
            UWorld* World = LoadObject<UWorld>(nullptr, Restore.MapPath);
            if (!World || !World->PersistentLevel)
            {
                Warnings.Add(FString::Printf(TEXT("%s is not a World - actor not restored."), Restore.MapPath));
                continue;
            }

            UClass* Class = LoadClass<AActor>(nullptr, Restore.ClassPath);
            if (!Class)
            {
                Warnings.Add(FString::Printf(TEXT("%s: %s could not be loaded - actor not restored."),
                    Restore.MapPath, Restore.ClassPath));
                continue;
            }

            bool bAlreadyThere = false;
            for (AActor* Actor : World->PersistentLevel->Actors)
            {
                if (Actor && Actor->GetClass() == Class)
                {
                    bAlreadyThere = true;
                    break;
                }
            }
            if (bAlreadyThere)
            {
                continue; // already repaired, or never lost
            }

            FActorSpawnParameters Params;
            Params.OverrideLevel = World->PersistentLevel;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Params.bNoFail = true;

            AActor* Actor = Cast<AActor>(World->SpawnActor(Class, &FTransform::Identity, Params));
            if (!Actor)
            {
                Warnings.Add(FString::Printf(TEXT("%s: spawning %s failed."), Restore.MapPath, Restore.ClassPath));
                continue;
            }

            ++NumRestored;
            DirtyPackages.AddUnique(World->GetOutermost());
        }

        return NumRestored;
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
    TArray<FString> Warnings;
    TArray<FString> Diagnostics;
    int32 NumReparented = 0;
    int32 NumTablesRepaired = 0;
    int32 NumInterfaceGraphsRelinked = 0;

    // --- 0. every asset in the test content, keyed by package ---------------------------
    // Loading by package path alone does not work on the renamed assets: the file is called
    // BP_UnLuaNovaTestActor_Issue300 but the object inside it is still BP_UnLuaTestActor_
    // Issue300, so "/…/BP_UnLuaNovaTestActor_Issue300" asks for an object that does not
    // exist. The asset registry knows the real object path, so it is the only reliable way
    // to get from a package name to the asset in it.
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
    TMap<FString, UBlueprint*> BlueprintsByPackage;
    TArray<FString> Skipped;
    for (const FAssetData& Asset : Assets)
    {
        const FString Package = Asset.PackageName.ToString();

        // Skipped before loading rather than after: loading a Blueprint compiles it, and
        // compiling one of these fills the log with the very errors this test is judged on.
        if (IsUnrepairable(Package))
        {
            Skipped.Add(Package);
            continue;
        }

        // Worlds are left unloaded: loading one compiles its level Blueprint, and two of
        // the test levels have graphs the rename left pointing at types that are gone. That
        // is their business - no test of theirs is running - except that the compiler says
        // so at Error verbosity, and an Error logged while this test runs fails it. Nothing
        // here needs a World: the levels it puts actors back into are loaded by name below.
        if (Asset.AssetClassPath == UWorld::StaticClass()->GetClassPathName())
        {
            continue;
        }

        if (UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset()))
        {
            Blueprints.Add(Blueprint);
            BlueprintsByPackage.Add(Package, Blueprint);
        }
    }

    UBlueprint* const* FindBlueprint = nullptr;

    // --- 1. put the missing parents back ---------------------------------------------
    for (const FParentRepair& Repair : GParentRepairs)
    {
        FindBlueprint = BlueprintsByPackage.Find(Repair.PackagePath);
        UBlueprint* Blueprint = FindBlueprint ? *FindBlueprint : nullptr;
        if (!Blueprint)
        {
            Warnings.Add(FString::Printf(TEXT("%s is not a Blueprint - parent not repaired."), Repair.PackagePath));
            continue;
        }

        if (Blueprint->ParentClass != nullptr)
        {
            continue; // nothing to put back
        }

        UClass* Parent = LoadClass<UObject>(nullptr, Repair.ParentPath);
        if (!Parent)
        {
            Warnings.Add(FString::Printf(TEXT("%s: intended parent %s could not be loaded."),
                Repair.PackagePath, Repair.ParentPath));
            continue;
        }

        Blueprint->ParentClass = Parent;
        ++NumReparented;
        DirtyPackages.AddUnique(Blueprint->GetOutermost());
    }

    // --- 1b. re-link the graphs the rename dropped -------------------------------------
    for (UBlueprint* Blueprint : Blueprints)
    {
        NumInterfaceGraphsRelinked += RelinkInterfaceGraphs(Blueprint);
    }

    const int32 NumComponentsRestored = RestoreMissingComponents(BlueprintsByPackage, Warnings, Diagnostics);

    // Before compiling: a struct node without a struct cannot compile, and it takes the pin
    // it feeds - and any pin it is linked to - down with it.
    const int32 NumStructsRestored = RestoreMissingStructs(BlueprintsByPackage, Warnings, Diagnostics);

    // --- 2. compile every Blueprint in the test content --------------------------------
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

    // --- 2b. put the actors the rename took out of the levels back ----------------------
    const int32 NumActorsRestored = RestoreMissingActors(DirtyPackages, Warnings);

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
        TEXT("Repaired: %d parent class(es), %d DataTable(s), %d interface graph(s), %d component(s), %d struct type(s), %d actor(s). Compiled %d of %d Blueprint(s), skipped %d past repair; saved %d package(s)."),
        NumReparented, NumTablesRepaired, NumInterfaceGraphsRelinked,
        NumComponentsRestored, NumStructsRestored, NumActorsRestored, NumCompiled, Blueprints.Num(), Skipped.Num(),
        DirtyPackages.Num()));

    for (const FString& Path : StillBroken)
    {
        // Reported rather than failed on: a Blueprint that still will not compile needs a
        // human to look at its graph, and naming the asset is the useful half.
        AddWarning(FString::Printf(TEXT("Still does not compile: %s"), *Path));
    }

    for (const FString& Path : Skipped)
    {
        AddWarning(FString::Printf(TEXT("Skipped, cannot be repaired here: %s"), *Path));
    }

    for (const FString& Warning : Warnings)
    {
        AddWarning(Warning);
    }

    for (const FString& Line : Diagnostics)
    {
        AddInfo(Line);
    }

    return true;
}

#endif

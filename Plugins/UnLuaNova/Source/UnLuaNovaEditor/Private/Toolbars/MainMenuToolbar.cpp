// Tencent is pleased to support the open source community by making UNLUANOVA available.
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

#include "MainMenuToolbar.h"
#include "ISettingsModule.h"
#include "UNLUANOVAAboutScreen.h"
#include "UNLUANOVAIntelliSenseGenerator.h"
#include "LevelEditor.h"
#include "UNLUANOVAEditorCommands.h"
#include "UNLUANOVAEditorFunctionLibrary.h"
#include "UNLUANOVAEditorSettings.h"
#include "UNLUANOVAFunctionLibrary.h"
#include "Interfaces/IMainFrameModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "UNLUANOVAMainMenuToolbar"

FMainMenuToolbar::FMainMenuToolbar()
    : CommandList(new FUICommandList)
{
    CommandList->MapAction(FUNLUANOVAEditorCommands::Get().HotReload, FExecuteAction::CreateStatic(UUNLUANOVAFunctionLibrary::HotReload), FCanExecuteAction());

    CommandList->MapAction(FUNLUANOVAEditorCommands::Get().GenerateIntelliSense, FExecuteAction::CreateLambda([]
    {
        FUNLUANOVAIntelliSenseGenerator::Get()->UpdateAll();
    }), FCanExecuteAction());

    CommandList->MapAction(FUNLUANOVAEditorCommands::Get().OpenRuntimeSettings, FExecuteAction::CreateLambda([]
    {
        if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
            SettingsModule->ShowViewer("Project", "Plugins", "UNLUANOVA");
    }), FCanExecuteAction());

    CommandList->MapAction(FUNLUANOVAEditorCommands::Get().OpenEditorSettings, FExecuteAction::CreateLambda([]
    {
        if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
            SettingsModule->ShowViewer("Project", "Plugins", "UNLUANOVA Editor");
    }), FCanExecuteAction());

    CommandList->MapAction(FUNLUANOVAEditorCommands::Get().ReportIssue, FExecuteAction::CreateLambda([]
    {
        const TCHAR* URL = TEXT("cmd");
        const TCHAR* Params = TEXT("/k start https://github.com/Tencent/UNLUANOVA/issues/new/choose");
        FPlatformProcess::ExecProcess(URL, Params, nullptr, nullptr, nullptr);
    }), FCanExecuteAction());

    CommandList->MapAction(FUNLUANOVAEditorCommands::Get().About, FExecuteAction::CreateLambda([]
    {
        const FText AboutWindowTitle = LOCTEXT("AboutUnLua", "About UNLUANOVA");

        TSharedPtr<SWindow> AboutWindow =
            SNew(SWindow)
            .Title(AboutWindowTitle)
            .ClientSize(FVector2D(600.f, 200.f))
            .SupportsMaximize(false).SupportsMinimize(false)
            .SizingRule(ESizingRule::FixedSize)
            [
                SNew(SUnLuaAboutScreen)
            ];

        IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
        TSharedPtr<SWindow> ParentWindow = MainFrame.GetParentWindow();

        if (ParentWindow.IsValid())
            FSlateApplication::Get().AddModalWindow(AboutWindow.ToSharedRef(), ParentWindow.ToSharedRef());
        else
            FSlateApplication::Get().AddWindow(AboutWindow.ToSharedRef());
    }), FCanExecuteAction());
}

void FMainMenuToolbar::Initialize()
{
#if ENGINE_MAJOR_VERSION >= 5
    UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
    FToolMenuSection& Section = ToolbarMenu->AddSection("UNLUANOVASettings");
    Section.AddEntry(FToolMenuEntry::InitComboButton(
		"UNLUANOVASettings",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FMainMenuToolbar::GenerateUnLuaSettingsMenu),
		LOCTEXT("UNLUANOVA_Label", "UNLUANOVA"),
		LOCTEXT("UNLUANOVA_ToolTip", "UNLUANOVA"),
		FSlateIcon("UNLUANOVAEditorStyle", "UNLUANOVAEditor.UNLUANOVALogo")
		));
#else
    TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);
    Extender->AddToolBarExtension("Settings", EExtensionHook::After, CommandList,
                                  FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& Builder)
                                  {
                                      Builder.BeginSection(NAME_None);
                                      Builder.AddComboButton(FUIAction(),
                                                             FOnGetContent::CreateRaw(this, &FMainMenuToolbar::GenerateUnLuaSettingsMenu),
                                                             LOCTEXT("UNLUANOVA_Label", "UNLUANOVA"),
                                                             LOCTEXT("UNLUANOVA_ToolTip", "UNLUANOVA"),
                                                             FSlateIcon("UNLUANOVAEditorStyle", "UNLUANOVAEditor.UNLUANOVALogo")
                                      );
                                      Builder.EndSection();
                                  })
    );
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(Extender);
#endif
    const auto& Settings = *GetDefault<UUNLUANOVAEditorSettings>();
    if (Settings.UpdateMode == EUpdateMode::Start)
        UUNLUANOVAEditorFunctionLibrary::FetchNewVersion();
}

TSharedRef<SWidget> FMainMenuToolbar::GenerateUnLuaSettingsMenu()
{
    const FUNLUANOVAEditorCommands& Commands = FUNLUANOVAEditorCommands::Get();
    FMenuBuilder MenuBuilder(true, CommandList);

    MenuBuilder.BeginSection(NAME_None, LOCTEXT("Section_Action", "Action"));
    MenuBuilder.AddMenuEntry(Commands.HotReload, NAME_None, LOCTEXT("HotReload", "Hot Reload"));
    MenuBuilder.AddMenuEntry(Commands.GenerateIntelliSense, NAME_None, LOCTEXT("GenerateIntelliSense", "Generate IntelliSense"));
    MenuBuilder.EndSection();

    MenuBuilder.BeginSection(NAME_None, LOCTEXT("Section_Help", "Help"));
    MenuBuilder.AddSubMenu(LOCTEXT("Section_SettingsMenu", "Settings"),
                           LOCTEXT("Section_SettingsMenu_ToolTip", "UNLUANOVA Settings"),
                           FNewMenuDelegate::CreateLambda([Commands](FMenuBuilder& SubMenuBuilder)
                           {
                               SubMenuBuilder.AddMenuEntry(Commands.OpenRuntimeSettings, NAME_None, LOCTEXT("OpenRuntimeSettings", "Runtime"));
                               SubMenuBuilder.AddMenuEntry(Commands.OpenEditorSettings, NAME_None, LOCTEXT("OpenEditorSettings", "Editor"));
                           }));
    MenuBuilder.AddMenuEntry(Commands.ReportIssue, NAME_None, LOCTEXT("ReportIssue", "Report Issue"));
    MenuBuilder.AddMenuEntry(Commands.About, NAME_None, LOCTEXT("About", "About"));
    MenuBuilder.EndSection();

    return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE

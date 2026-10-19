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

#include "MainMenuToolbar.h"
#include "ISettingsModule.h"
#include "UnLuaNovaAboutScreen.h"
#include "UnLuaNovaIntelliSenseGenerator.h"
#include "LevelEditor.h"
#include "UnLuaNovaEditorCommands.h"
#include "UnLuaNovaEditorFunctionLibrary.h"
#include "UnLuaNovaEditorSettings.h"
#include "UnLuaNovaFunctionLibrary.h"
#include "Interfaces/IMainFrameModule.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "UnLuaNovaMainMenuToolbar"

FMainMenuToolbar::FMainMenuToolbar()
    : CommandList(new FUICommandList)
{
    CommandList->MapAction(FUnLuaNovaEditorCommands::Get().HotReload, FExecuteAction::CreateStatic(UUnLuaNovaFunctionLibrary::HotReload), FCanExecuteAction());

    CommandList->MapAction(FUnLuaNovaEditorCommands::Get().GenerateIntelliSense, FExecuteAction::CreateLambda([]
    {
        FUnLuaNovaIntelliSenseGenerator::Get()->UpdateAll();
    }), FCanExecuteAction());

    CommandList->MapAction(FUnLuaNovaEditorCommands::Get().OpenRuntimeSettings, FExecuteAction::CreateLambda([]
    {
        if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
            SettingsModule->ShowViewer("Project", "Plugins", "UnLuaNova");
    }), FCanExecuteAction());

    CommandList->MapAction(FUnLuaNovaEditorCommands::Get().OpenEditorSettings, FExecuteAction::CreateLambda([]
    {
        if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
            SettingsModule->ShowViewer("Project", "Plugins", "UnLuaNova Editor");
    }), FCanExecuteAction());

    CommandList->MapAction(FUnLuaNovaEditorCommands::Get().ReportIssue, FExecuteAction::CreateLambda([]
    {
        const TCHAR* URL = TEXT("cmd");
        const TCHAR* Params = TEXT("/k start https://github.com/Tencent/UnLuaNova/issues/new/choose");
        FPlatformProcess::ExecProcess(URL, Params, nullptr, nullptr, nullptr);
    }), FCanExecuteAction());

    CommandList->MapAction(FUnLuaNovaEditorCommands::Get().About, FExecuteAction::CreateLambda([]
    {
        const FText AboutWindowTitle = LOCTEXT("AboutUnLua", "About UnLuaNova");

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
    FToolMenuSection& Section = ToolbarMenu->AddSection("UnLuaNovaSettings");
    Section.AddEntry(FToolMenuEntry::InitComboButton(
		"UnLuaNovaSettings",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FMainMenuToolbar::GenerateUnLuaSettingsMenu),
		LOCTEXT("UNLUANOVA_Label", "UnLuaNova"),
		LOCTEXT("UNLUANOVA_ToolTip", "UnLuaNova"),
		FSlateIcon("UnLuaNovaEditorStyle", "UnLuaNovaEditor.UnLuaNovaLogo")
		));
#else
    TSharedPtr<FExtender> Extender = MakeShareable(new FExtender);
    Extender->AddToolBarExtension("Settings", EExtensionHook::After, CommandList,
                                  FToolBarExtensionDelegate::CreateLambda([this](FToolBarBuilder& Builder)
                                  {
                                      Builder.BeginSection(NAME_None);
                                      Builder.AddComboButton(FUIAction(),
                                                             FOnGetContent::CreateRaw(this, &FMainMenuToolbar::GenerateUnLuaSettingsMenu),
                                                             LOCTEXT("UNLUANOVA_Label", "UnLuaNova"),
                                                             LOCTEXT("UNLUANOVA_ToolTip", "UnLuaNova"),
                                                             FSlateIcon("UnLuaNovaEditorStyle", "UnLuaNovaEditor.UnLuaNovaLogo")
                                      );
                                      Builder.EndSection();
                                  })
    );
    FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
    LevelEditorModule.GetToolBarExtensibilityManager()->AddExtender(Extender);
#endif
    const auto& Settings = *GetDefault<UUnLuaNovaEditorSettings>();
    if (Settings.UpdateMode == EUpdateMode::Start)
        UUnLuaNovaEditorFunctionLibrary::FetchNewVersion();
}

TSharedRef<SWidget> FMainMenuToolbar::GenerateUnLuaSettingsMenu()
{
    const FUnLuaNovaEditorCommands& Commands = FUnLuaNovaEditorCommands::Get();
    FMenuBuilder MenuBuilder(true, CommandList);

    MenuBuilder.BeginSection(NAME_None, LOCTEXT("Section_Action", "Action"));
    MenuBuilder.AddMenuEntry(Commands.HotReload, NAME_None, LOCTEXT("HotReload", "Hot Reload"));
    MenuBuilder.AddMenuEntry(Commands.GenerateIntelliSense, NAME_None, LOCTEXT("GenerateIntelliSense", "Generate IntelliSense"));
    MenuBuilder.EndSection();

    MenuBuilder.BeginSection(NAME_None, LOCTEXT("Section_Help", "Help"));
    MenuBuilder.AddSubMenu(LOCTEXT("Section_SettingsMenu", "Settings"),
                           LOCTEXT("Section_SettingsMenu_ToolTip", "UnLuaNova Settings"),
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

// Tencent is pleased to support the open source community by making UnLuauNova available.
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

#include "UnLuauNovaEditorStyle.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BOX_BRUSH(RelativePath, ...) FSlateBoxBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define BORDER_BRUSH(RelativePath, ...) FSlateBorderBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)
#define TTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".ttf")), __VA_ARGS__)
#define OTF_FONT(RelativePath, ...) FSlateFontInfo(RootToContentDir(RelativePath, TEXT(".otf")), __VA_ARGS__)

TSharedPtr<ISlateStyle> FUnLuauNovaEditorStyle::Instance = nullptr;

FUnLuauNovaEditorStyle::FUnLuauNovaEditorStyle()
    : FSlateStyleSet("UnLuauNovaEditorStyle")
{
    const FVector2D Icon40x40(40.0f, 40.0f);

    SetContentRoot(IPluginManager::Get().FindPlugin("UnLuauNova")->GetBaseDir() / TEXT("Resources"));

    Set("UnLuauNovaEditor.UnLuauNovaLogo", new IMAGE_BRUSH("Icons/icon_unlua_logo_40x", Icon40x40));
    Set("UnLuauNovaEditor.Status_NotBound", new IMAGE_BRUSH("Icons/icon_unlua_logo_40x", Icon40x40));
    Set("UnLuauNovaEditor.Status_Unknown", new IMAGE_BRUSH("Icons/icon_status_unknown_40x", Icon40x40));
    Set("UnLuauNovaEditor.Status_Bound", new IMAGE_BRUSH("Icons/icon_status_bound_40x", Icon40x40));
    Set("UnLuauNovaEditor.Status_BoundButInvalid", new IMAGE_BRUSH("Icons/icon_status_bound_but_invalid_40x", Icon40x40));
    Set("UnLuauNovaEditor.RevealInExplorer", new IMAGE_BRUSH("Icons/icon_reveal_in_explorer_40x", Icon40x40));
    Set("UnLuauNovaEditor.CreateLuaTemplate", new IMAGE_BRUSH("Icons/icon_create_template_40x", Icon40x40));
    Set("UnLuauNovaEditor.CopyAsRelativePath", new IMAGE_BRUSH("Icons/icon_copy_40x", Icon40x40));
    Set("UnLuauNovaEditor.BindToLua", new IMAGE_BRUSH("Icons/icon_bind_to_lua_40x", Icon40x40));
    Set("UnLuauNovaEditor.UnbindFromLua", new IMAGE_BRUSH("Icons/icon_unbind_from_lua_40x", Icon40x40));
    Set("UnLuauNovaEditor.HotReload", new IMAGE_BRUSH("Icons/icon_reload_40x", Icon40x40));
    Set("UnLuauNovaEditor.GenerateIntelliSense", new IMAGE_BRUSH("Icons/icon_intellisense_40x", Icon40x40));

    FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FUnLuauNovaEditorStyle::~FUnLuauNovaEditorStyle()
{
    FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
#undef TTF_FONT
#undef OTF_FONT

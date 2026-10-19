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

#include "UnLuauNova.h"
#include "UnLuauNovaFunctionLibrary.h"
#include "UnLuauNovaDelegates.h"
#include "UnLuauNovaModule.h"

FString UUnLuauNovaFunctionLibrary::GetScriptRootPath()
{
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + TEXT("Script/"));
}

int64 UUnLuauNovaFunctionLibrary::GetFileLastModifiedTimestamp(FString Path)
{
    const FDateTime FileTime = IFileManager::Get().GetTimeStamp(*Path);
    return FileTime.GetTicks();
}

void UUnLuauNovaFunctionLibrary::HotReload()
{
    IUnLuauNovaModule::Get().HotReload();
}

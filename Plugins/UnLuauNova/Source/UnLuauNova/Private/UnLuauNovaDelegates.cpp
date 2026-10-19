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

#include "UnLuauNovaDelegates.h"

FUnLuauNovaDelegates::FOnLuaStateCreated FUnLuauNovaDelegates::OnLuaStateCreated;
FUnLuauNovaDelegates::FOnLuaContextInitialized FUnLuauNovaDelegates::OnLuaContextInitialized;
FUnLuauNovaDelegates::FOnLuaContextCleanup FUnLuauNovaDelegates::OnPreLuaContextCleanup;
FUnLuauNovaDelegates::FOnLuaContextCleanup FUnLuauNovaDelegates::OnPostLuaContextCleanup;
FUnLuauNovaDelegates::FOnPreStaticallyExport FUnLuauNovaDelegates::OnPreStaticallyExport;
FUnLuauNovaDelegates::FOnObjectBinded FUnLuauNovaDelegates::OnObjectBinded;
FUnLuauNovaDelegates::FOnObjectUnbinded FUnLuauNovaDelegates::OnObjectUnbinded;
FUnLuauNovaDelegates::FGenericLuaDelegate FUnLuauNovaDelegates::HotfixLua;
FUnLuauNovaDelegates::FGenericLuaDelegate FUnLuauNovaDelegates::ReportLuaCallError;
FUnLuauNovaDelegates::FGenericLuaDelegate FUnLuauNovaDelegates::ConfigureLuaGC;
FUnLuauNovaDelegates::FCustomLuaFileLoader FUnLuauNovaDelegates::CustomLoadLuaFile;

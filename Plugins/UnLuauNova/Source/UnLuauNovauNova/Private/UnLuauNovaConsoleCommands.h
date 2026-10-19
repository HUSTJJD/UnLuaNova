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

#pragma once

#include "UnLuauNovaModule.h"

namespace UnLuauNova
{
    class FUnLuauNovaConsoleCommands
    {
    public:
        FAutoConsoleCommand DoCommand;

        FAutoConsoleCommand DoFileCommand;

        FAutoConsoleCommand CollectGarbageCommand;

        explicit FUnLuauNovaConsoleCommands(IUnLuauNovaModule* InModule);

        void Do(const TArray<FString>& Args) const;

        void DoFile(const TArray<FString>& Args) const;

        void CollectGarbage(const TArray<FString>& Args) const;

    private:
        IUnLuauNovaModule* Module;
    };
}

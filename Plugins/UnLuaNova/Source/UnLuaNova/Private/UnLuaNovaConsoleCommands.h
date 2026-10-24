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

#pragma once

#include "UnLuaNovaModule.h"

namespace UnLuaNova
{
    class FUnLuaNovaConsoleCommands
    {
    public:
        FAutoConsoleCommand DoCommand;

        FAutoConsoleCommand DoFileCommand;

        FAutoConsoleCommand CollectGarbageCommand;

        FAutoConsoleCommand MemStatsCommand;

        FAutoConsoleCommand ProfilerCommand;

        FAutoConsoleCommand DebugCommand;

        FAutoConsoleCommand CoroutinesCommand;

        FAutoConsoleCommand CodeGenCommand;

        explicit FUnLuaNovaConsoleCommands(IUnLuaNovaModule* InModule);

        void Do(const TArray<FString>& Args) const;

        void DoFile(const TArray<FString>& Args) const;

        void CollectGarbage(const TArray<FString>& Args) const;

        void MemStats(const TArray<FString>& Args) const;

        void Profiler(const TArray<FString>& Args) const;

        void Debug(const TArray<FString>& Args) const;

        void Coroutines(const TArray<FString>& Args) const;

        void CodeGen(const TArray<FString>& Args) const;

    private:
        IUnLuaNovaModule* Module;
    };
}

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

#include "Perfs/UnLuaNovaBenchmarkFunctionLibrary.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"

double UUnLuaNovaBenchmarkFunctionLibrary::StartTime;
FString UUnLuaNovaBenchmarkFunctionLibrary::StartTitle;
float UUnLuaNovaBenchmarkFunctionLibrary::BenchmarkMultiplier;
FString UUnLuaNovaBenchmarkFunctionLibrary::BenchmarkTitle;
TArray<FString> UUnLuaNovaBenchmarkFunctionLibrary::Messages;

void UUnLuaNovaBenchmarkFunctionLibrary::Start(const FString& Title, const int32 N)
{
    BenchmarkTitle = Title;
    BenchmarkMultiplier = 1000000000.0f / N;
    Messages.Reset();
    FBlueprintCoreDelegates::SetScriptMaximumLoopIterations(0x7FFFFFFF);
}

void UUnLuaNovaBenchmarkFunctionLibrary::StartTimer(const FString& Title)
{
    StartTime = FPlatformTime::Seconds();
    StartTitle = Title;
}

void UUnLuaNovaBenchmarkFunctionLibrary::StopTimer()
{
    const auto Cost = (FPlatformTime::Seconds() - StartTime) * BenchmarkMultiplier;
    const auto Message = FString::Printf(TEXT("%s ; %f"), *StartTitle, Cost);
    Messages.Add(Message);
}

void UUnLuaNovaBenchmarkFunctionLibrary::Stop()
{
    const auto Message = FString::Join(Messages, TEXT("\n"));
    const auto FilePath = FString::Printf(TEXT("%sBenchmark/%s-Benchmark-%s.csv"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()), *BenchmarkTitle, *FDateTime::Now().ToString());
    FFileHelper::SaveStringToFile(Message, *FilePath);
}

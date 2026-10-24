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

#include "UnLuaNovaTimerSubsystem.h"

#include "LuaBackend.h"
#include "LuaEnv.h"
#include "UnLuaNovaBase.h"

#include "Engine/World.h"

DECLARE_CYCLE_STAT(TEXT("UnLuaNova Timer"), STAT_UnLuaNovaTimer, STATGROUP_Tickables);

namespace
{
    TMap<int32, TWeakObjectPtr<UUnLuaNovaTimerSubsystem>> TimerOwners;
    int64 NextTimerHandle = 1;
}

void UUnLuaNovaTimerSubsystem::Shutdown()
{
    bShuttingDown = true;
    RemoveAll();
    UnLuaNova::FLuaEnv::OnDestroyed.Remove(EnvDestroyedHandle);
    EnvDestroyedHandle.Reset();
}

void UUnLuaNovaTimerSubsystem::Deinitialize()
{
    Shutdown();
    Super::Deinitialize();
}

void UUnLuaNovaTimerSubsystem::BeginDestroy()
{
    Shutdown();
    Super::BeginDestroy();
}

void UUnLuaNovaTimerSubsystem::OnEnvDestroyed(UnLuaNova::FLuaEnv& Env)
{
    RemoveForState(Env.GetMainState());
}

UUnLuaNovaTimerSubsystem* UUnLuaNovaTimerSubsystem::Find(lua_State* State, int32 Handle)
{
    auto* Env = UnLuaNova::FLuaEnv::FindEnv(State);
    const auto* Owner = TimerOwners.Find(Handle);
    auto* Subsystem = Owner ? Owner->Get() : nullptr;
    const int32 Index = Subsystem ? Subsystem->IndexOf(Handle) : INDEX_NONE;
    return Env && Index != INDEX_NONE && Subsystem->Timers[Index].State == Env->GetMainState()
        ? Subsystem : nullptr;
}

void UUnLuaNovaTimerSubsystem::RemoveForState(lua_State* State)
{
    for (int32 Index = Timers.Num() - 1; Index >= 0; --Index)
        if (Timers[Index].State == State)
            Remove(Timers[Index].Handle);
}

int32 UUnLuaNovaTimerSubsystem::NumForState(lua_State* State) const
{
    int32 Count = 0;
    for (const auto& Entry : Timers)
        Count += Entry.State == State ? 1 : 0;
    return Count;
}

TStatId UUnLuaNovaTimerSubsystem::GetStatId() const
{
    return GET_STATID(STAT_UnLuaNovaTimer);
}

double UUnLuaNovaTimerSubsystem::Now(bool bUnscaled) const
{
    const UWorld* World = GetWorld();
    if (!World)
        return 0.0;

    // TimeSeconds 受暂停与时间膨胀影响；RealTimeSeconds 不受。计时器存的是这两个之一，
    // 而不是 DeltaTime 累加出来的数 —— 后者在暂停 / 时间膨胀 / PIE 重启下全是错的。
    return bUnscaled ? World->GetRealTimeSeconds() : World->GetTimeSeconds();
}

int32 UUnLuaNovaTimerSubsystem::IndexOf(int32 Handle) const
{
    for (int32 i = 0; i < Timers.Num(); ++i)
    {
        if (Timers[i].Handle == Handle)
            return i;
    }
    return INDEX_NONE;
}

bool UUnLuaNovaTimerSubsystem::IsValid(int32 Handle) const
{
    return IndexOf(Handle) != INDEX_NONE;
}

int32 UUnLuaNovaTimerSubsystem::Add(lua_State* InState, int32 CallbackRef, float Interval, bool bLoop, float Delay, bool bUnscaled)
{
    UWorld* World = GetWorld();
    auto* Env = UnLuaNova::FLuaEnv::FindEnv(InState);
    if (!World || !Env || bShuttingDown || CallbackRef < 0 || NextTimerHandle > MAX_int32
        || !FMath::IsFinite(Interval) || !FMath::IsFinite(Delay) || Interval < 0 || Delay < 0)
        return INDEX_NONE;

    if (!EnvDestroyedHandle.IsValid())
        EnvDestroyedHandle = UnLuaNova::FLuaEnv::OnDestroyed.AddUObject(this, &UUnLuaNovaTimerSubsystem::OnEnvDestroyed);

    FEntry Entry;
    Entry.Handle = static_cast<int32>(NextTimerHandle++);
    Entry.CallbackRef = CallbackRef;
    // Registry refs are shared by coroutines. Callbacks run on the owning VM's main
    // state, so collecting the caller coroutine cannot leave a dangling state here.
    Entry.State = Env->GetMainState();
    Entry.Interval = FMath::Max(0.0, static_cast<double>(Interval));
    Entry.bLoop = bLoop;
    Entry.bUnscaled = bUnscaled;
    Entry.Next = Now(bUnscaled) + FMath::Max(0.0, static_cast<double>(Delay));

    Timers.Add(Entry);
    TimerOwners.Add(Entry.Handle, this);
    return Entry.Handle;
}

void UUnLuaNovaTimerSubsystem::Release(const FEntry& Entry) const
{
    if (UnLuaNova::FLuaEnv::GetAll().Contains(Entry.State) && Entry.CallbackRef >= 0)
        luaL_unref(Entry.State, LUA_REGISTRYINDEX, Entry.CallbackRef);
}

bool UUnLuaNovaTimerSubsystem::Remove(int32 Handle)
{
    const int32 Index = IndexOf(Handle);
    if (Index == INDEX_NONE)
        return false;

    Release(Timers[Index]);
    TimerOwners.Remove(Handle);
    Timers.RemoveAtSwap(Index);
    return true;
}

bool UUnLuaNovaTimerSubsystem::SetPaused(int32 Handle, bool bPaused)
{
    const int32 Index = IndexOf(Handle);
    if (Index == INDEX_NONE)
        return false;

    FEntry& Entry = Timers[Index];
    if (Entry.bPaused != bPaused)
    {
        const double Current = Now(Entry.bUnscaled);
        if (bPaused)
            Entry.Remaining = FMath::Max(0.0, Entry.Next - Current);
        else
            Entry.Next = Current + Entry.Remaining;
        Entry.bPaused = bPaused;
    }

    return true;
}

void UUnLuaNovaTimerSubsystem::RemoveAll()
{
    for (const FEntry& Entry : Timers)
    {
        Release(Entry);
        TimerOwners.Remove(Entry.Handle);
    }
    Timers.Empty();
}

void UUnLuaNovaTimerSubsystem::Tick(float DeltaTime)
{
    if (Timers.Num() == 0 || bTicking || bShuttingDown)
        return;

    UWorld* World = GetWorld();
    if (!World)
        return;

    TGuardValue<bool> TickGuard(bTicking, true);

    const double Scaled = World->GetTimeSeconds();
    const double Unscaled = World->GetRealTimeSeconds();

    // ① 先抄一份到期项：回调里 Add / Remove 会动 Timers，边遍历边改会错位
    TArray<int32> Due;
    for (FEntry& Entry : Timers)
    {
        if (Entry.bPaused)
            continue;

        const double Current = Entry.bUnscaled ? Unscaled : Scaled;
        if (Current < Entry.Next)
            continue;

        // ② 循环定时器按 Interval 追平，掉帧时不会越落越多；
        //    追平后若仍落后于当下（一帧掉了很多个周期），直接夹到下一个周期，不补发历史
        if (Entry.bLoop)
        {
            if (Entry.Interval > 0.0)
            {
                Entry.Next += Entry.Interval;
                if (Entry.Next <= Current)
                    Entry.Next = Current + Entry.Interval;
            }
            else
            {
                Entry.Next = Current;
            }
        }

        Due.Add(Entry.Handle);
    }

    // ③ 执行。一次性定时器在这里摘掉（引用也一起释放）
    for (const int32 Handle : Due)
    {
        const int32 Index = IndexOf(Handle);
        if (Index == INDEX_NONE || Timers[Index].bPaused)
            continue;

        const FEntry Item = Timers[Index];

        if (Item.State && Item.CallbackRef >= 0)
        {
            lua_rawgeti(Item.State, LUA_REGISTRYINDEX, Item.CallbackRef);
            // The stack keeps the function alive after unref. One-shots are absent
            // before invoking Lua, including when callbacks reenter the subsystem.
            if (!Item.bLoop)
                Remove(Item.Handle);
            // 不用 LUA_OK：两个后端对它的定义不完全一致，只认"0 是成功"
            if (lua_pcall(Item.State, 0, 0, 0) != 0)
            {
                const char* Message = lua_tostring(Item.State, -1);
                UE_LOG(LogUnLuaNova, Error, TEXT("UnLuaNova.Timer 回调出错：%s"),
                    Message ? UTF8_TO_TCHAR(Message) : TEXT("(no message)"));
                lua_pop(Item.State, 1);
            }
        }

    }
}

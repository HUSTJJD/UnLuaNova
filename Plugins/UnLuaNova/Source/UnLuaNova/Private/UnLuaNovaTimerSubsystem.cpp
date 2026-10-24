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
#include "UnLuaNovaBase.h"

#include "Engine/World.h"

DECLARE_CYCLE_STAT(TEXT("UnLuaNova Timer"), STAT_UnLuaNovaTimer, STATGROUP_Tickables);

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
    if (!World || !InState || CallbackRef < 0)
        return INDEX_NONE;

    FEntry Entry;
    Entry.Handle = NextHandle++;
    Entry.CallbackRef = CallbackRef;
    Entry.State = InState;
    Entry.Interval = FMath::Max(0.0, static_cast<double>(Interval));
    Entry.bLoop = bLoop;
    Entry.bUnscaled = bUnscaled;
    Entry.Next = Now(bUnscaled) + FMath::Max(0.0, static_cast<double>(Delay));

    Timers.Add(Entry);
    return Entry.Handle;
}

void UUnLuaNovaTimerSubsystem::Release(const FEntry& Entry) const
{
    if (Entry.State && Entry.CallbackRef >= 0)
        luaL_unref(Entry.State, LUA_REGISTRYINDEX, Entry.CallbackRef);
}

bool UUnLuaNovaTimerSubsystem::Remove(int32 Handle)
{
    const int32 Index = IndexOf(Handle);
    if (Index == INDEX_NONE)
        return false;

    Release(Timers[Index]);
    Timers.RemoveAtSwap(Index);
    return true;
}

bool UUnLuaNovaTimerSubsystem::SetPaused(int32 Handle, bool bPaused)
{
    const int32 Index = IndexOf(Handle);
    if (Index == INDEX_NONE)
        return false;

    FEntry& Entry = Timers[Index];
    const bool bChanged = Entry.bPaused != bPaused;
    Entry.bPaused = bPaused;

    // 恢复时把到期时间往後推同样的量，否则"暂停了 10 秒"会在恢复瞬间补一次
    if (bChanged && !bPaused)
        Entry.Next = Now(Entry.bUnscaled) + Entry.Interval;

    return true;
}

void UUnLuaNovaTimerSubsystem::RemoveAll()
{
    for (const FEntry& Entry : Timers)
        Release(Entry);
    Timers.Empty();
}

void UUnLuaNovaTimerSubsystem::Tick(float DeltaTime)
{
    if (Timers.Num() == 0)
        return;

    UWorld* World = GetWorld();
    if (!World)
        return;

    const double Scaled = World->GetTimeSeconds();
    const double Unscaled = World->GetRealTimeSeconds();

    // ① 先抄一份到期项：回调里 Add / Remove 会动 Timers，边遍历边改会错位
    TArray<FDueEntry> Due;
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

        Due.Add({Entry.Handle, Entry.CallbackRef, Entry.State, Entry.bLoop});
    }

    // ③ 执行。一次性定时器在这里摘掉（引用也一起释放）
    for (const FDueEntry& Item : Due)
    {
        // 回调期间可能已经被别的回调移除了
        if (!IsValid(Item.Handle))
            continue;

        if (Item.State && Item.CallbackRef >= 0)
        {
            lua_rawgeti(Item.State, LUA_REGISTRYINDEX, Item.CallbackRef);
            // 不用 LUA_OK：两个后端对它的定义不完全一致，只认"0 是成功"
            if (lua_pcall(Item.State, 0, 0, 0) != 0)
            {
                const char* Message = lua_tostring(Item.State, -1);
                UE_LOG(LogUnLuaNova, Error, TEXT("UnLuaNova.Timer 回调出错：%s"),
                    Message ? UTF8_TO_TCHAR(Message) : TEXT("(no message)"));
                lua_pop(Item.State, 1);
            }
        }

        if (!Item.bLoop)
            Remove(Item.Handle);
    }
}

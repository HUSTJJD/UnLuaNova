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

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h" // UTickableWorldSubsystem lives here (moved out of its own header)
#include "UnLuaNovaTimerSubsystem.generated.h"

struct lua_State;
namespace UnLuaNova { class FLuaEnv; }

/**
 * 脚本计时器：把"多久之后回调一次"这件事交给引擎时间，而不是 Lua 自己数。
 *
 * 为什么值得做成 C++ 而不是 Lua 表：不是性能，是**正确性**。脚本侧手写计时器几乎必然踩这四件事：
 *
 *   ① 时间源用 `os.time()` —— 秒级精度，且完全无视暂停与时间膨胀
 *   ② 用 `DeltaTime` 累加 —— 时间膨胀、暂停、PIE 重启全都不对
 *   ③ 用 `==` 判间隔 —— 中间跳一帧就永远错过
 *   ④ 回调里增删定时器 —— 遍历中改表，索引错位
 *
 * 这里的实现：到期时间存在**世界时间**上（`GetTimeSeconds()` 受暂停与时间膨胀影响，
 * `GetRealTimeSeconds()` 不受），每帧只比较不累加；到期项先快照再回调，避免 ④；
 * 循环回调每帧至多执行一次，不补发错过的周期。回调在所属 Env 主状态执行，不允许 yield。
 *
 * 平铺定时任务的性能收益是顺带的：到期判断在 C++，Lua 侧不用每帧扫表。
 */
UCLASS()
class UNLUANOVA_API UUnLuaNovaTimerSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    // ── FTickableGameObject / UTickableWorldSubsystem ──
    virtual void Tick(float DeltaTime) override;
    virtual bool IsTickable() const override { return !IsTemplate() && Timers.Num() > 0; }
    virtual TStatId GetStatId() const override;
    virtual void Deinitialize() override;
    virtual void BeginDestroy() override;
    virtual bool IsTickableWhenPaused() const override { return true; }

    /**
     * 注册一个回调。回调是 Lua 函数，调用方已经 `luaL_ref` 进注册表，所有权随之转给本子系统
     * （仅成功时转移所有权；失败时调用方仍需 unref）。
     *
     * @return 句柄；INDEX_NONE 表示失败（无世界 / 无 VM）
     */
    int32 Add(lua_State* InState, int32 CallbackRef, float Interval, bool bLoop, float Delay, bool bUnscaled);

    /** 移除并释放回调引用。返回是否真的移到了。 */
    bool Remove(int32 Handle);

    bool SetPaused(int32 Handle, bool bPaused);
    bool IsValid(int32 Handle) const;

    /** Resolve an opaque handle only for the environment that created it. */
    static UUnLuaNovaTimerSubsystem* Find(lua_State* State, int32 Handle);
    void RemoveForState(lua_State* State);
    int32 NumForState(lua_State* State) const;

    /** 清掉全部定时器（地图切换 / 环境销毁时用）。 */
    void RemoveAll();

    int32 Num() const { return Timers.Num(); }

    /** 当前世界时间：bUnscaled 为真取真实时间（不受暂停与时间膨胀影响）。 */
    double Now(bool bUnscaled) const;

private:
    struct FEntry
    {
        int32 Handle = INDEX_NONE;
        int32 CallbackRef = INDEX_NONE;
        /** 回调所属的 VM。多环境（LuaEnvLocator）下不能只存一个全局 L。 */
        lua_State* State = nullptr;
        double Interval = 0.0;
        /** 到期时间，存的是世界时间（不是累加出来的）。 */
        double Next = 0.0;
        double Remaining = 0.0;
        bool bLoop = false;
        bool bUnscaled = false;
        bool bPaused = false;
    };

    int32 IndexOf(int32 Handle) const;
    void Release(const FEntry& Entry) const;
    void OnEnvDestroyed(UnLuaNova::FLuaEnv& Env);
    void Shutdown();

    TArray<FEntry> Timers;
    FDelegateHandle EnvDestroyedHandle;
    bool bTicking = false;
    bool bShuttingDown = false;
};

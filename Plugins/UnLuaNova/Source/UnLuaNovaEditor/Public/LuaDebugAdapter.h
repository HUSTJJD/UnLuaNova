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
#include "HAL/Runnable.h"

#include <atomic>

/** 适配线程持有的断点（DAP 的 source 先按原样存，推送时再裁剪）。 */
struct FLuaDapBreakpoint
{
    FString Source;
    int32 Line = 0;
};

/**
 * DAP（Debug Adapter Protocol）服务器：把 headless 的 FLuaDebugService 接到 IDE 上。
 *
 * FLuaDebugService 刻意不碰 socket、不定义线协议（见它的头注释），于是"能调试"和
 * "能在 VSCode 里调试"之间一直缺一层。这一层就是它：
 *
 *   VSCode  ──DAP/JSON──►  FLuaDebugAdapter（本类，自带线程）
 *                              │
 *                              └──►  FLuaDebugService（断点 / 单步 / 栈 / 局部变量）
 *
 * 用法：控制台 `lua.dap start [port]`（默认 5699），VSCode 用 attach 方式连上即可。
 *
 * 线程模型（这是实现里唯一需要小心的地方）：
 * - socket 读写与 JSON 解析全在**适配线程**，不占游戏线程；
 * - 断点变更走 AsyncTask(GameThread) 排队执行 —— 游戏线程在断点处是**阻塞自旋**的，
 *   这时候排进去的任务要等 Continue() 之后才跑，所以"暂停中改断点"会延迟生效；
 * - Continue() 是线程安全的（服务侧明确支持跨线程调用），所以 continue/next 直接在
 *   适配线程调用，否则永远解不开阻塞。
 */
// Exported: this header is part of the module's public API, which is what lets
// UnLuaNovaTestSuite drive a real DAP session against the server (see
// Private/Specs/DebugAdapter.spec.cpp) without reaching into Private/.
class UNLUANOVAEDITOR_API FLuaDebugAdapter : public FRunnable
{
public:
    static FLuaDebugAdapter& Get();

    /** 起服务；已在跑就只报错。0 selects an OS-assigned port. */
    bool Start(int32 InPort = DefaultPort);

    /**
     * 停服务（对外入口）。
     *
     * 不叫 Stop()：FRunnable::Stop() 是虚函数，由 FRunnableThread 在请求停止时调用，
     * 在里面 WaitForCompletion() 会把自己等死。分工是 Stop() 置标志、StopServer() 收线程。
     */
    void StopServer();

    bool IsRunning() const { return bRunning; }
    int32 GetPort() const;

    // FRunnable
    virtual uint32 Run() override;
    virtual void Stop() override { bStopping = true; }

    static constexpr int32 DefaultPort = 5699;

private:
    FLuaDebugAdapter() = default;

    // ── 连接 ──
    bool Listen();
    void AcceptClient();
    void CloseClient();

    // ── 线协议 ──
    bool ReadMessage(FString& OutJson);
    void SendJson(const TSharedRef<class FJsonObject>& Payload);
    void SendEvent(const FString& Event, const TSharedPtr<class FJsonObject>& Body);
    void SendError(const TSharedRef<class FJsonObject>& Request, const FString& Message);

    // ── 请求分发 ──
    void Dispatch(const TSharedRef<class FJsonObject>& Request);
    void OnInitialize(const TSharedRef<class FJsonObject>& Request);
    void OnAttach(const TSharedRef<class FJsonObject>& Request);
    void OnSetBreakpoints(const TSharedRef<class FJsonObject>& Request);
    void OnThreads(const TSharedRef<class FJsonObject>& Request);
    void OnStackTrace(const TSharedRef<class FJsonObject>& Request);
    void OnScopes(const TSharedRef<class FJsonObject>& Request);
    void OnVariables(const TSharedRef<class FJsonObject>& Request);
    void OnEvaluate(const TSharedRef<class FJsonObject>& Request);
    void OnPause(const TSharedRef<class FJsonObject>& Request);
    void OnContinue(const TSharedRef<class FJsonObject>& Request, bool bSingleStep);
    void OnDisconnect(const TSharedRef<class FJsonObject>& Request);
    void Unsupported(const TSharedRef<class FJsonObject>& Request, const FString& Why);

    /** 把当前断点集合推给游戏线程上的 FLuaDebugService（清空后重设，幂等）。 */
    void RequestBreakpointSync();

    // ── 暂停状态轮询：阻塞模式下服务不会主动通知，只能自己看 ──
    void PumpStoppedState();

private:
    class FSocket* ListenerSocket = nullptr;
    class FSocket* ClientSocket = nullptr;
    class FRunnableThread* Thread = nullptr;
    mutable FCriticalSection LifecycleLock;
    std::atomic<uint64> ServerGeneration{0};

    TArray<uint8> ReadBuffer;
    int32 Port = DefaultPort;
    std::atomic<bool> bRunning{false};
    std::atomic<bool> bStopping{false};

    int32 Seq = 0;
    bool bPausedReported = false;

    /** 适配线程持有的断点集合；推送到服务时整体覆盖。 */
    TArray<struct FLuaDapBreakpoint> PendingBreakpoints;
    FCriticalSection PendingLock;
};

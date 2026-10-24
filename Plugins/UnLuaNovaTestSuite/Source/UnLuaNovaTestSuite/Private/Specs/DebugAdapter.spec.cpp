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

// Socket traffic is polled by the game-thread ticker. Spec teardown removes the ticker
// before closing the client/server, including when the automation framework times out.
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "UnLuaNovaBase.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"
#include "LuaDebugAdapter.h"
#include "LuaDebugService.h"
#include "Async/Async.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Parse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
    constexpr double RequestTimeout = 5.0;
    constexpr double SessionTimeout = 20.0;

    TArray<uint8> FrameDap(const FString& Json)
    {
        const FTCHARToUTF8 Body(*Json);
        const FTCHARToUTF8 Header(*FString::Printf(TEXT("Content-Length: %d\r\n\r\n"), Body.Length()));
        TArray<uint8> Frame;
        Frame.Append(reinterpret_cast<const uint8*>(Header.Get()), Header.Length());
        Frame.Append(reinterpret_cast<const uint8*>(Body.Get()), Body.Length());
        return Frame;
    }

    class FDapFrames
    {
    public:
        void Append(const uint8* Data, int32 Size) { Pending.Append(Data, Size); }

        bool Pop(FString& Json)
        {
            int32 HeaderEnd = INDEX_NONE;
            for (int32 Index = 0; Index + 3 < Pending.Num(); ++Index)
            {
                if (Pending[Index] == '\r' && Pending[Index + 1] == '\n'
                    && Pending[Index + 2] == '\r' && Pending[Index + 3] == '\n')
                {
                    HeaderEnd = Index + 4;
                    break;
                }
            }
            if (HeaderEnd == INDEX_NONE) return false;

            const FUTF8ToTCHAR Header(reinterpret_cast<const ANSICHAR*>(Pending.GetData()), HeaderEnd);
            const FString HeaderText(Header.Length(), Header.Get());
            int32 Length = -1;
            if (!FParse::Value(*HeaderText, TEXT("Content-Length:"), Length)
                || Length < 0 || Length > 1024 * 1024 || Pending.Num() - HeaderEnd < Length)
                return false;

            const FUTF8ToTCHAR Body(reinterpret_cast<const ANSICHAR*>(Pending.GetData() + HeaderEnd), Length);
            Json = FString(Body.Length(), Body.Get());
            Pending.RemoveAt(0, HeaderEnd + Length, EAllowShrinking::No);
            return true;
        }

    private:
        TArray<uint8> Pending;
    };

    class FDapClient
    {
    public:
        explicit FDapClient(int32 InWriteChunkSize) : WriteChunkSize(InWriteChunkSize) {}

        ~FDapClient()
        {
            if (Socket)
            {
                Socket->Close();
                ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Socket);
            }
        }

        bool Connect(int32 Port)
        {
            auto* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
            Socket = Subsystem ? Subsystem->CreateSocket(NAME_Stream, TEXT("UnLuaNovaDapClient")) : nullptr;
            if (!Socket || !Socket->SetNonBlocking(true)) return false;

            const auto Address = Subsystem->CreateInternetAddr();
            Address->SetLoopbackAddress();
            Address->SetPort(Port);
            if (Socket->Connect(*Address)) return true;
            const ESocketErrors Error = Subsystem->GetLastErrorCode();
            return Error == SE_EWOULDBLOCK || Error == SE_EINPROGRESS;
        }

        bool IsConnected() const { return Socket && Socket->GetConnectionState() == SCS_Connected; }
        void Queue(const FString& Json) { Outgoing.Append(FrameDap(Json)); }
        bool Pop(FString& Json) { return Incoming.Pop(Json); }

        bool Pump()
        {
            if (!Socket) return false;
            if (Sent < Outgoing.Num())
            {
                int32 Count = 0;
                if (Socket->Send(Outgoing.GetData() + Sent, FMath::Min(WriteChunkSize, Outgoing.Num() - Sent), Count))
                    Sent += Count;
                else if (ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLastErrorCode() != SE_EWOULDBLOCK)
                    return false;
                if (Sent == Outgoing.Num())
                {
                    Outgoing.Reset();
                    Sent = 0;
                }
            }

            // Bound work per frame even if a faulty server keeps writing.
            for (int32 Chunk = 0; Chunk < 16; ++Chunk)
            {
                uint8 Buffer[4096];
                int32 Read = 0;
                if (!Socket->Recv(Buffer, sizeof(Buffer), Read) || Read <= 0) break;
                Incoming.Append(Buffer, Read);
            }
            return true;
        }

    private:
        FSocket* Socket = nullptr;
        FDapFrames Incoming;
        TArray<uint8> Outgoing;
        int32 Sent = 0;
        int32 WriteChunkSize;
    };

    class FDapSession
    {
    public:
        FDapSession(bool bInUnknownCommand, int32 WriteChunkSize) : Client(WriteChunkSize), bUnknownCommand(bInUnknownCommand) {}
        bool Connect(int32 Port) { return Client.Connect(Port); }

        bool Tick(FAutomationTestBase& Test)
        {
            const double Now = FPlatformTime::Seconds();
            if (Now - Started >= SessionTimeout || Now - StepStarted >= RequestTimeout)
                return Fail(Test, FString::Printf(TEXT("DAP timeout at step %d"), static_cast<int32>(Step)));

            if (Step == EStep::Connecting)
            {
                if (!Client.IsConnected()) return false;
                Send(bUnknownCommand ? EStep::Unknown : EStep::Initialize,
                    bUnknownCommand ? TEXT("notARealCommand") : TEXT("initialize"));
            }
            if (!Client.Pump()) return Fail(Test, TEXT("DAP socket send failed"));

            auto& Service = UnLuaNova::FLuaDebugService::Get();
            if (Step == EStep::Attached)
            {
                if (!Service.IsAttached()) return false;
                auto Args = MakeShared<FJsonObject>();
                Args->SetStringField(TEXT("expression"), TEXT("1 + 1 == 2 and '中文' == '中文'"));
                Args->SetStringField(TEXT("context"), TEXT("repl"));
                Send(EStep::Evaluate, TEXT("evaluate"), Args);
                return false;
            }
            if (Step == EStep::Detached) return !Service.IsAttached();

            FString Json;
            while (Client.Pop(Json))
            {
                TSharedPtr<FJsonObject> Reply;
                if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Reply) || !Reply.IsValid())
                    return Fail(Test, TEXT("Malformed DAP JSON response"));
                if (Reply->GetStringField(TEXT("type")) != TEXT("response")
                    || static_cast<int32>(Reply->GetNumberField(TEXT("request_seq"))) != Seq)
                    continue;

                if (Step == EStep::Unknown)
                {
                    Test.TestFalse(TEXT("Unknown command is rejected"), Reply->GetBoolField(TEXT("success")));
                    return true;
                }
                if (!Reply->GetBoolField(TEXT("success"))) return Fail(Test, TEXT("DAP request failed"));

                switch (Step)
                {
                case EStep::Initialize:
                {
                    const auto Body = Reply->GetObjectField(TEXT("body"));
                    if (!Body.IsValid() || !Body->GetBoolField(TEXT("supportsEvaluateRequest")))
                        return Fail(Test, TEXT("Missing evaluate capability"));
                    auto Args = MakeShared<FJsonObject>();
                    Args->SetNumberField(TEXT("env"), 0);
                    Send(EStep::Attach, TEXT("attach"), Args);
                    break;
                }
                case EStep::Attach:
                    Step = EStep::Attached;
                    StepStarted = Now;
                    break;
                case EStep::Evaluate:
                {
                    const auto Body = Reply->GetObjectField(TEXT("body"));
                    if (!Body.IsValid() || Body->GetStringField(TEXT("result")) != TEXT("true"))
                        return Fail(Test, TEXT("DAP evaluate returned the wrong value"));
                    Send(EStep::Threads, TEXT("threads"));
                    break;
                }
                case EStep::Threads:
                {
                    const auto Body = Reply->GetObjectField(TEXT("body"));
                    const TArray<TSharedPtr<FJsonValue>>* Threads = nullptr;
                    if (!Body.IsValid() || !Body->TryGetArrayField(TEXT("threads"), Threads) || Threads->Num() == 0)
                        return Fail(Test, TEXT("DAP returned no threads"));
                    Send(EStep::Disconnect, TEXT("disconnect"));
                    break;
                }
                case EStep::Disconnect:
                    Step = EStep::Detached;
                    StepStarted = Now;
                    break;
                default:
                    return Fail(Test, TEXT("Unexpected DAP response"));
                }
            }
            return false;
        }

    private:
        enum class EStep { Connecting, Initialize, Attach, Attached, Evaluate, Threads, Disconnect, Detached, Unknown };
        void Send(EStep Next, const FString& Command, TSharedPtr<FJsonObject> Args = nullptr)
        {
            auto Request = MakeShared<FJsonObject>();
            Request->SetNumberField(TEXT("seq"), ++Seq);
            Request->SetStringField(TEXT("type"), TEXT("request"));
            Request->SetStringField(TEXT("command"), Command);
            Request->SetObjectField(TEXT("arguments"), Args.IsValid() ? Args.ToSharedRef() : MakeShared<FJsonObject>());
            FString Json;
            FJsonSerializer::Serialize(Request, TJsonWriterFactory<>::Create(&Json));
            Client.Queue(Json);
            Step = Next;
            StepStarted = FPlatformTime::Seconds();
        }

        static bool Fail(FAutomationTestBase& Test, const FString& Message)
        {
            Test.AddError(Message);
            return true;
        }

        FDapClient Client;
        bool bUnknownCommand;
        EStep Step = EStep::Connecting;
        int32 Seq = 0;
        double Started = FPlatformTime::Seconds();
        double StepStarted = Started;
    };
}

BEGIN_DEFINE_SPEC(FDebugAdapterSpec, "UnLuaNova.API.DebugAdapter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
    FTSTicker::FDelegateHandle PollHandle;
    TSharedPtr<FDapSession> Session;
    bool bOwnsServer = false;
    void BeginSession(bool bUnknownCommand, const FDoneDelegate& Done, int32 WriteChunkSize = MAX_int32);
END_DEFINE_SPEC(FDebugAdapterSpec)

void FDebugAdapterSpec::BeginSession(bool bUnknownCommand, const FDoneDelegate& Done, int32 WriteChunkSize)
{
    auto& Adapter = FLuaDebugAdapter::Get();
    bOwnsServer = Adapter.Start(0);
    if (!bOwnsServer)
    {
        AddError(TEXT("Could not start the DAP test server"));
        Done.Execute();
        return;
    }
    TestTrue(TEXT("OS-assigned port is available"), Adapter.GetPort() > 0);
    Session = MakeShared<FDapSession>(bUnknownCommand, WriteChunkSize);
    if (!Session->Connect(Adapter.GetPort()))
    {
        AddError(TEXT("Could not connect the DAP test client"));
        Done.Execute();
        return;
    }
    PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([this, Done](float)
    {
        if (!Session->Tick(*this)) return true;
        Done.Execute();
        return false;
    }));
}

void FDebugAdapterSpec::Define()
{
    BeforeEach([this]
    {
        UnLuaNova::Startup();
        TestNotNull(TEXT("Lua environment exists"), UnLuaNova::GetState());
    });

    AfterEach([this]
    {
        FTSTicker::GetCoreTicker().RemoveTicker(PollHandle);
        PollHandle.Reset();
        Session.Reset();
        if (bOwnsServer)
        {
            FLuaDebugAdapter::Get().StopServer();
            UnLuaNova::FLuaDebugService::Get().Detach();
            bOwnsServer = false;
        }
    });

    LatentIt(TEXT("一轮真实会话：initialize / attach / evaluate / threads / disconnect"), [this](const FDoneDelegate& Done)
    {
        BeginSession(false, Done);
    });

    LatentIt(TEXT("未知命令回 error 而不是把连接挂死"), [this](const FDoneDelegate& Done)
    {
        BeginSession(true, Done);
    });

    LatentIt(TEXT("DAP session accepts fragmented UTF-8 requests"), [this](const FDoneDelegate& Done)
    {
        BeginSession(false, Done, 7);
    });

    It(TEXT("DAP framing preserves coalesced messages"), [this]
    {
        const FString First = TEXT("{\"type\":\"event\",\"event\":\"initialized\"}");
        const FString Second = TEXT("{\"type\":\"response\",\"request_seq\":2}");
        TArray<uint8> Bytes = FrameDap(First);
        Bytes.Append(FrameDap(Second));
        FDapFrames Frames;
        Frames.Append(Bytes.GetData(), Bytes.Num());
        FString Json;
        TestTrue(TEXT("First message"), Frames.Pop(Json));
        TestEqual(TEXT("First body"), Json, First);
        TestTrue(TEXT("Second message survives the first read"), Frames.Pop(Json));
        TestEqual(TEXT("Second body"), Json, Second);
        TestFalse(TEXT("No phantom third message"), Frames.Pop(Json));
    });

    It(TEXT("DAP framing preserves fragmented UTF-8 bytes"), [this]
    {
        const FString Expected = TEXT("{\"value\":\"中文\"}");
        const TArray<uint8> Bytes = FrameDap(Expected);
        FDapFrames Frames;
        FString Json;
        for (int32 Index = 0; Index < Bytes.Num(); ++Index)
        {
            Frames.Append(&Bytes[Index], 1);
            TestEqual(TEXT("Only a complete byte frame can be read"), Frames.Pop(Json), Index == Bytes.Num() - 1);
        }
        TestEqual(TEXT("UTF-8 byte length is not TCHAR length"), Json, Expected);
    });

    It(TEXT("Concurrent stop and restart use a fresh server"), [this]
    {
        auto& Adapter = FLuaDebugAdapter::Get();
        bOwnsServer = Adapter.Start(0);
        TestTrue(TEXT("Start"), bOwnsServer);
        if (!bOwnsServer) return;
        FEvent* Ready = FPlatformProcess::GetSynchEventFromPool(true);
        auto Stop = [&Adapter, Ready]
        {
            Ready->Wait();
            Adapter.StopServer();
        };
        auto FirstStop = Async(EAsyncExecution::Thread, Stop);
        auto SecondStop = Async(EAsyncExecution::Thread, Stop);
        Ready->Trigger();
        // Both callers finish before teardown can touch the singleton again.
        FirstStop.Wait();
        SecondStop.Wait();
        FPlatformProcess::ReturnSynchEventToPool(Ready);
        Adapter.StopServer();
        TestFalse(TEXT("Stopped"), Adapter.IsRunning());
        bOwnsServer = Adapter.Start(0);
        TestTrue(TEXT("Restart"), bOwnsServer);
    });
}

#endif

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

#include "LuaDebugAdapter.h"

#include "LuaDebugService.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaBase.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "LuaDebugAdapter"

FLuaDebugAdapter& FLuaDebugAdapter::Get()
{
    static FLuaDebugAdapter Instance;
    return Instance;
}

// ── 生命周期 ───────────────────────────────────────────────────────────────
bool FLuaDebugAdapter::Start(int32 InPort)
{
    if (bRunning)
    {
        UE_LOG(LogUnLuaNova, Warning, TEXT("lua.dap: already running on port %d"), Port);
        return false;
    }

    Port = InPort > 0 ? InPort : DefaultPort;
    if (!Listen())
        return false;

    bStopping = false;
    bRunning = true;
    Thread = FRunnableThread::Create(this, TEXT("LuaDapThread"), 0, TPri_BelowNormal);
    if (!Thread)
    {
        bRunning = false;
        return false;
    }

    UE_LOG(LogUnLuaNova, Display, TEXT("lua.dap: listening on 127.0.0.1:%d (DAP, attach from VSCode)"), Port);
    return true;
}

void FLuaDebugAdapter::StopServer()
{
    if (!bRunning)
        return;

    bStopping = true;
    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
    bRunning = false;

    UE_LOG(LogUnLuaNova, Display, TEXT("lua.dap: stopped"));
}

bool FLuaDebugAdapter::Listen()
{
    auto* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Subsystem)
    {
        UE_LOG(LogUnLuaNova, Error, TEXT("lua.dap: no socket subsystem"));
        return false;
    }

    ListenerSocket = Subsystem->CreateSocket(NAME_Stream, TEXT("LuaDapListener"), false);
    if (!ListenerSocket)
        return false;

    ListenerSocket->SetReuseAddr();
    ListenerSocket->SetNonBlocking(true);
    ListenerSocket->SetRecvErr();

    const TSharedRef<FInternetAddr> Addr = Subsystem->CreateInternetAddr();
    Addr->SetAnyAddress();
    Addr->SetPort(Port);

    if (!ListenerSocket->Bind(*Addr))
    {
        UE_LOG(LogUnLuaNova, Error, TEXT("lua.dap: bind failed on port %d (in use?)"), Port);
        Subsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }

    if (!ListenerSocket->Listen(1))
    {
        UE_LOG(LogUnLuaNova, Error, TEXT("lua.dap: listen failed"));
        Subsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }

    return true;
}

void FLuaDebugAdapter::CloseClient()
{
    if (!ClientSocket)
        return;

    ClientSocket->Close();
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ClientSocket);
    ClientSocket = nullptr;
    ReadBuffer.Empty();
    bPausedReported = false;
}

void FLuaDebugAdapter::AcceptClient()
{
    if (ClientSocket)
        return;

    bool bPending = false;
    ListenerSocket->HasPendingConnection(bPending);
    if (!bPending)
        return;

    FSocket* Accepted = ListenerSocket->Accept(TEXT("LuaDapClient"));
    if (!Accepted)
        return;

    Accepted->SetNonBlocking(true);
    ClientSocket = Accepted;
    UE_LOG(LogUnLuaNova, Display, TEXT("lua.dap: client attached"));
}

// ── 线协议：Content-Length 分帧 ────────────────────────────────────────────
bool FLuaDebugAdapter::ReadMessage(FString& OutJson)
{
    if (!ClientSocket)
        return false;

    uint8 Chunk[4096];
    int32 Read = 0;
    while (ClientSocket->Recv(Chunk, sizeof(Chunk), Read) && Read > 0)
        ReadBuffer.Append(Chunk, Read);

    if (ReadBuffer.Num() == 0)
        return false;

    // 找 "\r\n\r\n" 结尾的头部
    const ANSICHAR* Data = reinterpret_cast<const ANSICHAR*>(ReadBuffer.GetData());
    const int32 Available = ReadBuffer.Num();
    int32 HeaderEnd = -1;
    for (int32 i = 0; i + 3 < Available; ++i)
    {
        if (Data[i] == '\r' && Data[i + 1] == '\n' && Data[i + 2] == '\r' && Data[i + 3] == '\n')
        {
            HeaderEnd = i + 4;
            break;
        }
    }
    if (HeaderEnd < 0)
        return false;

    // 头部是 ASCII，直接按字节转
    FUTF8ToTCHAR Converter(Data, HeaderEnd);
    FString HeaderText(Converter.Get());

    int32 ContentLength = -1;
    TArray<FString> Lines;
    HeaderText.Replace(TEXT("\r"), TEXT("")).ParseIntoArray(Lines, TEXT("\n"), true);
    for (const FString& Line : Lines)
    {
        FString Key, Value;
        if (Line.Split(TEXT(":"), &Key, &Value))
        {
            Key.TrimStartAndEndInline();
            Value.TrimStartAndEndInline();
            if (Key.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
                ContentLength = FCString::Atoi(*Value);
        }
    }

    if (ContentLength < 0 || Available < HeaderEnd + ContentLength)
        return false;

    FUTF8ToTCHAR BodyConverter(Data + HeaderEnd, ContentLength);
    OutJson = FString(BodyConverter.Get());
    ReadBuffer.RemoveAt(0, HeaderEnd + ContentLength, EAllowShrinking::No);
    return true;
}

void FLuaDebugAdapter::SendJson(const TSharedRef<FJsonObject>& Payload)
{
    if (!ClientSocket)
        return;

    FString Body;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
    FJsonSerializer::Serialize(Payload, Writer);

    const FTCHARToUTF8 BodyUtf8(*Body);
    const FString Header = FString::Printf(TEXT("Content-Length: %d\r\n\r\n"), BodyUtf8.Length());

    const FTCHARToUTF8 HeaderUtf8(*Header);
    int32 Sent = 0;
    ClientSocket->Send(reinterpret_cast<const uint8*>(HeaderUtf8.Get()), HeaderUtf8.Length(), Sent);
    Sent = 0;
    ClientSocket->Send(reinterpret_cast<const uint8*>(BodyUtf8.Get()), BodyUtf8.Length(), Sent);
}

void FLuaDebugAdapter::SendEvent(const FString& Event, const TSharedPtr<FJsonObject>& Body)
{
    const TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
    Payload->SetNumberField(TEXT("seq"), ++Seq);
    Payload->SetStringField(TEXT("type"), TEXT("event"));
    Payload->SetStringField(TEXT("event"), Event);
    if (Body.IsValid())
        Payload->SetObjectField(TEXT("body"), Body.ToSharedRef());

    SendJson(Payload);
}

void FLuaDebugAdapter::SendError(const TSharedRef<FJsonObject>& Request, const FString& Message)
{
    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), Request->GetStringField(TEXT("command")));
    Response->SetBoolField(TEXT("success"), false);
    Response->SetStringField(TEXT("message"), Message);
    SendJson(Response);
}

void FLuaDebugAdapter::Unsupported(const TSharedRef<FJsonObject>& Request, const FString& Why)
{
    SendError(Request, FString::Printf(TEXT("lua.dap: %s"), *Why));
}

// ── 暂停状态：阻塞模式下服务不会主动通知，只能轮询 ─────────────────────────
void FLuaDebugAdapter::PumpStoppedState()
{
    auto& Service = UnLuaNova::FLuaDebugService::Get();
    const bool bPaused = Service.IsAttached() && Service.IsPaused();

    if (bPaused && !bPausedReported)
    {
        bPausedReported = true;

        const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
        Body->SetStringField(TEXT("reason"), TEXT("breakpoint"));
        Body->SetNumberField(TEXT("threadId"), 1);
        SendEvent(TEXT("stopped"), Body);
    }
    else if (!bPaused && bPausedReported)
    {
        bPausedReported = false;
    }
}

// ── 断点：适配线程收集，游戏线程上落地 ─────────────────────────────────────
void FLuaDebugAdapter::RequestBreakpointSync()
{
    TArray<FLuaDapBreakpoint> Copy;
    {
        FScopeLock Lock(&PendingLock);
        Copy = PendingBreakpoints;
    }

    // 游戏线程在断点处是阻塞自旋的：这里排队执行，"暂停中改断点"要等 Continue 后生效
    AsyncTask(ENamedThreads::GameThread, [Copy]()
    {
        auto& Service = UnLuaNova::FLuaDebugService::Get();
        Service.ClearBreakpoints();
        for (const FLuaDapBreakpoint& Bp : Copy)
            Service.AddBreakpoint(Bp.Source, Bp.Line);
    });
}

// ── 请求分发 ──────────────────────────────────────────────────────────────
void FLuaDebugAdapter::OnInitialize(const TSharedRef<FJsonObject>& Request)
{
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetBoolField(TEXT("supportsConfigurationDoneRequest"), false);
    Body->SetBoolField(TEXT("supportsEvaluateRequest"), false);
    Body->SetBoolField(TEXT("supportsSetVariable"), false);
    Body->SetBoolField(TEXT("supportsSingleThreadExecutionRequests"), true);

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("initialize"));
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("body"), Body);
    SendJson(Response);

    SendEvent(TEXT("initialized"), nullptr);
}

void FLuaDebugAdapter::OnAttach(const TSharedRef<FJsonObject>& Request)
{
    // Attach 要装 debug hook，必须在游戏线程做；这里只排队，不等结果
    AsyncTask(ENamedThreads::GameThread, []()
    {
        auto* Env = IUnLuaNovaModule::Get().GetEnv();
        if (!Env)
        {
            UE_LOG(LogUnLuaNova, Warning, TEXT("lua.dap: no lua env to attach"));
            return;
        }

        auto& Service = UnLuaNova::FLuaDebugService::Get();
        Service.Attach(Env->GetMainState());
        // 阻塞模式：命中断点后游戏线程自旋等 Continue()，IDE 才有机会看栈
        Service.SetBlocking(true);
    });

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("attach"));
    Response->SetBoolField(TEXT("success"), true);
    SendJson(Response);
}

void FLuaDebugAdapter::OnSetBreakpoints(const TSharedRef<FJsonObject>& Request)
{
    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    const TSharedPtr<FJsonObject> Source = Args.IsValid() ? Args->GetObjectField(TEXT("source")) : nullptr;
    const FString Path = Source.IsValid() ? Source->GetStringField(TEXT("path")) : FString();

    const TArray<TSharedPtr<FJsonValue>>* Lines = nullptr;
    if (Args.IsValid() && Args->TryGetArrayField(TEXT("breakpoints"), Lines))
    {
        FScopeLock Lock(&PendingLock);
        // 先摘掉这个文件的旧断点，再按本次请求重建
        PendingBreakpoints.RemoveAll([&Path](const FLuaDapBreakpoint& Bp) { return Bp.Source == Path; });
        for (const TSharedPtr<FJsonValue>& Value : *Lines)
        {
            const TSharedPtr<FJsonObject> Bp = Value->AsObject();
            if (!Bp.IsValid())
                continue;
            PendingBreakpoints.Add({Path, (int32)Bp->GetNumberField(TEXT("line"))});
        }
    }

    RequestBreakpointSync();

    // 回包按我们实际持有的断点集合构造（不谎报 verified）
    TArray<TSharedPtr<FJsonValue>> Out;
    {
        FScopeLock Lock(&PendingLock);
        for (const FLuaDapBreakpoint& Bp : PendingBreakpoints)
        {
            if (Bp.Source != Path)
                continue;
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("line"), Bp.Line);
            Entry->SetBoolField(TEXT("verified"), false);
            Out.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("breakpoints"), Out);

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("setBreakpoints"));
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("body"), Body);
    SendJson(Response);
}

void FLuaDebugAdapter::OnThreads(const TSharedRef<FJsonObject>& Request)
{
    TArray<TSharedPtr<FJsonValue>> Threads;
    const TSharedRef<FJsonObject> Thread = MakeShared<FJsonObject>();
    Thread->SetNumberField(TEXT("id"), 1);
    Thread->SetStringField(TEXT("name"), TEXT("Lua"));
    Threads.Add(MakeShared<FJsonValueObject>(Thread));

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("threads"), Threads);

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("threads"));
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("body"), Body);
    SendJson(Response);
}

void FLuaDebugAdapter::OnStackTrace(const TSharedRef<FJsonObject>& Request)
{
    auto& Service = UnLuaNova::FLuaDebugService::Get();
    const TArray<UnLuaNova::FLuaDebugFrame>& Frames = Service.GetStackTrace();

    TArray<TSharedPtr<FJsonValue>> Out;
    for (const UnLuaNova::FLuaDebugFrame& Frame : Frames)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("id"), Frame.Depth);
        Entry->SetStringField(TEXT("name"), Frame.Function.IsEmpty() ? TEXT("(anonymous)") : Frame.Function);

        const TSharedRef<FJsonObject> Source = MakeShared<FJsonObject>();
        Source->SetStringField(TEXT("path"), Frame.Source);
        Entry->SetObjectField(TEXT("source"), Source);
        Entry->SetNumberField(TEXT("line"), Frame.Line);
        Entry->SetNumberField(TEXT("column"), 0);
        Out.Add(MakeShared<FJsonValueObject>(Entry));
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("stackFrames"), Out);
    Body->SetNumberField(TEXT("totalFrames"), Out.Num());

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("stackTrace"));
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("body"), Body);
    SendJson(Response);
}

void FLuaDebugAdapter::OnScopes(const TSharedRef<FJsonObject>& Request)
{
    TArray<TSharedPtr<FJsonValue>> Scopes;
    const TSharedRef<FJsonObject> Locals = MakeShared<FJsonObject>();
    Locals->SetStringField(TEXT("name"), TEXT("Locals"));
    Locals->SetNumberField(TEXT("variablesReference"), 1);   // 1 = 局部变量
    Locals->SetBoolField(TEXT("expensive"), false);
    Scopes.Add(MakeShared<FJsonValueObject>(Locals));

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("scopes"), Scopes);

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("scopes"));
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("body"), Body);
    SendJson(Response);
}

void FLuaDebugAdapter::OnVariables(const TSharedRef<FJsonObject>& Request)
{
    auto& Service = UnLuaNova::FLuaDebugService::Get();
    const TArray<UnLuaNova::FLuaDebugVariable>& Locals = Service.GetLocals();

    TArray<TSharedPtr<FJsonValue>> Out;
    for (const UnLuaNova::FLuaDebugVariable& Var : Locals)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Var.Name);
        Entry->SetStringField(TEXT("value"), Var.Value);
        Entry->SetStringField(TEXT("type"), Var.Type);
        Entry->SetNumberField(TEXT("variablesReference"), 0);
        Out.Add(MakeShared<FJsonValueObject>(Entry));
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("variables"), Out);

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("variables"));
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("body"), Body);
    SendJson(Response);
}

void FLuaDebugAdapter::OnEvaluate(const TSharedRef<FJsonObject>& Request)
{
    auto& Service = UnLuaNova::FLuaDebugService::Get();

    const FString Expression = Request->GetObjectField(TEXT("arguments"))->GetStringField(TEXT("expression"));

    // DAP frame ids are handed out by OnStackTrace as the frame depth, so the id is the
    // index - with one exception: expressions typed into a watch/repl window arrive with
    // no frameId at all, which means "globals only".
    int32 FrameIndex = INDEX_NONE;
    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    if (Args->HasField(TEXT("frameId")))
        FrameIndex = (int32)Args->GetNumberField(TEXT("frameId"));

    FString Value;
    FString Error;
    const bool bOk = Service.Evaluate(Expression, FrameIndex, Value, Error);

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("result"), bOk ? Value : Error);
    Body->SetNumberField(TEXT("variablesReference"), 0);

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("evaluate"));
    Response->SetBoolField(TEXT("success"), bOk);
    Response->SetObjectField(TEXT("body"), Body);
    if (!bOk)
        Response->SetStringField(TEXT("message"), Error);
    SendJson(Response);
}

void FLuaDebugAdapter::OnPause(const TSharedRef<FJsonObject>& Request)
{
    // RequestPause() touches VM state (it arms Luau's single step), so it has to run on
    // the game thread. The stop itself is picked up by PumpStoppedState, which sends the
    // `stopped` event - this response is only the acknowledgement.
    AsyncTask(ENamedThreads::GameThread, []
    {
        UnLuaNova::FLuaDebugService::Get().RequestPause();
    });

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("pause"));
    Response->SetBoolField(TEXT("success"), true);
    SendJson(Response);
}

void FLuaDebugAdapter::OnContinue(const TSharedRef<FJsonObject>& Request, bool bSingleStep)
{
    // Continue 必须在适配线程调用：游戏线程正阻塞在断点上，排 AsyncTask 永远等不到
    auto& Service = UnLuaNova::FLuaDebugService::Get();
    Service.SetSingleStep(bSingleStep);
    Service.Continue();
    bPausedReported = false;

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetBoolField(TEXT("allThreadsContinued"), true);

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), Request->GetStringField(TEXT("command")));
    Response->SetBoolField(TEXT("success"), true);
    Response->SetObjectField(TEXT("body"), Body);
    SendJson(Response);
}

void FLuaDebugAdapter::OnDisconnect(const TSharedRef<FJsonObject>& Request)
{
    auto& Service = UnLuaNova::FLuaDebugService::Get();
    Service.SetSingleStep(false);
    Service.Continue();

    AsyncTask(ENamedThreads::GameThread, []()
    {
        auto& Service2 = UnLuaNova::FLuaDebugService::Get();
        Service2.SetBlocking(false);
        Service2.Detach();
    });

    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), TEXT("disconnect"));
    Response->SetBoolField(TEXT("success"), true);
    SendJson(Response);

    CloseClient();
}

void FLuaDebugAdapter::Dispatch(const TSharedRef<FJsonObject>& Request)
{
    const FString Command = Request->GetStringField(TEXT("command"));

    if (Command == TEXT("initialize"))                  { OnInitialize(Request); return; }
    if (Command == TEXT("attach"))                      { OnAttach(Request); return; }
    if (Command == TEXT("setBreakpoints"))              { OnSetBreakpoints(Request); return; }
    if (Command == TEXT("threads"))                     { OnThreads(Request); return; }
    if (Command == TEXT("stackTrace"))                  { OnStackTrace(Request); return; }
    if (Command == TEXT("scopes"))                      { OnScopes(Request); return; }
    if (Command == TEXT("variables"))                   { OnVariables(Request); return; }
    if (Command == TEXT("evaluate"))                    { OnEvaluate(Request); return; }
    if (Command == TEXT("pause"))                       { OnPause(Request); return; }
    if (Command == TEXT("continue"))                    { OnContinue(Request, false); return; }
    if (Command == TEXT("next") || Command == TEXT("stepIn") || Command == TEXT("stepOut"))
                                                        { OnContinue(Request, true); return; }
    if (Command == TEXT("disconnect"))                  { OnDisconnect(Request); return; }

    Unsupported(Request, FString::Printf(TEXT("command '%s' is not implemented"), *Command));
}

// ── 适配线程主循环 ────────────────────────────────────────────────────────
uint32 FLuaDebugAdapter::Run()
{
    while (!bStopping)
    {
        AcceptClient();

        if (ClientSocket)
        {
            FString Json;
            while (ReadMessage(Json))
            {
                const auto Reader = TJsonReaderFactory<TCHAR>::Create(Json);
                TSharedPtr<FJsonObject> Request;
                if (FJsonSerializer::Deserialize(Reader, Request) && Request.IsValid())
                    Dispatch(Request.ToSharedRef());
            }

            PumpStoppedState();
        }

        FPlatformProcess::Sleep(0.01f);
    }

    CloseClient();

    if (ListenerSocket)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
    }
    return 0;
}

#undef LOCTEXT_NAMESPACE

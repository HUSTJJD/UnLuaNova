// Copyright (c) 2026 UnLuaNova contributors
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
#include "UnLuaNovaCompatibility.h"

#include "LuaDebugService.h"
#include "LuaEnv.h"
#include "UnLuaNovaModule.h"
#include "UnLuaNovaBase.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

#define LOCTEXT_NAMESPACE "LuaDebugAdapter"

namespace
{
    using EStepMode = UnLuaNova::FLuaDebugService::EStepMode;

    // `source`, `setBreakpoints` and `loadedSources` all resolve scripts from disk: the
    // adapter thread must never touch the lua_State. Accept an absolute path, else resolve
    // a dotted module name against the project's and the plugin's Script roots. On success
    // OutResolvedPath (when supplied) is the real file that was loaded; a missing file is
    // an explicit, non-fabricated result.
    bool FindLuaScript(const FString& InPath, FString& OutContent, FString* OutResolvedPath = nullptr)
    {
        if (InPath.IsEmpty())
            return false;

        TArray<FString> Candidates;
        Candidates.Add(InPath);

        FString Module = InPath;
        Module.ReplaceCharInline(TEXT('.'), TEXT('/'));
        Candidates.Add(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Script"), Module + TEXT(".lua")));
        Candidates.Add(FPaths::Combine(FPaths::ProjectDir(), TEXT("Plugins"), TEXT("UnLuaNova"),
            TEXT("Plugins"), TEXT("UnLuaNova"), TEXT("Content"), TEXT("Script"), Module + TEXT(".lua")));

        for (const FString& Candidate : Candidates)
        {
            if (FPaths::FileExists(Candidate) && FFileHelper::LoadFileToString(OutContent, *Candidate))
            {
                if (OutResolvedPath)
                    *OutResolvedPath = Candidate;
                return true;
            }
        }
        return false;
    }

    // Standard DAP requests this adapter deliberately does not implement. Each maps to a
    // concrete reason so an IDE (or someone reading the log) can tell "out of scope" from
    // "forgot to wire it up". The generic "not implemented" fallback below is reserved for
    // commands that are not part of DAP at all.
    struct FUnsupportedDapCommand
    {
        const TCHAR* Command;
        const TCHAR* Reason;
    };

    const TCHAR* FindUnsupportedDapReason(const FString& Command)
    {
        static const FUnsupportedDapCommand Known[] = {
            {TEXT("restart"),                   TEXT("restart is not supported: the editor owns the VM; use disconnect and attach again")},
            {TEXT("restartFrame"),              TEXT("restartFrame is not supported: Lua frames cannot be rewound; re-run the function instead")},
            {TEXT("terminate"),                 TEXT("terminate is not supported: the editor owns the VM; use disconnect to detach")},
            {TEXT("stepInTargets"),             TEXT("stepInTargets is not supported: stepIn resolves its target through the line hook")},
            {TEXT("goto"),                      TEXT("goto is not supported: Lua execution cannot jump to an arbitrary line")},
            {TEXT("gotoTargets"),               TEXT("gotoTargets is not supported: goto is not supported")},
            {TEXT("setExpression"),             TEXT("setExpression is not supported: use setVariable to write a local")},
            {TEXT("completions"),               TEXT("completions is not supported: there is no symbol table to complete against")},
            {TEXT("dataBreakpointInfo"),        TEXT("dataBreakpointInfo is not supported: Lua tables have no watchpoint primitive")},
            {TEXT("setDataBreakpoints"),        TEXT("setDataBreakpoints is not supported: Lua tables have no watchpoint primitive")},
            {TEXT("setInstructionBreakpoints"), TEXT("setInstructionBreakpoints is not supported: this adapter is source-level only")},
            {TEXT("readMemory"),                TEXT("readMemory is not supported: the adapter never exposes raw VM memory")},
            {TEXT("writeMemory"),               TEXT("writeMemory is not supported: the adapter never exposes raw VM memory")},
            {TEXT("disassemble"),               TEXT("disassemble is not supported: this adapter is source-level only; no bytecode view")},
            {TEXT("cancel"),                    TEXT("cancel is not supported: requests are handled synchronously, one at a time")},
            {TEXT("breakpointLocations"),       TEXT("breakpointLocations is not supported: setBreakpoints resolves line locations")},
            {TEXT("runInTerminal"),             TEXT("runInTerminal is not supported: the editor is already running; nothing to launch")},
        };
        for (const FUnsupportedDapCommand& Entry : Known)
        {
            if (Command == Entry.Command)
                return Entry.Reason;
        }
        return nullptr;
    }
}

FLuaDebugAdapter& FLuaDebugAdapter::Get()
{
    static FLuaDebugAdapter Instance;
    return Instance;
}

// ── 生命周期 ───────────────────────────────────────────────────────────────
bool FLuaDebugAdapter::Start(int32 InPort)
{
    FScopeLock Lifecycle(&LifecycleLock);
    if (bRunning)
    {
        UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("lua.dap: already running on port %d"), Port);
        return false;
    }

    Port = InPort >= 0 ? InPort : DefaultPort;
    if (!Listen())
        return false;

    bStopping = false;
    ++ServerGeneration;
    Seq = 0;
    PendingBreakpoints.Reset();
    bRunning = true;
    Thread = FRunnableThread::Create(this, TEXT("LuaDapThread"), 0, TPri_BelowNormal);
    if (!Thread)
    {
        bRunning = false;
        bStopping = true;
        ListenerSocket->Close();
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }

    UNLUANOVA_LOGF(LogUnLuaNova, Display, TEXT("lua.dap: listening on 127.0.0.1:%d (DAP, attach from VSCode)"), Port);
    return true;
}

void FLuaDebugAdapter::StopServer()
{
    // A timed-out caller and module teardown can arrive together. Only one owns Thread.
    FScopeLock Lifecycle(&LifecycleLock);
    if (!bRunning)
        return;

    bStopping = true;
    ++ServerGeneration;
    if (Thread)
    {
        Thread->WaitForCompletion();
        delete Thread;
        Thread = nullptr;
    }
    bRunning = false;

    UNLUANOVA_LOGF(LogUnLuaNova, Display, TEXT("lua.dap: stopped"));
}

int32 FLuaDebugAdapter::GetPort() const
{
    FScopeLock Lifecycle(&LifecycleLock);
    return Port;
}

bool FLuaDebugAdapter::Listen()
{
    auto* Subsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!Subsystem)
    {
        UNLUANOVA_LOGF(LogUnLuaNova, Error, TEXT("lua.dap: no socket subsystem"));
        return false;
    }

    ListenerSocket = Subsystem->CreateSocket(NAME_Stream, TEXT("LuaDapListener"), false);
    if (!ListenerSocket)
        return false;

    ListenerSocket->SetReuseAddr();
    ListenerSocket->SetNonBlocking(true);
    ListenerSocket->SetRecvErr();

    const TSharedRef<FInternetAddr> Addr = Subsystem->CreateInternetAddr();
    Addr->SetLoopbackAddress();
    Addr->SetPort(Port);

    if (!ListenerSocket->Bind(*Addr))
    {
        UNLUANOVA_LOGF(LogUnLuaNova, Error, TEXT("lua.dap: bind failed on port %d (in use?)"), Port);
        Subsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }

    if (!ListenerSocket->Listen(1))
    {
        UNLUANOVA_LOGF(LogUnLuaNova, Error, TEXT("lua.dap: listen failed"));
        Subsystem->DestroySocket(ListenerSocket);
        ListenerSocket = nullptr;
        return false;
    }

    Port = ListenerSocket->GetPortNo();
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
    UNLUANOVA_LOGF(LogUnLuaNova, Display, TEXT("lua.dap: client attached"));
}

// ── 线协议：Content-Length 分帧 ────────────────────────────────────────────
bool FLuaDebugAdapter::ReadMessage(FString& OutJson)
{
    if (!ClientSocket)
        return false;

    uint8 Chunk[4096];
    int32 Read = 0;
    while (!bStopping && ClientSocket->Recv(Chunk, sizeof(Chunk), Read) && Read > 0)
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
    FString HeaderText(Converter.Length(), Converter.Get());

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
    OutJson = FString(BodyConverter.Length(), BodyConverter.Get());
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

void FLuaDebugAdapter::SendSimpleResponse(const TSharedRef<FJsonObject>& Request,
    const TSharedPtr<FJsonObject>& Body)
{
    const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
    Response->SetNumberField(TEXT("seq"), ++Seq);
    Response->SetStringField(TEXT("type"), TEXT("response"));
    Response->SetNumberField(TEXT("request_seq"), Request->GetNumberField(TEXT("seq")));
    Response->SetStringField(TEXT("command"), Request->GetStringField(TEXT("command")));
    Response->SetBoolField(TEXT("success"), true);
    if (Body.IsValid())
        Response->SetObjectField(TEXT("body"), Body.ToSharedRef());
    SendJson(Response);
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
        const FString Reason = Service.GetStopReason();
        Body->SetStringField(TEXT("reason"), Reason);
        Body->SetNumberField(TEXT("threadId"), 1);
        if (Reason == TEXT("exception"))
        {
            Body->SetStringField(TEXT("text"), Service.GetLastException());
            Body->SetBoolField(TEXT("allThreadsStopped"), true);
        }
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
    const uint64 Generation = ServerGeneration.load();
    AsyncTask(ENamedThreads::GameThread, [this, Generation, Copy]()
    {
        if (bStopping || ServerGeneration.load() != Generation) return;
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
    Body->SetBoolField(TEXT("supportsConfigurationDoneRequest"), true);
    // OnEvaluate is implemented - locals and upvalues of the requested frame are bound as
    // parameters of a wrapper, and the expression runs with them in scope. Declaring it
    // false here is what made IDEs grey out the watch window entirely.
    Body->SetBoolField(TEXT("supportsEvaluateRequest"), true);
    Body->SetBoolField(TEXT("supportsSetVariable"), true);
    Body->SetBoolField(TEXT("supportsSingleThreadExecutionRequests"), true);
    Body->SetBoolField(TEXT("supportsModulesRequest"), true);
    // Genuinely not implemented: declare false so an IDE never asks for them.
    Body->SetBoolField(TEXT("supportsConditionalBreakpoints"), false);
    Body->SetBoolField(TEXT("supportsHitConditionalBreakpoints"), false);
    Body->SetBoolField(TEXT("supportsStepInTargetsRequest"), false);
    Body->SetBoolField(TEXT("supportsRestartRequest"), false);
    // OnSetFunctionBreakpoints is implemented, Dispatch routes setFunctionBreakpoints and
    // the command-probe spec asserts success=true: all three are in place, so declaring
    // false here was the sole reason IDEs never sent the request.
    Body->SetBoolField(TEXT("supportsFunctionBreakpoints"), true);
    // loadedSources is backed by the same FLuaDebugService::GetLoadedModules() snapshot as
    // `modules` (handler + Dispatch branch + spec case all present), so it is supported.
    Body->SetBoolField(TEXT("supportsLoadedSourcesRequest"), true);
    // exceptionInfo reads the error recorded by the shared Lua error handler; the two
    // filters are the ones that handler can actually arm. Anything else is refused by
    // setExceptionBreakpoints with verified=false rather than silently ignored.
    Body->SetBoolField(TEXT("supportsExceptionInfoRequest"), true);

    TArray<TSharedPtr<FJsonValue>> ExceptionFilters;
    auto AddFilter = [&ExceptionFilters](const TCHAR* Id, const TCHAR* Label)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("filter"), Id);
        Entry->SetStringField(TEXT("label"), Label);
        Entry->SetBoolField(TEXT("default"), false);
        ExceptionFilters.Add(MakeShared<FJsonValueObject>(Entry));
    };
    AddFilter(TEXT("raised"), TEXT("Raised Lua errors"));
    AddFilter(TEXT("uncaught"), TEXT("Uncaught Lua errors"));
    Body->SetArrayField(TEXT("exceptionBreakpointFilters"), ExceptionFilters);

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
    // Which env to attach to. "The" env is not a well-defined thing once
    // ULuaEnvLocator_ByGameInstance is in play - one per game instance, so a listen server
    // with two clients has three, and attaching to the wrong one means breakpoints that
    // never fire with no error anywhere. A launch.json that names an index is explicit;
    // leaving it out keeps the old single-env behaviour.
    int32 EnvIndex = 0;
    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    if (Args.IsValid() && Args->HasField(TEXT("env")))
    {
        EnvIndex = (int32)Args->GetNumberField(TEXT("env"));
    }

    // Attach 要装 debug hook，必须在游戏线程做；这里只排队，不等结果
    const uint64 Generation = ServerGeneration.load();
    AsyncTask(ENamedThreads::GameThread, [this, Generation, EnvIndex]()
    {
        if (bStopping || ServerGeneration.load() != Generation) return;
        const TArray<UnLuaNova::FLuaEnv*> Envs = UnLuaNova::FLuaEnv::GetAllEnvs();
        if (!Envs.IsValidIndex(EnvIndex))
        {
            UNLUANOVA_LOGF(LogUnLuaNova, Warning, TEXT("lua.dap: no lua env at index %d (%d env(s) exist)"),
                EnvIndex, Envs.Num());
            return;
        }

        auto& Service = UnLuaNova::FLuaDebugService::Get();
        Service.Attach(Envs[EnvIndex]->GetMainState());
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

void FLuaDebugAdapter::OnLaunch(const TSharedRef<FJsonObject>& Request)
{
    // UE owns the VM and the editor is already running: there is nothing to launch.
    Unsupported(Request, TEXT("launch is not supported; use 'attach' against a running editor"));
}

void FLuaDebugAdapter::OnConfigurationDone(const TSharedRef<FJsonObject>& Request)
{
    SendSimpleResponse(Request, nullptr);
}

void FLuaDebugAdapter::OnSetExceptionBreakpoints(const TSharedRef<FJsonObject>& Request)
{
    // The shared ReportLuaCallError handler forwards every uncaught error to the debug
    // service; enabling a filter arms the line hook so the VM stops with reason
    // "exception" on the next line. Unknown filters are reported verified=false instead
    // of being quietly accepted.
    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    const TArray<TSharedPtr<FJsonValue>>* Filters = nullptr;
    TArray<FString> Enabled;
    TArray<TSharedPtr<FJsonValue>> Out;

    if (Args.IsValid() && Args->TryGetArrayField(TEXT("filters"), Filters))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Filters)
        {
            const FString Filter = Value.IsValid() ? Value->AsString() : FString();
            const bool bSupported = Filter == TEXT("raised") || Filter == TEXT("uncaught");
            if (bSupported)
                Enabled.Add(Filter);

            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetBoolField(TEXT("verified"), bSupported);
            if (!bSupported)
                Entry->SetStringField(TEXT("message"),
                    FString::Printf(TEXT("unsupported exception filter '%s'"), *Filter));
            Out.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    // VM-side state belongs on the game thread; the adapter thread never touches lua_State.
    const uint64 Generation = ServerGeneration.load();
    AsyncTask(ENamedThreads::GameThread, [this, Generation, Enabled]()
    {
        if (bStopping || ServerGeneration.load() != Generation) return;
        UnLuaNova::FLuaDebugService::Get().SetExceptionFilters(Enabled);
    });

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("breakpoints"), Out);
    SendSimpleResponse(Request, Body);
}

void FLuaDebugAdapter::OnExceptionInfo(const TSharedRef<FJsonObject>& Request)
{
    const FString Message = UnLuaNova::FLuaDebugService::Get().GetLastException();
    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("exceptionId"), Message.IsEmpty() ? TEXT("none") : TEXT("luaError"));
    Body->SetStringField(TEXT("description"), Message.IsEmpty()
        ? TEXT("No Lua error has been recorded in this session.") : Message);
    Body->SetStringField(TEXT("breakMode"), Message.IsEmpty() ? TEXT("never") : TEXT("always"));
    SendSimpleResponse(Request, Body);
}

void FLuaDebugAdapter::OnSetFunctionBreakpoints(const TSharedRef<FJsonObject>& Request)
{
    // The service matches these names against lua_getinfo("n") in its line hook, so a
    // function breakpoint stops on the first line of a matching frame. Empty names are
    // reported verified=false rather than silently accepted.
    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    const TArray<TSharedPtr<FJsonValue>>* Breakpoints = nullptr;
    TArray<FString> Names;
    TArray<TSharedPtr<FJsonValue>> Out;

    if (Args.IsValid() && Args->TryGetArrayField(TEXT("breakpoints"), Breakpoints))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Breakpoints)
        {
            const TSharedPtr<FJsonObject> Bp = Value.IsValid() ? Value->AsObject() : nullptr;
            const FString Name = Bp.IsValid() ? Bp->GetStringField(TEXT("name")) : FString();
            const bool bVerified = !Name.IsEmpty();
            if (bVerified)
                Names.Add(Name);

            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetBoolField(TEXT("verified"), bVerified);
            if (!bVerified)
                Entry->SetStringField(TEXT("message"), TEXT("a function name is required"));
            Out.Add(MakeShared<FJsonValueObject>(Entry));
        }
    }

    // VM-side state is only touched on the game thread.
    const uint64 Generation = ServerGeneration.load();
    AsyncTask(ENamedThreads::GameThread, [this, Generation, Names]()
    {
        if (bStopping || ServerGeneration.load() != Generation) return;
        UnLuaNova::FLuaDebugService::Get().SetFunctionBreakpoints(Names);
    });

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("breakpoints"), Out);
    SendSimpleResponse(Request, Body);
}

void FLuaDebugAdapter::OnSetVariable(const TSharedRef<FJsonObject>& Request)
{
    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    const FString Name = Args.IsValid() ? Args->GetStringField(TEXT("name")) : FString();
    const FString Value = Args.IsValid() ? Args->GetStringField(TEXT("value")) : FString();
    int32 FrameIndex = INDEX_NONE;
    double FrameId = 0;
    if (Args.IsValid() && Args->TryGetNumberField(TEXT("frameId"), FrameId))
        FrameIndex = (int32)FrameId;

    FString OutValue, OutError;
    if (!UnLuaNova::FLuaDebugService::Get().SetVariable(Name, Value, FrameIndex, OutValue, OutError))
    {
        Unsupported(Request, OutError.IsEmpty() ? TEXT("setVariable failed") : OutError);
        return;
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("value"), OutValue);
    Body->SetNumberField(TEXT("variablesReference"), 0);
    SendSimpleResponse(Request, Body);
}

void FLuaDebugAdapter::OnSource(const TSharedRef<FJsonObject>& Request)
{
    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    const TSharedPtr<FJsonObject> Source = Args.IsValid() ? Args->GetObjectField(TEXT("source")) : nullptr;
    const FString Path = Source.IsValid() ? Source->GetStringField(TEXT("path")) : FString();

    FString Content;
    if (!FindLuaScript(Path, Content))
    {
        Unsupported(Request, FString::Printf(TEXT("source '%s' could not be read"), *Path));
        return;
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("content"), Content);
    Body->SetStringField(TEXT("mimeType"), TEXT("text/x-lua"));
    SendSimpleResponse(Request, Body);
}

void FLuaDebugAdapter::OnModules(const TSharedRef<FJsonObject>& Request)
{
    // Real package.loaded keys, snapshotted on the VM thread at attach and at every stop.
    // The adapter thread reads the copy kept by the service - it must never read the
    // lua_State itself. An unattached VM yields an empty list, which is the truth, not a
    // placeholder module invented to make the response look populated.
    const TArray<FString> Names = UnLuaNova::FLuaDebugService::Get().GetLoadedModules();

    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    double StartValue = 0.0;
    double CountValue = (double)Names.Num();
    if (Args.IsValid())
    {
        Args->TryGetNumberField(TEXT("startModule"), StartValue);
        Args->TryGetNumberField(TEXT("moduleCount"), CountValue);
    }
    const int32 Start = FMath::Clamp((int32)StartValue, 0, Names.Num());
    // A missing / zero moduleCount means "all remaining", as DAP specifies.
    int32 Count = (int32)CountValue;
    if (Count <= 0)
        Count = Names.Num() - Start;
    const int32 End = FMath::Clamp(Start + Count, Start, Names.Num());

    TArray<TSharedPtr<FJsonValue>> Modules;
    for (int32 Index = Start; Index < End; ++Index)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetNumberField(TEXT("id"), Index + 1);
        Entry->SetStringField(TEXT("name"), Names[Index]);
        // The host loader names modules by dotted name; that is the only path it has.
        Entry->SetStringField(TEXT("path"), Names[Index]);
        Entry->SetBoolField(TEXT("isOptimized"), false);
        Entry->SetBoolField(TEXT("isUserCode"), !Names[Index].StartsWith(TEXT("_")));
        Modules.Add(MakeShared<FJsonValueObject>(Entry));
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("modules"), Modules);
    Body->SetNumberField(TEXT("totalModules"), Names.Num());
    SendSimpleResponse(Request, Body);
}

void FLuaDebugAdapter::OnLoadedSources(const TSharedRef<FJsonObject>& Request)
{
    // DAP `loadedSources`: report every package.loaded key as a Source. Like `modules`
    // this reads the service's VM-thread snapshot - the adapter thread must never touch
    // lua_State. `modules` reports the dotted name as its path; here we try to resolve a
    // real disk path with the same resolver `source` / `setBreakpoints` use.
    const TArray<FString> Names = UnLuaNova::FLuaDebugService::Get().GetLoadedModules();

    TArray<TSharedPtr<FJsonValue>> Sources;
    for (const FString& Name : Names)
    {
        const TSharedRef<FJsonObject> Source = MakeShared<FJsonObject>();
        Source->SetStringField(TEXT("name"), Name);

        FString Script;
        FString ResolvedPath;
        const bool bResolved = FindLuaScript(Name, Script, &ResolvedPath);
        if (bResolved)
        {
            Source->SetStringField(TEXT("path"), ResolvedPath);
        }
        else
        {
            // No real script on disk: keep the dotted name (the host loader's only
            // identity for the module) as the path instead of inventing a file, and say
            // so explicitly. `pathUnresolved` is an adapter-specific hint; DAP clients
            // ignore unknown Source fields.
            Source->SetStringField(TEXT("path"), Name);
            Source->SetBoolField(TEXT("pathUnresolved"), true);
        }
        // 0 tells the client to load this Source from `path`, not by sourceReference.
        Source->SetNumberField(TEXT("sourceReference"), 0);
        Sources.Add(MakeShared<FJsonValueObject>(Source));
    }

    const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetArrayField(TEXT("sources"), Sources);
    SendSimpleResponse(Request, Body);
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

    // A breakpoint is verified when its source resolves to a real script on disk and the
    // line exists in that file. Source lookup is the same resolver `source` uses, so a
    // dotted module name and an absolute path agree on what is breakable.
    FString Script;
    const bool bSourceFound = FindLuaScript(Path, Script);
    int32 LineCount = 0;
    if (bSourceFound)
    {
        LineCount = 1;
        for (int32 Index = 0; Index < Script.Len(); ++Index)
        {
            if (Script[Index] == TEXT('\n'))
                ++LineCount;
        }
    }

    TArray<TSharedPtr<FJsonValue>> Out;
    {
        FScopeLock Lock(&PendingLock);
        for (const FLuaDapBreakpoint& Bp : PendingBreakpoints)
        {
            if (Bp.Source != Path)
                continue;
            const bool bVerified = bSourceFound && Bp.Line >= 1 && Bp.Line <= LineCount;
            const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
            Entry->SetNumberField(TEXT("line"), Bp.Line);
            Entry->SetBoolField(TEXT("verified"), bVerified);
            if (!bVerified)
            {
                Entry->SetStringField(TEXT("message"), bSourceFound
                    ? FString::Printf(TEXT("line %d is outside the %d-line source"), Bp.Line, LineCount)
                    : TEXT("source could not be resolved to a Lua script on disk"));
            }
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
    // Not named Thread: FLuaDebugAdapter has a member by that name (the DAP socket
    // thread), and shadowing it is an error under the plugin's warning settings.
    const TSharedRef<FJsonObject> ThreadEntry = MakeShared<FJsonObject>();
    ThreadEntry->SetNumberField(TEXT("id"), 1);
    ThreadEntry->SetStringField(TEXT("name"), TEXT("Lua"));
    Threads.Add(MakeShared<FJsonValueObject>(ThreadEntry));

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

    const TSharedPtr<FJsonObject> Args = Request->GetObjectField(TEXT("arguments"));
    double Requested = Service.GetLocalsReferenceId();
    if (Args.IsValid())
        Args->TryGetNumberField(TEXT("variablesReference"), Requested);
    const int32 ReferenceId = (int32)Requested;

    // Reference 1 is the Locals scope. Anything above that is a table captured at the
    // last stop; GetChildren refuses stale or unpaused handles instead of returning a
    // plausible-looking empty list.
    TArray<UnLuaNova::FLuaDebugVariable> Vars;
    if (ReferenceId <= Service.GetLocalsReferenceId())
    {
        Vars = Service.GetLocals();
    }
    else
    {
        FString Error;
        if (!Service.GetChildren(ReferenceId, Vars, Error))
        {
            Unsupported(Request, Error);
            return;
        }
    }

    TArray<TSharedPtr<FJsonValue>> Out;
    for (const UnLuaNova::FLuaDebugVariable& Var : Vars)
    {
        const TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("name"), Var.Name);
        Entry->SetStringField(TEXT("value"), Var.Value);
        Entry->SetStringField(TEXT("type"), Var.Type);
        Entry->SetNumberField(TEXT("variablesReference"), Var.VariablesReference);
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
    const uint64 Generation = ServerGeneration.load();
    AsyncTask(ENamedThreads::GameThread, [this, Generation]
    {
        if (bStopping || ServerGeneration.load() != Generation) return;
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

void FLuaDebugAdapter::OnContinue(const TSharedRef<FJsonObject>& Request, uint8 StepModeValue)
{
    // Continue 必须在适配线程调用：游戏线程正阻塞在断点上，排 AsyncTask 永远等不到
    auto& Service = UnLuaNova::FLuaDebugService::Get();
    const EStepMode Mode = static_cast<EStepMode>(StepModeValue);
    if (Mode == EStepMode::None)
    {
        Service.SetSingleStep(false);
        Service.Continue();
    }
    else
    {
        // BeginStep resumes the stopped VM too, so it stays on this thread.
        Service.BeginStep(Mode);
    }
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

    const uint64 Generation = ServerGeneration.load();
    AsyncTask(ENamedThreads::GameThread, [this, Generation]()
    {
        if (bStopping || ServerGeneration.load() != Generation) return;
        auto& Service2 = UnLuaNova::FLuaDebugService::Get();
        Service2.SetBlocking(false);
        Service2.SetExceptionFilters(TArray<FString>());
        Service2.SetFunctionBreakpoints(TArray<FString>());
        Service2.ClearException();
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
    if (Command == TEXT("launch"))                      { OnLaunch(Request); return; }
    if (Command == TEXT("configurationDone"))           { OnConfigurationDone(Request); return; }
    if (Command == TEXT("setBreakpoints"))              { OnSetBreakpoints(Request); return; }
    if (Command == TEXT("setExceptionBreakpoints"))     { OnSetExceptionBreakpoints(Request); return; }
    if (Command == TEXT("exceptionInfo"))               { OnExceptionInfo(Request); return; }
    if (Command == TEXT("setFunctionBreakpoints"))      { OnSetFunctionBreakpoints(Request); return; }
    if (Command == TEXT("setVariable"))                 { OnSetVariable(Request); return; }
    if (Command == TEXT("source"))                      { OnSource(Request); return; }
    if (Command == TEXT("modules"))                     { OnModules(Request); return; }
    if (Command == TEXT("loadedSources"))               { OnLoadedSources(Request); return; }
    if (Command == TEXT("threads"))                     { OnThreads(Request); return; }
    if (Command == TEXT("stackTrace"))                  { OnStackTrace(Request); return; }
    if (Command == TEXT("scopes"))                      { OnScopes(Request); return; }
    if (Command == TEXT("variables"))                   { OnVariables(Request); return; }
    if (Command == TEXT("evaluate"))                    { OnEvaluate(Request); return; }
    if (Command == TEXT("pause"))                       { OnPause(Request); return; }
    if (Command == TEXT("continue"))                    { OnContinue(Request, (uint8)EStepMode::None); return; }
    if (Command == TEXT("next"))                        { OnContinue(Request, (uint8)EStepMode::Over); return; }
    if (Command == TEXT("stepIn"))                      { OnContinue(Request, (uint8)EStepMode::In); return; }
    if (Command == TEXT("stepOut"))                     { OnContinue(Request, (uint8)EStepMode::Out); return; }
    if (Command == TEXT("disconnect"))                  { OnDisconnect(Request); return; }

    // A known DAP request we deliberately do not implement: give the concrete reason from
    // the table instead of the generic "not implemented" (which reads like a missing hook).
    if (const TCHAR* Reason = FindUnsupportedDapReason(Command))
    {
        Unsupported(Request, Reason);
        return;
    }

    // Truly unknown / non-DAP command: original fallback.
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
            while (!bStopping && ReadMessage(Json))
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

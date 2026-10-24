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

// A real loopback session against the DAP server, because the interesting failures are all
// in the wire layer. Everything below `FLuaDebugAdapter` - the service, the breakpoints, the
// stack - is covered by UnLuaNova.API.DebugService; what has never been tested is the part
// that decides whether an IDE can drive any of it: the Content-Length framing, the request
// dispatch, and the capability flags in `initialize`.
//
// Concretely, `supportsEvaluateRequest` was false while `OnEvaluate` was fully implemented,
// which turns the watch window grey in every IDE. A unit test of `OnEvaluate` would have
// passed the whole time. Only reading the actual bytes off the socket catches it.
//
// Design notes, because a socket test is where hangs come from:
//   * The body runs on ThreadPool, not the game thread. `OnAttach` and `OnDisconnect` queue
//     an AsyncTask(GameThread), so blocking the game thread waiting for them would deadlock.
//   * Every wait is bounded and returns false instead of spinning forever; a failure is a
//     failed assertion, never a hung suite.
//   * The port comes from binding port 0 rather than 5699, so a developer with `lua.dap`
//     already running does not get a spurious red.

#if WITH_EDITOR

#include "UnLuaNovaBase.h"
#include "Misc/AutomationTest.h"
#include "UnLuaNovaTestHelpers.h"

#include "LuaDebugAdapter.h"
#include "LuaDebugService.h"

#include "Dom/JsonObject.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Sockets.h"
#include "SocketSubsystem.h"

namespace
{
	constexpr double WaitSeconds = 5.0;

	int32 FindFreePort()
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets)
			return 0;

		FSocket* Probe = Sockets->CreateSocket(NAME_Stream, TEXT("UnLuaNovaDapPortProbe"));
		if (!Probe)
			return 0;

		TSharedRef<FInternetAddr> Addr = Sockets->CreateInternetAddr();
		Addr->SetAnyAddress();
		Addr->SetPort(0);

		int32 Port = 0;
		if (Probe->Bind(*Addr))
			Port = Probe->GetPortNo();

		Probe->Close();
		Sockets->DestroySocket(Probe);
		return Port;
	}

	FSocket* ConnectLoopback(int32 Port)
	{
		ISocketSubsystem* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
		if (!Sockets || Port <= 0)
			return nullptr;

		FSocket* Client = Sockets->CreateSocket(NAME_Stream, TEXT("UnLuaNovaDapClient"));
		if (!Client)
			return nullptr;

		Client->SetNonBlocking(false);

		TSharedRef<FInternetAddr> Addr = Sockets->CreateInternetAddr();
		Addr->SetLoopbackAddress();
		Addr->SetPort(Port);

		// The listener may not be up the instant Start() returns, so a failure to connect
		// is retried for a moment rather than reported immediately.
		const double Start = FPlatformTime::Seconds();
		while (!Client->Connect(*Addr))
		{
			if (FPlatformTime::Seconds() - Start > WaitSeconds)
			{
				Client->Close();
				Sockets->DestroySocket(Client);
				return nullptr;
			}
			FPlatformProcess::Sleep(0.02f);
		}

		return Client;
	}

	bool SendDap(FSocket* Client, const FString& Json)
	{
		const FTCHARToUTF8 Body(*Json);
		const FString Header = FString::Printf(TEXT("Content-Length: %d\r\n\r\n"), Body.Length());

		int32 Sent = 0;
		if (!Client->Send((const uint8*)TCHAR_TO_ANSI(*Header), Header.Len(), Sent))
			return false;

		Sent = 0;
		return Client->Send((const uint8*)Body.Get(), Body.Length(), Sent);
	}

	// Reads one framed message. Accumulates ASCII only - which is all DAP traffic here
	// carries, and a chunk boundary in the middle of a multi-byte sequence is the one thing
	// this deliberately does not handle.
	bool ReadDapMessage(FSocket* Client, FString& OutJson)
	{
		FString Pending;
		const double Start = FPlatformTime::Seconds();

		while (FPlatformTime::Seconds() - Start < WaitSeconds)
		{
			uint8 Buffer[4096];
			int32 Read = 0;
			if (Client->Recv(Buffer, sizeof(Buffer), Read) && Read > 0)
			{
				Pending.AppendChars((const ANSICHAR*)Buffer, Read);
			}
			else
			{
				FPlatformProcess::Sleep(0.01f);
			}

			const int32 HeaderEnd = Pending.Find(TEXT("\r\n\r\n"));
			if (HeaderEnd == INDEX_NONE)
				continue;

			const FString Header = Pending.Left(HeaderEnd);
			int32 Length = 0;
			if (!FParse::Value(*Header, TEXT("Content-Length:"), Length))
				continue;

			const int32 BodyStart = HeaderEnd + 4;
			if (Pending.Len() - BodyStart < Length)
				continue;

			OutJson = Pending.Mid(BodyStart, Length);
			return true;
		}

		return false;
	}

	// Sends a request and waits for *its* response. `initialize` also emits an `initialized`
	// event, so the first message back is not necessarily the answer - match on
	// request_seq, and drop anything that is not a response to this request.
	TSharedPtr<FJsonObject> RoundTrip(FSocket* Client, int32& Seq, const FString& Command,
		const TSharedPtr<FJsonObject>& Arguments)
	{
		const int32 ThisSeq = ++Seq;

		const TSharedRef<FJsonObject> Request = MakeShared<FJsonObject>();
		Request->SetNumberField(TEXT("seq"), ThisSeq);
		Request->SetStringField(TEXT("type"), TEXT("request"));
		Request->SetStringField(TEXT("command"), Command);
		if (Arguments.IsValid())
			Request->SetObjectField(TEXT("arguments"), Arguments);

		FString Out;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Request, Writer);

		if (!SendDap(Client, Out))
			return nullptr;

		const double Start = FPlatformTime::Seconds();
		while (FPlatformTime::Seconds() - Start < WaitSeconds)
		{
			FString Json;
			if (!ReadDapMessage(Client, Json))
				return nullptr;

			const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
			TSharedPtr<FJsonObject> Message;
			if (!FJsonSerializer::Deserialize(Reader, Message) || !Message.IsValid())
				continue;

			if (Message->GetStringField(TEXT("type")) != TEXT("response"))
				continue;

			if ((int32)Message->GetNumberField(TEXT("request_seq")) == ThisSeq)
				return Message;
		}

		return nullptr;
	}
}

BEGIN_DEFINE_SPEC(FDebugAdapterSpec, "UnLuaNova.API.DebugAdapter", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	lua_State* L;
END_DEFINE_SPEC(FDebugAdapterSpec)

void FDebugAdapterSpec::Define()
{
	BeforeEach([this]
	{
		// The adapter attaches to an env, so there has to be one.
		UnLuaNova::Startup();
		L = UnLuaNova::GetState();
	});

	AfterEach([this]
	{
		// Leave no server listening and no debug hook installed: both are process-wide, and
		// a breakpoint hook left behind would slow every later spec down.
		FLuaDebugAdapter::Get().StopServer();
		UnLuaNova::FLuaDebugService::Get().Detach();
		L = nullptr;
	});

	It(TEXT("一轮真实会话：initialize / attach / evaluate / threads / disconnect"), EAsyncExecution::ThreadPool, [this]()
	{
		const int32 Port = FindFreePort();
		TEST_TRUE(Port > 0);

		FLuaDebugAdapter& Adapter = FLuaDebugAdapter::Get();
		TEST_TRUE(Adapter.Start(Port));
		TEST_TRUE(Adapter.IsRunning());
		TEST_EQUAL(Adapter.GetPort(), Port);

		FSocket* Client = ConnectLoopback(Port);
		TEST_TRUE(Client != nullptr);
		if (!Client)
		{
			Adapter.StopServer();
			return;
		}

		int32 Seq = 0;

		// ── initialize: the capability flags are what the IDE reads to decide whether the
		// watch window is usable at all ──
		const TSharedRef<FJsonObject> InitArgs = MakeShared<FJsonObject>();
		InitArgs->SetStringField(TEXT("adapterID"), TEXT("unluanova"));
		const TSharedPtr<FJsonObject> Init = RoundTrip(Client, Seq, TEXT("initialize"), InitArgs);
		TEST_TRUE(Init.IsValid());
		if (Init.IsValid())
		{
			TEST_TRUE(Init->GetBoolField(TEXT("success")));
			const TSharedPtr<FJsonObject> Body = Init->GetObjectField(TEXT("body"));
			TEST_TRUE(Body.IsValid());
			if (Body.IsValid())
			{
				// False here greys out every watch expression, for a feature that is
				// implemented. See the file header.
				TEST_TRUE(Body->GetBoolField(TEXT("supportsEvaluateRequest")));
			}
		}

		// ── attach: which env. Queued to the game thread, hence the wait below rather than
		// an assumption that it has already happened ──
		const TSharedRef<FJsonObject> AttachArgs = MakeShared<FJsonObject>();
		AttachArgs->SetNumberField(TEXT("env"), 0);
		const TSharedPtr<FJsonObject> Attach = RoundTrip(Client, Seq, TEXT("attach"), AttachArgs);
		TEST_TRUE(Attach.IsValid());
		TEST_TRUE(Attach.IsValid() && Attach->GetBoolField(TEXT("success")));

		auto& Service = UnLuaNova::FLuaDebugService::Get();
		const double Start = FPlatformTime::Seconds();
		while (!Service.IsAttached() && FPlatformTime::Seconds() - Start < WaitSeconds)
			FPlatformProcess::Sleep(0.02f);
		TEST_TRUE(Service.IsAttached());

		// ── evaluate: no frameId means "globals only", which is a watch/repl window ──
		const TSharedRef<FJsonObject> EvalArgs = MakeShared<FJsonObject>();
		EvalArgs->SetStringField(TEXT("expression"), TEXT("1 + 1 == 2"));
		EvalArgs->SetStringField(TEXT("context"), TEXT("repl"));
		const TSharedPtr<FJsonObject> Eval = RoundTrip(Client, Seq, TEXT("evaluate"), EvalArgs);
		TEST_TRUE(Eval.IsValid());
		if (Eval.IsValid())
		{
			TEST_TRUE(Eval->GetBoolField(TEXT("success")));
			const TSharedPtr<FJsonObject> Body = Eval->GetObjectField(TEXT("body"));
			TEST_TRUE(Body.IsValid());
			if (Body.IsValid())
				TEST_EQUAL(Body->GetStringField(TEXT("result")), FString(TEXT("true")));
		}

		// ── threads ──
		const TSharedPtr<FJsonObject> Threads = RoundTrip(Client, Seq, TEXT("threads"), nullptr);
		TEST_TRUE(Threads.IsValid());
		if (Threads.IsValid())
		{
			TEST_TRUE(Threads->GetBoolField(TEXT("success")));
			const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
			const TSharedPtr<FJsonObject> Body = Threads->GetObjectField(TEXT("body"));
			TEST_TRUE(Body.IsValid() && Body->TryGetArrayField(TEXT("threads"), List));
			if (List)
				TEST_TRUE(List->Num() >= 1);
		}

		// ── disconnect ──
		const TSharedPtr<FJsonObject> Bye = RoundTrip(Client, Seq, TEXT("disconnect"), nullptr);
		TEST_TRUE(Bye.IsValid() && Bye->GetBoolField(TEXT("success")));

		Client->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Client);

		Adapter.StopServer();
		TEST_FALSE(Adapter.IsRunning());
	});

	It(TEXT("未知命令回 error 而不是把连接挂死"), EAsyncExecution::ThreadPool, [this]()
	{
		const int32 Port = FindFreePort();
		TEST_TRUE(Port > 0);

		FLuaDebugAdapter& Adapter = FLuaDebugAdapter::Get();
		TEST_TRUE(Adapter.Start(Port));

		FSocket* Client = ConnectLoopback(Port);
		TEST_TRUE(Client != nullptr);
		if (!Client)
		{
			Adapter.StopServer();
			return;
		}

		int32 Seq = 0;
		const TSharedPtr<FJsonObject> Reply = RoundTrip(Client, Seq, TEXT("notARealCommand"), nullptr);

		// The alternative is silence: no response at all, and an IDE that sits there waiting
		// while looking like it is still connected.
		TEST_TRUE(Reply.IsValid());
		if (Reply.IsValid())
			TEST_FALSE(Reply->GetBoolField(TEXT("success")));

		Client->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Client);
		Adapter.StopServer();
	});
}

#endif

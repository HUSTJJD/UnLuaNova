// Two-process coverage for the parts of Lua-declarative networking that a single-process
// PIE session cannot prove.
//
// LuaNetClientServerTest asserts where each RPC kind executes, that arguments survive the
// wire, and that a Lua-declared replicated field reaches the client. What it cannot prove
// is the one thing the docs listed as an open boundary: **that both peers number the RPCs
// the same way**. An RPC travels as an id, not a name, and in a single-process PIE session
// both peers share one UClass - so the ids agree by construction, whatever the code does.
// Two processes build the class independently, which is the case that can actually differ.
//
// So this file runs the declaration in two separate processes and compares what each one
// ended up with:
//
//   UnLuaNova.API.Net.DedicatedServer.Report   runs inside a peer process
//       (-UnLuaNovaNetRole=Server|Client). It opens the map, spawns the net actor so the
//       Lua module binds and declares, then writes what *this* process sees to
//       Saved/UnLuaNovaNet/<Role>.json.
//
//   UnLuaNova.API.Net.DedicatedServer          runs in the process driving the test. It
//       launches one peer per role, waits for both reports, and compares them.
//
// Note what this does not need: the peers never talk to each other. RPC numbering depends
// only on the module and the declaration order, both of which are identical on the two
// sides - so a connection is not required to prove the ids match, and the test stays free
// of the port / firewall / "did the DS actually start listening" failure modes that make
// networked CI flaky. What the -server flag buys is that the report is produced under
// NM_DedicatedServer, which is a path PIE never exercises.

#include "UnLuaNovaTestCommon.h"
#include "UnLuaNovaTestHelpers.h"
#include "TestCommands.h"
#include "UnLuaNovaNet.h"
#include "UnLuaNovaNetDecl.h"

#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace UnLuaNovaTestSuite
{
	// ------------------------------------------------------------------ paths ----

	static FString NetReportDir()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnLuaNovaNet"));
	}

	static FString NetReportPath(const FString& Role)
	{
		return FPaths::Combine(NetReportDir(), Role + TEXT(".json"));
	}

	/** -UnLuaNovaNetRole=Server|Client; empty in the process that drives the test. */
	static FString GetNetRole()
	{
		FString Role;
		FParse::Value(FCommandLine::Get(), TEXT("-UnLuaNovaNetRole="), Role);
		return Role;
	}

	// ------------------------------------------------------------------ report ----

	/**
	 * What one process sees: every Lua-declared RPC with the id it was given, plus the
	 * replicated fields. Small and sorted on purpose - it exists to be diffed.
	 */
	static TSharedRef<FJsonObject> BuildNetReport()
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();

		UClass* Class = AUnLuaNovaTestNetActor::StaticClass();

		TArray<TSharedPtr<FJsonValue>> Rpcs;
		for (TFieldIterator<UFunction> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
		{
			const UFunction* Function = *It;
			if (!Function->HasAnyFunctionFlags(FUNC_Net))
				continue;

			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("Name"), Function->GetName());
			// The whole point of the test. 0 means EnsureRpcId() never ran on this side,
			// which is the failure being hunted, so it is reported and not skipped.
			Entry->SetNumberField(TEXT("RpcId"), (double)Function->RPCId);
			Entry->SetBoolField(TEXT("Reliable"), Function->HasAnyFunctionFlags(FUNC_NetReliable));
			Rpcs.Add(MakeShared<FJsonValueObject>(Entry));
		}

		Rpcs.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
		{
			return A->AsObject()->GetStringField(TEXT("Name")) < B->AsObject()->GetStringField(TEXT("Name"));
		});
		Root->SetArrayField(TEXT("Rpcs"), Rpcs);

		TArray<TSharedPtr<FJsonValue>> Props;
		for (const FLuaRepDecl& Decl : FUnLuaNovaNet::GetReplicatedProps(Class))
		{
			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("Name"), Decl.Name.ToString());
			Entry->SetNumberField(TEXT("Type"), (double)Decl.Type);
			Entry->SetNumberField(TEXT("Condition"), (double)Decl.Condition);
			Props.Add(MakeShared<FJsonValueObject>(Entry));
		}
		Root->SetArrayField(TEXT("Props"), Props);

		// Recorded so a mismatch can be told apart from "the wrong process answered".
		Root->SetStringField(TEXT("Role"), GetNetRole());
		return Root;
	}

	static bool WriteNetReport(const FString& Path)
	{
		const TSharedRef<FJsonObject> Report = BuildNetReport();

		FString Text;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
		if (!FJsonSerializer::Serialize(Report, Writer))
			return false;

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Text, *Path);
	}

	/** Reads a report back. False on a missing or unparseable file - never a partial read. */
	static bool ReadNetReport(const FString& Path, TSharedPtr<FJsonObject>& Out)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
			return false;

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	}

	// ------------------------------------------------------------ latent cmds ----

	/** Spawns the net actor so its Lua module binds and declares. */
	class FSpawnNetActorCommand : public IAutomationLatentCommand
	{
	public:
		FSpawnNetActorCommand(FAutomationTestBase& InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			UWorld* World = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game)
				{
					World = Context.World();
					break;
				}
			}

			if (!World)
				return false;

			if (!World->SpawnActor<AUnLuaNovaTestNetActor>())
			{
				Test.AddError(TEXT("spawn AUnLuaNovaTestNetActor failed - the Lua module never binds, "
					"so nothing is declared and the report would be empty"));
			}
			return true;
		}

	private:
		FAutomationTestBase& Test;
	};

	/** Writes this peer's report. */
	class FWriteNetReportCommand : public IAutomationLatentCommand
	{
	public:
		FWriteNetReportCommand(FAutomationTestBase& InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const FString Path = NetReportPath(GetNetRole());

			// A stale report is the dangerous case: the driving process would read last
			// run's file and pass on a peer that never wrote one.
			IFileManager::Get().Delete(*Path, false, true, true);

			if (!WriteNetReport(Path))
				Test.AddError(FString::Printf(TEXT("could not write report %s"), *Path));

			return true;
		}

	private:
		FAutomationTestBase& Test;
	};

	/** Launches one peer process and remembers the handle so it can be cleaned up. */
	class FLaunchNetPeerCommand : public IAutomationLatentCommand
	{
	public:
		FLaunchNetPeerCommand(FAutomationTestBase& InTest, FString InRole, FString InCommandLine)
			: Test(InTest), Role(MoveTemp(InRole)), CommandLine(MoveTemp(InCommandLine)) {}

		virtual bool Update() override
		{
			const FString Exe = FPlatformProcess::ExecutablePath();
			if (!FPaths::FileExists(Exe))
			{
				Test.AddError(FString::Printf(TEXT("no executable at %s to launch the %s peer with"), *Exe, *Role));
				return true;
			}

			uint32 Pid = 0;
			const FProcHandle Handle = FPlatformProcess::CreateProc(*Exe, *CommandLine, false, false, false, &Pid, 0, nullptr, nullptr);
			if (!Handle.IsValid())
			{
				Test.AddError(FString::Printf(TEXT("launching the %s peer failed"), *Role));
				return true;
			}

			// Kept process-wide: the peer that hangs is the one that has to be killed, and
			// a latent command cannot be relied on to run its destructor.
			RunningPeers.Add(Handle);
			return true;
		}

		/** Every peer launched in this process; terminated by FTerminateNetPeersCommand. */
		static TArray<FProcHandle> RunningPeers;

	private:
		FAutomationTestBase& Test;
		FString Role;
		FString CommandLine;
	};

	TArray<FProcHandle> FLaunchNetPeerCommand::RunningPeers;

	/** Waits for every peer report to appear. */
	class FWaitForNetReportsCommand : public IAutomationLatentCommand
	{
	public:
		FWaitForNetReportsCommand(FAutomationTestBase& InTest, TArray<FString> InRoles)
			: Test(InTest), Roles(MoveTemp(InRoles)) {}

		virtual bool Update() override
		{
			// Wall clock, not FApp::GetDeltaTime(): a latent command can be pumped from a
			// context where the engine is not ticking, and a delta of 0 would spin forever
			// instead of ever reaching the timeout below.
			const double Now = FPlatformTime::Seconds();
			if (StartedAt == 0.0)
				StartedAt = Now;

			bool bAllPresent = true;
			for (const FString& Role : Roles)
			{
				if (!FPaths::FileExists(NetReportPath(Role)))
				{
					bAllPresent = false;
					break;
				}
			}

			if (bAllPresent)
				return true;

			if (Now - StartedAt > TimeoutSeconds)
			{
				for (const FString& Role : Roles)
				{
					if (!FPaths::FileExists(NetReportPath(Role)))
					{
						Test.AddError(FString::Printf(
							TEXT("%s peer never produced %s (%.0fs) - check its log under Saved/Logs"),
							*Role, *NetReportPath(Role), TimeoutSeconds));
					}
				}
				return true;
			}

			return false;
		}

	private:
		FAutomationTestBase& Test;
		TArray<FString> Roles;
		double StartedAt = 0.0;
		// Two editor processes have to start, load a map, bind a module and exit. Generous
		// on purpose: a timeout here is reported as a failure nobody will trust.
		double TimeoutSeconds = 180.0;
	};

	/** Kills whatever is still running. Always runs, including after a failure. */
	class FTerminateNetPeersCommand : public IAutomationLatentCommand
	{
	public:
		virtual bool Update() override
		{
			for (FProcHandle& Handle : FLaunchNetPeerCommand::RunningPeers)
			{
				if (FPlatformProcess::IsProcRunning(Handle))
					FPlatformProcess::TerminateProc(Handle, true);
				FPlatformProcess::CloseProc(Handle);
			}
			FLaunchNetPeerCommand::RunningPeers.Empty();
			return true;
		}
	};

	// ------------------------------------------------------------- assertions ----

	/** "Name:Id" per RPC - the form that makes a diff readable in a failure message. */
	static FString DescribeRpcs(const TSharedPtr<FJsonObject>& Report)
	{
		TArray<FString> Parts;
		for (const TSharedPtr<FJsonValue>& Value : Report->GetArrayField(TEXT("Rpcs")))
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			Parts.Add(FString::Printf(TEXT("%s:%d"), *Entry->GetStringField(TEXT("Name")), (int32)Entry->GetNumberField(TEXT("RpcId"))));
		}
		return FString::Join(Parts, TEXT(", "));
	}

	/** The command line a peer is launched with. */
	static FString BuildPeerCommandLine(const FString& MapName, const FString& Role, bool bServer)
	{
		// Same executable as the process running the test: a peer has to bind the same Lua
		// modules and load the same classes, and nothing guarantees that for a packaged
		// binary that may not have been built for this run.
		FString Args = FString::Printf(TEXT("\"%s\" "), *FPaths::GetProjectFilePath());

		if (bServer)
			Args += TEXT("-server ");

		Args += FString::Printf(TEXT("%s "), *MapName);
		Args += FString::Printf(
			TEXT("-unattended -NoSplash -nosound "
				 "-ExecCmds=\"Automation RunTests UnLuaNova.API.Net.DedicatedServer.Report; Quit\" "
				 "-UnLuaNovaNetRole=%s "),
			*Role);

		return Args;
	}
}

// ------------------------------------------------------------- the report ----

BEGIN_TESTSUITE(FLuaNetDedicatedServerReportTest,
	TEXT("UnLuaNova.API.Net.DedicatedServer.Report 在 -UnLuaNovaNetRole 指定的进程里导出本端的网络声明"))
	virtual bool RunTest(const FString& Parameters) override
	{
		const FString Role = UnLuaNovaTestSuite::GetNetRole();

		// Only when launched as a peer. In the driving process this would write a report
		// for the wrong role and the comparison would end up passing against itself.
		if (Role.IsEmpty())
		{
			AddInfo(TEXT("not running as a peer (no -UnLuaNovaNetRole) - nothing to report"));
			return true;
		}

		const FString MapName = TEXT("/UnLuaNovaTestSuite/Tests/Misc/Empty");
		ADD_LATENT_AUTOMATION_COMMAND(FOpenMapLatentCommand(MapName));
		ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FSpawnNetActorCommand(*this));
		// A tick between spawn and export: the Lua module binds on spawn, but the report
		// reads UFunction::RPCId, which is only filled once the declaration has run.
		ADD_LATENT_AUTOMATION_COMMAND(FUnLuaNovaTestCommand_WaitOneTick());
		ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FWriteNetReportCommand(*this));
		// Close the session before the -ExecCmds "Quit" lands, so the peer exits cleanly
		// rather than being killed with a PIE world still open.
		ADD_LATENT_AUTOMATION_COMMAND(FEndPieSessionCommand());
		return true;
	}
END_TESTSUITE(FLuaNetDedicatedServerReportTest)

// --------------------------------------------------------- the comparison ----

namespace UnLuaNovaTestSuite
{
	class FCompareNetReportsCommand : public IAutomationLatentCommand
	{
	public:
		FCompareNetReportsCommand(FAutomationTestBase& InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			TSharedPtr<FJsonObject> Server, Client;
			if (!ReadNetReport(NetReportPath(TEXT("Server")), Server)
				|| !ReadNetReport(NetReportPath(TEXT("Client")), Client))
			{
				Test.AddError(TEXT("a peer report could not be read"));
				return true;
			}

			const FString ServerRpcs = DescribeRpcs(Server);
			const FString ClientRpcs = DescribeRpcs(Client);

			Test.TestTrue(TEXT("peer reports are not empty"), ServerRpcs.Len() > 0 && ClientRpcs.Len() > 0);
			Test.TestEqual(TEXT("RPC ids agree across two processes"), ServerRpcs, ClientRpcs);

			// 0 is EnsureRpcId() never having run on that side. Equal-and-zero would pass
			// the comparison above while proving nothing, so it is called out separately.
			Test.TestFalse(TEXT("every RPC got an id on both sides"),
				ServerRpcs.Contains(TEXT(":0")) || ClientRpcs.Contains(TEXT(":0")));

			const int32 ServerPropCount = Server->GetArrayField(TEXT("Props")).Num();
			const int32 ClientPropCount = Client->GetArrayField(TEXT("Props")).Num();
			Test.TestEqual(TEXT("replicated field count agrees"), ServerPropCount, ClientPropCount);
			Test.TestTrue(TEXT("the Lua-declared field is in the report"), ServerPropCount > 0);

			return true;
		}

	private:
		FAutomationTestBase& Test;
	};
}

BEGIN_TESTSUITE(FLuaNetDedicatedServerTest,
	TEXT("UnLuaNova.API.Net.DedicatedServer 两个进程各自声明出的 RPC 编号一致"))
	virtual bool RunTest(const FString& Parameters) override
	{
		if (!UnLuaNovaTestSuite::GetNetRole().IsEmpty())
		{
			AddInfo(TEXT("running as a peer - the comparison belongs to the driving process"));
			return true;
		}

		const FString MapName = TEXT("/UnLuaNovaTestSuite/Tests/Misc/Empty");
		const FString ServerPath = UnLuaNovaTestSuite::NetReportPath(TEXT("Server"));
		const FString ClientPath = UnLuaNovaTestSuite::NetReportPath(TEXT("Client"));

		// Both deleted up front: a leftover from a previous run is indistinguishable from a
		// peer that answered, and this test exists to catch a peer that did not.
		IFileManager::Get().Delete(*ServerPath, false, true, true);
		IFileManager::Get().Delete(*ClientPath, false, true, true);

		ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FLaunchNetPeerCommand(
			*this, TEXT("Server"), UnLuaNovaTestSuite::BuildPeerCommandLine(MapName, TEXT("Server"), true)));
		ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FLaunchNetPeerCommand(
			*this, TEXT("Client"), UnLuaNovaTestSuite::BuildPeerCommandLine(MapName, TEXT("Client"), false)));
		ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FWaitForNetReportsCommand(
			*this, { TEXT("Server"), TEXT("Client") }));
		ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FCompareNetReportsCommand(*this));

		// Last, and unconditionally: a peer that is still running holds the report file and
		// keeps a second editor alive after the run has finished.
		ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FTerminateNetPeersCommand());
		return true;
	}
END_TESTSUITE(FLuaNetDedicatedServerTest)

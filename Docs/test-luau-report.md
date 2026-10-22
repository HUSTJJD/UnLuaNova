---
title: "测试报告（Luau 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Luau 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-22 15:38:54。请勿手工编辑。

## 崩溃

**测试进程崩了** —— 自动化框架没来得及导出任何 JSON，所以下面的统计不是这次运行的真实结果。

- 崩溃时正在跑：`分发条件：非网络 / 非 latent / 非接口函数会走预解析路径`

```
[2026.10.22-07.38.53:278][654]LogWindows: Error: === Critical error: ===
[2026.10.22-07.38.53:278][654]LogWindows: Error: Fatal error!
[2026.10.22-07.38.53:278][654]LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000000000000014
```

<details><summary>调用栈</summary>

```
[2026.10.22-07.38.53:278][654]LogWindows: Error: [Callstack] 0x000001ce65da8e0f UnrealEditor-UnLuaNovaTestSuite.dll!`FCallUEFastSpec::Define'::`2'::<lambda_2>::operator()() [Plugins\UnLuaNovaTestSuite\Source\UnLuaNovaTestSuite\Private\Specs\CallUEFast.spec.cpp:79]
[2026.10.22-07.38.53:278][654]LogWindows: Error: [Callstack] 0x000001ce65d5c30d UnrealEditor-UnLuaNovaTestSuite.dll!`FAutomationSpecBase::FAsyncLatentCommand::Update'::`5'::<lambda_1>::operator()() [<UE>\Engine\Source\Runtime\Core\Public\Misc\AutomationTest.h:3119]
[2026.10.22-07.38.53:279][654]LogWindows: Error: [Callstack] 0x000001ce65d3f9cb UnrealEditor-UnLuaNovaTestSuite.dll!SetPromise<TUniqueFunction<void __cdecl(void)> &>() [<UE>\Engine\Source\Runtime\Core\Public\Async\Async.h:70]
[2026.10.22-07.38.53:279][654]LogWindows: Error: [Callstack] 0x000001ce65d940c9 UnrealEditor-UnLuaNovaTestSuite.dll!TGraphTask<TAsyncGraphTask<void> >::ExecuteTask() [<UE>\Engine\Source\Runtime\Core\Public\Async\TaskGraphInterfaces.h:703]
[2026.10.22-07.38.53:279][654]LogWindows: Error: [Callstack] 0x00007ffe849d4fb3 UnrealEditor-Core.dll!UE::Tasks::Private::FTaskBase::TryExecuteTask() [<UE>\Engine\Source\Runtime\Core\Public\Tasks\TaskPrivate.h:524]
[2026.10.22-07.38.53:279][654]LogWindows: Error: [Callstack] 0x00007ffe849c88fd UnrealEditor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread() [<UE>\Engine\Source\Runtime\Core\Private\Async\TaskGraph.cpp:807]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ffe849c8b19 UnrealEditor-Core.dll!FNamedTaskThread::ProcessTasksUntilIdle() [<UE>\Engine\Source\Runtime\Core\Private\Async\TaskGraph.cpp:707]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ffebe3591ae UnrealEditor-RenderCore.dll!FFrameEndSync::Sync() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderingThread.cpp:2568]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ff6c35b352b UnrealEditor-Cmd.exe!FEngineLoop::Tick() [<UE>\Engine\Source\Runtime\Launch\Private\LaunchEngineLoop.cpp:6088]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ff6c359c7cc UnrealEditor-Cmd.exe!GuardedMain() [<UE>\Engine\Source\Runtime\Launch\Private\Launch.cpp:190]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ff6c359c8ca UnrealEditor-Cmd.exe!GuardedMainWrapper() [<UE>\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:123]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ff6c35a155d UnrealEditor-Cmd.exe!LaunchWindowsStartup() [<UE>\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:277]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ff6c35b5974 UnrealEditor-Cmd.exe!WinMain() [<UE>\Engine\Source\Runtime\Launch\Private\Windows\LaunchWindows.cpp:338]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007ff6c35b808a UnrealEditor-Cmd.exe!__scrt_common_main_seh() [D:\a\_work\1\s\src\vctools\crt\vcstartup\src\startup\exe_common.inl:288]
[2026.10.22-07.38.53:280][654]LogWindows: Error: [Callstack] 0x00007fff3413dda7 KERNEL32.DLL!UnknownFunction []
```

</details>

## 结论

**全部通过 ✅** — 通过率 0.0%

| 指标 | 数值 |
|---|---|
| 用例总数 | 1 |
| 通过 | 0 |
| 失败 | 0 |
| 跳过 | 1 |
| 截图 | 0 |
| Lua 后端 | `Luau` |

## 跳过明细（1 项）

| 用例 | 来源 |
|---|---|
| `luau-analyze` | `luau-analyze.json` |
## 截图

没有截图。截图由 `UnLuaNova.Showcase.*` 这几条用例产出（默认就会收进报告目录），加 `-NoScreenshots` 或跑 `-NullRHI`（无头）时不会有画面。

## 静态类型检查

这一轮没有跑：`Tools/RunTests.ps1` 没写 `luau-analyze.json`。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Backend Luau -Publish`（要两份就 `-Backend Both`）。


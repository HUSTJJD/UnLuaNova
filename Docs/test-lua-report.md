---
title: "测试报告（Lua 后端）"
description: "自动化测试产出的通过率、通过明细、失败明细与截图"
---

# 测试报告（Lua 后端）

> 由 `Tools/GenerateTestReport.py` 自动生成于 2026-10-22 21:59:40。请勿手工编辑。

## 崩溃

**测试进程崩了** —— 自动化框架没来得及导出任何 JSON，所以下面的统计不是这次运行的真实结果。

- 崩溃时正在跑：`UMG UMG 与 Lua 生命周期示例截图`

```
[2026.10.22-13.58.14:519][438]LogWindows: Error: === Critical error: ===
[2026.10.22-13.58.14:519][438]LogWindows: Error: Fatal error!
[2026.10.22-13.58.14:520][438]LogWindows: Error: Unhandled Exception: EXCEPTION_ACCESS_VIOLATION reading address 0x0000000080000084
```

<details><summary>调用栈</summary>

```
[2026.10.22-13.58.14:520][438]LogWindows: Error: [Callstack] 0x00007ffe85b20d78 UnrealEditor-Core.dll!mi_usable_size() [<UE>\Engine\Source\ThirdParty\mimalloc\2.0.0\src\alloc.c:549]
[2026.10.22-13.58.14:520][438]LogWindows: Error: [Callstack] 0x00007ffe85b19ef0 UnrealEditor-Core.dll!mi_heap_realloc_zero_aligned_at() [<UE>\Engine\Source\ThirdParty\mimalloc\2.0.0\src\alloc-aligned.c:122]
[2026.10.22-13.58.14:520][438]LogWindows: Error: [Callstack] 0x00007ffe85b1d043 UnrealEditor-Core.dll!mi_realloc_aligned() [<UE>\Engine\Source\ThirdParty\mimalloc\2.0.0\src\alloc-aligned.c:188]
[2026.10.22-13.58.14:520][438]LogWindows: Error: [Callstack] 0x00007ffe853c0b9e UnrealEditor-Core.dll!FMallocMimalloc::Realloc() [<UE>\Engine\Source\Runtime\Core\Private\HAL\MallocMimalloc.cpp:227]
[2026.10.22-13.58.14:520][438]LogWindows: Error: [Callstack] 0x00007ffe853c100b UnrealEditor-Core.dll!FMemory::Realloc() [<UE>\Engine\Source\Runtime\Core\Private\HAL\UnrealMemory.cpp:685]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffe8528fab8 UnrealEditor-Core.dll!TSizedHeapAllocator<32,FMemory>::ForAnyElementType::ResizeAllocation() [<UE>\Engine\Source\Runtime\Core\Public\Containers\ContainerAllocationPolicies.h:863]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffe72878f53 UnrealEditor-D3D12RHI.dll!UE::Core::Private::ReallocGrow<3,TSizedHeapAllocator<32,FMemory>::ForAnyElementType>() [<UE>\Engine\Source\Runtime\Core\Public\Containers\Array.h:625]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffe729752b1 UnrealEditor-D3D12RHI.dll!TArray<FColor,TSizedDefaultAllocator<32> >::SetNumUninitialized() [<UE>\Engine\Source\Runtime\Core\Public\Containers\Array.h:2597]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffe7296e6f7 UnrealEditor-D3D12RHI.dll!FD3D12DynamicRHI::RHIReadSurfaceData() [<UE>\Engine\Source\Runtime\D3D12RHI\Private\D3D12RenderTarget.cpp:633]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffec085c7cb UnrealEditor-SlateRHIRenderer.dll!FRHICommandListImmediate::ReadSurfaceData() [<UE>\Engine\Source\Runtime\RHI\Public\RHICommandList.h:4619]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffec0836c81 UnrealEditor-SlateRHIRenderer.dll!TRDGLambdaPass<FReadbackTextureParameters,`FSlateRHIRenderer::DrawWindowViewport_RenderThread'::`77'::<lambda_2> >::Execute() [<UE>\Engine\Source\Runtime\RenderCore\Public\RenderGraphPass.h:704]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffebb7e213d UnrealEditor-RenderCore.dll!FRDGBuilder::ExecutePass() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderGraphBuilder.cpp:3525]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffebb7e299b UnrealEditor-RenderCore.dll!FRDGBuilder::ExecuteSerialPass() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderGraphBuilder.cpp:3549]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffebb7e0772 UnrealEditor-RenderCore.dll!FRDGBuilder::Execute() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderGraphBuilder.cpp:2157]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffec083469b UnrealEditor-SlateRHIRenderer.dll!FSlateRHIRenderer::DrawWindows_RenderThread() [<UE>\Engine\Source\Runtime\SlateRHIRenderer\Private\SlateRHIRenderer.cpp:1493]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffec0815418 UnrealEditor-SlateRHIRenderer.dll!`FSlateRHIRenderer::DrawWindows_Private'::`76'::<lambda_3>::operator()() [<UE>\Engine\Source\Runtime\SlateRHIRenderer\Private\SlateRHIRenderer.cpp:1752]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffebb8546d3 UnrealEditor-RenderCore.dll!ExecuteCommand() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderingThread.cpp:1533]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffebb814f2c UnrealEditor-RenderCore.dll!UE::RenderCommandPipe::FCommandList::ConsumeCommands<`FRenderThreadCommandPipe::ExecuteCommands'::`2'::<lambda_1> >() [<UE>\Engine\Source\Runtime\RenderCore\Public\RenderingThread.h:335]
[2026.10.22-13.58.14:521][438]LogWindows: Error: [Callstack] 0x00007ffebb83d101 UnrealEditor-RenderCore.dll!`FRenderThreadCommandPipe::EnqueueAndLaunch'::`5'::<lambda_1>::operator()() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderingThread.cpp:1928]
[2026.10.22-13.58.14:523][438]LogWindows: Error: [Callstack] 0x00007ffebb854f3c UnrealEditor-RenderCore.dll!TGraphTask<TFunctionGraphTaskImpl<void __cdecl(void),1> >::ExecuteTask() [<UE>\Engine\Source\Runtime\Core\Public\Async\TaskGraphInterfaces.h:703]
[2026.10.22-13.58.14:523][438]LogWindows: Error: [Callstack] 0x00007ffe85144fb3 UnrealEditor-Core.dll!UE::Tasks::Private::FTaskBase::TryExecuteTask() [<UE>\Engine\Source\Runtime\Core\Public\Tasks\TaskPrivate.h:524]
[2026.10.22-13.58.14:523][438]LogWindows: Error: [Callstack] 0x00007ffe851388fd UnrealEditor-Core.dll!FNamedTaskThread::ProcessTasksNamedThread() [<UE>\Engine\Source\Runtime\Core\Private\Async\TaskGraph.cpp:807]
[2026.10.22-13.58.14:523][438]LogWindows: Error: [Callstack] 0x00007ffe85138c6e UnrealEditor-Core.dll!FNamedTaskThread::ProcessTasksUntilQuit() [<UE>\Engine\Source\Runtime\Core\Private\Async\TaskGraph.cpp:696]
[2026.10.22-13.58.14:523][438]LogWindows: Error: [Callstack] 0x00007ffebb883623 UnrealEditor-RenderCore.dll!RenderingThreadMain() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderingThread.cpp:261]
[2026.10.22-13.58.14:523][438]LogWindows: Error: [Callstack] 0x00007ffebb8852a4 UnrealEditor-RenderCore.dll!FRenderingThread::Run() [<UE>\Engine\Source\Runtime\RenderCore\Private\RenderingThread.cpp:390]
```

</details>

## 结论

**没有找到任何自动化测试结果：进程崩溃，见上面的「崩溃」一节。**
## 整场录屏

不是 showcase 的剪辑——剪辑是用例自己录的，只覆盖 showcase 那几个测试；跑挂了的那一轮往往一个剪辑都没有。这一段是 `Tools/RunTests.ps1` 在启动编辑器的同时开始录、编辑器退出时结束，从第一条测试到最后一条都在里面（窗口出现前的几秒除外）。

<video controls muted playsinline width="640" src="Images/tests-lua/session.mp4"></video>

[打不开播放器的话：直接下载这个 mp4（标准 H.264）](Images/tests-lua/session.mp4)

## 截图

### Showcase_Binding

![Showcase_Binding](Images/tests-lua/Showcase_Binding.png)

### Showcase_Container

![Showcase_Container](Images/tests-lua/Showcase_Container.png)

### Showcase_Override

![Showcase_Override](Images/tests-lua/Showcase_Override.png)

### Showcase_UMG

![Showcase_UMG](Images/tests-lua/Showcase_UMG.png)

## 视频

### Showcase_Binding_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Binding_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_Binding_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Container_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Container_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_Container_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_Override_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_Override_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_Override_Clip.mp4)

，60 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

### Showcase_UMG_Clip

<video controls muted loop playsinline width="640" src="Images/tests-lua/Showcase_UMG_Clip.mp4"></video>

[播放不了？直接打开视频文件（标准 H.264 mp4）](Images/tests-lua/Showcase_UMG_Clip.mp4)

，15 帧，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。

## 静态类型检查

这一轮没有跑：`Tools/RunTests.ps1` 没写 `lua-analyze.json`。

## 性能环比

| 用例 | 本次（µs/次） | 上次（µs/次） | 变化 |
|---|---|---|---|
| Lua | 1.8601 | 1.6709 | +11.3% |
| Native | 0.0001 | — | 无上次数据 |
| Reflection | 0.5225 | 0.8428 | -38.0% ⚠️ |

> 超过 ±30% 会标 ⚠️，但**不会**让这轮失败：计时受机器负载影响，这里只做提示，不做判定。

---

原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），这里只提交结论。重新生成：`Tools/RunTests.ps1 -Backend Lua -Publish`（要两份就 `-Backend Both`）。


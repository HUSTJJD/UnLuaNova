---
title: "测试与自动化"
description: "用 UE Automation 框架跑测试、生成报告与截图"
---

# 测试与自动化

UnLuaNova 用 **UE 原生 Automation 框架**做测试，没有引入第三方框架。所有测试集中在插件
`UnLuaNovaTestSuite` 里，跑一次就能得到通过率、失败原因和截图。

> 本工程（UnLuaNovaDemo）在 `.uproject` 里把它打开了，自动化测试随时可跑。
> 它是**开发期工具、不是运行期依赖**，所以插件本身 `EnabledByDefault: false` ——
> 把 UnLuaNova 用到别的工程时不会被默认带进去，需要跑测试再手动开。

## 目录

```
Plugins/UnLuaNovaTestSuite/
├── Source/UnLuaNovaTestSuite/
│   ├── Public/                    测试基础设施（latent 命令、夹具、断言宏）
│   │   ├── TestCommands.h/.cpp        打开关卡 / 截图 等 latent 命令
│   │   ├── UnLuaNovaTestCommon.h      测试基类与宏
│   │   └── UnLuaNovaTestHelpers.h     UObject 桩、断言宏、静态导出
│   ├── Private/Specs/             单元测试（BDD 风格，BEGIN_DEFINE_SPEC）
│   └── Private/Tests/             回归与实例测试（Issue###Test）
└── Content/Tests/                 场景测试用的关卡与资源
```

## 一键运行

```powershell
# 跑全部 UnLuaNova 用例：先编译，两个后端各跑一遍，各出一份报告
.\Tools\RunTests.ps1

# 只跑某一类
.\Tools\RunTests.ps1 -Filter UnLuaNova.API

# 只跑三个可选扩展插件（LuaRapidjson / LuaProtobuf / LuaSocket）
.\Tools\RunTests.ps1 -Filter UnLuaNova.Extensions

# 只跑一个后端
.\Tools\RunTests.ps1 -Backend Luau

# 带截图，并把结果发布到文档站
.\Tools\RunTests.ps1 -Screenshots -Publish
```

脚本封装的就是这条命令，team 里不用再手敲 `-ExecCmds`：

```
UnrealEditor-Cmd.exe <uproject> -unattended -NoSplash -stdout
    -ExecCmds="Automation RunTests <filter>; Quit"
    -ReportExportPath=<dir> -abslog=<dir>\Editor.log
```

`-Backend` 默认 `Both`，所以上面那条"跑全部"实际上是**两轮**：改配置 → 编译 → 跑测试 →
再改回来 → 再编译 → 再跑，然后给出两份互不相干的报告。详见下方"两个后端各一份报告"。

### 常用参数

| 参数 | 默认 | 说明 |
|---|---|---|
| `-EnginePath` | 自动发现 | UE 源码根目录，见下方"路径怎么找" |
| `-ProjectPath` | 自动发现 | 工程文件，取仓库根目录下第一个 `.uproject` |
| `-DryRun` | 关 | 只打印将要执行的命令，不启动编辑器、不编译 |
| `-Backend` | `Both` | 跑哪个 Lua 后端：`Lua`、`Luau` 或 `Both` |
| `-Platform` / `-Configuration` | `Win64` / `Development` | 传给 UBT 的编译参数 |
| `-SkipBuild` | 关 | 跳过编译，直接跑现有二进制；**不允许同时切换后端**（见下） |
| `-Filter` | `UnLuaNova` | 用例过滤器，前缀匹配 |
| `-Map` | 空 | 场景测试要开哪个关卡；多数用例自建 World，不需要 |
| `-OutputDir` | `Saved/TestReports/<时间戳>` | 报告输出根目录，每个后端一个子目录 |
| `-ExtraCommands` | 空 | 跑完追加执行的控制台命令（截图用） |
| `-Screenshots` | 关 | 保留真实视口并收集截图；不加则 `-NullRHI`，快很多 |
| `-Publish` | 关 | 把报告和截图复制到 `Docs/`，供 Pages 发布 |
| `-NoReport` | 关 | 只跑测试，不生成报告 |

有失败用例时脚本退出码为 `1`，可以直接挂到任何调度里。

### 需要真实视口的用例

默认跑法是 `-NullRHI`（不渲染，快很多）。但 UMG 控件在 `-NullRHI` 下**不会被 Tick**——
Slate 没有东西要画，`SObjectWidget::Tick` 压根不跑，所以任何"等控件 Tick 一次"的用例在
无头环境里永远等不到，表现和"被测的东西坏了"一模一样。Issue434 就是这种用例：它靠控件的
Tick 来 `Broadcast` 那个多播委托。

这类用例用 `BEGIN_VIEWPORT_TESTSUITE` 注册，也就是带上 `EAutomationTestFlags::NonNullRHI`：
命令行有 `-NullRHI` 时，框架直接把它从这一轮里剔除，而不是记一条失败——它没跑，不算通过也
不算失败。要真的跑到它，就别关渲染：

```powershell
.\Tools\RunTests.ps1 -Screenshots
```

报告里因此会比用例总数少那么一两条，这不是漏跑：把无头跑不了的条件判成"通过"才是假结果。

## 两个后端各一份报告

Lua 后端是**编译期选择**（见[设置项](Settings.md)里的"Lua 后端"）：两个后端导出同样的
`lua_*` 符号，不可能同时存在于一个构建里。所以"两个后端都测"不是加个开关就行，而是：

```powershell
.\Tools\RunTests.ps1 -Backend Both -Publish
```

对每个后端依次做：

1. 写 `Config/DefaultUnLuaNovaEditor.ini` 的 `LuaBackend=<后端>`；
2. 清掉 `Plugins/UnLuaNova/Intermediate/`（UBT 不会盯着这个 ini，不清就可能把上一个后端的
   目标文件当成最新的）；
3. `Engine/Build/BatchFiles/Build.bat <工程>Editor <平台> <配置> -Project=<uproject>`；
4. 跑测试，结果落到 `Saved/TestReports/<时间戳>/lua` 和 `.../luau`；
5. 生成报告；跑完把 ini 恢复成原样（`try/finally`，中断也会恢复）。

### 切后端是全局状态，所以整套动作是原子的

ini 是仓库级可变状态、二进制又是编译期产物，两者一旦分叉，跑出来的东西就会记在错误的
后端名下（真发生过：目录叫 `lua`，里面跑的是 Luau 的二进制）。脚本为此加了四道保险：

- **同时只允许一个 `RunTests.ps1`**：`Saved/RunTests.lock` 持有者写了自己的 PID，第二个
  进程直接报错退出；PID 已经不存在的残留锁会被自动清掉。少了这层，第二次运行的
  `Build.bat` 只是在 UBT 的 mutex 上等一等，然后跑第一次编译出来的产物。
- **编译失败立刻还原 ini**，不等脚本结束：否则 ini 指向一个从没编译成功的后端，下一次
  `-SkipBuild` 就会把旧二进制当成它来测。
- **启动编辑器前校验产物**：读 UBT 给插件模块生成的
  `Definitions.UnLuaNova.h`（里面的 `UNLUANOVA_LUA_BACKEND`），确认磁盘上的二进制确实是
  本次要跑的后端，而不是看目录名或 ini。
- **`-SkipBuild` 不允许切后端**：它只改 ini 不重编，等于给现有二进制换个标签，直接拒绝。

因此 `-SkipBuild` 只在"接着跑刚才那个后端"时才有用，比如报告生成失败想重跑一次。

### 崩溃也能出报告

编辑器崩掉时自动化框架不会导出任何 JSON，以前报告就只剩一句"0 tests"，看不出死在哪。
现在：`RunTests.ps1` 会把崩溃栈直接打到控制台，`GenerateTestReport.py` 从 `Editor.log`
里抓出**崩溃时正在跑的用例**和调用栈写进报告的「崩溃」一节，`summary.json` 里也有
`crashed` / `crash_test`（脚本据此把退出码置 1，CI 不会误判成通过）。

产物：

| 后端 | 本机报告 | 发布到文档站 |
|---|---|---|
| `Lua` | `Saved/TestReports/<时间戳>/lua/report.md` | `Docs/test-lua-report.md` |
| `Luau` | `Saved/TestReports/<时间戳>/luau/report.md` | `Docs/test-luau-report.md` |

两份报告都带后端标记（标题、结论表里的"Lua 后端"一行），不会看着看着忘了自己看的是哪份。
只想跑一个后端就用 `-Backend Lua` 或 `-Backend Luau`。


## 资产坏了怎么办

改名（UnLua → UnLuaNova）只能改文本，改不了 `.uasset`，所以测试内容里留下了几类**只有编辑器
能修**的损伤：蓝图存盘时没编译（包里没有 `SKEL_*` / `Default__*` / `BlueprintGeneratedClass`，
`Class ..._C` 取不到）、DataTable 的 RowStruct 丢了（引擎按 **Error** 级别打
`Missing RowStruct`，而 UE 的自动化把"测试期间出现 Error 日志"算作失败，于是一串不相干的
用例跟着挂）。

这些都交给 editor-context 的自动化用例 `UnLuaNova.Editor.RepairTestAssets`
（`Private/Tests/AssetRepairTest.cpp`）：它编译 `/UnLuaNovaTestSuite/` 下的所有蓝图、把两个
DataTable 的 RowStruct 与示例行补回去，然后存盘。它**不在默认的 `UnLuaNova.API` 过滤器下**
（会改资产），要显式跑：

```powershell
Tools\RunTests.ps1 -Backend Luau -Filter UnLuaNova.Editor
```

跑一次就够，之后照常 `Tools\RunTests.ps1`。仍然编译不过的资产它会以警告列出来 —— Issue634
是已知的：它的图里引用了一个已经不存在的结构体资产，得手工删节点。

这条用例**可能显示为 failed 但其实干完了活**：蓝图编译器对编译不过的资产会按 Error 打日志，
而"测试运行期间出现 Error 日志"在 UE 的自动化里就算失败。看报告里的 Info 行
（`Repaired: ... Compiled N of M Blueprint(s)`）判断修好了多少。

手工做是同一件事：Content Browser 里右键 `/UnLuaNovaTestSuite/` → Resave All，或逐个打开蓝图
点 Compile；引擎自带的命令行方案是
`UnrealEditor-Cmd.exe <uproject> -run=ResavePackages -packagefolder=/UnLuaNovaTestSuite/`
（不保证会触发蓝图重编译）。

> 用自动化驱动 UI 行不行？UE 有 Automation Driver（`FAutomationDriver`，可以合成输入去点
> Slate 控件），但那是测**界面行为**的，不是修资产的：修资产的正确入口就是编辑器 API
> （`FKismetEditorUtilities::CompileBlueprint` + 存包），headless 能跑、可重复、能进 CI，
> 也不受界面改版影响。

## 路径怎么找

脚本里**没有任何机器专属路径**，全部按下面顺序推导，所以换个克隆/换台机器都能直接跑：

1. 仓库根目录 = 脚本所在目录的上一级（`Tools/` 的父目录）；
2. 工程文件 = 仓库根目录下第一个 `*.uproject`；
3. UE 根目录，依次尝试：
   - `-EnginePath` 显式传入；
   - 环境变量 `UNREAL_ENGINE_PATH`；
   - **`.uproject` 里的 `EngineAssociation` GUID → 注册表 `HKCU:\Software\Epic Games\Unreal Engine\Builds`**（UnrealVersionSelector 维护的映射，最权威）；
   - 仓库根目录及其上 5 层里存在 `Engine/Binaries/Win64/UnrealEditor-Cmd.exe` 的目录（源码构建常放在工程旁边）；
   - 注册表里任意能找到该 exe 的引擎。

都没找到就报错并告诉你三种修法。想先确认解析结果就加 `-DryRun`。

## 报告与结论

跑完在每个后端的目录 `Saved/TestReports/<时间戳>/<后端>/` 下：

| 文件 | 内容 |
|---|---|
| `report.md` | 结论：通过率、总数/通过/失败/跳过、**通过明细**、失败明细（含错误消息与源码位置）、最慢用例、截图画廊 |
| `summary.json` | 机器可读的总数（含 `backend`），脚本据此决定退出码 |
| `Editor.log` | 本次运行的完整日志 |
| `screenshots/` | 测试期间产生的截图 |
| `*.json` | UE 导出的原始结果 |

`-Publish` 写到 `Docs/test-lua-report.md` / `Docs/test-luau-report.md` 的那两份是**要提交进 git** 的，
所以它们不写本机路径、也不写这一次的时间戳目录名（那两个每次跑都变，会让同一份结论在
git 里反复产生无意义 diff）。本机那份 `report.md` 仍然指向原始 JSON 目录，方便回溯。

失败明细会直接列出错误消息和 `文件:行号`，打开报告就能定位，不用回头翻日志。
如果一条用例被标成 success 但带了 error 记录，报告会按失败处理——那是伪装成通过的失败。

**通过明细同样会列出来**，不只是失败项：改了公共代码之后，"哪些还在正常"和"哪些坏了"一样重要，
而一个百分比答不了这个问题。通过项按用例名排序，所以结果没变时 git diff 也是干净的。

## 截图

两种触发方式：

1. **测试内部**：把 `FScreenshotLatentCommand` 加到 latent 命令序列里，它会等 PNG 真正落盘再继续。

   ```cpp
   ADD_LATENT_AUTOMATION_COMMAND(UnLuaNovaTestSuite::FScreenshotLatentCommand(TEXT("Showcase_Binding")));
   ```

2. **不经测试**：跑的时候追加控制台命令。

   ```powershell
   .\Tools\RunTests.ps1 -Map /UnLuaNovaTestSuite/Tests/Misc/YourMap -Screenshots `
       -ExtraCommands "UnLuaNovaTest.Screenshot Showcase_Binding"
   ```

截图默认写到 `Saved/Screenshots/WindowsEditor/`，脚本会收进报告目录。
`-Screenshots` 不能和 `-NullRHI` 同时用——无渲染就没有画面。

## 为什么不在 GitHub Actions 上跑

UnLuaNova 依赖**本地 UE 源码构建**（UE 5.8.3）。GitHub 托管 runner 拿不到这个引擎，
也编译不了它。所以：

- 测试在本机跑；
- 想上站点就加 `-Publish`，把生成的 `Docs/test-lua-report.md`、`Docs/test-luau-report.md` 和截图提交；
- Actions 只负责把 `Docs/` 渲染发布到 GitHub Pages。

如果你有自托管 Windows runner，把 `Tools/RunTests.ps1` 接进去即可，脚本的退出码已经准备好了。

## 写新用例的约定

- **命名**：`UnLuaNova.<域>.<主题>`，例如 `UnLuaNova.API.FLuaEnv`。Issue 回归保留 `Issue###` 编号。
- **单元测试**放 `Private/Specs/`，用 `BEGIN_DEFINE_SPEC`；**场景/实例测试**放 `Private/Tests/`。
- 全部包在 `#if WITH_DEV_AUTOMATION_TESTS` 里，保证打包版本不含测试代码。
- 需要 World 的用例走 `FOpenMapLatentCommand`，不要自己 `LoadMap`。

---

## 延伸阅读

- [测试报告（Lua 后端）](../test-lua-report.md) — 最新一次 Lua 后端测试报告
- [测试报告（Luau 后端）](../test-luau-report.md) — 最新一次 Luau 后端测试报告
- [Showcase 示例](Showcase.md) — 由测试产出的示例截图
- [调试](Debugging.md) — 调试 Lua 脚本

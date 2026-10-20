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
# 跑全部 UnLuaNova 用例
.\Tools\RunTests.ps1

# 只跑某一类
.\Tools\RunTests.ps1 -Filter UnLuaNova.API

# 只跑三个可选扩展插件（LuaRapidjson / LuaProtobuf / LuaSocket）
.\Tools\RunTests.ps1 -Filter UnLuaNova.Extensions

# 带截图，并把结果发布到文档站
.\Tools\RunTests.ps1 -Screenshots -Publish
```

脚本封装的就是这条命令，team 里不用再手敲 `-ExecCmds`：

```
UnrealEditor-Cmd.exe <uproject> -unattended -NoSplash -stdout
    -ExecCmds="Automation RunTests <filter>; Quit"
    -ReportExportPath=<dir> -abslog=<dir>\Editor.log
```

### 常用参数

| 参数 | 默认 | 说明 |
|---|---|---|
| `-EnginePath` | 自动发现 | UE 源码根目录，见下方"路径怎么找" |
| `-ProjectPath` | 自动发现 | 工程文件，取仓库根目录下第一个 `.uproject` |
| `-DryRun` | 关 | 只打印将要执行的命令，不启动编辑器 |
| `-Filter` | `UnLuaNova` | 用例过滤器，前缀匹配 |
| `-Map` | 空 | 场景测试要开哪个关卡；多数用例自建 World，不需要 |
| `-OutputDir` | `Saved/TestReports/<时间戳>` | 报告输出目录 |
| `-ExtraCommands` | 空 | 跑完追加执行的控制台命令（截图用） |
| `-Screenshots` | 关 | 保留真实视口并收集截图；不加则 `-NullRHI`，快很多 |
| `-Publish` | 关 | 把报告和截图复制到 `Docs/`，供 Pages 发布 |
| `-NoReport` | 关 | 只跑测试，不生成报告 |

有失败用例时脚本退出码为 `1`，可以直接挂到任何调度里。

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

跑完在 `Saved/TestReports/<时间戳>/` 下：

| 文件 | 内容 |
|---|---|
| `report.md` | 结论：通过率、总数/通过/失败/跳过、失败明细（含错误消息与源码位置）、最慢用例、截图画廊 |
| `summary.json` | 机器可读的总数，脚本据此决定退出码 |
| `Editor.log` | 本次运行的完整日志 |
| `screenshots/` | 测试期间产生的截图 |
| `*.json` | UE 导出的原始结果 |

失败明细会直接列出错误消息和 `文件:行号`，打开报告就能定位，不用回头翻日志。
如果一条用例被标成 success 但带了 error 记录，报告会按失败处理——那是伪装成通过的失败。

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
- 想上站点就加 `-Publish`，把生成的 `Docs/test-report.md` 和截图提交；
- Actions 只负责把 `Docs/` 渲染发布到 GitHub Pages。

如果你有自托管 Windows runner，把 `Tools/RunTests.ps1` 接进去即可，脚本的退出码已经准备好了。

## 写新用例的约定

- **命名**：`UnLuaNova.<域>.<主题>`，例如 `UnLuaNova.API.FLuaEnv`。Issue 回归保留 `Issue###` 编号。
- **单元测试**放 `Private/Specs/`，用 `BEGIN_DEFINE_SPEC`；**场景/实例测试**放 `Private/Tests/`。
- 全部包在 `#if WITH_DEV_AUTOMATION_TESTS` 里，保证打包版本不含测试代码。
- 需要 World 的用例走 `FOpenMapLatentCommand`，不要自己 `LoadMap`。

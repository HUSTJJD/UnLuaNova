---
title: 移动端验收路径（Android / iOS）
description: Android / iOS 的可复现验收路径、前置条件、脚本产物字段与升级为「已验收」的判定标准。当前两者均未验收，本页只提供可复现路径。
---

# 移动端验收路径（Android / iOS）

> **当前状态：Android / iOS 均未验收，本页只提供可复现路径。**
> 本仓库目前只有 **UE 6.0 / Win64** 通过完整 Release；移动端没有任何编译、打包、
> cooked smoke 或真机验收记录。本页不声称跑过 Unreal Engine，也不把预检当成验收结论。
> 权威状态以 [引擎版本与平台支持矩阵](Engine_Support.md) 为准；本页与
> [生产部署与运维](Production_Deployment.md) 配套阅读。

## 当前状态（平台现状表）

| 平台 | 本机 OS | 编译可行性 | 真机 / cooked smoke | 仓库验收状态 |
|---|---|---|---|---|
| Win64 | Windows 11 | 已通过 | 已通过（双后端自动化） | 已验证：UE 6.0 双后端 41/41 完整 Release |
| Android | Windows 11 | Android 工具链变量已设置；**UE 源码引擎的 Android 平台支持是否安装未经核实** | 需设备 / 模拟器，**是否可用未经核实** | 未验收 |
| iOS | Windows 11 | **本机物理不可行**（需 macOS + Xcode + iOS SDK） | 本机不可行 | 未验收 |
| Mac / Linux / 主机 | — | 声明为编译目标 | — | 未验收（编译目标，非已验收配置） |

主插件 `UnLuaNova.uplugin` 的 `PlatformAllowList` 含 `Win64 / Mac / IOS / Android / Linux`，
但 README 与 [引擎版本与平台支持矩阵](Engine_Support.md) 都明确：这只是**允许作为编译目标**，
不等于已通过验收。构建与验收工具链目前只在 Win64 跑过。

## 本机已核实的移动端环境

以下为维护者本机**已核实**的事实，写成文档是为了准确，不代表 UE 的移动端平台支持已装或已验收，
也不是对其它机器的要求。脚本通过环境变量 / 注册表动态发现这些值，**不在脚本里写死任何本机绝对路径**。

| 项 | 已核实值 / 事实 |
|---|---|
| 操作系统 | Windows 11（因此 iOS 编译与真机验收在本机物理不可行） |
| `ANDROID_HOME` | `C:\Android\Sdk` |
| `NDKROOT` | `C:\Android\Sdk\ndk\23.2.8568313` |
| `ANDROID_NDK_ROOT` | `D:\dev\android-ndk-r21e` |
| `JAVA_HOME` | `C:\Program Files\TencentKona-21.0.5.b1` |
| 源码引擎 | 注册在 `HKCU:\Software\Epic Games\Unreal Engine\Builds`，GUID 对应 UE 6.0 源码构建；由 `.uproject` 的 `EngineAssociation` 经注册表解析 |
| 未核实项 | 是否安装 UE 源码引擎的 Android 平台 SDK 支持、是否有可用设备 / 模拟器，**均未经核实** |

> iOS 需要 macOS + Xcode + iOS SDK + 签名身份 + 真机。GitHub 托管的 `macos-14` runner
> 有 Xcode，但**没有 UE 源码引擎、没有签名身份、没有已配置设备**，无法构建 UE；
> 详见下文与 `.github/workflows/mobile-acceptance.yml`。

## Android 前置条件与完整步骤

前置条件：

1. 一台 Windows 主机，且已安装并关联 **UE 6.0 源码引擎**（通过 `UNREAL_ENGINE_PATH`
   或 `.uproject` 的注册 GUID 发现）。
2. 设置 `ANDROID_HOME` / `NDKROOT` / `JAVA_HOME`，并完成 UE 的 Android 平台 SDK 配置
   （对应引擎的 `SetupAndroid` 流程）。**这些是否就绪需要本机确认，本仓库不能替你保证。**
3. 至少一台可用 Android 设备或模拟器（`adb devices` 能列出），且架构为 arm64。
4. 明确本插件刻意**不接受字节码输入**（见 [Luau 后端](Luau.md)），`lua.codegen` 在
   arm64 / Shipping 下**未做运行验收**（见 [生产部署与运维](Production_Deployment.md)），
   因此 cooked smoke 必须用源码脚本，不能依赖 codegen。

完整步骤（全部为 Operator 动作，本仓库不自动执行）：

```powershell
# 1) 只读预检：CI / 无设备环境也能真实跑一遍，输出 verdict=not-run
.\Tools\RunAndroidAcceptance.ps1 -PrecheckOnly

# 2) 仅预检、跳过构建
.\Tools\RunAndroidAcceptance.ps1 -SkipBuild

# 3) 完整构建编排（需真实环境与设备；引擎默认从 UNREAL_ENGINE_PATH / 注册表发现）
.\Tools\RunAndroidAcceptance.ps1 -EngineRoot <UE 源码引擎根目录> -Project <.uproject> -Configuration Development
```

`Tools/RunAndroidAcceptance.ps1` 只做**预检 + 构建编排 + 产物检查 + 结果落盘**：

- 预检：`ANDROID_HOME` / `NDKROOT` / `JAVA_HOME` 是否存在、UE 的 Android 平台 SDK 配置
  静态探测、`adb devices` 是否列出设备、引擎 `Engine/Build/Build.version` 是否可读、
  目标架构 arm64。预检是**尽力而为的静态探测**，不等于工具链已可用。
- 构建：调用 `Engine/Build/BatchFiles/RunUAT.bat BuildCookRun -platform=Android -cook
  -stage -package ...`。参数是 **UE 6.0 常规写法**（如 `-architectures=arm64`、
  `-cookflavor=ASTC`），**需按本机引擎小版本 / 设备实际调整**；引擎路径由参数 / 环境 /
  注册表动态发现，绝不硬编码。
- 产物检查：`Saved/StagedBuilds`、`Binaries/Android` 下 APK / AAB 是否存在、大小，
  以及归档内是否含 `Content/Script/` 脚本资产（见 [脚本交付](ScriptDelivery.md)）。
  这是**尽力而为的 zip 条目探测**：cooked 资产可能落在 `.pak` / `.utoc`（IoStore）里，
  字面路径不保证出现；真正的脚本 smoke 必须在设备上跑。

> 脚本**不执行设备 smoke、不伪造设备 / 编译 / 验收结果**。`-PrecheckOnly` 与
> `-SkipBuild` 永远不触发构建，结果永远是 `not-run`，便于 CI 诚实跑一遍。

独立 workflow `.github/workflows/mobile-acceptance.yml`（`workflow_dispatch` 触发）：

- `android-precheck`（`windows-latest`）：跑 `-PrecheckOnly`，把 JSON 作为 artifact 上传。
  托管 runner **没有 UE 源码引擎**，预期 `verdict = not-run`。
- `ios-precheck`（`macos-14`）：只做工具链静态检查并输出「需要 self-hosted macOS +
  UE 源码引擎 + Xcode 才能真实验收」，**不假装能构建 UE**。

## iOS 为何需要 macOS / self-hosted runner

- 编译 iOS 包需要 macOS + Xcode + iOS SDK + Apple 签名身份；在 Windows 11 本机
  **物理不可行**，不在本机尝试。
- GitHub 托管的 `macos-14` runner 虽有 Xcode，但**不含 UE 源码引擎、不含签名身份、
  不含已配置真机**，无法承担 UE 的 iOS 编译与验收；因此 `ios-precheck` 只做静态检查，
  输出 `not-run`，**不声称构建过 UE**。
- 真实 iOS 验收只能在 **self-hosted macOS runner**（装有 UE 源码引擎 + Xcode + iOS SDK
  + 真机 + 签名）上进行，并遵循与 Android 相同的「已验收」判定标准。

## 脚本产物字段说明

结果 JSON 输出到 `Saved/MobileAcceptance/android-<utc>.json`（可由 `-OutputDir` 覆盖）。
`Tools/tests/test_mobile_acceptance.py` 对脚本的预检项、verdict 枚举、无硬编码路径、
以及「预检失败不得产生 passed」做静态与构造 JSON 的回归校验。

| 字段 | 含义 |
|---|---|
| `schema` | 固定 `unluanova.mobile-acceptance/1` |
| `generated_utc` | UTC 生成时间 |
| `platform` / `os` / `configuration` / `mode` | 平台（Android）、OS（Windows）、配置、模式（`precheck-only` / `skip-build` / `full`） |
| `engine_root` / `engine_root_source` / `project` | 运行时发现的引擎根、来源（`env` / `registry` / `parameter`）、工程路径；均为真实运行时数据，非硬编码 |
| `precheck` | `ok`（全部预检通过）、`target_arch`（`arm64`）、`checks[]`、`reasons[]`、`notes[]` |
| `build` | `status`（`not-run` / `passed` / `failed`）、`command`、`exit_code`、`reason` |
| `artifacts` | `status`、`package`（相对路径）、`bytes`、`ok`、`has_script_assets`、`script_asset_count`、`notes`、`reason` |
| `smoke` | `status`（`not-run` / `passed`）、`evidence`、`device`、`backend`、`reason` |
| `verdict` | 只允许 `passed` / `failed` / `not-run` |
| `reason` | 对 verdict 的文字说明 |
| `verdict_enum` | 允许枚举 `['passed', 'failed', 'not-run']` |

verdict 语义（诚实标注）：

- **预检不通过 ⇒ `not-run` 并给出原因，绝不为 `passed`**（脚本内有硬规则与防御性不变量）。
- `-PrecheckOnly` / `-SkipBuild` ⇒ 未跑构建与 smoke ⇒ `not-run`。
- 构建真实失败 ⇒ `failed`；构建成功但产物缺失 ⇒ `failed`。
- `passed` **仅当** precheck / build / artifacts 全绿，且提供了外部真实 cooked 设备 smoke
  证据（`-SmokeEvidence`，脚本只校验其结构、不证明其可信度）时才可能出现；本脚本自身
  **不会**产生 `passed`。换句话说，单独运行本脚本不会得到 `passed`，这是刻意设计。

## 升级为「已验收」的判定标准

要把 Android 或 iOS 从「未验收」升级为「已验收」，必须**全部**满足（与
[引擎版本与平台支持矩阵](Engine_Support.md)、[发布就绪度](Release_Readiness.md) 一致）：

1. **编译通过**：目标平台（Android / iOS）用真实 UE 源码引擎完成构建与打包，且
   `BuildCookRun` 成功、APK / AAB（或 IPA）产物非空。
2. **cooked smoke**：cooked（非 Editor）构建下运行脚本烟测，覆盖脚本加载 / require /
   双后端基本能力；字节码输入不被接受，不得依赖 `lua.codegen` 的 arm64 / Shipping
   运行验收（尚未做）。
3. **至少一台真机运行**：在真实设备上安装、启动并采集日志，记录设备型号、OS、架构、
   后端与 commit；模拟器不能替代真机。
4. **双后端**：Lua 5.5.1 与 Luau 0.743-1 **两个后端各自**跑通上述编译 + cooked smoke + 真机。
5. **Release 记录**：产出可追溯摘要（`release-summary.json` 同等证据），`provenance.commit`
   与 HEAD 一致，写入 [引擎版本与平台支持矩阵](Engine_Support.md) 与 `CHANGELOG.md`，
   打 UnLuaNova 版本标签，并由 [测试与自动化](Testing.md) 的门禁复核。

在此之前，任何平台的状态都保持「未验收」，任何 `passed` 结论都不成立。

## 延伸阅读

[引擎版本与平台支持矩阵](Engine_Support.md) · [生产部署与运维](Production_Deployment.md) ·
[Luau 后端](Luau.md) · [测试与自动化](Testing.md) · [脚本交付](ScriptDelivery.md) ·
[发布就绪度](Release_Readiness.md)

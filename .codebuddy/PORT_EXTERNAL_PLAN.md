# 外部项目 UnLua feature 移植方案（调研结论）

> 源：某内部商业 UE 项目（下称「源项目」）。**源侧路径、项目名、内部插件名与私有 SDK 名均已脱敏**，本文不引用任何源项目绝对路径。
> 目标：本仓库（插件 `Plugins/UnLuauNova/`，UE 5.8.3 基线，已并入 11 个社区 PR + 静态绑定层 + Luau 子模块未接线）
> 本文只做方案，不含代码改动。

---

## 0. 一个必须先说清的前提：两边不是同一代架构

| | 源项目 | 本仓库（UnLuauNova） |
|---|---|---|
| 世代 | UnLua **2.1/2.2** 老架构 | UnLua **2.3.6** 新架构 |
| 运行时 | `FLuaContext` 单例 `GLuaCxt` | `UnLuauNova::FLuaEnv`（多 Env，可按 World/GameInstance 隔离） |
| 委托 | `LuaFunctionInjection` / `DelegateHelper` | `LuaOverrides` / `Registries/*` |
| 反射 | `Public/ReflectionUtils/**` | `Private/Registries/*` + `StaticBinding/*` |
| Lua VM | Lua 5.3 系（独立插件提供） | Lua 5.4.3，正在迁 Luau |

**结论：源侧核心模块（`Source/UnLua/` 下那份 fork）整体不可用。** 它是从 2.3.6 回退到 2.1 的分支，还丢了 UE5 适配（`UnLuaCompatibility.h` 在 `ENGINE_MINOR_VERSION<25` 分支未判 `ENGINE_MAJOR_VERSION`，UE5.0/5.1 下会把 `FProperty` typedef 成 `UProperty`）。
**只挑外围的、与内核解耦的能力，按新架构重写接入，不回退架构。**

### 0.1 源侧模块代号对照

为便于执行又不暴露源项目信息，下文统一用代号指代源侧模块：

| 代号 | 实际性质 |
|---|---|
| `SrcLuaTrace` | hook 式 Lua CPU Profiler，事件写进 Unreal Insights（自研，约 1300 行） |
| `SrcBlueprintExport` | 蓝图元数据导出三件套（Helper + ContentBrowser 右键菜单 + Commandlet，约 1580 行） |
| `SrcLuaFramework` | Lua 环境装配层，含沙盒优先加载器（约 330 行相关代码） |
| `SrcMemStats` | Lua 内存分配栈采集 + 云端上传（约 270 行） |
| `SrcRemoteDebugger` | 远程 Lua 调试器 + 编辑器可视化调试 UI（约 6500 行，依赖闭源 SDK） |
| `SrcSampleProfiler` | 自研采样式 Lua Profiler，TCP 上报（约 6000 行，含第三方 2000 行） |
| `SrcManualProfilerMark` | 手工埋点式 Profiler（约 330 行） |
| `SrcIdeLauncher` | 编辑器内拉起外部 IDE 的工具（约 24 KB） |
| `SrcUserdataParser` | 调试器变量树：UnLua userdata → 反射树展开（约 700 行） |

---

## 1. 四条筛选标准（用于下面每一项判定）

1. **不绑私有生态**：不依赖源项目自研 Profiler / 指标系统、外部 IDE、闭源调试 SDK、商业性能工具、云存储上传链路。
2. **不碰 Lua 内部结构**：不用 `lstate.h`、`G(L)->frealloc`、`ar->i_ci`、`gcvalue`。本仓库目标是迁 Luau 的，任何 Lua 内部布局依赖都是未来债务。
3. **本仓库没有等价物**：已有 `FDeadLoopCheck`、`FDanglingCheck`、`HotfixLua`、`CustomLoadLuaFile`、`StaticBindingRegistry::Unregister` 的，不再搬重复品。
4. **能落进现有模块**：不新开 UE 模块（唯一例外是 `UnLuaExtensions` 下的扩展插件，那本来就是插件模板）。

---

## 2. 建议纳入（5 项 P0 + 3 项 P1）

### P0-1 · Lua CPU Profiler（源自 `SrcLuaTrace`）— 替换现有 Insights Hook

**为什么值得**：本仓库现在只有一个 30 行的玩具 Hook（`Private/LuaEnv.cpp:46-76`），它有四个硬伤：
- 每次 call/ret 都 `FString::Printf` 拼串 + `FName` 构造（高频分配）
- `ar->what == FName("Lua")` 是拿 `const char*` 与 `FName` 比，语义可疑
- **没有尾调用修正**，Insights 时间轴会错乱
- 只在编译期 `ENABLE_UNREAL_INSIGHTS` 开，运行时无法开关，且开 DeadLoopCheck 时直接失效（`LuaEnv.cpp:171-174`）

源侧约 1300 行解决了全部这些：函数名缓存（解析表 + 指针哈希桶）、`OutputEventType` 预注册 + 未知 SpecId 补登记（防 SpecId 无限增长）、尾调用修正（`TailParentPtr`）、C 函数/元函数过滤、命令行运行时开关。

**落到哪里**：`Plugins/UnLuauNova/Source/UnLuauNova/Private/LuaProfiler.{h,cpp}`（Runtime 模块内新增文件，不新开模块），由 `FLuaEnv` 持有实例。

**必须改造**：

| 源侧 | 处理 |
|---|---|
| 项目私有日志头 + `MOE_LOG` 宏 | 删，换 `UE_LOG(LogUnLuauNova, ...)` |
| `HELP_TEXT(...)`（源项目内查无定义） | 换 `TEXT(...)` |
| `#if ENABLE_PERF_MONITOR`（源项目 Target.cs 宏） | 删 |
| `LUA_GC_EVENT` / `lua_set_gc_event_handler`（源项目给 Lua 打的私有补丁） | 整段删，含 `EnableLuaGCEvent/DisableLuaGCEvent` 两个导出 |
| 全局裸指针单例 | **必须改成 per-Env**：本仓库是多 `FLuaEnv`，全局单例会串 |
| `GLuaSrcFullPath` | 本仓库已有同名变量（`Private/UnLuaPrivate.h:105`），可直接用 |
| 硬编码业务脚本根路径 | 改为配置项 |

**顺带对齐 `PROJECT_PLAN.md` §4.1 的 `ILuaBackend::SetHook`**，这个 profiler 正好是那个抽象的第一个消费者。

工作量：约 1.5 天移植 + 0.5 天自测。

---

### P0-2 · 蓝图元数据 JSON 导出（源自 `SrcBlueprintExport`）

**为什么值得**：本仓库编辑器已有 **EmmyLua 注解生成**（`UnLuaIntelliSense.cpp` 20KB + Generator + Commandlet），但**没有结构化元数据导出**。而 `PROJECT_PLAN.md` §3 的 Type Manifest 主线第一句话就是「**可观测：先能看见导出面，再谈收紧**」——这份 JSON 正是"看见导出面"的输入，还能喂 §4.4 的 Luau `.d.lua` 生成。

**与内核零耦合**：三个文件只 include 公开的 `UnLuaInterface.h`，不碰 `UnLua/Private`。这是全部候选里移植风险最低的。

**落到哪里**：`Plugins/UnLuauNova/Source/UnLuauNovaEditor/Private/BlueprintExport/`（3 个文件）+ `Commandlets/UnLuaBlueprintExportCommandlet.{h,cpp}`。
`UnLuaEditor.Build.cs` 需追加 `AssetRegistry`、`ContentBrowser`、`Json`（`JsonUtilities` 可选）。

**必须改造**：
1. Commandlet 里 `ExportBlueprintData/CollectProperties/CollectFunctions/...` 是 Helper 的**重复实现**（约 380 行）→ 删掉改调 `FUnLuaBlueprintExportHelper::`，整体从约 1580 行压到约 1200 行
2. **UE5 API 适配**（引擎基线是 5.8，源是按老版本写的）：
   - `AssetData.AssetClass` → `AssetData.AssetClassPath`（`FTopLevelAssetPath`）
   - `AssetRegistry.GetAssetByObjectPath(FName)` / `GetAssetsByClass(Name, Arr, true)` → UE5 新签名
   - `AssetData.ObjectPath` → `GetSoftObjectPath()`
3. 砍掉 Python 链路：源侧「导出 JSON 后调 Python 生成 Lua 注释」依赖写死的脚本路径，与本仓库已有 IntelliSense 重复 → 连带 `ExportBlueprintLuaFile` 一起删，只保留 JSON 导出
4. 两版 JSON 字段不一致（Helper 用 `DefinesGetModuleName`，Commandlet 用 `HasLuaBinding`）→ 统一成 Helper 版

工作量：约 1 天。

---

### P0-3 · `stringy`（Lua 字符串扩展）→ `UnLuaExtensions` 新插件

242 行纯 C，MIT，零 UE / 零 UnLua 源码依赖，提供 `split/strip/startswith/endswith/count/find/join/compare`。

**落到哪里**：`Plugins/UnLuaExtensions/LuaStringy/`，**逐字照抄现有 `LuaProtobuf` / `LuaRapidjson` / `LuaSocket` 的模板**（uplugin 依赖 `UnLuauNova`、`LoadingPhase: PreLoadingScreen`、`PrivateDependencyModuleNames: UnLuauNova + Lua`、`bUseUnity=false`、关 UndefinedIdentifierWarning）。

注册就用现有委托，7 行：
```cpp
FUnLuaDelegates::OnLuaStateCreated.AddLambda([](lua_State* L) {
    luaL_requiref(L, "stringy", luaopen_stringy, 1); lua_pop(L, 1);
});
```

注意：`fastsearch.h` 无版权头（源自 CPython stringlib），需自行确认许可；`stringy.c` / `lsqlite3.c` 均无独立 LICENSE 文件，移植时补一份。

工作量：约 1 小时。

---

### P0-4 · Lua 内存统计回调（**重新设计，不照搬**）

源侧 `SrcMemStats` **明确丢弃**：它要求改 UnLua 核心分配器埋点、每次 Lua 分配都做 `lua_getstack` 全栈回溯 + MD5、依赖云对象存储上传，且模块初始化处**硬编码了云 API 密钥**（安全风险，本方案不引用其值）。

但"Lua 内存可观测"这个需求是真的。本仓库现在只有 `STAT_UnLua_Lua_Memory`（编译期 `UNLUA_STAT_MEMORY_*` 宏），**没有任何运行时回调或查询 API**。

**建议自研一个轻量版**（不搬源码，只取需求）：
- `FUnLuaDelegates` 加三个 delegate：`OnLuaMemoryAlloc` / `OnLuaMemoryFree` / `OnLuaMemoryRealloc`，**只传 size，绝不做栈回溯**
- 埋点位置唯一：`LuaEnv.cpp:741-762 DefaultLuaAllocator`（已是 `virtual GetLuaAllocator()`，天然可插拔）
- 加一个控制台命令 `lua.memstats`，输出当前/峰值/分配次数
- 默认全关，仅 STATS 或 `-luamemstats` 开启

工作量：约 0.5 天。

---

### P0-5 · `SkipObjectReferencer`（UMG ListView 泄漏修复）

源侧给 `IUnLuaInterface` 加的 `BlueprintNativeEvent`，解决 ListView EntryWidget 已销毁但仍被 UnLua 的 `AutoObjectReference` 持有、无法 GC 的问题。

本仓库 `Public/UnLuaInterface.h` **只有 `GetModuleName()` 一个事件**，`UUnLuaManager` 也没有任何按 World 分级清理或跳过引用的机制。这是 UE 项目的高频坑，且成本极低。

**落到哪里**：`Public/UnLuaInterface.h` 加一个 `BlueprintNativeEvent`；`UUnLuaManager::Bind` 判断后跳过 `AutoObjectReference`。约 40 行。

> 配套的 `IgnoreAutoCleanup` + Editor 下的对象分类校验**暂不搬**——那套是 Editor 下的强校验逻辑，与源项目的 World 管理约定绑得较紧，建议先只取 `SkipObjectReferencer`，观察一段时间再决定。

工作量：约 2 小时。

---

### P1-1 · 静态导出反注册（`DeportClass/DeportFunction/DeportEnum`）

已核实本仓库确实有这个洞：`Private/Binding.cpp` 的 `FExported` **只增不减**（无 Remove/Unregister），而 `UnLuaEx.h` 里导出 helper 的析构只 `delete ExportedClass`，**指针仍残留在 Map 里** → 模块卸载/热重载后 `FindExportedClass` 可能返回悬空指针。

本仓库新增的 `StaticBindingRegistry` **已有** `Unregister`/`Clear`，说明这是同一类问题的新架构解法，照此给 `FExported` 补一组 Remove 即可。

**落到哪里**：`Private/Binding.cpp` + `Public/Binding.h` + `Public/UnLuaEx.h`（析构里调用）。约 40 行。

工作量：约 2 小时（含验证 2.3.6 是否真复现）。

---

### P1-2 · Android 沙盒文件时间戳兜底

源侧 `SrcLuaFramework` 中：Android 下 `IFileManager::GetTimeStamp` 读不到 `ProjectPersistentDownloadDir` 里的热更文件，用 `stat()` + epoch 手工补。这是真机热更判断文件新旧时的实际坑。

本仓库 `LuaEnv.cpp:601-646 LoadFromFileSystem` **已有 DownloadDir 优先**（所以沙盒加载主逻辑不用搬），但**没有时间戳兜底**。

**落到哪里**：`Private/BaseLib/LuaLib_File.cpp`，暴露 `UE.GetFileTimeStamp(Path)`。约 40 行（含显式 `#include <sys/stat.h>`）。

> 同一批的 `UELoadLuaFile` / `UELoadRawFile` / `UELoadEncryptFile` **不搬**：它们是在 Lua 侧再造一套加载路径，本仓库已经有官方钩子 `FUnLuaDelegates::CustomLoadLuaFile`（4 参，带 `FLuaEnv&`），正确性更好。只需在文档里说明用法。
> `UELoadEncryptFile` 的简易加解密**内置硬编码默认密钥**，本来就不该进开源仓库。

工作量：约 2 小时。

---

### P1-3 · `lsqlite3`（待定）

约 2450 行上游代码（MIT），给 Lua 提供 SQLite3 绑定（存档 / 本地库）。**阻塞项**：本仓库完全没有 sqlite3，需先引入引擎 `SQLite`/`SQLiteCore` 模块并配 include path；且与 Luau 的 `luaL_Reg` / `luaL_requiref` 行为未经验证（上游代码有 5.1→5.3 的 shim，Luau 是 5.1 基线，理论可行但需实测）。

建议：**等真有需求再做**，不与本次一起合。

---

## 3. 明确丢弃（附理由，避免以后重复调研）

| 源侧代号 | 规模 | 丢弃理由 |
|---|---|---|
| `SrcRemoteDebugger` | **约 6500 行** | 依赖闭源调试 SDK（还会拖入 Protobuf / libcurl / OpenSSL）；5600 行 Slate UI 是全部候选中维护负担最重；断点握手走 **Windows-only 命名共享内存**；菜单挂点依赖源项目里另一个编辑器插件。两个子模块共用同一 SDK，要么全留要么全删，不可拆分 |
| `SrcMemStats` | 约 270 行（插件 2600 行） | 必须改核心分配器 + 每次分配全栈回溯 + 云上传链路 + **硬编码云密钥** |
| 死循环看门狗 | 约 295 行 | 本仓库已有 `FDeadLoopCheck` + `FDanglingCheck`；且源侧用 `LUA_MASKLINE`（每执行一行回调一次，5~20x 减速） |
| `lua_State` 地址导出 | 约 50 行 | 只在进程内静态变量存了个指针 + 打日志，外部进程根本拿不到，等于空模块 |
| `SrcSampleProfiler` | 约 6000 行 | 直接读 Lua 内部 `G(L)->frealloc`、`ar->i_ci->func` —— **与迁 Luau 的方向正面冲突**；且功能被 P0-1 覆盖 |
| `SrcManualProfilerMark` | 约 330 行 | 绑死源项目自研 Profiler 与指标采集器；剥完后剩下的功能被 P0-1 完全覆盖 |
| `SrcIdeLauncher` | 约 24 KB | 外部 IDE 可执行文件路径 / adb / 固定端口 / 手写 bat，纯私有生态 |
| `SrcUserdataParser` | 约 700 行 | 依赖 UnLua `Private` 内部头 + 闭源调试库的私有变量协议。**思路值得留档**：若将来做 Lua 调试器变量视图，应在 UnLua 侧定义中立的 `FLuaDebugVariable` 公开 API，而不是搬这个文件 |
| 移动端性能指标上报 | 约 3200 行 | 仅 Android/iOS，依赖源项目多个业务模块；启动还会**阻塞轮询最多 10 秒** |
| `SrcLuaFramework` 的环境表 / 序列化库 / 加解密 / RPC 装配 | 约 330 KB | 属于源项目技术栈选型，不是 UnLua 能力；其中加解密含硬编码密钥 |
| 源侧 `Source/UnLua/` 核心 fork 全部 | — | 2.1 老架构回退，且 `UnLuaCompatibility.h` 在 UE5.0/5.1 编译失败。**只参考其中的 bugfix 思路** |
| 多搜索路径 / Lua 环境锁 / 两个私有崩溃兜底开关 | — | 本仓库已用 `PackagePath`（分号分隔）+ DownloadDir 优先覆盖；`FLuaEnv` 多 Env 已隔离；两个开关是源项目私有崩溃兜底 |

---

## 4. 落点总表（回答"什么东西放什么位置"）

| 能力 | 落点位置 | 新增/修改 | 模块归属 |
|---|---|---|---|
| Lua CPU Profiler | `UnLuauNova/Private/LuaProfiler.{h,cpp}` | 新增 2 文件 + 改 `LuaEnv.cpp` | Runtime（现有模块） |
| 蓝图 JSON 导出 | `UnLuauNovaEditor/Private/BlueprintExport/*` + `Commandlets/UnLuaBlueprintExportCommandlet.*` | 新增 5 文件 + 改 `UnLuaEditorCommands` / `UnLuaEditorModule` / `UnLuaEditor.Build.cs` | Editor（现有模块） |
| `stringy` | `Plugins/UnLuaExtensions/LuaStringy/` | **新增 1 个扩展插件** | 扩展插件（照模板） |
| Lua 内存回调 | `UnLuaDelegates.h` + `LuaEnv.cpp` + `UnLuaConsoleCommands.cpp` | 改 3 文件 | Runtime（现有模块） |
| `SkipObjectReferencer` | `UnLuaInterface.h` + `UnLuaManager.cpp` | 改 2 文件 | Runtime（现有模块） |
| 导出反注册 | `Binding.{h,cpp}` + `UnLuaEx.h` | 改 3 文件 | Runtime（现有模块） |
| Android 时间戳兜底 | `BaseLib/LuaLib_File.cpp` | 改 1 文件 | Runtime（现有模块） |

**净结果：源侧 15 个 uplugin 模块 + 8 个插件，收敛成 1 个新文件目录（BlueprintExport）+ 1 个新扩展插件（LuaStringy）+ 6 处现有文件改动。不新增任何 UE 模块。**

---

## 5. 建议执行顺序

```
第 1 批（低风险、独立、可立即验证）
  ├─ P0-3 stringy              ~1h
  ├─ P0-5 SkipObjectReferencer  ~2h
  └─ P1-1 导出反注册            ~2h

第 2 批（核心收益）
  ├─ P0-2 蓝图 JSON 导出        ~1d   ← 与 Type Manifest 主线同向，建议优先
  └─ P0-1 Lua CPU Profiler      ~2d   ← 替换 LuaEnv.cpp 现有 Hook，需自测

第 3 批（可选）
  ├─ P0-4 Lua 内存回调          ~0.5d
  └─ P1-2 Android 时间戳兜底     ~2h
```

**前置依赖**：
- 每批都要先跑通 `UnLuaTestSuite`（Session Frontend → Automation，需编辑器环境）
- 第 2 批必须在**改名后首次编译通过**之后做。当前 `HEAD` 的改名（`ed1aea4c`）和 Luau 引入都未编译验证过，先确保基线能编，否则无法区分"移植引入的错"和"改名引入的错"

**每批都建独立分支**，格式 `port/external-<name>`，与 `pr/NNN` 的吸纳分支区分开，便于单独回滚。

---

## 6. 待确认的三个决策点

1. **蓝图 JSON 导出的定位**：只做"导出面观测工具"（纯 JSON，喂 Type Manifest），还是也要做成"Lua 注释生成"（那就与现有 IntelliSense 重复，建议砍掉）？
2. **`stringy` 要不要顺带把 `lsqlite3` 一起做**：单独做 lsqlite 要先引入引擎 SQLite 模块，成本跳一个量级。
3. **移植后的 Lua 侧模块名**：源项目的 profiler 注册名带项目前缀。作为开源项目建议改成 `UnLua.*` 命名空间下的名字（这是本仓库保留的兼容命名空间）。这决定了是否与源项目的 Lua 业务脚本保持兼容。

---

## 附：开源化脱敏检查清单

本仓库在对外开源前，需确认以下几类信息不出现在任何提交中：

- [ ] 源项目名、内部插件名（两个私有插件目录名）、外部 IDE / 调试 SDK 名 —— **本文已脱敏**
- [ ] 任何指向源项目或本地引擎的绝对路径
- [ ] 云 API 密钥 / SecretId / SecretKey（源侧某模块初始化处有硬编码，**本方案未引用其值**）
- [ ] 加解密默认密钥（源侧简易加密模块内硬编码，**本方案未引用其值**）
- [ ] 内部员工账号名（源侧部分代码注释含作者署名）
- [ ] `.codebuddy/` 下的规划文档是否随仓库分发——若否，建议加入 `.gitignore`

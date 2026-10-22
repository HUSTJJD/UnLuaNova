# UnLuaNova 测试与文档盘点（覆盖度 / 缺口 / 补充建议）

> 盘点对象：`Plugins/UnLuaNova`、`Plugins/UnLuaNovaTestSuite`、`Docs/` + 两份 README
> 结论先行：**量已经不小，但"分布"和"反馈回路"是短板** —— 覆盖高度集中在数学类型/容器，核心生命周期与覆写链几乎空白；文档页面齐全但存在"文档写了、代码没有"的漂移；且**测试代码不在 CI 编译**，这正是本次 `bIsLatent` 编译错误潜伏到一次 23 分钟全量构建才暴露的根因。

---

## 一、现状数字

| 项 | 现状 |
| --- | --- |
| C++ 规约测试（Spec） | 37 个 `*.spec.cpp`，约 369 个 `It()` |
| 回归测试（Private/Tests） | 约 64 个：60 个 `Issue###Test.cpp`（Issue276…Issue668）+ `BindingTest` + `OutParamTest` + `ShowcaseTest` |
| Lua 侧测试 | **0**（`Content/Script/Tests/**` 46 个 lua 只是 C++ 用例的夹具，内部无断言；无 `describe/it` 框架） |
| 执行方式 | UE Automation，`Tools/RunTests.ps1`（`-Backend Lua/Luau/Both`、`-Publish`、`-NullRHI`、`-NoVideo`、`-SkipBuild`、`-FailOnTypeErrors`…） |
| 静态检查 | `Tools/CheckLuauTypes.ps1`（luau-analyze + 生成的 `UnLuaNova.d.lua`），默认只记录不失败 |
| CI | `.github/workflows/ci.yml` 只做：Python/PowerShell 语法检查 + `mkdocs build`；**不编译、不跑测试、无覆盖率/性能基线** |
| 文档 | `Docs/CN` 19 页 + `Docs/EN` 19 页 + `Docs/index.md` + 2 份自动生成报告；`mkdocs.yml` 导航完整 |

---

## 二、覆盖热力图

### 🟢 强（可作为样板）
- 数学类型：`FVector/FVector2D/FVector4/FQuat/FRotator/FTransform/FColor/FLinearColor/FIntPoint/FIntVector`，每个 10~15 个 `It`（约 100+）
- 容器：`TArray`(37) / `TMap`(20) / `TSet`(16)，含 `pairs`、ToTable、增删改查
- 后端抽象：`LuaBackend.spec.cpp`(40)，能力宏 `UNLUANOVA_HAS_*`、`GetBackend()/HasFeature()`、sandbox、coroutine、Profiler
- 基础调用：`UnLuaNovaBase.spec.cpp`(31)（Push / RunChunk / Call / by-ref 出参）
- 扩展插件：`Extensions.spec.cpp`（rapidjson / protobuf / socket）

### 🟡 弱（有入口，点到为止）
| 子系统 | 现状 | 问题 |
| --- | --- | --- |
| `FLuaEnv`（34KB，插件最核心） | `LuaEnv.spec.cpp` **仅 2 个 It** | 多 env 隔离、`DoString` 失败路径、`GC()`、`AddLoader/AddBuiltInLoader` 搜索链、`TryBind/TryReplaceInputs`、`OnCreated/OnDestroyed`、`GetAll()/FindEnv()` 全无断言 |
| HotReload | `LuaBackend.spec.cpp:276` 只验 `require('UnLuaNova.HotReload')` 能加载 | C++ 侧 `FLuaEnv::HotReload()` / `UUnLuaNovaFunctionLibrary::HotReload` 未被驱动 |
| Sandbox | 有 `UNLUANOVA_SANDBOX` / `RestoreTablesAfterSandbox` 用例 | 未验"保护后引擎类型仍能惰性注册"的真实回归路径 |
| 控制台命令（9KB） | `EditorCommands.spec.cpp` 只验命令存在 | `lua.do / profiler / memstats / dbg / sandbox` 的**行为**未测 |
| 设置项 | 仅 `LuaDeadLoopCheck` 1 个 It；Issue346/517 只是"改设置" | 多数设置项改了之后**没有断言生效结果** |
| Locator | Issue346 只切换 `ModuleLocatorClass` | `ULuaModuleLocator_ByPackage` / `ULuaEnvLocator` 解析规则无独立用例 |
| 静态绑定生成器 | 1 个 It | 幂等性、opt-out 类型、失败路径未测 |
| 静态绑定分发 | `CallUEFast.spec.cpp` 4 个 It | 只测正向分支，`bOverridable` 必须回退反射路径这一分支无用例 |

### 🔴 空白（源码存在，零用例）
- `LuaDanglingCheck.cpp`：悬垂检测全部逻辑未驱动（Issue517 仅置 `DanglingCheck = true`）
- `LuaOverrides.cpp` / `LuaOverridesClass.cpp` / `LuaFunction.cpp`：**覆写是最有价值也最脆弱的机制**，`Override / RestoreOverrides / SuspendOverrides / ResumeOverrides / IsOverridable / execCallLua` 全无直接用例
- `UnLuaNovaDebugBase.cpp`（27.9KB）：断点/栈变量/调用栈，零用例
- `UnLuaNovaManager.cpp`、`UnLuaNovaModule.cpp`、`UnLuaNovaDelegates.cpp`、`UnLuaNovaLatentAction.cpp`；尤其 `LuaCore.cpp:1230/1241` 两条 latent 错误消息**写在代码里、没有用例固定**
- `UnLuaNovaIntelliSense.cpp`(20KB) / `IntelliSenseGenerator` / `LuauDecls`：编辑器侧最大一块，零用例
- `UnLuaNovaDefaultParamCollector.cpp`(26KB)：仅 4 个 It
- `LowLevel.cpp`、`LuaValue.cpp`、`CollisionHelper.cpp`
- 错误处理/栈平衡：语法错误、运行时错误、错误消息与调用栈、`pcall` 边界、出错后后续调用是否仍正常 —— 无系统性用例
- 性能：`Private/Perfs/*` **注册 0 个自动化用例**，只手工跑 umap 出 CSV，无回归基线

### 结构性缺口（比"少几个用例"更要紧）
1. **没有 Lua 侧测试**：所有断言都在 C++，随插件发布的 Lua 库（`UnLuaNova.HotReload`、`StringEx`、`Vector/Buffer`）纯 Lua 逻辑没有自测手段
2. **测试不在 CI 编译** → 本次 `Flags.bIsLatent` / `LoadModulePtr` 这类"测试自身编译不过"只能靠本地全量构建发现。**当前风险最高的一条**
3. **无"用例数护栏"**：`UNLUANOVA.API.Base`、`UNLUANOVA.API.Enum` 大小写与约定 `UnLuaNova.<域>.<主题>` 不符，`-Filter UnLuaNova` 可能悄悄漏跑且报告不报警
4. **无覆盖率与性能基线**

---

## 三、文档盘点

### 优点
- 结构完整：CN/EN 各 19 页一一对应，均在 `mkdocs.yml` 导航内
- `Docs/CN/Testing.md`(16KB) 相当细：一键运行、参数、双后端报告、崩溃也出报告、截图录屏、为何不在 GitHub Actions 跑、写新用例约定
- 有 2 份自动生成报告页；上游 UnLua CHANGELOG 独立归档

### 🔴 漂移（文档有、代码没有 —— 直接误导用户）
- `Docs/CN/API.md`、`Docs/EN/API.md` 仍介绍 **`FLuaContext` / `FLuaContext::GLuaCxt`**，全库 0 命中
- `Docs/EN/API.md` 仍列 `SUPPORTS_RPC_CALL`、`SUPPORTS_COMMANDLET`（已删）；宏名写成 `AUTO_UnLuaNova_STARTUP` / `UnLuaNova_ENABLE_DEBUG`，与代码 `AUTO_UNLUA_STARTUP` / `UNLUANOVA_ENABLE_DEBUG` 不一致
- `Docs/index.md` 写「许可 MIT」，而 README 与 `Docs/*/License.md` 是 **Polyform Small Business 1.0.0**；`mkdocs.yml:135` copyright 仍为 MIT
- `Docs/CN|EN/Features.md` 写「UE 4.17.x – 5.x」（上游遗留），与 README「5.8.3 基线」矛盾
- `README.md:109` / `README_EN.md:109` **UTF-8 乱码**；`README.md:138` 跑测试命令被**截断**

### 🟡 缺失（代码有、文档没有）
- 公开 C++ API 大面积未文档化：`FLuaEnv` 整个类、`IUnLuaNovaModule`、`ULuaEnvLocator`/`ULuaModuleLocator`、`UUnLuaNovaFunctionLibrary`、`RestoreTablesAfterSandbox`、`Binding.h` 导出注册接口（含新增 `UnExportClass`）、`FStaticBindingRegistry` + `UNLUANOVA_REGISTER_STATIC_BINDING`（Docs 0 命中）、`UnLuaNova::LowLevel::*`、`ULuaFunction`/`ULuaOverridesClass`、`FLuaDebugValue`/`FLuaVariable`
- **没有架构/贡献者文档**：双后端抽象、LuaCore 绑定与覆写（thunk + opcode 注入）、静态绑定生成链路、"如何新增一个 LuaLib 类型"；`CONTRIBUTING.md` 仅 1.7KB
- `Tools/Rename-To-UnLuaNova.ps1` 未在任何文档提及
- CHANGELOG 提到但 Docs 无说明：`UE.__newindex` 行为、`Saved/RunTests.lock` 单实例锁
- `CHANGELOG.md:16` 笔误 `UnLuaStaticBindingGenerator`（真实命令 `UnLuaNovaStaticBindingGenerator`）

### 🟡 中英漂移
| 页 | CN | EN | 差异 |
| --- | --- | --- | --- |
| API.md | 8.8KB | 5.9KB | EN 缺 `GetBackend/HasFeature`、字符串扩展、运行时 Lua 模块、多数宏，且含已删宏 |
| Settings.md | 7.8KB | 5.1KB | CN 独有 5 节（返回值顺序/蓝图路径后缀/BOM/传参方式/检查新版本） |
| FAQ / CustomTemplate / Programming_Guide | — | — | 30%~50% 体量差 |
| Testing.md | 16.3KB | 17.4KB | EN 更新（有 `-FailOnTypeErrors`、`luau-analyze.json`），**CN 反而落后** |
| EN 页内链 | — | — | 多处链到 `../CN/*.md`（EN 已有同名页） |
| 测试报告 | — | — | 两份中文报告同时挂在 CN/EN 导航下 |

---

## 四、建议补充清单（按优先级）

### P0 — 先补"反馈回路"与"致命漂移"（1~2 天）
1. **让测试代码至少能被编译**：自托管 Windows runner + UE 5.8.3 源码，nightly 跑 `Tools/RunTests.ps1 -Backend Both`；退而求其次：nightly 只编译 Editor + TestSuite（`RunTests.ps1` 增 `-BuildOnly`）
2. **加"用例数护栏"**：`Tools/GenerateTestReport.py` 输出 `summary.json` 后与上次基线对比，用例总数下降 / 新增 skip / 失败数非 0 → 非 0 退出；顺手统一 `UNLUANOVA.API.Base|Enum` → `UnLuaNova.API.Base|Enum`（同步文档 Filter 示例）
3. **修文档漂移**：删 `FLuaContext` 与已删宏、统一宏名大小写、`Docs/index.md` 许可改 Polyform、`Features.md` UE 版本口径、README 乱码与截断、EN 页跨语言链接
4. **`Docs/CN/Testing.md` 补用例清单表**（37 个 spec：注册名 / 覆盖域 / 是否需要视口 / 后端相关性），并补上 CN 缺失的 `-FailOnTypeErrors` 与 `luau-analyze.json`

### P1 — 补核心能力用例（3~5 天，收益最高）
| 新文件 / 扩容 | 建议用例 |
| --- | --- |
| `LuaEnv.spec.cpp`（2 → ~15） | 多 env 隔离；`Start` 启动模块名错误不静默；`DoString` 语法/运行时错误返回 false 且不脏栈；`GC()`；`AddLoader/AddBuiltInLoader/AddSearcher` 优先级；`HotReload()`；`OnCreated/OnDestroyed` 顺序；`GetAll()/FindEnv()` |
| `Overrides.spec.cpp`（新，~12） | `Override` 后 Lua 实现被调用；`RestoreOverrides` 回到蓝图实现；`Suspend/ResumeOverrides`；`IsOverridable` 对非 BlueprintEvent 返回 false；覆写函数报错时引擎不崩且可恢复；覆写 + 静态绑定 `bOverridable` 必须走反射路径 |
| `Lifetime.spec.cpp`（新，~10） | `LuaDanglingCheck` 命中时报告而非崩；UObject 被 GC 后 userdata 标记 released 且访问受控报错；Lua 持有引用时不被 GC；`OnDestroyed` 后再访问 |
| `ErrorHandling.spec.cpp`（新，~10） | Lua 语法/运行时错误消息与栈内容；`pcall` 边界；出错后同 env 继续调用仍正确（栈平衡）；traceback 含 Lua 文件名行号 |
| `Latent.spec.cpp`（新，~6） | 协程内 latent 正常挂起恢复；**主线程调用报 "Can't call latent action in main lua thread"**；**从 C 回调进入的线程报 yield 受限**（固化 `LuaCore.cpp:1230/1241`）；世界暂停后 delay（并入 Issue276） |
| `Locators.spec.cpp`（新，~6） | `ULuaModuleLocator` 默认规则；`_ByPackage` 包名→模块名映射；换 `ULuaEnvLocator` 子类后定位到指定 env |
| `ConsoleCommands.spec.cpp`（新，~8） | `lua.do`（含脚本错误）、`lua.profiler`、`lua.memstats`、`lua.dbg`、`lua.sandbox` 的行为断言 |
| `Settings.spec.cpp`（新，~8） | 主要设置项"改了之后行为真的变了" |
| `StaticBindingGenerator.spec.cpp`（1 → ~6） | 生成内容含预期类型；opt-out 类型被跳过；重复生成幂等；输出目录不存在时报错而非崩 |
| `IntelliSense.spec.cpp`（新，~5） | 生成的 `UnLuaNova.d.lua` 含指定 UCLASS/UFUNCTION；重命名后重新生成生效 |

### P2 — 补结构（2~4 天）
1. **Lua 侧自测能力**：加极简断言模块（如 `Content/Script/Tests/Lua/Spec.lua` 收集结果），C++ 侧加一个 driver spec 执行 `Tests/Lua/**/*.lua` 并读结果 → `StringEx`、`HotReload`、`Vector/Buffer` 等纯 Lua 逻辑可自测，不再为每个断言写 C++ 样板
2. **性能基线**：`Private/Perfs` benchmark 接成自动化用例写 CSV，`GenerateTestReport.py` 做环比（阈值放宽，只告警显著劣化）
3. **容器增强**：容器内放 UObject/struct 的生命周期；`TSet pairs`（新宏 `UNLUANOVA_ITER_ENTRY(TSet_Pairs)`）**迭代中增删**；嵌套容器
4. **文档新增页**：`Docs/CN+EN/Architecture.md`（双后端抽象 / 绑定与覆写 / 静态绑定生成 / 如何新增 LuaLib 类型）、`Docs/CN+EN/CxxApi.md`（补上述未文档化 API）
5. **收敛中英漂移**：先对齐 `API.md`、`Settings.md`；测试报告要么双语、要么只挂中文导航；补占位报告以重新启用 `mkdocs build --strict`

---

## 五、不做什么（避免无效投入）
- 不再给数学类型加用例（已 100+ `It`，边际收益极低）
- 不引入第三方测试框架（UE Automation 已够用，`RunTests.ps1` 的报告/截图/录屏链路已成熟）
- 不为 vendored 的 `ThirdParty/LuaLib/testes`、`Luau/bench` 接 runner（上游资产，与插件行为无关）

---

## 六、建议落地顺序
1. P0-1/P0-2（CI 编译 + 用例数护栏）→ 立刻降低"测试自己烂掉"的风险
2. P0-3/P0-4（文档漂移 + 用例清单）→ 半天，纯收益
3. P1 的 `Overrides` / `Lifetime` / `ErrorHandling` / `LuaEnv` 四个 spec → 覆盖插件最脆、最难手测的部分
4. P2 的 Lua 侧自测 + 性能基线 → 结构补齐
5. P2 文档：`Architecture.md` + `CxxApi.md`，最后收中英漂移

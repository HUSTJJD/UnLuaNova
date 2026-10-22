# UnLuaNova 审计修复 Task 清单

> 依据 `.codebuddy/TEST_DOC_AUDIT.md`。每条任务给出：**目标 / 涉及文件（精确到行）/ 具体步骤 / 验收标准 / 工作量**。
> 约定：所有路径相对仓库根 `d:/learning/UnLua`；新 spec 一律放在
> `Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private/Specs/`，注册名 `UnLuaNova.<域>.<主题>`，
> 骨架照抄 `Private/Specs/LuaEnv.spec.cpp`（`BEGIN_DEFINE_SPEC` + `BeforeEach/AfterEach` 里 `Env.Reset()` + `It(..., EAsyncExecution::TaskGraphMainThread, ...)`）。
> 断言用 `UnLuaNovaTestHelpers.h:265-278` 的 `TEST_TRUE / TEST_FALSE / TEST_EQUAL / TEST_QUAT_EQUAL`。

---

## 阶段 P0：反馈回路（先把"测试自己烂掉"堵住）

### T1 · 让测试代码每天至少被编译一次
- **目标**：本次 `Flags.bIsLatent` / `LoadModulePtr` 这类"测试代码编译不过"的问题，当天被发现，而不是等一次 23 分钟的人工全量构建。
- **涉及文件**：新增 `.github/workflows/nightly-ue.yml`（或本地计划任务，见降级方案）
- **步骤**
  1. 装一台 Windows 自托管 runner：装好 UE 5.8.3 源码构建 + 本仓库（`git submodule update --init --recursive`），注册时打标签 `self-hosted / Windows / UE`。
  2. 新增 workflow：`on: schedule: - cron: '0 18 * * *'` + `workflow_dispatch`；`runs-on: [self-hosted, Windows, UE]`。
  3. 步骤：`actions/checkout@v4`（`submodules: recursive`）→ `pwsh: .\Tools\RunTests.ps1 -Backend Both -NoVideo -NoScreenshots`（**不要**加 `-Publish`，发布报告是人工动作）→ `actions/upload-artifact@v4` 上传 `Saved/TestReports/**` 与 `Saved/Logs/**`（`if: always()`）。
  4. `RunTests.ps1` 已保证失败非 0 退出（`Tools/RunTests.ps1:632-641` 编译失败、`728` 失败用例、`733` 崩溃），所以 job 会红；再加一步 `if: failure()` 的 `actions/github-script` 自动开 issue（标题带 `[nightly]`）。
- **降级方案（暂无自托管机器）**：只做编译 —— 先完成 T2，再用 Windows「任务计划程序」每晚跑
  `pwsh -File D:\learning\UnLua\Tools\RunTests.ps1 -Backend Luau -BuildOnly -NoReport > D:\learning\UnLua\Saved\nightly-build.log 2>&1`，
  失败时由计划任务的「发邮件」或人工看日志兜底。编译已能在 1 天内抓到同类问题。
- **验收**：故意在 `Private/Specs/` 里写一行 `Func->Flags.bIsLatent;` → nightly（或计划任务）失败并给出编译错误行。
- **工作量**：0.5–1 天（不含机器准备）

### T2 · `Tools/RunTests.ps1` 增加 `-BuildOnly`
- **目标**：给"只想编译不想跑"的场景一个官方开关（T1 降级方案、日常改 spec 后的快速自检都要用）。
- **涉及文件**：`Tools/RunTests.ps1`
- **步骤**
  1. param 块（`RunTests.ps1:118-137`）加一行 `[switch] $BuildOnly`（放在 `-SkipBuild` 旁边）。
  2. 参数校验：`if ($BuildOnly -and $SkipBuild) { throw "-BuildOnly 与 -SkipBuild 互斥。" }`（与 `:601-608` 的既有校验风格一致）。
  3. 主循环：在 `Assert-BackendBuilt` 之后、`$RunDir` 之前（`RunTests.ps1:646-648` 之间）插入
     ```powershell
     if ($BuildOnly) {
         Write-Host "BuildOnly: 编译通过，跳过 $B 后端的测试运行。"
         continue
     }
     ```
  4. `-DryRun` 分支（`:570-582`）也要打印一行 `Build  : ...` 即可，已有，无需改。
- **验收**：`-BuildOnly -Backend Luau` 编完退出 0 且不启动编辑器；编译失败退出 1；`-BuildOnly -SkipBuild` 直接 throw。
- **工作量**：0.5 小时

### T3 · 用例数 / 耗时基线护栏
- **目标**：用例被改名、被 filter 漏掉、被误删时**报告会说话**；不再出现"看起来全绿其实少跑了一半"。
- **涉及文件**：`Tools/GenerateTestReport.py`（summary 段 `:800-823`、main `:695-719`）、新增 `Tools/baseline/{lua,luau}.json`、新增 `.github/workflows/ci.yml` 里可选一步
- **步骤**
  1. `summary` 字典增加 `"names": sorted(t["name"] for t in tests)`（用例全名列表；如担心体积，可单独写 `tests.json`）。
  2. `main()` 增加 `--baseline PATH` 与 `--update-baseline`：
     - 有 `--baseline` 且文件存在 → 比对 `total` / `names`（消失的、新增的）/ `duration`（本次 vs 上次，超过 ±50% 提示）
     - 打印 `BASELINE: -2 用例消失: UnLuaNova.API.Xxx..., +1 新增: ...`；`--fail-on-drift` 时非 0 退出
     - `--update-baseline` 把本次 `total/failed/skipped/duration/names` 写入 `Tools/baseline/<backend>.json`
  3. `Tools/RunTests.ps1` 在生成报告后（`:713-720` 附近）追加 `--baseline Tools/baseline/$($B.ToLower()).json`；新增 `[switch] $UpdateBaseline`（人工确认后跑一次提交进仓库）。
  4. 首次基线：本地跑一次 `-Backend Both -UpdateBaseline`，把两个 json 提交。
- **验收**：删掉某个 `It` 后重跑 → 输出"N 个用例消失"；加 `-FailOnDrift`（映射到 `--fail-on-drift`）时脚本非 0 退出；默认只 warn 不 fail（防止 CI 因合理的用例调整变红）。
- **工作量**：0.5–1 天

### T4 · 统一测试命名大小写
- **目标**：`UNLUANOVA.API.Base` / `UNLUANOVA.API.Enum` 与约定的 `UnLuaNova.<域>.<主题>` 不一致，存在被 `-Filter` 漏掉的风险。
- **涉及文件**：`Private/Specs/UnLuaNovaBase.spec.cpp:22`、`Private/Specs/UnLuaNovaEnum.spec.cpp:22`
- **步骤**
  1. `"UNLUANOVA.API.Base"` → `"UnLuaNova.API.Base"`；`"UNLUANOVA.API.Enum"` → `"UnLuaNova.API.Enum"`。
  2. 类名 `FUNLUANOVABaseSpec` → `FUnLuaNovaBaseSpec`、`FUNLUANOVAEnumSpec` → `FUnLuaNovaEnumSpec`（`BEGIN_DEFINE_SPEC` 与 `END_DEFINE_SPEC`、两个 `::Define()` 都要同步改）。
  3. **先实测大小写敏感性**（我这边没有引擎源码，无法确认）：分别跑 `-Filter UNLUANOVA` 与 `-Filter UnLuaNova`，比较两次 `summary.json` 的 `total`。若两者相同说明过滤大小写不敏感，本任务降级为"纯规范统一"，仍建议做（可读性与未来 `Automation List` 输出一致）。
  4. 在 `Docs/CN|EN/Testing.md` 的「写新用例的约定」章节补一句：注册名一律 `UnLuaNova.<域>.<主题>`，不要全大写。
- **验收**：`-Filter UnLuaNova` 的报告里能同时看到 `Base` 与 `Enum` 两组用例；全仓 `grep -n "UNLUANOVA\.API"` 只剩 0 命中。
- **工作量**：0.5 小时（含一次双 filter 实测）

### T5 · `Testing.md` 补用例清单表 + 补 CN 落后内容
- **目标**：文档现在只给了一个 `-Filter` 示例，30+ 个 spec 没有清单，读者无法知道"哪些域有测试、哪些没有、哪些要视口"。
- **涉及文件**：`Docs/CN/Testing.md`、`Docs/EN/Testing.md`
- **步骤**
  1. 在「一键运行」之后新增「## 用例清单」章节：三列表格 —— `注册名 | 覆盖什么 | 备注（需视口/仅编辑器/后端相关）`。37 行由 `Private/Specs/*.spec.cpp` 的 `BEGIN_DEFINE_SPEC` 第二参数生成，逐个对照补齐（不要照抄我这里的摘要，以代码为准）。
  2. CN 补两处 EN 有而 CN 没有的内容：`-FailOnTypeErrors`（见 `Docs/EN/Testing.md:84` 附近的参数表）与 `luau-analyze.json` 产物行（`Docs/EN/Testing.md:222` 附近）。
  3. 两页结构对齐后，把 T4 的命名约定写进「写新用例的约定」（`Docs/CN/Testing.md:301`）。
- **验收**：CN/EN 该章节逐条一致；表内 37 个注册名与代码一一对应（T3 的基线 json 可作为对照源）。
- **工作量**：0.5 天

---

## 阶段 P1：文档漂移（半天 ~ 1 天，纯收益）

> 做完这条，P0/P1 合计约 1.5 天，建议一次性提交为一个 PR。

### T6 · 删掉"幽灵 API"（文档有、代码没有）
- **涉及文件**：`Docs/CN/API.md:15,29,31,55-61,77`、`Docs/EN/API.md:15,29,31,77,133,137,139,143`
- **步骤**
  1. `FLuaContext::GLuaCxt`（两页 `:15`）→ 改为 `UnLuaNova::FLuaEnv`（多环境）与 `IUnLuaNovaModule::Get().GetEnv()`（主环境），并指向新的 T20 文档页（若尚未建，先写一句话说明）。
  2. `CreateState` / `GetState`（EN `:29,31`）里 "created in FLuaContext class" / "by CreateState" 改为 `FLuaEnv::GetMainState()`。
  3. 委托段落（CN `:55-61`）：`OnLuaContextInitialized`、`OnPreLuaContextCleanup`、`OnPostLuaContextCleanup`、`OnPreStaticallyExport` 这 4 个委托若代码里已删，直接删；若仍在，把描述里的 `FLuaContext` 换成真实类型。**先 `grep -rn "OnLuaContextInitialized" Plugins/` 确认存在性再决定删/改**。
  4. 类段落（两页 `:77`）`FLuaContext` → `FLuaEnv`，一句话说明其职责并注明"多环境各自一个实例"。
  5. EN `:133` `AUTO_UnLuaNova_STARTUP` → `AUTO_UNLUA_STARTUP`；EN `:143` `UnLuaNova_ENABLE_DEBUG` → `UNLUANOVA_ENABLE_DEBUG`（与 CN `:201` 一致）；EN `:137,139` 直接删除 `SUPPORTS_RPC_CALL`、`SUPPORTS_COMMANDLET`（已在 `Docs/Archive/UnLua_CHANGELOG.md:45` 记录为 Removed）。
- **验收**：`grep -rn "FLuaContext\|SUPPORTS_RPC_CALL\|SUPPORTS_COMMANDLET\|AUTO_UnLuaNova_STARTUP\|UnLuaNova_ENABLE_DEBUG" Docs/ ` 只剩 `Docs/Archive/` 命中。
- **工作量**：1 小时

### T7 · 许可口径统一（MIT → Polyform）
- **涉及文件**：`Docs/index.md:131`、`mkdocs.yml:135`（copyright）、核对 `Docs/index.md:112-114`
- **步骤**：`| 许可 | MIT |` → `| 许可 | Polyform Small Business 1.0.0 ／ 商用超阈值需付费 |`（措辞与 `README.md:96` 保持一致）；`mkdocs.yml` 的 `copyright` 同步改成 Polyform。
- **验收**：全仓（除 `LICENSE` 正文与 `COMMERCIAL.md`）不再出现"许可为 MIT"的表述。
- **工作量**：15 分钟

### T8 · 引擎版本口径统一
- **涉及文件**：`Docs/CN/Features.md:18`、`Docs/EN/Features.md:20`
- **步骤**：把「Unreal Engine 4.17.x - 5.x」（上游遗留）改成：
  > 引擎基线：**UE 5.8.3**（源码版）；代码保留了 4.17–5.x 时期的兼容写法，但不做验证。
  与 `README.md:93`、`Docs/index.md:128` 对齐。
- **验收**：三处口径一致。
- **工作量**：15 分钟

### T9 · README 乱码与截断
- **涉及文件**：`README.md:109`、`README.md:138`、`README_EN.md:109`、`README_EN.md:139`
- **步骤**：用 UTF-8 编辑器（不要 GBK 另存）修正
  - `:109` `Luau 鍚庣` → `Luau 后端`
  - `:138` `- 跑测试：`.\Tools\RunT` → `- 跑测试：``.\Tools\RunTests.ps1``（详见 [测试与自动化](Docs/CN/Testing.md)）`
- **验收**：`file README.md README_EN.md` 报 UTF-8；GitHub 渲染无乱码、无断句。
- **工作量**：15 分钟

### T10 · EN 页回链中文页
- **涉及文件**：`Docs/EN/*.md` 中所有 `../CN/*.md`
- **步骤**：`grep -rn "\.\./CN/" Docs/EN/` 列出全部（已知 `Features.md:40,79,85,110`），逐个改成 `../EN/<同名>.md`；确认目标文件存在。
- **验收**：`grep -rn "\.\./CN/" Docs/EN/` 零命中；`mkdocs build` 无警告新增。
- **工作量**：0.5 小时

### T11 · 补齐"代码有、文档没有"的零散项
- **涉及文件**：`Docs/CN|EN/Testing.md`、`Docs/CN|EN/API.md`（或 `Luau.md`）、`CHANGELOG.md:16`
- **步骤**
  1. Testing.md 补 `Tools/Rename-To-UnLuaNova.ps1`（改名/迁移脚本，现在 README 与 Docs 都没提）。
  2. Testing.md「切后端是全局状态」章节（CN 约 `:126`）就近补一句：`Saved/RunTests.lock` 是单实例锁，异常退出后需手动删除。
  3. API.md 补 `UE.__newindex`：引擎类型命名空间只读，脚本覆盖某个引擎类型会报错（CHANGELOG 已提到但 Docs 无）。
  4. `CHANGELOG.md:16` 的 `UnLuaStaticBindingGenerator` → `UnLuaNovaStaticBindingGenerator`（与 `Plugins/UnLuaNova/Source/UnLuaNovaEditor/Public/Commandlets/UnLuaNovaStaticBindingGeneratorCommandlet.h` 一致）。
- **验收**：Docs 里所有命令名/脚本名与 `Tools/`、`Plugins/` 中的真实名字一致。
- **工作量**：0.5 小时

---

## 阶段 P2：核心能力用例（3 ~ 5 天，收益最高）

> 通用约束（每个 spec 都要遵守）：
> - `AfterEach` 必须还原全局状态：`Env.Reset()`、还原 `UUnLuaNovaSettings`（用 `GetMutableDefault<UUnLuaNovaSettings>()`，改完必须写回原值，参考 `Private/Tests/Issue346Test.cpp:30/55` 的写法）、`FStaticBindingRegistry::Get().Unregister(...)`（参考 `CallUEFast.spec.cpp:64`）。
> - 需要新 stub 成员时加在 `Public/UnLuaNovaTestHelpers.h` 的 `UUnLuaNovaTestStub` 上（现有：`SimpleEvent/SimpleHandler/ComplexHandler/Counter/MapForIssue407/AddCount()/ProbeWithStaticBinding()/ProbeWithoutStaticBinding()`），不要新建 UCLASS。
> - 需要 Lua 夹具时放 `Content/Script/Tests/Specs/<Spec名>/`，命名与现有 `Tests/Specs/LuaEnv/支持启动脚本和参数.lua` 一致。
> - 一个 `It` 只验一件事；中文 `It` 描述沿用现有风格。

### T12 · `LuaEnv.spec.cpp` 扩容（2 → ~15）
- **文件**：`Private/Specs/LuaEnv.spec.cpp`（注册名保持 `UnLuaNova.API.FLuaEnv`）
- **逐条怎么做**
  1. **多 env 隔离**：已有（`:36-49`），补一条"env1 定义的全局变量在 env2 不可见"（`lua_getglobal(Env2.GetMainState(), "X")` 为 nil）。
  2. **`DoString` 语法错误**：`Env->DoString("this is not lua")` 返回 false（或按真实签名断言），且后续 `DoString("return 1")` 仍正确 → 证明没把状态机搞脏。
  3. **`DoString` 运行时错误**：`DoString("error('boom')")` 同上，并断言栈未被污染（`lua_gettop` 前后一致）。
  4. **`GC()`**：`DoString("t = {}")` → `Env->GC()` → 不崩且 env 仍可用（`DoString("return 1")` 正常）。
  5. **`AddLoader`**：注册一个自定义 loader，让 `require('SpecOnlyModule')` 命中它（返回一段 `return {ok=true}`），断言 loader 被调用且模块内容正确。
  6. **`AddBuiltInLoader`**：同上，改注册 builtin loader，并断言优先级高于文件系统（同名模块优先取 builtin）。
  7. **搜索路径**：`require` 一个 `Content/Script` 下真实存在的模块成功；require 不存在的模块 → 报错而不是静默 nil。
  8. **`Start` 启动模块不存在**：断言有明确错误/日志，而不是静默跳过（这是"改名后启动脚本失效"这类事故的典型入口）。
  9. **`Start` 参数**：已有（`:51-62`），补一条"多个参数都能取到"。
  10. **`HotReload()`**：跑一次 `Env->HotReload()`，断言不崩且之后 `DoString` 仍可用（先只断言"不崩 + 仍可用"，深入的热重载语义留给后续）。
  11. **`OnCreated` / `OnDestroyed` 委托**：绑定计数 lambda → 构造/销毁 env → 断言各触发一次且顺序正确。
  12. **`GetAll()` / `FindEnv()`**：构造两个 env，`GetAll().Num()` 增加，且能按 `lua_State` 找回。
- **验收**：`Automation RunTests UnLuaNova.API.FLuaEnv` 全绿；两个后端都跑。
- **工作量**：1 天

### T13 · `Overrides.spec.cpp`（新建，~12）
- **文件**：`Private/Specs/Overrides.spec.cpp`，注册名 `UnLuaNova.API.Overrides`
- **参考实现**：`Private/Tests/IssueOverridesTest.cpp` + `Content/Script/Tests/Binding/BP_UnLuaTestActor.lua`；API 见 `Public/LuaFunction.h`（`ULuaFunction::Override/RestoreOverrides/SuspendOverrides/ResumeOverrides/IsOverridable`）
- **逐条怎么做**
  1. `Override` 后调用该函数走 Lua 实现（断言 Lua 里设置的标记变量被写入）。
  2. `RestoreOverrides` 后回到蓝图/C++ 实现（断言 Lua 标记不再变化）。
  3. `SuspendOverrides` → 调用走原实现；`ResumeOverrides` → 又走 Lua。
  4. `IsOverridable`：对非 `BlueprintEvent` 的普通 `UFUNCTION(BlueprintCallable)` 返回 false；对 `BlueprintImplementableEvent` 返回 true。
  5. 覆写函数**内部报错**：Lua 实现里 `error('x')` → 引擎不崩、错误被记录、之后该对象仍可正常调用（这是最容易变成崩溃的分支）。
  6. 覆写 + **静态绑定 `bOverridable` 回退**：给一个已注册静态绑定的 `BlueprintEvent` 覆写后调用，断言仍走 Lua 实现（即 `FunctionDesc.cpp:225` 的快速路径条件里 `bOverridable` 分支被正确排除）。
  7. 同一个类被两个对象覆写，各自独立（改 A 不影响 B）。
  8. 覆写后销毁 Lua env，对象调用不崩（生命周期边界）。
- **验收**：全绿；**故意把 `FunctionDesc.cpp:225` 的 `bOverridable` 判断去掉后，第 6 条必须失败** —— 否则这条用例没有真正锁住行为。
- **工作量**：1 天

### T14 · `Lifetime.spec.cpp`（新建，~10）
- **文件**：`Private/Specs/Lifetime.spec.cpp`，注册名 `UnLuaNova.API.Lifetime`
- **对象**：`Private/LuaDanglingCheck.cpp`、`Private/LuaCore.cpp` 的 userdata/释放标记、`UnLuaNova::LowLevel::IsReleasedPtr`
- **逐条怎么做**
  1. Lua 持有 UObject 引用时，引擎侧 `CollectGarbage` 不回收该对象（断言 `IsValid`）。
  2. 对象被 `MarkAsGarbage`/销毁后，从 Lua 访问该 userdata 得到**受控错误**而不是崩溃（`pcall` 能捕获）。
  3. `LuaDanglingCheck` 打开时（参考 `Private/Tests/Issue517Test.cpp:35-36` 的设设置写法），悬垂访问会产生一条可读的诊断（断言日志/计数，而非崩）。
  4. `AddManualObjectReference` 后对象不被 GC；`RemoveManualObjectReference` 后可被 GC。
  5. env 销毁后，之前 push 给 Lua 的 UObject userdata 不可再用（不崩）。
- **验收**：全绿；用例本身不产生崩溃与泄漏（跑完整套后无 `Ensure` 洪水）。
- **工作量**：0.5–1 天

### T15 · `ErrorHandling.spec.cpp`（新建，~10）
- **文件**：`Private/Specs/ErrorHandling.spec.cpp`，注册名 `UnLuaNova.API.ErrorHandling`
- **逐条怎么做**
  1. 语法错误：`RunChunk`/`DoString` 编译失败时返回 false 并给出含文件名/行号的消息。
  2. 运行时错误：`error('boom')` 的错误消息含 `'boom'` 与 Lua 文件名行号。
  3. `pcall` 边界：`pcall(function() error('x') end)` 返回 false + 消息，且不影响后续调用。
  4. **栈平衡**：连续 3 次错误后 `lua_gettop(L)` 与初始一致（这是"错误污染栈"类 bug 的探测器）。
  5. 调用不存在的 UFunction：受控报错，不崩。
  6. 参数类型错误（传 string 给 int 参数）：受控报错。
  7. Lua 里访问 nil 引擎对象调用方法：受控报错。
  8. 错误消息里不含机器绝对路径（与 `GenerateTestReport.py:172 strip_machine_paths` 的理念一致，防止报告 diff 噪音）。
- **验收**：全绿；每条断言"不崩"的用例在 Release/Development 下都稳。
- **工作量**：0.5–1 天

### T16 · `Latent.spec.cpp`（新建，~6）
- **文件**：`Private/Specs/Latent.spec.cpp`，注册名 `UnLuaNova.API.Latent`
- **代码锚点**：`Private/LuaCore.cpp:1217`（`Class_CallLatentFunction`）、`:1230`（主线程调用报错）、`:1241`（C 回调进入的线程不可 yield）、`Private/UnLuaNovaLatentAction.cpp`
- **逐条怎么做**
  1. 协程内调用 latent 函数：能挂起并在回调后恢复（断言恢复后的返回值/后续代码被执行）。
  2. **主线程调用 latent**：断言报出 `LuaCore.cpp:1230` 的那条消息（用 `UnLuaNova::RunChunk` 取回错误消息并 `TEST_TRUE(Msg.Contains(...))`）—— 把这条消息固化成契约。
  3. **从引擎 C 回调进入的 Lua 线程**（例如委托回调）里起 latent：断言报出 `:1241` 那条消息。
  4. latent 完成后回调只触发一次（不重复）。
  5. latent 期间 env 销毁：不崩。
  6. 世界暂停后 delay 仍能推进（把 `Private/Tests/Issue276Test.cpp` 的语义收进来，或在该文件加注释指向本 spec）。
- **验收**：全绿；把 `LuaCore.cpp` 里两条错误消息文案改一个字，对应用例必须失败。
- **工作量**：0.5–1 天

### T17 · `Locators.spec.cpp`（新建，~6）
- **文件**：`Private/Specs/Locators.spec.cpp`，注册名 `UnLuaNova.API.Locators`
- **代码锚点**：`Public/LuaEnvLocator.h`、`Public/LuaModuleLocator.h`（`ULuaModuleLocator`、`ULuaModuleLocator_ByPackage`）；设置项切换写法见 `Private/Tests/Issue346Test.cpp:30/55`
- **逐条怎么做**
  1. 默认 `ULuaModuleLocator`：给定 UObject（用 `UUnLuaNovaTestStub` 与一个测试 Actor）得到预期模块名。
  2. 切到 `ULuaModuleLocator_ByPackage`：同一对象的模块名按包路径推导（断言与默认不同且符合预期格式）。
  3. 替换自定义 `ULuaModuleLocator` 子类：断言使用子类的结果（证明可扩展点真的生效）。
  4. 替换 `ULuaEnvLocator` 子类：断言对象被定位到指定 env。
  5. 设置项还原：用例结束后 `ModuleLocatorClass` 必须写回原值（否则污染后续用例 —— 用 `AfterEach` 保证）。
- **验收**：全绿；单独跑 `UnLuaNova.API.Locators` 与全套连跑结果一致（不依赖顺序）。
- **工作量**：0.5 天

### T18 · `ConsoleCommands.spec.cpp`（新建，~8）
- **文件**：`Private/Specs/ConsoleCommands.spec.cpp`，注册名 `UnLuaNova.Editor.ConsoleCommands`
- **现状**：`EditorCommands.spec.cpp` 只验命令存在；本 spec 验**行为**
- **逐条怎么做**（命令名以 `Private/UnLuaNovaConsoleCommands.cpp` 实际注册名为准，先 grep 一遍）
  1. `lua.do`：执行一段合法脚本生效（例如设置一个全局变量并能读回）。
  2. `lua.do` 一段错误脚本：命令返回/打印错误，编辑器不崩。
  3. `lua.profiler`：开关后能取到性能数据（断言数据非空 / 状态位翻转）。
  4. `lua.memstats`：输出内存统计（断言关键字段存在）。
  5. `lua.dbg`：开关调试服务（断言状态位翻转，且重复开关幂等）。
  6. `lua.sandbox`：切换沙盒后 `_G` 写保护生效/失效（`rawset(_G,'X',1)` 在 `pcall` 里的结果）。
  7. 未知命令/缺参数：不崩，给出用法提示。
- **验收**：全绿；命令名与代码一一对应（命令改名必须让用例失败）。
- **工作量**：0.5 天

### T19 · `Settings.spec.cpp`（新建，~8）
- **文件**：`Private/Specs/Settings.spec.cpp`，注册名 `UnLuaNova.Settings`（与现有 `LuaDeadLoopCheck.spec.cpp` 同前缀，把后者并入或保留均可）
- **逐条怎么做**：挑 6~8 个"改了必须看得见效果"的设置项（后端、沙盒、dead-loop 阈值、dangling check、启动模块名、模块定位类等），每条：保存原值 → 设置 → 断言行为变化 → `AfterEach` 还原。
- **验收**：全套连跑时其他 spec 不受影响（把还原写成 `AfterEach`，不要依赖用例内手动还原）。
- **工作量**：0.5 天

### T20 · 静态绑定生成器 & IntelliSense 用例
- **文件**：`Private/Specs/StaticBindingGenerator.spec.cpp`（1 → ~6）、新增 `Private/Specs/IntelliSense.spec.cpp`（~5，注册名 `UnLuaNova.Editor.IntelliSense`）
- **逐条怎么做**
  1. 生成器：对 `UUnLuaNovaTestStub` 生成的表**内容**断言（含 `ProbeWithStaticBinding`，其参数 Kind 为 Int）—— 现在只验"生成了文件"。
  2. 生成器：opt-out 类型（文档 `StaticExportBinding.md` 提到的排除机制）不出现在输出里。
  3. 生成器：连续两次生成结果一致（幂等，可比对文件字节）。
  4. 生成器：`-OutputDir` 指向不存在的目录时，报错而非崩（退出码非 0 + 可读消息）。
  5. 生成器：输出的表能被 `FStaticBindingRegistry::Register` 且 `Find/FindFunc` 命中（把生成与消费连起来 —— 目前两端各测各的）。
  6. IntelliSense：生成的 `UnLuaNova.d.lua` / 类型符号包含指定 UCLASS 与其 UFUNCTION；重命名类后重新生成生效；输出目录不存在时报错。
- **验收**：全绿；生成器输出改一处（比如漏一个函数）用例即失败。
- **工作量**：1 天

---

## 阶段 P3：结构与文档补齐（2 ~ 4 天）

### T21 · Lua 侧自测能力
- **目标**：随插件发布的 Lua 库（`UnLuaNova.HotReload`、`StringEx`、`Vector`/`Buffer`）现在只能被 C++ 间接驱动，纯 Lua 逻辑没有自测手段。
- **步骤**
  1. 新增 `Content/Script/Tests/Lua/Spec.lua`：极简框架，提供 `describe/it/equal`，把结果收集到一张结果表（不引第三方框架）。
  2. 新增 `Content/Script/Tests/Lua/*.spec.lua`：先覆盖 `StringEx`（split/join/strip/count）与 `Vector`/`Buffer`。
  3. C++ 侧新增 `Private/Specs/LuaSpecs.spec.cpp`（注册名 `UnLuaNova.Lua`）：遍历执行 `Tests/Lua/**/*.spec.lua`，把结果表搬回 C++ 逐条 `TestTrue`（失败要把 Lua 侧的文件名/行号带出来，否则没法定位）。
- **验收**：故意改坏 `StringEx.split` → 该 spec 失败并指出 Lua 文件与行号。
- **工作量**：1 天

### T22 · 性能基线
- **步骤**：把 `Private/Perfs/` 的 benchmark（现在注册 0 个自动化用例、只手工跑 umap 出 CSV）接成自动化用例：跑 N 次调用写 CSV 到 `Saved/Benchmark/`；`GenerateTestReport.py` 读取并与上次基线做环比，劣化超过阈值（建议 ±30%，先放宽防 flaky）只 warn。
- **验收**：报告里出现性能环比段落；本地连跑两次结果稳定。
- **工作量**：1 天

### T23 · 容器增强用例
- **步骤**：在现有 `LuaLib_Array/Map/Set.spec.cpp` 上补：容器内放 UObject/struct 时的生命周期（配合 T14）；`TSet pairs`（新宏 `UNLUANOVA_ITER_ENTRY(TSet_Pairs)`）**迭代中增删**的行为；嵌套容器。
- **工作量**：0.5 天

### T24 · 新增 `Architecture.md`（CN + EN）
- **内容**：目录结构；双后端抽象（`Public/LuaBackend.h` 的能力宏，为什么不能写死 `UNLUANOVA_LUA_BACKEND`）；LuaCore 绑定与覆写（thunk 替换 + opcode 注入，见 `Docs/CN/How_To_Implement_Overriding.md`）；静态绑定生成 → 注册 → `CallUEFast` 分发全链路；**如何新增一个 LuaLib 类型**（照着 `LuaLib_FIntPoint` 走一遍）；测试如何写（指向 Testing.md）。
- **注意**：新增页面要加 YAML front matter（`title`/`description`）、页尾「延伸阅读」，并挂进 `mkdocs.yml` 导航（`CONTRIBUTING.md:32-35` 的要求）。
- **工作量**：1 天

### T25 · 新增 `CxxApi.md`（CN + EN）
- **内容**：补齐审计里"公开 C++ API 大面积未文档化"的部分 —— `FLuaEnv` 全量方法、`IUnLuaNovaModule`、`ULuaEnvLocator`/`ULuaModuleLocator`、`UUnLuaNovaFunctionLibrary`、`RestoreTablesAfterSandbox`、`Binding.h` 导出注册接口（含 `UnExportClass`）、`FStaticBindingRegistry` + `UNLUANOVA_REGISTER_STATIC_BINDING`、`UnLuaNova::LowLevel::*`、`ULuaFunction`/`ULuaOverridesClass`、`FLuaDebugValue`/`FLuaVariable`。
- **做法**：以头文件为准逐项抄签名 + 一句话说明，不要凭记忆写。
- **工作量**：1 天

### T26 · 中英收敛与 docs 严格模式
- **步骤**：
  1. 先对齐差距最大的 `API.md`（CN 8.8KB / EN 5.9KB）与 `Settings.md`（7.8 / 5.1），把 CN 独有的 5 节补到 EN。
  2. 两份自动生成的中文报告现在同时挂在 CN/EN 导航下（`mkdocs.yml`）：要么让 `GenerateTestReport.py --publish` 同时产出英文页，要么只在中文导航挂报告、EN 导航给一句说明。
  3. 提交**占位报告**（`Docs/test-lua-report.md`、`Docs/test-luau-report.md` 的最小可用版本），让 CI 的 `mkdocs build` 可以重新加回 `--strict`（`.github/workflows/ci.yml:70-74` 解释了当初为什么去掉）。
- **工作量**：1 天

---

## 执行顺序与并行建议

| 批次 | 任务 | 说明 | 预估 |
| --- | --- | --- | --- |
| 批次 1（同一 PR） | T4 → T6 → T7 → T8 → T9 → T10 → T11 | 纯文档/命名，半天搞定，立刻消除误导 | 0.5 天 |
| 批次 2 | T2 → T3 → T1（降级版即可） | 反馈回路，优先级最高 | 1 天 |
| 批次 3 | T13 → T14 → T15 → T12 | 覆盖插件最脆、最难手测的部分 | 2.5 天 |
| 批次 4 | T16 → T17 → T18 → T19 → T20 | 其余核心域 | 2.5 天 |
| 批次 5 | T21 → T22 → T23 | 结构补齐 | 2.5 天 |
| 批次 6 | T5 → T24 → T25 → T26 | 文档收尾 | 2.5 天 |

批次 1 与批次 2 可并行（不同文件）；批次 3/4 建议串行（同一批 spec 一起编译，一次全量构建 ~23 分钟，攒着一起验更省时间）。

## 通用完成标准（每个任务都要满足）
1. `.\Tools\RunTests.ps1 -Backend Both`（至少 `-Backend Luau`）通过，且用例数**不低于**基线（T3 护栏生效后会自动报）
2. 改了 Docs 就 `python -m mkdocs build --strict` 通过，CN/EN 同步改，并挂好导航（`CONTRIBUTING.md:19,32-35`）
3. 行为变更同步进 `CHANGELOG.md`
4. 新增/删除用例时更新 `Tools/baseline/*.json`（`-UpdateBaseline`）

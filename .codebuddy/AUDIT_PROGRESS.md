# UnLuaNova 审计修复 · 执行进度

依据 `.codebuddy/AUDIT_TASKS.md`。已完成 **批次 1（文档/命名）+ 批次 2（反馈回路）**，
即 P0 与 P1 全部任务（T1–T4、T6–T11）。

## 已完成

| 任务 | 内容 | 改动 |
|---|---|---|
| T4 | 测试注册名统一大小写 | `UnLuaNovaBase.spec.cpp`、`UnLuaNovaEnum.spec.cpp`：`UNLUANOVA.API.Base/Enum` → `UnLuaNova.API.Base/Enum`；类名 `FUNLUANOVABaseSpec/EnumSpec` → `FUnLuaNovaBaseSpec/EnumSpec`。约定写进 CN/EN `Testing.md` |
| T6 | 删掉幽灵 API | `Docs/CN\|EN/API.md`：`FLuaContext::GLuaCxt` → `UnLuaNova::FLuaEnv` + `IUnLuaNovaModule::Get().GetEnv()`；`CreateState/GetState` 指向 `FLuaEnv::GetMainState()`；`AUTO_UnLuaNova_STARTUP` → `AUTO_UNLUA_STARTUP`、`UnLuaNova_ENABLE_DEBUG` → `UNLUANOVA_ENABLE_DEBUG`；删除已移除的 `SUPPORTS_RPC_CALL`/`SUPPORTS_COMMANDLET`；新增 `UE.__newindex` 说明 |
| T7 | 许可口径统一 | `Docs/index.md:131`、`mkdocs.yml:135` → Polyform Small Business 1.0.0 |
| T8 | 引擎版本口径统一 | `Docs/CN/Features.md:18`、`Docs/EN/Features.md:20` → 基线 UE 5.8.3 |
| T9 | README 乱码/截断 | `README.md`、`README_EN.md` 第 109 行 `Luau 后端`（原 GBK 误读乱码）与"跑测试"条目（原截断为 `.\Tools\RunT`）；两文件均为 UTF-8 |
| T10 | EN 回链修正 | `Docs/EN/{Debugging,Features,Settings}.md` 共 6 处 `../CN/` → `../EN/` |
| T11 | 补齐"有代码无文档" | CN/EN `Testing.md` 新增 `Tools/` 脚本表（含 `Rename-To-UnLuaNova.ps1`）；API.md 补 `UE.__newindex`；`CHANGELOG.md` Commandlet 改名 |
| T2 | `RunTests.ps1 -BuildOnly` | 新开关（与 `-SkipBuild` 互斥）+ 校验 + 主循环 `continue` + 文档行 + `.PARAMETER`/`.EXAMPLE` |
| T3 | 用例清单基线护栏 | `summary.json` 增 `names`；`GenerateTestReport.py` 增 `--baseline`/`--update-baseline`/`--fail-on-drift` 与比对/打印逻辑；`RunTests.ps1` 增 `-UpdateBaseline`/`-FailOnDrift` 并自动带 `--baseline`；新增 `Tools/baseline/{lua,luau}.json` |
| T1 | nightly（降级版） | 新增 `Tools/Install-NightlyTask.ps1`（注册每晚 `-BuildOnly` 编译的计划任务）；CN/EN `Testing.md` 新增「每晚至少编一次」+「用例清单基线」两节 |

## 新增文件
- `Tools/Install-NightlyTask.ps1`
- `Tools/baseline/lua.json`（427 用例）、`Tools/baseline/luau.json`（436 用例）

## 验证结果
- PowerShell 语法：`RunTests.ps1`、`Install-NightlyTask.ps1` 解析通过
- Python 语法：`GenerateTestReport.py` 编译通过
- 基线功能实测：
  - 与基线一致 → `BASELINE: 与基线一致（427 个用例）`，退出 0
  - 删掉 3 个用例名 → `BASELINE: 总数 433 -> 436(+3)` / `3 个用例新增`，`--fail-on-drift` 退出 1
- 文档链接：Docs + README 相对链接全部可解析（Showcase 图片缺失为既有问题，非本次引入）
- 代码围栏配对检查通过
- `mkdocs` 未安装，未执行 `mkdocs build --strict`（CI 会跑）

## 遗留 / 需要人工
1. **基线是从本机最近一轮报告（`Saved/TestReports/20261022-154202`）生成的**，不是新跑的。
   建议合入前跑一次 `.\Tools\RunTests.ps1 -Backend Both -UpdateBaseline` 刷新并提交。
2. `Docs/EN/index.md:36` 仍指向 `../CN/index.md` —— 这是**有意**的跨语言链接（提示读者可机翻中文页），故保留。
3. `OnLuaContextInitialized` / `OnPreLuaContextCleanup` / `OnPostLuaContextCleanup` 在代码里
   只有声明与定义、**没有 Broadcast 点**，已在文档中标注（未删除声明）。
4. T4 步骤 3 要求的「`-Filter UNLUANOVA` vs `-Filter UnLuaNova` 大小写敏感性实测」未做，
   需要一次真实跑测；改动本身是规范统一，两种情况下都成立。

## 未完成（后续批次）
- 批次 3：T13 Overrides、T14 Lifetime、T15 ErrorHandling、T12 LuaEnv 扩容（~2.5 天）
- 批次 4：T16 Latent、T17 Locators、T18 ConsoleCommands、T19 Settings、T20 静态绑定/IntelliSense（~2.5 天）
- 批次 5：T21 Lua 侧自测、T22 性能基线、T23 容器增强（~2.5 天）
- 批次 6：T5 用例清单表、T24 Architecture.md、T25 CxxApi.md、T26 中英收敛（~2.5 天）

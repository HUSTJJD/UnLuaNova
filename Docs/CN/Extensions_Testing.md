---
title: "扩展插件测试"
description: "LuaProtobuf / LuaRapidjson / LuaSocket 的测试覆盖、未覆盖范围与升级为 automation 的流程"
---

# 扩展插件测试

## 三个插件是什么：薄封装 + vendored 第三方

`LuaProtobuf` / `LuaRapidjson` / `LuaSocket` 是三个可选扩展插件（默认关闭、彼此无依赖，
详见 [插件与模块](Plugins_And_Modules.md)）。它们本身几乎是薄封装：各自的 `*Module.cpp` 只在
`FLuaEnv::OnCreated` 时用 `AddBuiltInLoader` 把第三方 Lua C 模块注册成内建 loader，并按需把
自带脚本目录追加到 `UnLuaNova.PackagePath`，没有业务逻辑。

真正的实现是仓库里 vendored 的第三方源码：

- `LuaProtobuf`：`Source/src`（`pb.cpp` / `pb.h`）与 `Content/Script`（`protoc.lua` / `serpent.lua`，
  lua-protobuf）；
- `LuaRapidjson`：`Source/src` 与 `Source/include/rapidjson`（lua-rapidjson / RapidJSON）；
- `LuaSocket`：`Source/src` 与 `Content/Script`（`socket.lua` 等，LuaSocket 3.0-rc1）。

第三方 C 源码不直接包含 `lua.h`，而是包含 UnLuaNova 提供的 `lua.hpp` 以适配 Lua / Luau 后端；
第三方组件与许可证记录在仓库根目录的 `LICENSE-THIRD-PARTY.md`。源码里真实注册的内建
loader 名为 `pb`（另 `pb.unsafe`）、`rapidjson`、`socket` / `socket.core` / `mime.core`；
Lua 侧对应 `require("pb")` / `require("rapidjson")` / `require("socket.core")`。关于调试器对
luasocket 的依赖，见 [调试](Debugging.md)。

## 当前测试覆盖

两层检查，都**不需要 UE 编译**、都**不改变 automation 用例集合**：

1. **Python 静态契约**：`Tools/tests/test_extension_plugins.py`（`unittest`，仅用 `pathlib` /
   `re` / `json` 读文件，不加载 Lua、不跑 UE）。它检查三个 `.uplugin` 存在且 `Plugins` 依赖
   `UnLuaNova` 正确、`Modules.Type` 合理、`EnabledByDefault` 契约；从 `*Module.cpp` 真实提取
   注册名并核对冒烟脚本与 [插件与模块](Plugins_And_Modules.md) 里记载的 `require` 名一致；
   确认 vendored 第三方源码目录存在、`LICENSE-THIRD-PARTY.md` 记载了三者、冒烟脚本里三个
   模块名都出现。运行：

   ```powershell
   python -B -m unittest discover -s Tools/tests -p "test_extension_plugins.py"
   ```

2. **Lua 手动冒烟**：`Content/Script/Tests/Extensions/SmokeExtensions.lua`。分别 `require`
   三个模块并做最小真实调用（protobuf 编解码 / JSON encode-decode / socket 基本对象创建），
   每个模块用 `pcall` 隔离并返回 `ok` / `err`，末尾返回汇总表；模块未启用时给出可诊断错误
   而不是崩溃。运行方式与预期输出见同目录 `README.md`。

## 未覆盖：UE 运行时

本次交付**没有 UE 运行时验证**：不新增 `*.spec.cpp`、不注册任何 `UnLuaNova*` automation 用例、
不跑 UE 编译、不生成真实测试报告。因此：

- Python 测试只做静态契约检查，不加载这些 Lua 模块、不执行任何 Lua 代码；
- 冒烟脚本需要人在编辑器 Lua 控制台或 PIE 里手动执行，其结果是一张 `ok` / `err` 汇总表，
  不是 automation 结果；
- **不声称"在 UE 里跑过"**。第三方库在真实 Lua/Luau 后端下的行为、以及跨后端差异，
  仍需在 UE 里验证。

## 为什么不做成 automation spec

`Tools/baseline/{lua,lua-game,luau,luau-game}.json` 是 `--fail-on-drift` 的用例清单门禁，
完整 Release 对 Editor 用例与两个报告阶段都会传该标志（见 [测试与自动化](Testing.md) 与
[测试架构](Testing_Architecture.md)）。新增 `*.spec.cpp` 会注册新的 `UnLuaNova*` 用例、改变
用例名集合；而本次无法跑 UE 编译、无法用真实报告生成新的用例清单。若只新增用例而四份清单
未同步刷新，完整 Release 就会因"清单漂移"失败。

所以本次刻意选择"不需要 UE 编译、不改变 automation 用例集合"的形式：Python 静态契约 +
Lua 手动冒烟。冒烟脚本不注册任何用例、不被任何 C++ spec 引用，运行它不会影响四份清单。

## 未来升级为 automation spec 的正确流程

要把这些检查提升为真正的 automation 用例，必须按下面顺序进行，缺一步就会触发漂移门禁：

1. 在测试插件（`UnLuaNovaTestSuite`）新增对应的 `*.spec.cpp`，注册 `UnLuaNova*` 用例；
   此时用例名集合发生变化。
2. 跑完整 Release（`Tools/RunTests.ps1`），真实编译并产出各后端 Editor 与 cooked `-game`
   报告（不要用失败的验证运行充当基线）。
3. 用该轮**真实报告**经 `--update-baseline` 刷新四份清单：Editor carriers 用
   `Tools/RunDevelopmentTests.ps1 -UpdateBaseline -Backend <B>` 重写
   `Tools/baseline/<B>.json`；cooked carriers 用
   `Tools/GenerateTestReport.py --reports <game dir> --out <game dir> --backend <B>
   --baseline Tools/baseline/<B>-game.json --update-baseline` 重写
   `Tools/baseline/<B>-game.json`。即四份文件
   `lua.json` / `lua-game.json` / `luau.json` / `luau-game.json` 都要以真实报告刷新。
4. 复核漂移门禁：完整 Release 对 Editor 与两个报告阶段都传 `--fail-on-drift`，确认用例总数
   与用例名集合与四份清单一致后再合入。不要手工编辑清单伪造"跑过"的用例。

---

## 延伸阅读

- [插件与模块](Plugins_And_Modules.md) — 扩展插件的组成与开关
- [调试](Debugging.md) — luasocket 与调试器的关系
- [测试与自动化](Testing.md) — 单入口 Release 验证
- [测试架构](Testing_Architecture.md) — 测试插件的组织

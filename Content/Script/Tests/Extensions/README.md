# 扩展插件手动冒烟脚本

本目录的 `SmokeExtensions.lua` 是给 `LuaProtobuf` / `LuaRapidjson` / `LuaSocket` 三个可选扩展
插件用的**手动冒烟脚本**，供人在编辑器 Lua 控制台或 PIE 里执行。它**不是 UE automation 用例**：
不注册任何 `UnLuaNova*` 用例、不被任何 C++ `*.spec.cpp` 引用、不改变
`Tools/baseline/{lua,lua-game,luau,luau-game}.json` 里的用例名集合。取舍原因见
`Docs/CN/Extensions_Testing.md`。

> 本脚本及本说明均**未声称在 UE 里实际运行过**。它只提供一套可手工执行的最小真实调用，
> 结果需要由执行者观察。

## 运行方式

脚本位于 `Content/Script/Tests/Extensions/SmokeExtensions.lua`，按 UnLuaNova 的模块路径，
`Content/Script` 是 require 的根，所以模块名是点分路径 `Tests.Extensions.SmokeExtensions`。

在编辑器 / PIE 的 Lua 控制台执行（任选其一）：

```lua
-- 方式一：require（推荐，路径与工程里其它 Tests.Lua.* 脚本一致）
local result = require("Tests.Extensions.SmokeExtensions").run()

-- 方式二：拿到返回值后自行遍历
local smoke = require("Tests.Extensions.SmokeExtensions")
local result = smoke.run()
for _, m in ipairs(result.modules) do
    print(m.name, m.ok, m.err or m.detail)
end
```

也可以用 `dofile` 加载该 `.lua` 文件后取 `run()`，但需自行给出工程内的实际文件路径；
`require` 方式更稳妥，因为它走插件已经配置好的 `UnLuaNova.PackagePath`。

`run()` 不抛错：每个模块单独用 `pcall` 包裹，某个插件未启用（对应内建 loader 没注册）时
`require` 会失败，被记录成该模块的 `ok=false` / `err`，不会影响其余两个模块。

## 预期输出

`run()` 返回一张汇总表，并在有 `print` 的控制台里逐模块打印一行。示意格式如下
（**仅为格式示意，非实测结果**）：

```
[SmokeExtensions] pb: ok - pb.encode/pb.decode round-trip ok (7 bytes)
[SmokeExtensions] rapidjson: ok - rapidjson.encode/rapidjson.decode ok ({"number":7,"label":"ok"})
[SmokeExtensions] socket.core: ok - socket.core.tcp() object created
```

返回表结构：

```lua
{
    all_ok = true,               -- 三个模块都 ok 时为 true
    modules = {
        { name = "pb",          ok = true,  detail = "..." },
        { name = "rapidjson",   ok = true,  detail = "..." },
        { name = "socket.core", ok = true,  detail = "..." },
    },
    note = "manual smoke; not a UE automation case; ...",
}
```

当某个插件未启用时，对应条目形如（示意）：

```lua
{ name = "pb", ok = false, err = "module 'pb' not found: ..." }
```

此时应先按下节启用插件并重启编辑器，让插件在 `FLuaEnv::OnCreated` 时注册内建 loader，
再重跑。`err` 里保留的是 `require` 的原始错误文本，可直接据此判断是"模块没注册（插件未启用）"
还是"模块加载了但调用失败"。

## 三个扩展插件的启用方式

三个插件默认关闭、彼此无依赖、可只开其中一个（见 `Docs/CN/Plugins_And_Modules.md`）。
在工程的 `.uproject` 的 `Plugins` 数组里，把需要的一项设为 `"Enabled": true`：

```json
{ "Name": "LuaProtobuf",  "Enabled": true },
{ "Name": "LuaRapidjson", "Enabled": true },
{ "Name": "LuaSocket",    "Enabled": true }
```

本 demo 工程 `UnLuaNovaDemo.uproject` 已经把三者都设为 `true`。改动 `.uproject` 后需要
重启编辑器：Lua C 模块是插件启动时通过 `AddBuiltInLoader` 注册进 Lua 环境的，运行中改配置
不会即时生效。仓库里没有显式 `EnabledByDefault` 字段，UE 缺省即 `false`，即默认关闭。

关于 luasocket 与 LuaPanda / LuaHelper 调试器依赖的关系，见 `Docs/CN/Debugging.md`。

## 为什么不做成 automation spec

`Tools/baseline/{lua,lua-game,luau,luau-game}.json` 是 `--fail-on-drift` 的用例清单门禁，
完整 Release 对 Editor 用例与两个报告阶段都会传该标志。新增 `*.spec.cpp` 会注册新的
`UnLuaNova*` 用例、改变用例名集合；而本次交付无法跑 UE 编译、无法生成新的真实用例清单，
一旦新增用例而四份清单未同步刷新，完整 Release 就会因"清单漂移"失败。

因此本次采用**不需要 UE 编译、不改变 automation 用例集合**的形式：

- Python 静态契约检查：`Tools/tests/test_extension_plugins.py`（纯静态，不加载 Lua、不跑 UE）；
- Lua 手动冒烟：本目录的 `SmokeExtensions.lua`（人工执行，返回 ok/err 汇总）。

未来若要升级为 automation spec，必须"新增 spec → 跑完整 Release 产出真实报告 → 用真实报告
`--update-baseline` 刷新四份清单 → 复核漂移门禁"，完整流程见
`Docs/CN/Extensions_Testing.md`。

# 从零接入第三方工程（最小清单）

> **先说清楚：这个目录不是一份可以直接打开的 `.uproject`。**
> 它是一份「接入清单 + 可直接复制的最小文件」：没有关卡、没有蓝图资产、没有 `.sln`，
> 也没有可运行的工程。请按下面的步骤，把这些文件放进**你自己已有的 UE 工程**里。
> 本目录刻意不含任何 `.uasset` / `.umap` / `.sln`，也不复制 `Content/` 里的资产。

## 目录里有什么

| 文件 | 用途 |
|---|---|
| `README.md` | 本文：接入清单、后端切换与失败排查 |
| `MyActor.lua` | 复制到自己工程 `Content/Script/MyActor.lua` 的最小脚本 |
| `DefaultUnLuaNova.ini.example` | 复制到自己工程 `Config/DefaultUnLuaNova.ini` 的覆盖项示例 |
| `uproject-snippet.json` | `.uproject` 里 `Plugins` 数组的**片段**，不是完整 `.uproject` |

## 前提

- 一个**你自己已有的 UE 工程**，且是 C++ 工程（有 `Source/` 和 `*.Build.cs`）。UnLuaNova 是 C++ 插件，编译它离不开 C++ 工具链。
- 本机装有与该工程匹配的引擎。`.uproject` 的 `EngineAssociation` 要填**你自己本机**的引擎版本或 GUID，不要照抄本仓库 demo 工程的值（本仓库 demo 用的是一个本机注册 GUID）。
- 命令行里 git 可用，第 2 步拉取子模块要用到。

## 步骤

### 1. 取得本仓库

```bash
git clone https://github.com/HUSTJJD/UnLuaNova
cd UnLuaNova
```

如果你已经把本仓库作为自己工程的子模块，直接在子模块目录里做下面两步即可。

### 2. 初始化 git 子模块

两个 Lua 后端（官方 Lua 与 Luau）的源码都以 git 子模块引入，位于 `Plugins/UnLuaNova/Source/ThirdParty/` 下（另有若干工具子模块在 `Tools/`）。**不初始化子模块就没有可编译的虚拟机源码。**

```bash
git submodule update --init --recursive
```

这一步要在**本仓库根目录**执行。

### 3. 把插件拷入自己工程的 `Plugins/`

- `Plugins/UnLuaNova/`：核心插件，**必需**。包含运行时模块 `UnLuaNova` 和编辑器模块 `UnLuaNovaEditor`。
- `Plugins/UnLuaNovaExtensions/`：可选扩展示例插件（LuaSocket / LuaRapidjson / LuaProtobuf），默认关闭，用不到就**不用拷**；用到时再在 `.uproject` 里把对应项 `"Enabled": true` 打开。
- `Plugins/UnLuaNovaTestSuite/`：自动化测试插件，第三方工程不需要，不用拷。

**一个容易踩的坑**：第 2 步的子模块内容由 git 管理，单纯「拷贝目录」不会自动带上子模块源码。请先完成第 2 步，再拷贝**完整的** `Plugins/UnLuaNova/`，并确认 `Source/ThirdParty/LuaLib/Lua` 与 `Source/ThirdParty/LuauLib/Luau` 下不是空目录；或者干脆把本仓库作为你自己工程的 git submodule，连子模块关系一起带过去。

### 4. 在 `.uproject` 里启用插件

打开自己工程的 `.uproject`，在 `Plugins` 数组里加入：

```json
{
  "Plugins": [
    { "Name": "UnLuaNova", "Enabled": true }
  ]
}
```

`uproject-snippet.json` 里就是这段。**它是片段，不是完整的 `.uproject`**：请把它合并进你自己工程已有的 `Plugins` 数组，不要拿它当工程文件打开。JSON 不允许注释，说明写在这里：

- `EngineAssociation` 保持你自己工程的原值，不要改成本仓库 demo 的值。
- 如果拷了 `UnLuaNovaExtensions`，按需再追加 `{ "Name": "UnLuaNovaExtensions", "Enabled": true }`；其下的 Lua C 模块（如 `LuaSocket`）也是同样方式单独启用。
- 可选扩展插件默认不启用，需要时显式打开；三者之间没有依赖，可以只开其中一个。

### 5. 生成工程文件并编译

- 右键自己工程的 `.uproject` → `Generate Visual Studio project files`（或用引擎的构建工具生成工程文件）。
- 编译 C++ 工程（例如 Development Editor / Win64）。首次编译会构建 `UnLuaNova`、`UnLuaNovaEditor` 模块以及所选后端的源码。
- 若编辑器启动时自动重新生成静态绑定产物（`.cpp`），再编译一次让产物进二进制即可；默认生成范围很小，这次重编很轻。

### 6. 创建 `Content/Script/MyActor.lua`

把本目录的 `MyActor.lua` 复制到 `<你的工程>/Content/Script/MyActor.lua`：

```lua
local M = UnLuaNova.Class()

function M:ReceiveBeginPlay()
    print(string.format("[MyActor] ReceiveBeginPlay, backend=%s", tostring(UnLuaNova.GetBackend())))
end

function M:ReceiveTick()
    -- 每帧回调；这里刻意留空，只用于确认脚本已绑定并在运行。
end

return M
```

约定（脚本文件顶部注释里也写了）：

- 脚本根目录固定是 `<工程>/Content/Script/`。
- 模块路径相对该目录、用点号分隔、不带 `.lua` 后缀：`MyActor` → `Content/Script/MyActor.lua`；`Game.Actor` → `Content/Script/Game/Actor.lua`。
- `Content/Script/` 下的 `.lua` 在 UE 内容浏览器里看不到，请用系统文件浏览器确认；建议把该目录加进 IDE 工程。
- `UnLuaNova.GetBackend()` 可查询当前后端（`"Lua"` 或 `"Luau"`）。本脚本只写两个后端都能解析的语法。

### 7. 用 ini 或绑定组件把蓝图绑到 `MyActor` 模块

默认模块定位器 `ULuaModuleLocator_ByBinding` 按这个顺序找模块名：

1. 实现 `IUnLuaNovaInterface::GetModuleName()`（C++ 或蓝图）
2. `DefaultUnLuaNova.ini` 里的 `+ClassModuleBindings` 类→模块映射
3. 蓝图 Actor 上的 `UnLuaNova Binding` 组件

**关键约束**：`bDataDrivenBinding` 默认 `False`，它是一条安全边界，不是性能开关。第 2、3 种方式作用在**没有实现接口**的蓝图上时，必须在工程 `Config/DefaultUnLuaNova.ini` 里设 `bDataDrivenBinding=True`，否则不生效。

选一种即可：

**方式 A：接口 + GetModuleName（无需改 ini，最省事）**

1. 新建一个以 Actor 为父类的蓝图（例如 `BP_MyActor`）。
2. 打开蓝图 → UnLuaNova 工具栏「绑定」；它会按蓝图路径自动填充模块名，把 `GetModuleName` 的返回值改成 `MyActor`，与第 6 步的脚本对应。
3. 「编译」并「保存」蓝图。图文步骤见 [UE 新手快速上手](../../Docs/CN/Quickstart_For_UE_Newbie.md)。

**方式 B：ini 类→模块映射**

1. 把 `DefaultUnLuaNova.ini.example` 复制成 `<工程>/Config/DefaultUnLuaNova.ini`。
2. 取消注释 `+ClassModuleBindings` 那行，把 `Class` 换成自己蓝图的软类路径（蓝图生成类以 `_C` 结尾，例如 `/Game/Blueprints/BP_MyActor.BP_MyActor_C`），`ModuleName` 保持 `"MyActor"`。
3. 保留 `bDataDrivenBinding=True`。

**方式 C：绑定组件**

蓝图里 Add Component → `UnLuaNova Binding`，在组件里填模块名 `MyActor`；若蓝图未实现接口，同样需要 `bDataDrivenBinding=True`。

注意：工程侧 ini **只写覆盖项**，不要整份照抄插件自带的 `Plugins/UnLuaNova/Config/DefaultUnLuaNova.ini`，否则插件升级改了默认值，工程这份会停在旧值上。

### 8. PIE 验证

1. 把 `BP_MyActor` 放进一个关卡（新建或现有关卡都行），打开该关卡。
2. 点 Play（PIE）。
3. Output Log 里应看到 `[MyActor] ReceiveBeginPlay, backend=Luau`（按当前后端显示 `Luau` 或 `Lua`）；`ReceiveTick` 每帧被调用，示例里留空。
4. 改了脚本后，热重载默认模式是 `Manual`：按 `Alt+L` 或点工具栏「热重载」。

### 9. 切换 Lua 后端（编译期，需重编）

在**自己工程**的 `Config/DefaultUnLuaNova.ini` 里：

```ini
[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]
LuaBackend=Luau
```

| 值 | 后端 |
|---|---|
| `Luau` | Luau（默认），提供渐进类型、原生沙盒、原生断点调试等能力 |
| `Lua` | 官方 Lua，与上游 UnLua 行为一致 |

- `LuaBackend` 不是 UPROPERTY：`UnLuaNova.Build.cs` 直接从**工程** `Config/DefaultUnLuaNova.ini` 读这个键，编辑器设置 UI 里看不到；写进插件自带的 `Plugins/UnLuaNova/Config/DefaultUnLuaNova.ini` **不会影响编译结果**。
- 后端是**编译期**选择：改完必须**重新编译**，否则旧二进制仍是旧后端。
- 没写这个键时默认 `Luau`。
- 两个后端源码都是第 2 步的 git 子模块，切换前确认都已初始化。
- 双后端语法差异：Luau 专属语法（`continue`、复合赋值、类型注解、vector / buffer 等）在 `Lua` 后端下 require 那一刻就是语法错误。这类示例统一走 `Content/Script/Luau/Index.lua` 守卫入口：`require "Luau.Index"` 会先用 `UnLuaNova.GetBackend()` 判断，只有确认是 Luau 才加载那些文件，否则返回 `Available = false`。

## 常见失败排查

**找不到模块**

- 脚本不在 `<工程>/Content/Script/` 下，或文件名大小写不一致；用系统文件浏览器确认（内容浏览器不显示 `.lua`）。
- 模块路径写成了 `Content/Script/MyActor.lua`、`MyActor.lua` 或 `My/Actor`；正确写法是相对脚本根目录、点号分隔、不带后缀，例如 `MyActor`。
- 文件存成了带 UTF-8 BOM 的编码；`bLegacyAllowUTF8WithBOM` 默认 `False`，建议保存为无 BOM 的 UTF-8。
- 改完脚本没生效：`HotReloadMode` 默认 `Manual`，需要手动热重载。

**绑定不生效（脚本存在但不执行）**

- 蓝图没实现 `IUnLuaNovaInterface`，而 `bDataDrivenBinding` 仍是默认 `False` → ini 映射和绑定组件都不会生效。改用方式 A，或设 `bDataDrivenBinding=True`。
- `+ClassModuleBindings` 的 `Class` 路径不对：蓝图类要写完整软类路径并以 `_C` 结尾；短类名要用 `ClassName=` 而不是 `Class=`（无效路径会被 UE 清空）。
- 蓝图改完没有「编译」并「保存」。
- Actor 没有放进当前打开的关卡，或 PIE 没在跑。
- 像 `UAnimNotifyState` / `UBlueprintFunctionLibrary` 这类 PIE 退出后仍常驻内存的类型，第二次进 PIE 可能捕获不到构造事件导致绑定失效；把这些类加进 `+PreBindClasses`。

**后端不匹配**

- 改的是插件自带的 ini，而不是工程 `Config/DefaultUnLuaNova.ini`（Build.cs 只认工程那份）。
- 改完没有重新编译。
- 在 `Lua` 后端下 require 了含 Luau 专属语法的脚本；改走 `Luau.Index` 守卫，或把后端切成 `Luau` 并重编。
- 子模块没初始化，某个后端源码缺失导致编译失败；回到第 2 步执行 `git submodule update --init --recursive`。

## 本目录不提供什么

- 没有 `.uproject`、`.sln`、`.uasset`、`.umap`，也没有 `Content/` 资产；不要试图直接打开这个目录运行。
- 不复制 `Content/Script/` 里的示例脚本，只有一份最小 `MyActor.lua`。
- 关卡、蓝图资产都需要在你自己的工程里创建。

## 延伸阅读

- [UE 新手快速上手](../../Docs/CN/Quickstart_For_UE_Newbie.md) — 图形化的绑定与模板生成步骤
- [设置项](../../Docs/CN/Settings.md) — 全部设置项与默认值
- [插件与模块](../../Docs/CN/Plugins_And_Modules.md) — 插件组成与开关
- [教程索引](../../Docs/CN/Tutorials.md) — 13 个可运行教程

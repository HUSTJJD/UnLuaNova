# UnLuaNova

**UnLuaNova** 是面向 Unreal Engine 的 Lua 脚本方案，是 [Tencent/UnLua](https://github.com/Tencent/UnLua) 的社区续作。

UnLua 的官方发布线冻结在 v2.3.6（2023-11），`develop` 分支虽仍有零星修复但无版本、无维护者。
UnLuaNova 把这些散落的社区成果收敛成一条有人维护的发行线，并在此基础上做三件事：

1. **静态绑定** —— 为声明类型生成绑定表，绕开反射热路径与运行时查找开销
2. **双 Lua 后端** —— [Lua 5.5](https://www.lua.org/) 与 [Luau](https://luau.org)（默认），均以 git 子模块引入
3. **Lua 可观测性** —— CPU Profiler（写入 Unreal Insights）、headless 调试服务、沙盒、死循环看门狗

## 状态

⚠️ **当前代码尚未在 UE 5.8 上编译验证**，属于早期重构阶段，请勿用于生产。

| 项 | 值 |
|---|---|
| 引擎基线 | **UE 5.8.3**（源码版） |
| Lua 后端 | **Luau（默认）** ／ Lua 5.5 —— 均以子模块引入 |
| 上游基线 | Tencent/UnLua v2.3.6 + `develop`(2026-04) |

## 快速开始

1. 复制 `Plugins` 目录到你的 UE 工程根目录
2. 初始化子模块 —— Lua 5.5 与 Luau 均以 git 子模块引入，不初始化会缺少 VM 源码：

   ```bash
   git submodule update --init --recursive
   ```

3. 重新启动工程
4. 新建蓝图 → UnLuaNova 工具栏选择 `绑定` → 填入 Lua 模块路径（如 `GameModes.BP_MyGameMode`）
5. 选择 `创建 Lua 模版文件`，在 `Content/Script/GameModes/BP_MyGameMode.lua` 里写代码

## 在 UE 中使用 Lua

- 直接访问 UCLASS / UPROPERTY / UFUNCTION / USTRUCT / UENUM，无需胶水代码
- 替换蓝图中定义的实现（Event / Function）
- 处理各类事件通知（Replication / Animation / Input）

## 相对 UnLua 的增量

- UE 5.4 / 5.6 / 5.8 支持
- 静态绑定 + 构建期代码生成（类型选择 **opt-out**：默认全导出，只需给不想要的加 `meta=(UnLuaNovaNoExport)`）
- 双 Lua 后端（Lua 5.5 / Luau）+ 后端抽象层
- Lua CPU Profiler（写入 Unreal Insights）、`lua.dbg` 调试服务、`lua.sandbox` 沙盒
- 死循环看门狗与 Profiler 经 VM hook 分发器共存

## 从 UnLua 迁移

Lua 侧的全局名已从 `UnLua` 改为 **`UnLuaNova`**，需要批量替换：

```lua
local M = UnLua.Class()        -- UnLua
local M = UnLuaNova.Class()   -- UnLuaNova
```

另外 UCLASS 名也已改名（`UUnLuaSettings` → `UUnLuaNovaSettings` 等），需要 resave 资产并迁移 `Config/*.ini` 的 section 名。

## 文档

- [编程指南](Docs/CN/UnLua_Programming_Guide.md)
- [设置选项](Docs/CN/Settings.md) ｜ [调试](Docs/CN/Debugging.md) ｜ [智能提示](Docs/CN/IntelliSense.md)
- [FAQ](Docs/CN/FAQ.md) ｜ [API](Docs/CN/API.md)
- 上游历史变更：[Docs/Archive/UnLua_CHANGELOG.md](Docs/Archive/UnLua_CHANGELOG.md)

## 许可

MIT。基于 Tencent/UnLua（MIT），见 [LICENSE](LICENSE.TXT)。

Lua 虚拟机使用 [Lua 5.5](https://www.lua.org/)（MIT）；Luau 使用 [luau-lang/luau](https://github.com/luau-lang/luau)（MIT）。两者均以 git 子模块引入，需执行 `git submodule update --init --recursive`。

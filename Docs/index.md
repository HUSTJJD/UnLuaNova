# UnLuaNova

**UnLuaNova** 是面向 Unreal Engine 的 Lua 脚本方案，是 [Tencent/UnLua](https://github.com/Tencent/UnLua) 的社区续作。

UnLua 的官方发布线终结在 v2.3.6（2023-11），`develop` 分支虽仍有零星修复但无版本、无维护者。UnLuaNova 把这些散落的社区成果收敛成一条有人维护的发布线，并在此基础上做三件事：

1. **静态绑定** —— 为声明类型生成绑定表，绕开反射热路径与运行时查找开销
2. **可换 Lua 后端** —— [Lua 5.5](https://www.lua.org/) 与 [Luau](https://luau.org)（默认），均以 git 子模块引入
3. **Lua 可观测性** —— CPU Profiler（写入 Unreal Insights）、headless 调试服务、沙箱、死循环看门狗

## 状态

⚠️ **当前代码尚未在 UE 5.8 上编译验证**，属于早期重构阶段，请勿用于生产。

| 项 | 值 |
|---|---|
| 引擎基线 | **UE 5.8.3**（源码版） |
| Lua 后端 | **Luau（默认）** + Lua 5.5 —— 均以子模块引入 |
| 上游基线 | Tencent/UnLua v2.3.6 + `develop`(2026-04) |

## 快速开始

1. 复制 `Plugins` 目录到你的 UE 工程根目录
2. 初始化子模块（Lua 5.5 与 Luau 均以 git 子模块引入，不初始化会缺 VM 源码）：
   ```bash
   git submodule update --init --recursive
   ```
3. 重新启动工程
4. 新建蓝图 → UnLuaNova 工具栏选择 `绑定` → 填入 Lua 模块路径（如 `GameModes.BP_MyGameMode`）
5. 选择 `创建 Lua 模板文件`，在 `Content/Script/GameModes/BP_MyGameMode.lua` 里写代码

## 文档导航

- **中文文档** —— 从 [UE 新手快速上手](CN/Quickstart_For_UE_Newbie.md) 开始，或查看[功能清单](CN/Features.md)、[编程指南](CN/UnLuaNova_Programming_Guide.md)
- **English Docs** —— [Programming Guide](EN/UnLuaNova_Programming_Guide.md) / [API Reference](EN/API.md)

## 仓库

- GitHub：[HUSTJJD/UnLuaNova](https://github.com/HUSTJJD/UnLuaNova)

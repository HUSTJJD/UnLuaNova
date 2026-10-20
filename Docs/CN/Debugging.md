---
title: "调试"
description: "用 LuaPanda / LuaHelper 在 VSCode 里断点调试 UnLuaNova 脚本"
---

# 调试

调试脚本有三条路：内置的 headless 调试服务（Luau 后端，零依赖）、LuaPanda / LuaHelper（VSCode 图形化断点）、
以及第三方调试器。先用哪条取决于后端与你的环境。

## 使用内置调试服务（lua.dbg）

Luau 后端提供 VM 原生的断点原语（`lua_breakpoint` / `lua_singlestep`），UnLuaNova 把它包成了一个
不带界面的调试服务：不用装插件、不用 `luasocket`，直接在控制台驱动。

```
lua.dbg                        -- 状态：是否挂载 / 暂停 / 命中次数 / 断点数
lua.dbg bp MyActor 42          -- 在 MyActor 的第 42 行下断点（再执行一次即移除）
lua.dbg stack                  -- 打印上一次捕获的调用栈
lua.dbg locals                 -- 打印上一次捕获的局部变量
lua.dbg step                   -- 单步
lua.dbg continue               -- 继续执行
lua.dbg clear                  -- 清除全部断点
lua.dbg detach                 -- 卸载调试 Hook，恢复零开销
```

特点：

* 不敲命令时不安装任何 Hook，**常态零开销**
* 断点与单步是 Luau 专属能力；Lua 5.5 后端退化为按行号匹配
* 它本身不带界面，能力是暴露给外部工具驱动的（DAP 适配器在规划中）

详见[Luau 后端](Luau.md#原生断点调试)。

## 使用 LuaPanda / LuaHelper 调试

### 一、准备工作
1. 从VSCode应用市场安装 [LuaPanda](https://marketplace.visualstudio.com/items?itemName=stuartwang.luapanda) / [LuaHelper](https://marketplace.visualstudio.com/items?itemName=yinfei.luahelper)
2. 从 [LuaPanda官方仓库](https://github.com/Tencent/LuaPanda/tree/master/Debugger) 获取`LuaPanda.lua`，放入`{UE工程}/Content/Script`目录
3. 在Lua代码中加入 `require("LuaPanda").start("127.0.0.1",8818)`

### 二、开始调试
VSCode环境请参考各插件的配置文档，这里不再赘述。
- [LuaPanda](https://github.com/Tencent/LuaPanda)
- [LuaHelper](https://github.com/Tencent/LuaHelper)

注：调试器依赖`luasocket`，UnLuaNova已通过扩展插件集成，如果发现无法连接请检查`{UE工程}/Plugins/UnLuaNovaExtensions`目录是否存在。

## 使用 LuaBooster 调试

TODO:

---

## 延伸阅读

- [智能提示 IntelliSense](IntelliSense.md) — 让 IDE 认识 UE API
- [控制台命令](ConsoleCommand.md) — lua.dbg 等运行时命令
- [Luau 后端](Luau.md) — 原生断点、沙盒等 Luau 专属能力
- [测试与自动化](Testing.md) — 自动化测试与截图

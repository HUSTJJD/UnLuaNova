---
title: "调试"
description: "用 LuaPanda / LuaHelper 在 VSCode 里断点调试 UnLuaNova 脚本"
---

# 调试

调试脚本有四条路：内置的 DAP 服务（`lua.dap`，VSCode 直接 attach）、内置 headless 调试服务
（`lua.dbg`，控制台驱动）、LuaPanda / LuaHelper（VSCode 图形化断点）、以及第三方调试器。
先用哪条取决于后端与你的环境。

## 使用 DAP 服务在 IDE 里调试（lua.dap）

`lua.dbg` 刻意不带 socket、不定义线协议 —— 那一层属于 DAP。缺的一层现在有了：

```
VSCode ──DAP/JSON──► lua.dap（编辑器进程内，自带线程）──► lua.dbg 的调试服务
```

```
lua.dap start [port]     -- 默认 5699，起服务并等 IDE 来 attach
lua.dap status
lua.dap stop
```

VSCode 侧用 **attach** 方式连 `127.0.0.1:5699`（`request: "attach"`）。已实现：
`initialize` / `attach` / `setBreakpoints` / `threads` / `stackTrace` / `scopes` /
`variables` / `pause` / `evaluate` / `continue` / `next` / `stepIn` / `stepOut` / `disconnect`。

服务仅监听回环地址。端口 `0` 由系统分配，实际端口可通过 `lua.dap status` 查看。
启动与停止串行化，停止后排队的旧服务任务不会作用于重启后的服务。

几个行为要先知道，不然会以为坏了：

* **命中断点时游戏线程是阻塞自旋的**（等 `Continue()`），这是 IDE 能看栈的前提。
  编辑器 UI 会跟着卡住，属预期
* **暂停中改断点要等 `Continue()` 之后才生效**：游戏线程阻塞着，排不进去
* 只支持 attach，不支持 launch：调试目标是编辑器 / PIE 进程，不是独立进程
* **多环境时用 `arguments.env` 指定挂到第几个**（下标从 0，缺省 0）：
  `{"arguments": {"env": 1}}`。PIE 下监听服务器 + 两个客户端是三个环境，
  挂错了的表现是断点永远不命中，而没有任何报错

### 暂停与求值

`pause` 与 `evaluate` 的实现方式值得单独说清，因为它们都不是"理所当然能动"的：

* **`pause`**：Lua 没有"从外部打断正在跑的 VM"这回事，所以它不是强制中断，而是
  **让下一行成为停止点**。Luau 后端的 debug hook 只在断点与单步时触发，因此实现是
  临时武装一次单步、落地后立即解除（`FLuaDebugService::RequestPause`）。
  请求发生在游戏线程（`AsyncTask`），停止事件由 `PumpStoppedState` 推出。
* **`evaluate`**：当前在适配线程执行，运行中的 VM 缺少并发求值同步保护；应在断点暂停后使用，
  不能据此宣称运行中求值是线程安全的。表达式被编译成

  ```lua
  return (function(<该帧的局部变量...>) return <表达式> end)(...)
  ```

  再用该帧的局部变量值作参数调用，所以 `self.HP`、`i + 1` 写起来就是看上去的意思。
  **upvalue 也参与**：局部变量之后会接着把该帧函数的 upvalue 收进来一起当参数 ——
  Lua 项目的逻辑常常就活在 upvalue 里（用了 `HotReload.lua` 的更是如此），少了它们，
  一个明明在作用域里的变量会回答"attempt to index a nil value"，看起来像"它没了"，
  把人引向错误的方向。`_ENV` 是唯一被排除的：它是每个函数的第一个 upvalue，
  当参数绑进来会把全局表整个遮住。同名局部变量优先 —— 它是更近的绑定。
  没暂停或没带 `frameId` 时只对全局求值，供 watch / repl 使用。

## 使用内置调试服务（lua.dbg）

Luau 后端提供 VM 原生的断点原语（`lua_breakpoint` / `lua_singlestep`），UnLuaNova 把它包成了一个
不带界面的调试服务：不用装插件、不用 `luasocket`，直接在控制台驱动。

```
lua.dbg                        -- 状态：挂在第几个环境 / 是否挂载 / 暂停 / 命中次数 / 断点数
lua.dbg env                    -- 列出当前所有 Lua 环境，标出正在调试的那个
lua.dbg env 1                  -- 切到第 1 个环境（下标从 0）
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
* **两个后端现在都是按行匹配**。Luau 的 `lua_breakpoint()` 要的是一个已在栈上的函数
  （它只补那一个 Proto），而 `(source, line)` 这种写法在"有东西正在执行它"之前定位不到
  Proto，所以当前实现是：有断点时借 `lua_singlestep` 让 hook 按行走，再自己匹配
  source:line，与 Lua 5.5 后端同一套机制。代价是断点挂着的那段时间每行一次 hook；
  不挂断点时仍然是零开销
* **多环境要选**。`ULuaEnvLocator_ByGameInstance` 下每个 GameInstance 一个环境
  （监听服务器 + 两个客户端就是三个），调试服务一次只挂一个。默认跟着模块那个环境走，
  要换就用 `lua.dbg env <n>`；DAP 侧是 launch.json 里的 `arguments.env`
* 它本身不带界面，能力是暴露给外部工具驱动的 —— IDE 那层是 [`lua.dap`](#使用-dap-服务在-ide-里调试luadap)

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

## 延伸阅读

- [智能提示 IntelliSense](IntelliSense.md) — 让 IDE 认识 UE API
- [控制台命令](ConsoleCommand.md) — lua.dbg 等运行时命令
- [Luau 后端](Luau.md) — 原生断点、沙盒等 Luau 专属能力
- [测试与自动化](Testing.md) — 自动化测试与截图

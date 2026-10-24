---
title: Luau 后端
description: UnLuaNova 的 Luau 后端：立即可用的语言特性、类型声明与 luau-lsp、沙盒、原生断点调试，以及与 Lua 5.5 的行为差异
---

# Luau 后端

[Luau](https://luau.org) 是 UnLuaNova 的**默认后端**，另一个后端是官方 Lua 5.5。
本文面向脚本作者：能用什么、不能用什么、以及哪些能力还没打通。

## 切换后端

在 `Config/DefaultUnLuaNova.ini` 里选择，然后**重新编译**：

```ini
[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]
LuaBackend=Luau   ; 或 Lua
```

!!! warning "后端是编译期选择"
    两个后端导出同名符号、共用 `lua.h` 等头文件名，无法在同一个构建里共存。
    改完 ini 必须重新编译，不是运行时开关。

---

## 一、立即可用的语言特性

这些来自 Luau 语言与标准库本身（`luaL_openlibs` 已注册全部 Luau 库），不需要动引擎代码：

| 特性 | 状态 | 备注 |
|---|:--:|---|
| 类型注解、`--!strict` / `--!nonstrict` | 可用 | 配合生成的 `UnLuaNova.d.lua` 使用，见[类型声明](#三类型声明与-luau-lsp) |
| `continue` | 可用 | Lua 5.5 没有这个关键字 |
| 复合赋值 `+=` `-=` `*=` `/=` `%=` `..=` | 可用 | 同上，属于语法层面不兼容 |
| `type()` / `typeof()` | 可用 | `typeof` 对 userdata 返回类名 |
| `table.freeze` / `isfrozen` / `clone` / `clear` | 可用 | freeze 后改写会报错，可用来保护配置表 |
| `coroutine.close` | 可用 | Lua 5.4+ 才有 |
| `bit32`、`utf8` | 可用 | Luau 标准库自带 |
| `vector`（原生 vector3 值类型） | 已打通 | 见[vector 与 FVector](#vector-与-fvector) |
| `buffer` | 库已注册，容器需显式转换 | 见[vector 与 FVector](#vector-与-fvector) |
| `load()` | 由 UnLuaNova 补齐 | Luau 原生没有源码加载器 |
| `debug.getupvalue` / `setupvalue` / `upvalueid` / `getlocal` / `setlocal` | 由 UnLuaNova 补齐 | 热重载依赖它们 |

一段能体现 Luau 味道的写法：

```lua
--!strict
local Config = table.freeze({
    MaxHp = 100,
    RegenPerSecond = 2,
})

function M:ReceiveTick(DeltaTime: number)
    for _, Member in ipairs(self.Members) do
        if not Member.bAlive then
            continue                                   -- Luau 关键字；Lua 5.5 下是语法错误
        end
        Member.Hp += Config.RegenPerSecond * DeltaTime  -- 复合赋值
        if Member.Hp > Config.MaxHp then
            Member.Hp = Config.MaxHp
        end
    end
end
```

!!! tip "跨后端写法"
    上面这段代码在 Lua 5.5 后端下**编译不过**（`continue`、复合赋值、类型注解都是 Luau 语法）。
    需要双后端通用的代码请避开它们，或放到独立目录里做运行时守卫。

---

## 二、UnLuaNova 已接线的 Luau 能力

这些是 UnLuaNova 主动用上 Luau 原生 API 的地方，Lua 5.5 后端**没有对应能力**：

| 能力 | Luau 原生 API | 入口 | 说明 |
|---|---|---|---|
| 沙盒 | `luaL_sandbox` | CVar `lua.sandbox` | 全局表只读，见[沙盒](#沙盒) |
| 断点 / 单步 | `lua_breakpoint`、`lua_singlestep` | `lua.dbg` | 见[原生断点调试](#原生断点调试) |
| 分配分类统计 | `lua_callbacks->onallocate` | `lua.memstats` | 按内存类别 / Lua 类型 / userdata 标签 |
| 函数级打点 | `lua_Debug.protoid` | `lua.profiler` | 稳定且唯一的 profiler 缓存键 |
| 迭代协议 | `__iter` | 容器绑定 | `pairs(TArray)` 等在两端都能用 |
| 64 位整数 | `lua_pushinteger64` | 绑定层 | Lua 5.5 走 `lua_pushinteger` |

### 沙盒

`luaL_sandbox` 把全局表置为只读并标记环境为安全，脚本就无法意外改写引擎绑定了。

!!! warning "它是启动期 CVar，不是运行时命令"
    `lua.sandbox` 是一个控制台变量（默认 `0`），在**每个 Lua 环境创建时读取一次**。
    运行中再改它不会影响已经存在的环境，也不会重新加固。
    要开启就写进 ini，让它在启动时生效：

    ```ini
    ; Config/DefaultEngine.ini
    [ConsoleVariables]
    lua.sandbox=1
    ```

开起来之后，这类写法会直接报错：

```lua
UE = nil                     -- 改写全局：沙盒下报错
UE.UObject = somethingElse   -- 改写引擎绑定：沙盒下报错
```

热更新 / 下载脚本建议开启；存量项目先评估，因为它会改变"全局可写"这一既有行为。
该能力依赖 Luau 原生沙盒，Lua 5.5 后端不支持（`UNLUANOVA_HAS_SANDBOX == 0`）。

另外，`UE` 命名空间自身装了 `__newindex`：脚本改写里面的类型会被直接拒绝 ——
类型是由绑定层用 `lua_rawset` 发布的，脚本没有理由去改它。这一条在两个后端下都生效。

!!! note "沙盒会顺手冻结 `UE`，UnLuaNova 已经把它解冻"
    `luaL_sandbox` 会把全局表下的所有子表都设成只读，其中就包括 `UE`，
    而 UE 的类型是**懒注册**的：第一次访问 `UE.XXX` 时才写进去。
    所以沙盒之后，UnLuaNova 会把 `UE`、`Lua`、`package.loaded` 这几张表的写权限还回去 ——
    否则下一个访问新类型的脚本、下一次 `require`，都会变成错误。
    脚本那一侧的写入仍然由 `__newindex` 拦着，保护的强度没有下降。

### 原生断点调试

Luau 的断点原语能让 VM 停在指定行，这是 `lua_sethook` 做不到的（后者只能"报告"每一行）。
UnLuaNova 把它包成了 headless 调试服务，见[调试](Debugging.md)：

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

不敲命令时不安装任何 Hook，常态零开销。断点与单步是 Luau 专属，Lua 5.5 后端退化为按行号匹配。

### 内存分类统计

```
lua.memstats on     -- 安装分配回调
lua.memstats        -- 输出报告
lua.memstats reset  -- 清零
```

报告分两部分：**总量**（当前占用、峰值、分配/重分配/释放次数）与**分类**（按内存类别、Lua 类型、userdata 标签各输出 Top N）。
分类部分依赖 Luau 的分配回调，Lua 5.5 后端会输出 `Categorized breakdown requires the Luau backend.` 后跳过。

!!! note "分类统计的是分配量，不是存活字节"
    `onfree` 回调不带大小，所以分类维度统计的是**分配量（churn）**。
    想知道存活多少、峰值多少，看报告的总量部分。

### vector 与 FVector

Luau 的 `vector` 是塞在 TValue 里的值类型（3 个 float）：运算不分配任何对象，
而 `FVector` 是带元表的 userdata，每次 `a + b` 都要新建一个 userdata 再被 GC 掉。
UnLuaNova 打通了这几条路：

```lua
local Native = vector.create(1, 2, 3)   -- 纯数学，零分配
local Sum = (Native + Native) * 2

local Vec = UE.FVector(Native)          -- vector -> FVector，不走三次 lua_tonumber
Vec:Set(vector.create(10, 20, 30))      -- 一次写入三个分量
local Back = Vec:ToVector()             -- 取回原生 vector（Lua 5.5 返回 nil）

-- UFUNCTION 的 FVector 参数可以直接收 vector
self:K2_SetActorLocation(vector.create(0, 0, 100), false, false)
```

反射路径（`FScriptStructPropertyDesc`）也认它：vector 没有元表，如果不显式开一个分支，
它会先被类型检查以「需要 userdata」为由拒掉。

!!! warning "精度：vector 是 float，FVector 在 UE5 是 double"
    UE 5 的 `FVector` 分量是 double，`vector` 是 float，往返一次会掉精度。
    数学上不敏感的用途（方向、速度、偏移）没问题；需要精确回读的场景仍用 `FVector`。

### buffer 与 TArray

`buffer` 库是 Luau 自带的（`UnLuaNova.HasFeature("buffer")` 为 true），可以和 `TArray` 互转：

```lua
local Bytes = UE.TArray(0)
Bytes:Add(1) Bytes:Add(2) Bytes:Add(3)
local Buf = UnLuaNova.Buffer.FromArray(Bytes)   -- TArray -> buffer
local Back = UnLuaNova.Buffer.ToArray(Buf)      -- buffer -> TArray
```

逐字节循环是 O(n)，适合一次性打包/解包，不要放进每帧的热路径。
`UnLuaNova.Vector` / `UnLuaNova.Buffer` 两个模块在两个后端下都能安全调用
（Lua 5.5 下 `HasVector()` / `HasBuffer()` 返回 false，转换函数返回 nil）。

---

## 三、类型声明与 luau-lsp

编辑器模块会为引擎类型生成 Luau 声明文件，供 `luau-lsp` 做静态检查 ——
把一部分 UE API 误用（函数名拼错、参数个数/类型不对）提前到"编译期"。

**生成**：UnLuaNova 工具栏 → 导出智能提示，产物落在

```
Plugins/UnLuaNova/IntelliSense/
├── EmmyLua/             Lua 方言，给 IDE 补全 + LuaLS
└── Luau/                Luau 方言，给 luau-lsp
    └── UnLuaNova.d.lua
```

!!! note "产物是 `.d.lua`，不是给脚本 require 的"
    它是 `declare class` / `declare function` 组成的纯声明文件，
    只在静态检查时被引用，运行时不要 `require` 它。

!!! note "Lua 后端用的是另一个分析器：LuaLS"
    同一份导出里，`.d.lua` 归 `luau-lsp`，`*.lua`（EmmyLua）归
    `lua-language-server`，对应脚本是 `Tools/CheckLuaTypes.ps1`。
    这不是偏好而是语法事实：Luau 的类型注解 Lua 5.5 解析不了，
    反过来 Lua 后端的脚本也喂不进 `luau-lsp`。
    `RunTests.ps1` 按后端分派，两个后端都会在报告里出一节「静态类型检查」。

!!! note "为什么是 luau-lsp，而不是 Luau 自带的 luau-analyze"
    上游把 `luau-analyze` 的 `--defs` 选项删了，声明文件喂不进去；缺了声明，
    每个 `UE.xxx` 都会被报成 Unknown global，报告全是假错。`luau-lsp analyze
    --definitions` 读的是同一份 `.d.lua`。它由 `Tools/Build-LuauLsp.ps1` 从
    `Tools/luau-lsp` 子模块构建（cmake + MSVC）。

**使用**：把 `UnLuaNova.d.lua` 放进脚本工程，脚本头部声明严格模式，然后用 `luau-lsp` 检查：

```lua
--!strict
local M = UnLuaNova.Class()

function M:ReceiveBeginPlay()
    self.Health = 100
    -- 下面这行会在 luau-lsp 阶段报错：UE.UWidgetBlueprintLibrary 没有这个方法
    local W = UE.UWidgetBlueprintLibrary.CreateWidget(self)
end

return M
```

```bash
luau-lsp analyze --platform=standard --definitions=@unluanova=Plugins/UnLuaNova/IntelliSense/Luau/UnLuaNova.d.lua Content/Script/
```

类型映射规则：整数各宽度与浮点统一为 `number`，`FString`/`FName`/`FText` 为 `string`，
`TArray<T>` → `{T}`，`TMap<K,V>` → `{[K]: V}`，`TSet<T>` → `{[T]: boolean}`，
结构体按 `USTRUCT` 名，委托等无法表达的类型退化为 `any`。

**一键检查**：`Tools/CheckLuauTypes.ps1` 把上面两步串起来 —— 找到 `luau-lsp`、
带上 `.d.lua`、对脚本目录跑一遍，有类型错误就以非零退出码结束：

```powershell
# 检查整个 Content/Script
.\Tools\CheckLuauTypes.ps1

# 只看 Luau 示例，严格模式
.\Tools\CheckLuauTypes.ps1 -Path Content/Script/Luau -Mode strict

# 指定 luau-lsp；找不到时就报错而不是跳过
.\Tools\CheckLuauTypes.ps1 -LuauLsp C:\Tools\luau-lsp.exe -Require
```

`luau-lsp` 本身不在仓库里（它是个可执行文件），脚本按这个顺序找：
`-LuauLsp` 参数 → 环境变量 `LUAU_LSP` → `PATH` → `Tools/luau-lsp` 子模块的 cmake 构建目录。
找不到时默认跳过并以 0 退出，所以可以放心挂到任何调度里；想让"没工具"也算失败就加 `-Require`。

!!! note "为什么不在 CI 里跑"
    GitHub 托管 runner 上没有 `luau-lsp`，也没有本地 UE 源码构建，
    所以类型检查是**本地步骤**；CI 只保证脚本本身语法正确。

---

## 四、与 Lua 5.5 的行为差异

写跨后端代码时最需要注意的几条：

| 项 | Luau | Lua 5.5 |
|---|---|---|
| `lua_Integer` | **32 位** | 64 位 |
| 64 位整数传递 | 绑定层走 `UNLUANOVA_PUSHINT64/TOINT64` | `lua_pushinteger` 直接够用 |
| userdata `__gc` | **不存在**（`UNLUANOVA_HAS_USERDATA_GC = 0`） | 存在 |
| 迭代元方法 | `__iter` | `__pairs` / `__ipairs` |
| `load()` | 原生没有，UnLuaNova 补 | 原生有 |
| `debug` 库 | 只有 traceback / info / profiler hooks，其余由 UnLuaNova 补 | 完整 |
| 沙盒 | `luaL_sandbox` | 无对应能力 |
| 断点 / 单步 | 原生 `lua_breakpoint` / `lua_singlestep` | 无对应能力 |
| 生成器 GC | 无分代模式，走 goal / step multiplier | `LUA_GCGEN` |

两条容易踩的实践结论：

1. **不要把资源释放寄托在 `__gc` 上。** Luau 没有 userdata `__gc`，
   容器（`TArray`/`TMap`/`TSet`）这类 userdata 在 Luau 下不会被终结器回收，
   生命周期请交给引用链或显式的释放逻辑。
2. **yield 边界更严格。** Luau 在跨过一层 C 调用边界后拒绝 `lua_yield()`，
   报 `attempt to yield across metamethod/C-call boundary`。
   从脚本里启动 Latent 函数没问题；从引擎回调里启动则可能失败，用 `UNLUANOVA_IS_YIELDABLE` 先问一句。

---

## 五、示例与测试

### 示例集

`Content/Script/Luau/` 下是按 Luau 特性组织的示例，通过 `Index.lua` 进入：

```lua
local Examples = require "Luau.Index"
if Examples.Available then
    Examples.StrictTyping:Run()
end
```

| 文件 | 演示什么 |
|---|---|
| `Index.lua` | 后端守卫：不是 Luau 就一个示例都不加载 |
| `01_StrictTyping.lua` | `--!strict`、类型注解、冻结的配置表 |
| `02_SandboxBoundary.lua` | 沙盒开启后什么能写、什么会被拦截 |
| `03_NativeDebug.lua` | 给 `lua.dbg` 下断点的目标文件 |
| `04_LuauSyntax.lua` | `continue`、复合赋值、`table.freeze/clone/clear`、`bit32` |
| `05_VectorInterop.lua` | `vector` ↔ `FVector` 的四条路径 |

!!! warning "这些文件在 Lua 5.5 后端下不能加载"
    它们用的是 Luau 语法（类型注解、`continue`、复合赋值），在 Lua 5.5 下是**编译期错误**。
    `Index.lua` 先问 `UnLuaNova.GetBackend()`，只有确认是 Luau 才 `require`；
    不要绕过 `Index.lua` 直接 require 其中任何一个。

### 测试

`LuaBackend.spec.cpp` 里新增了「Luau 专属能力」一组用例，按能力宏分支：

* 两端都跑：`GetBackend()` 报告正确后端、`HasFeature()` 与编译期能力一致、脚本改写 `UE` 命名空间被拦截
* 仅 Luau：Luau 语法能编译并求值、`vector` 进出 `FVector`、`UnLuaNova.Vector` 转换、`buffer` 读写、沙盒拦截全局写入、沙盒下类型注册仍可用、断点原语可用、分配回调能收到事件
* 仅 Lua 5.5：`ToVector()` 降级返回 `nil`

跑法与其他测试一样，注意后端是编译期选择，脚本会先按 `-Backend` 编译再跑：

```powershell
# 只跑 Luau 后端的这一组
.\Tools\RunTests.ps1 -Backend Luau -Filter UnLuaNova.API.LuaBackend

# 两个后端各跑一遍，各出一份报告（默认就是 Both）
.\Tools\RunTests.ps1 -Filter UnLuaNova.API.LuaBackend
```

---

## 六、官方特性接入（性能 / 沙盒 / 剖析）

Luau 不是"另一个 Lua 发行版"，它砍掉了东西，也加了 Lua 没有的东西。这一节是**沿着
luau.org 的 performance / sandbox / profile 三份文档把能接的接上**的记录，以及按我们自己
的需要补回去的库。全部走控制台变量，全部默认关闭或保持原值——它们都会改变语义或成本，
不该由插件替项目决定。

| CVar | 默认 | 作用 | 影响什么 |
|---|---|---|---|
| `luau.optimize` | 1 | 编译优化级别，2 = 内联 + 循环展开 | 之后编译的 chunk |
| `luau.debuglevel` | 1 | 0/1/2，行号与名字 | 之后编译的 chunk |
| `luau.coverage` | 0 | 覆盖率级别 | 之后编译的 chunk |
| `lua.codegen` | 0 | 把加载的 chunk 交给原生编译器 | 之后加载的 chunk |
| `lua.codegen.modules` | 空 | 只编译名字命中的模块，空 = 全部 | 同上 |
| `lua.gcgoal` / `lua.gcstepmul` | 200 / 200 | Luau 增量 GC 的两个旋钮 | 创建环境时 |
| `lua.gcstepsize` | 1 (KB) | 多久打断一次做一步 GC | 创建环境时 |
| `lua.sandbox` | 0 | 硬化全局表 | 创建环境时 |
| `lua.sandboxthread` | 0 | 每个协程拿到自己的全局表 | 创建环境时 |
| `lua.sandbox.freezeclasstables` | 0 | 类表注册即只读 | 每个类注册时 |
| `lua.io` | 0 | 装一个 rooted 的 io 库 | 创建环境时 |
| `lua.ioroot` | 空 = 项目目录 | io 能碰到的目录 | 每次路径解析 |

### 编译：`-O2` 与原生编译

`luau.optimize 2` 打开函数内联与循环展开。代价写在文档里也写在这里：**内联后的调用和
展开后的循环不再可观测**，`debug.traceback` 少帧、`lua_getlocal` 看不到被展开的迭代。
挂着断点调试时用 1，打包时用 2。

`lua.codegen 1` 是另一回事——Luau 的原生编译器。它不是追踪式 JIT：不采样、不猜测，只在
宿主明确要求时把指定函数（及其嵌套的一切）编译成机器码，唯一吃的提示是源码里的类型注解。
所以"编译这个模块"是项目能做的决定，而"猜哪个函数热了"是 VM 不会替你做的。配合
`lua.codegen.modules Hot,Foo` 可以只编译点名的模块，避免为所有脚本付编译时间。

`lua.codegen` 不带参数打印现状：能不能用（需要 x64/arm64 的后端）、开着没、编译了几个。

### GC：两个旋钮是一对的

Luau 没有分代模式，只有一个增量收集器，靠 G（目标）和 S（步进倍率）两个百分比调。它们
不是独立的：S 得追得上 G 放开的堆增长。lua.h 给的区间是
`S ∈ [100/(G-100), 100+100/(G-100))` 且不小于 150%，即 G=200 时 S 落在 `[150,200]`。

**这里原来写的是 100/100，那不是一个更严格的设置，是一个坏掉的设置**：G=100 时堆目标塌缩
成"堆 == 存活量"，而一轮 full GC 之后的阈值算下来是 `totalbytes * (G*S/100 - 100)/S`，
G=S=100 时等于 0（VM 会夹到 totalbytes，于是收集器刚结束一轮就立刻开下一轮）。现在默认
回到 Luau 自己的 200/200，并且配对越界时打一条警告——越界不是"多收集点"，而是收集器根本
追不到你设的目标。

### 沙盒：全局表与协程

`lua.sandbox` 硬化全局表；`lua.sandboxthread` 更进一步，**每个协程创建时**拿到自己的全局
表（读穿透到真全局，写留在自己这里）。挂点是 Luau 的 `userthread` 回调——插件自己从不调
`lua_newthread`，脚本里的协程全部来自 `coroutine.create`，所以宿主侧包一层根本拦不住。

它同时也是性能开关：Luau 的 imports / fastcall 优化只在"环境是纯的"这个前提下成立，
`lua_setsafeenv` 就是那个标记。跳过它的线程，VM 只能假定它的环境随时会变。

默认关，因为隔离是**可观测的**：协程里写的全局，外面读不到。

再往下一层是 `lua.sandbox.freezeclasstables`。`luaL_sandbox` 只硬化全局表，所以即使开了沙盒，
`UE.MyClass.Member = ...` 仍然随便写 —— 引擎绑定是可以被脚本改掉的。真正的只读是类表自己
只读。这件事以前做不了：类表是懒缓存的，首次访问一个属性会 `rawset` 进类表，冻结之后那次
写入直接抛错。现在改成**写不进去就跳过**（`GetFieldInternal` 里的 `CacheFieldIfWritable`）：
读路径不在乎有没有缓存，只是下次多解析一次。也是可选，因为 monkey-patch 类的脚本会因此失效。

### 补回的库：io 与 package

Luau 删掉 io 是有理由的（luau.org/sandbox：它能读文件、能起进程）。但那个理由针对的是
**不可信**代码；游戏自己的脚本是可信的，而一个读不了自己发布的数据文件的脚本层缺工具。

所以这不是"把 io 原样装回来"，而是把不安全的那部分换成项目自己做的决定：

* 可选（`lua.io`，默认 0）
* 有根（所有路径解析到 `lua.ioroot`，默认项目目录；绝对路径和 `..` 逃逸直接拒绝）
* 没有进程（没有 `io.popen`、没有 `os.execute`）
* 没有 stdin（`io.read` 直接报错）

文件对象是 Lua 表而不是 userdata：Luau **没有 `__gc`**，持有 `FString`/`TArray` 的 userdata
在脚本丢掉句柄时会泄漏。字符串和整数放进表里，跟着表一起回收。

`package` 补了 `preload`（并且真的生效——preload 查找器挂在 `package.searchers[1]`，和
Lua 一样优先于任何文件查找）、`config`、`path`、`cpath`。注意 `path`/`cpath` 只是**发布**
出来给脚本读的，UnLuaNova 自己的加载器不读它们；想要按路径解析，注册一个读它们的
searcher 就行——`package.searchers` 在两个后端都是真的。

### 剖析：Luau 自己的 profile.out

`lua.profiler trace on` 记采样，`lua.profiler trace dump [path]` 写成：

```
<微秒>  <source>,<name>,<linedefined>;<source>,<name>,<linedefined>;
```

这就是 `tools/perfgraph.py` 吃的格式——Luau 官方那个脚本已经随插件放在
`Tools/LuauPerfgraph/perfgraph.py`，所以编辑器里抓的 profile 可以直接用上游工具开火焰图，
不需要转换：

```powershell
python Tools\LuauPerfgraph\perfgraph.py --width 1200 Saved\Profiling\profile.out > profile.svg
```

默认约 1kHz 采样、20 万条封顶（越界自动停并告警）——每个安全点走一遍完整栈不是免费的。
GC 时间会带一个合成的 `GC,GC,0;` 帧，跟官方一致。

另外，协程是采样剖析器天生会搞错的东西：被中途丢掉的协程永远不返回，它的帧会一直压在共享
栈上，直到某个不相干的 return 把它弹掉。现在 `userthread` 回调会在线程销毁时清掉属于它的帧。

---

## 七、仍未完成的部分

| 项 | 现状 | 备注 |
|---|---|---|
| `buffer` ↔ `TArray<uint8>` 的零拷贝 | 目前是显式逐字节转换 | 真正的零拷贝要让容器绑定直接读写 buffer 内存，而不是先拷一份 |
| 容器 / FText / File 的 userdata 永不 `__gc` | Luau 明确不提供终结器（`LuaBackend.h:748`） | 只能靠显式 `Release`/重置；要么给容器加显式生命周期 API，要么别让它们持有需要析构的东西 |

已落地（原先在这个表里，现在不是了）：

| 项 | 变成什么 |
|---|---|
| 沙盒下冻结**类表** | `lua.sandbox.freezeclasstables`。阻塞点是"类表懒缓存首次访问会 `rawset`"——现在是写不进去就跳过（`CacheFieldIfWritable`），读照旧，只是下次多解析一次 |
| DAP 适配器 | `lua.dap start [port]`（默认 5699），见 [调试](Debugging.md) |

**不做的**（有意为之，不是没做）：

| 项 | 为什么不进 |
|---|---|
| `luau-lsp` 进 CI | 一次 cmake 构建几分钟，而 CI 拿不到引擎、也拿不到编辑器导出的声明文件，`UE.xxx` 全是 `Unknown global`，跑出来的东西要人工二次判读。它是本地工具：`Tools/Build-LuauLsp.ps1` 构建，`Tools/CheckLuauTypes.ps1` 跑 |

---

## 延伸阅读

- [设置项](Settings.md) —— 后端怎么选、子模块怎么初始化
- [功能清单](Features.md) —— 相对 UnLua 的增量里，哪些是 Luau 带来的
- [控制台命令](ConsoleCommand.md) —— `lua.dbg` / `lua.memstats` / `lua.profiler`
- [调试](Debugging.md) —— 断点调试与 IDE 接入
- [智能提示](IntelliSense.md) —— 声明文件与 IDE 补全
- [测试与自动化](Testing.md) —— 后端相关用例怎么跑

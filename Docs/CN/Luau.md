---
title: Luau 后端
description: UnLuaNova 的 Luau 后端：立即可用的语言特性、类型声明与 luau-analyze、沙盒、原生断点调试，以及与 Lua 5.5 的行为差异
---

# Luau 后端

[Luau](https://luau.org) 是 UnLuaNova 的**默认后端**，另一个后端是官方 Lua 5.5。
本文面向脚本作者：能用什么、不能用什么、以及哪些能力还没打通。

## 切换后端

在 `Config/DefaultUnLuaNovaEditor.ini` 里选择，然后**重新编译**：

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
| 类型注解、`--!strict` / `--!nonstrict` | 可用 | 配合生成的 `UnLuaNova.d.lua` 使用，见[类型声明](#三类型声明与-luau-analyze) |
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

## 三、类型声明与 luau-analyze

编辑器模块会为引擎类型生成 Luau 声明文件，供 `luau-analyze` 做静态检查 ——
把一部分 UE API 误用（函数名拼错、参数个数/类型不对）提前到"编译期"。

**生成**：UnLuaNova 工具栏 → 导出智能提示，产物落在

```
Plugins/UnLuaNova/IntelliSense/
├── EmmyLua/             Lua 方言，给 IDE 补全 + LuaLS
└── Luau/                Luau 方言，给 luau-analyze
    └── UnLuaNova.d.lua
```

!!! note "产物是 `.d.lua`，不是给脚本 require 的"
    它是 `declare class` / `declare function` 组成的纯声明文件，
    只在静态检查时被引用，运行时不要 `require` 它。

!!! note "Lua 后端用的是另一个分析器：LuaLS"
    同一份导出里，`.d.lua` 归 `luau-analyze`，`*.lua`（EmmyLua）归
    `lua-language-server`，对应脚本是 `Tools/CheckLuaTypes.ps1`。
    这不是偏好而是语法事实：Luau 的类型注解 Lua 5.5 解析不了，
    反过来 Lua 后端的脚本也喂不进 `luau-analyze`。
    `RunTests.ps1` 按后端分派，两个后端都会在报告里出一节「静态类型检查」。

**使用**：把 `UnLuaNova.d.lua` 放进脚本工程，脚本头部声明严格模式，然后用 `luau-analyze` 检查：

```lua
--!strict
local M = UnLuaNova.Class()

function M:ReceiveBeginPlay()
    self.Health = 100
    -- 下面这行会在 luau-analyze 阶段报错：UE.UWidgetBlueprintLibrary 没有这个方法
    local W = UE.UWidgetBlueprintLibrary.CreateWidget(self)
end

return M
```

```bash
luau-analyze --solver=V2 --settings=luau-settings.json Content/Script/
```

类型映射规则：整数各宽度与浮点统一为 `number`，`FString`/`FName`/`FText` 为 `string`，
`TArray<T>` → `{T}`，`TMap<K,V>` → `{[K]: V}`，`TSet<T>` → `{[T]: boolean}`，
结构体按 `USTRUCT` 名，委托等无法表达的类型退化为 `any`。

**一键检查**：`Tools/CheckLuauTypes.ps1` 把上面两步串起来 —— 找到 `luau-analyze`、
带上 `.d.lua`、对脚本目录跑一遍，有类型错误就以非零退出码结束：

```powershell
# 检查整个 Content/Script
.\Tools\CheckLuauTypes.ps1

# 只看 Luau 示例，严格模式
.\Tools\CheckLuauTypes.ps1 -Path Content/Script/Luau -Mode strict

# 指定 luau-analyze；找不到时就报错而不是跳过
.\Tools\CheckLuauTypes.ps1 -LuauAnalyze C:\Tools\luau-analyze.exe -Require
```

`luau-analyze` 本身不在仓库里（它是个可执行文件），脚本按这个顺序找：
`-LuauAnalyze` 参数 → 环境变量 `LUAU_ANALYZE` → `PATH` → Luau 子模块的 cmake 构建目录。
找不到时默认跳过并以 0 退出，所以可以放心挂到任何调度里；想让"没工具"也算失败就加 `-Require`。

!!! note "为什么不在 CI 里跑"
    GitHub 托管 runner 上没有 `luau-analyze`，也没有本地 UE 源码构建，
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

## 六、仍未完成的部分

| 项 | 现状 | 备注 |
|---|---|---|
| `buffer` ↔ `TArray<uint8>` 的零拷贝 | 目前是显式逐字节转换 | 真正的零拷贝要让容器绑定直接读写 buffer 内存 |
| 沙盒下冻结**类表** | 只冻结了全局表 + 拦截脚本写入 `UE` | 类表是懒缓存的（首次访问会 `rawset`），冻结会打断缓存 |
| `luau-analyze` 进 CI | 本地脚本已有，CI 没有可执行文件 | 有自托管 runner 就可以直接挂上去 |
| DAP 适配器 | `lua.dbg` 是 headless 的 | 能力已暴露，缺一个把它接到 IDE 的适配层 |

---

## 延伸阅读

- [设置项](Settings.md) —— 后端怎么选、子模块怎么初始化
- [功能清单](Features.md) —— 相对 UnLua 的增量里，哪些是 Luau 带来的
- [控制台命令](ConsoleCommand.md) —— `lua.dbg` / `lua.memstats` / `lua.profiler`
- [调试](Debugging.md) —— 断点调试与 IDE 接入
- [智能提示](IntelliSense.md) —— 声明文件与 IDE 补全
- [测试与自动化](Testing.md) —— 后端相关用例怎么跑

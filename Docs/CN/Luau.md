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
| `vector`（原生 vector3 值类型） | 库已注册，**UE 侧未打通** | 见[尚未打通的部分](#五尚未打通的部分) |
| `buffer` | 库已注册，**UE 侧未打通** | 同上 |
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

---

## 三、类型声明与 luau-analyze

编辑器模块会为引擎类型生成 Luau 声明文件，供 `luau-analyze` 做静态检查 ——
把一部分 UE API 误用（函数名拼错、参数个数/类型不对）提前到"编译期"。

**生成**：UnLuaNova 工具栏 → 导出智能提示，产物落在

```
Plugins/UnLuaNova/Intermediate/IntelliSense/
├── *.lua                EmmyLua 注解，给 IDE 补全用（见[智能提示](IntelliSense.md)）
└── UnLuaNova.d.lua      Luau 声明，给 luau-analyze 用
```

!!! note "产物是 `.d.lua`，不是给脚本 require 的"
    它是 `declare class` / `declare function` 组成的纯声明文件，
    只在静态检查时被引用，运行时不要 `require` 它。

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

!!! tip "类型检查工具链正在补齐"
    目前声明文件已经能生成，但仓库里还没有一键检查脚本。
    计划提供 `Tools/CheckLuauTypes.ps1` 把 `luau-analyze` 接到本地与 CI 流程里。

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

## 五、尚未打通的部分

Luau 的语言特性里，还有几块引擎侧没有用起来，正在按优先级推进：

| 项 | 现状 | 打通后能带来什么 |
|---|---|---|
| `vector` ↔ `FVector` | 库已可用，但 `FVector` 仍是 userdata | 原生 vector 是寄存器内的值类型，运算零分配，热路径上省掉一次 userdata 分配 |
| `buffer` ↔ `TArray<uint8>` | 库已可用，未与容器绑定 | 字节流读写免拷贝 |
| 沙盒下 freeze 类型表 | 沙盒只加固全局表 | 已注册的 UE 类型表一并冻结，改写直接失败，且 Luau 对冻结表有优化 |
| `luau-analyze` 工具链 | 声明可生成，无检查脚本 | 静态检查进 CI，UE API 误用提前暴露 |
| 示例与测试 | 示例全是纯 Lua 写法，spec 未覆盖 Luau 专属能力 | 见下 |

配套计划中的示例集（`Content/Script/Luau/`）会覆盖：严格类型写法、沙盒下的安全边界、
`lua.dbg` 断点命中、Luau 语法速览；测试侧会在 `LuaBackend.spec.cpp` 里按
`UNLUANOVA_HAS_SANDBOX` / `UNLUANOVA_HAS_BREAKPOINT` / `UNLUANOVA_HAS_ALLOC_CALLBACKS`
分支补上对应的用例。

---

## 延伸阅读

- [设置项](Settings.md) —— 后端怎么选、子模块怎么初始化
- [控制台命令](ConsoleCommand.md) —— `lua.dbg` / `lua.memstats` / `lua.profiler`
- [调试](Debugging.md) —— 断点调试与 IDE 接入
- [智能提示](IntelliSense.md) —— 声明文件与 IDE 补全
- [测试与自动化](Testing.md) —— 后端相关用例怎么跑

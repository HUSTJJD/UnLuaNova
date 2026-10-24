# 内置库

随插件发布的 Lua 模块，都在 `Content/Script/UnLuaNova/` 下，`require` 直接用：

```lua
local Timer = require('UnLuaNova.Timer')
local Event = require('UnLuaNova.Event')
```

## 边界：这里只放"机制"，不放"策略"

这是判断"该不该进内置库"的唯一标准：

| | 进内置库 | 不进 |
|---|---|---|
| 判断 | **不碰引擎/VM 就实现不对**，且每个项目写法都一样 | 项目有强意见，一半人会重写 |
| 例子 | 计时器（世界时间）、事件总线（广播中增删）、模块热重载 | UI 栈、场景流、HTTP、Model 层、网络协议 |

理由很实在：**一旦发布 `UnLuaNova.X`，就承诺了 API 稳定性**。改一次就是 breaking change，
而"免费送的功能"最容易被骂的正是"你为什么要改"。所以：

- 机制类：进。它们是"每个项目都要重写一遍，且都会写错"的部分
- 策略类：不进。写成示例工程的参考实现，或者做成独立的扩展插件

另一条：**不是什么东西都值得用 C++ 写**。跨语言调用的成本结构决定了 —— 回调还在 Lua，
你就省不掉那次跨界。下沉 C++ 只在命中下面任一条件时才成立：

1. **正确性只能由引擎给**（世界时间、暂停、时间膨胀、PIE 生命周期）
2. **规模大到 Lua 侧每帧扫表不可接受**（上万条）
3. **C++ 侧也要发/读**（跨语言互操作）
4. **需要零拷贝或特定内存布局**（容器、缓冲区）

`Timer` 命中 1 和 2，`Event` 一条都不命中 —— 所以下面是 C++，事件是 Lua。
**别为了高性能而 C++，为了正确性而 C++。**

## 插拔：所有特性同一种开关格式

全插件**只有一处**定义特性开关：`Config/DefaultUnLuaNova.ini` 的
`[/Script/UnLuaNova.UnLuaNovaSettings]`。规则固定三条：

1. **键名一律 `b<CamelCase>`**，查询名就是去掉 `b` 的小写形式（`bTimer` → `HasFeature('timer')`）
2. **一个入口**：脚本侧 `UnLuaNova.HasFeature('<名>')`，C++ 侧 `UnLuaNovaLib` 里那张表 —— 没有第二处
3. **大小写不敏感**：`oop` 与 `OOP` 同义，省掉"到底是大写还是小写"这种问题

```ini
[/Script/UnLuaNova.UnLuaNovaSettings]
bTimer=true                ; 内置库 UnLuaNova.Timer（改动需重编）
bEvent=true                ; 内置库 UnLuaNova.Event（改动需重编）
bOOP=true                  ; 内置库 UnLuaNova.OOP（改动需重编）
bDataDrivenBinding=false   ; 数据驱动绑定（默认关，见 Settings）
bDebugAdapter=true         ; lua.dap
DebugAdapterPort=5699      ; lua.dap 默认端口
```

**"编译期 vs 运行时"只有一条例外规则**：内置库（Timer / Event / OOP）关掉后代码不进二进制，
所以它们同时由 `UnLuaNova.Build.cs` 读**同一份 ini、同一批键名**生成宏
（`UNLUANOVA_HAS_STDLIB_*`）—— 不是另起一套 `bWithStdLibXxx`。其余特性全是运行时的。
宏只在 `UnLuaNova.Build.cs` 定义，`UnLuaNovaBase.h` 不写 `#ifndef` 兜底值：一份配置一个答案，
而且"忘记定义"应该表现为构建没跑，而不是悄悄把库打开。

关掉一个内置库会发生什么：

| 库类型 | 行为 |
|---|---|
| native（Timer） | 不注册加载器，`require` 直接失败 |
| Lua（Event / OOP） | 模块头自检，`require` 时明确报错"本构建关闭了该库"，而不是返回一个半残的表 |
| 全部 | `UnLuaNova.HasFeature('<名>')` 回答 false |

默认值随插件发布在 `Plugins/UnLuaNova/Config/DefaultUnLuaNova.ini`（写全并带注释），
工程侧 ini 只写覆盖项；`[需重编]` 的键要改就改**工程**那份（Build.cs 不读插件自带的 ini）。

后端能力（`sandbox` / `vector` / `buffer` / `breakpoint` / `allocCallbacks`）**不可配置** —— 它们由
后端决定，但对外走同一个 `HasFeature`，所以脚本侧看起来完全一致：不必知道这个值是从 ini 来的还是
从后端来的。

正确写法是问能力，不是猜：

```lua
if UnLuaNova.HasFeature('timer') then
    local Timer = require('UnLuaNova.Timer')
end
```

加一个特性 = 设置类里加一个 `b<Name>` + 表（`UnLuaNovaLib.cpp` 的 `Features[]`）里加一行。
**就这两处**，没有第三处。

## 后端一致性：所有模块都要在 Lua 与 Luau 上都能跑

后端是编译期二选一（两者导出同名 `lua_*` 符号、字节码不兼容，不能共存），所以每个内置库
都得同时过两个后端，而"在我这次编的后端上能用"不算能用。

Lua 模块（Event / OOP）的硬要求：

- 语法保持 **Lua 5.1 兼容**：不要 `continue`、不要复合赋值（`+=`）、不要类型标注
- 不要用某一后端独有的库函数；`_G.xxx` 取用而不是假设全局一定在
- 本仓库的用例会在两个后端各跑一遍（见[测试与自动化](Testing.md)），新增模块请顺手补上

native 模块（Timer）的硬要求：

- C API 只走 `LuaBackend.h` 的 shim，不要直接用某一后端的私有头
- 整数进出用 `UNLUANOVA_PUSHINT` / `UNLUANOVA_TOINT64` 这组宏：
  Luau 上 `lua_pushinteger` 推的是 double，直接写会在编译期静默截断
- 成功/失败只认"0 是成功"，不要依赖 `LUA_OK`（两个后端对它的定义不完全一致）

## Timer（C++ 驱动）

```lua
local Timer = require('UnLuaNova.Timer')

local Handle = Timer.Set(Callback, Interval, bLoop, Delay, bUnscaled)
Timer.Clear(Handle)
Timer.Pause(Handle, true)
Timer.IsValid(Handle)
Timer.ClearAll()
local Seconds = Timer.Time(bUnscaled)
local N = Timer.Count()
```

| 参数 | 说明 |
|---|---|
| `Callback` | 到点调用的函数，无参数。想传参数用闭包 |
| `Interval` | 秒。`0` 且 `bLoop` 为真 = 每帧 |
| `bLoop` | 循环（默认 false，即一次性） |
| `Delay` | 首次触发前再等几秒（默认 0） |
| `bUnscaled` | 用真实时间：**不受暂停与时间膨胀影响**（默认 false） |

`Set` 返回句柄，用 `Clear(Handle)` 取消。**回调不需要你自己保活**，句柄摘掉时引用一起释放。

### 为什么是 C++

脚本侧手写计时器几乎必然踩这四件事，而它们都不在 Lua 语法层面，在
"Lua 时间与引擎时间的关系"层面：

| 手写版本常见写法 | 问题 |
|---|---|
| `os.time()` 取时间 | 秒级精度，无视暂停与时间膨胀 |
| 累加 `DeltaTime` | 暂停、时间膨胀、PIE 重启全错 |
| `==` 判间隔 | 中间跳一帧就永远错过 |
| 遍历中增删定时器 | 索引错位，漏触发 |

本实现的处理：到期时间存在**世界时间**上（`GetTimeSeconds()` 受暂停与时间膨胀影响，
`GetRealTimeSeconds()` 不受），只比较不累加；到期项先快照再回调，回调里增删都不乱；
掉帧时用 `+= Interval` 追平并夹紧，不会越落越多。

与 latent 的 `Delay` 共用同一份世界时间源 —— 不要再开一个时间源，否则就是把
"世界暂停后回调不工作"那类 bug（Issue276）换个地方再犯一次。

### 已知限制

- **多世界 / 多 PIE**：脚本侧不传 World，由插件替你挑（优先 PIE / Game，再退回任意有效世界）。
  同时跑多个 PIE 时取的是第一个。真需要按世界区分时请提需求加 World 参数
- 回调里抛错只记日志，不会中断其余定时器
- `Pause(Handle, false)` 恢复时会把到期时间顺延一个周期，不会在恢复瞬间补发

## Event（纯 Lua）

```lua
local Event = require('UnLuaNova.Event')

local Token = Event.On("PlayerDead", Handler)        -- 订阅
local Token = Event.Once("PlayerDead", Handler)      -- 只触发一次
local Token = Event.On("PlayerDead", Handler, self)  -- 带 owner 订阅（推荐）

Event.Emit("PlayerDead", DamageCauser)               -- 广播，返回触发数量
Event.Off(Token)                                     -- 取消某一个
Event.Off("PlayerDead", Handler)                     -- 取消某事件下这个函数
Event.Off("PlayerDead")                              -- 清空这个事件
Event.UnbindOwner(self)                              -- 摘掉 owner 名下全部订阅
Event.Count("PlayerDead")
Event.Clear()                                        -- 全清（不传名就是全清）
```

它解决的不是"存一张表"（那太简单），是这四件事：

| 手写版本的问题 | 处理 |
|---|---|
| 广播中增删监听者，漏发或重发 | 先快照再逐个触发 |
| 一个监听者报错带崩整条广播 | 逐个 `pcall`，错误只记日志 |
| 监听者抓着已销毁的 UObject | `On(name, fn, owner)` + `UnbindOwner(owner)`；owner 索引是**弱键表**，不会把对象钉住 |
| 热重载后残留旧闭包 | 没有银弹；唯一可靠做法是**带 owner 注册 + 停用时 `UnbindOwner`**，所以这两个 API 是刚需 |

不带 owner 的订阅，热重载后旧闭包会留在总线上 —— 这是脚本方案里最常见的一类"越跑越卡、
还偶尔执行两遍"的成因。

## OOP（纯 Lua）

```lua
local OOP = require('UnLuaNova.OOP')

local Animal = OOP.Class("Animal")
function Animal:Ctor(Name) self.Name = Name end
function Animal:Tag() return "Animal" end

local Dog = OOP.Class("Dog", Animal)
function Dog:Ctor(Name) Dog.Super.Ctor(self, Name) end   -- 显式调父类构造
function Dog:Tag() return "Dog+" .. Dog.Super.Tag(self) end

local Rex = Dog:New("Rex")
OOP.Is(Rex, Animal)     -- true
OOP.IsClass(Dog)        -- true
OOP.NameOf(Rex)         -- "Dog"
print(Rex)              -- "instance of Dog"
```

| API | 说明 |
|---|---|
| `OOP.Class(Name)` / `OOP.Class(Name, Super)` / `OOP.Class(Super)` | 建类，单继承 |
| `Class:New(...)` | 建实例，**从基类到子类**依次调 `Ctor`，参数透传给每一层 |
| `Class.Super` | 父类；调父类实现要写 `Dog.Super.Ctor(self, ...)`（别写 `Dog.Super:Ctor()`，那会把父类当 self） |
| `OOP.Is(Instance, Class)` | 沿继承链判断 |
| `OOP.IsClass(Value)` / `OOP.NameOf(Value)` | 是不是类 / 类名（日志用） |

手写 class 最容易错的两处，本模块把它们钉死了：

- **构造链漏掉基类**：`New` 沿 `__Chain` 从基类到子类依次调 `Ctor`，中间某层没有 `Ctor` 也不打断
- **`Super:` 把父类当 self**：`Dog.Super:Tag()` 传进去的 self 是父类表，行为不对但不报错。
  正确写法是 `Dog.Super.Tag(self)`

**不提供**（有意的边界）：多继承 / mixin（用组合）；继承 UE 类型（那是 native 的
`UnLuaNova.Class()`，绑定 UCLASS 用的，两者不是一回事）；生命周期钩子（项目框架层的口味）。

## 其他内置模块

| 模块 | 说明 |
|---|---|
| `UnLuaNova.StringEx` | 字符串扩展 |
| `UnLuaNova.Vector` | `FVector` 的纯 Lua 算术（不跨界，热路径省调用开销） |
| `UnLuaNova.Buffer` | 缓冲区（Luau 后端有 native 实现，见 `UnLuaNova.HasFeature('buffer')`） |
| `UnLuaNova.Input` / `UnLuaNova.EnhancedInput` | 输入封装 |
| `UnLuaNova.HotReload` | 热重载脚本 |

可用性与能力一律走 `UnLuaNova.HasFeature`：

```lua
if UnLuaNova.HasFeature('timer')  then ... end
if UnLuaNova.HasFeature('event')  then ... end
if UnLuaNova.HasFeature('oop')    then ... end
if UnLuaNova.HasFeature('buffer') then ... end
if UnLuaNova.HasFeature('sandbox') then ... end
```

## 往里加一个模块要做什么

1. 先过一遍上面的"机制 vs 策略"和下沉 C++ 的四条判据，写在模块头注释里
2. Lua 模块放 `Content/Script/UnLuaNova/<Name>.lua`；语法保持 Lua 5.1 兼容
   （不要 `continue` / 复合赋值 / 类型标注）—— 两个后端都得能编
3. native 模块走 `FLuaEnv::AddBuiltInLoader`（`require` 的第四个 searcher 会命中它）
4. **要有 spec**，且如果号称"为了性能"，就把 Lua 版与 C++ 版都放进
   `UnLuaNova.Perf.Benchmark` 的 CSV —— 数字说值得才下沉（见[测试与自动化](Testing.md)）

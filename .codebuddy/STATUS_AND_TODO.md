# 现状与待办（2026-10-19 存档）

> 本文是**当前真实状态**的快照，用于中断后续接。所有结论都基于实测，不含推测。
> 相关文档：`PROJECT_PLAN.md`（立项与主规划）、`PORT_EXTERNAL_PLAN.md`（feature 方案）。

---

## 0. 快照

| 项 | 值 |
|---|---|
| 仓库 | `d:/learning/UnLua`，分支 `master` |
| HEAD | `6331944d feat: backend-independent string helpers` |
| 工作区 | 干净（0 未提交改动） |
| 远端 | `origin = HUSTJJD/UnLua`（已同步到 `36bb8291`）、`upstream = Tencent/UnLua`（**只读，勿推**） |
| 插件名 | `Plugins/UnLuaNova` / `UnLuaNovaExtensions` / `UnLuaNovaTestSuite` |
| 引擎基线 | UE 5.8.3（源码版，路径见 `PROJECT_PLAN.md` §8.1） |
| 后端 | **Lua 5.5.1**（子模块 `ThirdParty/Lua55`，**默认**）+ **Luau 0.743**（子模块 `ThirdParty/Luau`，opt-in） |
| 内部头依赖 | **0 个文件**（解耦完成） |

**后端布局**（Build.cs 一律放在子模块外，否则提交不进本项目）：

```
Source/ThirdParty/
├── Lua/        ← submodule github.com/lua/lua
├── LuaLib/     ← 本项目：Lua.Build.cs + LuaUnity.c + GenerateWrappers.py
├── Luau/       ← submodule github.com/luau-lang/luau
└── LuauLib/    ← 本项目：Luau.Build.cs + GenerateWrappers.py + Generated/
```

**两个后端都由 UBT 直接编译源码，不需要任何预构建步骤。**

这是比照引擎自身的做法定的（实测 `Engine/Source/ThirdParty`：188 个 Build.cs 中
180 个是 `ModuleType.External` 预编译，8 个由 UBT 编译源码，`nanosvg` 是后者的模板）。
**整个引擎 ThirdParty 没有一处 Build.cs 真正调用 CMake**——唯一出现 "cmake" 的是
`HarfBuzz.Build.cs:120` 的注释，指向外部脚本产物的目录布局。Epic 对需要预编译的库
走 `BuildThirdPartyLibs.bat`，而不是在 Build.cs 里跑外部构建系统。

对 Lua/Luau 这种纯源码、零外部依赖的库，UBT 编译是正确选择：不需要为每个平台×配置
维护二进制。

| 后端 | 编译单元 | 说明 |
|---|---|---|
| Lua | `LuaLib/LuaUnity.c`（1 个） | 上游有官方聚合 `onelua.c`，`#define MAKE_LIB` 后 include 即可 |
| Luau | `LuauLib/Generated/*.cpp`（57 个） | 无官方聚合，且 3 个文件级 `static` 符号重名，**不能** unity 合并 |

Luau 那 57 个 wrapper 由 `LuauLib/GenerateWrappers.py` 生成，每个只 `#include` 一个
Luau 源文件，使其保持独立翻译单元。冲突的三个名字是 `createmetatable`
（`lstrlib.cpp`/`lveclib.cpp`）、`period`（`TimeTrace.cpp`/`lperf.cpp`）、`run`
（`lapi.cpp`/`lgc.cpp`/`lvmload.cpp`）。

> `Luau.Build.cs` 里 `bUseUnity = false` 是**必需的**：若 UBT 把 wrapper 再合并成
> unity blob，上述冲突会重新出现。

Luau 子模块增删文件后，重新运行 `python LuauLib/GenerateWrappers.py` 并提交结果。

---

## 0.5 命名约定（**这两件事不是一个东西，别再改混**）

| 概念 | 当前值 | 决定因素 | 出现在哪 |
|---|---|---|---|
| **Lua 全局表名** | `Lua` | 纯 API 选择，只影响脚本书写长短 | `Lua.Log(...)`、`Lua.Class(...)`、`Lua.HotReload()` |
| **require 路径前缀** | `UnLuaNova.` | **必须是磁盘目录名** `Content/Script/UnLuaNova/` | `require("UnLuaNova.HotReload")` |

两者**故意不同**：改全局表名只是改一个 Lua 变量；改 require 前缀会让 Linux/Mac 直接找不到文件（大小写敏感）。

`UnLua` 和 `UnLuaNova` 作为**别名**同时注册，都指向 `Lua` 这张表，所以三种写法都能用。

> ⚠️ 重命名时千万别对 `UnLua` 做朴素字符串替换——`UnLuaNova` 含子串 `UnLua`，会把它变成 `LuaNova` 而悄悄破坏 require 路径。必须用单词边界 `\bUnLua\b`（`UnLuaNova` 里 `UnLua` 后面紧跟字母，边界不成立，天然安全）。

---

## 1. 已完成（按提交倒序）

| 提交 | 内容 |
|---|---|
| `6331944d` | F7 字符串扩展：`Content/Script/UnLua/StringEx.lua`，启动自动安装 |
| `4d487dd5` | F2 Lua 内存统计 `lua.memstats`；默认后端切回 Lua55；`Luau.Build.cs` 缺产物时明确报错 |
| `36bb8291` | **LuaCore.cpp 解耦**：userdata 标记层重写为纯公开 API，删掉最后一个内部头依赖 |
| `f7e615b0` | LuaEnv.cpp 解耦：GC / `lua_resume` / mainthread 收进 `LuaBackend.h` |
| `5e543f0c` | 修目录嵌套、删 vendored Lua 5.4.x、Build.cs 后端选择 |
| `f8bb5db8` | 修改名脚本重复替换 bug（`UnLuaNovauNova*`） |
| `f6352326` | 改名 UnLuauNova → UnLuaNova、vendor Lua 5.5.1、后端可切换 |
| `6daa6938`… | `LuaBackend.h` 建立（VM 差异唯一收敛点） |

更早：`ed1aea4c` 改名、`517645d2` 静态绑定层 + UE5.8 基线、11 个社区 PR 吸纳（`PROJECT_PLAN.md` §9）。

---

## 2. 存在的问题

### A. 阻塞级

**A1 · 整棵树从未编译验证过**
本机没有 `UnrealEditor-*.dll`，改名的 473 文件、解耦、双后端改造全部没有编译反馈。
按可疑度排序，最可能出问题的：

| 位置 | 风险点 |
|---|---|
| `LuaCore.cpp` `CalcUserdataPadding` | 改为**运行时探测** payload 起始偏移（原来是靠 `sizeof(UUdata)` 硬编码 40）。若 Lua 5.5 的 userdata 起始偏移不固定，padding 算错 → **16 对齐类型（FVector4/FQuat/FLinearColor）访问异常** |
| `LuaCore.cpp` `NewUserdataWithDesc` | 新布局 `[FUserdataDesc][Padding][payload]`，`alignas(8)` 的 `FUserdataDesc` 实际 sizeof 必须为 8 |
| `Lua55Lib/Lua55Unity.c` | `#define MAKE_LIB` + `#include "../Lua55/onelua.c"`，从未编译过 |
| `UnLuaNova.Build.cs` | 类名已改回 `UnLuaNova`；ini section 改为 `/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings` |

**A2 · Luau 后端目前不可编译**
`LuauLib/Luau.Build.cs` 是 External 模块，链接 `build/Release/Luau.VM.lib` 等，而 `ThirdParty/Luau/build/` **不存在**。
已加明确报错（告诉你该跑哪两条 CMake 命令），并已把默认后端切回 Lua55，所以**这不再阻塞主流程**。
要启用 Luau：先 CMake 构建 Luau，再 `Config/DefaultUnLuaNovaEditor.ini` 设 `LuaBackend=Luau`。

### B. 文档与代码实际不符

| 位置 | 问题 |
|---|---|
| `README.md:9` | 「默认 Lua 5.4，实验性支持 Luau」—— 5.4 已删除 |
| `README.md:19` | 「Lua 5.4.x（默认）／ Luau（实验，未接线）」 |
| `README.md:64` | 「Lua 虚拟机使用 Lua 5.4」 |
| `Docs/CN/Settings.md:111-113` | 「自定义Lua版本」整节已废弃：写的是 `lua-5.4.3`，并指向**已删除**的 `Source/ThirdParty/Lua`；`LuaVersion` 配置项已从 Build.cs 移除 |
| `README.md` | 未说明仓库含两个子模块，clone 需要 `--recurse-submodules`，否则 ThirdParty 是空目录 |
| `PROJECT_PLAN.md` | **存在两个 §11**（L802「解耦进展与一次失败」、L840「双后端决策」），编号重复 |

### C. 安全与遗留

- **备份 refs 仍含已清理的敏感文档**：`backup/master-before-filter`（分支）、`backup/pre-lua55-cleanup`（tag）。它们指向历史重写前的提交。**未推送过**（当时显式只推了 `master`），但任何 `git push --all` / `--tags` 都会把它们带上去。
- 本地 git 对象里仍保留着那份敏感文件的副本（因为被上述 refs 引用），`gc` 不会回收。

### D. 技术债与待决策

| # | 问题 | 说明 |
|---|---|---|
| ~~D1~~ | **已解决**（`7c6b509f` + `715affda`） | 见下方「已修复：标识符分裂」 |
| D2 | Luau 语义差异未处理 | 无 table `__gc`、无整数子类型、无 `__pairs/__ipairs`（用 `__iter`）、无字节码 `string.dump`、无 `io/os/loadfile/dofile`、`__eq` 对 rawequal 也触发、C 栈 200 层上限。`LuaBackend.h` 已有 `UNLUANOVA_HAS_*` 标记，但**还没有调用点使用它们** |
| D3 | `UnLuaTestSuite` 未运行 | 所有改造都未经回归测试。套件需编辑器环境（Session Frontend → Automation） |
| D4 | 旧规划进度未更新 | `PROJECT_PLAN.md` §2（PR 吸纳）、§3（Type Manifest）、§5（路线图 M0-M6）是早期写的，与当前状态有出入 |

---

## 2.5 已修复：标识符分裂（**编译阻塞，很可能就是之前编译失败的原因**）

改名脚本把一部分标识符写成了全大写 `UNLUANOVA`（宏风格），另一部分写成驼峰 `UnLuaNova`（模块名）。C++ 命名空间和类名**大小写敏感**，且没有任何别名兜底。

实测分裂规模：

| 形态 | 分裂情况 |
|---|---|
| `namespace UNLUANOVA` vs `namespace UnLuaNova` | `LuaEnv.cpp`、`ObjectRegistry.cpp`、`UnLuaNovaBase.cpp`、`UnLuaNovaLegacy.*`、`UnLuaNovaTemplate.h` 用前者，其头文件用后者 → **定义与声明不匹配** |
| `UNLUANOVA::` | 390 处 |
| `LogUNLUANOVA` vs `LogUnLuaNova` | 99 处 / 并存 |
| `#include "UNLUANOVA…h"` | 102 处，Windows 能解析，Linux/Mac 失败 |
| `FUNLUANOVAEditorToolbar` vs `FUnLuaNovaEditorToolbar` | 16 / 10 —— **同一个类两种写法** |
| `UNLUANOVAExport` vs `UnLuaNovaExport`（metadata 标签） | 4 / 1 —— 静态绑定生成器会漏掉一半类型 |
| Lua 全局表名 | C++ 注册 `UNLUANOVA`，Lua 脚本用 `UnLuaNova`（`EnhancedInput.lua:1`、`HotReload.lua:74/98`、`Input.lua:147`）→ 脚本拿到 nil |
| `FindPlugin(TEXT("UNLUANOVA"))` | `UnLuaNovaEditorToolbar.cpp:293`，插件名实为 `UnLuaNova` → 返回 nullptr |

**修复**（`715affda`，26 文件 / 852 处纯替换，insertions 与 deletions 完全对称）：

- `namespace UNLUANOVA` → `namespace UnLuaNova`
- `UNLUANOVA::` → `UnLuaNova::`
- `LogUNLUANOVA` → `LogUnLuaNova`
- `#include "UNLUANOVA…"` → `#include "UnLuaNova…"`
- `UNLUANOVA<CamelCase>` → `UnLuaNova<CamelCase>`（含 `F`/`U` 前缀的类名，因此**不能**用 `\b` 匹配）
- require 路径前缀 `UNLUANOVA.X` → `UnLuaNova.X`
- Lua 全局表名统一为 `UnLuaNova`

**刻意保留**（UBT 生成 / 编译期宏，改名会破坏编译）：
`UNLUANOVA_API`（77 处）、`UNLUANOVA_[A-Z]` 系列（226 处，如 `UNLUANOVA_ENABLE_*`、`UNLUANOVA_LUA_BACKEND`、`UNLUANOVA_HAS_*`）。

另：`Content/Script/UnLua/` → `Content/Script/UnLuaNova/`（`StringEx.lua` 已用 `git mv` 迁入，旧目录删除）。

---

## 3. TODO

### P0 · 编译验证（**一切的前提**）

- [ ] 编译当前树，拿报错回来修
- [ ] 优先验证 `CalcUserdataPadding`：写个小用例打印 `FVector4` userdata 的 payload 地址，确认按 16 对齐
- [ ] 跑 `UnLuaTestSuite`（Session Frontend → Automation），重点是容器与数学类型相关 spec
- [ ] 回退预案：`git revert 36bb8291`（只撤 LuaCore 解耦）/ `git reset --hard f7e615b0`（撤全部解耦）

### P1 · 文档同步

- [ ] `README.md` 三处 Lua 5.4 → Lua 5.5 + Luau 双后端
- [ ] `Docs/CN/Settings.md` 删除「自定义Lua版本」节，改写到后端切换（说明 `LuaBackend=Lua55|Luau`）
- [ ] README 加子模块 clone 说明
- [ ] `PROJECT_PLAN.md` 修 §11 重复编号（建议：L802 → §10.5，L840 保留为 §11）

### P2 · Feature（按 `PORT_EXTERNAL_PLAN.md`）

- [x] **F1 Lua CPU Profiler** —— `LuaProfiler.{h,cpp}`，Insights SpecId 缓存；Luau 走 `lua_Callbacks.interrupt` + `protoid`，Lua 5.5 走 `lua_sethook`。运行时开关 `lua.profiler`
- [x] **F6 Luau 类型声明生成** —— `UnLuaNovaLuauDecls.{h,cpp}`，输出 `UnLuaNova.d.lua` 供 `luau-analyze`
- [x] **F3 死循环检测** —— 新增 `LuaVmHooks.h` 分发器：VM 只有一个 hook 槽，Profiler 与看门狗原先互相覆盖（看门狗还会因 `lua_gethook()` 检查静默失效）。现两者注册到分发器共存；Luau 侧改走 `interrupt`，在循环回边触发，比原 `MASKLINE` 逐行回调更高效
- [x] **F5 沙盒** —— `luaL_sandbox` 经 `LuaBackend.h` 抽象，CVar `lua.sandbox` 默认关闭；卡在 `UELib::Open()` 之后调用（否则锁住 UnLua 自己正要写的全局表）。Lua 5.5 无对应能力，明确降级
- [x] **F4 headless 调试服务** —— `LuaDebugService.{h,cpp}` + `lua.dbg` 命令。Luau 的 `lua_callhook` 是独立于 `interrupt` 的通道，故不与 F3 冲突
- [x] **静态导出反注册** —— `UnExportClass()`；原析构只 `delete` 不从注册表摘除，热重载后 `FindExportedClass` 返回悬垂指针
- [x] **静态绑定优化（规模相关）** —— ① 读路径去锁：`FCriticalSection`→`FRWLock`，加 `PendingCount` 原子快判（原 `FindProp`/`FindFunc` 每次都跑 `ResolvePending`）；② 类型选择改 opt-out，删除 `-Types=` 命令行，项目只需给不想要的类加 `meta=(UnLuaNovaNoExport)`
- [x] ~~F8 vector/buffer 编组~~ —— **已取消**（2026-10-19 决策）。Luau 的 `vector` / `buffer` 是为 Roblox 设计的类型，不接入；UE 类型继续用 `MathLib/` 自封装，跨后端行为一致
- [x] ~~SkipObjectReferencer~~ —— **调研后丢弃**。源项目那个 UMG ListView EntryWidget 泄漏源于 2.1 老架构的强引用容器；2.3.6 的 `ObjectRegistry` 用弱表（`__mode="v"`），`AutoObjectReference` 只持有 `ULuaDelegateHandler` 且有清理路径，问题结构上不存在

### P3 · 清理与决策

- [ ] 删除备份 refs 并回收本地敏感对象（**建议确认编译无误后**）：
      `git branch -D backup/master-before-filter` / `git tag -d backup/pre-lua55-cleanup`
      `git reflog expire --expire=now --all && git gc --prune=now --aggressive`
- [ ] 决定 D1（目录改名 vs `__index` 前缀改名）
- [ ] 决定 `.codebuddy/` 是否随仓库分发（当前**不在** `.gitignore` 里，这三份文档会被提交）

---

## 4. 几条操作提醒

- **推送**：历史已重写过，之后每次推送都需要 `--force-with-lease`（不要裸 `--force`）。只推 `origin`，**绝不推 `upstream`**。
- **子模块**：改过 `.gitmodules` 后记得 `git submodule sync`；曾经出现过 `.git/config` 残留旧路径（`Plugins/UnLuauNova/...`）导致所有 git 命令静默失败的情况。
- **提交信息**：PowerShell 里多行 `-m` 会被拆成多个参数而报 `pathspec` 错误，**用单行 message**。
- **敏感信息**：涉及内部项目的名称、路径、SDK 名、密钥一律不写进仓库（含提交历史）。

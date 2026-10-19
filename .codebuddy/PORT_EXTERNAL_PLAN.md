# 基于 Luau 重写 feature 的方案

> 源项目（某内部商业 UE 项目）**只提供需求清单**，不提供实现。
> 实现全部基于 Luau v0.743 原生能力重新设计，源码证据见 §1。
> 源侧路径、项目名、内部插件名、私有 SDK 名均已脱敏，本文不引用任何源项目绝对路径。

---

## 0. 定位转变：从「移植」到「重写」

源项目那套东西基于 **Lua 5.3 + UnLua 2.1 老架构**，两边都不是同一代。它的真正价值是：

> **它证明了哪些能力在真实商业项目里被需要，以及需求优先级。** 这是最难 얻得的、花钱买不到的信息。

所以本文的做法是：**只抽取需求，实现全部重写**。带来的直接后果是，上一版方案里有几个结论必须翻转：

| Feature | 上一版（移植视角） | 本版（Luau 视角） |
|---|---|---|
| Lua 内存统计 | ❌ 丢弃（改核心 + 全栈回溯 + 云密钥） | ✅ **翻转纳入**，Luau 原生给分类信息，成本骤降 |
| Lua 调试器 | ❌ 丢弃（闭源 SDK + 5600 行 Slate） | ✅ **翻转为可行**，Luau 原生提供整条调试链；但 UI 形态要改 |
| stringy C 扩展 | ✅ 新增一个 C 扩展插件 | ⬇️ **降级**：`string.split` 已内建，其余纯 Luau 实现 |
| 死循环检测 | 不动（已有 `FDeadLoopCheck`） | 🔄 **改造**：用 `interrupt` 替掉 MASKLINE |
| vector / buffer 编组 | 无 | 🆕 **新增**：老项目给不了，Luau 独有的性能红利 |

---

## 1. Luau v0.743 原生能力清单（已核实源码）

| 能力 | API | 源码证据 | 能替代源项目的什么 |
|---|---|---|---|
| 安全点中断 | `lua_Callbacks.interrupt(L, gc)` | `lua.h:615`（注释：safepoints = loop back edges, call/ret, gc） | profiler 的 `sethook(CALL\|RET)`、死循环的 `MASKLINE` |
| 分配分类回调 | `onallocate(L, block, osize, nsize, memcat, tt, tag)` | `lua.h:630` | 源项目改核心分配器 + `lua_getstack` 全栈回溯 + MD5 |
| 释放回调 | `onfree(L, block)` | `lua.h:636` | 同上 |
| 原生断点 | `lua_breakpoint(L, funcidx, line, enabled)` | `lua.h:566` | 源项目的闭源调试 SDK |
| 原生单步 | `lua_singlestep(L, enabled)` | `lua.h:565` | 同上 |
| 断点命中查询 | `lua_atbreakpoint(L)` | `lua.h:569` | 同上 |
| 调试钩子注入 | `lua_callhook` + `lua_Debug::userdata` | `lua.h:552,587` | 同上 |
| 栈与变量访问 | `lua_stackdepth` / `lua_getargument` / `lua_getlocal` / `lua_getupvalue` | `lua.h:554-560` | 同上 |
| 覆盖率与计数器 | `lua_getcoverage` / `lua_getcounters` | `lua.h:594,602` | 源项目没有；可做行级热点 |
| **函数唯一标识** | `lua_Debug::protoid`（VM 内全局唯一，C 函数为 0）、`bytecodeid` | `lua.h:582-583` | 源项目手写的函数指针哈希桶 + 名称缓存 |
| userdata tag | `lua_newuserdatatagged` / `lua_setuserdatatag` / `LUA_UTAG_LIMIT=128` | `lua.h:219,373`；`luaconf.h:101` | UnLua 现在的 `USERDATA_MAGIC` 魔数 hack |
| 原生沙盒 | `luaL_sandbox` / `luaL_sandboxthread` | `lualib.h:161-162` | 源项目手写的路径映射 |
| vector 类型 | `LUA_TVECTOR` / `luaopen_vector` / `LUA_VECTOR_SIZE`(3\|4) / `LUA_VECTOR_DOUBLE` | `luaconf.h:136-142`；`lualib.h:152` | 源项目完全没有 |
| buffer 类型 | `luaopen_buffer` | `lualib.h:137` | 源项目完全没有 |
| 字符串 split | `string.split` 已内建 | `lstrlib.cpp:1838` | 源项目 stringy 的 split |

### 1.1 一条贯穿全部 feature 的设计原则

所有 feature **定义在 `ILuaBackend` 抽象层**（`PROJECT_PLAN.md` §4.1 / §11），由两个后端各自实现：

```
ILuaBackend::SetInterrupt(fn)        → Luau: lua_Callbacks.interrupt   | Lua55: lua_sethook(CALL|RET)
ILuaBackend::SetAllocCallbacks(a,f)  → Luau: onallocate/onfree(分类)   | Lua55: 自定义 allocator（仅总量）
ILuaBackend::SetBreakpoint(...)      → Luau: lua_breakpoint            | Lua55: 不支持（降级为无调试）
ILuaBackend::NewTaggedUserdata(...)  → Luau: lua_newuserdatatagged     | Lua55: user value 存 desc
ILuaBackend::NewExternalString(...)  → Luau: 不支持（用 buffer）        | Lua55: 外部字符串（5.5 新增）
```

**Feature 是公共的，红利是分侧的。** 两个后端各有一半的独有能力，都不炸。

> **后端只有两个**：Lua 5.5（`lua/lua` @ `v5.5.1`）与 Luau（@ 0.743），**都以子模块引入**。5.4.x / LuaJIT 等已全部排除。详见 `PROJECT_PLAN.md` §11。

---

## 2. Feature 重设计

### F1 · Lua Profiler

**需求**（源项目证明）：真机定位 Lua 热点，且不能常开。
**源项目做法**：`sethook(CALL|RET)` + 手写函数名缓存 + 指针哈希桶 + 尾调用修正 + SpecId 管理 ≈ 1300 行。

**基于 Luau 的新设计**：
- 用 `interrupt` 在安全点回调，**只做采样**（每 N 个安全点采一次），而不是每个 call/ret 都记事件 → 开销从"每调用两次 C 回调"降到采样率分之一
- 函数标识直接用 **`lua_Debug::protoid`**（VM 内全局唯一）→ **整删源项目那套名称缓存与指针哈希**
- **Luau 无尾调用 → 整删源项目的 `TailParentPtr` 尾调用修正**
- Insights SpecId 映射仍需缓存，但 key 从字符串换成 `protoid`
- 行级热点可直接用 `lua_getcoverage`（源项目没有）

净效果：源项目 1300 行里，大约 40% 的复杂度（名称缓存、哈希桶、尾调用修正）在 Luau 下**根本不需要存在**。

**落点**：`UnLuaNova/Private/Profiler/`，替换 `LuaEnv.cpp:46-76` 现有玩具 Hook。

---

### F2 · Lua 内存统计（结论翻转）

**需求**（源项目证明）：真机 Lua 内存增长定位。源项目为此专门建了云端上传链路 + 每次分配全栈回溯，**说明这个需求非常强烈**。
**源项目做法**：改核心分配器 → 每次分配 `lua_getstack` 全栈回溯 → MD5 去重 → 上传云。开销极大，且初始化处硬编码云密钥。

**基于 Luau 的新设计**：
- 直接挂 `onallocate` / `onfree`。**Luau 在回调参数里已经给了 `memcat`（内存类别）、`tt`（Lua 类型）、`tag`（userdata tag）** —— 零回溯就能做分类统计
- 统计维度：按 `memcat × tt × tag` 分桶，输出 Top N
- **不需要改核心、不需要栈回溯、不需要云、不需要任何凭据**
- 默认全关，仅 `-luamemstats` 或 STATS 开

成本从源项目的"高"降到"低"，且**安全性问题根本不会出现**。这是本次重写收益最大的一项。

**落点**：`UnLuaNova/Private/MemStats/` + 控制台命令 `lua.memstats`。

---

### F3 · 死循环检测（改造现有）

**需求**：真机/编辑器里 Lua 死循环要能断栈。
**源项目做法**：`MASKLINE` hook（每执行一行回调一次，5~20x 减速）+ 看门狗线程比对帧计数。
**本仓库现状**：`FDeadLoopCheck` 用的就是 `lua_sethook(L, OnLuaLineEvent, LUA_MASKLINE)` —— 一样慢。

**基于 Luau 的新设计**：`interrupt` **在循环回边就会触发**，而死循环天然是密集回边 —— 命中率极高，且完全不需要 LINE hook。把 `FGuard` 的语义改成"设置 interrupt 计数阈值 + 帧计数比对"即可。

**落点**：改 `Private/LuaDeadLoopCheck.cpp`。Lua 5.5 后端保留现有 `MASKLINE` 实现（5.5 无 interrupt 等价物）。

---

### F4 · 调试器（结论翻转，但排最后）

**需求**（源项目证明）：断点、单步、变量查看。源项目为此引入闭源 SDK + 5600 行 Slate UI，说明需求真实。
**源项目做法**：闭源调试 SDK（还拖入 Protobuf/curl/OpenSSL）+ 编辑器内 Slate UI + Windows 命名共享内存握手。

**基于 Luau 的新设计**：Luau 原生提供 `lua_breakpoint` / `lua_singlestep` / `lua_atbreakpoint` / `lua_callhook` / `lua_stackdepth` / `lua_getlocal` / `lua_getupvalue` —— **整条调试协议链都是原生的，闭源 SDK 完全不需要**。

但真正的成本不在协议，在 UI。源项目 5600 行 Slate 就是教训。建议：
- **做 headless 调试服务**（DAP 风格，走 TCP），UI 交给 VSCode 插件
- 不在 UE 编辑器里写 Slate 调试器
- 这样既满足需求，又避开源项目踩的最大的坑

**落点**：这是唯一值得新开的模块 `UnLuaNovaDebugger`（独立协议服务）。**排期放在 Luau 后端稳定之后。**

---

### F5 · 沙盒与热更

**需求**：真机热更、下载目录优先、文件时间戳。
**源项目做法**：手写 `GetRealFilePath`（自己折叠 `/../`、把 ProjectDir 前缀替换成 DownloadDir）+ Android `stat` 兜底。
**本仓库现状**：`LuaEnv.cpp:601-646 LoadFromFileSystem` **已有 DownloadDir 优先**，比源项目手写折叠更稳。

**基于 Luau 的新设计**：
- 路径映射继续用 UE `FPaths`（不搬源项目手写折叠）
- 新增 `luaL_sandbox` / `luaL_sandboxthread` —— **Luau 原生沙盒**，源项目完全没有这个层次
- ⚠️ **约束**：Luau 无字节码 `string.dump`，热更只能走源码。这不是缺陷，但要在文档里明确写清楚

**落点**：`LuaEnv.cpp` + `BaseLib/LuaLib_File.cpp` 补 Android 时间戳兜底（约 40 行）。

---

### F6 · 类型声明生成（直接做 Luau 版，不做 EmmyLua）

**需求**：脚本侧对 UE API 的补全与类型检查。
**源项目做法**：EmmyLua 注解，编辑器里遍历全部 UClass（体感卡顿的来源）。

**基于 Luau 的新设计**：直接生成 **Luau `.d.lua` 类型声明**，喂 `luau-analyze`，**把 UE API 误用从运行时前移到 CI**。这正是 `PROJECT_PLAN.md` §4.4 认定的核心卖点，也是源项目那套技术栈给不了的价值。

**落点**：扩展现有 `UnLuaIntelliSenseGenerator`，加一个 Luau 输出后端（与 EmmyLua 后端并列，可配置）。

---

### F7 · 字符串扩展（降级为纯 Luau）

**需求**：`split` / `strip` / `startswith` / `endswith` / `count` / `join`。
**源项目做法**：C 扩展模块（242 行）。

**基于 Luau 的新设计**：
- `string.split` **Luau 已内建**（`lstrlib.cpp:1838`）
- 其余用**纯 Luau 实现** —— Luau 解释器性能足够，不需要 C 扩展
- **省掉一个扩展插件**，`Plugins/UnLuaExtensions/` 保持不变

**落点**：`Plugins/UnLuaNova/Content/Script/UnLua/StringEx.lua`。

---

### F8 · vector / buffer 编组（新增，Luau 独有红利）

**需求**：源项目没有 —— 但这是 Luau 能给的最大性能红利，值得单列。

**设计**：
- `FVector` / `FVector4` / `FQuat` / `FLinearColor` → Luau 原生 `vector`，读值走 `lua_tovector`，省掉一次 userdata 解引用 + 元表查找 + `FProperty` 读
- `TArray<uint8>` → Luau `buffer`，零拷贝

⚠️ **必须先决策的取舍**：`LUA_VECTOR_SIZE` 是**编译期全局**的，只能 3 或 4 二选一（`luaconf.h:136-138`）。
- `=3` → `FVector` / `FRotator` 原生，`FQuat` / `FVector4` / `FLinearColor` 走老路径
- `=4` → `FQuat` / `FVector4` / `FLinearColor` 原生，`FVector` 需特殊处理

另外 UE5 默认 LWC，`FVector` 是 **double**，而 `LUA_VECTOR_DOUBLE` 默认 0（float）。要原生映射就得开 `LUA_VECTOR_DOUBLE=1`，会让所有 vector 变 double（体积翻倍）。**建议先不开，把 vector 编组列为 M5 之后的性能专项**，不要阻塞主线。

---

## 3. 排除项（重写视角下依然不做）

| 项 | 理由 |
|---|---|
| 源项目 `Source/UnLua/` 核心 fork | 2.1 老架构回退，且 `UnLuaCompatibility.h` 在 UE5.0/5.1 编译失败 |
| 源项目的蓝图 JSON 导出 | **需求已由 F6 覆盖且做得更好**（JSON 只是中间产物，终态是 `.d.lua`）。UE5 AssetRegistry 适配那部分工作量也省了 |
| 移动端性能指标上报（PerfDog 类） | 仅 Android/iOS，绑死多个业务模块，启动阻塞轮询最多 10 秒 |
| `lsqlite3` | 需先引入引擎 SQLite 模块；且 Luau 下 `luaL_Reg` 行为待验。等真实需求 |
| 编辑器内拉起外部 IDE | 纯私有生态（IDE 路径 / adb / 固定端口 / 手写 bat） |
| 手工埋点 Profiler | 被 F1 完全覆盖 |
| 简易加解密模块 | 内置硬编码默认密钥，不适合开源仓库 |

**保留作为需求参考但未立项的**：`SkipObjectReferencer`（UMG ListView EntryWidget 泄漏）、静态导出反注册（`Binding.cpp` 只增不减）。这两项是纯 UE 生命周期问题，与 Luau 无关，可独立做，不占 Luau 排期。

---

## 4. 前置依赖与执行顺序

**硬前置**：`PROJECT_PLAN.md` §4 阶段 0（解耦 Lua 内部头）+ 阶段 1（`ILuaBackend`）+ **§11 的双后端子模块改造**。
这些 feature 全部通过 §1.1 的抽象层访问 VM，**阶段 0 没做完就动手，等于又造一遍源项目那种"改核心才能用"的耦合**。

> ⚠️ 顺序不能颠倒：**先解耦，再切 Lua 5.5 子模块**（`PROJECT_PLAN.md` §11.4）。Lua 5.5 改了 Table 内部布局（更紧凑的数组），若先切版本再解耦，会同时面对「内部头 API 变化」和「抽象层缺失」两个问题。

```
① 阶段 0  解耦：UnLua 核心零 Lua 内部头依赖                    ← 硬前置
② 阶段 1  ILuaBackend + 双后端子模块（Lua 5.5 / Luau）         ← §11.3 改造清单
③ Lua 5.5 后端跑通 + UnLuaTestSuite 全绿                       ← 默认后端基线
④ Luau 后端跑通 + UnLuaTestSuite 全绿                          ← F1~F4/F8 的门槛
   │
批次 A —— 只依赖 Lua 5.5，可先做
  ├─ F5 沙盒 + 时间戳兜底  ~0.5d   （5.5 有全局变量声明可用）
  └─ F7 字符串扩展         ~1h     （纯 Lua，两后端通用）
   │
批次 B —— 吃 Luau 原生 API，成本低
  ├─ F2 内存统计          ~0.5d   （onallocate/onfree）
  └─ F3 死循环改造         ~0.5d   （interrupt 替 MASKLINE）
   │
批次 C —— 主力
  └─ F1 Profiler          ~2d     （interrupt 采样 + protoid）
   │
批次 D
  ├─ F6 Luau 类型声明      ~2d     ← 与 Type Manifest 主线同向，建议与 M2/M4 合并做
  └─ F8 vector/buffer      ~3d     ← 先定 LUA_VECTOR_SIZE，建议 M5 之后
   │
批次 E（独立，不占后端排期）
  ├─ SkipObjectReferencer  ~2h
  └─ 静态导出反注册         ~2h
   │
最后
  └─ F4 调试服务           ~1~2w   （headless DAP，UI 交 VSCode；仅 Luau 后端可用）
```

---

## 5. 净结果

| | 源项目 | 本方案 |
|---|---|---|
| 模块/插件数 | 15 个 uplugin 模块 + 8 个插件 | **新增 0 个**（F4 若做则 +1 个调试服务模块） |
| 需改 UnLua 核心 | 是（分配器埋点） | **否**（全部走公开回调 / 抽象层） |
| 外部依赖 | 闭源 SDK + Protobuf/curl/OpenSSL + 云 + 3.9MB framework | **无** |
| 硬编码凭据 | 有（云密钥 + 加密密钥） | **无** |
| 代码来源 | 1300+ 行移植 + 大量改造 | **重写**，且 Luau 让其中约 40% 复杂度不存在 |

---

## 6. 待确认

1. **`LUA_VECTOR_SIZE` 定 3 还是 4**？这决定 F8 的覆盖面，且是编译期全局、不可运行时切换。建议先不定，F8 排到 M5 之后。
2. **`LUA_VECTOR_DOUBLE` 要不要开**？开了能原生映射 UE5 LWC 的 double FVector，但所有 vector 体积翻倍。
3. **F4 调试器要不要做**？Luau 让协议层免费，但 headless 服务 + VSCode 插件仍是 1~2 周量级。若不做，至少要把 `ILuaBackend::SetBreakpoint` 留出来。
4. **`.codebuddy/` 是否随仓库分发**？若否，加一行 `.gitignore`。

---

## 附：开源化脱敏检查清单

- [ ] 源项目名、内部插件名、外部 IDE / 调试 SDK 名 —— **本文已脱敏**
- [ ] 任何指向源项目或本地引擎的绝对路径 —— **已清理**
- [ ] 云 API 密钥 / SecretId / SecretKey（源侧某模块初始化处有硬编码，**本文未引用其值**）
- [ ] 加解密默认密钥（源侧简易加密模块内硬编码，**本文未引用其值**）
- [ ] **内部员工账号名** —— 源侧代码注释里有多处署名，重写实现时**不得带入**
- [ ] `.codebuddy/` 是否加入 `.gitignore`

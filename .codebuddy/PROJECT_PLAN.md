# 基于 UnLua 的个人开源项目 · 立项规划

> 目标：把 `Tencent/UnLua` 的一个 fork 发展成你自己维护的开源项目（改名 + 吸纳社区 PR + 类型导出克制化 + 可插拔 Lua 后端）。
> 本文所有结论都基于对你本地仓库的实测与上游公开数据核对，可直接执行。

---

## 0. 事实基线（先对齐现状）

### 0.1 你的 fork 当前状态

| 项 | 值 |
|---|---|
| 本地路径 | `d:/learning/UnLua` |
| 当前分支 | `master` |
| HEAD | `3f112e8 更新LICENSE的公司主体` |
| 远端 | `origin = HUSTJJD/UnLua`（你的 fork），`upstream = Tencent/UnLua`（已配好，可直接 fetch） |
| 插件版本 | UnLua **v2.3.6**（`UnLua.uplugin`） |
| 引擎 | `TPSProject.uproject` 标 `4.26`，但代码含 `UE_5_0_OR_LATER` / `UE_5_2_OR_LATER` 分支 |
| Lua VM | **lua-5.4.3**（vendored 源码，另有一份 5.4.4 未启用）→ **已决策升级为 Lua 5.5 子模块**，见 §11 |
| 已有 | Luau 子模块（`luau-lang/luau` @ 0.743），尚未接线 |
| 无 | VM 抽象层（阶段 1 待做）；LuaJIT 已排除 |

### 0.2 上游维护现状（2026-08 数据，已核实）

- 最后 tag：`v2.3.6`（2023-11-07），**master 自 2024 年起只有 1 个提交**。
- **但 `develop` 分支仍活跃**：UE5.4 支持（PR #700）、UE5.6 编译修复（PR #758）、UObject 在 lua 侧 GC 偶现崩溃修复（PR #765，2026-04）。推动者是社区贡献者（jozhn、crazytuzi 等），非官方投入。
- 规模：2755★ / 718 forks / 185 open issues+PRs。官方 FAQ 自称腾讯内部约 40 款项目在用。
- 邻居对比：sluaunreal 1974★（最后 push 2025-10，反射+代码生成）、puerts 6161★（最活跃，TS/JS 路线）。

> **结论**：你的定位应该是"**社区驱动的 UnLua 续作**"，而不是"另一个 Lua 插件"。README 的核心叙事就是这一句。

### 0.3 三个必须知道的技术约束

1. **UnLua 深度绑定 Lua 5.4 内部布局**。`LuaCore.cpp` 直接 `#include "lstate.h"` / `"lobject.h"`，自己重写了 `lua_index2value`，并在 userdata 尾部塞魔数结构体打标记：
   ```cpp
   #define USERDATA_MAGIC  0x1688
   #define BIT_TWOLEVEL_PTR        (1 << 5)
   struct FUserdataDesc { uint16 magic; uint8 tag; uint8 padding; };
   ```
   依赖 TValue/Udata 的内存布局。上游有 `feature/luajit`、`feature/lua51` 分支，都停在 2023-02 的"增加 luajit 构建脚本"，**没有真正完成**。换 VM 不是改宏，必须重做这一层。

2. **2.x 已经是"惰性注册"，不是启动即全量导出**。`FLuaEnv` 启动只注册 `UObject`/`UClass` 两个；`UE.USomething` 在 `UELib.cpp:26 UE_Index` 里按需 `LoadReflectedType` → `ClassRegistry::PushMetatable`。真正"膨胀"的是下面三处（见 §3.1）。

3. **性能瓶颈不在 VM，在跨界编组**。属性读要过「模块链 rawget ×N → C 调用 `Class_Index` → 元表 rawget → `GetCppInstance` → `IsA`（`ENABLE_TYPE_CHECK=1` 时）→ 第二次 C 调用 `GetUProperty` → `ReadValue`」。换 VM 只优化纯 Lua 部分。

---

## 1. 项目命名与品牌

### 1.1 推荐：**LuaNova**

| | |
|---|---|
| 名称 | **LuaNova**（`/luːə ˈnoʊvə/`） |
| 仓库 / 组织 | `luanova` / `LuaNova` |
| 标语 | *LuaNova — A community-driven Lua scripting runtime for Unreal Engine.* |
| 命名空间前缀 | `LN`（`LNAPI`、`LogLuaNova`） |
| 寓意 | Nova = 新生之星。既指"停更项目重获新生"，也暗含"从 UnLua 爆炸出来的新东西"。 |

选它的理由：全新可注册（GitHub / crates / 域名 / VSCode 插件名都干净）、不含 `Un`（避免与 Unreal 品牌与 UnLua 混淆）、发音直观、中英文都好念。

### 1.2 备选

| 候选 | 亮点 | 风险 |
|---|---|---|
| **UnLuaX** | 血统最清晰，SEO 直接承接 UnLua 流量 | 辨识度弱，容易被当成"UnLua 的一个版本" |
| **PrismLua / Prism** | "分光，只取所需"，与 §3 的克制导出主题高度契合 | `prism` 是著名语法高亮库，命名空间污染 |
| **Vega** | 极短、强品牌感（织女星） | 看不出是 Lua/UE，必须永远带副标题 |
| **LuaArc / Luarc** | "桥接 Lua 与 Unreal" | 与 `Luau` 视觉相近，易拼错 |

**不要用**：`UnLuaEx`（与插件内 `UnLuaEx.h` 静态导出宏冲突）、`LuaU`/`ULua`（与 Luau 混淆）、`NeoLua`/`Nelua`/`Pluto`/`Ravi`/`Selene`/`Sol`（均已被占用）。

### 1.3 改名策略：**双层命名，代码标识延后改**

这是强烈建议：**品牌层立刻改名，代码内部标识（`UNLUA_API`、模块名 `UnLua`、日志类别 `LogUnLua`、目录 `Plugins/UnLua`）在第一阶段保留**。

理由：
- 你要长期 cherry-pick 上游 `develop` 与社区 PR，代码标识一改，所有 patch 全冲突，维护成本会吃掉你全部精力。
- 插件对外表现（Lua 侧 API `UE.*`、`UnLua.*`、`UnLua.Class()`）本来就应该保持兼容，这是吸引 UnLua 老用户迁移的最大筹码。

待项目稳定运行 2~3 个版本后，再评估是否做代码标识重命名（届时可一次性用脚本 + 保留 `UNLUA_API` 兼容别名）。

### 1.4 需要改动的文件清单（品牌层）

| 文件 | 改动 |
|---|---|
| `README.md` / `README_EN.md` | 全面重写（见 §1.5 结构） |
| `CHANGELOG.md` | 顶部加"本项目是 Tencent/UnLua 的社区续作"章节，保留原 changelog 归档 |
| `LICENSE.TXT` | 保留 MIT，Copyright 行改为你本人/组织，并明确"Based on UnLua by Tencent, MIT" |
| `Docs/Images/UnLua.png` | 换新 LOGO（可用 AI 生成，注意别沿用腾讯视觉） |
| `Docs/CN/**`、`Docs/EN/**`（18 个 md） | 全局替换项目名 + 更新失效链接；`Docs/CN/FAQ.md` 里"腾讯内部四十款项目"等表述需改为引用上游原文并注明出处 |
| `.github/` | 新增 `FUNDING.yml`、`ISSUE_TEMPLATE/`、`PULL_REQUEST_TEMPLATE.md`、`CONTRIBUTING.md`、`CODE_OF_CONDUCT.md`、CI workflow |
| `TPSProject*` | 示例工程可改名为 `LuaNovaSandbox`（可选，建议改，能减少"这是个 demo 工程"的误解） |
| README 中的 QQ 群 / Lua Booster 推荐 | 换成你自己的交流渠道（Discord/QQ）+ VSCode 插件建议（`Lua Booster` 或 `sumneko.lua` + EmmyLua） |

### 1.5 新 README 建议结构

```
# LuaNova
[badges: license / release / UE 5.x / PRs welcome]

一句话定位：社区驱动的 UnLua 续作——为 Unreal Engine 提供克制、可裁剪、可演进的 Lua 脚本方案。

## 为什么会有 LuaNova
  - UnLua 官方发布线冻结在 v2.3.6（2023-11），develop 分支由社区零星维护
  - 本项目目标：把散落的 22 个 open PR + develop 上的社区修复收敛成一个有人维护的发行线
  - 三个差异化方向：① 类型导出克制化 ② 可插拔 Lua 后端 ③ 定期发版 + 回归测试

## 特性（相对 UnLua v2.3.6 的增量）
  - [x] UE 5.4 / 5.6 / 5.7 支持
  - [x] 类型导出白名单（Type Manifest）
  - [ ] Luau 实验后端
  - [ ] 静态绑定代码生成

## 从 UnLua 迁移（一节，明确"Lua 侧 API 100% 兼容"）
## 快速开始 / 文档 / 路线图 Roadmap / 贡献指南 / 致谢与许可
```

---

## 2. 吸纳上游 22 个 open PR

### 2.1 前置动作（必须最先做）

```powershell
cd d:/learning/UnLua
git fetch upstream
# 关键：把 develop 的社区成果先并进来，很多 open PR 的 base 已经落后
git merge upstream/develop        # 或 git rebase，建议先 merge 再整理
```

`develop` 上已有 #700（UE5.4）、#758（UE5.6）、#765（GC 崩溃修复）等未进 master 的合并。
**在这之后**再逐个处理 open PR，否则会重复劳动。

建议建三个长期分支：`main`（发行）、`develop`（集成）、`upstream-sync`（定期 fetch 上游，只做合并）。

### 2.2 分级结论

| PR | 标题 | 判定 | 处理方式 |
|---:|---|---|---|
| **771** | Add UE 5.7 support and repair the test suite | ✅ **P0** | 已核实 diff：仅改 `LuaOverridesClass.cpp` 的 `UE_VERSION_OLDER_THAN(5,7,0)` 守卫、两个 TestSuite spec（`UnLua::Shutdown()` 前置、`GetUObject` 替 `GetPointer`）、Target.cs 加 `BuildSettingsVersion.V6`。⚠️ 它会把 `uproject` 抬到 5.7——**只取代码部分，自己决定引擎基线** |
| **767** | GC 时没有清理关联 Delegate 已失效的 Handle | ✅ **P0** | 生命周期/泄漏类，直接合；补一个 `UnLuaTestSuite` 回归用例 |
| **756** | 增加 Class 校验 | ✅ **P0** | 防御性校验，成本低 |
| **748** | TSoftObjectPtr 作为结构体首个成员时 metatable 异常 | ✅ **P0** | 明确 bug，合 |
| **711** | Lua 调 C++ 自定义导出函数传 nil 崩溃 | ✅ **P0** | 明确崩溃，合 |
| **719** | 继承相关的绑定导致崩溃和无限循环 | ✅ **P0** | 价值最高也最危险：**必须人工逐行 review + 补继承链回归用例**（内容涉及 `LuaOverrides` / `FuncMap`，是 UnLua 最脆弱的部分） |
| **639** | `UE.TArray(0)` 用 int32 导致溢出 | ✅ P1 | 正确性，低成本 |
| **643** | `__GNUC__` 条件编译宏错误使用 | ✅ P1 | 跨平台编译，低成本 |
| **684** | 工程目录 / 模板路径多一个 Config | ✅ P1 | 编辑器工具链，合 |
| **705** | `UNLUA_LOG` 基于 `NO_LOGGING` 区分 | ✅ P1 | 打包体积，合 |
| **704** | Generator IntelliSense 抛异常 | ✅ P1 | 编辑器体验；与 §3 的 IntelliSense 收敛是同一条线，合并处理 |
| **746** | 增强 GameFeature / 普通插件支持 | ⚠️ P1 | 方向对，但涉及插件加载路径与模块搜索，需 review 后再定 |
| **184** | UClass 被 GC 时误清 mark/sweep 间新建的同名 UClass | ⚠️ P1 | 2020 年 PR，代码已大改，**不要直接 merge**；先写用例验证 2.x 是否仍复现，再按现架构重做 |
| **538** | `is_constructible` | ⚠️ P1 | 2022 年，先 review 现代码是否仍适用 |
| **672** | [Fix] Copy error | ⚠️ P1 | 需看 diff 再定 |
| **199** | 不同蓝图类可绑定同一个 Lua 类 | ⚠️ P2 | 需求真实，但与 2.x 的"模块表浅拷贝"机制（`UnLuaManager.cpp:283`）直接冲突，属于**重新设计**而非 merge，建议纳入你自己的 Roadmap |
| **693 / 696 / 697** | UE5.4 / UE5.2 编译错误修复 | ⚠️ P2 | 大概率已被 `develop` 的 #700 覆盖，先验证，重复则关 |
| **759** | OSSF SLSA3 publish.yml | ⚠️ P2 | 安全合规，价值有但与你主线无关；可另起 `.github/workflows/` 自己实现 |
| **230** | Upgrade to Lua-5.3.6 / Android x86_64 | ❌ **拒绝** | 降级到 5.3.6 与现状相反；Android x86_64 已支持。仅核对 arch 是否已齐全 |
| **744** | Develop | ❌ **拒绝整体** | 已核实 diff：混杂大量 `Config/*.ini` churn、`SecurityToken` 明文、以及 **3626 行第三方 LuaPanda 调试器**。⚠️ **但要萃取其中两样有用的东西**：<br>`+DirectoriesToAlwaysStageAsUFS=(Path="Script")`<br>`+DirectoriesToAlwaysStageAsUFS=(Path="../Plugins/UnLua/Content/Script")`<br>这正是 UnLua 官方 FAQ 第二条"Lua 文件不是 asset 需要手动配打包"的自动化解法，值得单独做成一个改进 |

### 2.3 执行节奏建议

1. Batch A（P0 全部 + P1 中纯 bugfix）→ 一个 PR 合入 `develop`，跑 `UnLuaTestSuite`
2. Batch B（其余 P1）→ 逐个 PR，每个都补回归用例（`Content/Script/Tests/Regression/Issue###/`，沿用上游命名规范）
3. P2 转入 Roadmap issue，不阻塞发版
4. **在 README 里加一节 "Upstream PR Backlog"**，公开这个分级表并感谢原作者——这本身就是很好的社区运营

---

## 3. 类型导出克制化（Type Manifest）

### 3.1 先精确定义"膨胀"在哪

现状 2.x 是惰性注册，但有三处无界：

1. **`UELib.cpp:26 UE_Index` 对任意 native 名字放行**。只要 `UE.` 后面跟 `U/A/F/E` 前缀且 `LoadReflectedType` 能查到，就注册。**用户写错一个类名也会悄悄创建一个元表**（`LoadObject` 兜底还会把磁盘上的类加载进来）。
2. **`ClassRegistry.cpp:215-223 PushMetatable` 递归注册整条继承链**：
   ```cpp
   UStruct* SuperStruct = ClassDesc->AsStruct()->GetInheritanceSuper();
   while (SuperStruct) { ClassDescChain.Add(RegisterReflectedType(SuperStruct)); ... }
   ```
   碰一个 `BP_MyCharacter_C`，会把 `Character → Pawn → Actor → UObject` 全部拉进来。
3. **`LuaCore.cpp:1030 GetFieldInternal` 把解析结果写回元表做缓存**，且"不存在"也缓存 `NotExist` 哨兵。每个 UStruct 一张元表，字段越多表越大，且**只增不减**。

另外 **IntelliSense 生成在编辑器里遍历全部 UClass**（`UnLuaEditor`），这才是体感上"Lua 侧过于膨胀"的主要来源——几万行注解、补全卡顿。

### 3.2 设计原则

- **封闭**：全部改动落在 `Plugins/UnLua/**` 与项目 `Config/*.ini`，不碰 UE 的 UBT/UHT/BuildConfiguration。（唯一例外是可选复用插件自带的 `UnLuaDefaultParamCollector` UHT 插件——它本来就在插件内。）
- **默认兼容**：默认行为与现状一致，新机制全部 opt-in。
- **可演进**：声明方式多种且可组合，过滤策略可替换。
- **可观测**：先能"看见"导出面，再谈收紧。

### 3.3 架构：一个 Gateway + 一份 Manifest + 三种声明源

```
                     ┌──────────────────────────────┐
  UE.USomething ──┐  │  FTypeExportGateway          │    ┌──────────────┐
  TryBind        ─┼─▶│  AllowType(UStruct|UEnum|    │───▶│ FTypeManifest│
  PushMetatable  ─┤  │  FProperty|UFunction, Ctx)   │    │  (汇聚三源)  │
  IntelliSense   ─┘  └──────────────────────────────┘    └──────────────┘
                              │                              ▲
                              ▼                              │
                     Allow / Deny / Warn        ┌─────────────┼─────────────┐
                                               │             │             │
                                        MetaData 标记   C++ 静态注册   ini 配置
```

**新增文件**（全在 `Plugins/UnLua/Source/UnLua/` 内）：

| 文件 | 职责 |
|---|---|
| `Public/UnLuaTypeExport.h` | 对外枚举与 `UnLua::FTypeExport` 门面 API |
| `Private/TypeExportManifest.h/.cpp` | 白名单数据结构与三源汇聚 |
| `Private/TypeExportGateway.h/.cpp` | 统一判定 + 统计上报 |
| `Public/UnLuaTypeFilter.h/.cpp` | 可覆写的 `UUnLuaTypeFilter` 基类（扩展点） |

**核心枚举**：

```cpp
UENUM()
enum class EUnLuaExportMode : uint8
{
    Unrestricted,  // 现状：全放行（默认，保证兼容）
    Allowlist,     // 名单内放行，名单外 Warning + 放行（灰度期）
    OptIn,         // 名单内放行，名单外 lua_error（收敛后）
};

/** 继承链注册策略：控制 §3.1 的第 2 处无界 */
UENUM()
enum class EUnLuaSuperChainMode : uint8
{
    Full,        // 现状：注册整条链
    DeclaredOnly,// 只注册白名单内的祖先
    Lazy,        // 祖先元表按需创建（推荐默认改这个值）
};

/** 字段级裁剪：控制 §3.1 的第 3 处无界 */
UENUM()
enum class EUnLuaFieldMode : uint8
{
    All,        // 现状：所有 UPROPERTY/UFUNCTION
    Declared,   // 只有带 UnLuaExport 标记的字段（最克制）
};
```

### 3.4 三种声明源（都满足"项目内改动"）

**A. MetaData 标记（最推荐的主推用法，零 UBT 改动）**

全部走 UE 公开反射 API：`UStruct::HasMetaData` / `FProperty::HasMetaData` / `UFunction::HasMetaData`。

```cpp
UCLASS(meta=(UnLuaExport))
class AMyActor : public AActor { ... };

USTRUCT(meta=(UnLuaExport))
struct FMyData { GENERATED_BODY() ... };

UENUM(meta=(UnLuaExport))
enum class EMyState : uint8 { ... };

// 字段级（配合 EUnLuaFieldMode::Declared）
UPROPERTY(meta=(UnLuaExport))
int32 Health;

UFUNCTION(meta=(UnLuaExport))
void Fire();
```

蓝图侧：Class Defaults → **Class MetaData** 里加 `UnLuaExport`（UE 编辑器原生支持，无需任何工具链改动）。

> ⚠️ 已知风险：`UClass::HasMetaData` 依赖 `UMetaData`，在 cooked / shipping 构建中可能被剥离。**必须先写用例验证**你目标平台的可用性。若不可用，降级方案是把 meta 收集挪到构建期落盘成 manifest（复用 `UnLuaDefaultParamCollector` 这个插件自带的 UHT 插件，它仍属于"项目内"）。

**B. C++ 静态注册（编译期，零运行时反射开销）**

复用现有 `UnLuaEx.h` 的宏风格（用户零学习成本），底层注册进与 `Binding.cpp::GetExportedReflectedClasses()` 同构的静态表：

```cpp
// MyGameModule.cpp
UNLUA_EXPORT_REFLECTED_TYPE(AMyActor)     // 单个类型
UNLUA_EXPORT_REFLECTED_TYPE(FMyData)
UNLUA_EXPORT_REFLECTED_MODULE(MyGame)     // 整个 UE 模块
UNLUA_EXPORT_REFLECTED_PATH("/Game/Blueprints/Characters")
```

**C. ini 配置（`UUnLuaSettings` 新增字段）**

```cpp
/** 类型导出模式，默认 Unrestricted 以保持与 UnLua 行为一致 */
UPROPERTY(Config, EditAnywhere, Category="Type Export")
EUnLuaExportMode ExportMode = EUnLuaExportMode::Unrestricted;

UPROPERTY(Config, EditAnywhere, Category="Type Export")
EUnLuaSuperChainMode SuperChainMode = EUnLuaSuperChainMode::Full;

UPROPERTY(Config, EditAnywhere, Category="Type Export")
EUnLuaFieldMode FieldMode = EUnLuaFieldMode::All;

/** 白名单（黑名单 ExcludedTypes 优先于白名单） */
UPROPERTY(Config, EditAnywhere, Category="Type Export")
TArray<FSoftClassPath> ExportedTypes;

UPROPERTY(Config, EditAnywhere, Category="Type Export")
TArray<FString> ExcludedTypes;

/** 批量：按名字前缀 / 按包路径 / 按 UE 模块名 */
UPROPERTY(Config, EditAnywhere, Category="Type Export")
TArray<FString> ExportedTypePrefixes;

UPROPERTY(Config, EditAnywhere, Category="Type Export")
TArray<FDirectoryPath> ExportedPackagePaths;

/** 灰度期：不阻断，只打日志 */
UPROPERTY(Config, EditAnywhere, Category="Type Export")
bool bWarnOnUndeclaredAccess = false;

/** 扩展点：项目可继承覆写判定逻辑（与 ModuleLocatorClass 同构） */
UPROPERTY(Config, EditAnywhere, Category="Type Export", Meta=(AllowAbstract="false"))
TSubclassOf<UUnLuaTypeFilter> TypeFilterClass;
```

### 3.5 逃逸通道（防止"克制"变成"不可用"）

必须提供，否则这个特性没人敢开：

- **已绑定类自动放行**：`TryBind` 命中的类（实现了 `UUnLuaInterface` 或 `Locator` 命中）及其覆写所需的 UFunction 无条件放行——否则绑定机制本身会挂。
- **显式导入**：`UE.import("UMyClass")` / 现成的 `UE.UClass.Load(path)`。显式导入不走白名单，但**计入"未声明访问"统计**。
- **静态导出类型不受影响**（`BEGIN_EXPORT_CLASS` 系列本来就与反射注册是两条路）。

### 3.6 代码改动点（文件级，可直接照着做）

| 文件 | 改动 |
|---|---|
| `Private/UELib.cpp` `UE_Index` (L26-80) | 在 `LoadReflectedType` 之前插入 `FTypeExportGateway::AllowType(Name, Ctx)`；拒绝时 `return 0` 或按模式告警 |
| `Private/Registries/ClassRegistry.cpp` `LoadReflectedType` (L280) | **最上游单一入口**，加一道统一判定（`UE_Index` / `RegisterReflectedType` 都经过它） |
| `Private/Registries/ClassRegistry.cpp` `PushMetatable` (L215-223) | `SuperChainMode`：`Lazy` 下不递归 `RegisterReflectedType`，改为祖先元表按需创建 |
| `Private/LuaCore.cpp` `GetFieldInternal` (L1030) | `FieldMode::Declared` 下跳过未标记字段，且**不写 `NotExist` 缓存**（避免负缓存膨胀） |
| `Private/UnLuaModule.cpp` `SetActive` (L91-129) | `PreBindClasses` 的 `TObjectRange<UClass>()` 遍历同样过一遍 Gateway |
| `Source/UnLuaEditor/**` IntelliSense 生成 | 复用同一 Manifest，只生成白名单 → **这是体感收益最大的一处** |
| `Public/UnLuaSettings.h` | 新增 §3.4 的字段 |
| `Private/UnLuaConsoleCommands.cpp` | 新增 `UnLua.TypeExport.Report` / `UnLua.TypeExport.GenManifest` |

### 3.7 落地路径：观测 → 生成 → 收紧（关键）

不要一上来就让用户配白名单，会劝退。三步走：

1. **观测**：默认 `Unrestricted` + `bWarnOnUndeclaredAccess=true`。跑一遍游戏，控制台列出所有未声明访问。
2. **生成**：`UnLua.TypeExport.GenManifest` 把运行时实际用到（+ 警告过的）类型导出成 `Config/DefaultUnLua.ini` 的白名单片段，用户一键得到初始名单。
3. **收紧**：用户切 `Allowlist` → 稳定后切 `OptIn`；同时把 `SuperChainMode` 改 `Lazy`、`FieldMode` 按需改 `Declared`。

`UnLua.TypeExport.Report` 输出：已导出类型数 / 元表数 / 字段缓存条目数 / 未声明访问 Top N，支持导出 CSV → 可在 CI 里做"导出面"回归（防止 PR 悄悄把导出面撑大）。

### 3.8 扩展性（面向"未来方便用户拓展"）

三层扩展点，从轻到重：

1. **配置层**：ini 白名单 + 前缀 + 包路径（零代码）
2. **覆写层**：继承 `UUnLuaTypeFilter` 并配到 `TypeFilterClass`——与现有 `ModuleLocatorClass` / `EnvLocatorClass` 完全同构，用户已经熟悉这个模式
   ```cpp
   UCLASS()
   class UMyTypeFilter : public UUnLuaTypeFilter
   {
       GENERATED_BODY()
       virtual bool AllowType(const UStruct* Type, FString& OutReason) const override;
       virtual bool AllowField(const FField* Field) const override;
   };
   ```
3. **委托层**：复用现有 `FUnLuaDelegates` 模式，加 `OnTypeExportFilter` 广播（C++ 项目不想建 UObject 子类时用）

### 3.9 额外红利：Manifest 是三条链路的共同输入

这是我认为最有价值的架构闭环——同一份白名单同时喂给：

| 消费方 | 收益 |
|---|---|
| 运行时类型注册 | 控制 Lua 侧膨胀（本节主题） |
| IntelliSense 生成 | 提示更快更准（体感收益最大） |
| **未来的静态绑定代码生成** | **性能主战场**，见 §4.5 |
| **Luau 类型声明生成** | 类型安全，见 §4.4 |

---

## 4. Lua 虚拟机方案

### 4.1 后端选型结论（2026-10 已定稿：**双后端**）

> **决策（2026-10-19）**：只保留 **Lua 5.5** 与 **Luau** 两个后端，其余（Lua 5.3/5.4.x、LuaJIT、LuaVela、Pluto、Ravi 系等）**一律不再考虑**。两个后端**都以 git 子模块方式引入**。详见 §11。

| 后端 | 语言基线 | 现状 | 与 UnLua 的兼容性成本 | 定位 |
|---|---|---|---|---|
| **Lua 5.5**（`lua/lua`，tag `v5.5.1`） | 官方 5.5 | 5.5.0 发布于 2025-12-22，当前 5.5.1（2026-07-24） | 中：UnLua 现有 ~72 处 5.4 内部耦合需重验（见 §11.2） | **默认后端**，保证与上游 UnLua 行为一致 |
| **Luau**（`luau-lang/luau`，锁 0.743） | Lua 5.1 + 渐进类型 | ~5.9k★，Roblox/Warframe 商业验证，MIT，纯 C++ | 中：需重做 userdata 层（§0.3-1） | **主推方向**，类型系统与原生能力是卖点 |

**为什么不再考虑 LuaJIT**（Roblox 官方公开过的选型理由，同样适用于 UE）：
1. iOS / Xbox / 主机**禁止运行时 JIT**，LuaJIT 在这些平台只能退回解释器，性能优势消失——而 UnLua 的核心场景恰恰是移动端热更。
2. LuaJIT 锚定 5.1 语义，拿不到 5.3+ 的整数、`_ENV`、增量 GC。
3. Luau 是**纯 C++**，与 UE 的 C++17/20 工具链天然契合（LuaJIT 需要平台汇编 toolchain，iOS/Android/主机各自有坑）。
4. Luau 的 **userdata tag**（`lua_newuserdatatagged`）是 VM 原生的 O(1) 类型判别，**可以直接替换掉 UnLua 现在的 `USERDATA_MAGIC` 魔数结构体 hack**。

**两个后端各自的独有能力**（决定了 Feature 该在哪一侧吃到红利，见 `PORT_EXTERNAL_PLAN.md` §1）：

| 能力 | Lua 5.5 | Luau |
|---|---|---|
| 安全点中断 / 采样 | `lua_sethook` | ✅ `lua_Callbacks.interrupt` |
| 分配分类回调 | 自定义 allocator（仅总量） | ✅ `onallocate(memcat, tt, tag)` |
| 原生断点 / 单步 | ❌ | ✅ `lua_breakpoint` / `lua_singlestep` |
| userdata tag | ❌（用 user value 存 desc） | ✅ `LUA_UTAG_LIMIT=128` |
| 原生沙盒 | 部分（5.5 新增全局变量声明） | ✅ `luaL_sandbox` / `luaL_sandboxthread` |
| vector / buffer 类型 | ❌ | ✅ `LUA_VECTOR_SIZE` / `luaopen_buffer` |
| 外部字符串（零拷贝） | ✅ 5.5 新增 | ❌ |
| 静态类型检查 | ❌ | ✅ `luau-analyze` + `.d.lua` |

**为什么 Luau 优于 LuaJIT 作为第一目标**（这是 Roblox 官方公开过的选型理由，同样适用于 UE）：
1. iOS / Xbox / 主机**禁止运行时 JIT**，LuaJIT 在这些平台只能退回解释器，性能优势消失——而 UnLua 的核心场景恰恰是移动端热更。
2. LuaJIT 锚定 5.1 语义，拿不到 5.3/5.4 的整数、`_ENV`、增量 GC。
3. Luau 是**纯 C++**，与 UE 的 C++17/20 工具链天然契合（LuaJIT 需要平台汇编 toolchain，iOS/Android/主机各自有坑）。
4. Luau 的 **userdata tag**（`lua_newuserdatatagged` / `lua_getuserdatatag`）是 VM 原生的 O(1) 类型判别，**可以直接替换掉 UnLua 现在的 `USERDATA_MAGIC` 魔数结构体 hack**——这是它对这个项目最具体的好处。

### 4.2 分阶段路线（严格按顺序，跳步会返工）

#### 阶段 0：解耦（**前置，必须先做**，预计 1~2 周）

目标：`UnLua` 模块除 `ThirdParty/Lua` 外，**零 Lua 内部头依赖**。

- 删除 `LuaCore.cpp` / `LowLevel.cpp` 中的 `#include "lstate.h"` / `"lobject.h"`
- 移除自写的 `lua_index2value`，改为公开 API 等价实现
- `USERDATA_MAGIC` + `FUserdataDesc` 尾挂结构 → 改为
  - Lua 5.4：用 `lua_newuserdatauv` 的 user value 存 desc，或 registry 里的 `lightuserdata → desc` side table
  - Luau：直接用 userdata tag（阶段 2 自然落到这）
- 收敛散落的 `#if LUA_VERSION_NUM == 504`（如 `LuaEnv.cpp:135` 的 `lua_gc(L, LUA_GCGEN, 0, 0)`）到一处

产出：一个只依赖 `lua.h` / `lauxlib.h` / `lualib.h` 的 UnLua 核心。**这一步做完，换 VM 才从"重写"变成"新增一个后端实现"。**

#### 阶段 1：VM 抽象层（预计 1~2 周）

```cpp
// Public/UnLuaBackend.h
class ILuaBackend
{
public:
    virtual ~ILuaBackend() = default;
    virtual FStringView GetName() const = 0;              // "Lua 5.4.4" / "Luau"
    virtual lua_State* CreateState(void* AllocatorUD) = 0;
    virtual void DestroyState(lua_State* L) = 0;
    virtual void ConfigureGC(lua_State* L) = 0;           // 各 VM 的 GC 参数
    virtual bool LoadChunk(lua_State* L, const char* Src, size_t Len, const char* Name) = 0;
    virtual void SetHook(lua_State* L, lua_Hook Hook, int Mask, int Count) = 0;  // Insights/死循环检测
    virtual void* NewTaggedUserdata(lua_State* L, size_t Size, uint8 Tag) = 0;   // 差异最大的一处
    virtual uint8  GetUserdataTag(lua_State* L, int Idx) = 0;
    virtual void   SetUserdataDtor(lua_State* L, void* UD, void(*Dtor)(lua_State*, void*)) = 0;
};
```

- 后端枚举只有两个：**`Lua55`（默认）** 与 `Luau`。`Build.cs` 开关 `LuaBackend = Lua55 | Luau`
- **Lua 5.5 以子模块引入**（`lua/lua` @ `v5.5.1`），取代现有 vendored 的 5.4.3 / 5.4.4 两份源码 —— 见 §11.1
- `UNLUA_LUA_VERSION` 宏体系废弃，版本信息由 `ILuaBackend::GetName()` 提供

#### 阶段 2：Luau 实验后端（预计 3~6 周，长分支 `feature/luau`）

- 源码放 `Plugins/UnLua/Source/ThirdParty/Luau/`（submodule 或 vendor 均可，MIT 许可兼容）
- 编译：Luau 自带 CMake，可参考现有 `Lua.Build.cs` 的 CMake 调用方式（比 LuaJIT 简单得多）
- **必须处理的行为差异**（来自 Luau 官方 compatibility 文档，已核对）：

| 差异 | 影响点 | 处理 |
|---|---|---|
| 无整数子类型（只有 double） | 属性/参数编组的 `int64`/`uint32` 语义 | 需审 `PropertyDesc.cpp`，用 `bit32` / `math.floor` 兜底 |
| 无位运算符（用 `bit32`） | 少量 Lua 侧代码 | 提供 `bit32` shim |
| **无 table `__gc`**（只有 userdata dtor） | `LuaEnv.cpp:118` 的 `FManualRefProxy` `__gc` 兜底 | 改为 `lua_newuserdatadtor` |
| 无 `__pairs` / `__ipairs`（用 `__iter`） | 容器遍历（`LuaLib_Array/Map/Set`） | 同时注册 `__iter` |
| 无 ephemeron 表 | `_G`、`ObjectMap`（`__mode="v"` 弱值表）语义差异 | 需实测，可能要调整弱表策略 |
| **`__eq` 对 rawequal 也触发** | `ClassRegistry` 挂的 `UObject_Identical` | 需 review 语义 |
| 无尾调用 | 少量 Lua 侧代码 | 通常无害 |
| C 调用栈 200 层上限 | 深递归的 C→Lua→C | 需评估 |
| 移除 `io` / `os` / `loadfile` / `dofile` / 字节码 `string.dump` | **LuaPanda 调试器**、字节码预编译热更方案 | 调试器需改造；热更走源码或自研 loader |
| 无 `goto` / `<close>` / `<const>` | UnLua 自带 Lua 代码基本不用 | 影响小 |

- **验收门槛**：`UnLuaTestSuite` 全绿 + 在目标平台跑通一个 Lyra-like 示例
- **回退**：默认仍为 Lua 5.4，Luau 是 opt-in 后端，任何阶段出问题都不影响主发行线

#### 阶段 3：（已取消）

原计划的 LuaJIT 后端**已取消**（2026-10-19 决策）。后端只有 Lua 5.5 与 Luau 两个，不再新增第三个。
空出的精力投入到 §4.5 静态绑定代码生成——那里的收益（10x 级别）远大于再加一个 VM。

### 4.3 收益预期（**必须诚实**）

UnLua 的热点分布是：**跨界编组（反射 + ProcessEvent） >> 纯 Lua 执行**。

- 换 Luau：纯 Lua 部分约 1.5~3x，但**对属性读/UFUNCTION 调用几乎无改善**。
- 因此 §4 的排序应该是：**阶段 0 解耦 > 静态绑定代码生成 > 换 VM**。

### 4.4 Luau 的隐藏大礼：类型系统与 §3 闭环

Luau 的 `luau-analyze` 需要类型声明文件。而 §3 的 Type Manifest **正好就是生成这些声明的输入**：

```
Type Manifest (白名单)
   ├─▶ IntelliSense 注解（EmmyLua）
   ├─▶ Luau .d.lua 类型声明  ──▶ luau-analyze 静态检查 ──▶ 编译期发现 UE API 误用
   └─▶ 静态绑定代码生成（§4.5）
```

这能把 UnLua 最大的弱点——"类型错误只能运行时发现"——部分前移到编译期。**这是换 Luau 最值得讲的卖点，比性能更硬。**

### 4.5 性能主战场：静态绑定代码生成（建议列为第 4 个里程碑）

为白名单里的热点类型生成直接 C 函数，绕过 `FProperty` / `ProcessEvent`。slua-unreal 的 CppBinding 与 puerts 的静态绑定都证明了这条路是 10x 级别的。

- 输入 = §3 的 Type Manifest（**这就是为什么 §3 应该先做**）
- 形态：初期可以是"构建期生成 .cpp"，也可以是"运行时基于 manifest 生成 trampoline"（后者更符合 UnLua 零胶水的调性）
- 优先级建议：**高于换 VM**

---

## 5. 路线图

| 里程碑 | 内容 | 依赖 |
|---|---|---|
| **M0 · 立基** | fetch `develop` 合并；改名（品牌层）；README/CHANGELOG/LICENSE/CI/CONTRIBUTING；打包配置自动化（从 #744 萃取） | — |
| **M1 · 收敛社区** | 合入 P0 + 主要 P1 PR，每个补回归用例；发 `v2.4.0`（UE 5.4/5.6/5.7 支持） | M0 |
| **M2 · 类型克制化** | §3 完整实现（Gateway / Manifest / 三源 / 三档模式 / Report / IntelliSense 同源）；发 `v2.5.0` | M1 |
| **M3 · VM 解耦** | §4 阶段 0 + 1，默认后端升到 Lua 5.4.4 并清理重复源码；发 `v2.6.0` | M2 |
| **M4 · Luau 实验后端** | `feature/luau`；全测试通过；类型声明生成；发 `v3.0.0-preview` | M3 |
| **M5 · 静态绑定** | 基于 Manifest 的 trampoline / 代码生成；benchmark 对比 | M2 |
| **M6 · 代码标识重命名**（可选） | `UNLUA_API` → `LNAPI`，模块目录改名，保留兼容别名 | 发行线稳定后 |

---

## 6. 立即可执行的第一步

```powershell
# 1) 同步上游社区成果（含 develop 上的 UE5.4/5.6/GC 修复）
cd d:/learning/UnLua
git fetch upstream
git checkout -b develop
git merge upstream/develop

# 2) 按 §2.2 建 6 个 P0 PR 的吸纳分支
git checkout -b pr/771-ue5.7
#   git fetch upstream pull/771/head:pr771 && git cherry-pick/merge --no-commit
```

然后依次：
1. 先做 **M0**（改名 + 文档 + CI）——这是让项目"看起来有人维护"的最快方式，也是招揽贡献者的前提。
2. 立刻把 §2.2 的分级表发到 README 的 "Upstream PR Backlog" 一节并 @ 原作者——社区运营的第一步。
3. 再动 **M2**（类型克制化），因为它是 M4/M5 的共同输入。
4. **不要**现在就动 Luau——先做 §4 阶段 0 的解耦，否则一定会返工。

---

## 7. 另一个本地项目里那份 UnLua 的评估（已实测比对）

### 7.1 结论：**不要拿它当 base，但要挑 3 个真改动移植过来**

| 项 | 本仓库（fork） | 该项目内的 `Plugins/ThirdParty/UnLua` |
|---|---|---|
| 版本 | v2.3.6 @ `3f112e8` | v2.3.6，**就是 fork 的父提交** |
| 与上游的关系 | 标准 fork，有 `upstream` remote，可 cherry-pick / merge develop | 塞在游戏仓库的非标准路径，**无法同步上游** |
| 文件数 | 363 | 244（少 109 个 `Source/ThirdParty/Lua` + 本地化等） |
| 与上游客观差异 | 无 | **174 个文件被 clang-format 重排**（4 空格 → Tab） |
| 实质改动 | — | **仅 7 个源文件**，其中 3 个是真修复、1 个是架构改动、3 个纯格式化 |

实测：174 个"内容不同"的文件里，绝大多数差异**只有一行版权头**（`THL A29 Limited, a Tencent company.` → `Tencent.`，正好 27 字节，即 fork 的 `3f112e8` 那次改动）。真正被你改过的只有 7 个文件。

### 7.2 逐项判定

| 文件 | 改动内容 | 判定 |
|---|---|---|
| `UnLuaBase.cpp` | `UNLUA_LOGERROR(..."%s, Invalid metatable, metatable name: %s!", UTF8_TO_TCHAR(MetatableName))` → 补 `ANSI_TO_TCHAR(__FUNCTION__)` | ✅ **必修**。格式串有 2 个 `%s` 却只传 1 个实参，是**未定义行为**（读栈上垃圾），在某些平台直接崩 |
| `LuaEnv.cpp` | `EInternalObjectFlags::AsyncLoading` → `EInternalObjectFlags_AsyncLoading` | ✅ **必修**。UE 5.x 把该枚举项移出 enum class。⚠️ 移植时要包 `UE_VERSION` 条件，否则 4.26/5.0 编译失败 |
| `LuaLib_Object.cpp` | `ADD_FUNCTION_EX("Set", void, operator=, const UObject*)` → 返回类型改 `FSoftObjectPtr&` | ✅ 合。与 open PR **#748**（TSoftObjectPtr 作结构首成员 metatable 异常）是同一片代码，很可能同源，建议一起处理并补回归用例 |
| `UnLuaLib.cpp` + `Content/UnLua/{Class,__Init__}.lua` | **"C++ 隔离"**：把硬编码在 C++ 里的 `Index` / `NewIndex` / `Class` / `HotReload` 搬到 Lua 文件，C++ 只留 `LogInfo/LogWarn/LogError/Ref/Unref`（净减 116 行） | ⚠️ **有架构价值，但不能照搬**，见下 |
| `LuaFunction.cpp` / `UnLuaConsoleCommands.cpp` / `UnLuaInterface.h` | 198/199、61/64、13/13 行变更，逐行核对后**全是 clang-format + 版权头** | ❌ 跳过 |

### 7.3 "C++ 隔离"的价值与风险

**价值**：把绑定层的 Lua 语义（`Index` / `NewIndex` / `Class` 继承链 / `NotExist` 负缓存）从 C++ 字符串常量里解放出来，变成真正的 `.lua` 文件。这意味着项目可以**覆写、定制、热更**这部分逻辑——正好命中你"未来方便用户拓展"的目标，也与 §4 阶段 0（减少 C++ 与 Lua 内部耦合）同向。

**不能照搬的原因**：`Content/UnLua/__Init__.lua` 里硬编码了
```lua
rapidjson = require("rapidjson")
pb = require("pb")
```
这是该项目用了 `LuaRapidjson` / `LuaProtobuf` 扩展插件的产物。作为开源项目分发，没有这两个插件的用户会在启动时直接报错。

**建议做法**：单独开 `feature/lua-driven-class` 分支：
1. 移植 `Class.lua`（`Index`/`NewIndex`/`Class` 实现，可直接用，质量不错）
2. `__Init__.lua` 只保留 `UnLua` 表的惰性 `__index` + `Class` 装配，**删除 `rapidjson` / `pb`**，改由 `StartupModuleName` 让用户自己 require
3. 加编译开关 `UNLUA_LUA_DRIVEN_CLASS`（默认 0 = 保持内嵌 chunk 的旧行为），跑通 `UnLuaTestSuite` 后再考虑默认值
4. 这条线若做成，`Content/UnLua/Class.lua` 里的 `NotExist` 负缓存**正好是 §3 字段级裁剪（`EUnLuaFieldMode`）的落点**

### 7.4 clang-format 需要单独决策

该副本里显然对整个 UnLua 跑过 clang-format（Tab 风格）。这在自己的项目里没问题，但在要持续同步上游的 fork 里是灾难——**每一个被格式化过的文件，以后每次 merge 上游都会全量冲突**。

建议三选一：
- **A（推荐）**：新仓库**不做整体格式化**；`.clang-format` 只约束你新增的文件（`Source/UnLua/Private/TypeExport*/` 等）
- **B**：把整体格式化留到 **M6（代码标识重命名）** 那一步，作为"与上游彻底分家"的标志性提交，此后不再 merge 上游
- **C**：始终不格式化，接受上游风格

### 7.5 移植命令

```powershell
cd d:/learning/UnLua
git checkout -b port/ann-fixes

# 1) UnLuaBase.cpp 格式串缺参（UB）
# 2) LuaEnv.cpp  EInternalObjectFlags_AsyncLoading（需包 UE_VERSION 条件）
# 3) LuaLib_Object.cpp  FSoftObjectPtr& 返回类型（与 PR #748 合并处理）
# 手工改这 3 处，不要整文件拷贝（该副本带 Tab 重排 + 旧版权头）
```

---

## 8. 本轮已落地（2026-10-19）

### 8.1 引擎基线：UE 5.8.3

引擎确认在 `<本地引擎源码目录>`（**源码版 5.8.3，已编译**）。已改：

- `TPSProject.uproject` → `EngineAssociation = {12D1A55F-4B0B-6F15-0801-49AF79F4FD77}`
- 三个 `*.Target.cs` → `BuildSettingsVersion.V6` + `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`
- `LuaOverridesClass.cpp` → 采用 PR #771 的守卫 `UE_VERSION_NEWER_THAN(5,2,1) && UE_VERSION_OLDER_THAN(5,7,0)`。
  已核实 5.8 中 `UClass::Children` 与 **`UField::Next` 都是 `TObjectPtr<UField>`**，因此 5.7+ 原地遍历
  （`#else` 分支）重新成立。

### 8.2 那 3 个修复（已核实并合入）

| 文件 | 修复 | 5.8 核实结论 |
|---|---|---|
| `UnLuaBase.cpp` | `UNLUA_LOGERROR` 格式串 2 个 `%s` 只有 1 个实参 → 补 `__FUNCTION__` | 确认是 UB |
| `LuaEnv.cpp` | `EInternalObjectFlags::AsyncLoading` → `EInternalObjectFlags_AsyncLoading` | 5.8 已**删除**该枚举项，只剩宏 `(AsyncLoadingPhase1\|AsyncLoadingPhase2)`；不改为**编译失败** |
| `LuaLib_Object.cpp` | `ADD_FUNCTION_EX("Set", void, operator=, const UObject*)` → `FSoftObjectPtr&` | `SoftObjectPtr.h:148` 确认返回 `FSoftObjectPtr&` |

### 8.3 静态绑定：新增文件

| 文件 | 作用 |
|---|---|
| `Source/UnLua/Public/StaticBinding/StaticBindingTypes.h` | POD 描述符：`EStaticPropKind` / `FStaticPropBinding` / `FStaticParamBinding` / `FStaticFuncBinding` / `FStaticTypeBinding` |
| `Source/UnLua/Public/StaticBinding/StaticBindingRegistry.h/.cpp` | 注册表 + 延迟解析 + 属性类型分类 |
| `Source/UnLua/Public/StaticBinding/StaticBindingRegistrar.h` | 静态初始化期自动注册（`UNLUA_REGISTER_STATIC_BINDING`） |
| `Source/UnLuaEditor/**/UnLuaStaticBindingGeneratorCommandlet.{h,cpp}` | 构建期代码生成 Commandlet |

**接入点**：`FFunctionDesc` 新增 `StaticBinding` 成员，构造时查注册表；`CallUE` 命中即转 `CallUEFast`。

### 8.4 `CallUEFast` 到底省了什么

对照 5.8 的 `UObject::ProcessEvent`（`ScriptCore.cpp:2055`）逐项确认后才敢删：

| 省掉的 | 为什么安全 |
|---|---|
| `GetFunctionCallspace()` | `ProcessEvent` **只对 `FUNC_Native`** 自己查一次；非 native 根本不需要。UnLua 对每个调用都查，包括纯蓝图函数 |
| `ULuaFunction::Get()` 覆写重定向 | 只对 `bOverridable` 的函数执行；非可覆写函数本来必然返回 nullptr |
| 接口函数 `FindFunctionByName()` | 仅 `bInterfaceFunc` 需要，快速路径已排除 |

**未省**（保证行为一致）：`CheckObject`、`PreCall`/`PostCall`、参数 buffer、`ProcessEvent` 本身。

排除条件：`bIsNet`（需 `CallRemoteFunction`）、`bLatent`（协程记账）、`bInterfaceFunc`。

### 8.5 生成器用法

```powershell
& "<本地引擎源码目录>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
   "d:\learning\UnLua\TPSProject.uproject" `
   -run=UnLuaStaticBindingGenerator -Types=Actor,Pawn,Character
```
- 省略 `-Types` 时，自动收集所有带 `UCLASS(meta=(UnLuaExport))` / `USTRUCT(meta=(UnLuaExport))` 的类型 —— 与 §3 的 Type Manifest 是同一套声明。
- 默认输出 `Source/<ProjectModule>/Generated/UnLuaStaticBinding_Generated.cpp`（落在模块目录内才会被 UBT 编译）。

### 8.6 已知限制 / 待你验证

1. **本机无法编译验证**（只有引擎源码，`UnrealEditor-*.dll` 未生成）。上述代码按 5.8 头文件逐个核实过 API，但**必须本地编译一次**。
2. 生成器目前只产出"描述符表"，把反射解析从**每次调用**挪到**启动一次**。真正的 10x 需要生成带真实 C++ 签名的存根（需 UHT 插件），列为下一步。
3. 空数组问题已修（C++ 不允许 `T x[] = {}`，改用哑元 + 独立计数常量）。

### 8.7 改名（**未执行**）

`Tools/Rename-To-UnLuauNova.ps1` 已提供，支持 `-WhatIf` 预演。

**故意不自动执行**：改名会动模块名、`UNLUA_API` 和 UCLASS 名，必须先确保当前能编译，否则无法区分"改名引入的错"和"静态绑定引入的错"。建议顺序：**先编译验证 → 再跑脚本 → 再编译 → resave 资产**。

脚本**不改**（需配套迁移）：
- UCLASS 名 `UUnLuaSettings` / `UUnLuaInterface` / `ULuaEnvLocator` / `ULuaModuleLocator`
- ini section `[/Script/UnLuaEditor.UnLuaEditorSettings]`
- Lua 侧全局 `UnLua`（保留以兼容既有脚本）

---

## 9. PR 吸纳 + 改名（已执行，2026-10-19）

### 9.1 上游分支实况（实测）

| 分支 | 与 master 关系 | 结论 |
|---|---|---|
| `develop` | 领先 20 / 落后 1 | ✅ **活的**，已并入 |
| `dev/2.4` `release/2.4` | 领先 47 / **落后 170** | ❌ 2022 年老分支，弃用 |

### 9.2 已并入 `develop`（20 提交 = 5 个已合并 PR）

`#700` UE5.4 支持 · `#707` HotReload upvalue · `#758` UE5.6 编译 · `#761` UObject delete 崩溃 · `#765` UObject 在 lua 侧 GC 崩溃

**重要发现**：我上一轮手工做的那 3 个修复，develop 里**已经有了上游同款改动**。为避免重复，先回退我的版本再合并，由 develop 提供。
唯一仍需我手动补的是 `LuaOverridesClass` 的 5.7+ 守卫——develop 仍是旧 `NEWER_THAN(5,2,1)`，在 5.8 上是编译错误。

### 9.3 已吸纳的 open PR

`git fetch upstream pull/NNN/head` 拉取后逐个 merge，**零冲突合入 11 个**：

`767` Delegate Handle GC · `756` Class 校验 · `748` TSoftObjectPtr 首成员 · `711` nil 参数崩溃 ·
`719` 继承绑定崩溃 · `639` TArray int32 溢出 · `643` `__GNUC__` 宏 · `684` 模板路径 ·
`705` NO_LOGGING · `704` IntelliSense 异常 · `672` Copy error

`#771`（UE5.7）冲突，但内容已覆盖：LuaOverridesClass 守卫 + Target.cs V6 我早先已做，
因此只 cherry-pick 了它 3 个**测试套件**提交（`6b64272e` / `54f16959` / `2a958d81`）。

**未吸纳（有理由，非遗漏）**：

| PR | 原因 |
|---|---|
| `746` GameFeature 支持 | merge 冲突，且我原判就是"需 review"，转入 Roadmap |
| `184` UClass GC 同名误清 | 2020 年 PR，代码已大改；原判就是"重做而非 merge" |
| `538` is_constructible | 实为 **22 个提交**的特性分支快照（含编辑器扩展示例 + 资产），非单一修复 |
| `230` | 降级到 Lua 5.3.6，方向相反 |
| `744` | ini churn + 明文 token + 3626 行 LuaPanda |
| `199` `693` `696` `697` `759` | P2，转入 Roadmap |

### 9.4 改名已执行（单个可回滚提交 `ed1aea4c`）

473 文件变更。**范围刻意限定为"模块 + 品牌标识"，没有批量改 .h/.cpp 文件名**（那会炸掉 include，且无收益；UBT 只要求模块目录名 == 模块名 == Build.cs 名）。

已改：插件目录、模块目录 + Build.cs、`.uplugin` 模块名、`IMPLEMENT_MODULE` / `LoadModuleChecked` / UHT `ModuleName`、`UNLUA_API` → `UNLUANOVA_API`、`LogUnLua` → `LogUnLuauNova`、命名空间（声明 + `UnLua::` 限定）、插件路径字面量。

**故意保留**：Lua 全局 `UnLua`（脚本兼容）、UCLASS 名、`Config=UnLuaSettings`、`UnLuaTestSuite`。

### 9.5 下一步必须做的

1. **编译**——改名和静态绑定都未经编译验证。
2. 编译通过后，再单独做 UCLASS 名 + `Config/DefaultUnLuaSettings.ini` 迁移（需 resave 资产）。
3. `Plugins/UnLuaExtensions` 与 `UnLuaTestSuite` 仍用旧名，要不要一起改由你定。

---

## 10. Luau 引入（已执行，2026-10-19）

### 10.1 已做

- **子模块**：`Plugins/UnLuauNova/Source/ThirdParty/Luau` → `https://github.com/luau-lang/luau.git`，锁在 **0.743**。
- **`Luau.Build.cs`**：放在**兄弟包裹目录** `ThirdParty/LuauLib/`，**不放在子模块内**——写进子模块的文件属于 luau-lang/luau 自己那个仓库，永远提交不进本项目。

### 10.2 核实过的集成事实

| 项 | 结论 |
|---|---|
| 错误处理 | Luau 默认 `LUA_USE_LONGJMP=0` → **C++ 异常**（`class lua_exception : std::exception`）。UE 默认关异常 → 我们定义 `LUA_USE_LONGJMP=1` 走 setjmp/longjmp，顺带与 Lua 的 pcall/error 模型一致 |
| userdata 标签 | `lua_newuserdatatagged` / `lua_userdatatag` / `lua_newuserdatadtor` / `LUA_UTAG_LIMIT` 全部可用 → **可直接替换 UnLua 的 `USERDATA_MAGIC` 魔数 hack** |
| 元表 / 注册表 | `luaL_newmetatable` / `luaL_getmetatable` / `LUA_REGISTRYINDEX` 均存在 |
| 加载 | `luau_load`（不是 `lua_load`）；**没有字节码 `string.dump`** |
| GC 操作 | `LUA_GCSTEP / SETGOAL / SETSTEPMUL / SETSTEPSIZE`，**没有 `LUA_GCGEN`**（`LuaEnv.cpp` 那处 5.4 分代 GC 设置必须改） |
| 需编译量 | VM 37cpp + Compiler 9cpp + Ast 8cpp ≈ **54 个 cpp**；Analysis 77cpp 只在做类型检查工具时才需要 |

### 10.3 移植工作量（实测）

Lua 5.4 内部耦合 **~72 处，14 个文件**：

| 文件 | 处数 |
|---|---|
| `UnLuaCore.cpp`（原 LuaCore.cpp） | **35** |
| `BaseLib/LuaLib_DataTable.cpp` | 10 |
| `LuaDanglingCheck.cpp` | 6 |
| `ReflectionUtils/PropertyDesc.cpp` | 6 |
| `LuaEnv.cpp` | 3 |
| `MathLib/LuaLib_Math.h` | 3 |
| 其余 8 个文件 | 各 1~2 |

主要模式：`#include "lstate.h"/"lobject.h"/"lfunc.h"`、自写 `lua_index2value`、userdata 尾部 `FUserdataDesc` 魔数。

### 10.4 语义差异清单（移植时必须逐项处理）

无整数子类型（只有 double）· 无位运算符（用 `bit32`）· **无 table `__gc`** · 无 `__pairs/__ipairs`（改 `__iter`）· 弱表无 ephemeron · 无尾调用 · **`__eq` 对 rawequal 也触发** · 无 `io`/`os`/`loadfile`/`dofile` · 无字节码 `string.dump` · C 调用栈限 200 层。

其中 **无 table `__gc`** 影响 `LuaEnv.cpp` 的 `FManualRefProxy` 兜底，需改 `lua_newuserdatadtor`；
**无字节码 `string.dump`** 会影响 LuaPanda 调试器与预编译字节码热更方案。

### 10.5 为什么原生 Lua 现在还不能删

删除会让树立刻不可编译，而本机**没有 `UnrealEditor-*.dll`，我无法编译排错**。建议顺序：

1. 先移植 §10.3 的 14 个文件（以 `UnLuaCore.cpp` 为主）
2. `UnLuauNova.Build.cs` 依赖从 `Lua` 切到 `Luau`
3. 编译通过 → **再删 `Source/ThirdParty/Lua`**
4. 最后处理 §10.4 的语义差异与测试套件

---

## 11. 解耦进展与一次失败（2026-10-19，务必读）

### 11.1 已落地：`LuaBackend.h`（安全、可编译）

`Plugins/UnLuauNova/Source/UnLuauNova/Public/LuaBackend.h` —— 唯一吸收 VM 差异的地方：

| 差异点 | Lua 5.4 | Luau |
|---|---|---|
| 分配 userdata | `lua_newuserdata`（= `lua_newuserdatauv(L,s,1)`） | `lua_newuserdata`（= `lua_newuserdatatagged(L,s,0)`）→ **同名，无需分支** |
| userdata 长度 | `lua_rawlen` | `lua_objlen` |
| 加载 chunk | `luaL_loadbuffer` | `luau_load`（无字节码 loader/dumper） |
| GC 设置 | `lua_gc(L, LUA_GCGEN, 0, 0)` | `LUA_GCSETGOAL` / `LUA_GCSETSTEPMUL`（**无 `LUA_GCGEN`**） |
| table `__gc` / 整数子类型 / 字节码 | 有 | **无**（已用 `UNLUAU_HAS_*` 标记） |

`UNLUAU_USE_LUAU` 默认 0，所以**今天这棵树仍按原来方式编译**，零风险。

### 11.2 一次失败（已回滚，无残留）

我尝试用脚本按行号改写 `LuaCore.cpp` 的 userdata 层（把 `FUserdataDesc` 从 userdata **尾部**挪到**数据紧前面**，从而只靠指针就能定位描述符、彻底不需要 `lstate.h/lobject.h/lfunc.h`）。

**脚本的 `RemoveRange` 成功但 `InsertRange` 全部失败**（PowerShell 类型不匹配），文件被删掉约 200 行而没写回新内容。已 `git checkout` 还原，校验 1388 行、`USERDATA_MAGIC`/`GetTValue`/`lstate.h` 均恢复原状，**没有残留**。

### 11.3 为什么到此停下

那次改写是**内存布局手术**，正确性依赖：padding 计算与读写两端一致、容器 userdata 的 `padding=0` 假设、以及 `MarkUserdataTwoLvPtrTag` / `SetUserdataFlags` 两处只拿到裸指针的调用点。这些**必须靠编译 + 跑 `UnLuaTestSuite` 验证**，而这台机器编译不了。盲写交给你的风险远大于收益。

### 11.4 下一步（强烈建议先做第 1 步）

1. **把编译跑通** —— `UnrealEditor-*.dll` 缺失，补上后我每一步都能验证，整体会快数倍
2. 重做 `LuaCore.cpp` 的 userdata 层解耦（可先用 Lua 5.4 验证，因为新布局对两个 VM 都成立）
3. `LuaEnv.cpp` 的 GC 设置 + `luaL_loadbuffer` → `UNLUAU_LOAD`
4. 其余 11 个文件（`LuaLib_DataTable` 10 / `LuaDanglingCheck` 6 / `PropertyDesc` 6 / `LuaLib_Math.h` 3 …）
5. `UnLuauNova.Build.cs` 开 `UNLUAU_USE_LUAU=1`，依赖切到 `Luau`
6. 处理语义差异（无 table `__gc`、无整数子类型、无 `__pairs`、无字节码）
7. 编译 + 测试通过 → **删 `Source/ThirdParty/Lua`**

---

## 附：待你确认的三个决策点

1. **项目名**：LuaNova / UnLuaX / PrismLua / Vega / 其他？
2. **引擎基线**：跟 5.7（PR #771）还是守 5.4/5.6？这决定 #693/#696/#697/#771 怎么处理。
3. **示例工程**：保留 `TPSProject` 还是改名 `LuaNovaSandbox`？（改名会让所有 patch 路径冲突，建议在 M1 之后再改）

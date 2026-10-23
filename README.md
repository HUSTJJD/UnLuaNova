<div align="center">

<img src="Docs/Images/UnLuaNova.png" alt="UnLuaNova" width="160" />

# UnLuaNova

**面向 Unreal Engine 的 Lua 脚本方案 —— [Tencent/UnLua](https://github.com/Tencent/UnLua) 的社区续作**

[![CI](https://github.com/HUSTJJD/UnLuaNova/actions/workflows/ci.yml/badge.svg)](https://github.com/HUSTJJD/UnLuaNova/actions/workflows/ci.yml)
[![Docs](https://github.com/HUSTJJD/UnLuaNova/actions/workflows/docs.yml/badge.svg)](https://hustjjd.github.io/UnLuaNova/)
[![License: Polyform Small Business](https://img.shields.io/badge/License-Polyform%20Small%20Business-0175C2.svg)](LICENSE)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-blueviolet.svg)](https://www.unrealengine.com/)
[![Lua](https://img.shields.io/badge/Lua-5.5%20%2F%20Luau-blue.svg)](https://www.lua.org/)
[![GitHub stars](https://img.shields.io/github/stars/HUSTJJD/UnLuaNova?style=social)](https://github.com/HUSTJJD/UnLuaNova/stargazers)

[在线文档](https://hustjjd.github.io/UnLuaNova/) · [快速开始](#快速开始) · [文档索引](#文档索引) · [English](README_EN.md)

</div>

---

## 这是什么

UnLua 的官方发布线冻结在 **v2.3.6（2023-11）**：`develop` 分支虽仍有零星修复，但没有版本、也没有维护者。
UnLuaNova 把散落的社区成果收敛成一条**有人维护的发行线**，并在此基础上做三件事：

| | 做什么 | 带来什么 |
|---|---|---|
| **静态绑定** | 构建期为声明类型生成绑定表 | 绕开反射热路径与运行时查找开销 |
| **双 Lua 后端** | [Lua 5.5](https://www.lua.org/) 与 [Luau](https://luau.org)（默认），均以 git 子模块引入 | 后端差异收敛在一处抽象层，可按需切换 |
| **Lua 可观测性** | CPU Profiler（写入 Unreal Insights）、headless 调试服务、沙盒、死循环看门狗 | Lua 不再是黑盒，性能与故障可定位 |

一句话总结：**保留 UnLua 的编程手感，补上它缺失的维护、性能与调试能力。**

## 为什么是 UnLuaNova

| | Tencent/UnLua v2.3.6 | UnLuaNova |
|:---|:---|:---|
| 维护状态 | 停更（2023-11） | 持续维护中 |
| 引擎版本 | UE 4.17 – 5.x（新版本需自行适配） | 基线 **UE 5.8.3**，并合入社区 UE 5.4 / 5.6 修复 |
| Lua 后端 | 单一 Lua 版本 | **Lua 5.5 / Luau** 双后端可切换 |
| 绑定方式 | 运行时反射 | 构建期**静态绑定**（opt-out 选择类型） |
| 性能 | — | `CallUEFast` 跳过冗余查找、参数缓存持久化 |
| 可观测性 | 基础 | CPU Profiler / 内存统计 / 调试服务 / 沙盒 / 看门狗 |
| 类型检查 | LuaLS + 导出的 EmmyLua 注解 | 生成 Luau `.d.lua`，`luau-lsp` 提前暴露 UE API 误用 |

每一项**为什么这么做、换来了什么、代价在哪**：[相对 UnLua：原理与优势](Docs/CN/WhyUnLuaNova.md)
（英文版 [Why UnLuaNova](Docs/EN/WhyUnLuaNova.md)）。

## 测试结果

自动化测试在**两个后端各跑一遍全量**，报告由 `Tools/RunTests.ps1` 生成、随仓库发布 ——
不是抽样，是仓库里的每一条用例：

| 后端 | 用例 | 通过 | 失败 | 跳过 | 耗时 | 静态类型检查 |
|:---|---:|---:|---:|---:|---:|:---|
| **Luau**（默认） | **518** | 518 ✅ | 0 | 0 | 205.9s | `luau-lsp` strict：**0 错误** |
| Lua 5.5 | 509 | 509 ✅ | 0 | 0 | 225.1s | LuaLS：**0 错误** |

完整报告：[Luau 后端](Docs/test-luau-report.md) · [Lua 5.5 后端](Docs/test-lua-report.md)

518 条里有 **59 条是上游 issue 的回归测试**（`Issue276` … `Issue668`）——
上游那些「有人报、有人改、但没人验证过」的问题，在这里是一条会红的用例，
而不是一段留在 issue 区里的对话。

Luau 比 Lua 5.5 多出的 9 条，是 Luau 专属能力：原生 `vector` ↔ `FVector`、`buffer` 读写、
沙盒拦截、**原生断点与单步**（常态零开销）、分配分类统计。这些在 Lua 5.5 下**不存在**，
不是降级实现 —— 详见 [Luau 后端](Docs/CN/Luau.md)。

### 运行时实录

下面四张不是摆拍，是自动化测试自己跑出来的画面（用例 `UnLuaNova.Showcase.*`）：

<div align="center">

<table>
<tr>
<td align="center"><img src="Docs/Images/tests-luau/Showcase_Binding.png" width="400"><br><b>静态绑定</b></td>
<td align="center"><img src="Docs/Images/tests-luau/Showcase_Container.png" width="400"><br><b>容器访问</b></td>
</tr>
<tr>
<td align="center"><img src="Docs/Images/tests-luau/Showcase_Override.png" width="400"><br><b>蓝图事件覆写</b></td>
<td align="center"><img src="Docs/Images/tests-luau/Showcase_UMG.png" width="400"><br><b>UMG 与 Lua 生命周期</b></td>
</tr>
</table>

</div>

每一段 showcase 还有 60 帧的实机剪辑（`Tools/RunTests.ps1 -Video` 录制，标准 H.264 mp4）：

[静态绑定](Docs/Images/tests-luau/Showcase_Binding_Clip.mp4) ｜
[容器访问](Docs/Images/tests-luau/Showcase_Container_Clip.mp4) ｜
[蓝图事件覆写](Docs/Images/tests-luau/Showcase_Override_Clip.mp4) ｜
[UMG 与 Lua 生命周期](Docs/Images/tests-luau/Showcase_UMG_Clip.mp4) ｜
[整场录屏：从第一条用例跑到最后一条](Docs/Images/tests-luau/session.mp4)

> GitHub 不内联播放仓库内的 mp4，点开上面的链接即可观看；文档站的
> [测试报告](https://hustjjd.github.io/UnLuaNova/) 里是可以直接播放的。

### 调用开销

`UnLuaNova.Perf.Benchmark` 的实测数据（UE 5.8.3 Development，本机）：

| 路径 | 迭代次数 | µs/次 |
|---|---:|---:|
| 直接 C++ 调用 | 20000 | 0.0034 |
| UE 反射 `ProcessEvent` | 20000 | 0.4495 |
| 从 Lua 调用 UE 函数 | 20000 | 1.4712 |
| 纯 VM：算术循环 | 200000 | 0.0086 |
| 纯 VM：函数调用 | 200000 | 0.0322 |
| 纯 VM：表读写 | 200000 | 0.0282 |

前三行跨越语言边界，后三行不跨越 —— 分开看才有意义：

+ **跨语言那一跳是微秒级。** 20000 次「从 Lua 调 UE 函数」共 29.4 ms，单次 1.47 µs，
  只有反射 `ProcessEvent` 的 3 倍多。远没到「为了性能不敢从脚本调引擎」的程度。
+ **脚本自身快两个数量级。** 一次纯 VM 迭代 0.0086–0.032 µs，也就是 9–32 纳秒。
  真正吃掉时间的是跨语言边界，不是脚本里的循环和函数调用。

> 上表是 **Lua 5.5 后端**这一轮的完整数据。两个后端是编译期选择，要各编译各跑，
> Luau 那一轮的并排数据会在这里补齐 —— 只有 `VM.*` 三项能真正区分两个虚拟机，
> 前三行的开销在参数编组与函数解析上，两个后端本来就落在同一量级。

Luau 在纯脚本侧还有一条 Lua 5.5 没有的路，是 `vector`：

```lua
local Native = vector.create(1, 2, 3)
local Sum = (Native + Native) * 2     -- 值类型运算，零分配

local Vec = UE.FVector(1, 2, 3)
local Sum2 = (Vec + Vec) * 2          -- 每次 + 都新建一个 userdata，随后被 GC
```

`vector` 的三个分量塞在 TValue 里，运算不分配任何对象；`FVector` 是带元表的 userdata，
每一次 `a + b` 都要新建再回收一个。数学密集的脚本（方向、速度、偏移、插值）在 Luau 下
可以直接避开这一整条分配链。

> 想自己复现：`.\Tools\RunTests.ps1 -Backend Both -Publish`，两个后端各出一份报告，
> 性能数据写进 `Saved/Benchmark/`。

## 快速开始

```bash
# 1. 把 Plugins 目录复制到你的 UE 工程根目录
# 2. 初始化子模块（Lua 5.5 与 Luau 都是子模块，不初始化会缺 VM 源码）
git submodule update --init --recursive
```

3. 重新启动工程
4. 新建蓝图 → UnLuaNova 工具栏选择 `绑定` → 填入 Lua 模块路径（如 `GameModes.BP_MyGameMode`）
5. 选择 `创建 Lua 模版文件`，在 `Content/Script/GameModes/BP_MyGameMode.lua` 里写代码

> 如果你是 UE 新手，照着图走一遍更快：[UE 新手快速上手](Docs/CN/Quickstart_For_UE_Newbie.md)。

## 写起来是什么样

无需胶水代码，直接访问 `UCLASS` / `UPROPERTY` / `UFUNCTION` / `USTRUCT` / `UENUM`，也能直接替换蓝图里的实现：

```lua
local BP_PlayerController_C = UnLuaNova.Class()

function BP_PlayerController_C:ReceiveBeginPlay()
    -- 直接调用引擎 API
    local Widget = UE.UWidgetBlueprintLibrary.Create(
        self, UE.UClass.Load("/Game/Core/UI/UMG_Main_C"))
    Widget:AddToViewport()

    -- 想保留蓝图里的原实现？显式调回去
    self.Overridden.ReceiveBeginPlay(self)
end

-- 覆写带返回值的蓝图事件
function BP_PlayerController_C:GetCharacterInfo(HP, Position, Name)
    return true, 99, UE.FVector(128.0, 128.0, 0.0), "Marcus"
end

return BP_PlayerController_C
```

同样的套路也适用于 **Replication Notify**、**Animation Notify**、**Input Event**：
Lua 函数名对上（`AnimNotify_xxx`）即完成替换，不需要任何辅助代码。

## 状态

| 项 | 值 |
|---|---|
| 当前版本 | **1.0.0**（[CHANGELOG](CHANGELOG.md)） |
| 引擎基线 | **UE 5.8.3**（源码版） |
| Lua 后端 | **Luau（默认）** ／ Lua 5.5 —— 均以子模块引入 |
| 上游基线 | Tencent/UnLua v2.3.6 + `develop`(2026-04) |
| 许可 | Polyform Small Business 1.0.0 ／ 商用超阈值需付费 |

Luau 后端 518 条自动化测试全绿（见[测试结果](#测试结果)），可以拿来评估与接入；
上生产前建议锁定版本，并先读完两节已知边界：
[相对 UnLua：原理与优势](Docs/CN/WhyUnLuaNova.md#六代价与已知边界) 与
[Luau 后端](Docs/CN/Luau.md#六仍未完成的部分)。

## 文档索引

<div align="center">

| 中文 | English |
|:---|:---|
| [UE 新手快速上手](Docs/CN/Quickstart_For_UE_Newbie.md) | [Programming Guide](Docs/EN/UnLuaNova_Programming_Guide.md) |
| [功能清单](Docs/CN/Features.md) | [API Reference](Docs/EN/API.md) |
| [相对 UnLua：原理与优势](Docs/CN/WhyUnLuaNova.md) | [Why UnLuaNova](Docs/EN/WhyUnLuaNova.md) |
| [Luau 后端](Docs/CN/Luau.md) | [Luau Backend](Docs/EN/Luau.md) |
| [编程指南](Docs/CN/UnLuaNova_Programming_Guide.md) | [How To Implement Overriding](Docs/EN/How_To_Implement_Overriding.md) |
| [API 参考](Docs/CN/API.md) | [FAQ](Docs/EN/FAQ.md) |
| [设置项](Docs/CN/Settings.md) | [在线文档站](https://hustjjd.github.io/UnLuaNova/) |
| [调试](Docs/CN/Debugging.md) ｜ [智能提示](Docs/CN/IntelliSense.md) | |
| [静态导出绑定](Docs/CN/StaticExportBinding.md) ｜ [控制台命令](Docs/CN/ConsoleCommand.md) | |
| [测试与自动化](Docs/CN/Testing.md) ｜ [FAQ](Docs/CN/FAQ.md) | |

</div>

上游 2019–2023 年的历史变更：[`Docs/Archive/UnLua_CHANGELOG.md`](Docs/Archive/UnLua_CHANGELOG.md)。

## 从 UnLua 迁移

Lua 侧的全局名已从 `UnLua` 改为 **`UnLuaNova`**，批量替换即可：

```lua
local M = UnLua.Class()       -- UnLua
local M = UnLuaNova.Class()   -- UnLuaNova
```

UCLASS 名同样改了名（`UUnLuaSettings` → `UUnLuaNovaSettings` 等），需要 resave 资产并迁移 `Config/*.ini` 的 section 名。

## 参与进来

Star 是最省力的支持方式 ⭐ —— 也欢迎：

- 提 issue：编译问题、UE 版本适配、API 疑问都可以
- 提 PR：修文档、补测试、合入上游未合并的社区修复
- 跑测试：`.\Tools\RunTests.ps1`（详见 [测试与自动化](Docs/CN/Testing.md)）

## 许可

UnLuaNova 采用 **[Polyform Small Business 1.0.0](https://polyformproject.org/licenses/small-business/1.0.0)**，
并附加了更严格的规模限定（见 [LICENSE](LICENSE)）：

| 你的情况 | 能不能用 |
|:---|:---|
| 个人学习、业余项目、开源项目 | 可以，免费 |
| 组织：员工+承包商 **< 10 人** 且 上一财年营收/融资 **< $1M** | 可以，免费，含商用 |
| 超过其中任一条 | 需要商业授权，见 [COMMERCIAL.md](COMMERCIAL.md) |

规模按集团**合并计算** —— 母公司、子公司、受共同控制的关联实体算作一个主体，
大厂旗下的小工作室不能只按自己的人数算。

一句话：源码公开、小团队免费商用，赚到钱的付费。

贡献代码需要签 [CLA](CLA.md)：它授予维护者把你的贡献再许可给商业客户的权利。
没有这份授权，商业授权在法理上卖不干净，项目也就没钱继续维护你写的代码。
详见 [CONTRIBUTING.md](CONTRIBUTING.md)。

上游组件仍按 MIT 授权、不受本次变更影响：[Tencent/UnLua](https://github.com/Tencent/UnLua)、
[Lua 5.5](https://www.lua.org/)、[Luau](https://github.com/luau-lang/luau)，
见 [LICENSE-THIRD-PARTY.md](LICENSE-THIRD-PARTY.md)。

Lua 虚拟机使用 [Lua 5.5](https://www.lua.org/)（MIT）；Luau 使用 [luau-lang/luau](https://github.com/luau-lang/luau)（MIT）。
两者均以 git 子模块引入，克隆后需执行 `git submodule update --init --recursive`。

<div align="center">
<img src="Docs/Images/UnLuaNova.png" alt="UnLuaNova" width="160" />

# UnLuaNova

面向 Unreal Engine 的 Lua / Luau 脚本集成，基于 Tencent/UnLua fork。

[中文文档](Docs/CN/index.md) · [English](README_EN.md) · [现状与路线图](Docs/CN/Benchmark_Review.md)
</div>

## 产品目标与当前状态

目标是结合 UnLua 的 UE 互操作、slua-unreal 的产品能力、FUnLua 的对象与绑定设计，以及 Luau 的语言和工具链能力，形成可验证、可诊断、可维护的脚本方案。**这是产品目标，尚不能宣称全面覆盖、全面兼容或性能领先。**

当前源码包含反射绑定与覆写、静态导出和生成 thunk、容器和委托、Lua 5.5.1 / Luau 双后端、调试与 Profiler、类型声明导出、运行时库和声明式网络。生成 thunk 仍调用 `ProcessEvent`；Luau 原生编译需要显式开启；声明式网络仍处于实验阶段。

2026-10-24 的源码核查发现网络复制条件、Timer 生命周期、Luau 手工对象引用终结等发布阻塞项。当前工作树的 C++ 修改尚未编译、自动化用例尚未执行，旧报告不代表当前代码。

- [当前审计与验收顺序](Docs/CN/Benchmark_Review.md)
- [能力、源码、测试和示例矩阵](Docs/feature-matrix.md)
- [slua-unreal / FUnLua 源码对照](Docs/CN/Comparison_sluaunreal.md)
- [Luau 能力与 Lua 兼容边界](Docs/CN/Luau.md)
- [性能验收方法](Docs/CN/Performance_Parity_slua.md)

## 接入

1. 将本仓库 `Plugins/` 中需要的插件放进 UE 工程。
2. 初始化子模块：`git submodule update --init --recursive`。依赖版本由 gitlink 固定，见 [版本清单](Plugins/UnLuaNova/Source/ThirdParty/VERSIONS.md)。
3. 在 `Config/DefaultUnLuaNova.ini` 选择后端，由工程维护者编译对应 UE 目标。
4. 用 UnLuaNova 工具栏绑定蓝图模块，脚本放在 `Content/Script/`。

后端切换需要重新编译。仓库配置以本地 UE 5.8.3 环境为基线，其他引擎版本和目标平台需要各自验收。

```lua
local M = UnLuaNova.Class()
function M:ReceiveBeginPlay()
    print("UnLuaNova", UnLuaNova.GetBackend())
end
return M
```

详见 [快速开始](Docs/CN/Quickstart_For_UE_Newbie.md)、[编程指南](Docs/CN/UnLuaNova_Programming_Guide.md) 和 [示例索引](Docs/CN/Showcase.md)。

## 验证

不启动编译的仓库检查：`python Tools/AuditRepository.py`。它只检查能力清单、引用和文档一致性，不证明运行时正确。

UE 验收只需 `.\Tools\RunTests.ps1`，默认自动完成 Win64 双后端构建、Editor/cooked 用例、联网、类型检查与打包，见 [测试说明](Docs/CN/Testing.md)。缺少必需证据即失败，汇总位于 `Saved/ReleaseTests/`。完整流程尚未执行；当前状态见 [Lua](Docs/test-lua-report.md) / [Luau](Docs/test-luau-report.md)。

## 来源与许可

保留 [Tencent/UnLua](https://github.com/Tencent/UnLua) 的来源和 MIT 声明。本仓库根许可包含 PolyForm Small Business 及附加条款；源码头、根许可与授权范围仍需权利人统一。第三方组件独立遵循其许可，详见 [第三方清单](LICENSE-THIRD-PARTY.md) 与 [许可现状](Docs/CN/License.md)。

slua-unreal、FUnLua 是设计对照来源，不代表其作者认可本项目，也不自动授权复制其实现。

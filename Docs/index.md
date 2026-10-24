# UnLuaNova

基于 Tencent/UnLua 的 Unreal Engine Lua / Luau 脚本集成。

[中文入口](CN/index.md) · [English](EN/index.md)

## 当前状态

源码包含双后端、UE 绑定、覆写、容器、调试、性能诊断、运行时库和实验性声明式网络。目标是综合 UnLua、slua-unreal、FUnLua 和 Luau 的能力，并以独立设计改进质量；全面覆盖与性能领先仍需要证据。

2026-10-24 审计以源码静态核对为主。当前工作树尚未重新编译或运行 UE 自动化，不能引用旧报告作为当前通过证明。

- [现状、风险与路线图](CN/Benchmark_Review.md)
- [能力与证据矩阵](feature-matrix.md)
- [slua / FUnLua 对照](CN/Comparison_sluaunreal.md)
- [Luau 支持边界](CN/Luau.md)
- [测试框架与验收](CN/Testing_Architecture.md)
- [Lua 验证状态](test-lua-report.md) / [Luau 验证状态](test-luau-report.md)

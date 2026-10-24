# slua-unreal 与 FUnLua 源码对照

以 [当前审计](Benchmark_Review.md) 的固定提交为基线，产品行为和源码同时核对。README 性能宣传不作为本项目的实测结论。

## slua-unreal

[源码树](https://github.com/Tencent/sluaunreal/tree/7970ab72fd59118e1b3f86aac2d016828b54b748/Plugins/slua_unreal/Source)

| 对照能力 | 本项目现状 / 应吸收的要求 |
|---|---|
| 反射、双向调用、Blueprint override、out/ref、枚举与运算符 | 从 UnLua 继承大部分路径；需要参数与异常契约、继承/网络覆写的矩阵验收 |
| 模板导出普通 C++、静态代码生成、非反射扩展方法 | 已有 UnLuaNovaEx 模板和手工导出；生成器主要处理反射签名，不等价于 libclang 任意 C++ 导出 |
| 容器、结构体、对象 GC、委托回调 | 已有实现；借用视图失效、手工引用和多 Env 清理仍有风险 |
| 多 Lua state、热更新、脚本驱动 Actor/Component/Widget | 已有入口；同类不同实例模块绑定和多世界 Timer 隔离未闭环 |
| RPC / 属性复制 | 有实验实现；条件复制、int64、schema/继承顺序及真实网络验收仍有缺口 |
| CPU/内存诊断、真机调试、死循环检测 | 本项目有 Profiler、DAP、看门狗；真机远程调试与权限/重连/源码映射需要产品化 |
| 多线程 GC | 本项目是显式分片/预算调度，不能称为同等线程 GC；需用帧停顿和线程安全证明替代方案 |

重点阅读 `LuaObject`、`LuaVar`、`LuaState`、`LuaDelegate`、`LuaOverrider`、`LuaProfiler` 及 `CppBinding`，并结合 `Tools` 导出工具。设计上应吸收“普通 C++ 与反射 API 都可用”的工作流，避免把某一种生成方式作为产品目标本身。

## FUnLua

[源码树](https://github.com/fcscript/FUnLua/tree/7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3/Plugins/FCScript/Source/FCScript)

| 源码入口 | 核实到的设计 | 本项目行动 |
|---|---|---|
| `Private/FCGetObj.h` | FCObjRef 带引用类型、对象 ID、父子关系 | 统一 owned/borrowed 语义，父对象失效时子视图可诊断地失败；覆盖结构体/容器迭代器 |
| `Private/FCSafeProperty.h` | 弱 FProperty 元数据引用 | 验证蓝图重编译、热重载、reinstance 后旧属性缓存失效 |
| `Private/FCObjectManager.h/.cpp` | 对象绑定及 (Object, Function) override 身份 | 支持同类实例绑定不同模块；当前 UClass 缓存不能证明此能力 |
| `Private/FCDynamicDelegateManager.h` | table/object 与 Lua function 组成回调身份，去重与清理 | 设计 token、owner 与幂等取消；保留 UE 单播委托语义，不把单播强改多播 |
| `Private/FCRefObjCache.cpp` | 缓存与预算释放，依赖全局脚本上下文 | 学习批量释放成本控制；本项目需按 Env 隔离，避免照搬全局假设 |

FUnLua 的“无需手动释放”和“不增加 GC 计数”不能脱离源码路径作绝对保证，其代码也存在手工 FGCObject 引用。README 的 5 倍/10 倍性能数字没有在本项目环境复现。

## 兼容与许可

目标是覆盖工作流并提供迁移说明，不要求 API 名称或内存布局逐字兼容。UnLua fork 的既有接口必须保留回归测试；slua/FUnLua 脚本迁移需要独立适配表。

slua-unreal 为 BSD-3-Clause，不能误写 MIT。FUnLua README 链接 `LICENSE.TXT`，但本轮固定提交的树中未找到该文件；暂不导入其源码。本轮仅借鉴设计并记录来源，未复制这两个项目的实现。完整规则见 [许可现状](License.md)。

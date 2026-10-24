# 当前源码审计与路线图

核查日期：2026-10-24。范围：运行时、绑定生成器、编辑器调试、测试工具、示例、文档、第三方来源。此页只记录当前结论，历史修改以 Git 为准。

**结论：尚未达到 slua-unreal / FUnLua 产品能力全面覆盖，也未证明性能领先或测试全覆盖。** 本次不启动编译；源码存在、用例存在、执行通过、目标平台验收是四种不同证据。逐项状态及源码入口见 [能力矩阵](../feature-matrix.md)。

## 对照基线

| 项目 | 核查提交 | 用途 |
|---|---|---|
| slua-unreal | `7970ab72fd59118e1b3f86aac2d016828b54b748` | 产品场景、CppBinding、对象/委托、Profiler、GC |
| FUnLua | `7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3` | 对象引用树、属性失效、实例绑定、委托身份 |
| Luau 上游 0.744 | `32c28089305f488f901aaf1dbf3fd1cb65fa3d11` | VM、库、Compiler、Require 与测试 |
| 当前集成 Luau | `6d36d483cb88973276af611198169f76f3ce05b0` | 0.743 + 1；未升级依赖 |
| UnLua 参考 HEAD | `3f112e8689405eaad79bd1f8f0bc8d598c8f7fc1` | 继承来源，非本轮主要重审对象 |

Luau 0.744 与当前 pin 的公开 VM 头文件一致，但 18 个 VM 实现文件有变化，不能把“接口未变”当作无风险升级。对照链接与具体吸收方式见 [源码对照](Comparison_sluaunreal.md)。

## 发布阻塞与验收

| ID / 优先级 | 源码事实与影响 | 下一步及通过条件 |
|---|---|---|
| NET-01 / P0 | `UnLuaNovaReplicationComponent::GetLifetimeReplicatedProps` 从实例 owner 推导条件；UE 的 replication layout 按类缓存。同类型字段共用桶，`BucketCondition` 不能表达逐字段条件 | 使用类级不可变 schema，按类型与条件设计稳定布局，或采用经目标复制系统验证的自定义 serializer。至少两个 actor、owner/非 owner、混合条件、加入顺序互换、晚加入、relevancy、dormancy；分别验 legacy/Iris/push model |
| NET-02 / P0 | 网络 int64 直接经过 `lua_tointeger`；Luau 此接口为 int32。未知条件回退 `COND_None` 会扩大复制范围 | 统一 int64 编组契约，拒绝未知条件、冲突、重名和 schema 不匹配；验证 ±2^31、±2^53、int64 极值及错误输入 |
| LIFE-01 / P0 | Timer 保存调用方裸 `lua_State*`，未保活协程、未随 Env 销毁清理；World 选择取首个 PIE/Game | 绑定显式 Env/World 身份，关闭前取消回调与 registry refs；验证 coroutine GC、Env 先销毁、World 先销毁、多 PIE、重入取消 |
| LIFE-02 / P0 | 容器、FText、非平凡导出构造已接 VM dtor；但 `ObjectRegistry::AddManualRef` 代理仍依赖 userdata `__gc`，Luau 不调用它 | 逐分配点追踪 owner / borrowed / manual ref / smart pointer；不得在 GC dtor 中操作 Lua 栈。双后端强制 GC + UE GC + teardown，引用计数回到基线 |
| COMPAT-01 / P1 | `LuaScriptCompat` 的 upvalueid、registry、uservalue 是近似或占位，load mode 未完整执行 | 不支持的操作显式失败或发布明确子集；验证共享标量 upvalue、闭包别名、热重载失败回滚、文本/字节码模式 |
| BIND-01 / P1 | `UnLuaNovaManager::Classes` 按 UClass 缓存；已有类绑定短路，不能等同于 FUnLua 的同类实例绑定不同模块 | 定义类默认与实例 override 分层；同类 A/B 模块并存、继承、解绑、热重载不串线 |
| EVENT-01 / P1 | Event token 强持有 Owner；弱键索引不自动解决 owner 保活。Once 重入需要在调用前取消 | 明确主动注销契约；验证递归 Emit、异常、解绑、闭包持有 owner；本轮修复 Once 重入，其他生命周期仍需验收 |
| SAFE-01 / P1 | sandbox 默认关闭；UE、debug、io、socket 等宿主能力不由只读表自动隔离。io 文件表可修改，路径检查不是 OS 沙盒 | 可信脚本 / 不可信脚本分别定义能力策略；检查符号链接、配额、超时、长跳转清理、调试服务暴露 |
| TOOL-01 / P1 | 编译 coverage 选项有接入，尚无完整导出与门禁；CodeGen 计数记录请求而非成功函数；LSP 缺失可能跳过 | 导出构建身份、成功/跳过/失败及原因；类型检查在验收任务中必须存在；coverage 给出分母与排除项 |
| QA-01 / P1 | 源码用例与旧报告不能覆盖当前修改；部分网络测试只比较独立进程 RPCId | 真正连接的服务器/多客户端、错误输入、GC/销毁、打包、多平台矩阵；按能力证明，不能只按注册用例数 |
| PERF-01 / P1 | 已有 Benchmark 与阈值逻辑，但没有已提交的性能基线；缺基线只告警。thunk 仍调用 ProcessEvent | 同机同版本运行 slua/FUnLua/本项目，记录分布、分配、GC、冷启动和代码体积；见性能页 |
| LEGAL-01 / P0（发布） | 根许可与 MIT 文件头/授权说明范围不清；扩展库清单不全；FUnLua 缺可核对的许可文件 | 权利人确认授权边界，补齐完整 notice 与来源映射；对外发布前完成，不擅自改许可证 |

## 实施顺序

1. **正确性与归属**：先关闭 NET/LIFE 阻塞，补失败输入和销毁顺序测试；修复之前把网络明确标为实验功能。
2. **覆盖与工具闭环**：完成能力矩阵各行的验收条件、示例 smoke、后端契约测试；运行报告必须包含源码提交与 dirty 状态、UE/编译器、后端、平台、flags、拓扑和原始产物。
3. **性能与发布**：建立可复现对照和阈值，执行打包与目标设备验收，完成许可清单。通过后才发布能力和性能结论。

## 设计创新的交付标准

| 方向 | 要解决的问题 | 验收 |
|---|---|---|
| 带代际标识的 Env/World 回调句柄 | 裸 VM 指针、环境重建和跨世界回调 | 销毁后必失效，地址复用不误命中，诊断能指到注册点 |
| 同源签名描述 | thunk、反射回退、类型声明互相漂移 | 同一签名生成三者；异常、out/ref、默认值行为对照 |
| 确定性网络 schema | 运行时顺序、版本差异、条件冲突 | 稳定排序、版本/哈希协商、拒绝未知 schema，双端一致 |
| 能力与验证清单 | “有 API”被误当作“全支持” | 版本/后端/平台/示例/测试/验证产物可追溯 |

以上创新尚未完成。线程 GC 也不能仅为对标照搬：先证明 VM 与 UObject 的线程约束和停顿收益，再决定分片 GC 或更深层 VM 改造。

## 本轮变更与验证边界

本轮修正 CodeGen 的 VM/协程初始化归属、io 的 CRLF 与输入校验、RPC 声明排序和 schema 条件指纹，补充对应回归源码；修复 Event.Once 重入并增加可执行示例。相关 C++/Lua 用例均等待维护者构建后执行。

文档删除重复历史计划、撤下旧报告结论、保留当前使用说明。仓库检查仅验证文件引用、矩阵结构与脚本逻辑，不执行 UE、不衡量运行覆盖率。测试步骤见 [测试架构](Testing_Architecture.md)。

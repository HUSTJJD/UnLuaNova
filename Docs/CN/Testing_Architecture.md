# 测试架构与验收

当前框架包含 UE C++ Spec、Lua 侧 Spec、场景/视口测试、FNetSession 跨进程测试、性能用例、报告与静态分类工具。

## 分层

| 层 | 载体 | 验证内容 |
|---|---|---|
| 仓库检查 | Python / PowerShell parser | 矩阵、链接、工具逻辑、脚本语法 |
| VM / 绑定 | UE Spec + Lua Spec | 后端、编组、容器、异常、库契约 |
| 内存 | 双后端 Editor / cooked Spec + Python 门禁 | VM 逐块归零、原生载荷/UObject/回调释放、预热后增长、保留引用对照 |
| World | 游戏或编辑器 | Timer、对象销毁、Latent、多环境 |
| Network | PIE + 真正连接的进程 | RPC、属性、owner、多客户端 |
| UI / 示例 | Editor + 真实 RHI | UMG、输入、截图、示例操作 |
| 性能 / 发布 | 固定机器 + 打包目标 | 分布、GC、内存、设备、许可 |

`ClassifyTests.py` 的 A–E 是静态启发式分类，不是动态覆盖率。Lua 侧多个断言汇总到一个 UE 用例，不应把文件数、It 数和 UE 注册用例数混为同一分母。

## 网络语义

`LuaNetDedicatedServerTest.cpp` 中两个独立进程的 RPCId 一致性只反映确定性，不代表进程间已联网。`LuaNetMulticastTest.cpp` 组织一个 server 和两个 client 的会话；加入客户端失败、缺失 peer、超时或测试进程崩溃都会使会话失败。

## 验收入口

维护者只运行 `.\Tools\RunTests.ps1`（无参数）。这条命令自动完成完整 Release：双后端构建、生成代码重编、完整 Editor 和 cooked Development 用例、真实网络 peer、类型检查、性能门禁、Server 构建与 Shipping 打包。实际顺序、失败规则与覆盖边界以 [测试说明](Testing.md) 为准。

局部 filter、Fast、NullRHI 或 SkipBuild 属于独立的 `Tools/RunDevelopmentTests.ps1`，不能作为 release 结果。

## 无编译检查（已集成到唯一入口）

`RunTests.ps1` 的 `repository-audit` 与 `tooling-tests` 阶段分别执行
`Tools/AuditRepository.py`、`python -B -m unittest discover -s Tools/tests -p "test_*.py"`
与 `TestReleaseSupport.ps1`，**无需手工运行**；`docs-site` 阶段还会做
`mkdocs build --strict`。这些检查不启动 UE、UBT、CMake 或 LSP 构建，通过只说明仓库
检查通过，最终判定仍以 `release-summary.json` 的整轮结果为准。

# 安全政策

本文件说明 UnLuaNova 接受哪些版本的安全报告、如何报告，以及维护者能够给出什么样的响应。
UnLuaNova 是社区独立项目，并非 Epic Games 官方产品；Unreal® 是 Epic Games, Inc. 的商标。

## 支持的版本

安全修复只针对当前维护版本提供。当前版本为 **v1.0.0**（在 UE 6.0 / Win64 通过完整发布验证，
UE 5.8.3 / Win64 为已验证基线；具体组合见仓库内的引擎版本与平台支持矩阵）。
更早的历史 tag 与旧提交不提供安全修复；请升级到当前版本后再复现问题。

## 如何报告

**请不要用公开 issue、discussion 或 PR 披露漏洞细节。** 请通过 GitHub Security Advisories 的
私有报告入口提交：打开仓库 <https://github.com/HUSTJJD/UnLuaNova> 的 Security 标签页，
选择 "Report a vulnerability"（新建仅维护者可见的私有 advisory，地址
<https://github.com/HUSTJJD/UnLuaNova/security/advisories/new>）。报告在协调披露前不会公开。

若问题来自上游组件而非本插件自身，例如 Unreal Engine、Lua、Luau、LuaSocket、lua-protobuf、
lua-rapidjson，或 `Tools/` 下的 LuaLS / luau-lsp，请同时报告给对应上游，并在本仓库的私有报告中
说明关联，便于我们跟进。第三方组件清单见 [LICENSE-THIRD-PARTY.md](LICENSE-THIRD-PARTY.md)。

## 响应预期

本项目由单人维护，安全事务按「尽力而为 / best effort」处理：

* 会阅读并确认每一份私有报告，必要时向你追问复现信息；
* **不承诺任何 SLA、修复时限、响应时限或定期安全公告**，也不设公开的安全响应值班；
* 是否修复、何时修复、是否发布带安全说明的版本，取决于问题性质与维护者可用时间；
* 在修复可用前报告内容保持非公开，协调披露的时间以维护者判断为准。

请勿把本政策理解成有时限保证的商用安全支持；如你需要有合同约束的响应，请自行评估依赖风险。

## 报告时请提供的信息

信息越完整，越有助于在缺少专职团队的情况下定位问题：

* **UE 版本**：引擎版本号，以及是否为源码构建（本仓库验收基于 Windows / PowerShell / Win64 / UE 源码构建）
* **Lua 后端**：Lua 5.5.1 还是 Luau（编译期切换，见 `Config/DefaultUnLuaNova.ini` 的 `LuaBackend`）
* **平台**：操作系统与架构，例如 Windows 11 / Win64
* **构建配置**：Editor / Game / Server / Shipping 中的哪一种
* **复现步骤**：最小可复现工程、`Content/Script/` 脚本片段、相关 ini 配置
* **日志**：相关 UE 日志、崩溃调用栈、控制台输出
* **验收摘要**：是否跑过 `Tools/RunTests.ps1`，并附上 `Saved/ReleaseTests/` 下的摘要，
  尤其是 `release-summary.json` 的相关片段。这是本仓库唯一的验收入口；CI 只构建文档站与
  轻量门禁，不做 UE 编译，不能用 CI 结果代替本地验收

## 已知边界：沙盒不是安全边界

这是本项目最重要的安全边界，请勿误解：

* `lua.sandbox` 默认关闭；它的路径检查是脚本层面的访问控制，**不是 OS 级沙盒**，
  不构成进程、文件系统或权限层面的安全边界。
* **不可信脚本需要由集成方自行制定能力策略**：限制可用库、文件与网络访问、资源配额、
  脚本加载来源等，不要依赖本插件的沙盒来隔离不可信代码。
* 把来自不可信来源的脚本加载进游戏进程，等同于把该代码作为进程内代码运行，风险由集成方承担。

更多面向使用者的限制见 README 的「当前边界」与在线文档 <https://hustjjd.github.io/UnLuaNova/>。

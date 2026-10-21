# Changelog

UnLuaNova 的变更记录。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

> 本项目源自 [Tencent/UnLua](https://github.com/Tencent/UnLua)（MIT）。
> 上游 2019–2023 年的历史变更请见 `Docs/Archive/UnLua_CHANGELOG.md`，或查阅本仓库的 git 历史。

## [Unreleased]

### Added
- `TSet` 支持 `pairs` 迭代：注册 `UNLUANOVA_ITER_ENTRY(TSet_Pairs)`（Luau 下是 `__iter`、
  Lua 5.x 下是 `__pairs`），产出 `(元素, true)`；之前 `pairs(Set)` 在两个后端都会报错，
  只能靠 `ToArray()` / `ToTable()`
- **静态绑定层** `StaticBinding/`：POD 描述符 + 注册表（延迟解析）+ 自动注册器
- **`UnLuaStaticBindingGenerator`** Commandlet：构建期为声明类型生成静态绑定表
- `CallUEFast`：对已解析函数跳过 `GetFunctionCallspace` / 覆写重定向查找 / 接口函数查找
- **Lua 后端抽象** `LuaBackend.h`：吸收 Lua 5.4 与 Luau 的 API 差异（`UNLUANOVA_LUA_BACKEND` 开关）
- Luau 以 git submodule 引入（`Source/ThirdParty/Luau`，0.743）+ `Luau.Build.cs`
- 改名脚本 `Tools/Rename-To-UnLuaNova.ps1`
- **Luau 能力落地**：
  - `UnLuaNova.GetBackend()` / `UnLuaNova.HasFeature()`：脚本按能力分支，而不是猜后端
  - `vector` ↔ `FVector`：`UE.FVector(vector)`、`v:Set(vector)`、`v:ToVector()`，
    UFUNCTION 的 `FVector` 参数与反射写入路径都能直接收 `vector`
  - Luau 模块 `UnLuaNova.Vector` / `UnLuaNova.Buffer`（`buffer` ↔ `TArray` 显式转换）
  - 示例集 `Content/Script/Luau/`（严格类型、沙盒边界、`lua.dbg` 目标、语法速览、vector 互操作），
    由 `Index.lua` 做后端守卫，Lua 5.5 下一个文件都不会加载
  - `Tools/CheckLuauTypes.ps1`：用 `luau-analyze` + 生成的 `UnLuaNova.d.lua` 做静态类型检查
  - 文档：`Docs/CN/Luau.md` 与 `Docs/EN/Luau.md`

### Changed
- 引擎基线升到 **UE 5.8.3**：`BuildSettingsVersion.V6` + `IncludeOrderVersion.Latest`
- `LuaOverridesClass` 的 UField 链遍历适配 UE 5.7+（`UField::Next` 已是 `TObjectPtr`）
- 项目全面改名 `UnLua` → `UnLuaNova`（模块、UCLASS、宏、文件名、Lua 全局名、ini section）
- `EInternalObjectFlags::AsyncLoading` → `EInternalObjectFlags_AsyncLoading`（UE 5.8 已移除该枚举项）
- `FSoftObjectPtr::operator=` 导出返回类型修正为 `FSoftObjectPtr&`
- `UNLUA_LOGERROR` 格式串缺参修正（原为未定义行为）
- **许可变更**：MIT → **Polyform Small Business 1.0.0** + 附加条款
  （人数 < 10 且营收/融资 < $1M 可免费商用，超出需商业授权，见 `LICENSE` / `COMMERCIAL.md`）；
  上游组件的 MIT 授权不变，见 `LICENSE-THIRD-PARTY.md`
- 贡献需要签 CLA（见 `CLA.md` / `CONTRIBUTING.md`），否则商业授权无法向下再许可
- `UE` 命名空间新增 `__newindex`：脚本改写引擎类型会被直接拒绝

### Fixed
- **Lua 5.5 后端一跑就崩**：`TSharedPtr` 的 `__gc`（`ReleaseSharedPtr`）用 `lua_touserdata()` 取到的是
  块首的 `FUserdataDesc`，而不是 `NewSmartPointer()` 放置智能指针的 payload，于是 `Reset()` 把描述符
  当成 `TSharedPtr` 拆了 —— 静态导出之后第一个 `lua_close()` 必然破坏堆，整个 Lua 后端跑不出任何一条
  测试结果。Luau 没有 `__gc`，这条路径在 Luau 下从未执行过，所以一直没暴露。
  现在改用 `GetSmartPointer()`（按 payload 取），`FObjectRegistry::Get()` 一并修正
- **Luau 下 64 位整数无法转字符串**：vendored Luau 带第二个数值 tag `LUA_TINTEGER`（`lua_pushinteger64`，
  用来精确表示 64 位值），但 VM 的 `luaV_tostring()` 只认 `LUA_TNUMBER`，于是所有走
  `lua_tolstring()` 的路径都拒绝整数：`string.format('%s', v)` 报
  `invalid argument #N to 'format' (string expected, got integer)`，拼接同样不行。
  任何从 64 位 UE 属性读出来的值都是这一类。已按 `luaL_tolstring()` 的写法补上
  `LUA_TINTEGER` 分支（对 vendored Luau 的本地补丁，`lvmutils.cpp` 内有标注）
- **Luau 下 `require('string')` 报 `module 'string' not found`**：plain Lua 的 `luaL_openlibs()`
  同时把每个标准库登记进 `package.loaded`，Luau 根本没有 package 库，`package` 是我们自己建的，
  之前只放了 `loaded` / `searchers`。第三方脚本就吃这个（LuaSocket 的 `socket.lua` 前两行就是
  `require("string")` / `require("math")`），所以在 Luau 上整个 LuaSocket 加载不起来。
  现在自建 `package` 时把标准库一并登记成模块（后端没有的库名自动跳过）
- `UE4File` 的 `Read` / `Write` / `__gc` 同样用 `lua_touserdata()` 取实例，拿到的是块首的
  `FUserdataDesc` 而不是 `TConstructor` 放置对象的 payload —— 与 `ReleaseSharedPtr` 是同一个
  布局问题。`UE.File` 目前没有被任何脚本或测试使用，所以还没踩出来；现在三处都改用
  `GetCppInstanceFast()`（按 payload 取，没有描述符时退回块首）
- 合并上游 `develop`：PR #700（UE5.4）、#707、#758（UE5.6）、#761、#765
- 合入社区 PR：#767 #756 #748 #711 #639 #643 #684 #705 #704 #672
- 合入 PR #771 的测试套件修复（`UnLua::Shutdown()` 前置、`GetUObject` 回读等）
- 沙盒（`lua.sandbox`）会顺带冻结 `UE` / `Lua` / `package.loaded`，导致类型懒注册与 `require` 失败；
  现在沙盒后会还回这几张表的写权限，脚本侧写入仍由 `UE` 表的 `__newindex` 拦截
- 文档把 `lua.sandbox` 写成了可执行命令，实际它是启动期 CVar（每个环境创建时读取一次）
- **撤下 Luau 调试 hook 必崩**：`UNLUANOVA_SET_DEBUG_HOOK` 被映射到了 `lua_callhook()`，而它不是
  setter —— 它是"就地调用一次 hook"。于是 `FLuaDebugService::Attach()` 什么都没装上，
  `Detach()` 传 nullptr 时则带着空函数指针、且 `L->ci` 停在 `base_ci`（`L->ci->p` 为空）
  走进 `luau_callhook()`，在 `lvmexecute.cpp` 里解引用一个不存在的 proto —— 编辑器直接
  `EXCEPTION_ACCESS_VIOLATION reading address 0x88`。
  现在改为写 `lua_callbacks(L)->debugbreak` / `debugstep`（Luau 真正回调调试 hook 的两个槽位），
  nullptr 即撤下；顺带修好 `lua.dbg` 的单步：此前没有任何回调装在 `debugstep` 上，
  `SetSingleStep()` 是静默失效的

### Reverted
- **PR #719**：其守卫会在子类实例上跳过 Lua 覆写（详见 `Docs/Archive/` 说明），
  需先补「子类实例仍能执行 Lua 覆写」的测试再评估

### Known Issues
- **剩余测试失败的分类结论**（Luau 357 条里 32 条失败，与 Lua 后端 367 条里 23 条失败逐条比对得出）：
  - **17 条两个后端都挂**，与后端分叉无关，全部是资产/数据层：
    6 条是 `Failed to find object 'Class .../BP_X.BP_X_C'`（Multiple / Overridden / Issue288 /
    Issue295 / Issue314 / Issue362 / Issue398，`.uasset` 在盘上但生成类取不到，需要在编辑器里
    resave 重新编译）；3 条是 `Missing RowStruct`（DataTable_CppTest / DataTable_Issue583 的
    RowStruct 结构已不存在）；Issue634 是蓝图编译失败（`Issue 634Struct` 结构体已不存在）；
    其余 6 条（Issue286 / 300 / 401 / 434 / 443 / 453）是蓝图脚本没写回期望的全局变量，需要运行时定位
  - 另有 4 条「按引用传递参数」是 chunk 里类名大小写写错（`UE.FUNLUANOVATestLib` /
    `UE.UUNLUANOVATestFunctionLibrary`，实际是 `FUnLuaNovaTestLib` / `UUnLuaNovaTestFunctionLibrary`），
    取到 nil，两个后端一样；已改正
  - **Luau 独有**里剩下的：`创建Actor，指定初始Transform...`（Translation 取到 `98.451/120.737/1`，
    Z 对而 X/Y 是脏值，像是别名/复用内存，需运行时确认）与 Issue291（委托 `attempt to call a nil value`）
  - `UE.FUNLUANOVATestLib` / `UE.UUNLUANOVATestFunctionLibrary` 在 Luau 下取到 nil（plain Lua 能取到），
    是「按引用传递参数」4 条在 Luau 上多出来的一层失败
- 尚未在 UE 5.8 上编译验证
- UCLASS 改名需要 resave 资产与迁移 `Config/*.ini`
- Luau 后端未接线：`UNLUANOVA_LUA_BACKEND` 默认 0，`LuaCore` 仍依赖 Lua 5.4 内部头

[Unreleased]: https://github.com/HUSTJJD/UnLuaNova/commits/master

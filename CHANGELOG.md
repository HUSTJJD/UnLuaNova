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
- `Tools/RunTests.ps1` 切换 Lua 后端改为**原子操作**：后端是编译期选择，ini 与二进制一旦分叉，
  结果就会被记在错误的后端名下（真发生过：目录叫 `lua`，跑的是 Luau 的二进制）。现在加了
  单实例锁（`Saved/RunTests.lock`，带持有者 PID）、编译失败立刻还原 ini、启动编辑器前按
  UBT 生成的 `Definitions.UnLuaNova.h` 校验磁盘上真正的后端，`-SkipBuild` 与切后端互斥
- **`Tools/RunTests.ps1` 默认不再 `-NullRHI`**：无头更快，但那不是"同一套用例跑得快一点"——
  UMG 控件在无渲染时不会被 Tick（Issue434 因此永远等不到委托广播，看着就像绑定坏了），
  框架会把这类用例整条剔除。默认改成带真实视口跑全套，要快再加 `-NullRHI`（此时报告总数会少
  几条，那几条是没跑，不是通过）。`-Screenshots` 现在只管"收不收集 PNG"，不再兼管渲染开关

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
- **测试进程崩溃后报告只剩"0 tests"**：崩溃时自动化框架不导出任何 JSON，报告与"压根没跑起来"
  长得一模一样，只能去翻几百 KB 的 `Editor.log`。现在 `GenerateTestReport.py` 会从中抓出
  **崩溃时正在跑的用例**和调用栈，写进报告的「崩溃」一节，`summary.json` 增加 `crashed` /
  `crash_test`；`RunTests.ps1` 同时把崩溃栈打到控制台并把退出码置 1（否则 CI 会当成通过）
- **Luau 没有 `collectgarbage`**：Luau 的基础库只到 `gcinfo()`，凡是强制 gc 的脚本（Issue291
  就是靠它证明委托绑定能活过一次 gc）都死在 `attempt to call a nil value`。现在
  `LuaScriptCompat` 在名字空闲时补上（plain Lua 自带，不会被覆盖），支持
  `collect` / `step` / `count`，单位与 plain Lua 一致
- **Luau 的 `%s` 只吃字符串**：`string.format("%s", 整数或布尔)` 在 Luau 下是
  `invalid argument #4 to 'format' (string expected, got integer)`，plain Lua 会自动
  `tostring`。`Tests/Binding/BP_UnLuaTestActor.lua` 的错误拼接因此炸掉，连带 Dynamic /
  Static / Priority 三条绑定用例在 Luau 上失败；已改为显式 `tostring()`
- **Issue362 的 chunk 里类名大小写被改名脚本改坏**：`UE.UUNLUANOVATestStub` 应为
  `UE.UUnLuaNovaTestStub`（改名把 `UnLua` 替换成 `UNLUANOVA` 时把 `UUnLua...` 拼坏了），取到 nil，
  两个后端都失败
- **Issue288 / Issue295 按新类名取 UMG 类**：资产文件改名成了 `UnLuaNovaTestUMG_Issue288`，
  但里面的 `BlueprintGeneratedClass` 仍是 `UnLuaTestUMG_Issue288_C`（改文件名不会改资产里的类），
  `UE.UClass.Load` 因此拿到 nil、`NewObject` 报 `Invalid class!`。已按资产里真实存在的类取
- **Luau 下 int64 属性没法跟脚本里的字面量比较**：Luau 的相等是先比 tag 的
  （`equalobj()` 就是 `ttype(o1) == ttype(o2) && luaV_equalval(...)`），`LUA_TINTEGER` 永远不等于
  `LUA_TNUMBER`，而 Luau 编译器把每个字面量都编成 number。于是 `self.Integer64Property ~= 3`
  对"值就是 3"的属性也成立，对它做算术还会报 `attempt to perform arithmetic (add) on integer
  and number`。现在 `UNLUANOVA_PUSHINT64` 对 |v| ≤ 2^53 的值推 number（double 能精确表示这些整数），
  只有超出 double 精度的才保留 64 位整型 tag —— 需要精确时仍然精确，其余情况回到脚本可用的状态
- **`UNLUANOVA_IS_YIELDABLE` 用例按 plain Lua 的语义断言主线程**：Luau 的
  `lua_isyieldable()` 是 `nCcalls <= baseCcalls`，主线程在没跨过 C 边界时回答"可 yield"，
  所以 `== 0` 在 Luau 上必然失败。现在按后端断言，并额外验证"跨过 C 边界后两边都回答不可 yield"

### Reverted
- **PR #719**：其守卫会在子类实例上跳过 Lua 覆写（详见 `Docs/Archive/` 说明），
  需先补「子类实例仍能执行 Lua 覆写」的测试再评估

### Added
- **`UnLuaNova.Editor.RepairTestAssets`**（`Private/Tests/AssetRepairTest.cpp`）：一个 editor-context
  的自动化用例，把改名留下的资产损伤修掉 —— 给父类引用丢了的蓝图补回父类（包里的 `ParentClass`
  导入解析不出来，编译器只说 `has missing or NULL parent class`，这个信息在资产里已经没了，
  所以按资产写死在用例里）、编译并保存 `/UnLuaNovaTestSuite/` 下所有蓝图（未编译的包里没有
  `SKEL_*` / `Default__*` / `BlueprintGeneratedClass`，`Class ..._C` 取不到），
  把 `DataTable_CppTest` / `DataTable_Issue583` 的 RowStruct 与示例行补回去、把改名时从关卡里
  一起丢掉的 actor 放回地图、把 struct 丢失的 `MakeStruct` 节点与函数参数类型补回（连函数签名
  一起修，并重新接上 struct → 返回值的连线）。
  编译跑两遍，因为调用别蓝图函数的蓝图要等对方先编译好。
  扫描时**不加载 World**：加载关卡会编译它的关卡蓝图，而 Issue500 / Issue634 的关卡蓝图有改名
  留下的坏节点，那些 `LogBlueprint` 的 Error 会记在这条用例头上；真正要写 actor 的关卡是按名字
  单独加载的
  它会改资产，所以**不在默认的 `UnLuaNova.API` 过滤器下**，要显式跑：
  `Tools\RunTests.ps1 -Backend Luau -Filter UnLuaNova.Editor`

### Known Issues
- **剩余测试失败的分类结论**（Luau 16 条失败、Lua 15 条失败，逐条比对 + 直接读 `.uasset` 二进制得出）：
  - 上一轮修掉的：Issue288 / Issue295（按资产里真实的类名取 UMG 类）、Issue362（chunk 里
    `UE.UUNLUANOVATestStub` 大小写）、Issue291（Luau 缺 `collectgarbage`）、Dynamic / Static /
    Priority 的 `string.format` 与 int64 比较、`UNLUANOVA_IS_YIELDABLE`
  - **资产未编译（3 条，两个后端）**：`Tests/Binding/BP_UnLuaNovaTestActor_StaticBindingChild`
    （Multiple / Overridden）、`Tests/Regression/Issue314/BP_UnLuaNovaTestActor_Issue314_Child`
    （Issue314）、`Tests/Regression/Issue398/BP_Character`（Issue398）这三个 `.uasset` 里既没有
    `SKEL_*` 也没有 `Default__*`，更没有 `BlueprintGeneratedClass` 导出——是"未编译"状态，
    所以 `Class ..._C` 取不到。同目录的 Parent / 兄弟蓝图都有，说明是落盘时没编译，
    **必须在编辑器里编译并保存**（`BP_Character` 内部还残留旧包路径 `/UnLuaTestSuite/`）
  - **DataTable（3 条，两个后端）**：`Tests/Misc/DataTable_CppTest.uasset` 的 RowStruct 是空的
    （资产很旧，只剩一个 `RowStructure` 属性名，没有任何 `/Script/...Row` 引用），
    需要设成 `UnLuaNovaTestTableRow` 并补数据；`Tests/Regression/Issue583/DataTable_Issue583.uasset`
    引用的是 `/Script/UnLuaNovaTestSuite.Issue583Row`（结构体还在），但同样需要 resave
  - **Issue634**：`Test` 函数图里的 `MakeStruct` 节点丢了 struct（改名后模块里已没有
    `Issue634Struct`），连带的 `NewParam` 参数也丢了类型，蓝图编译不过。现在由 `RepairTestAssets`
    三级修复：struct 填回节点；参数按 `RemoveUserDefinedPinByName` + `CreateUserDefinedPin` 重建
    （签名存在 `UserDefinedPins` 里，直接改 pin 的 `PinSubCategoryObject` 会被下次重建覆盖，
    编译照样说 `had null struct object`）；再把 struct → 返回值的连线接回去。
    修好后用例通过 —— 之前"一修就崩编辑器"是**半修状态**（签名还是错的）造成的，不是插件 bug
  - **"值不回传"（6 条，两个后端一致）**：Issue286 / 300 / 401 / 443 / 453 其实不是运行时问题 ——
    是地图里的 actor 丢了（改名时一起没的），`RepairTestAssets` 按包路径放回去后全部通过。
    剩下的 Issue434 是另一种情况（见下）
  - **Issue434（两个后端）**：不是委托绑定坏了。插桩后看得很清楚：`Construct` 跑了、
    `BP_CreateEntry` 拿到了 3 个 entry、`:Add(self, M.OnTestDelegate)` 加了 3 个 handler、
    在 `Construct` 里直接 `Broadcast` 时三个 handler 全部命中（`Result1 == 7`）—— 但控件的
    `Tick` 在 `-NullRHI` 下**根本不会被调用**（Slate 无渲染 → `SObjectWidget::Tick` 不跑），
    委托没人广播，`Result1` 就一直是 0。走真实视口时同一条用例通过。
    已改用 `BEGIN_VIEWPORT_TESTSUITE`（`EAutomationTestFlags::NonNullRHI`）：无头跑时框架把它
    剔除，而不是记一条失败 —— 它没跑，不算通过也不算失败
  - **Luau 独有 1 条**：`创建Actor，指定初始Transform/CollisionHanding/Owner/Instigator`
    （5 个参数版 Translating 的 X/Y 是脏值）已随其余一并修好，当前两个后端均通过
- 尚未在 UE 5.8 上编译验证
- UCLASS 改名需要 resave 资产与迁移 `Config/*.ini`
- Luau 后端未接线：`UNLUANOVA_LUA_BACKEND` 默认 0，`LuaCore` 仍依赖 Lua 5.4 内部头

[Unreleased]: https://github.com/HUSTJJD/UnLuaNova/commits/master

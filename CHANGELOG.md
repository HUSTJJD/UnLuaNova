# Changelog

UnLuaNova 的变更记录。格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本遵循 [Semantic Versioning](https://semver.org/lang/zh-CN/)。

> 本项目源自 [Tencent/UnLua](https://github.com/Tencent/UnLua)（MIT）。
> 上游 2019–2023 年的历史变更请见 `Docs/Archive/UnLua_CHANGELOG.md`，或查阅本仓库的 git 历史。

## [Unreleased]

### Added
- **静态绑定层** `StaticBinding/`：POD 描述符 + 注册表（延迟解析）+ 自动注册器
- **`UnLuaStaticBindingGenerator`** Commandlet：构建期为声明类型生成静态绑定表
- `CallUEFast`：对已解析函数跳过 `GetFunctionCallspace` / 覆写重定向查找 / 接口函数查找
- **Lua 后端抽象** `LuaBackend.h`：吸收 Lua 5.4 与 Luau 的 API 差异（`UNLUANOVA_LUA_BACKEND` 开关）
- Luau 以 git submodule 引入（`Source/ThirdParty/Luau`，0.743）+ `Luau.Build.cs`
- 改名脚本 `Tools/Rename-To-UnLuaNova.ps1`

### Changed
- 引擎基线升到 **UE 5.8.3**：`BuildSettingsVersion.V6` + `IncludeOrderVersion.Latest`
- `LuaOverridesClass` 的 UField 链遍历适配 UE 5.7+（`UField::Next` 已是 `TObjectPtr`）
- 项目全面改名 `UnLua` → `UnLuaNova`（模块、UCLASS、宏、文件名、Lua 全局名、ini section）
- `EInternalObjectFlags::AsyncLoading` → `EInternalObjectFlags_AsyncLoading`（UE 5.8 已移除该枚举项）
- `FSoftObjectPtr::operator=` 导出返回类型修正为 `FSoftObjectPtr&`
- `UNLUA_LOGERROR` 格式串缺参修正（原为未定义行为）

### Fixed
- 合并上游 `develop`：PR #700（UE5.4）、#707、#758（UE5.6）、#761、#765
- 合入社区 PR：#767 #756 #748 #711 #639 #643 #684 #705 #704 #672
- 合入 PR #771 的测试套件修复（`UnLua::Shutdown()` 前置、`GetUObject` 回读等）

### Reverted
- **PR #719**：其守卫会在子类实例上跳过 Lua 覆写（详见 `Docs/Archive/` 说明），
  需先补「子类实例仍能执行 Lua 覆写」的测试再评估

### Known Issues
- 尚未在 UE 5.8 上编译验证
- UCLASS 改名需要 resave 资产与迁移 `Config/*.ini`
- Luau 后端未接线：`UNLUANOVA_LUA_BACKEND` 默认 0，`LuaCore` 仍依赖 Lua 5.4 内部头

[Unreleased]: https://github.com/HUSTJJD/UnLuaNova/commits/master

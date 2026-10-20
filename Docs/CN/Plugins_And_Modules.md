# 插件与模块说明

## UnLuaNova

核心功能插件，安装使用UnLuaNova时至少要保证你的工程Plugin目录下有它。

模块列表：
* UnLuaNova 主要运行时模块
* UnLuaNovaEditor 编辑器模块，提供Lua模版生成/智能提示导出/控制台命令等功能
* UnLuaNovaDefaultParamCollector 编码模块，收集UFUNCTION的默认参数

## UnLuaNovaExtensions

UnLuaNova扩展示例插件，在需要集成一些第三方库时可以参考它的实现。

这三个插件默认都是开启的（`EnabledByDefault: true`）。不需要的话在工程 `.uproject` 里把对应的
`"Enabled"` 改成 `false` 即可，它们之间没有依赖。

模块列表：
* [LuaSocket](https://github.com/lunarmodules/luasocket) 一些纯Lua调试器可能会需要这个库
* [LuaRapidjson](https://github.com/xpol/lua-rapidjson) JSON 编解码
* [LuaProtobuf](https://github.com/starwing/lua-protobuf) protobuf 编解码

它们都是 Lua C 模块，由插件在 `FLuaEnv::OnCreated` 时注册成内建 loader，Lua 侧直接
`require("socket.core")` / `require("rapidjson")` / `require("pb")` 即可。C 源码是第三方代码，
不直接包含 `lua.h`：统一包含 UnLuaNova 提供的 `lua.hpp`，由它按 `LuaBackend` 选择后端并补齐
Luau 没有的 lauxlib 拼写，所以切换 Lua/Luau 后端不需要改这些插件。

## UnLuaNovaTestSuite

自动化测试插件，覆盖了UnLuaNova提供的API的规范测试以及一些Issue对应的回归测试。
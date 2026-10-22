---
title: "智能提示 IntelliSense"
description: "生成引擎类型符号，让 VSCode 等 IDE 对 UE API 提供补全"
---

# 智能提示
## 1. 生成智能提示信息
![生成智能提示信息](../Images/generate_intellisense.png)

打开UnLuaNova工具栏，点击导出智能提示，会在`{UE工程}/Plugins/UnLuaNova/IntelliSense`下生成声明文件。

这个目录是每台机器自己生成的产物（已 gitignore）：内容取决于本机装了哪些插件和模块，入库
会让它每次都变。它不在 `Intermediate/` 里 —— 那是 UBT 的构建中间产物目录，随时可能被清掉，
而这里是给 IDE 和静态检查器看的东西（注意别和 `Content/IntelliSense` 搞混：后者是手写的
注解源文件，是要入库的）。

## 2. 加入到LuaIDE

![VSCode工作区](../Images/vscode_workspace.png)

以VSCode为例，参考示例工程的[UnLuaNovaDemo.code-workspace](https://github.com/HUSTJJD/UnLuaNova/blob/master/UnLuaNovaDemo.code-workspace)。一个工作区中分别添加了`Script`和`IntelliSense`，这样每次生成智能提示信息后会自动刷新，就不需要再手动拷贝了。

## 3. 完成

![UE智能提示](../Images/ue_intellisense.png)

验证智能提示信息是否工作，只需要在工程的任意lua文件中输入`UE.`，正常应该就会显示引擎下所有类型的提示了。

如果没有出现提示，可以检查下IDE的智能提示插件是否存在或启用。

## 4. Luau 类型声明（给 luau-analyze 用）

同一次导出还会额外生成一个 `UnLuaNova.d.lua`：

一次导出产出**两种方言，分两个目录**：

```
Plugins/UnLuaNova/IntelliSense/
├── EmmyLua/             Lua 方言，给 IDE 补全 + LuaLS（Lua 后端静态检查）
│   ├── UE.lua           引擎类型的聚合入口
│   ├── UnLuaNova.lua    UnLuaNova 自己的 API（从插件 Content 拷入）
│   └── <模块>/<类型>.lua 按 UE 包名分目录，如 Script/Engine/、Game/Foo/
└── Luau/                Luau 方言，给 luau-analyze（Luau 后端静态检查）
    └── UnLuaNova.d.lua
```

分开是因为两个分析器解析不了对方的语言：把 `declare class` 喂给 LuaLS 只会得到语法错误。

`EmmyLua/` 是给 IDE 和 LuaLS 看的类型注解，`Luau/UnLuaNova.d.lua` 是给静态检查器看的
`declare class` / `declare function` 声明 —— **不要 `require` 它**，它只在检查阶段被引用。
把 `.d.lua` 放进脚本工程、脚本头部写 `--!strict`，就可以用 `luau-analyze` 把 UE API 误用
提前暴露出来。

详见[Luau 后端](Luau.md#三类型声明与-luau-analyze)。

---

## 延伸阅读

- [调试](Debugging.md) — 调试 Lua 脚本
- [Luau 后端](Luau.md) — 类型声明与 luau-analyze 静态检查
- [常见问题 FAQ](FAQ.md) — 智能提示相关问题
- [设置项](Settings.md) — 「生成智能提示信息」设置项

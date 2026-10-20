---
title: "智能提示 IntelliSense"
description: "生成引擎类型符号，让 VSCode 等 IDE 对 UE API 提供补全"
---

# 智能提示
## 1. 生成智能提示信息
![生成智能提示信息](../Images/generate_intellisense.png)

打开UnLuaNova工具栏，点击导出智能提示，会在`{UE工程}/Plugins/UnLuaNova/Intermediate`下生成`IntelliSense`的目录。

## 2. 加入到LuaIDE

![VSCode工作区](../Images/vscode_workspace.png)

以VSCode为例，参考示例工程的[UnLuaNovaDemo.code-workspace](https://github.com/HUSTJJD/UnLuaNova/blob/master/UnLuaNovaDemo.code-workspace)。一个工作区中分别添加了`Script`和`IntelliSense`，这样每次生成智能提示信息后会自动刷新，就不需要再手动拷贝了。

## 3. 完成

![UE智能提示](../Images/ue_intellisense.png)

验证智能提示信息是否工作，只需要在工程的任意lua文件中输入`UE.`，正常应该就会显示引擎下所有类型的提示了。

如果没有出现提示，可以检查下IDE的智能提示插件是否存在或启用。

## 4. Luau 类型声明（给 luau-analyze 用）

同一次导出还会额外生成一个 `UnLuaNova.d.lua`：

```
Plugins/UnLuaNova/Intermediate/IntelliSense/
├── *.lua                EmmyLua 注解，给 IDE 补全用（上面三步讲的就是这个）
└── UnLuaNova.d.lua      Luau 声明，给 luau-analyze 做静态检查用
```

前者是给 IDE 看的类型注解，后者是给静态检查器看的 `declare class` / `declare function` 声明 ——
**不要 `require` 它**，它只在检查阶段被引用。把 `.d.lua` 放进脚本工程、脚本头部写 `--!strict`，
就可以用 `luau-analyze` 把 UE API 误用提前暴露出来。

详见[Luau 后端](Luau.md#三类型声明与-luau-analyze)。

---

## 延伸阅读

- [调试](Debugging.md) — 调试 Lua 脚本
- [Luau 后端](Luau.md) — 类型声明与 luau-analyze 静态检查
- [常见问题 FAQ](FAQ.md) — 智能提示相关问题
- [设置项](Settings.md) — 「生成智能提示信息」设置项

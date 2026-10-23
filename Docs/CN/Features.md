---
title: "功能清单"
description: "UnLuaNova 的能力总览，以及相对 UnLua v2.3.6 的增量"
---

# 功能清单

## 概览
* 静态绑定和动态绑定
* 编辑器环境下模拟服务端/客户端
* 蓝图模版生成与导出
* 手动静态导出
* 结构体访问优化
* 通过 `UE` 命名空间访问C++类型

## 平台支持
* 运行平台：Windows / Android / iOS / Linux / OSX
* 引擎基线：**Unreal Engine 5.8.3**（源码版）；代码保留了 4.17–5.x 时期的兼容写法，但不做验证

## 支持使用Lua替换实现
* 蓝图里所有标记了 `BlueprintImplementableEvent` 或 `BlueprintNativeEvent` 的"蓝图事件"
* 所有网络复制事件通知（Replication Notify）
* 所有动画事件通知（Animation Notify）
* 所有输入事件通知（Input Event）

## Lua调用蓝图
* 支持访问所有 `UCLASS`, `UPROPERTY`, `UFUNCTION`, `USTRUCT`, `UENUM`
* 支持访问已被覆盖掉的原蓝图逻辑
* 支持所有标记了 `BlueprintCallable` 或 `Exec` 的 `UFUNCTION` 的默认参数
* 支持在协程里调用 `Latent` 函数

## Lua调用C++
* 单播委托绑定、解绑、触发
* 多播委托绑定、解绑、清理、广播
* 访问 `UENUM`
* 支持[自定义碰撞枚举](CollisionEnum.md)
* 成员变量
* 成员函数
* 静态成员变量
* 静态成员函数
* 全局函数

## C++调用Lua
* 全局函数
* 全局表里的函数

## 其他工具
* 支持[自定义生成Lua模版](CustomTemplate.md)

## 相对 UnLua 的增量

以下是 UnLuaNova 在 UnLua v2.3.6 之上新增的能力。

> 想知道**每一项为什么这么做、换来了什么、代价在哪**：
> [相对 UnLua：原理与优势](WhyUnLuaNova.md)。本文只列"有什么"。

### 双 Lua 后端

* 支持 **Lua 5.5** 与 **Luau** 两个后端，默认 Luau，通过配置切换（见[设置](Settings.md#lua后端)）
* 两者均以 git 子模块引入，克隆后需执行 `git submodule update --init --recursive`
* 后端差异全部收敛在 `LuaBackend.h`；上层功能定义在抽象层，由各后端提供实现
* Luau 后端能用上哪些语言特性、哪些还没打通：见[Luau 后端](Luau.md)

### 可观测性

* **CPU 性能分析**：Lua 函数级数据写入 Unreal Insights，`lua.profiler` 运行时开关
* **内存统计**：总量（占用 / 峰值 / 分配次数）与分类（内存类别 / Lua 类型 / userdata 标签），`lua.memstats`
* **调试服务**：headless 断点、单步、调用栈与局部变量查看，`lua.dbg`
* **沙盒**：加固 Lua 全局表，`lua.sandbox`
* **死循环检测**：与 CPU 性能分析可同时开启，通过 VM Hook 分发器共享 Hook 槽，不再互相覆盖

以上命令的用法见[控制台命令](ConsoleCommand.md)。

### 静态绑定

* 类型选择采用 **opt-out**：默认导出全部类型，只需给不想要的类型标记 `meta=(UnLuaNovaNoExport)`
* 在某个基类上标记 `meta=(UnLuaNovaExport)` 即可把整个类族拉回，适合类型数量庞大的项目
* 读取路径不使用独占锁，类型数量增长不会放大单次访问开销
* 详见[静态导出与静态绑定](StaticExportBinding.md)

### 脚本侧类型检查

* 可生成 Luau `.d.lua` 类型声明，交由 `luau-analyze` 做静态检查，把一部分 UE API 误用提前到编译期
* Lua 后端走同一套思路的另一半：编辑器导出的 EmmyLua 注解交给 LuaLS 做静态检查
  （两个后端各用自己的分析器 —— 互相解析不了对方的语言，见[测试](Testing.md)）
* 见[智能提示](IntelliSense.md)与[Luau 后端](Luau.md#三类型声明与-luau-analyze)

### 字符串扩展

* 纯 Lua 实现的常用字符串函数（`split` / `strip` / `startswith` / `endswith` / `count` / `join`）
* 环境启动时自动装到全局 `string` 表上，脚本直接用即可；`join` 只在模块表里
  （`local StringEx = require("UnLuaNova.StringEx")`）
* `split` / `count` 按**字面量**分隔，不做模式匹配 —— 传 `"."` 得到的是按点分隔，不是空表
* 见 [API 参考](API.md#字符串扩展)

### 容器与调用路径

* `TSet` 支持 `pairs` 迭代，产出 `(元素, true)`；此前 `pairs(Set)` 在两个后端都会报错，
  只能靠 `ToArray()` / `ToTable()`
* `CallUEFast`：对已解析过的函数跳过 `GetFunctionCallspace` / 覆写重定向查找 / 接口函数查找
* `LuaScriptCompat` 兼容层：把 Lua 5.x 有而 Luau 没有的东西补上（例如 Luau 的基础库没有
  `collectgarbage`，只有 `gcinfo()`），脚本不必为后端差异写两套代码

### 示例与截图

* 文档站的示例截图由自动化测试产出（`UnLuaNova.Showcase.*`），不是手截的，见[测试与自动化](Testing.md)
* 录一段帧序列并编码成 `.mp4` 嵌进测试报告（**默认就录**，`-NoVideo` 才不录）；
  编码器来自 `imageio-ffmpeg` 这个 pip 包，不需要装系统级 ffmpeg

---

## 延伸阅读

- [相对 UnLua：原理与优势](WhyUnLuaNova.md) — 每个增量的痛点 / 原理 / 收益 / 边界
- [编程指南](UnLuaNova_Programming_Guide.md) — 编程指南
- [静态导出与静态绑定](StaticExportBinding.md) — 静态绑定如何工作
- [Showcase 示例](Showcase.md) — 可运行示例

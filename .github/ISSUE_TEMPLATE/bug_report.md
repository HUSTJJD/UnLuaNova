---
name: Bug 报告
about: 报告本插件自身的缺陷；安全漏洞请勿用本模板，改用 Security Advisories 私有报告
title: "[Bug] "
labels: bug
---

> 安全漏洞请不要开公开 issue，请使用仓库 Security 标签页的 "Report a vulnerability"
> 私有报告（见仓库根目录 SECURITY.md）。
> 提交前请先查在线文档与常见问题 FAQ，入口见 issue 选择页的联系链接。

## 环境

<!-- 尽量填全；拿不到的项写「未知」，不要删掉字段 -->

- **UE 版本**（含是否源码构建）：
- **Lua 后端**（Lua 5.5.1 / Luau，见 `Config/DefaultUnLuaNova.ini` 的 `LuaBackend`）：
- **平台**（操作系统 / 架构，如 Windows 11 / Win64）：
- **构建配置**（Editor / Game / Server / Shipping）：
- **是否跑过 `Tools/RunTests.ps1`**（通过 / 失败 / 未跑，注明原因）：

## 复现步骤

1.
2.
3.

<!-- 附上最小可复现工程、Content/Script/ 脚本片段与相关 ini 配置 -->

## 期望行为与实际行为

**期望**：

**实际**：

## 日志与验收摘要

<!-- 相关 UE 日志、崩溃调用栈、控制台输出；粘贴前去掉敏感信息 -->

```text

```

<!-- 若跑过验收，附上 Saved/ReleaseTests/ 下的摘要，尤其是
     release-summary.json 的相关片段；CI 不做 UE 编译，不能代替本地验收 -->

```json

```

## 其他补充

<!-- 是否两个后端都复现、是否与特定引擎版本或平台相关、临时规避办法等 -->

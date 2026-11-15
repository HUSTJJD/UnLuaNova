---
title: 运营期脚本包热更新（参考实现）
description: 脚本包格式、构建、签名、下发、验签、差分、灰度、回滚的闭环参考方案，及其明确的安全边界与未验证状态。
---

# 运营期脚本包热更新（参考实现）

## 1. 概述与定位

插件当前已经具备「加载路径优先级」与「自定义 Loader」两项能力：默认加载会优先
查找 `FPaths::ProjectPersistentDownloadDir()` 下的脚本，见 [常见问题](FAQ.md)；
也可以用自定义加载器完全定制加载策略，见
`Content/Script/Tutorials/12_CustomLoader.lua`。但仓库此前**没有**把「包构建 → 签名 →
下发 → 验签 → 差分应用 → 灰度 → 一键回滚」串成一套可运行、可测试的闭环。

本页给出这样一套**脚本包闭环参考实现**。它由 `Tools/ScriptDelivery/` 下的 Python
标准库脚本与 `Tools/tests/test_script_delivery.py` 单元测试组成，用来说明**包格式**
与**接收侧校验契约**。

必须首先明确它的边界：

- 这是**源码脚本包**方案，不是 `luac` 字节码包。Luau 后端明确不接受字节码输入，
  不承诺字节码 / 版本兼容，见 [Luau 后端](Luau.md)。
- **网络下载器不在本参考实现内**。本方案只定义「接收侧契约」，不实现下载、不发起
  任何网络请求、不实现 CDN 与灰度服务。
- 这是**参考实现**：通过 Python 单元测试，**未经 UE 运行时验证、未接入 41 阶段 Release
  门禁、移动端（Android / iOS）未验收**。当前验证平台只有 Win64，且仅覆盖 Python
  工具本身，不代表 UE 运行时已跑通。详见第 12 节。

脚本根目录为 `Content/Script/`，模块路径点号分隔且不带后缀：`require "A.B.C"`
对应文件 `A/B/C.lua`。包内路径一律用 `/` 分隔的 POSIX 相对路径。

## 2. 包格式

一个「包」是一个目录，包含三个文件：

| 文件 | 内容 |
|---|---|
| `package.zip` | 源码脚本载荷。条目按路径排序；`ZIP_STORED`（不压缩，避免 zlib 版本差异）；时间戳固定为 1980-01-01；权限元数据固定；路径用 `/` 分隔 |
| `manifest.json` | 规范化 JSON（`sort_keys` + 紧凑分隔符 + UTF-8），描述包内每个文件与元信息 |
| `signature.json` | 规范化 JSON，覆盖 `manifest.json` 的规范化字节 |

`manifest.json` 字段：

| 字段 | 说明 |
|---|---|
| `schema` | 固定 `unluanova.script-package/1` |
| `version` | 非负整数，**单调递增**；相对上一版必须严格变大 |
| `created_utc` | UTC 时间字符串；可用 `--created-utc` 固定以复现字节 |
| `engine_compat` | 元数据字符串，例如 `UE 6.0 / Win64`；**不表示已验收** |
| `backend` | 元数据字符串：`lua55` / `luau` / `both` / `unspecified`；Lua 5.5.1 与 Luau 双后端 |
| `package_zip` | zip 文件名，固定 `package.zip` |
| `package_sha256` | 整个 zip 的 sha256，用于完整性校验 |
| `files` | 数组，每项含 `path`（相对 POSIX 路径）、`sha256`、`size` |
| `changes` | 相对上一版的 `added` / `modified` / `removed` 路径列表 |

`signature.json` 字段：`schema`、`algorithm`（`HMAC-SHA256`）、`manifest_sha256`、
`signature`。

**确定性**：zip 采用 `ZIP_STORED`、条目排序、时间戳与权限固定，规范化 JSON 保证
同样输入（含相同 `--created-utc`）在 Windows 与 Linux 上产生**同样的字节**；
Python 单测覆盖「同输入同字节」。

## 3. 构建

`build_package.py` 扫描脚本目录，生成 `package.zip` 与 `manifest.json`：

```bash
python -B build_package.py --source Content/Script --out out/v1 --version 1 \
    --engine-compat "UE 6.0 / Win64" --backend both \
    --created-utc "2026-11-15T00:00:00Z"

python -B build_package.py --source Content/Script --out out/v2 --version 2 \
    --previous out/v1/manifest.json --engine-compat "UE 6.0 / Win64" \
    --backend both --created-utc "2026-11-15T00:00:00Z"
```

传入 `--previous` 时，工具按文件 `sha256` / `size` 计算 `added` / `modified` /
`removed`；若新 `version` 未严格大于上一版，则**拒绝构建**（版本倒退被拒）。
隐藏文件与目录、以及包自身的元数据文件名不会进入载荷。

## 4. 签名

`sign_package.py` 读取 `manifest.json`，对**规范化字节**计算 HMAC-SHA256，写出
`signature.json`：

```bash
python -B sign_package.py --package out/v2 --key <hexkey>
```

因为签名覆盖的是规范化字节，磁盘上 JSON 的排版变化不影响验签结果，只要对象内容
一致。

**安全边界（重要）**：本实现用的是**对称 HMAC-SHA256**，仅用于**演示包格式与验签
流程**。签名与验签共用同一密钥，任何能验包的一方也能伪造包，因此它**不是生产级
真实性保证**。生产必须使用**非对称签名**（Ed25519 / ECDSA）配合**密钥轮换**：
私钥只留在签名服务，公钥随客户端分发；**签名密钥绝不能随包下发，也不能提交进本
仓库**。演示用十六进制密钥只用于本地 round-trip 与单元测试。

## 5. 下发

**网络下载器不在本参考实现内**。本节只定义「接收侧契约」，即宿主 / 服务端需要满足
的输入、校验与产物约定；本参考实现不实现下载、断点续传、传输加密或 CDN。

| 契约项 | 约定 |
|---|---|
| 输入 | `manifest.json`、`signature.json`、`package.zip`（全量）；或增量清单 + 对应文件（差分） |
| 传输 | 生产环境应使用 HTTPS 与服务端来源校验；本演示不提供传输层 |
| 前置校验 | 落盘前**必须**完成验签与包完整性校验（第 6 节），失败即丢弃，不激活 |
| 暂存 | 先写入临时目录，全部校验通过后再一次性提交，避免「半包」状态 |
| 落盘 | 校验通过的文件落到 `PersistentDownloadDir/Content/Script/<模块路径>` |
| 失败处理 | 任意校验失败时保留上一版本指针，不改动 `.current` |

接收侧把验签后的源码脚本落到优先加载目录，即可进入第 10 节描述的加载路径。

## 6. 验签

`verify_package.py` 依次做三层校验：

```bash
python -B verify_package.py --package out/v2 --key <hexkey>
```

1. **manifest 结构校验**：字段齐全、`version` 为整数、路径安全、哈希长度合法；
   缺字段的畸形 manifest 直接被拒。
2. **签名校验**：`signature.json` 的 `manifest_sha256` 与 manifest 规范化字节一致，
   HMAC 与签名一致；错误密钥验签失败。
3. **完整性校验**：整个 `package.zip` 的 sha256 与 manifest 一致，且包内每个文件的
   `sha256` / `size` 与 manifest 的 `files` 逐项一致；篡改任一文件后校验失败。

三层分别防「格式错误」「来源不符（演示级）」「内容损坏或篡改」。Python 单测覆盖
错误密钥、篡改 manifest、篡改 zip 内容、签名被改、畸形 manifest 等失败路径。

验签通过**不等于脚本可信**：它只证明字节与 manifest 一致、且演示来源校验通过，
不证明脚本行为安全，详见第 11 节。

## 7. 差分应用

`diff_packages.py` 基于两份 manifest 生成**增量包内容清单**（不是二进制补丁）：

```bash
python -B diff_packages.py --old out/v1/manifest.json \
    --new out/v2/manifest.json --out out/delta.json
```

输出含 `schema`、`from_version`、`to_version`、`added`、`modified`、`removed` 以及
`checks`。工具会：

- 拒绝版本倒退（`to_version` 必须大于 `from_version`）；
- **校验删除项不会误删**：每个 `removed` 路径必须在旧版中存在、在新版中确实不存在、
  是安全的相对路径，且与 `added` / `modified` 互斥。

接收侧按清单应用：`added` / `modified` 从新包取对应文件，`removed` 删除对应文件；
**先全部校验、再一次性提交**，避免删到一半失败。清单只描述内容，接收侧仍需按
第 6 节校验新包文件本身的哈希。

## 8. 灰度（按设备 / 用户分桶的版本指针）

灰度由**版本指针**驱动，而不是由包本身驱动：

- 服务端维护「桶 → 目标版本」映射，例如 `hash(device_id 或 user_id) % 100` 落入
  某个百分比桶；每个桶指向一个已构建、已验签的版本号。
- 接收侧上报桶标识，拿到该桶的目标版本，只下载并激活该版本；未命中的桶继续停留在
  旧版本。
- 流程：新版本先投小比例桶 → 观测 → 逐步扩量 → 全量；某桶异常时只把该桶指针
  回退到上一已验签版本。

`rollback.py` 管理的是**单节点** `.current` 指针，**不实现分桶服务**（诚实边界：
分桶决策、桶映射持久化、上报与下发都在参考实现之外）。多节点部署时，由服务端按桶
分发不同版本号，各节点各自调用 `switch`，等价于每个节点维护自己的 `.current`。

## 9. 一键回滚

`rollback.py` 维护 `versions/<version>/`（每个版本的不可变包副本）与一个
`.current` 指针（规范化 JSON，含 `schema` 与 `version`）：

```bash
python -B rollback.py add     --repo repo --package out/v1 --key <hexkey>
python -B rollback.py add     --repo repo --package out/v2 --key <hexkey>
python -B rollback.py list    --repo repo
python -B rollback.py switch  --repo repo --version 2 --key <hexkey>
python -B rollback.py switch  --repo repo --version 1 --key <hexkey>   # 一键回滚
python -B rollback.py current --repo repo
```

- `switch` 前会**校验目标版本包完整性**（提供密钥时还会验签），校验不过不切换。
- 目标版本**不在历史中**则拒绝切换，因此不会「倒退到不存在的版本」。
- 切到已存在的更旧版本是合法回滚，允许；这正是「一键回滚」的含义。
- `.current` 通过「临时文件 + `os.replace`」原子替换，避免写一半的指针。

注意：回滚的是**指针**，不是文件内容。脚本文件的落地与旧文件清理仍由接收侧按
版本包 / 差分清单完成，本参考实现不替 UE 侧搬运或删除脚本文件。

## 10. 与 UE 侧的衔接点

本参考实现**不修改**下列任何机制，只在落盘侧提供「可校验的包」：

- **`FPaths::ProjectPersistentDownloadDir()`**：默认加载优先该目录下的脚本。以
  Win64 为例，`require "A.B.C"` 依次尝试
  `Saved/PersistentDownloadDir/Content/Script/A/B/C.lua` 与打包内容里的
  `Content/Script/A/B/C.lua`，见 [常见问题](FAQ.md)。接收侧把验签后的文件落到该目录
  即可进入优先加载路径。FAQ 也列出 Android / iOS 的下载目录，但**本方案未在移动端
  验收**，见第 12 节。
- **`UnLuaNova.PackagePath`**：可以追加自定义查找目录，默认值为
  `Content/Script/?.lua;Plugins/UnLuaNova/Content/Script/?.lua`，见
  [常见问题](FAQ.md)。若采用自定义目录，接收侧落盘位置需与该值保持一致。
- **自定义 Loader**：`FUnLuaDelegates::CustomLoadLuaFile` 允许完全定制加载策略，示例
  见 `Content/Script/Tutorials/12_CustomLoader.lua`。它可用于把「版本指针 → 实际
  文件」的解析收拢到一处，但该集成**不在本参考实现内**。

模块路径始终点号分隔、不带后缀（`require "A.B.C"` ↔ `A/B/C.lua`），包内路径与之一致。

## 11. 安全边界

| 边界 | 说明 |
|---|---|
| HMAC 仅演示 | 对称 HMAC-SHA256 只演示包格式；生产必须非对称签名（Ed25519 / ECDSA）+ 密钥轮换，密钥不得随包下发 |
| 不可信脚本 | 验签不证明脚本行为安全；不可信脚本需**另定能力策略**（能力裁剪、代码审查、来源治理） |
| 沙盒非 OS 级 | `lua.sandbox` 的**路径检查不是 OS 级沙盒**（见 `UnLuaNovaSandbox.h:47-50` 与 [生产部署与运维](Production_Deployment.md)）；本工具不执行脚本、不做 OS 隔离 |
| 源码而非加密字节码 | 脚本是**源码文本**，不是加密或混淆的字节码；Luau 后端不接受字节码输入，见 [Luau 后端](Luau.md) |
| 元数据不代表验收 | `engine_compat` / `backend` 只是字符串，不表示任何 UE 运行时或平台验收结论 |

本工具只做字节级完整性与演示级来源校验，不读取、不修改 `Config/`、`Saved/`、
`Plugins/` 源码、`Content/` 下任何文件，也不改动任何 Release 流水线配置。

## 12. 状态与未验证项

- **已验证**：`Tools/ScriptDelivery/` 的 Python 工具通过
  `Tools/tests/test_script_delivery.py` 的单元测试（12 项），覆盖确定性打包、
  篡改失败、错误密钥、版本倒退被拒、增量清单、回滚指针、包内哈希一致、
  畸形 manifest 被拒，以及一次命令行 round-trip。
- **未经 UE 运行时验证**：没有在 UE 编辑器或打包游戏里加载过这些包，不声称热更新
  已在 UE 中跑通。
- **未接入 Release 门禁**：未纳入 [生产部署与运维](Production_Deployment.md) 描述的
  41 阶段 Release 验收，不影响现有发布流水线；相关导航与 CI 由主代理统一处理。
- **移动端未验收**：Android / iOS 未验收；FAQ 中的移动端下载目录仅为背景信息，
  见 [常见问题](FAQ.md) 与 [引擎版本与平台支持矩阵](Engine_Support.md)。
- **不含性能 / 验收数据**：不提供任何性能基准或验收统计，也不伪造这类数据。

## 延伸阅读

[生产部署与运维](Production_Deployment.md) · [Luau 后端](Luau.md) ·
[常见问题](FAQ.md) · [引擎版本与平台支持矩阵](Engine_Support.md) ·
[发布就绪度](Release_Readiness.md) · [API 稳定性](API_Stability.md) ·
[测试与自动化](Testing.md)

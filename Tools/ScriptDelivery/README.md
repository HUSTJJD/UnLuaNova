# ScriptDelivery：运营期脚本包闭环（参考实现）

本目录是一套**只依赖 Python 标准库**的脚本包工具，用于演示「构建 → 签名 → 验签 →
差分 → 回滚」的完整包格式与校验流程。所有脚本可用 `python -B <脚本>.py` 直接运行，
路径处理统一使用 `pathlib`，可在 Windows 与 Linux 上执行。

## 定位与边界（务必先读）

- 这是**参考实现**，只验证「包格式 + Python 侧校验逻辑」。它**没有接入 UE 运行时**，
  **没有纳入 41 阶段 Release 验收门禁**，也**没有在 Android / iOS 上验收**；当前仅
  在 Win64 开发环境下跑过 Python 单元测试。详见 `Docs/CN/ScriptDelivery.md`。
- 这是**源码脚本包**，不是 `luac` 字节码包。Luau 后端明确不接受字节码输入，也不承诺
  字节码 / 版本兼容，见 `Docs/CN/Luau.md`。
- **网络下载器不在本参考实现内**。本目录只定义「接收侧契约」：下载、灰度决策、把文件
  落到 `PersistentDownloadDir` 的环节由宿主 / 服务端实现，本工具不发起任何网络请求。
- `manifest.json` 里的 `engine_compat`、`backend` 只是**元数据字符串**，不代表任何
  UE 运行时验收结论。

## 文件清单

| 文件 | 作用 |
|---|---|
| `build_package.py` | 构建确定性 `package.zip` + `manifest.json`；支持上一版 manifest 计算增量 |
| `sign_package.py` | 生成 `signature.json`（演示用 HMAC-SHA256） |
| `verify_package.py` | 校验 manifest 结构、签名、包完整性；同时是其它脚本复用的格式校验中心 |
| `diff_packages.py` | 由两份 manifest 生成增量内容清单，并校验删除项不会误删 |
| `rollback.py` | 维护 `versions/<version>/` 与原子 `.current` 指针，支持列出历史与切换 |

`verify_package.py` 额外承担共享库角色：规范化 JSON 编码、manifest 校验、zip 完整性
校验、密钥解析都集中在此，其余四个脚本 `import` 它，保证全体工具对「合法包」只有
一个定义。这样本目录不引入第五个模块文件。

## 包格式

一个「包」就是一个目录，包含：

```
package.zip      确定性 zip：条目按路径排序，ZIP_STORED（不压缩），
                 时间戳固定为 1980-01-01，权限固定；路径一律用 / 分隔
manifest.json    规范化 JSON（sort_keys + 紧凑分隔符 + UTF-8）
signature.json   规范化 JSON，覆盖 manifest 的规范化字节
```

`manifest.json` 字段：

| 字段 | 说明 |
|---|---|
| `schema` | 固定 `unluanova.script-package/1` |
| `version` | 非负整数，单调递增；相对 `--previous` 必须严格变大 |
| `created_utc` | UTC 时间字符串；可用 `--created-utc` 固定以复现字节 |
| `engine_compat` | 元数据字符串，例如 `UE 6.0 / Win64`；不表示已验收 |
| `backend` | 元数据字符串：`lua55` / `luau` / `both` / `unspecified` |
| `package_zip` | zip 文件名，固定 `package.zip` |
| `package_sha256` | 整个 zip 的 sha256，用于完整性校验 |
| `files` | 数组，每项含 `path`（相对 POSIX 路径）、`sha256`、`size` |
| `changes` | 相对上一版的 `added` / `modified` / `removed` 路径列表 |

`signature.json` 字段：`schema`、`algorithm`（`HMAC-SHA256`）、
`manifest_sha256`、`signature`（HMAC 十六进制）。

增量清单（由 `diff_packages.py` 产出）：`schema`、`from_version`、`to_version`、
`added`、`modified`、`removed` 以及 `checks`（删除项确认在新版中不存在、三类集合互斥）。

版本库（由 `rollback.py` 维护）：

```
<repo>/
  versions/
    1/  (manifest.json, package.zip, signature.json 的不可变副本)
    2/
    ...
  .current   规范化 JSON：{"schema":"unluanova.current-pointer/1","version":N}
```

## CLI 用法

下列示例中 `<hexkey>` 是十六进制字符串（如 `0a1b2c...`），也可用
`--key-file <path>` 从文件读取十六进制文本。两种方式都按十六进制**文本**解释，
避免 Windows / Linux 换行差异改变密钥字节。

```bash
# 1. 构建（不传 --previous 时 added 为全部文件）
python -B build_package.py --source Content/Script --out out/v1 --version 1 \
    --engine-compat "UE 6.0 / Win64" --backend both \
    --created-utc "2026-11-15T00:00:00Z"

# 2. 构建下一版（传入上一版 manifest，得到 added/modified/removed）
python -B build_package.py --source Content/Script --out out/v2 --version 2 \
    --previous out/v1/manifest.json --engine-compat "UE 6.0 / Win64" --backend both \
    --created-utc "2026-11-15T00:00:00Z"

# 3. 签名（演示）
python -B sign_package.py --package out/v2 --key <hexkey>

# 4. 验签 + 完整性校验
python -B verify_package.py --package out/v2 --key <hexkey>

# 5. 生成增量清单（可写文件）
python -B diff_packages.py --old out/v1/manifest.json --new out/v2/manifest.json \
    --out out/delta.json

# 6. 版本登记、列出、切换 / 一键回滚
python -B rollback.py add --repo repo --package out/v1 --key <hexkey>
python -B rollback.py add --repo repo --package out/v2 --key <hexkey>
python -B rollback.py list --repo repo
python -B rollback.py switch --repo repo --version 2 --key <hexkey>
python -B rollback.py switch --repo repo --version 1 --key <hexkey>   # 回滚
python -B rollback.py current --repo repo
```

`rollback.py add` / `switch` 在提供密钥时会额外验签；不提供时仍会做 manifest 结构与
zip 完整性校验。`switch` 只接受**已登记**的版本号，切到不存在的版本会被拒绝；切到
已存在的更旧版本即合法回滚。

## 密钥管理边界（重要）

- 本实现用的是**对称 HMAC-SHA256**，只为演示**包格式与验签流程**。签名与验签共用
  同一密钥，任何能验包的一方也能伪造包，因此它**不是生产级真实性保证**。
- 生产必须使用**非对称签名**（Ed25519 / ECDSA）+ **密钥轮换**：私钥只留在签名服务，
  公钥随客户端分发；**签名密钥绝不能随包下发，也不能提交进本仓库**。
- 演示用的十六进制密钥仅用于本地 round-trip 与单元测试，不代表任何真实凭据。

## 其它安全边界

- `lua.sandbox` 的**路径检查不是 OS 级沙盒**（见 `UnLuaNovaSandbox.h:47-50`）；不可信
  脚本仍需另定能力策略。本工具只做字节级完整性 / 来源演示校验，不执行脚本、不做
  能力隔离。
- 脚本是**源码文本**，不是加密或混淆的字节码；下发内容对持有者可见。
- 本工具不读取、不修改 `Config/`、`Saved/`、`Plugins/` 源码、`Content/` 下任何文件，
  也不改动任何 Release 流水线配置。

## 与 UE 侧的衔接点（概念，未实现）

- 默认加载优先 `FPaths::ProjectPersistentDownloadDir()`，其次打包内容，见
  `Docs/CN/FAQ.md`；接收侧把验签后的包文件落到该目录即可进入优先加载路径。
- `UnLuaNova.PackagePath` 可追加自定义查找目录，见 `Docs/CN/FAQ.md`。
- `FUnLuaDelegates::CustomLoadLuaFile` 自定义 Loader 示例见
  `Content/Script/Tutorials/12_CustomLoader.lua`。
- 完整闭环、灰度分桶与接收侧契约见 `Docs/CN/ScriptDelivery.md`。

## 状态

参考实现通过 `Tools/tests/test_script_delivery.py` 的 Python 单元测试；
**未经 UE 运行时验证、未接入 41 阶段 Release 门禁、移动端（Android / iOS）未验收**。
不得据此声称已在 UE 或移动端跑过热更新，也不包含任何性能 / 验收数据。

---
title: "Runtime Script Package Delivery (Reference Implementation)"
description: "A closed-loop reference for package format, build, signing, delivery, verification, delta, canary and rollback, with explicit security boundaries and unverified status."
---

# Runtime Script Package Delivery (Reference Implementation)

## 1. Overview and Scope

The plugin already provides two capabilities: a loading-path priority (the default
loader looks under `FPaths::ProjectPersistentDownloadDir()` first, see
[FAQ](FAQ.md)) and a fully custom loader strategy, see
`Content/Script/Tutorials/12_CustomLoader.lua`. What the repository did **not**
have is a runnable, testable closed loop spanning "package build -> signing ->
delivery -> verification -> delta application -> canary -> one-click rollback".

This page documents such a **script-package closed-loop reference implementation**.
It consists of Python standard-library CLIs under `Tools/ScriptDelivery/` plus
`Tools/tests/test_script_delivery.py`, and it defines the **package format** and
the **receiving-side contract**.

Its boundaries must be stated up front:

- This is a **source-script** package scheme, NOT a `luac` bytecode package. The
  Luau backend explicitly refuses bytecode input and makes no bytecode / version
  compatibility promise, see [Luau Backend](Luau.md).
- **The network downloader is NOT part of this reference implementation.** Only
  the "receiving-side contract" is defined here. No download, no network request,
  no CDN and no canary service are implemented.
- This is a **reference implementation**: it passes Python unit tests, but it is
  **NOT validated in the UE runtime, NOT wired into the 41-stage Release gate,
  and NOT accepted on mobile (Android / iOS)**. The only validation platform is
  Win64, and even there it covers the Python tooling only, not UE runtime
  execution. See section 12.

The script root is `Content/Script/`. Module paths are dot-separated and carry
no extension: `require "A.B.C"` maps to the file `A/B/C.lua`. Paths inside a
package are always POSIX relative paths using `/`.

## 2. Package Format

A "package" is a directory containing three files:

| File | Content |
|---|---|
| `package.zip` | Source-script payload. Entries sorted by path; `ZIP_STORED` (no compression, so no zlib-version differences); timestamp fixed at 1980-01-01; permission metadata fixed; paths use `/` |
| `manifest.json` | Canonical JSON (`sort_keys` + compact separators + UTF-8) describing every file and the metadata |
| `signature.json` | Canonical JSON covering the canonical bytes of `manifest.json` |

`manifest.json` fields:

| Field | Meaning |
|---|---|
| `schema` | Fixed `unluanova.script-package/1` |
| `version` | Non-negative integer, **monotonically increasing**; must strictly grow versus the previous version |
| `created_utc` | UTC timestamp string; `--created-utc` can fix it to reproduce bytes |
| `engine_compat` | Metadata string, e.g. `UE 6.0 / Win64`; **does not imply acceptance** |
| `backend` | Metadata string: `lua55` / `luau` / `both` / `unspecified`; Lua 5.5.1 and Luau dual backends |
| `package_zip` | Zip file name, fixed `package.zip` |
| `package_sha256` | sha256 of the whole zip, used for integrity checks |
| `files` | Array of `path` (POSIX relative), `sha256`, `size` per file |
| `changes` | `added` / `modified` / `removed` path lists versus the previous version |

`signature.json` fields: `schema`, `algorithm` (`HMAC-SHA256`),
`manifest_sha256`, `signature`.

**Determinism**: `ZIP_STORED`, sorted entries, fixed timestamp and permissions,
plus canonical JSON, guarantee that identical input (including the same
`--created-utc`) produces **identical bytes** on Windows and Linux. A Python unit
test covers "same input, same bytes".

## 3. Build

`build_package.py` scans the script directory and produces `package.zip` and
`manifest.json`:

```bash
python -B build_package.py --source Content/Script --out out/v1 --version 1 \
    --engine-compat "UE 6.0 / Win64" --backend both \
    --created-utc "2026-11-15T00:00:00Z"

python -B build_package.py --source Content/Script --out out/v2 --version 2 \
    --previous out/v1/manifest.json --engine-compat "UE 6.0 / Win64" \
    --backend both --created-utc "2026-11-15T00:00:00Z"
```

With `--previous`, the tool computes `added` / `modified` / `removed` from file
`sha256` / `size`. If the new `version` is not strictly greater than the previous
one, the build is **refused** (version rollback rejected). Hidden files and
directories, plus the package's own metadata file names, are excluded from the
payload.

## 4. Signing

`sign_package.py` reads `manifest.json`, computes HMAC-SHA256 over its
**canonical bytes**, and writes `signature.json`:

```bash
python -B sign_package.py --package out/v2 --key <hexkey>
```

Because the signature covers canonical bytes, reformatting the JSON on disk does
not change the verification result as long as the object is identical.

**Security boundary (important)**: this implementation uses **symmetric
HMAC-SHA256** purely to **demonstrate the package format and verification flow**.
Signing and verification share one secret, so any party able to verify a package
can also forge one; it is **NOT a production-grade authenticity mechanism**.
Production MUST use **asymmetric signatures** (Ed25519 / ECDSA) with **key
rotation**: the private key stays in the signing service, the public key ships
with the client, and **the signing key must NEVER ship inside or alongside the
package, nor be committed to this repository**. The demo hex key is only for
local round-trips and unit tests.

## 5. Delivery

**The network downloader is NOT part of this reference implementation.** This
section only defines the "receiving-side contract": the inputs, checks and outputs
the host / server must honour. No download, resumable transfer, transport
encryption or CDN is implemented here.

| Contract item | Convention |
|---|---|
| Input | `manifest.json`, `signature.json`, `package.zip` (full); or a delta list plus the corresponding files (incremental) |
| Transport | Production should use HTTPS and server-side origin checks; this demo provides no transport layer |
| Pre-check | Signature verification and package integrity MUST complete **before** any file is placed; on failure, discard and do not activate |
| Staging | Write to a temporary directory first, then commit atomically only after all checks pass, to avoid a "half package" state |
| Placement | Verified files go to `PersistentDownloadDir/Content/Script/<module path>` |
| Failure | On any check failure, keep the previous version pointer and do not touch `.current` |

Once the receiving side places verified source scripts into the priority search
directory, they enter the loading path described in section 10.

## 6. Verification

`verify_package.py` performs three layers of checks:

```bash
python -B verify_package.py --package out/v2 --key <hexkey>
```

1. **Manifest shape**: all required fields present, `version` is an integer,
   paths are safe, hash lengths are valid. A malformed manifest with missing
   fields is refused outright.
2. **Signature**: `signature.json`'s `manifest_sha256` matches the canonical
   manifest bytes and the HMAC matches the signature; a wrong key fails.
3. **Integrity**: the whole `package.zip` sha256 matches the manifest, and every
   file's `sha256` / `size` matches its `files` entry; tampering with any file
   makes verification fail.

The three layers respectively guard against "malformed format", "origin mismatch
(demo level)" and "corrupted or tampered content". Python unit tests cover the
failure paths: wrong key, tampered manifest, tampered zip content, altered
signature, malformed manifest.

Passing verification does **NOT** mean the script is trustworthy: it only proves
that the bytes match the manifest and that the demo origin check passed. It does
not prove the script's behaviour is safe; see section 11.

## 7. Delta Application

`diff_packages.py` produces an **incremental content list** from two manifests
(NOT a binary patch):

```bash
python -B diff_packages.py --old out/v1/manifest.json \
    --new out/v2/manifest.json --out out/delta.json
```

Output contains `schema`, `from_version`, `to_version`, `added`, `modified`,
`removed` and `checks`. The tool will:

- refuse a version rollback (`to_version` must be greater than `from_version`);
- **validate that removals cannot delete by mistake**: every `removed` path must
  exist in the old version, be genuinely absent from the new version, be a safe
  relative path, and be disjoint from `added` / `modified`.

The receiving side applies the list: `added` / `modified` files are taken from the
new package, `removed` files are deleted. **Validate everything first, then
commit once**, to avoid a half-finished deletion. The list only describes content;
the receiving side still verifies the new package file hashes per section 6.

## 8. Canary (Per-Device / Per-User Bucketed Version Pointers)

Canary rollout is driven by a **version pointer**, not by the package itself:

- The server maintains a "bucket -> target version" mapping, e.g.
  `hash(device_id or user_id) % 100` lands in a percentage bucket; each bucket
  points at one built, verified version number.
- The receiving side reports its bucket, receives that bucket's target version,
  and only downloads and activates that version; buckets not selected stay on the
  old version.
- Flow: ship the new version to a small bucket -> observe -> expand -> full rollout.
  If one bucket misbehaves, roll only that bucket's pointer back to the previous
  verified version.

`rollback.py` manages a **single-node** `.current` pointer and does **NOT
implement the bucketing service** (honest boundary: bucket decisions, mapping
persistence, reporting and delivery all live outside the reference). For
multi-node deployments the server distributes different version numbers per
bucket, and each node calls `switch` on its own, equivalent to every node keeping
its own `.current`.

## 9. One-Click Rollback

`rollback.py` maintains `versions/<version>/` (an immutable package copy per
version) plus a `.current` pointer (canonical JSON with `schema` and `version`):

```bash
python -B rollback.py add     --repo repo --package out/v1 --key <hexkey>
python -B rollback.py add     --repo repo --package out/v2 --key <hexkey>
python -B rollback.py list    --repo repo
python -B rollback.py switch  --repo repo --version 2 --key <hexkey>
python -B rollback.py switch  --repo repo --version 1 --key <hexkey>   # one-click rollback
python -B rollback.py current --repo repo
```

- Before `switch`, the **target package integrity is verified** (and the
  signature too when a key is supplied); a failed check blocks the switch.
- If the target version is **not in history**, the switch is refused, so it can
  never "roll back to a non-existent version".
- Switching to an existing older version is a legitimate rollback and is allowed;
  that is exactly what "one-click rollback" means.
- `.current` is replaced atomically via "temp file + `os.replace`", avoiding a
  half-written pointer.

Note: rollback moves the **pointer**, not the file contents. Script placement and
old-file cleanup are still performed by the receiving side from the version
package / delta list; this reference implementation does not move or delete
script files on the UE side.

## 10. Integration Points with UE

This reference implementation does **not modify** any of the following
mechanisms; it only supplies a "verifiable package" on the placement side:

- **`FPaths::ProjectPersistentDownloadDir()`**: the default loader searches this
  directory first. On Win64, `require "A.B.C"` tries
  `Saved/PersistentDownloadDir/Content/Script/A/B/C.lua` before the packaged
  `Content/Script/A/B/C.lua`, see [FAQ](FAQ.md). Placing verified files there is
  enough to enter the priority loading path. The FAQ also lists Android / iOS
  download directories, but **this scheme is not accepted on mobile**, see
  section 12.
- **`UnLuaNova.PackagePath`**: custom search directories can be appended; the
  default is `Content/Script/?.lua;Plugins/UnLuaNova/Content/Script/?.lua`, see
  [FAQ](FAQ.md). If a custom directory is used, the placement location must stay
  consistent with this value.
- **Custom Loader**: `FUnLuaDelegates::CustomLoadLuaFile` allows a fully custom
  loading strategy, demonstrated in
  `Content/Script/Tutorials/12_CustomLoader.lua`. It could centralise the
  "version pointer -> actual file" resolution, but that integration is **outside
  this reference implementation**.

Module paths are always dot-separated and extension-less (`require "A.B.C"` maps
to `A/B/C.lua`), and package paths follow the same convention.

## 11. Security Boundaries

| Boundary | Statement |
|---|---|
| HMAC is demo-only | Symmetric HMAC-SHA256 only demonstrates the package format; production needs asymmetric signatures (Ed25519 / ECDSA) with key rotation, and the key must not ship with the package |
| Untrusted scripts | Verification does not prove script behaviour is safe; untrusted scripts need a **separate capability policy** (capability trimming, code review, origin governance) |
| Sandbox is not OS-level | `lua.sandbox` **path checks are not an OS-level sandbox** (see `UnLuaNovaSandbox.h` lines 47-50 and [Production Deployment](Production_Deployment.md)); this tool executes no scripts and provides no OS isolation |
| Source, not encrypted bytecode | Scripts are **source text**, not encrypted or obfuscated bytecode; the Luau backend refuses bytecode input, see [Luau Backend](Luau.md) |
| Metadata is not acceptance | `engine_compat` / `backend` are plain strings and imply no UE-runtime or platform acceptance |

This tool performs byte-level integrity and demo-level origin checks only. It
does not read or modify `Config/`, `Saved/`, `Plugins/` sources, any file under
`Content/`, or any Release pipeline configuration.

## 12. Status and Unverified Items

- **Verified**: the Python tools under `Tools/ScriptDelivery/` pass the unit
  tests in `Tools/tests/test_script_delivery.py` (12 cases), covering
  deterministic packaging, tamper failures, wrong key, refused version rollback,
  delta lists, rollback pointer, in-package hash agreement, malformed manifest
  rejection, and one command-line round-trip.
- **NOT validated in the UE runtime**: these packages have never been loaded in
  the UE editor or a packaged game; no claim is made that hot update works in UE.
- **NOT wired into the Release gate**: it is not part of the 41-stage Release
  acceptance described in [Production Deployment](Production_Deployment.md), and
  it does not affect the existing release pipeline; navigation and CI are handled
  by the main agent.
- **Not accepted on mobile**: Android / iOS have not been accepted; the mobile
  download directories in the FAQ are background information only, see
  [FAQ](FAQ.md) and [Engine Support Matrix](Engine_Support.md).
- **No performance or acceptance data**: no benchmarks or acceptance statistics
  are provided, and none are fabricated.

## Further Reading

[Production Deployment](Production_Deployment.md) · [Luau Backend](Luau.md) ·
[FAQ](FAQ.md) · [Engine Support Matrix](Engine_Support.md) ·
[Release Readiness](Release_Readiness.md) · [API Stability](API_Stability.md) ·
[Testing and Automation](Testing.md)

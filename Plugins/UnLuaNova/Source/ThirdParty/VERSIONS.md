# Vendored VM versions

Why this file exists: the plugin ships two Lua VMs, and a bug in either one is a bug we
have to reproduce, bisect and report upstream. That is impossible if the answer to "which
Luau is this?" is "some commit from around last spring" - which is what it was, the only
trace being a `LBC_VERSION_MAX` constant and a comment saying "since release/743".

Rule: **every change to `LuaLib/` or `LuauLib/` updates this file in the same commit.**
The version is part of the vendored artifact, not a note about it.

---

## Lua (`LuaLib/Lua`)

| | |
|:---|:---|
| Version | **5.5.1** |
| Where that comes from | `LuaLib/Lua/lua.h`, `LUA_VERSION_MAJOR/MINOR/RELEASE` (lines 20-22) |
| Vendored at | tag **v5.5.1**, commit `7579fc9d7ed90240487251dfb69168f8e64e9294` (`7579fc9d`, 2026-07-23) |
| Upstream | https://github.com/lua/lua |
| Local patches | see below |

`Config/DefaultUnLuaNova.ini` used to advertise `LuaVersion=lua-5.4.3` with a comment
calling it display-only, which is how a stale number ends up in a bug report. It now
carries the same number as this file; if they ever disagree, this file wins.

## Luau (`LuauLib/Luau`)

| | |
|:---|:---|
| Upstream release | **0.743 + 1 commit** (`git describe` = `0.743-1-g6d36d483`) |
| Vendored at | commit `6d36d483cb88973276af611198169f76f3ce05b0` (`6d36d483`, 2026-10-20) |
| Bytecode version | `LBC_VERSION_MAX = 14` (`Luau/Bytecode.h`) |
| Upstream | https://github.com/luau-lang/luau |

Both trees are submodules (`.gitmodules` at the repo root), so the exact commit is the
submodule pointer rather than a copied tree - and `git submodule status` in a fresh clone
lands on exactly these commits.

Keep this current on every sync:

```
git -C Source/ThirdParty/LuauLib/Luau describe --tags --long --dirty
git -C Source/ThirdParty/LuauLib/Luau rev-parse HEAD
```

`release/743` in `Luau.Build.cs` is now redundant with the row above; it is kept because it
documents *why* the 743-era code paths (`LBC_VERSION_MAX`, the incremental GC defaults) are
assumed elsewhere, not because it is the version.

---

## Local patches

Anything modified relative to upstream goes here, with the reason. A vendored tree with
unrecorded local edits is how a fix gets silently reverted by the next "just update it"
sync - and, worse, how a local fix gets reported upstream as an upstream bug.

| Tree | File | What changed | Why |
|:---|:---|:---|:---|
| - | - | (none recorded yet) | - |

If you are reading this because a sync just clobbered something: this table is the first
place to look, and the second is the entry it should have had.

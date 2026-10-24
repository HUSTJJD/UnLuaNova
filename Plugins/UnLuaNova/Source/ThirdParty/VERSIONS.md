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
| Upstream | https://www.lua.org/versions.html |
| Local patches | see below |

`Config/DefaultUnLuaNova.ini` used to advertise `LuaVersion=lua-5.4.3` with a comment
calling it display-only, which is how a stale number ends up in a bug report. It now
carries the same number as this file; if they ever disagree, this file wins.

## Luau (`LuauLib/Luau`)

| | |
|:---|:---|
| Upstream release | **not recorded** - see the gap below |
| Bytecode version | `LBC_VERSION_MAX = 14` (`Luau/Bytecode.h`) |
| Earliest known upstream | `release/743` (`Source/ThirdParty/LuauLib/Luau.Build.cs`) |
| Upstream | https://github.com/luau-lang/luau |

**Known gap, stated rather than papered over:** the vendored Luau has no recorded tag or
commit. `release/743` is a floor, not a version - it says what the tree is *at least*, not
what it *is*. Until it is filled in, an upgrade cannot answer "from where to where", and a
behaviour change cannot be attributed to a specific upstream commit.

To close it, run this in the vendored tree and paste the result above:

```
git -C Source/ThirdParty/LuauLib/Luau describe --tags --long --dirty
git -C Source/ThirdParty/LuauLib/Luau rev-parse HEAD
```

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

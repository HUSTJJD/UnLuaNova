# License and Provenance

**UnLuaNova is under the [MIT License](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE).**
No size threshold, no commercial license, no additional terms: individuals, hobbyists, studios and
companies may all use it for free, including commercially. No CLA is required — just open a PR.

## What it covers, and what it does not

MIT covers UnLuaNova's own source, documentation, scripts and samples — the parts original to this repository.

It does not cover, and does not relicense:

| Part | License | Note |
|---|---|---|
| Inherited Tencent/UnLua code | MIT, Copyright (C) 2019 Tencent | Its file headers and copyright notices remain and apply as written |
| Lua 5.5.1 (submodule) | MIT, Lua.org / PUC-Rio | Exact notice in the pinned `lua.h` |
| Luau (submodule) | MIT, Roblox Corporation / Lua.org | Preserve `LICENSE.txt` and `lua_LICENSE.txt` |
| LuaSocket, lua-protobuf, lua-rapidjson, RapidJSON | MIT, with some BSD-3 subcomponents | Granted by their own copyright holders |
| Tool submodules under `Tools/` | Their own licenses | Ship their notices if distributing their binaries |
| Unreal Engine | Epic's separate terms | This license grants no rights to engine code or assets |

Full inventory: [third-party sources and notices](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE-THIRD-PARTY.md).
Dependency revisions: [VERSIONS.md](https://github.com/HUSTJJD/UnLuaNova/blob/master/Plugins/UnLuaNova/Source/ThirdParty/VERSIONS.md).

## Why MIT

The root license used to be PolyForm Small Business 1.0.0 with additional terms. That caused two
problems: it conflicted with the MIT headers carried by a large number of inherited files, so a reader
could not tell which license applied to which code; and the size-threshold clause — "use not for the
benefit of a small business requires a separate license" — put a gate in front of the project before it
was ever used. With MIT, the root license matches the upstream headers and there is no size or
commercial condition of any kind.

## Comparison projects

[slua-unreal](https://github.com/Tencent/sluaunreal) is BSD-3-Clause. FUnLua states MIT in its README.
The relationship to slua-unreal and FUnLua is design comparison only: neither project's authors endorse
this one, and this license grants no permission to copy their source. Third-party copyright notices
for UnLua, Lua and Luau are preserved.

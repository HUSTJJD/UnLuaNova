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

## Still open

MIT resolves the license conflict itself. These remain, and are independent of the license choice:

- `Tools/GenerateNotices.py` now deterministically generates the shipped notice inventory and `--check`s it against `Saved/StagedBuilds/*/NOTICES.txt`. Full version, provenance and rights-holder scope for extensions, tools, binaries and sample assets still appear as `UNRESOLVED` entries and fail the gate. Those are rights-holder decisions, not something a script can supply.
- New third-party imports must record the exact URL / revision, keep the original notices and note modifications.
- Do not add Tencent copyright to original code, and do not remove upstream notices.

## Comparison projects

[slua-unreal](https://github.com/Tencent/sluaunreal/blob/7970ab72fd59118e1b3f86aac2d016828b54b748/LICENSE.TXT)
is BSD-3-Clause — do not call it MIT. FUnLua claims MIT in its README, but the linked license file is
absent from the audited commit; only its design is recorded here, no implementation copied.

The relationship to slua-unreal and FUnLua is design comparison only: neither project's authors endorse
this one, and this license grants no permission to copy their source. Provenance and copyright notices
for UnLua, Lua and Luau are preserved.

[Current audit](Benchmark_Review.md)

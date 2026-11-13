# Third-party Sources and Notices

Inventory checked 2026-10-24; root license changed to MIT on 2026-11-02. This document does not relicense upstream work. Retain existing copyright/permission notices and ship the applicable full notices with distributions.

The root license is now MIT, which matches the MIT headers carried by the inherited UnLua files, so the earlier root-versus-file scope conflict is resolved. **This inventory is still not a complete release compliance bundle:** the exact upstream revisions of the copied extensions and the full shipped-notice texts require the distributor / rights holders to confirm. The in-tree version identifiers that can be read from the vendored sources are recorded below; they are not upstream revision pins.

## Included Runtime Components

| Component | Location / source | License and attribution evidence |
|---|---|---|
| Tencent/UnLua | Inherited plugin source; [upstream](https://github.com/Tencent/UnLua) | MIT; Copyright (C) 2019 Tencent. Existing file headers must remain |
| Lua 5.5.1 | `Plugins/UnLuaNova/Source/ThirdParty/LuaLib/Lua` | MIT; Lua.org, PUC-Rio; exact notice in pinned `lua.h` |
| Luau | `Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau` | MIT; Copyright (c) 2019-2026 Roblox Corporation; Copyright (c) 1994-2019 Lua.org, PUC-Rio. Preserve `LICENSE.txt` and `lua_LICENSE.txt` |
| LuaSocket 3.0-rc1 | `Plugins/UnLuaNovaExtensions/LuaSocket`; [upstream](https://github.com/lunarmodules/luasocket) | MIT; `Source/src/luasocket.h` declares `LUASOCKET_VERSION "LuaSocket 3.0-rc1"` and Copyright (C) 1999-2013 Diego Nehab. Helpers report MIME / LTN12 / URL 1.0.3. Exact imported upstream revision and full distribution notice still to record |
| lua-protobuf / protoc.lua | `Plugins/UnLuaNovaExtensions/LuaProtobuf`; [upstream](https://github.com/starwing/lua-protobuf) | Upstream MIT; the in-tree sources carry no version macro (`pb.cpp` / `pb.h` only have Lua-version guards), so no revision can be asserted from this tree; matching full copyright/permission text still to record |
| serpent 0.30 | `Plugins/UnLuaNovaExtensions/LuaProtobuf/Content/Script/serpent.lua` | Local header at line 1: (C) 2012-17 Paul Kulchenko; MIT; version `"0.30"` |
| lua-rapidjson | `Plugins/UnLuaNovaExtensions/LuaRapidjson/Source/src`; [upstream](https://github.com/xpol/lua-rapidjson) | Upstream MIT; `Source/src/rapidjson.cpp` defaults `LUA_RAPIDJSON_VERSION` to `"scm"`, i.e. an unpinned snapshot. Exact imported revision and matching full notice still to record |
| RapidJSON 1.1.0 | `Plugins/UnLuaNovaExtensions/LuaRapidjson/Source/include/rapidjson` | MIT headers: Copyright (C) 2015 Tencent, and Milo Yip; `rapidjson.h` declares `RAPIDJSON_VERSION_STRING` 1.1.0. Included subcomponents may differ |
| msinttypes | RapidJSON `msinttypes/inttypes.h` and `stdint.h` | BSD-3-Clause text in those headers; Copyright (c) 2006-2013 Alexander Chemeris; retain Tencent modification notice |

Dependency pins for Lua/Luau are in [VERSIONS.md](Plugins/UnLuaNova/Source/ThirdParty/VERSIONS.md). Do not infer the version of a copied extension from the current upstream HEAD.

## Version identifiers verified in-tree (2026-11-04)

These values are read from the vendored sources themselves, not from the current upstream HEAD:

| Component | Identifier | Evidence |
|---|---|---|
| LuaSocket | `LuaSocket 3.0-rc1` | `LuaSocket/Source/src/luasocket.h:13` |
| MIME / LTN12 / URL helpers | `1.0.3` | `mime.h:16`, `ltn12.lua:29`, `url.lua:21` |
| serpent | `0.30` | `LuaProtobuf/Content/Script/serpent.lua:1` |
| lua-rapidjson | `scm` (unpinned) | `LuaRapidjson/Source/src/rapidjson.cpp:38-39` |
| RapidJSON | `1.1.0` | `include/rapidjson/rapidjson.h:75-79` |

`scm` and the absence of a lua-protobuf version macro are facts of this vendored snapshot, not blanks to fill by guessing. Before redistribution, pin each copied extension to an exact upstream URL/revision, attach its full copyright/permission notice and record local modifications; that is a distributor / rights-holder decision, not something this repository can assert on its own.

## Development Tools and Build Artifacts

- `Tools/lua-language-server` (LuaLS) and `Tools/luau-lsp` are separate pinned tool submodules, not runtime libraries. The superproject records only a gitlink; each checkout carries its own `LICENSE` / `LICENSE.md`, which governs those sources, and this repository does not relicense them. Before distributing their compiled binaries, take the exact license identifier and copyright holder **from the license file inside the pinned checkout** (not from upstream HEAD), include that full text, and add the transitive notices: LuaLS has nested submodules under `3rd/` and `meta/3rd/`, and luau-lsp vendors `luau/`. Neither tool is a runtime dependency of the plugin.
- `Tools/LuauPerfgraph/perfgraph.py` is a launcher for the pinned Luau `tools/perfgraph.py` and `tools/svg.py`; the upstream code remains under Luau's MIT notices.
- Python packages, imageio-ffmpeg and its bundled ffmpeg require package/binary-specific license inventory if redistributed. The requirement file alone is not that inventory.
- Unreal Engine is an external dependency with separate Epic terms; this repository's license does not grant rights to engine code or assets.
- Sample assets/images need an origin and redistribution-rights inventory before packaging a release.

## Comparison-only Sources

- [slua-unreal](https://github.com/Tencent/sluaunreal/blob/7970ab72fd59118e1b3f86aac2d016828b54b748/LICENSE.TXT): BSD-3-Clause, with separately licensed dependencies. No implementation imported in this audit.
- [FUnLua](https://github.com/fcscript/FUnLua/tree/7c5f6b5b4521260fe7eb2e8bd5c50500af9b0fc3): README claims MIT and links LICENSE.TXT, absent from the audited tree. Do not copy implementation until the grant and provenance are verified. This audit records design observations only.

Comparison is not endorsement. Future imports need exact URL/revision, retained notices, modification notes, dependency review and attribution; an independent implementation should record its behavioral requirements and original tests.

## MIT Permission Text

For the components identified as MIT above, preserve their component-specific copyright notices together with this permission text. This section does not replace BSD or other notices.

```
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Root License Scope

The repository root license is MIT (changed 2026-11-02), which is the same license carried by the
inherited UnLua file headers, so root and file scope now agree. Existing MIT code remains subject to
its own notices; BSD and other notices above are unaffected. The root license covers only UnLuaNova's
own source, documentation, scripts and samples — it does not relicense the components listed above,
which stay under the licenses granted by their own copyright holders.

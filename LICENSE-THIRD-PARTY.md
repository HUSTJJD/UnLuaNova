# Third-party Sources and Notices

Inventory checked 2026-10-24. This document does not relicense upstream work. Retain existing copyright/permission notices and ship the applicable full notices with distributions. **This inventory is not yet a complete release compliance bundle:** extension version/provenance and root/file license scope require reconciliation.

## Included Runtime Components

| Component | Location / source | License and attribution evidence |
|---|---|---|
| Tencent/UnLua | Inherited plugin source; [upstream](https://github.com/Tencent/UnLua) | MIT; Copyright (C) 2019 Tencent. Existing file headers must remain |
| Lua 5.5.1 | `Plugins/UnLuaNova/Source/ThirdParty/LuaLib/Lua` | MIT; Lua.org, PUC-Rio; exact notice in pinned `lua.h` |
| Luau | `Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau` | MIT; Copyright (c) 2019-2026 Roblox Corporation; Copyright (c) 1994-2019 Lua.org, PUC-Rio. Preserve `LICENSE.txt` and `lua_LICENSE.txt` |
| LuaSocket | `Plugins/UnLuaNovaExtensions/LuaSocket`; [upstream](https://github.com/lunarmodules/luasocket) | MIT; local `luasocket.h` identifies Copyright (C) 1999-2013 Diego Nehab; exact imported revision and full distribution notice still to record |
| lua-protobuf / protoc.lua | `Plugins/UnLuaNovaExtensions/LuaProtobuf`; [upstream](https://github.com/starwing/lua-protobuf) | Upstream MIT; local import revision and matching full copyright/permission text still to record |
| serpent 0.30 | `Plugins/UnLuaNovaExtensions/LuaProtobuf/Content/Script/serpent.lua` | Local header: (C) 2012-17 Paul Kulchenko; MIT |
| lua-rapidjson | `Plugins/UnLuaNovaExtensions/LuaRapidjson/Source/src`; [upstream](https://github.com/xpol/lua-rapidjson) | Upstream MIT; local import revision and matching full notice still to record |
| RapidJSON | `Plugins/UnLuaNovaExtensions/LuaRapidjson/Source/include/rapidjson` | MIT headers: Copyright (C) 2015 Tencent, and Milo Yip; included subcomponents may differ |
| msinttypes | RapidJSON `msinttypes/inttypes.h` and `stdint.h` | BSD-3-Clause text in those headers; Copyright (c) 2006-2013 Alexander Chemeris; retain Tencent modification notice |

Dependency pins for Lua/Luau are in [VERSIONS.md](Plugins/UnLuaNova/Source/ThirdParty/VERSIONS.md). Do not infer the version of a copied extension from the current upstream HEAD.

## Development Tools and Build Artifacts

- `Tools/luau-lsp` and `Tools/lua-language-server` are separate pinned tool submodules, not runtime libraries. Include their license files and transitive notices if distributing their binaries.
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

The repository root license contains PolyForm Small Business plus additional terms. Existing MIT code remains subject to its notices. The rights holder must reconcile file-level scope, contributor permissions and commercial explanations before release. This inventory records the issue; it does not change a grant.

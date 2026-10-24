# Pinned Dependencies

The superproject gitlinks are authoritative. Update this file with any dependency change and record local patches explicitly.

| Dependency | Version / revision | Role |
|---|---|---|
| Lua | 5.5.1; `7579fc9d7ed90240487251dfb69168f8e64e9294` | Runtime; version from lua.h |
| Luau | 0.743 + 1; `6d36d483cb88973276af611198169f76f3ce05b0` | Runtime/compiler/optional CodeGen; LBC_VERSION_MAX=14 |
| LuaLS | `a58fc7b01e1746c46275c71b0e61bfed40a6296d` | Development tool under Tools/lua-language-server |
| luau-lsp | `7a10f6d2dc6067934c66f9e3ad6cb4095594ac62` | Development tool under Tools/luau-lsp |

Lua/Luau are initialized in the audit checkout; the LSP tool submodules are not initialized. No local VM patches were recorded in this audit. Wrapper and compatibility changes live outside the submodules.

Read-only verification from the repository root:

```powershell
git submodule status
git -C Plugins/UnLuaNova/Source/ThirdParty/LuaLib/Lua rev-parse HEAD
git -C Plugins/UnLuaNova/Source/ThirdParty/LuauLib/Luau rev-parse HEAD
```

Upstream Luau 0.744 (`32c28089305f488f901aaf1dbf3fd1cb65fa3d11`) was inspected on 2026-10-24. It is not the integrated version. Compare implementation changes and run backend acceptance before updating a pin. Do not use a floating branch name as a release identity.

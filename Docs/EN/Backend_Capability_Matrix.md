---
title: Dual-backend Capability Matrix
description: Capability differences between Lua 5.5.1 and Luau, the explicit plain-Lua degradations, and the acceptance rules for adding a capability.
---

# Dual-backend Capability Matrix

UnLuaNova selects its backend at **compile time**; the two VMs never coexist at
runtime. Most advanced capabilities come from Luau, while plain Lua 5.5.1 takes the
degraded path behind disabled capability macros. This page records those differences
so a capability boundary is not mistaken for missing work.

## Single source of truth

The `UNLUANOVA_HAS_*` macros in
`Plugins/UnLuaNova/Source/UnLuaNova/Public/LuaBackend.h` are the only authoritative
capability declaration. There are twelve: `HOOK_EVENT`, `NATIVE_VECTOR`,
`NATIVE_BUFFER`, `READONLY_TABLES`, `SANDBOX`, `BREAKPOINT`, `ALLOC_CALLBACKS`,
`NATIVE_CODEGEN`, `THREAD_CALLBACK`, `GC_METAMETHOD`, `USERDATA_FINALIZER` and
`ITER_METAMETHOD`. Most are Luau=1 / Lua=0.

That header also carries three `#error` guards and a `static_assert` on the width of
`lua_Integer`, so a misconfigured backend fails at compile time instead of producing
a hard-to-debug runtime difference. **Never infer the backend in business code**;
always branch on a capability macro.

## Explicit plain-Lua degradations

The `// plain-Lua backend` branch of `Private/LuaBackend.cpp` contains six empty
implementations / fixed return values. They are deliberate capability boundaries,
not overlooked TODOs:

| Location | Form | Effect |
|---|---|---|
| `InstallSandboxThreadHardening` | empty | Sandbox thread hardening is Luau-only |
| `IsNativeCodeGenAvailable` | `return false` | Native codegen is Luau + x64/arm64 only |
| `MaybeCompileNativeChunk` | empty | Same as above |
| `Get*Count` × 2 | `return 0` | Codegen statistics are meaningful only on Luau |
| `IsClassTableFreezeEnabled` | `return false` | Read-only class tables are Luau-only |

The Luau side has complete implementations in the same file. Callers should branch on
the capability macro rather than checking whether a return value is 0.

## Related behavioural differences

| Capability | Lua 5.5.1 | Luau |
|---|---|---|
| Static type checker | `lua-language-server` (Lua 5.5 mode) | `luau-lsp analyze -Mode strict` |
| Coverage export (`lua.coverage`) | Unsupported; the command reports it explicitly | Supported; see [Luau Coverage](LuauCoverage.md) |
| Native codegen (`lua.codegen.enabled`) | N/A | Opt-in, default 0, applies only to chunks loaded after the switch |
| Sandbox (`lua.sandbox`) | Hardening beyond path checks unavailable | Full |

## Requirements for a new capability

1. Expose a `UNLUANOVA_HAS_*` macro in `LuaBackend.h` and define it for **both**
   backends, not just the Luau branch.
2. Add a case in `LuaBackend.spec.cpp`; verify behaviour on both backends.
3. If plain Lua can only degrade, state "this is a capability boundary" at the
   corresponding implementation in `LuaBackend.cpp` and record it in the table here.
4. Update `Docs/CN/Backend_Capability_Matrix.md` to keep both languages in sync.

## Further reading

[Luau Backend](Luau.md) · [Luau Coverage Export and Gate](LuauCoverage.md) · [Engine and Platform Support Matrix](Engine_Support.md)

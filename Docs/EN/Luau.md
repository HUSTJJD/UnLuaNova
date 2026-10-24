# Luau Backend

Integrated revision: `6d36d483cb88973276af611198169f76f3ce05b0` (0.743 + 1). Upstream 0.744 was inspected without upgrading. Backend selection is in `Config/DefaultUnLuaNova.ini` and requires rebuilding.

## Capability Boundaries

| Layer | Integration | Limit |
|---|---|---|
| Compiler / syntax | Pinned Luau syntax, annotations, generics, continue, compound assignment, interpolation and if expressions | Parsing annotations is not type checking; integrated conformance coverage remains incomplete |
| Standard libraries | base, coroutine, table, string, math, os/debug subsets, bit32, utf8, vector, buffer | Registration follows VM/src/linit.cpp; experimental integer/user-class features follow upstream flags |
| UE bridge | vector/FVector and buffer/TArray copies | float versus double precision; buffer does not alias external UE storage; result array is not guaranteed uint8 |
| Compiler settings | optimize/debuglevel/coverage | Coverage export and gates incomplete |
| CodeGen | Opt-in with module filter | Platform/executable-memory policy and debug interaction need validation; counters measure requests |
| Diagnostics | Debug primitives, profiler, allocation categories, coroutine census, GC controls | Churn is not live bytes; remote/packaged acceptance incomplete |
| Type checking | Generated declarations plus external luau-lsp | Analysis/Config are not runtime type checking; skip behavior must be checked |
| Modules | Host loader, package.searchers/preload | Not upstream Require path/alias/config semantics |

See upstream [compatibility](https://luau.org/compatibility/) and [sandbox](https://luau.org/sandbox/) documentation and the [local matrix](../feature-matrix.md).

## Lua Compatibility

Full Lua 5.x language equivalence is not promised: integer/operator syntax, goto and close semantics differ. Use explicit resource cleanup. The binding layer has dedicated int64 conversion, but networking still has an int32 truncation path.

Luau has no userdata __gc. `UNLUANOVA_HAS_GC_METAMETHOD=0` and `UNLUANOVA_HAS_USERDATA_FINALIZER=1`; container/FText/nontrivial exported constructors use VM destructors, while manual object reference proxies still need correction. Destructors must not touch the Lua stack. LONGJMP paths need a C++ cleanup audit.

Debug registry/uservalue functions include placeholders, upvalue identity is approximate, and load mode handling is incomplete. Shared closure aliases and reload rollback require tests. Host io is optional and buffered, not full Lua io; path validation is not protection against filesystem links. package.path/cpath do not define the host loader policy. Yield boundaries and iteration metamethods differ. Budgeted incremental GC is neither generational nor multithreaded GC.

## Configuration

Defaults: `luau.optimize=1`, `luau.debuglevel=1`, `luau.coverage=0`, `lua.codegen=0`, GC goal/step multiplier 200/200 and step size 1 KB. Sandbox, sandboxthread, class-table freezing and host io default off. An empty io root means the project directory.

Read-only globals do not restrict UE/debug/socket/io capabilities. Trusted and untrusted scripts need explicit host policies; the Lua backend's own io is not controlled by the Luau io switch.

Use `Tools/CheckLuauTypes.ps1 -Require` with existing tools and inspect skipped results. No LSP build or typecheck was run in this audit. Load examples through `require("Luau.Index")`; `Available` guards Luau-only files. VectorInterop and BufferInterop expose `:Run()`.

Profiler conversion uses the pinned submodule tool and its svg dependency:

```powershell
python Tools/LuauPerfgraph/perfgraph.py Saved/Profiling/profile.out > profile.svg
```

There is no --width option. New io/CodeGen/buffer regression source awaits maintainer build and execution. See [testing](Testing.md).

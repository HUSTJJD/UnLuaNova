# Features

| Area | Capabilities | Boundary |
|---|---|---|
| UE | Reflection, properties, overrides, containers, delegates, latent calls | Lifecycle, inheritance and multiple environments require explicit handling |
| Binding | Template exports, generated thunks, reflection fallback | Thunks use ProcessEvent; signature support is selective |
| Backends | Lua 5.5.1 and pinned Luau | Compile-time selection; semantics differ |
| Luau | Syntax, libraries, vector, buffer, debug primitives, optional CodeGen | External type checker; experimental libraries follow flags |
| Tools | DAP, profiling, memory/coroutine diagnostics, declarations | Packaged builds require separate validation |
| Runtime | Event, OOP, Timer, HotReload | Timer lifetime and shared upvalue semantics are known limitations |
| Network | RPC, typed replication buckets, per-condition replication groups, `UStruct`/container type specs | Experimental; eight conditions have their own storage group, unsupported conditions are rejected, and multi-Env schemas use joint ownership. Declarative networking is not a mature capability |

See [Luau](Luau.md) and [runtime modules](StdLib.md).

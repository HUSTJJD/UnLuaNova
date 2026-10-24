# Features

The [capability matrix](../feature-matrix.md) is the authoritative inventory. Implementation status is separate from execution evidence.

| Area | Source implementation | Boundary |
|---|---|---|
| UE | Reflection, properties, overrides, containers, delegates, latent calls | Lifecycle, inheritance and multiple environments need acceptance |
| Binding | Template exports, generated thunks, reflection fallback | Thunks use ProcessEvent; signature support is selective |
| Backends | Lua 5.5.1 and pinned Luau | Compile-time selection; semantics differ |
| Luau | Syntax, libraries, vector, buffer, debug primitives, optional CodeGen | External type checker; experimental libraries follow flags |
| Tools | DAP, profiling, memory/coroutine diagnostics, declarations | CI and packaged validation remain incomplete |
| Runtime | Event, OOP, Timer, HotReload | Timer lifetime and shared upvalue semantics remain open |
| Network | RPC and typed replication buckets | Experimental; condition and int64 defects block acceptance |

See [findings](Benchmark_Review.md), [Luau](Luau.md) and [runtime modules](StdLib.md).

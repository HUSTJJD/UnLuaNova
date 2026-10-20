# Test Status

Latest complete Release validation: **2026-11-03**, UE 5.8.3 source build / Win64 /
Visual Studio 2026 18.10. All 33 stages passed with 2482/2482 UE executions, 0 failed, 0 skipped.

| Backend | Carrier | Tests | Result |
|---|---|---:|---|
| Luau (default) | Editor | 641 | 641 passed / 0 failed / 0 skipped |
| Luau (default) | cooked `-game` | 615 | 615 passed / 0 failed / 0 skipped |
| Lua 5.5.1 | Editor | 626 | 626 passed / 0 failed / 0 skipped |
| Lua 5.5.1 | cooked `-game` | 600 | 600 passed / 0 failed / 0 skipped |

Coverage includes cooked runs, real dedicated-server plus two-client networking, each backend's
type check (0 errors), the 25% performance gate, and Development / Server / Shipping packaging.
Static analysis retained 298 Lua warnings and 7 Luau warnings — both are a known analyzer
limitation on `package.path`-style requires and do not affect loading.

[Lua status](../test-lua-report.md) | [Luau status](../test-luau-report.md) | [Testing](Testing.md) | [Audit](Benchmark_Review.md)

Reports must identify source revision and dirty state, dependencies, backend, UE/compiler/platform, feature flags, test scope and raw artifacts. Fast, NullRHI and filtered runs do not cover the entire product.

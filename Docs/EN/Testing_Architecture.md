---
title: Test Architecture and Acceptance
description: The layers of the UnLuaNova test framework, network semantics, the single release entry point, and checks that need no compilation.
---

# Test Architecture and Acceptance

The framework covers UE C++ specs, Lua-side specs, scene/viewport tests, FNetSession
cross-process tests, performance cases, report generation and static classification
tools.

## Layers

| Layer | Carrier | What it verifies |
|---|---|---|
| Repository checks | Python / PowerShell parser | Matrix, links, tool logic, script syntax |
| VM / binding | UE Spec + Lua Spec | Backends, marshalling, containers, errors, library contracts |
| Memory | Both backends, Editor / cooked Specs + Python gate | Per-block VM teardown, native payload / UObject / callback release, post-warmup growth, retained-reference control |
| World | Game or editor | Timers, object destruction, latent actions, multiple environments |
| Network | PIE + genuinely connected processes | RPCs, properties, owner, multiple clients |
| UI / samples | Editor + real RHI | UMG, input, screenshots, sample interactions |
| Performance / release | Fixed machine + packaged targets | Distributions, GC, memory, devices, licences |

`ClassifyTests.py`'s A-E tiers are a static heuristic classification, not dynamic
coverage. Several Lua-side assertions are aggregated into one UE case, so the number
of files, the number of `It` blocks and the number of registered UE cases must not be
mixed into one denominator.

## Network semantics

Matching RPCIds across the two independent processes in
`LuaNetDedicatedServerTest.cpp` demonstrates determinism only; it does **not** mean
the processes are networked. `LuaNetMulticastTest.cpp` organises a session with one
server and two clients. A client that fails to join, a missing peer, a timeout or a
crashed test process all fail the session.

## Acceptance entry point

Maintainers run only `.\Tools\RunTests.ps1` (no parameters). It performs the complete
Release: both backend builds, generated-code rebuild, full Editor and cooked
Development cases, real network peers, type checks, the performance gate, a Server
build and Shipping packaging. For the actual order, failure rules and coverage
boundaries, see [Testing and Automation](Testing.md).

Local filters, Fast, NullRHI and SkipBuild belong to the separate
`Tools/RunDevelopmentTests.ps1` and are never a release verdict.

## Checks that need no compilation (integrated)

The `repository-audit` and `tooling-tests` stages of `RunTests.ps1` run
`Tools/AuditRepository.py`, `python -B -m unittest discover -s Tools/tests -p "test_*.py"`
and `TestReleaseSupport.ps1` for you - **no manual run is needed**. The `docs-site`
stage also performs `mkdocs build --strict`. None of these starts UE, UBT, CMake or an
LSP build; passing them only means the repository checks passed, and the verdict still
comes from the whole run in `release-summary.json`.

## Further reading

[Testing and Automation](Testing.md) · [Luau Coverage Export and Gate](LuauCoverage.md) · [Engine and Platform Support Matrix](Engine_Support.md)

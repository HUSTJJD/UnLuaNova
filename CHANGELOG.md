# Current Development State

This file summarizes the current development tree. Historical changes are available in Git; current behavior and outstanding work are maintained in the [audit](Docs/CN/Benchmark_Review.md) and [capability matrix](Docs/feature-matrix.md).

## Unreleased

- Dual Lua 5.5.1 / pinned Luau backends; reflection, template exports and generated thunks.
- Debugging, profiling, type declarations, runtime libraries and experimental networking.
- Source audit covers slua-unreal, FUnLua and Luau, with explicit gaps and acceptance criteria.
- CodeGen initialization follows VM lifetime; io handles CRLF and validates mutable paths/read inputs.
- RPC registration is sorted; network schema fingerprints include replication conditions.
- Event.Once detaches before callback reentry; explicit unsubscription releases token owners.
- Regression source, executable examples and repository checks added; obsolete plans and old test conclusions removed.

C++ and runtime changes are unbuilt and unexecuted in this audit. Network conditions, timer lifetime, manual userdata references and license scope remain release blockers. See [current release status](Docs/CN/ReleaseNotes.md).

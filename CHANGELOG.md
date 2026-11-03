# Changelog

<!-- release-status: not-passed -->
<!-- release-provenance: 347bb8c5 -->

Entries follow the acceptance runs recorded under `Saved/ReleaseTests/` and the
[capability matrix](Docs/feature-matrix.md). Historical detail lives in Git; current scope and
outstanding work are maintained in the [audit](Docs/CN/Benchmark_Review.md).

## Unreleased — source changes, not yet validated

- Declarative networking: replicated fields now support eight lifetime conditions through one
  storage group per condition, because UE decides what to send from the property's own
  registration, which is cached per class. `COND_Custom` / `COND_Dynamic` / `COND_Never` /
  `COND_NetGroup` and the Replay* family are rejected at declaration time instead of being
  silently downgraded to `COND_None`. The obsolete component-wide `Condition` now only warns.
- A subclass with no declaration of its own inherits the parent class's schema, and
  `FUnLuaNovaNet::Cleanup(Env)` releases a schema by joint ownership (see below) instead of
  attributing it to whichever environment happened to shut down first. Orphan entries with no
  owner remain process-wide state that only `Cleanup(nullptr)` drops.
- Unit cases added for condition groups, reliable/unreliable RPC flags, scalar and Object
  bucket round-trips, schema-mismatch refusal and parent-class schema lookup.
- Evidence provenance: `RecordEvidence.py` now takes `--commit` / `--source` so importing a
  historical report cannot be misattributed to today's HEAD. The four real passing 2026-11-03
  baseline automation reports (Lua/Luau x Editor/Game) were recorded with `commit 347bb8c5` and
  flagged `historical`; `LuaNetDedicatedServerTest.cpp` is now registered in the matrix so DS
  cases map to the Networking capabilities instead of being dropped as unmapped.
- Connected-acceptance **scenario source** added: a non-owner Server RPC attempt with a unique
  token (proved never executed by exact `CallLog` comparison plus a `ValidateRelease.py` gate),
  and an `OwnerOnly` field mixed with an unconditional field with per-peer visibility assertions.
- Arbitrary `UStruct` / container types: corrected the earlier "architectural boundary" call.
  A `UClass` cannot gain a `UPROPERTY` at runtime, but neither delivery path needs one: RPC
  parameters attach real `FStructProperty` / `FArrayProperty` / `FSetProperty` / `FMapProperty`
  to the runtime-built `UFunction`, and replicated fields use a shared `Generic` bucket with a
  canonical self-describing payload. Specs (`Struct:<path>`, `Array:<spec>`, `Set:<spec>`,
  `Map:<key>:<value>`) are validated at bind time and folded into `SchemaVersion`. `UObject`
  references keep the dedicated Object bucket. Unit cases cover parsing, nesting, rejection and
  Lua <-> payload round-trips.
- Eight opt-in connected scenarios in `LuaNetNetScenariosTest.cpp`: late join, process-level
  reconnect (`FNetSession::RestartPeer`), dormancy, relevancy, join-order swap, packet loss /
  reordering (real `NetEmulation.PktLoss/PktOrder/PktLag`), `UNetConnection`-level reconnect
  (`Connection->Close()` plus an in-process reopen through `-UnLuaNovaNetReconnectUrl`), and
  cross-peer declaration fingerprints. `FNetSession` forwards the scenario flag, packet-loss
  percentage and exact reconnect URL to every peer, and records `launch_serial`, `first_pid`,
  `launch_count`, scenario and delivery variant in `topology.json`.
- Legacy / PushOff / Iris / Cooked delivery matrix: `Tools/RunNetMatrix.ps1` (also
  `RunTests.ps1 -Profile Development -NetMatrix`) and `RunTests.ps1 -ExtraArgs` pass real engine
  switches to every peer; the Cooked cell points the peer executables at staged
  `Windows` / `WindowsServer` binaries. `AppendWorldFacts` reads back Iris / push-model cvar
  state, compile-time `WITH_EDITOR`, and real `UNetConnection` byte / lost-packet counters.
  `BuildNetDeclarationReport()` exports each RPC parameter's spec from the **actual FProperty**,
  each field's `TypeSpec`, `RepSchemaVersion` and a deliberately different
  `VariantRepSchemaVersion`, so `ValidateRelease.py --net-matrix` can reject a cross-peer
  declaration mismatch instead of decoding into the wrong slot.
- `NetActor.lua` now declares a real `Inventory` (`Array:Int`) field and a `SendPayload` RPC with
  a `Struct:/Script/CoreUObject.Vector` parameter, giving the connected gate something to prove
  about generic types without changing the strict `CallLog` comparison.
- `FUnLuaNovaNet` schemas now use a **joint owner set**: identical declarations bound by several
  environments are released only when the last closes, in any order. Closing the first owner
  previously wiped a schema another Env still used. Two reverse-order unit cases and a weak
  `UObject` invalidation case were added.
- 58 Python tooling tests passed in the previous change. Five more gate cases (cross-peer schema,
  generic field / UStruct parameter, join-order swap + packet loss, connection-level reconnect,
  cooked variant) were added here and have **not** been run.
- These are **source and unit-case changes only**. None of the connected assertions, eight
  scenarios or four variants has been **executed** - they require a rebuild and real DS runs.
  There is no remaining source-level networking TODO; engine-level seamless travel
  (`ServerTravel`), hot reload and authorization checks remain explicitly out of scope rather
  than silently claimed. `release-status` stays `not-passed` and the three Networking capability
  groups stay `experimental`; see the [acceptance checklist](Docs/CN/LuaNetworking.md).

## Current release status: not passed

Current status follows `Saved/ReleaseTests/test/release-summary.json`. Later validations did not
pass for three diagnosed reasons: a Luau Editor performance regression that was reproduced as
measurement-environment interference (a later re-run passed 641/641 with the gate green, with the
committed baseline untouched); `ConflictingInstance` UBT mutex clashes and externally interrupted
compiler processes that failed packaging/build stages; and uncommitted Net source changes in the
current worktree that fail two `UnLuaNova.API.Net.*` cases. The baseline is not replaced and the
25% gate is not relaxed. Release stays blocked until the worktree is frozen, the generated matrix
is refreshed and a full Release run completes 33/33 with four green release gates.

## 2026-11-03 — baseline run, validated

The numbers below belong only to `.\Tools\RunTests.ps1 -Baseline` (`Saved/ReleaseTests/baseline`,
2026-11-03 03:32:11–04:20:26 UTC, commit `347bb8c5`, 8 uncommitted worktree changes recorded in
that run's provenance). They do not describe later commits or the current tree.

Complete Release validation on UE 5.8.3 / Win64 (Visual Studio 2026 18.10): 33/33 stages passed,
2482/2482 UE executions with zero failures and zero skips — Lua Editor 626, Lua cooked 600,
Luau Editor 641, Luau cooked 615.

- Both backends built from source: reflection, template exports and generated marshal thunks.
- Real dedicated-server plus two-client RPC and replication evidence on both backends
  (DedicatedServer and multicast sessions), with their runtime manifests and memory reports.
- Static analysis on both backends: 0 errors (298 Lua warnings and 7 Luau warnings retained,
  both a known analyzer limitation on `package.path`-style requires).
- Development, Server and Shipping packaging for both backends.
- 41 Python tooling tests and the PowerShell support checks passed.
- Source audit covers slua-unreal, FUnLua and Luau, with explicit gaps and acceptance criteria.
- Performance in this run: every benchmark case stayed inside the 25% gate against the committed
  reference; Luau leads Lua 5.5.1 on the container case (self-comparison, not a third-party comparison).

Boundaries that run did not close, recorded rather than waived: at that time conditional
replication stayed experimental and rejected non-`None` conditions explicitly (the per-condition
layout has since been delivered — see Unreleased above); third-party notices are still being
completed; non-Win64 platforms, Iris/push model and a same-machine comparison against
slua-unreal / FUnLua need their own acceptance runs.

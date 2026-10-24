# UnLuaNova

Unreal Engine scripting with Lua / Luau, forked from [Tencent/UnLua](https://github.com/Tencent/UnLua).

[中文](README.md) | [Documentation](Docs/EN/index.md) | [Current audit](Docs/EN/Benchmark_Review.md)

## Goal and State

The goal combines UnLua interoperability, slua-unreal product capabilities, FUnLua ownership and binding ideas, and Luau language/tooling support. **Complete parity, full Lua compatibility and superior performance have not been established.**

The source includes reflection and overrides, static exports and generated thunks, containers and delegates, Lua 5.5.1 / Luau backends, debugging, profiling, type declarations, runtime libraries and experimental networking. Generated thunks still invoke `ProcessEvent`. Native Luau compilation is opt-in.

The 2026-10-24 audit identifies release blockers in replication conditions, timer lifetime and Luau manual object references. Current C++ changes have not been built or executed. Old reports do not certify this working tree.

- [Audit and acceptance order](Docs/EN/Benchmark_Review.md)
- [Source, test and example matrix](Docs/feature-matrix.md)
- [slua-unreal and FUnLua comparison](Docs/EN/Comparison_sluaunreal.md)
- [Luau capabilities and compatibility](Docs/EN/Luau.md)
- [Performance acceptance](Docs/EN/Performance_Parity_slua.md)

## Integration

Copy the required plugins from `Plugins/` into your UE project, initialize submodules with `git submodule update --init --recursive`, select the backend in `Config/DefaultUnLuaNova.ini`, and build the matching UE target. Bind a Blueprint module through the toolbar and place scripts in `Content/Script/`.

Backend changes require rebuilding. UE 5.8.3 is the local baseline; other versions and platforms require explicit acceptance. Exact dependencies are recorded in [VERSIONS.md](Plugins/UnLuaNova/Source/ThirdParty/VERSIONS.md).

See the [quickstart](Docs/EN/Quickstart_For_UE_Newbie.md), [programming guide](Docs/EN/UnLuaNova_Programming_Guide.md) and [examples](Docs/EN/Showcase.md).

## Validation and Licensing

`python Tools/AuditRepository.py` checks the repository without compiling. Maintainers run only `.\Tools\RunTests.ps1`: it orchestrates Win64 dual-backend builds, Editor/cooked automation, networking, type checks and packaging, failing on missing evidence. Summaries go to `Saved/ReleaseTests/`; see [testing](Docs/EN/Testing.md). The full pipeline has not been executed during this change.

Preserve upstream UnLua MIT notices. The root license contains PolyForm Small Business terms plus additional terms; file notices and the scope of the root license require reconciliation by the rights holder. See [third-party notices](LICENSE-THIRD-PARTY.md) and [license status](Docs/EN/License.md). Comparison does not imply endorsement or permission to copy source.

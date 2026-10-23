<div align="center">

<img src="Docs/Images/UnLuaNova.png" alt="UnLuaNova" width="160" />

# UnLuaNova

**A Lua scripting solution for Unreal Engine — the community continuation of [Tencent/UnLua](https://github.com/Tencent/UnLua)**

[![CI](https://github.com/HUSTJJD/UnLuaNova/actions/workflows/ci.yml/badge.svg)](https://github.com/HUSTJJD/UnLuaNova/actions/workflows/ci.yml)
[![Docs](https://github.com/HUSTJJD/UnLuaNova/actions/workflows/docs.yml/badge.svg)](https://hustjjd.github.io/UnLuaNova/)
[![License: Polyform Small Business](https://img.shields.io/badge/License-Polyform%20Small%20Business-0175C2.svg)](LICENSE)
[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.8-blueviolet.svg)](https://www.unrealengine.com/)
[![Lua](https://img.shields.io/badge/Lua-5.5%20%2F%20Luau-blue.svg)](https://www.lua.org/)
[![GitHub stars](https://img.shields.io/github/stars/HUSTJJD/UnLuaNova?style=social)](https://github.com/HUSTJJD/UnLuaNova/stargazers)

[Documentation](https://hustjjd.github.io/UnLuaNova/) · [Quick Start](#quick-start) · [Docs Index](#docs-index) · [中文](README.md)

</div>

---

## What is this

The official UnLua release line froze at **v2.3.6 (2023-11)** — its `develop` branch still gets
occasional fixes, but there is no release and no maintainer. UnLuaNova pulls those scattered
community contributions into **one maintained release line**, and adds three things on top:

| | What | Why it matters |
|---|---|---|
| **Static binding** | Binding tables are generated at build time for declared types | Skips the reflection hot path and runtime lookups |
| **Two Lua backends** | [Lua 5.5](https://www.lua.org/) and [Luau](https://luau.org) (default), both as git submodules | Backend differences live behind a single abstraction layer |
| **Lua observability** | CPU profiler (into Unreal Insights), headless debug service, sandbox, infinite-loop watchdog | Lua stops being a black box: performance and failures are traceable |

In short: **keep UnLua's programming feel, add the maintenance, performance and debugging it never had.**

## Why UnLuaNova

| | Tencent/UnLua v2.3.6 | UnLuaNova |
|:---|:---|:---|
| Maintained | No (since 2023-11) | Yes |
| Engine | UE 4.17 – 5.x (newer versions need porting) | Baseline **UE 5.8.3**, community UE 5.4 / 5.6 fixes merged |
| Lua backend | One single Lua version | **Lua 5.5 / Luau**, switchable |
| Binding | Runtime reflection | Build-time **static binding** (opt-out type selection) |
| Performance | — | `CallUEFast` skips redundant lookups, persistent parameter buffers |
| Observability | Minimal | CPU profiler / memory stats / debug service / sandbox / watchdog |
| Type checking | LuaLS + generated EmmyLua annotations | Generated Luau `.d.lua`, checked by `luau-analyze` |

For **why each of these is built the way it is, what it buys and what it costs**:
[Why UnLuaNova](Docs/EN/WhyUnLuaNova.md) (Chinese: [相对 UnLua：原理与优势](Docs/CN/WhyUnLuaNova.md)).

## Quick start

```bash
# 1. Copy the Plugins directory into your UE project root
# 2. Initialize submodules (Lua 5.5 and Luau are submodules - without them the VM sources are missing)
git submodule update --init --recursive
```

3. Restart the project
4. Create a Blueprint → `Bind` in the UnLuaNova toolbar → fill in the Lua module path (e.g. `GameModes.BP_MyGameMode`)
5. Pick `Create Lua Template File` and write code in `Content/Script/GameModes/BP_MyGameMode.lua`

## What the code looks like

Access `UCLASS` / `UPROPERTY` / `UFUNCTION` / `USTRUCT` / `UENUM` directly with no glue code,
and replace Blueprint implementations outright:

```lua
local BP_PlayerController_C = UnLuaNova.Class()

function BP_PlayerController_C:ReceiveBeginPlay()
    -- call engine APIs directly
    local Widget = UE.UWidgetBlueprintLibrary.Create(
        self, UE.UClass.Load("/Game/Core/UI/UMG_Main_C"))
    Widget:AddToViewport()

    -- keep the original Blueprint implementation by calling it explicitly
    self.Overridden.ReceiveBeginPlay(self)
end

-- override a Blueprint event with return values
function BP_PlayerController_C:GetCharacterInfo(HP, Position, Name)
    return true, 99, UE.FVector(128.0, 128.0, 0.0), "Marcus"
end

return BP_PlayerController_C
```

The same pattern covers **Replication Notify**, **Animation Notify** and **Input Event**:
match the Lua function name (`AnimNotify_xxx`) and the replacement is done — no helper code.

## Status

| Item | Value |
|---|---|
| Engine baseline | **UE 5.8.3** (source build) |
| Lua backend | **Luau (default)** / Lua 5.5 — both as submodules |
| Upstream baseline | Tencent/UnLua v2.3.6 + `develop` (2026-04) |
| License | Polyform Small Business 1.0.0 / paid above threshold |

> ⚠️ Early stage, expect churn. Great for trying out, opening issues and pushing fixes — not yet recommended for production.
> Known issues: see [CHANGELOG](CHANGELOG.md#known-issues).

## Docs index

<div align="center">

| Chinese | English |
|:---|:---|
| [UE 新手快速上手](Docs/CN/Quickstart_For_UE_Newbie.md) | [Programming Guide](Docs/EN/UnLuaNova_Programming_Guide.md) |
| [功能清单](Docs/CN/Features.md) | [API Reference](Docs/EN/API.md) |
| [相对 UnLua：原理与优势](Docs/CN/WhyUnLuaNova.md) | [Why UnLuaNova](Docs/EN/WhyUnLuaNova.md) |
| [Luau 后端](Docs/CN/Luau.md) | [Luau Backend](Docs/EN/Luau.md) |
| [编程指南](Docs/CN/UnLuaNova_Programming_Guide.md) | [How To Implement Overriding](Docs/EN/How_To_Implement_Overriding.md) |
| [API 参考](Docs/CN/API.md) | [FAQ](Docs/EN/FAQ.md) |
| [设置项](Docs/CN/Settings.md) | [Docs site](https://hustjjd.github.io/UnLuaNova/) |
| [调试](Docs/CN/Debugging.md) · [IntelliSense](Docs/CN/IntelliSense.md) | |
| [静态导出绑定](Docs/CN/StaticExportBinding.md) · [控制台命令](Docs/CN/ConsoleCommand.md) | |
| [测试与自动化](Docs/CN/Testing.md) · [FAQ](Docs/CN/FAQ.md) | |

</div>

Upstream changelog for 2019–2023: [`Docs/Archive/UnLua_CHANGELOG.md`](Docs/Archive/UnLua_CHANGELOG.md).

## Migrating from UnLua

The Lua global was renamed from `UnLua` to **`UnLuaNova`** — a bulk rename is enough:

```lua
local M = UnLua.Class()       -- UnLua
local M = UnLuaNova.Class()   -- UnLuaNova
```

UCLASS names changed too (`UUnLuaSettings` → `UUnLuaNovaSettings`, etc.), so assets need a
resave and `Config/*.ini` section names need migrating.

## Contributing

A star is the cheapest way to help ⭐ — also welcome:

- Issues: build problems, engine version porting, API questions
- PRs: docs fixes, new tests, merging unmerged community fixes from upstream
- Tests: `.\Tools\RunTests.ps1` (see [Testing and Automation](Docs/EN/Testing.md))

## License

UnLuaNova is released under the **[Polyform Small Business License 1.0.0](https://polyformproject.org/licenses/small-business/1.0.0)**,
with additional terms that tighten the size thresholds (see [LICENSE](LICENSE)):

| Who you are | May you use it? |
|:---|:---|
| An individual: learning, hobby projects, open source | Yes, free |
| An organization: **< 10** employees + contractors **and** **< $1M** revenue/funding in the prior tax year | Yes, free, including commercial use |
| Anyone above either threshold | Commercial license required, see [COMMERCIAL.md](COMMERCIAL.md) |

Size is counted for the **whole group** — parents, subsidiaries and entities under common
control count as one, so a small studio owned by a large publisher cannot count only itself.

In short: the source is public, small teams may use it commercially for free, and those who
make money from it pay.

Contributions require signing the [CLA](CLA.md), which grants the maintainers the right to
sublicense your contribution to commercial licensees. Without it a commercial license cannot
be granted cleanly, and there is no revenue to keep the project maintained.
See [CONTRIBUTING.md](CONTRIBUTING.md).

The upstream components remain MIT-licensed and are unaffected by this change:
[Tencent/UnLua](https://github.com/Tencent/UnLua), [Lua 5.5](https://www.lua.org/) and
[Luau](https://github.com/luau-lang/luau) — see [LICENSE-THIRD-PARTY.md](LICENSE-THIRD-PARTY.md).

Lua VM: [Lua 5.5](https://www.lua.org/) (MIT); Luau: [luau-lang/luau](https://github.com/luau-lang/luau) (MIT).
Both are git submodules — run `git submodule update --init --recursive` after cloning.

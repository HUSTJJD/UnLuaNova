---
title: License and Commercial Use
description: UnLuaNova is licensed under Polyform Small Business 1.0.0 with tighter size limits - small teams may use it commercially for free, larger ones need a commercial license
---

# License and Commercial Use

The source is public. It is **not** unrestrictedly free for commercial use.

UnLuaNova is released under the
[Polyform Small Business License 1.0.0](https://polyformproject.org/licenses/small-business/1.0.0)
with additional terms that tighten the size thresholds. Full text:
[`LICENSE`](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE).

## May I use it?

Size is counted for the **whole group**: your organization together with its parents,
subsidiaries and every entity under common control counts as one. Both conditions must hold
for free use:

| Condition | Free range |
|---|---|
| Employees + independent contractors | **< 10** |
| Total revenue and funding, prior tax year | **< 1,000,000 USD** |

| Who you are | Verdict |
|:---|:---|
| An individual: learning, hobby, open source | Free |
| A solo developer | Free, including commercial use |
| A small studio within both thresholds | Free, including commercial use |
| More than 10 people, or over USD 1M revenue/funding | [Commercial license](https://github.com/HUSTJJD/UnLuaNova/blob/master/COMMERCIAL.md) required |
| A studio owned by a large publisher | Required (counted at group level) |
| A contractor integrating it for a large client | Required (counted for the beneficiary) |
| Redistributing it inside a product or middleware you sell | Required |

!!! tip "When in doubt, ask"
    Open an issue tagged `commercial licensing` describing your case. Under the VIOLATIONS
    clause you have 32 days to come into compliance after written notice, after which your
    licenses end immediately.

## Commercial license

See [`COMMERCIAL.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/COMMERCIAL.md) for
scope, pricing tiers, contact details, and what happens without one.

Planned commercial-only capabilities: a DAP debug adapter, deeper Profiler / Unreal Insights
integration, a command-line toolchain for the static binding generator, plus guaranteed
support for specific engine versions and priority support.

## Upstream components remain MIT

This change does **not** affect them — each is licensed by its own copyright holder and stays
usable under MIT on its own terms:

* [Tencent/UnLua](https://github.com/Tencent/UnLua) (MIT)
* [Lua 5.5](https://www.lua.org/) — Lua.org, PUC-Rio (MIT)
* [Luau](https://github.com/luau-lang/luau) (MIT)

Full list and license text:
[`LICENSE-THIRD-PARTY.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/LICENSE-THIRD-PARTY.md).

## Contributing requires a CLA

Sign [`CLA.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/CLA.md) before opening a PR.
It grants the maintainers the right to **sublicense** your contribution to commercial
licensees — without it a commercial license cannot be granted cleanly, and there is no
revenue to keep the project maintained. See
[`CONTRIBUTING.md`](https://github.com/HUSTJJD/UnLuaNova/blob/master/CONTRIBUTING.md).

!!! note "Not legal advice"
    This page explains the license; it is not legal advice. For compliance reviews, rely on
    the `LICENSE` text itself and your own counsel.

---

## Further reading

- [Programming Guide](UnLuaNova_Programming_Guide.md) — binding and two-way calls
- [Luau Backend](Luau.md) — backend capabilities and limits
- [API Reference](API.md) — C++ and Lua API

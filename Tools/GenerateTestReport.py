#!/usr/bin/env python3
"""Turn UE automation JSON into something a human will actually read.

UE's automation framework exports one JSON blob per test, and the exact shape has drifted
between engine versions (keys differ in case, `errors` is sometimes a count and sometimes
a list, `duration` has several names). This reads all of it defensively so a report still
comes out on an engine upgrade, and the numbers that matter - pass/fail, why, how slow -
end up on one page.

Outputs, next to --reports unless --out says otherwise:
    summary.json   machine-readable totals, so RunTests.ps1 can set its exit code
    report.md      the conclusions: totals, the passed tests, failures with their
                   messages, slowest tests, and a gallery of whatever the tests
                   screenshotted

--backend Lua|Luau records which Lua backend produced the run (a compile-time choice, see
UnLuaNova.Build.cs). With --publish it also picks the destination, so the two backend runs
do not overwrite each other:
    Docs/test-lua-report.md     --backend Lua
    Docs/test-luau-report.md    --backend Luau
    Docs/test-report.md         no --backend (single, unlabelled run)

Either way that copy is what gets committed and rendered by MkDocs, and it deliberately
points at no local path: the run directory is timestamped and Saved/ is gitignored, so
naming it here would make the file change on every single run.

Usage:
    python GenerateTestReport.py --reports <dir> [--out <dir>] [--backend Lua|Luau]
                                 [--publish]
"""

import argparse
import json
import re
import shutil
import sys
from datetime import datetime
from pathlib import Path

# UE reports success/failure as one of these strings, capitalisation included.
PASS_STATES = {"success", "passed", "pass"}
FAIL_STATES = {"fail", "failed", "failure", "error"}
SKIP_STATES = {"skipped", "skip", "notrun", "notrun", "ignored", "na"}

MAX_MESSAGE_LEN = 400
SLOWEST_COUNT = 10


def _get(obj, *names, default=None):
    """Case-insensitive lookup across a dict - UE is not consistent about casing."""
    if not isinstance(obj, dict):
        return default
    lowered = {str(k).lower(): v for k, v in obj.items()}
    for name in names:
        if name.lower() in lowered:
            return lowered[name.lower()]
    return default


def _get_path(obj, *paths, default=None):
    for path in paths:
        cur = obj
        for key in path:
            cur = _get(cur, key)
            if cur is None:
                break
        if cur is not None:
            return cur
    return default


def _message_list(value):
    """Normalise a UE 'errors'/'warnings' field into a list of strings."""
    if value is None:
        return []
    if isinstance(value, (int, float)):  # older exports just store a count
        return [] if value == 0 else [f"{int(value)} reported"]
    if isinstance(value, str):
        return [value] if value.strip() else []
    if isinstance(value, list):
        out = []
        for item in value:
            if isinstance(item, str):
                out.append(item)
            elif isinstance(item, dict):
                text = _get(item, "message", "text", "error", default="")
                if text:
                    out.append(str(text))
        return out
    return []


def _duration(entry):
    for key in ("duration", "durationseconds", "durationSeconds", "time", "elapsed"):
        value = _get(entry, key)
        if isinstance(value, (int, float)):
            return float(value)
    return None


def _state(entry):
    raw = _get(entry, "state", "result", "status", "teststatus", default="")
    return str(raw).strip().lower()


def _name(entry, fallback):
    return str(
        _get_path(entry, ("testDisplayName",), ("fullTestPath",), ("name",), ("testName",))
        or fallback
    )


def _entry_messages(entry, kind):
    """Pull error/warning text out of the per-event 'entries' list."""
    entries = _get(entry, "entries", "events", default=[])
    if not isinstance(entries, list):
        return []
    out = []
    for item in entries:
        if not isinstance(item, dict):
            continue
        event = _get(item, "event", "Event", default=item)
        etype = str(_get(event, "type", "Type", "verbosity", default="")).lower()
        want_error = kind == "error" and ("error" in etype or "fail" in etype)
        want_warn = kind == "warning" and "warn" in etype
        if not (want_error or want_warn):
            continue
        msg = _get(event, "message", "Message", "text", default="")
        if not msg:
            continue
        where = _get(item, "filename", "file", default="")
        line = _get(item, "lineNumber", "line", default="")
        if where:
            msg = f"{msg}  [{where}:{line}]" if line else f"{msg}  [{where}]"
        out.append(str(msg))
    return out


# Assertion failures carry the absolute path of the file that raised them, which means the
# checkout directory and the engine directory. That is fine in Saved/ and wrong in a file
# that is committed and published, so the published copy gets them rewritten to something
# that still identifies the file without saying where this machine keeps it.
_ENGINE_SOURCE_RE = re.compile(r"[A-Za-z]:\\[^\s\[\]\"']*?\\Engine\\Source\\")


def strip_machine_paths(text: str, repo_root: Path) -> str:
    text = _ENGINE_SOURCE_RE.sub(r"<UE>\\Engine\\Source\\", text)

    root = str(repo_root)
    for candidate in (root, root.replace("\\", "/")):
        for sep in ("\\", "/"):
            text = text.replace(candidate + sep, "")
    return text


def load_tests(reports_dir: Path):
    tests = []
    for path in sorted(reports_dir.glob("*.json")):
        if path.name == "summary.json":
            continue
        try:
            # utf-8-sig, not utf-8: the automation framework writes its index.json with a
            # UTF-8 BOM, and plain utf-8 raises on it - which used to make this skip the
            # report entirely and report "0 tests" for a run that had actually finished.
            data = json.loads(path.read_text(encoding="utf-8-sig", errors="replace"))
        except (json.JSONDecodeError, OSError) as exc:
            print(f"WARNING: could not read {path.name}: {exc}", file=sys.stderr)
            continue

        blobs = data if isinstance(data, list) else _get(data, "tests", "results", default=data)
        if isinstance(blobs, dict):
            blobs = [blobs]
        if not isinstance(blobs, list):
            continue

        for blob in blobs:
            if not isinstance(blob, dict):
                continue
            state = _state(blob)
            errors = _message_list(_get(blob, "errors", "errorCount", default=None))
            warnings = _message_list(_get(blob, "warnings", "warningCount", default=None))
            errors += _entry_messages(blob, "error")
            warnings += _entry_messages(blob, "warning")

            if state in PASS_STATES:
                passed = True
                skipped = False
            elif state in SKIP_STATES:
                passed = True
                skipped = True
            elif state in FAIL_STATES:
                passed = False
                skipped = False
            else:
                # No usable state field: fall back to whether anything was recorded as an
                # error. Everything unknown counts as skipped rather than silently passing.
                passed = not errors
                skipped = not errors

            if passed and errors:
                # A "success" with errors attached is a failure wearing a disguise.
                passed = False

            tests.append(
                {
                    "name": _name(blob, path.stem),
                    "state": state or ("success" if passed else "fail"),
                    "passed": passed,
                    "skipped": skipped,
                    "errors": errors,
                    "warnings": warnings,
                    "duration": _duration(blob),
                    "source": path.name,
                }
            )
    return tests


def render(tests, screenshots, image_prefix, generated, reports_dir, published=False, backend=None):
    total = len(tests)
    failed = [t for t in tests if not t["passed"]]
    skipped = [t for t in tests if t["skipped"]]
    passed = [t for t in tests if t["passed"] and not t["skipped"]]
    timed = [t for t in tests if t["duration"] is not None]
    total_time = sum(t["duration"] for t in timed)

    # Both backends are named in every heading: the two published reports are separate
    # pages, and a page that just says "测试报告" cannot be told apart from the other one
    # once it is open.
    suffix = f"（{backend} 后端）" if backend else ""

    lines = []
    if published:
        # MkDocs takes the page title and description from front matter. Writing the report
        # over this file without it dropped both, so the page fell back to its file name in
        # the nav and lost its description - the report looked like it had gone missing.
        lines.append("---")
        lines.append(f'title: "测试报告{suffix}"')
        lines.append('description: "自动化测试产出的通过率、通过明细、失败明细与截图"')
        lines.append("---")
        lines.append("")
    lines.append(f"# 测试报告{suffix}")
    lines.append("")
    lines.append(f"> 由 `Tools/GenerateTestReport.py` 自动生成于 {generated}。请勿手工编辑。")
    lines.append("")
    lines.append("## 结论")
    lines.append("")
    if total == 0:
        lines.append("**没有找到任何自动化测试结果。**")
        lines.append("")
        lines.append(
            "这通常是测试进程没跑起来（引擎启动失败、过滤器没匹配到任何用例），"
            "而不是\"全部通过\"。先看同目录下的 `Editor.log`。"
        )
    else:
        rate = len(passed) / total * 100
        verdict = "全部通过 ✅" if not failed else f"存在失败 ❌（{len(failed)} 项）"
        lines.append(f"**{verdict}** — 通过率 {rate:.1f}%")
        lines.append("")
        lines.append("| 指标 | 数值 |")
        lines.append("|---|---|")
        lines.append(f"| 用例总数 | {total} |")
        lines.append(f"| 通过 | {len(passed)} |")
        lines.append(f"| 失败 | {len(failed)} |")
        lines.append(f"| 跳过 | {len(skipped)} |")
        if timed:
            lines.append(f"| 总耗时 | {total_time:.2f}s |")
        lines.append(f"| 截图 | {len(screenshots)} |")
        if backend:
            lines.append(f"| Lua 后端 | `{backend}` |")

    # Passing tests are listed too, not just the failures: "what still works" is the part
    # that has to be checked after a change that touched shared code, and a bare pass rate
    # cannot answer it.
    if passed:
        lines.append("")
        lines.append(f"## 通过明细（{len(passed)} 项）")
        lines.append("")
        lines.append("| 用例 | 来源 | 耗时 |")
        lines.append("|---|---|---|")
        for t in sorted(passed, key=lambda t: (t["source"], t["name"])):
            duration = f"{t['duration']:.3f}s" if t["duration"] is not None else "-"
            lines.append(f"| `{t['name']}` | `{t['source']}` | {duration} |")

    if skipped:
        lines.append("")
        lines.append(f"## 跳过明细（{len(skipped)} 项）")
        lines.append("")
        lines.append("| 用例 | 来源 |")
        lines.append("|---|---|")
        for t in sorted(skipped, key=lambda t: (t["source"], t["name"])):
            lines.append(f"| `{t['name']}` | `{t['source']}` |")

    if failed:
        lines.append("")
        lines.append("## 失败明细")
        lines.append("")
        for t in failed:
            lines.append(f"### `{t['name']}`")
            lines.append("")
            lines.append(f"- 来源：`{t['source']}`")
            if t["duration"] is not None:
                lines.append(f"- 耗时：{t['duration']:.3f}s")
            if t["errors"]:
                for msg in t["errors"][:5]:
                    msg = msg if len(msg) <= MAX_MESSAGE_LEN else msg[:MAX_MESSAGE_LEN] + " ..."
                    lines.append(f"- {msg}")
            else:
                lines.append("- 没有抓到错误消息，去 `Editor.log` 里搜这个用例名。")
            if t["warnings"]:
                for msg in t["warnings"][:3]:
                    lines.append(f"- ⚠ {msg}")
            lines.append("")

    if timed:
        slowest = sorted(timed, key=lambda t: t["duration"], reverse=True)[:SLOWEST_COUNT]
        lines.append("## 最慢的用例")
        lines.append("")
        lines.append("| 用例 | 耗时 |")
        lines.append("|---|---|")
        for t in slowest:
            lines.append(f"| `{t['name']}` | {t['duration']:.3f}s |")
        lines.append("")

    if screenshots:
        lines.append("## 截图")
        lines.append("")
        for shot in screenshots:
            title = shot.replace("-", " ").rsplit(".", 1)[0]
            lines.append(f"### {title}")
            lines.append("")
            lines.append(f"![{title}]({image_prefix}{shot})")
            lines.append("")
    else:
        lines.append("## 截图")
        lines.append("")
        lines.append(
            "没有截图。截图需要真实视口，跑的时候加 `-Screenshots`"
            "（脚本会关掉 `-NullRHI` 并把 `Saved/Screenshots` 收进报告目录）。"
        )
        lines.append("")

    lines.append("---")
    lines.append("")
    if published:
        # This copy is committed and rendered by MkDocs, so it must not carry a machine
        # specific path or a run timestamp: either one makes the file diff on every run
        # even when the results did not change.
        regenerate = "Tools/RunTests.ps1 -Backend " + (backend if backend else "<Lua|Luau>") + " -Publish"
        lines.append(
            "原始 JSON 留在本机 `Saved/TestReports/<时间戳>/<后端>/`（`Saved/` 不入库），"
            f"这里只提交结论。重新生成：`{regenerate}`"
            + ("（要两份就 `-Backend Both`）。" if backend else "。")
        )
    else:
        lines.append(f"原始 JSON 目录：`{reports_dir}`")
    lines.append("")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--reports", required=True, help="directory holding the automation JSON")
    parser.add_argument("--out", default=None, help="where to write report.md / summary.json (default: --reports)")
    parser.add_argument(
        "--backend",
        default=None,
        metavar="Lua|Luau",
        help="which Lua backend produced this run; with --publish it selects the destination"
        " (Docs/test-lua-report.md or Docs/test-luau-report.md)",
    )
    parser.add_argument(
        "--publish",
        action="store_true",
        help="also write the Docs/ copy; that copy is committed, so it carries no "
        "machine specific path and no per-run directory name",
    )
    args = parser.parse_args()

    # Lua and Luau differ only by case here, and the published file name is derived from
    # it, so normalise once: --backend luau and --backend Luau must not be two reports.
    backend = None
    if args.backend:
        normalized = args.backend.strip().lower()
        if normalized not in ("lua", "luau"):
            raise SystemExit(f"unknown backend '{args.backend}' (expected Lua or Luau)")
        backend = "Lua" if normalized == "lua" else "Luau"

    reports_dir = Path(args.reports).resolve()
    out_dir = Path(args.out).resolve() if args.out else reports_dir
    if not reports_dir.is_dir():
        raise SystemExit(f"reports directory not found: {reports_dir}")
    out_dir.mkdir(parents=True, exist_ok=True)

    tests = load_tests(reports_dir)

    shot_dir = out_dir / "screenshots"
    screenshots = sorted(p.name for p in shot_dir.glob("*.png")) if shot_dir.is_dir() else []

    generated = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total = len(tests)
    failed = sum(1 for t in tests if not t["passed"])
    skipped = sum(1 for t in tests if t["skipped"])
    passed = total - failed - skipped

    (out_dir / "report.md").write_text(
        render(tests, screenshots, "screenshots/", generated, reports_dir, backend=backend),
        encoding="utf-8",
        newline="\n",
    )

    summary = {
        "generated": generated,
        "backend": backend,
        "total": total,
        "passed": passed,
        "failed": failed,
        "skipped": skipped,
        "duration": round(sum(t["duration"] for t in tests if t["duration"] is not None), 3),
        "screenshots": len(screenshots),
    }
    (out_dir / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n"
    )

    if args.publish:
        repo_root = Path(__file__).resolve().parent.parent
        docs_dir = repo_root / "Docs"
        # One page per backend, so the two runs are both visible instead of the second
        # overwriting the first. Screenshots get the same treatment: the two backends can
        # legitimately produce different pictures of the same scene.
        tag = backend.lower() if backend else ""
        page_name = f"test-{tag}-report.md" if tag else "test-report.md"
        image_dir_name = f"tests-{tag}" if tag else "tests"
        images_dir = docs_dir / "Images" / image_dir_name
        if screenshots:
            images_dir.mkdir(parents=True, exist_ok=True)
            for name in screenshots:
                shutil.copy2(shot_dir / name, images_dir / name)
        page_path = docs_dir / page_name
        page_path.write_text(
            strip_machine_paths(
                render(
                    tests,
                    screenshots,
                    f"Images/{image_dir_name}/",
                    generated,
                    reports_dir,
                    published=True,
                    backend=backend,
                ),
                repo_root,
            ),
            encoding="utf-8",
            newline="\n",
        )
        print(f"published to {page_path}")

    label = f"[{backend}] " if backend else ""
    print(
        f"{label}{total} tests: {passed} passed, {failed} failed, {skipped} skipped "
        f"-> {out_dir / 'report.md'}"
    )


if __name__ == "__main__":
    main()

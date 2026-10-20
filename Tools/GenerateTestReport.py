#!/usr/bin/env python3
"""Turn UE automation JSON into something a human will actually read.

UE's automation framework exports one JSON blob per test, and the exact shape has drifted
between engine versions (keys differ in case, `errors` is sometimes a count and sometimes
a list, `duration` has several names). This reads all of it defensively so a report still
comes out on an engine upgrade, and the numbers that matter - pass/fail, why, how slow -
end up on one page.

Outputs, next to --reports unless --out says otherwise:
    summary.json   machine-readable totals, so RunTests.ps1 can set its exit code
    report.md      the conclusions: totals, failures with their messages, slowest tests,
                   and a gallery of whatever the tests screenshotted

Usage:
    python GenerateTestReport.py --reports <dir> [--out <dir>] [--publish]
"""

import argparse
import json
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


def render(tests, screenshots, image_prefix, generated, reports_dir):
    total = len(tests)
    failed = [t for t in tests if not t["passed"]]
    skipped = [t for t in tests if t["skipped"]]
    passed = [t for t in tests if t["passed"] and not t["skipped"]]
    timed = [t for t in tests if t["duration"] is not None]
    total_time = sum(t["duration"] for t in timed)

    lines = []
    lines.append("# 测试报告")
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
    lines.append(f"原始 JSON 目录：`{reports_dir}`")
    lines.append("")
    return "\n".join(lines) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--reports", required=True, help="directory holding the automation JSON")
    parser.add_argument("--out", default=None, help="where to write report.md / summary.json (default: --reports)")
    parser.add_argument("--publish", action="store_true", help="also copy the report and screenshots into Docs/")
    args = parser.parse_args()

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
        render(tests, screenshots, "screenshots/", generated, reports_dir),
        encoding="utf-8",
        newline="\n",
    )

    summary = {
        "generated": generated,
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
        images_dir = docs_dir / "Images" / "tests"
        if screenshots:
            images_dir.mkdir(parents=True, exist_ok=True)
            for name in screenshots:
                shutil.copy2(shot_dir / name, images_dir / name)
        (docs_dir / "test-report.md").write_text(
            render(tests, screenshots, "Images/tests/", generated, reports_dir),
            encoding="utf-8",
            newline="\n",
        )
        print(f"published to {docs_dir / 'test-report.md'}")

    print(f"{total} tests: {passed} passed, {failed} failed, {skipped} skipped -> {out_dir / 'report.md'}")


if __name__ == "__main__":
    main()

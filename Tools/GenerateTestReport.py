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

A run that crashed exports no JSON at all, which used to make the report say "0 tests" -
indistinguishable from a run that never started. When that happens this reads Editor.log
next to the (missing) JSON instead and reports what was running and where it died.

Either way that copy is what gets committed and rendered by MkDocs, and it deliberately
points at no local path: the run directory is timestamped and Saved/ is gitignored, so
naming it here would make the file change on every single run.

Baseline guard-rail:
    --baseline <path>      compare this run with a previous one and say what moved
    --update-baseline      overwrite that file with this run
    --fail-on-drift        exit non-zero when something moved

A test that silently stops running is worse than a test that fails: the report stays green
while half the suite disappears (renamed, filtered out, deleted by accident). Comparing the
set of test *names* - not just the totals - is what catches that, so `summary.json` carries
the full list and `--baseline` diffs it. Drift only *warns* by default: renaming a test is a
legitimate thing to do, and a CI that goes red every time someone does is a CI people
silence.

Usage:
    python GenerateTestReport.py --reports <dir> [--out <dir>] [--backend Lua|Luau]
                                 [--publish] [--baseline <path>]
                                 [--update-baseline] [--fail-on-drift]
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

# UE reports success/failure as one of these strings, capitalisation included.
PASS_STATES = {"success", "passed", "pass"}
FAIL_STATES = {"fail", "failed", "failure", "error"}
SKIP_STATES = {"skipped", "skip", "notrun", "notrun", "ignored", "na"}

MAX_MESSAGE_LEN = 400
SLOWEST_COUNT = 10

# The whole-run recording written by Tools/RunTests.ps1 (it starts a gdigrab capture with the
# editor and stops it when the editor exits). It lives in the same videos/ directory as the
# showcase clips, which are recorded by the tests themselves - so it has to be told apart by
# name and is shown on its own, above everything else: the clips stop where the showcase
# tests stop, and a hung or crashed run produces no clip at all.
SESSION_VIDEO_STEM = "session"

# Written by Tools/RunTests.ps1. Which checker runs depends on the backend - Luau scripts
# are checked with luau-analyze, Lua scripts with LuaLS - and each one writes its summary
# under its own name next to the automation JSON.
TYPECHECK_FILES = {"Lua": "lua-analyze.json", "Luau": "luau-analyze.json"}
TYPECHECK_SHOWN = 20

# A clip is this many seconds long, whatever the capture count was: the playback rate is
# derived per sequence (frames / CLIP_SECONDS). Every captured frame is a full viewport
# readback plus a PNG encode, and that is what stalls the run, so RunTests.ps1 wants to be
# able to halve the capture count without halving the clip - 30 frames at 15fps and 60 frames
# at 30fps are both a 2-second clip.
CLIP_SECONDS = 2.0

# What a crash looks like in Editor.log. The automation framework exports nothing when the
# process dies, so the log is the only evidence there is.
CRASH_MARKERS = (
    "=== Critical error: ===",
    "Fatal error!",
    "Unhandled Exception",
    "Assertion failed",
    "Ensure failed",
    "LowLevelFatalError",
)
MAX_STACK_LINES = 25

# Baseline drift: how far the total runtime may move before it is worth mentioning. The
# number is deliberately loose - a shared machine, a cold shader cache or a background
# build all move it by more than any real regression would.
DURATION_DRIFT_RATIO = 0.5

# How many names to spell out in the drift line. The point is to notice, not to dump a
# hundred lines into the build log.
DRIFT_NAMES_SHOWN = 8


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
    # The static checker's summary lands in the same directory as the automation JSON, and
    # it is not a test: without this it was picked up here and showed up in 跳过明细 as a
    # test named "luau-analyze" that nobody had ever run.
    typecheck_files = set(TYPECHECK_FILES.values())
    for path in sorted(reports_dir.glob("*.json")):
        if path.name == "summary.json" or path.name in typecheck_files:
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


def find_log(reports_dir: Path):
    """The editor log for this run, if there is one."""
    for name in ("Editor.log", "Editor-backup.log"):
        candidate = reports_dir / name
        if candidate.is_file():
            return candidate
    found = sorted(reports_dir.glob("Editor*.log"))
    return found[-1] if found else None


def parse_crash(log_path):
    """Pull the crash out of Editor.log: what was running, and where it died.

    Returns None when the log has no crash in it, which is the normal case - a merely
    failed run still exports its JSON and is reported from that.
    """
    if log_path is None:
        return None
    try:
        # utf-8-sig: UE writes Editor.log with a BOM, and utf-8 would leave the marker at
        # the front of the first line.
        lines = log_path.read_text(encoding="utf-8-sig", errors="replace").splitlines()
    except OSError:
        return None

    crash_at = None
    for index, line in enumerate(lines):
        if any(marker in line for marker in CRASH_MARKERS):
            crash_at = index
            break
    if crash_at is None:
        return None

    # Which test died: the last "Test Started" with no "Test Completed" for the same name
    # before the crash. Tests run one at a time, so there is at most one.
    running = None
    for index in range(crash_at):
        started = re.search(r"Test Started\. Name=\{([^}]*)\}", lines[index])
        if not started:
            continue
        name = started.group(1)
        completed = any(
            "Test Completed." in line and f"Name={{{name}}}" in line for line in lines[index + 1 : crash_at]
        )
        if not completed:
            running = name

    window = lines[crash_at : crash_at + 60]
    head = [
        line.rstrip()
        for line in window
        if any(key in line for key in ("Critical", "Fatal", "Unhandled", "Assertion", "Ensure"))
    ][:6]
    stack = [line.rstrip() for line in window if "[Callstack]" in line][:MAX_STACK_LINES]

    return {"test": running, "head": head, "stack": stack}


def render_crash(crash):
    lines = ["## 崩溃", ""]
    lines.append("**测试进程崩了** —— 自动化框架没来得及导出任何 JSON，所以下面的统计不是这次运行的真实结果。")
    lines.append("")
    if crash["test"]:
        lines.append(f"- 崩溃时正在跑：`{crash['test']}`")
    else:
        lines.append("- 崩溃时没有正在跑的用例（可能死在启动或收尾阶段）")
    if crash["head"]:
        lines.append("")
        lines.append("```")
        lines.extend(crash["head"])
        lines.append("```")
    if crash["stack"]:
        lines.append("")
        lines.append("<details><summary>调用栈</summary>")
        lines.append("")
        lines.append("```")
        lines.extend(crash["stack"])
        lines.append("```")
        lines.append("")
        lines.append("</details>")
    lines.append("")
    return lines


def find_frame_sequences(shot_dir):
    """Group a run's PNGs into stills and frame sequences (<name>_0001.png ... <name>_0060.png)."""
    names = sorted(p.name for p in shot_dir.glob("*.png")) if shot_dir.is_dir() else []

    sequences = {}
    stills = []
    for name in names:
        m = re.match(r"^(.+)_(\d{4,})\.png$", name)
        if m:
            sequences.setdefault(m.group(1), []).append(name)
        else:
            stills.append(name)

    return stills, sequences


def find_ffmpeg(explicit=None):
    """Locate an encoder.

    Order: --ffmpeg, $FFMPEG, ffmpeg on PATH, then the static binary imageio-ffmpeg ships.
    The last one is why no system-wide ffmpeg install is needed: it is an ordinary pip
    package (Tools/requirements.txt) that carries its own ffmpeg.
    """
    candidates = []
    if explicit:
        candidates.append(("--ffmpeg", explicit))
    if os.environ.get("FFMPEG"):
        candidates.append(("$FFMPEG", os.environ["FFMPEG"]))
    on_path = shutil.which("ffmpeg")
    if on_path:
        candidates.append(("PATH", on_path))
    try:
        import imageio_ffmpeg

        candidates.append(("imageio-ffmpeg", imageio_ffmpeg.get_ffmpeg_exe()))
    except Exception:
        pass

    for origin, path in candidates:
        if path and Path(path).is_file():
            return origin, path
    return None, None


def encode_sequences(shot_dir, video_dir, sequences, ffmpeg, fps=None):
    """Encode every frame sequence into one .mp4. Returns a {name: reason} map of failures.

    Recording is part of the run, not an extra, so a burst that cannot be encoded is a
    failure with a reason - never a silent pile of PNGs the reader has to decode by eye.

    fps is the playback rate for all sequences; leave it None to derive it per sequence from
    CLIP_SECONDS.
    """
    failures = {}
    if not sequences:
        return failures

    video_dir.mkdir(parents=True, exist_ok=True)
    for name, frames in sorted(sequences.items()):
        out = video_dir / f"{name}.mp4"
        # -framerate: the frames were grabbed one per rendered frame, so any real frame rate
        # is a guess. Deriving it from the frame count keeps the clip CLIP_SECONDS long:
        # 60 frames play at 30fps, 30 frames at 15fps. Without this, halving the captures to
        # make the run less stuttery would also halve every clip.
        rate = fps if fps else max(1, round(len(frames) / CLIP_SECONDS))
        cmd = [
            ffmpeg, "-y", "-loglevel", "error",
            "-framerate", str(rate),
            "-i", str(shot_dir / f"{name}_%04d.png"),
            # libx264 refuses odd dimensions, and a viewport can be any size (a resized
            # editor window, a custom -ResX/-ResY). Crop the last row/column rather than
            # letting the encode fail on something that is not the tests' problem.
            "-vf", "scale=trunc(iw/2)*2:trunc(ih/2)*2",
            "-c:v", "libx264", "-pix_fmt", "yuv420p", "-crf", "23",
            str(out),
        ]
        try:
            proc = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        except (OSError, subprocess.SubprocessError) as e:
            failures[name] = f"编码器跑不起来：{e}"
            continue

        if proc.returncode != 0 or not out.is_file():
            # The last line is usually just "Conversion failed!"; the useful line is above it.
            detail = [line.strip() for line in (proc.stderr or "").strip().splitlines() if line.strip()]
            failures[name] = " / ".join(detail[-3:]) if detail else f"退出码 {proc.returncode}"

    return failures


def collect_media(shot_dir, video_dir):
    """Split what the run produced into stills, frame sequences and encoded clips.

    A clip arrives as a burst of <name>_0001.png ... <name>_0060.png. Listing sixty copies of
    the same scene as if they were sixty different pictures buries the stills, so they are
    collapsed into one entry per clip; if an encoder ran there is a real video instead.

    One mp4 in there is not a clip: session.mp4 is the whole run, recorded by the runner. It
    is returned separately so the report can lead with it - the clips are recorded by the
    tests, so they are silent exactly when something goes wrong.
    """
    stills, sequences = find_frame_sequences(shot_dir)

    clips = []
    session = None
    if video_dir.is_dir():
        for p in sorted(video_dir.glob("*.mp4")):
            entry = {"name": p.stem, "file": p.name, "frames": len(sequences.get(p.stem, []))}
            if p.stem == SESSION_VIDEO_STEM:
                session = entry
            else:
                clips.append(entry)

    # A burst that was never encoded is still worth showing: the first frame, and how many
    # there are, so the reader knows it is a recording and not a duplicate.
    uncoded = [
        {"name": key, "first": frames[0], "frames": len(frames)}
        for key, frames in sorted(sequences.items())
        if not any(c["name"] == key for c in clips)
    ]

    return stills, clips, uncoded, session


def load_typecheck(reports_dir: Path, backend=None):
    """Read the summary this backend's static checker left next to the JSON, if any.

    The two backends do not share a checker - Luau runs luau-analyze, Lua runs LuaLS - so
    the backend decides which file to look for. Without a backend, take whichever one is
    there, which keeps a single unlabelled run working.
    """
    names = []
    if backend in TYPECHECK_FILES:
        names.append(TYPECHECK_FILES[backend])
    names += [name for name in TYPECHECK_FILES.values() if name not in names]

    for name in names:
        path = reports_dir / name
        if not path.is_file():
            continue
        try:
            # utf-8-sig, not utf-8: PowerShell's Set-Content -Encoding UTF8 writes a BOM, and
            # plain utf-8 raises on it - which made the whole type checking section disappear
            # from every report while looking like the checker had never run at all.
            return json.loads(path.read_text(encoding="utf-8-sig", errors="replace"))
        except (OSError, json.JSONDecodeError) as e:
            # A broken summary must not take the report down with it.
            print(f"WARNING: could not read {path}: {e}", file=sys.stderr)
            return None
    return None


def render_typecheck(tc) -> list:
    # Named after the checker that actually ran: the two backends do not use the same one.
    checker = tc.get("checker") or "静态检查"
    lines = [f"## 静态类型检查（{checker}）", ""]

    if tc.get("skipped"):
        lines.append("**跳过。**")
        lines.append("")
        lines.append(tc.get("reason") or "没有说明原因。")
        lines.append("")
        return lines

    errors = tc.get("errors") or []
    warnings = tc.get("warnings") or []

    lines.append("| 指标 | 数值 |")
    lines.append("|---|---|")
    lines.append(f"| 检查文件数 | {tc.get('files', 0)} |")
    lines.append(f"| 模式 | `{tc.get('mode', '-')}` |")
    lines.append(f"| 错误 | {len(errors)} |")
    lines.append(f"| 警告 | {len(warnings)} |")
    lines.append("")

    if errors:
        lines.append("**有类型错误 ❌** —— 输出到日志里的前几条：")
        lines.append("")
        lines.append("```")
        for line in errors[:TYPECHECK_SHOWN]:
            lines.append(line[:MAX_MESSAGE_LEN])
        if len(errors) > TYPECHECK_SHOWN:
            lines.append(f"... 还有 {len(errors) - TYPECHECK_SHOWN} 条")
        lines.append("```")
        lines.append("")
    else:
        lines.append("**没有类型错误 ✅**（声明文件由编辑器「导出智能提示」生成。）")
        lines.append("")

    return lines


# Per backend on purpose: Benchmark.spec.cpp writes latest-Luau.csv / latest-Lua.csv. A single
# shared pair of files compared a Luau run against the Lua 5.5 run -Backend Both had just
# finished, and reported the gap between two different VMs as if it were a regression.
BENCHMARK_FILE = "latest-{backend}.csv"
BENCHMARK_PREVIOUS = "previous-{backend}.csv"
# Benchmarks are timed on whatever machine ran them, so a tight threshold would make the
# report cry wolf on every loaded build machine. 30% is deliberately loose: it is there to
# catch "this change made calls twice as slow", not to police noise.
BENCHMARK_DRIFT = 0.30


def load_benchmark(repo_root: Path, backend=None):
    """Read Saved/Benchmark for this backend: this run's CSV and, if there is one, the run
    before it - of the *same* backend.

    Returns (rows, previous). Both are dicts of Case -> per-op microseconds. Missing
    files are not an error: the benchmark is its own automation case, and a run that did
    not execute it should lose the section, not fail the report.
    """
    import csv

    bench_dir = repo_root / "Saved" / "Benchmark"

    def read(pattern):
        path = bench_dir / pattern.format(backend=backend or "unknown")
        if not path.is_file():
            return None
        try:
            with path.open(newline="", encoding="utf-8") as handle:
                return {
                    row["Case"]: float(row["PerOpUs"])
                    for row in csv.DictReader(handle)
                    if row.get("Case") and row.get("PerOpUs")
                }
        except (OSError, ValueError, KeyError):
            # A half-written CSV must not take the report down with it.
            return None

    return read(BENCHMARK_FILE), read(BENCHMARK_PREVIOUS)


def compare_benchmark(rows, previous):
    """Pair each case with its change against the previous run, if there was one."""
    if not rows:
        return []
    out = []
    for case in sorted(rows):
        now = rows[case]
        if previous and case in previous and previous[case] > 0:
            before = previous[case]
            change = (now - before) / before
            out.append({"case": case, "now": now, "before": before, "change": change})
        else:
            out.append({"case": case, "now": now, "before": None, "change": None})
    return out


def render_benchmark(comparison) -> list:
    lines = ["## 性能环比", ""]
    if not comparison:
        lines.append("这一轮没有性能数据（`Saved/Benchmark/latest-<后端>.csv` 不存在）。")
        lines.append("")
        return lines

    lines.append("| 用例 | 本次（µs/次） | 上次（µs/次） | 变化 |")
    lines.append("|---|---|---|---|")
    for row in comparison:
        if row["change"] is None:
            lines.append(f"| {row['case']} | {row['now']:.4f} | — | 无上次数据 |")
            continue
        pct = row["change"] * 100.0
        flag = " ⚠️" if abs(row["change"]) > BENCHMARK_DRIFT else ""
        lines.append(
            f"| {row['case']} | {row['now']:.4f} | {row['before']:.4f} | {pct:+.1f}%{flag} |"
        )
    lines.append("")
    lines.append("> `VM.*` 四项不跨越语言边界，测的是虚拟机自身；其余三项测的是脚本进入 C++ 的那一跳，")
    lines.append("> 开销在参数编组与函数解析上，两个后端因此落在同一量级。")
    lines.append("> `VM.TableGrow` 是空表逐个增长（rehash），`VM.TableRead` 是同一张表的纯数组读取 ——")
    lines.append("> 拆开是为了区分「建表贵」和「读表贵」，这两件事对脚本作者完全不是一回事。")
    lines.append("")
    lines.append(
        f"> 与**同一后端**的上一轮比较（CSV 按后端分开存），超过 ±{int(BENCHMARK_DRIFT * 100)}% 会标 ⚠️，"
        "但**不会**让这轮失败：计时受机器负载影响，这里只做提示，不做判定。"
    )
    lines.append("")
    return lines


def render(
    tests,
    screenshots,
    image_prefix,
    generated,
    reports_dir,
    published=False,
    backend=None,
    crash=None,
    clips=None,
    session=None,
    uncoded=None,
    typecheck=None,
    encode_failures=None,
    benchmark=None,
):
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
    if crash:
        # Up front: with a crash the numbers below are incomplete, and a reader who only
        # looks at the pass rate would take them at face value.
        lines.extend(render_crash(crash))
    lines.append("## 结论")
    lines.append("")
    if total == 0:
        if crash:
            lines.append("**没有找到任何自动化测试结果：进程崩溃，见上面的「崩溃」一节。**")
        else:
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

    clips = clips or []
    uncoded = uncoded or []

    # 整场录屏放在最前面：它是唯一能把"卡在第几秒"说清楚的东西。
    if session:
        lines.append("## 整场录屏")
        lines.append("")
        lines.append(
            "不是 showcase 的剪辑——剪辑是用例自己录的，只覆盖 showcase 那几个测试；"
            "跑挂了的那一轮往往一个剪辑都没有。这一段是 `Tools/RunTests.ps1` 在启动编辑器的同时"
            "开始录、编辑器退出时结束，从第一条测试到最后一条都在里面（窗口出现前的几秒除外）。"
        )
        lines.append("")
        lines.append(
            f'<video controls muted playsinline width="640" '
            f'src="{image_prefix}{session["file"]}"></video>'
        )
        lines.append("")
        lines.append(f"[打不开播放器的话：直接下载这个 mp4（标准 H.264）]({image_prefix}{session['file']})")
        lines.append("")
    else:
        lines.append("## 整场录屏")
        lines.append("")
        lines.append(
            "这一轮没有整场录屏：`-NoVideo` / `-NoSessionVideo` / `-NullRHI` 会关掉它，"
            "找不到 ffmpeg（没装 imageio-ffmpeg，见 `Tools/requirements.txt`）时也不会录。"
        )
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
            "没有截图。截图由 `UnLuaNova.Showcase.*` 这几条用例产出（默认就会收进报告目录），"
            "加 `-NoScreenshots` 或跑 `-NullRHI`（无头）时不会有画面。"
        )
        lines.append("")

    if clips or uncoded:
        lines.append("## 视频")
        lines.append("")
        for clip in clips:
            title = clip["name"].replace("-", " ")
            lines.append(f"### {title}")
            lines.append("")
            # Raw HTML: plain Markdown has no video syntax, and md_in_html is enabled for
            # exactly this kind of embed.
            lines.append(
                f'<video controls muted loop playsinline width="640" src="{image_prefix}{clip["file"]}"></video>'
            )
            lines.append("")
            # GitHub's markdown renderer strips raw HTML, so on a report page read there the
            # <video> above silently disappears and reads as "the clip cannot be played" - the
            # file itself is a standard H.264 mp4. A plain link survives every renderer.
            lines.append(f"[播放不了？直接打开视频文件（标准 H.264 mp4）]({image_prefix}{clip['file']})")
            lines.append("")
            frames = f"，{clip['frames']} 帧" if clip["frames"] else ""
            lines.append(f"{frames}，由这些帧编码而成（`Tools/RunTests.ps1 -Video`）。")
            lines.append("")
        for seq in uncoded:
            title = seq["name"].replace("-", " ")
            lines.append(f"### {title}")
            lines.append("")
            lines.append(f"![{title}]({image_prefix}{seq['first']})")
            lines.append("")
            # An unencoded burst is a failure, not a fallback: the point of the run was the
            # clip, and a reader cannot tell "not encoded" from "nothing happened".
            reason = (encode_failures or {}).get(seq["name"], "编码器没有产出文件。")
            lines.append(
                f"**编码失败 ❌** —— 只拿到帧序列（{seq['frames']} 帧："
                f"`{seq['name']}_0001.png` … `{seq['name']}_{seq['frames']:04d}.png`）。原因：{reason}"
            )
            lines.append("")

    # Type checking is not an automation test, so it never appears in the JSON the engine
    # exports - and before this it never appeared in the report either, which left the
    # Luau declarations and luau-analyze with no result anywhere anyone looks.
    if typecheck is not None:
        lines.extend(render_typecheck(typecheck))
    elif backend in TYPECHECK_FILES:
        lines.append("## 静态类型检查")
        lines.append("")
        lines.append(f"这一轮没有跑：`Tools/RunTests.ps1` 没写 `{TYPECHECK_FILES[backend]}`。")
        lines.append("")

    if benchmark is not None:
        lines.extend(render_benchmark(benchmark))

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


def default_baseline_path(backend):
    """Where the previous run of this backend is recorded, when nobody said otherwise."""
    name = (backend or "unknown").lower() + ".json"
    return Path(__file__).resolve().parent / "baseline" / name


def load_baseline(path: Path):
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        # A half-written or hand-edited baseline must not take the whole run down with it.
        print(f"WARNING: baseline '{path}' is unreadable ({exc}); skipping the comparison.", file=sys.stderr)
        return None
    if not isinstance(data, dict):
        print(f"WARNING: baseline '{path}' is not an object; skipping the comparison.", file=sys.stderr)
        return None
    return data


def compare_with_baseline(summary, baseline):
    """Diff this run against the recorded one.

    Returns (drifted, [human readable lines]). `drifted` is about the *set of tests*: a
    renamed, deleted or filtered-out test is the thing that must not pass unnoticed.
    """
    lines = []
    drifted = False

    old_total = baseline.get("total")
    new_total = summary.get("total")
    if isinstance(old_total, int) and isinstance(new_total, int) and old_total != new_total:
        lines.append(f"用例总数 {old_total} -> {new_total}（{new_total - old_total:+d}）")

    old_names = set(baseline.get("names") or [])
    new_names = set(summary.get("names") or [])
    missing = sorted(old_names - new_names)
    added = sorted(new_names - old_names)
    if missing:
        drifted = True
        shown = ", ".join(missing[:DRIFT_NAMES_SHOWN])
        lines.append(f"{len(missing)} 个用例消失：{shown}{' …' if len(missing) > DRIFT_NAMES_SHOWN else ''}")
    if added:
        drifted = True
        shown = ", ".join(added[:DRIFT_NAMES_SHOWN])
        lines.append(f"{len(added)} 个用例新增：{shown}{' …' if len(added) > DRIFT_NAMES_SHOWN else ''}")

    old_duration = baseline.get("duration")
    new_duration = summary.get("duration")
    if (
        isinstance(old_duration, (int, float))
        and isinstance(new_duration, (int, float))
        and old_duration > 0
    ):
        ratio = new_duration / old_duration
        if abs(ratio - 1.0) > DURATION_DRIFT_RATIO:
            lines.append(f"耗时 {old_duration:.1f}s -> {new_duration:.1f}s（{ratio:.0%}，超出 ±50%）")

    return drifted, lines


def baseline_payload(summary):
    """Only the stable parts: enough to notice drift, nothing that changes every run."""
    return {
        "generated": summary["generated"],
        "backend": summary["backend"],
        "total": summary["total"],
        "failed": summary["failed"],
        "skipped": summary["skipped"],
        "duration": summary["duration"],
        "names": summary["names"],
    }


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
        "--ffmpeg",
        default=None,
        metavar="PATH",
        help="encoder to use for the recorded frame sequences; by default $FFMPEG, then "
        "ffmpeg on PATH, then the binary imageio-ffmpeg ships",
    )
    parser.add_argument(
        "--publish",
        action="store_true",
        help="also write the Docs/ copy; that copy is committed, so it carries no "
        "machine specific path and no per-run directory name",
    )
    parser.add_argument(
        "--baseline",
        default=None,
        metavar="PATH",
        help="compare this run against a previous one (total, test names, duration) and "
        "print what moved; defaults to Tools/baseline/<backend>.json when updating",
    )
    parser.add_argument(
        "--update-baseline",
        action="store_true",
        help="overwrite the baseline with this run - an intentional, reviewed act, which "
        "is why it is not folded into a normal run",
    )
    parser.add_argument(
        "--fail-on-drift",
        action="store_true",
        help="exit non-zero when the set of tests changed; off by default so a legitimate "
        "rename does not turn the build red",
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

    # A crash exports no JSON, so this is the only way to say anything about it. Parsed
    # unconditionally - a run can crash after exporting part of its results, and then the
    # numbers are incomplete even though they are not all zero.
    crash = parse_crash(find_log(reports_dir))
    if crash:
        print(
            "WARNING: this run crashed"
            + (f" while running '{crash['test']}'" if crash["test"] else "")
            + "; the totals below are incomplete.",
            file=sys.stderr,
        )

    shot_dir = out_dir / "screenshots"
    video_dir = out_dir / "videos"

    # Recording is part of the run: the showcase tests capture a burst of frames, and those
    # bursts are encoded here. Encoding lives in this script rather than in RunTests.ps1 so
    # that "which encoder" is decided in exactly one place.
    _, sequences = find_frame_sequences(shot_dir)
    encode_failures = {}
    if sequences:
        origin, ffmpeg = find_ffmpeg(args.ffmpeg)
        if ffmpeg:
            print(f"Encoding   : {len(sequences)} clip(s) with ffmpeg from {origin}")
            encode_failures = encode_sequences(shot_dir, video_dir, sequences, ffmpeg)
        else:
            for name in sequences:
                encode_failures[name] = (
                    "找不到编码器：`pip install -r Tools/requirements.txt`（imageio-ffmpeg 自带静态 ffmpeg），"
                    "或把 ffmpeg 放进 PATH，或用 --ffmpeg 指定。"
                )
        for name, reason in sorted(encode_failures.items()):
            print(f"ERROR: clip '{name}' was not encoded: {reason}", file=sys.stderr)

    screenshots, clips, uncoded, session = collect_media(shot_dir, video_dir)

    typecheck = load_typecheck(reports_dir, backend)

    repo_root = Path(__file__).resolve().parent.parent
    bench_rows, bench_previous = load_benchmark(repo_root, backend)
    benchmark = compare_benchmark(bench_rows, bench_previous) if bench_rows else None

    generated = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    total = len(tests)
    failed = sum(1 for t in tests if not t["passed"])
    skipped = sum(1 for t in tests if t["skipped"])
    passed = total - failed - skipped

    (out_dir / "report.md").write_text(
        render(
            tests,
            screenshots,
            "screenshots/",
            generated,
            reports_dir,
            backend=backend,
            crash=crash,
            clips=clips,
            session=session,
            uncoded=uncoded,
            typecheck=typecheck,
            encode_failures=encode_failures,
            benchmark=benchmark,
        ),
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
        "videos": len(clips),
        # The whole-run recording is not a clip and does not count as one: it is one file
        # whatever the run did, including the runs that produced no clip at all.
        "session_recording": session["file"] if session else None,
        "videos_failed": len(encode_failures),
        "crashed": crash is not None,
        "crash_test": crash["test"] if crash else None,
        # The whole point of the baseline: totals alone cannot tell a renamed test from a
        # deleted one, so the set of names travels with the numbers.
        "names": sorted(t["name"] for t in tests),
        # None when it was not run this round - deliberately not folded into pass/fail,
        # because it is not an automation test and a skipped check is not a failed test.
        "typecheck": {
            "ran": typecheck is not None and not typecheck.get("skipped"),
            "errors": len(typecheck.get("errors") or []) if typecheck else 0,
            "warnings": len(typecheck.get("warnings") or []) if typecheck else 0,
        },
    }
    (out_dir / "summary.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n"
    )

    # Compared before anything can exit: a report that quietly covers fewer tests than last
    # time is the failure mode this is here to catch, and it is invisible in the numbers
    # alone because every test it still has is green.
    drift_exit = 0
    if args.baseline or args.update_baseline:
        baseline_path = Path(args.baseline).resolve() if args.baseline else default_baseline_path(backend)
        baseline = load_baseline(baseline_path)
        if baseline is None:
            print(f"BASELINE: 没有基线（{baseline_path}）；确认这份结果无误后用 --update-baseline 创建。")
        else:
            drifted, drift_lines = compare_with_baseline(summary, baseline)
            if drift_lines:
                for line in drift_lines:
                    print(f"BASELINE: {line}")
            else:
                print(f"BASELINE: 与基线一致（{summary['total']} 个用例）")
            if drifted:
                print(
                    "BASELINE: 用例集变了。确认是有意为之后，跑 RunTests.ps1 -UpdateBaseline 更新基线。",
                    file=sys.stderr,
                )
                drift_exit = 1

        if args.update_baseline:
            baseline_path.parent.mkdir(parents=True, exist_ok=True)
            baseline_path.write_text(
                json.dumps(baseline_payload(summary), ensure_ascii=False, indent=2) + "\n",
                encoding="utf-8",
                newline="\n",
            )
            print(f"BASELINE: 已写入 {baseline_path}（{summary['total']} 个用例）")

    if encode_failures:
        # Loud, not silent: a report that quietly shows a pile of PNGs looks like a run that
        # recorded something. Failing here also makes RunTests.ps1 exit non-zero.
        raise SystemExit(f"{len(encode_failures)} clip(s) could not be encoded; see the report.")

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
        # Everything the run produced, not just the stills: the frame sequences a clip is
        # made of are images too, and the report links them when ffmpeg is unavailable.
        if screenshots or clips or uncoded or session:
            images_dir.mkdir(parents=True, exist_ok=True)
            for name in screenshots + [f for seq in uncoded for f in [seq["first"]]]:
                shutil.copy2(shot_dir / name, images_dir / name)
            for clip in clips:
                shutil.copy2(video_dir / clip["file"], images_dir / clip["file"])
            if session:
                shutil.copy2(video_dir / session["file"], images_dir / session["file"])
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
                    crash=crash,
                    clips=clips,
                    session=session,
                    uncoded=uncoded,
                    # typecheck / encode_failures were missing here, so the committed page
                    # always said "这一轮没有跑" even on runs whose checker did run - only the
                    # copy under Saved/ ever showed the section.
                    typecheck=typecheck,
                    encode_failures=encode_failures,
                    benchmark=benchmark,
                ),
                repo_root,
            ),
            encoding="utf-8",
            newline="\n",
        )
        print(f"published to {page_path}")

    if benchmark:
        # Warn only. Timings depend on the machine, so a regression here is a signal to
        # look at, never a reason to fail a run - see BENCHMARK_DRIFT.
        for row in benchmark:
            if row["change"] is not None and abs(row["change"]) > BENCHMARK_DRIFT:
                print(
                    f"BENCHMARK: {row['case']} {row['change'] * 100:+.1f}% "
                    f"({row['before']:.4f} -> {row['now']:.4f} us/op) - warn only",
                    file=sys.stderr,
                )

    label = f"[{backend}] " if backend else ""
    print(
        f"{label}{total} tests: {passed} passed, {failed} failed, {skipped} skipped "
        f"-> {out_dir / 'report.md'}"
    )

    if drift_exit and args.fail_on_drift:
        raise SystemExit("用例集与基线不一致；确认后更新基线（RunTests.ps1 -UpdateBaseline）。")


if __name__ == "__main__":
    main()

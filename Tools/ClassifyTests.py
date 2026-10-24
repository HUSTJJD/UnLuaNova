#!/usr/bin/env python3
"""Classify every automation test by the environment it actually needs.

The test suite has grown by accretion, and the question "how much of the run is the editor
starting up, and how much is the tests" cannot be answered from the totals: every test
currently runs inside one UnrealEditor-Cmd process, so the cost of the carrier is spread
invisibly across all of them. Sorting them into what they *require* is the first step
towards moving whole classes off that carrier.

Classes, most demanding last:

    A   pure logic - no UWorld. Lua env, reflection metadata, strings, containers.
    B   needs a UWorld but no network. Opens a map standalone, or builds its own world.
    C   needs a PIE session *with* peers (NumClients / a PlayNetMode was passed).
    D   needs a real viewport / RHI (registered with NonNullRHI).
    E   captures pixels (screenshot or a frame burst).

D and E overlap heavily - both are the showcase tests - and both are the only ones that
genuinely need the editor. Everything else is on the editor carrier by inertia.

Classification is static: it reads the source, not a run. That is deliberate - it has to
work before any of this has been moved, and it has to say what a test *declares* rather
than what it happened to touch. A test that opens a map conditionally is classified as
needing the map; being over-inclusive here is safe, because the class is used to decide
what may be moved, and a test moved to a cheaper carrier that turns out to need more will
fail loudly.

Usage:
    python Tools/ClassifyTests.py [--out Tools/baseline/classification.json] [--print]

The output is consumed by GenerateTestReport.py (--classification), which multiplies it by
the per-test durations in the baseline to answer "how many seconds does each class cost".
"""

import argparse
import json
import re
import sys
from datetime import datetime
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SUITE_ROOT = REPO_ROOT / "Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private"
SCAN_DIRS = ("Specs", "Tests")

if not SUITE_ROOT.is_dir():
    # The suite is a plugin nested inside this plugin; accept the flatter layout too so the
    # script does not silently classify nothing when the checkout is arranged differently.
    SUITE_ROOT = REPO_ROOT / "Plugins/UnLuaNova/Plugins/UnLuaNovaTestSuite/Source/UnLuaNovaTestSuite/Private"

# Highest class wins, so a single file is reported by its strongest requirement.
CLASS_ORDER = ["A", "B", "C", "D", "E"]

CLASS_LABELS = {
    "A": "纯逻辑（不需要 UWorld）",
    "B": "需要 UWorld，不联网",
    "C": "联网（PIE 会话或跨进程 peer）",
    "D": "需要真实视口 / RHI",
    "E": "截图 / 连拍",
    "?": "未分类（没找到注册宏）",
}

# ---- markers ---------------------------------------------------------------
#
# Each entry: (class, compiled regex). The file gets that class when the regex matches.

MARKERS = [
    # E first in effect only through CLASS_ORDER; order here does not matter.
    ("E", re.compile(r"\bFScreenshotLatentCommand\b")),
    ("E", re.compile(r"\bFCaptureFramesLatentCommand\b")),
    ("D", re.compile(r"\bBEGIN_VIEWPORT_TESTSUITE\b")),
    # A network session is signalled by passing NumClients / PlayNetMode, i.e. more than
    # the map name (and optionally bForceReload). FOpenMapLatentCommand's signature is
    # (MapName, bForceReload=false, NumClients=1, PlayNetMode=PIE_Standalone), so three or
    # more arguments at a call site is what distinguishes B from C.
    ("C", re.compile(r"FOpenMapLatentCommand\s*\([^)]*,[^)]*,[^)]+[,)]", re.DOTALL)),
    # C also covers the cross-process topologies: a test that declares an FNetSession
    # launches real peers, which is a networked test whatever it does in its own process.
    ("C", re.compile(r"\bFNetSession\b")),
    ("B", re.compile(r"\bFOpenMapLatentCommand\b")),
    # Tests that build a world themselves rather than opening a map.
    ("B", re.compile(r"\bEWorldType::(Game|PIE)\b")),
    ("B", re.compile(r"\bFUnLuaNovaTestBase\b")),
]

# "UnLuaNova.API.Net" out of the registration macros, so durations can be joined by name.
RE_DEFINE_SPEC = re.compile(
    r"BEGIN_DEFINE_SPEC\s*\(\s*\w+\s*,\s*\"([^\"]+)\"", re.MULTILINE
)
# \w* covers the variants (BEGIN_TESTSUITE_ANY_CONTEXT): same macro, extra flag, and a
# test whose name is not extracted here is a test the carrier/tier logic cannot see.
RE_TESTSUITE = re.compile(
    r"BEGIN_TESTSUITE\w*\s*\(\s*\w+\s*,\s*TEXT\s*\(\s*\"((?:[^\"\\]|\\.)*)\"", re.MULTILINE
)
RE_VIEWPORT_SUITE = re.compile(
    r"BEGIN_VIEWPORT_TESTSUITE\s*\(\s*\w+\s*,\s*TEXT\s*\(\s*\"((?:[^\"\\]|\\.)*)\"", re.MULTILINE
)
# The raw engine macro, used directly by a couple of files that predate the wrappers
# (OutParamTest.cpp). Matched last - BEGIN_TESTSUITE expands to it, so everything above
# already matched once by the time this runs and duplicates are dropped by name.
RE_SIMPLE = re.compile(
    r"IMPLEMENT_SIMPLE_AUTOMATION_TEST\s*\(\s*\w+\s*,\s*TEXT\s*\(\s*\"((?:[^\"\\]|\\.)*)\"", re.MULTILINE
)


def classify_source(text: str) -> str:
    """The strongest class any marker in this file asks for."""
    found = {cls for cls, pattern in MARKERS if pattern.search(text)}
    for cls in reversed(CLASS_ORDER):
        if cls in found:
            return cls
    registered = any(
        p.search(text) for p in (RE_DEFINE_SPEC, RE_TESTSUITE, RE_VIEWPORT_SUITE, RE_SIMPLE)
    )
    return "A" if registered else "?"


def extract_names(text: str) -> list:
    """Pretty names registered by this file. Escaped quotes are unescaped."""
    names = []
    for pattern in (RE_DEFINE_SPEC, RE_TESTSUITE, RE_VIEWPORT_SUITE, RE_SIMPLE):
        for match in pattern.finditer(text):
            name = match.group(1).replace('\\"', '"')
            if name not in names:
                names.append(name)
    return names


def scan() -> dict:
    files = {}
    tests = {}
    unclassified_files = []

    for sub in SCAN_DIRS:
        directory = SUITE_ROOT / sub
        if not directory.is_dir():
            print(f"WARNING: {directory} does not exist; skipped.", file=sys.stderr)
            continue

        for path in sorted(directory.glob("*.cpp")):
            text = path.read_text(encoding="utf-8", errors="replace")
            cls = classify_source(text)
            rel = path.relative_to(REPO_ROOT).as_posix()
            files[rel] = cls

            if cls == "?":
                unclassified_files.append(rel)

            for name in extract_names(text):
                # A file can register several; keep the file's class for each. D/E files
                # register their showcase variants through macros, so the name-based map
                # is intentionally coarser than the file-based one.
                tests[name] = cls

    counts = {cls: 0 for cls in CLASS_ORDER + ["?"]}
    for cls in files.values():
        counts[cls] += 1

    return {
        "generated": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "classes": CLASS_LABELS,
        "counts": counts,
        "files": files,
        "tests": tests,
        "unclassified_files": unclassified_files,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument(
        "--out",
        default=None,
        help="where to write the JSON (default: Tools/baseline/classification.json)",
    )
    parser.add_argument("--print", action="store_true", help="also print a summary table")
    args = parser.parse_args()

    result = scan()

    out_path = Path(args.out) if args.out else REPO_ROOT / "Tools/baseline/classification.json"
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        json.dumps(result, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
        newline="\n",
    )

    total_files = sum(result["counts"].values())
    print(f"classified {total_files} files, {len(result['tests'])} registered test names -> {out_path}")
    for cls in CLASS_ORDER + ["?"]:
        count = result["counts"][cls]
        print(f"  {cls}  {CLASS_LABELS[cls]:<28} {count:>4} files")

    if result["unclassified_files"]:
        print(
            f"WARNING: {len(result['unclassified_files'])} file(s) had no registration macro; "
            "check them by hand.",
            file=sys.stderr,
        )

    if args.print:
        print()
        for rel, cls in sorted(result["files"].items(), key=lambda kv: (kv[1], kv[0])):
            print(f"  {cls}  {rel}")


if __name__ == "__main__":
    main()

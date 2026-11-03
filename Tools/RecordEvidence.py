#!/usr/bin/env python3
"""Turn a UE automation report into a capability evidence file.

`feature-matrix.json` refuses to record execution results on purpose:
"execution claims require an evidence schema, not a status edit". This script is that
schema's writer. It never touches `feature-matrix.json` — it reads the report, works out
which capability each executed test belongs to, and writes the result under
`Tools/evidence/`. `AuditRepository.py` then renders the two side by side.

The test -> capability path is derived, not hand-written:
    report `fullTestPath`
      -> longest matching spec name declared by BEGIN_DEFINE_SPEC / BEGIN_TESTSUITE
      -> that spec's source file
      -> capability ids whose `tests` list that file

Usage:
    python Tools/RecordEvidence.py --report Saved\\MyRun --backend Luau --carrier Editor

Importing a report produced by an older build MUST name that build explicitly, otherwise
the record would claim today's HEAD as its provenance:
    python Tools/RecordEvidence.py --report Saved\\ReleaseTests\\baseline\\lua\\game ^
        --backend Lua --carrier Game --commit 347bb8c5 ^
        --run-id 2026-11-03-baseline-game-lua
Records from a different commit are flagged `historical`; `worktree_dirty` still describes
the current tree, so it says nothing about that older run.
"""

from __future__ import annotations

import argparse
import io
import json
import os
import re
import subprocess
import sys
from datetime import datetime, timezone

SCHEMA = "unluanova-evidence/1"

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MATRIX = os.path.join(REPO_ROOT, "Tools", "feature-matrix.json")
EVIDENCE_DIR = os.path.join(REPO_ROOT, "Tools", "evidence")
TEST_ROOT = os.path.join(REPO_ROOT, "Plugins", "UnLuaNovaTestSuite", "Source")

# Spec name declarations live in the C++ sources. The name is the automation path prefix.
SPEC_RE = re.compile(
    r'BEGIN_DEFINE_SPEC\s*\(\s*\w+\s*,\s*"([^"]+)"'
    r'|BEGIN_TESTSUITE(?:_ANY_CONTEXT)?\s*\(\s*\w+\s*,\s*(?:TEXT\()?\s*"([^"]+)"'
)


def git(*args):
    try:
        out = subprocess.run(["git", "-C", REPO_ROOT] + list(args),
                             stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        if out.returncode != 0:
            return ""
        return out.stdout.decode("utf-8", "replace").strip()
    except Exception:
        return ""


def scan_specs():
    """Map declared spec name -> source path relative to the repo root."""
    specs = {}
    for root, _dirs, files in os.walk(TEST_ROOT):
        for name in files:
            if not name.endswith(".cpp"):
                continue
            path = os.path.join(root, name)
            with io.open(path, "r", encoding="utf-8", errors="replace") as handle:
                text = handle.read()
            for match in SPEC_RE.finditer(text):
                spec_name = match.group(1) or match.group(2)
                if spec_name:
                    specs[spec_name] = os.path.relpath(path, REPO_ROOT).replace("\\", "/")
    return specs


def spec_for_test(full_path, specs):
    """Longest declared spec name that prefixes this test path."""
    best_name, best_file = None, None
    for spec_name, spec_file in specs.items():
        if full_path == spec_name or full_path.startswith(spec_name + "."):
            if best_name is None or len(spec_name) > len(best_name):
                best_name, best_file = spec_name, spec_file
    return best_name, best_file


def norm(path):
    return (path or "").replace("\\", "/").lstrip("./")


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--report", required=True,
                        help="directory holding the automation report's index.json")
    parser.add_argument("--backend", required=True, choices=["Lua", "Luau"])
    parser.add_argument("--carrier", default="Editor", choices=["Editor", "Game"])
    parser.add_argument("--config", default="Development")
    parser.add_argument("--engine", default="unknown", help="e.g. 5.8.3-0+UE5")
    parser.add_argument("--run-id", default="")
    parser.add_argument("--commit", default="",
                        help="commit the recorded run was built from; defaults to current HEAD. "
                             "REQUIRED when importing a historical report: attributing an old run "
                             "to today's HEAD would be a false provenance claim.")
    parser.add_argument("--source", default="",
                        help="where the report came from; defaults to --report, made relative to the repo")
    args = parser.parse_args()

    index_path = os.path.join(args.report, "index.json")
    if not os.path.isfile(index_path):
        sys.stderr.write("no index.json under %s\n" % args.report)
        return 1

    # utf-8-sig, not utf-8: UE writes the automation report with a BOM and plain utf-8
    # raises JSONDecodeError on it.
    with io.open(index_path, "r", encoding="utf-8-sig") as handle:
        report = json.load(handle)
    with io.open(MATRIX, "r", encoding="utf-8-sig") as handle:
        matrix = json.load(handle)

    # spec file -> capability ids
    file_to_caps = {}
    for feature in matrix.get("features", []):
        for test_path in feature.get("tests") or []:
            file_to_caps.setdefault(norm(test_path), []).append(feature["id"])

    specs = scan_specs()
    if not specs:
        sys.stderr.write("no spec declarations found under %s\n" % TEST_ROOT)
        return 1

    capabilities = {}
    totals = {"tests": 0, "passed": 0, "failed": 0, "not_run": 0}
    # A spec can back more than one capability, so per-capability counts add up to more
    # than the number of tests. Count distinct mapped tests separately.
    mapped_tests = 0

    for test in report.get("tests") or []:
        full_path = test.get("fullTestPath") or ""
        state = (test.get("state") or "").lower()
        passed = state == "success"
        failed = state in ("fail", "failed", "error")

        totals["tests"] += 1
        if passed:
            totals["passed"] += 1
        elif failed:
            totals["failed"] += 1
        else:
            totals["not_run"] += 1

        _name, spec_file = spec_for_test(full_path, specs)
        if not spec_file:
            continue
        caps = file_to_caps.get(norm(spec_file), [])
        if caps:
            mapped_tests += 1
        for cap_id in caps:
            entry = capabilities.setdefault(cap_id, {
                "tests": 0, "passed": 0, "failed": 0, "not_run": 0, "specs": [],
            })
            entry["tests"] += 1
            if passed:
                entry["passed"] += 1
            elif failed:
                entry["failed"] += 1
            else:
                entry["not_run"] += 1
            if spec_file not in entry["specs"]:
                entry["specs"].append(spec_file)

    device = (report.get("devices") or [{}])[0]
    run_id = args.run_id or ("%s-%s-%s" % (
        datetime.now(timezone.utc).strftime("%Y-%m-%d"),
        args.carrier.lower(), args.backend.lower()))

    head_commit = git("rev-parse", "HEAD")
    source = args.source or os.path.relpath(os.path.abspath(args.report), REPO_ROOT).replace("\\", "/")

    evidence = {
        "schema": SCHEMA,
        "run_id": run_id,
        "commit": args.commit or head_commit,
        "historical": bool(args.commit) and args.commit[:8] != head_commit[:8],
        "source": source,
        "worktree_dirty": bool(git("status", "--short")),
        "engine": args.engine,
        "platform": "Win64",
        "config": args.config,
        "carrier": args.carrier,
        "backend": args.backend,
        "machine": device.get("deviceName", "unknown"),
        "cpu": (device.get("cPUModel") or "unknown").strip(),
        "report_created_on": report.get("reportCreatedOn", ""),
        "report_totals": {
            "succeeded": report.get("succeeded", 0),
            "succeeded_with_warnings": report.get("succeededWithWarnings", 0),
            "failed": report.get("failed", 0),
            "not_run": report.get("notRun", 0),
        },
        "totals": totals,
        "capabilities": dict(sorted(capabilities.items())),
    }

    if not os.path.isdir(EVIDENCE_DIR):
        os.makedirs(EVIDENCE_DIR)

    target = os.path.join(EVIDENCE_DIR, "%s.json" % run_id)
    with io.open(target, "w", encoding="utf-8") as handle:
        handle.write(json.dumps(evidence, indent=2, ensure_ascii=False))

    index_file = os.path.join(EVIDENCE_DIR, "index.json")
    index = []
    if os.path.isfile(index_file):
        with io.open(index_file, "r", encoding="utf-8") as handle:
            try:
                index = json.load(handle)
            except Exception:
                index = []
    index = [e for e in index if e.get("run_id") != run_id]
    index.append({"run_id": run_id, "file": "%s.json" % run_id,
                  "backend": args.backend, "carrier": args.carrier,
                  "engine": args.engine, "commit": evidence["commit"],
                  "source": source, "historical": evidence["historical"]})
    with io.open(index_file, "w", encoding="utf-8") as handle:
        handle.write(json.dumps(index, indent=2, ensure_ascii=False))

    print("wrote %s" % target)
    print("  %d tests (%d passed, %d failed) mapped to %d of %d capabilities"
          % (totals["tests"], totals["passed"], totals["failed"],
             len(capabilities), len(matrix.get("features", []))))
    unmapped = totals["tests"] - mapped_tests
    if unmapped:
        print("  %d tests did not map to a capability (spec not listed in the matrix)"
              % unmapped)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

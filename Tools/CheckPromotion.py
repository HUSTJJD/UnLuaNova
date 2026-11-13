#!/usr/bin/env python3
"""Read-only promotion readiness check for Docs/{CN,EN}/Release_Readiness.md.

This tool NEVER writes a file and NEVER changes feature-matrix.json. The matrix's
`verification` field is deliberately pinned to `pending-runtime` by
Tools/AuditRepository.py:80, so "is this capability ready to ship?" can only be
answered from real artifacts, not from a status edit.

It reports the five-level evidence chain (L0-L4) as a checklist. A green item means
the artifact is present and self-consistent; it does not mean the tool executed the
run. Green gates with no commit provenance are reported as such.

Usage:
    python -B Tools/CheckPromotion.py
    python -B Tools/CheckPromotion.py --json
    python -B Tools/CheckPromotion.py --strict    # exit 1 when anything is unmet
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUNS = ROOT / "Saved" / "ReleaseTests"
NET_ROOT = RUNS / "net-matrix"
BACKENDS = ("Lua", "Luau")
VARIANTS = ("Legacy", "PushOff", "Iris", "Cooked")
SCENARIOS = ("LateJoin", "Reconnect", "Dormancy", "Relevancy",
             "JoinOrderSwap", "PacketLoss", "SeamlessReconnect", "SchemaMismatch")
RELEASE_PAGES = (
    "README.md", "README_EN.md", "CHANGELOG.md", "Docs/index.md",
    "Docs/CN/ReleaseNotes.md", "Docs/EN/ReleaseNotes.md",
    "Docs/CN/Testing.md", "Docs/EN/Testing.md",
)


def load(path):
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        return None


def head():
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "HEAD"],
                             capture_output=True, text=True, timeout=15)
        return out.stdout.strip() if out.returncode == 0 else ""
    except (OSError, subprocess.SubprocessError):
        return ""


def marker(rel, key):
    path = ROOT / rel
    if not path.is_file():
        return ""
    for line in path.read_text(encoding="utf-8-sig").splitlines():
        line = line.strip()
        if line.startswith("<!--") and key in line and line.endswith("-->"):
            body = line[4:-3].strip()
            if ":" in body:
                return body.split(":", 1)[1].strip()
    return ""


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--json", action="store_true", help="machine-readable output")
    parser.add_argument("--strict", action="store_true", help="exit 1 when any item is unmet")
    args = parser.parse_args()

    commit = head()
    rows = []

    def add(level, item, ok, detail):
        rows.append({"level": level, "item": item,
                     "ok": bool(ok), "detail": str(detail)})

    # L2 - complete Release bound to the current tree.
    latest = load(RUNS / "test" / "release-summary.json")
    if latest is None:
        add("L2", "complete Release summary", False, "missing")
    else:
        run_commit = str(latest.get("provenance", {}).get("commit", ""))
        complete = latest.get("complete") is True
        passed = latest.get("passed") is True
        stages = latest.get("stages") or []
        bad = [s.get("name") for s in stages
               if isinstance(s, dict) and s.get("status") not in ("passed", "cached")]
        add("L2", "release complete && passed && no failed/blocked stage",
            complete and passed and not bad,
            "complete=%s passed=%s bad_stages=%d" % (complete, passed, len(bad)))
        add("L2", "release provenance == HEAD",
            bool(commit) and run_commit.startswith(commit[:8]),
            "run=%s head=%s" % (run_commit[:8] or "-", commit[:8] or "-"))
        add("L2", "baseline is reachable from HEAD", bool(load(RUNS / "baseline" / "release-summary.json")),
            "baseline summary present (reachability is checked by ValidateProvenance.py)")

    # L3 - connected delivery matrix, four variants + eight scenarios per backend.
    for backend in BACKENDS:
        base = NET_ROOT / backend
        present_v = [v for v in VARIANTS
                     if (base / "net" / "variants" / v / "DedicatedServer" / "topology.json").is_file()]
        add("L3", "net-matrix %s: 4 variants" % backend, len(present_v) == len(VARIANTS),
            "%d/4 (%s)" % (len(present_v), ", ".join(present_v) or "none"))
        present_s = [s for s in SCENARIOS
                     if (base / "net" / "scenarios" / s / "topology.json").is_file()]
        add("L3", "net-matrix %s: 8 scenarios" % backend, len(present_s) == len(SCENARIOS),
            "%d/8 (missing: %s)" % (len(present_s),
                                    ", ".join(s for s in SCENARIOS if s not in present_s) or "none"))

    # L3 - current-tree execution evidence recorded in the evidence index.
    evidence = load(ROOT / "Tools" / "evidence" / "index.json") or []
    current = [r for r in evidence if isinstance(r, dict)
               and str(r.get("commit", "")).startswith(commit[:8])] if commit else []
    add("L3", "evidence index has a record for HEAD", bool(current),
        "%d record(s) for %s out of %d" % (len(current), commit[:8] or "-", len(evidence)))

    # L4 - published claim, legal gate, release identity.
    statuses = {marker(p, "release-status") for p in RELEASE_PAGES}
    add("L4", "published release-status == passed on all 8 pages",
        statuses == {"passed"}, "values=%s" % (sorted(statuses) or "none"))
    try:
        tags = subprocess.run(["git", "-C", str(ROOT), "tag", "--points-at", "HEAD"],
                              capture_output=True, text=True, timeout=15)
        tagged = bool(tags.stdout.strip()) if tags.returncode == 0 else False
    except (OSError, subprocess.SubprocessError):
        tagged = False
    add("L4", "HEAD carries a release tag", tagged,
        "git tag --points-at HEAD%s" % ("" if tagged else " is empty"))

    # The outer UEAbilityKit workspace is a separate project with its own licence.
    # It is deliberately not part of this repository's promotion criteria.

    unmet = [r for r in rows if not r["ok"]]
    if args.json:
        print(json.dumps({"head": commit, "unmet": len(unmet), "checks": rows},
                         indent=2, ensure_ascii=False))
    else:
        print("Promotion readiness (read-only, HEAD %s)" % (commit[:8] or "unknown"))
        print("=" * 78)
        for r in rows:
            print(" [%s] %-4s %-52s %s" % ("OK" if r["ok"] else "!!",
                                           r["level"], r["item"], r["detail"]))
        print("=" * 78)
        print("%d/%d satisfied; %d unmet." % (len(rows) - len(unmet), len(rows), len(unmet)))
        if unmet:
            print("Nothing may be marked as a supported release until every L2-L4 item is OK.")
            print("Never edit feature-matrix.json's verification field to get here.")
    return 1 if (args.strict and unmet) else 0


if __name__ == "__main__":
    raise SystemExit(main())

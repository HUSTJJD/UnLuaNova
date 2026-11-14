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
    python -B Tools/CheckPromotion.py --release-gate --strict \
        --run Saved/ReleaseTests/test --net-root Saved/ReleaseTests/test/net-matrix

--release-gate is what ReleasePipeline.ps1 calls. It only enforces the items a
Release run can satisfy by itself (L2/L3: a complete green run bound to HEAD,
connected-matrix evidence, current-tree execution evidence). The two L4 items -
a published `passed` claim and a release tag - are human release decisions, so
they stay visible in the report but do not fail the Release gate. Without that
split, adding this checker to the pipeline would make every run fail forever.
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
    parser.add_argument("--release-gate", action="store_true",
                        help="only enforce items a Release run can satisfy on its own")
    parser.add_argument("--run", default=str(RUNS / "test"),
                        help="Release run root holding release-summary.json")
    parser.add_argument("--net-root", default=str(NET_ROOT),
                        help="root holding per-backend net/variants and net/scenarios")
    args = parser.parse_args()

    commit = head()
    rows = []
    run_root = Path(args.run)
    net_root = Path(args.net_root)

    def add(level, item, ok, detail, gate=True):
        rows.append({"level": level, "item": item, "ok": bool(ok),
                     "detail": str(detail), "gate": bool(gate)})

    # L2 - complete Release bound to the current tree.
    latest = load(run_root / "release-summary.json")
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
        # A baseline Release summary is a comparison reference, not a correctness gate.
        # The only release entry point (RunTests.ps1) cannot create one: -Baseline is a
        # maintainer switch on ReleasePipeline.ps1. Making this blocking would mean the
        # documented single command can never publish. Current-tree provenance is still
        # enforced by ValidateProvenance.py --require-current in its own stage.
        add("L2", "baseline is reachable from HEAD",
            bool(load(RUNS / "baseline" / "release-summary.json")),
            "baseline summary present (informational; current-tree provenance is gated separately)",
            gate=False)

    # L3 - connected delivery matrix, four variants + eight scenarios per backend.
    for backend in BACKENDS:
        base = net_root / backend
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

    # L4 - published claim and release identity. These are human decisions made
    # AFTER a green Release (the pipeline writes the markers; a human cuts the
    # tag), so they are informational under --release-gate.
    statuses = {marker(p, "release-status") for p in RELEASE_PAGES}
    add("L4", "published release-status == passed on all 8 pages",
        statuses == {"passed"}, "values=%s" % (sorted(statuses) or "none"), gate=False)
    try:
        tags = subprocess.run(["git", "-C", str(ROOT), "tag", "--points-at", "HEAD"],
                              capture_output=True, text=True, timeout=15)
        tagged = bool(tags.stdout.strip()) if tags.returncode == 0 else False
    except (OSError, subprocess.SubprocessError):
        tagged = False
    add("L4", "HEAD carries a release tag", tagged,
        "git tag --points-at HEAD%s" % ("" if tagged else " is empty"), gate=False)

    # The outer UEAbilityKit workspace is a separate project with its own licence.
    # It is deliberately not part of this repository's promotion criteria.

    gated = [r for r in rows if r["gate"]] if args.release_gate else rows
    unmet = [r for r in gated if not r["ok"]]
    informational = [r for r in rows if not r["gate"] and not r["ok"]]
    if args.json:
        print(json.dumps({"head": commit, "unmet": len(unmet),
                          "release_gate": bool(args.release_gate),
                          "informational": [r["item"] for r in informational],
                          "checks": rows}, indent=2, ensure_ascii=False))
    else:
        print("Promotion readiness (read-only, HEAD %s, release-gate=%s)"
              % (commit[:8] or "unknown", bool(args.release_gate)))
        print("=" * 78)
        for r in rows:
            tag = "INFO" if not r["gate"] else ("OK" if r["ok"] else "!!")
            print(" [%s] %-4s %-52s %s" % (tag, r["level"], r["item"], r["detail"]))
        print("=" * 78)
        print("%d/%d gate items satisfied; %d unmet." % (len(gated) - len(unmet), len(gated), len(unmet)))
        if informational and not args.release_gate:
            print("%d release-decision item(s) still open (marker / tag)."
                  % len(informational))
        if unmet:
            print("Nothing may be marked as a supported release until every L2-L4 item is OK.")
            print("Never edit feature-matrix.json's verification field to get here.")
    return 1 if (args.strict and unmet) else 0


if __name__ == "__main__":
    raise SystemExit(main())

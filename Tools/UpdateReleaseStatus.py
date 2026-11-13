#!/usr/bin/env python3
"""Single writer for the published release claims.

The eight published pages used to carry hand-maintained `release-status` /
`release-provenance` comments. They drifted from the actual runs (see the
2026-11-13 repository audit): a "passed" marker survived a failed run, and the
provenance comment named a commit that is no longer reachable from any ref.
This tool regenerates both markers from the Release summaries so the published
claim can never disagree with the run that produced it.

Rules, kept in sync with Tools/AuditRepository.py:validate_release_docs:

  * status     = "passed" iff the latest (`test`) run is complete AND passed;
                 otherwise "not-passed". A partial run is never a release verdict.
  * provenance = the commit of the `baseline` run (the comparison baseline),
                 falling back to the latest run when no baseline exists. It names
                 the run the published claim refers to, NOT the current HEAD.
                 "Does the current tree have execution evidence?" is a different
                 question, answered by Tools/ValidateProvenance.py --require-current.

Called by Tools/ReleasePipeline.ps1 right after Write-ReleaseSummary. It only
rewrites the two HTML comments; all other page content is untouched. A page that
lost a marker is an error, never silently skipped.

Usage:
    python -B Tools/UpdateReleaseStatus.py            # rewrite the markers
    python -B Tools/UpdateReleaseStatus.py --check    # exit 1 if anything drifted
"""

import argparse
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RUNS = ROOT / "Saved" / "ReleaseTests"

# Must stay identical to Tools/AuditRepository.py:RELEASE_PAGES.
RELEASE_PAGES = (
    Path("README.md"),
    Path("README_EN.md"),
    Path("CHANGELOG.md"),
    Path("Docs/index.md"),
    Path("Docs/CN/ReleaseNotes.md"),
    Path("Docs/EN/ReleaseNotes.md"),
    Path("Docs/CN/Testing.md"),
    Path("Docs/EN/Testing.md"),
)

STATUS_MARK = re.compile(r"<!--\s*release-status:\s*(?:passed|not-passed)\s*-->")
PROVENANCE_MARK = re.compile(r"<!--\s*release-provenance:\s*([0-9a-f]{7,40})\s*-->")


def read_summary(kind):
    path = RUNS / kind / "release-summary.json"
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as exc:
        raise SystemExit("ERROR: unreadable %s: %s" % (path, exc))


def read_page_bytes(path):
    raw = path.read_bytes()
    has_bom = raw.startswith(b"\xef\xbb\xbf")
    return raw.decode("utf-8-sig"), has_bom


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="do not write; exit 1 if any marker is stale")
    args = parser.parse_args()

    latest = read_summary("test")
    if latest is None:
        print("ERROR: no Saved/ReleaseTests/test/release-summary.json; "
              "run .\\Tools\\RunTests.ps1 before updating release claims.",
              file=sys.stderr)
        return 1

    complete = latest.get("complete") is True
    passed = latest.get("passed") is True
    status = "passed" if (complete and passed) else "not-passed"

    baseline = read_summary("baseline")
    commit = ""
    for source in (baseline, latest):
        if isinstance(source, dict):
            value = str(source.get("provenance", {}).get("commit", ""))
            if re.fullmatch(r"[0-9a-f]{7,40}", value):
                commit = value[:8]
                break
    if not commit:
        print("ERROR: no valid provenance commit in the Release summaries.",
              file=sys.stderr)
        return 1

    print("release-status=%s release-provenance=%s (from %s / baseline)"
          % (status, commit, "test"))

    drifted = []
    for page in RELEASE_PAGES:
        path = ROOT / page
        if not path.is_file():
            print("ERROR: release status page is missing: %s" % page, file=sys.stderr)
            return 1
        text, has_bom = read_page_bytes(path)
        n_status = len(STATUS_MARK.findall(text))
        n_prov = len(PROVENANCE_MARK.findall(text))
        if n_status != 1 or n_prov != 1:
            print("ERROR: %s has %d status / %d provenance markers; expected 1 each"
                  % (page, n_status, n_prov), file=sys.stderr)
            return 1

        updated = STATUS_MARK.sub("<!-- release-status: %s -->" % status, text, count=1)
        updated = PROVENANCE_MARK.sub(
            "<!-- release-provenance: %s -->" % commit, updated, count=1)
        if updated == text:
            print("  unchanged: %s" % page.as_posix())
            continue
        drifted.append(page)
        if args.check:
            print("  DRIFT: %s" % page.as_posix())
            continue
        payload = ("\ufeff" if has_bom else "") + updated
        path.write_bytes(payload.encode("utf-8"))
        print("  updated:   %s" % page.as_posix())

    if args.check:
        if drifted:
            print("FAIL: %d page(s) drifted; run python -B Tools/UpdateReleaseStatus.py"
                  % len(drifted), file=sys.stderr)
            return 1
        print("release claims are in sync")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

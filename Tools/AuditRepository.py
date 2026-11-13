#!/usr/bin/env python3
"""Check source evidence and local documentation without building or running UE."""

import argparse
import json
import re
import subprocess
from datetime import date
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
MATRIX = Path("Tools/feature-matrix.json")
PAGE = Path("Docs/feature-matrix.md")
EVIDENCE_DIR = Path("Tools/evidence")
STATUSES = {"implemented", "partial", "experimental", "planned"}
LINK = re.compile(r"\[[^\]\n]*\]\(\s*(<[^>]+>|[^\s)]+)(?:\s+[^)]*)?\)")
HTML_LINK = re.compile(
    r"<(?:a|img)\b[^>]*?\b(?:href|src)\s*=\s*[\"']([^\"']+)[\"'][^>]*>", re.IGNORECASE)
HEADING = re.compile(r"^\s{0,3}#{1,6}\s+(.+?)\s*#*\s*$", re.M)
FENCE = re.compile(r"^\s*(`{3,}|~{3,})")


def local_path(root, base, value):
    """Return a confined repository path, rejecting absolute and escaping paths."""
    relative = Path(value)
    if relative.is_absolute() or re.match(r"^[A-Za-z]:", value):
        raise ValueError("absolute path")
    target = (base / relative).resolve()
    # Path.is_relative_to() is 3.9+, and this script advertises "standard library only",
    # which has to include the 3.7/3.8 interpreters still common on build machines. Same
    # containment question, spelled with .parents.
    resolved_root = root.resolve()
    if target != resolved_root and resolved_root not in target.parents:
        raise ValueError("path escapes repository")
    return target


def validate_matrix(data, root):
    errors = []
    if not isinstance(data, dict) or data.get("schema_version") != 1:
        return ["matrix: unsupported schema"]
    try:
        date.fromisoformat(data.get("audit_date", ""))
    except (ValueError, TypeError):
        errors.append("matrix: invalid audit_date")
    if not isinstance(data.get("scope"), str) or not data["scope"].strip():
        errors.append("matrix: missing scope")
    features = data.get("features")
    upstreams = data.get("upstreams")
    if not isinstance(features, list) or not features:
        return ["matrix: features must be a nonempty list"]
    if not isinstance(upstreams, dict) or not upstreams:
        return ["matrix: missing upstream pins"]
    for name, pin in upstreams.items():
        if not isinstance(pin, dict):
            errors.append(f"matrix: invalid pin for {name}")
            continue
        revision = pin.get("revision")
        if not isinstance(revision, str) or not re.fullmatch(r"[0-9a-f]{40}", revision):
            errors.append(f"matrix: invalid revision for {name}")
        if not isinstance(pin.get("url"), str) or not pin["url"].startswith("https://"):
            errors.append(f"matrix: invalid URL for {name}")
    seen = set()
    for row in features:
        if not isinstance(row, dict):
            errors.append("matrix: feature must be an object")
            continue
        key = row.get("id")
        if not isinstance(key, str) or not re.fullmatch(r"[A-Z][A-Z0-9-]+", key):
            errors.append("matrix: invalid feature id")
            continue
        if key in seen:
            errors.append(f"{key}: duplicate id")
        seen.add(key)
        if not isinstance(row.get("status"), str) or row["status"] not in STATUSES:
            errors.append(f"{key}: invalid status")
        if not isinstance(row.get("priority"), str) or row["priority"] not in {"P0", "P1", "P2"}:
            errors.append(f"{key}: invalid priority")
        if row.get("verification") != "pending-runtime":
            errors.append(f"{key}: execution claims require an evidence schema, not a status edit")
        for field in ("area", "title", "gap", "acceptance"):
            if not isinstance(row.get(field), str) or not row[field].strip():
                errors.append(f"{key}: missing {field}")
        refs = row.get("upstream")
        if not isinstance(refs, list) or not refs or any(not isinstance(ref, str) or ref not in upstreams for ref in refs):
            errors.append(f"{key}: invalid upstream references")
        for field in ("source", "tests", "examples"):
            paths = row.get(field)
            if not isinstance(paths, list):
                errors.append(f"{key}: {field} must be a list")
                continue
            if field in {"source", "tests"} and row.get("status") == "implemented" and not paths:
                errors.append(f"{key}: implemented needs {field} evidence")
            for value in paths:
                try:
                    if not isinstance(value, str):
                        raise ValueError("path must be a string")
                    target = local_path(root, root, value)
                    if not target.is_file():
                        raise ValueError("file does not exist")
                except (ValueError, OSError) as exc:
                    errors.append(f"{key}: {field} {value!r}: {exc}")
    return errors


def all_links(text):
    """Yield (href, kind) for inline Markdown and HTML links outside code fences.

    Both syntaxes appear in published pages: the README uses <img src="..."> while the
    docs use [text](target). A broken link in either one breaks the same published site.
    """
    fence = None
    for line in text.splitlines():
        match = FENCE.match(line)
        if match:
            marker = match.group(1)
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
            continue
        if fence is None:
            for match in LINK.finditer(line):
                yield match.group(1).strip("<>"), "markdown"
            for match in HTML_LINK.finditer(line):
                yield match.group(1), "html"


def markdown_links(text):
    for href, kind in all_links(text):
        if kind == "markdown":
            yield href


def slugify(text):
    """MkDocs/Python-Markdown's default toc slug: lowercase, drop punctuation, spaces -> '-'."""
    value = text.strip().lower()
    value = re.sub(r"[^\w\s-]", "", value, flags=re.UNICODE)
    return re.sub(r"\s+", "-", value).strip("-")


def heading_slugs(text):
    return set(slugify(match.group(1)) for match in HEADING.finditer(text))


def validate_docs(root):
    errors = []
    docs = (root / "Docs").resolve()
    pages = sorted((root / "Docs").rglob("*.md")) + sorted(root.glob("*.md"))
    slug_cache = {}

    def slugs_for(path):
        key = path.resolve()
        if key not in slug_cache:
            try:
                slug_cache[key] = heading_slugs(path.read_text(encoding="utf-8-sig"))
            except (OSError, UnicodeError):
                slug_cache[key] = set()
        return slug_cache[key]

    for page in pages:
        # Pages under Docs/ are the ones MkDocs publishes; the ones at the repository root are
        # rendered by GitHub, where a link into the source tree is perfectly fine.
        published = docs in page.resolve().parents
        for href, kind in all_links(page.read_text(encoding="utf-8-sig")):
            url = urlsplit(href)
            if url.scheme or url.netloc:
                # Remote links: syntax and transport are static properties, reachability is
                # not. Never fetch anything - CI has no business making network calls here.
                if url.scheme != "https" or not url.netloc or re.search(r"\s", href):
                    errors.append(f"{page.relative_to(root)}: {href}: remote link must be a plain https URL")
                continue
            if not url.path and not url.fragment:
                continue
            try:
                if url.path:
                    target = local_path(root, page.parent, unquote(url.path))
                    if not target.exists():
                        raise ValueError("target does not exist")
                    # A published page can only link to a file MkDocs publishes. `mkdocs build
                    # --strict` fails a link that leaves docs_dir even when the file exists,
                    # because it is never copied into the site - which is a warning on CI and an
                    # error there, while a plain "does the file exist" check says nothing.
                    if published and target != docs and docs not in target.parents:
                        raise ValueError("link leaves Docs/, which mkdocs --strict rejects")
                else:
                    target = page.resolve()
                if url.fragment and unquote(url.fragment).lower() not in slugs_for(target):
                    raise ValueError(f"anchor #{url.fragment} has no matching heading")
            except (ValueError, OSError) as exc:
                errors.append(f"{page.relative_to(root)}: {href}: {exc}")
    nav = root / "mkdocs.yml"
    if nav.exists():
        # The repository's nav uses one plain .md path per line; no YAML execution.
        for name in re.findall(r"(?:^|\s)([A-Za-z0-9_./-]+\.md)\s*$", nav.read_text(encoding="utf-8"), re.M):
            try:
                if not local_path(root, root / "Docs", name).is_file():
                    raise ValueError("missing nav page")
            except ValueError as exc:
                errors.append(f"mkdocs.yml: {name}: {exc}")
    return errors


EVIDENCE_SCHEMA = "unluanova-evidence/1"


def load_evidence(root):
    """Read Tools/evidence/. Returns (by_capability, runs, problems).

    Execution results live here, never in feature-matrix.json: the matrix's `verification`
    field stays `pending-runtime` on purpose, so the only way to say "this ran" is to
    produce a record this function can read.
    """
    by_capability = {}
    runs = []
    problems = []

    index_file = root / EVIDENCE_DIR / "index.json"
    if not index_file.is_file():
        return by_capability, runs, problems

    try:
        index = json.loads(index_file.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as exc:
        return by_capability, runs, [f"evidence index: {exc}"]

    for entry in index:
        name = entry.get("file") or ""
        path = root / EVIDENCE_DIR / name
        if not path.is_file():
            problems.append(f"evidence: {name}: missing file")
            continue
        try:
            record = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as exc:
            problems.append(f"evidence: {name}: {exc}")
            continue

        if record.get("schema") != EVIDENCE_SCHEMA:
            problems.append(f"evidence: {name}: unknown schema {record.get('schema')!r}")
            continue
        if record.get("run_id") != entry.get("run_id"):
            problems.append(f"evidence: {name}: run_id does not match the index")
            continue
        for field in ("commit", "backend", "carrier", "engine", "machine"):
            if not record.get(field):
                problems.append(f"evidence: {name}: missing {field}")

        runs.append(record)
        label = "%s/%s" % (record.get("backend"), record.get("carrier"))
        for cap_id, stats in (record.get("capabilities") or {}).items():
            by_capability.setdefault(cap_id, []).append(
                (record.get("run_id"), label, stats.get("passed", 0),
                 stats.get("failed", 0), stats.get("tests", 0)))

    return by_capability, runs, problems


def evidence_recency(root):
    """Return (total, historical, not_marked_historical) for Tools/evidence/index.json.

    The rendered matrix keeps historical runs visible because they are real past
    executions. A release decision must not read them as proof of the current tree, so
    report the split explicitly. A missing `historical` flag does NOT prove a run matches
    HEAD - Tools/ValidateProvenance.py --require-current does that comparison.
    """
    index_file = root / EVIDENCE_DIR / "index.json"
    if not index_file.is_file():
        return 0, 0, 0
    try:
        index = json.loads(index_file.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        return 0, 0, 0
    if not isinstance(index, list):
        return 0, 0, 0
    historical = len([e for e in index if isinstance(e, dict) and e.get("historical") is True])
    return len(index), historical, len(index) - historical


def render_matrix(data, evidence=None):
    by_capability, runs, _problems = evidence or ({}, [], [])

    if runs:
        covered = len([r for r in data["features"] if r["id"] in by_capability])
        intro = ("Execution evidence is recorded per capability under `Tools/evidence/` "
                 f"({len(runs)} run(s), covering {covered} of {len(data['features'])} capabilities). "
                 "Those records include past runs: entries marked `historical` are real "
                 "executions of an earlier tree and are never proof of the current one. "
                 "`feature-matrix.json` itself never claims execution: its `verification` field "
                 "stays `pending-runtime`, and a status edit there is rejected by this audit. "
                 "Per-capability `Boundary` / `Acceptance` lines are the source-level gap "
                 "snapshot from that audit: they describe what was missing at audit time, "
                 "not the outcome of a later Release. Current execution status lives in "
                 "`Saved/ReleaseTests/test/release-summary.json` and the per-capability "
                 "evidence lines above.")
    else:
        intro = ("All runtime verification is pending: no records under `Tools/evidence/`. "
                 "Record a run with `python Tools/RecordEvidence.py`.")

    lines = ["# Capability and Evidence Matrix", "",
             "Generated from `Tools/feature-matrix.json`; edit that file and run "
             "`python Tools/AuditRepository.py --write-matrix`.", "",
             "Audit: " + data["audit_date"] + ". " + data["scope"], "",
             intro, "",
             "`implemented` means source exists within the stated boundary; it does not mean "
             "tested, complete parity or platform acceptance. "
             "`partial` has known gaps; `experimental` is not a release guarantee; `planned` is not delivered.", "",
             "## Upstream Pins", ""]
    for name, pin in data["upstreams"].items():
        lines.append(f"- [{name}]({pin['url']}/tree/{pin['revision']}): `{pin['revision']}`")
        if "integrated_revision" in pin:
            lines.append(f"  Integrated revision: `{pin['integrated_revision']}`.")
    area = None
    for row in data["features"]:
        if row["area"] != area:
            area = row["area"]
            lines.extend(["", f"## {area}"])
        lines.extend(["", f"### {row['id']}: {row['title']}", "",
                      f"Status: **{row['status']}**; priority: {row['priority']}; "
                      f"references: {', '.join(row['upstream'])}.", ""])
        evidence_rows = by_capability.get(row["id"], [])
        if evidence_rows:
            shown = "; ".join(
                f"`{run_id}` {label}: {passed}/{total} passed, {failed} failed"
                for run_id, label, passed, failed, total in evidence_rows)
            lines.append(f"Evidence: {shown}.")
        else:
            lines.append("Evidence: no execution recorded yet.")
        lines.append("")
        for field in ("source", "tests", "examples"):
            refs = [f"[{Path(path).name}](https://github.com/HUSTJJD/UnLuaNova/blob/master/{path})" for path in row[field]]
            lines.append(f"- {field.title()}: " + (", ".join(refs) if refs else "Missing dedicated evidence."))
        lines.extend(["", "Boundary: " + row["gap"], "", "Acceptance: " + row["acceptance"]])
    return "\n".join(lines) + "\n"


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
STATUS_MARK = re.compile(r"<!--\s*release-status:\s*(passed|not-passed)\s*-->")
PROVENANCE_MARK = re.compile(r"<!--\s*release-provenance:\s*([0-9a-f]{7,40})\s*-->")


def load_release_summary(root, kind):
    """Return a local release summary, or None when this machine has no such run."""
    path = root / "Saved" / "ReleaseTests" / kind / "release-summary.json"
    if not path.is_file():
        return None
    try:
        return json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError):
        return {"error": f"{kind}: unreadable release summary"}


def validate_release_docs(root):
    """Published release claims must name one run and agree across languages and evidence."""
    errors = []
    texts = {}
    for page in RELEASE_PAGES:
        path = root / page
        if not path.is_file():
            errors.append(f"{page}: release status page is missing")
            continue
        texts[page] = path.read_text(encoding="utf-8-sig")
    statuses = {}
    commits = {}
    for page, text in texts.items():
        status = STATUS_MARK.findall(text)
        commit = PROVENANCE_MARK.findall(text)
        if len(status) != 1:
            errors.append(f"{page}: expected exactly one release-status marker")
        else:
            statuses[page] = status[0]
        if len(commit) != 1:
            errors.append(f"{page}: expected exactly one release-provenance marker")
        else:
            commits[page] = commit[0]
    if len(set(statuses.values())) > 1:
        errors.append("release status markers disagree between published pages")
    if len(set(value[:8] for value in commits.values())) > 1:
        errors.append("release provenance markers disagree between published pages")

    baseline = load_release_summary(root, "baseline")
    if baseline and "error" not in baseline:
        commit = str(baseline.get("provenance", {}).get("commit", ""))[:8]
        stages = baseline.get("stages")
        automation = baseline.get("automation")
        if commit and commits and any(value[:8] != commit for value in commits.values()):
            errors.append(f"release provenance markers do not match baseline commit {commit}")
        if isinstance(stages, list) and isinstance(automation, list):
            total = sum(int(item.get("total", 0)) for item in automation if isinstance(item, dict))
            for page, text in texts.items():
                if "33/33" in text and len(stages) != 33:
                    errors.append(f"{page}: 33/33 claim disagrees with {len(stages)} baseline stages")
                if "2482/2482" in text and total != 2482:
                    errors.append(f"{page}: 2482/2482 claim disagrees with {total} baseline executions")

    latest = load_release_summary(root, "test")
    if latest and "error" not in latest and latest.get("complete") is True:
        expected = "passed" if latest.get("passed") is True else "not-passed"
        for page, status in statuses.items():
            if status != expected:
                errors.append(f"{page}: release-status {status} disagrees with the latest run ({expected})")
    return errors


def release_provenance_warnings(root):
    """Warn (never fail) when the published provenance cannot be reproduced.

    A commit that no ref reaches is still a valid Git object on the maintainer's
    machine, but nobody else can check it out - the release claim then points at
    nothing. Hard enforcement belongs to Tools/ValidateProvenance.py
    --require-current, which needs local run evidence that a documentation-only
    GitHub workflow cannot provide. This check only makes an orphaned claim
    visible in the audit output.
    """
    page = root / "README.md"
    if not page.is_file():
        return []
    match = PROVENANCE_MARK.search(page.read_text(encoding="utf-8-sig"))
    if not match:
        return []
    commit = match.group(1)

    def git(*args):
        try:
            return subprocess.run(["git", "-C", str(root), *args],
                                  stdout=subprocess.DEVNULL,
                                  stderr=subprocess.DEVNULL).returncode
        except OSError:
            return 1

    if git("cat-file", "-e", commit + "^{commit}") != 0:
        return ["WARNING: release-provenance %s is not a commit in this repository; "
                "the published release claim cannot be reproduced." % commit]
    if git("merge-base", "--is-ancestor", commit, "HEAD") != 0:
        return ["WARNING: release-provenance %s is not reachable from HEAD (no branch "
                "or tag contains it). Re-run .\\Tools\\ReleasePipeline.ps1 -Baseline "
                "to re-establish a reproducible baseline." % commit]
    return []


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-matrix", action="store_true", help="refresh the generated documentation page")
    args = parser.parse_args()
    try:
        data = json.loads((ROOT / MATRIX).read_text(encoding="utf-8"))
        errors = validate_matrix(data, ROOT)
    except (OSError, ValueError) as exc:
        print(f"matrix: {exc}")
        return 1
    evidence = load_evidence(ROOT)
    errors.extend(evidence[2])

    if not errors:
        expected = render_matrix(data, evidence)
        if args.write_matrix:
            # Path.write_text(newline=) is 3.10+; keep this script usable on older
            # interpreters, the same reason is_relative_to() is spelled out above.
            with open(str(ROOT / PAGE), "w", encoding="utf-8", newline="\n") as handle:
                handle.write(expected)
        # The capability matrix is an internal audit artifact and is not published with the
        # release docs. Only validate it when a maintainer has generated a local copy.
        elif (ROOT / PAGE).is_file() and (ROOT / PAGE).read_text(encoding="utf-8") != expected:
            errors.append("generated matrix is stale; run with --write-matrix")
    errors.extend(validate_docs(ROOT))
    errors.extend(validate_release_docs(ROOT))
    for warning in release_provenance_warnings(ROOT):
        print(warning)
    for error in errors:
        print(error)
    if errors:
        return 1

    by_capability, runs, _ = evidence
    covered = len([r for r in data["features"] if r["id"] in by_capability])
    if runs:
        print(f"Repository audit passed: {len(data['features'])} capability groups; "
              f"{covered} have execution evidence from {len(runs)} recorded run(s).")
    else:
        print(f"Repository audit passed: {len(data['features'])} capability groups; "
              f"runtime verification pending (no evidence recorded).")
    total, historical, other = evidence_recency(ROOT)
    if total:
        print(f"Evidence recency: {historical} of {total} recorded run(s) are marked historical; "
              f"{other} are not, but a non-historical label alone does not prove the current HEAD. "
              "Run Tools/ValidateProvenance.py --require-current before a release decision.")
    print("Checked evidence paths, generated matrix, inline and HTML local links, Markdown "
          "anchors, remote https URL syntax and plain MkDocs nav paths.")
    print("Checked release status/provenance markers and local release summaries.")
    print("Not checked: runtime behavior, real code coverage, remote URL reachability or local "
          "release evidence when Saved/ReleaseTests/ is absent (as on CI).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

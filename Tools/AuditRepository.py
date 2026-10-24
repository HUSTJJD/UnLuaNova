#!/usr/bin/env python3
"""Check source evidence and local documentation without building or running UE."""

import argparse
import json
import re
from datetime import date
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parent.parent
MATRIX = Path("Tools/feature-matrix.json")
PAGE = Path("Docs/feature-matrix.md")
STATUSES = {"implemented", "partial", "experimental", "planned"}
LINK = re.compile(r"\[[^\]\n]*\]\(\s*(<[^>]+>|[^\s)]+)(?:\s+[^)]*)?\)")
FENCE = re.compile(r"^\s*(`{3,}|~{3,})")


def local_path(root, base, value):
    """Return a confined repository path, rejecting absolute and escaping paths."""
    relative = Path(value)
    if relative.is_absolute() or re.match(r"^[A-Za-z]:", value):
        raise ValueError("absolute path")
    target = (base / relative).resolve()
    if not target.is_relative_to(root.resolve()):
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


def markdown_links(text):
    # Only inline file/image links are checked. Anchors and remote URLs are not verified.
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
                yield match.group(1).strip("<>")


def validate_docs(root):
    errors = []
    pages = sorted((root / "Docs").rglob("*.md")) + sorted(root.glob("*.md"))
    for page in pages:
        for href in markdown_links(page.read_text(encoding="utf-8-sig")):
            url = urlsplit(href)
            if url.scheme or url.netloc or not url.path:
                continue
            try:
                target = local_path(root, page.parent, unquote(url.path))
                if not target.exists():
                    raise ValueError("target does not exist")
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


def render_matrix(data):
    lines = ["# Capability and Evidence Matrix", "",
             "Generated from `Tools/feature-matrix.json`; edit that file and run "
             "`python Tools/AuditRepository.py --write-matrix`.", "",
             "Audit: " + data["audit_date"] + ". " + data["scope"], "",
             "All runtime verification is pending. `implemented` means source exists within "
             "the stated boundary; it does not mean tested, complete parity or platform acceptance. "
             "`partial` has known gaps; `experimental` is not a release guarantee; `planned` is not delivered.", "",
             "[Chinese audit](CN/Benchmark_Review.md) | [English audit](EN/Benchmark_Review.md)", "",
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
                      f"references: {', '.join(row['upstream'])}; execution: pending.", ""])
        for field in ("source", "tests", "examples"):
            refs = [f"[{Path(path).name}](https://github.com/HUSTJJD/UnLuaNova/blob/master/{path})" for path in row[field]]
            lines.append(f"- {field.title()}: " + (", ".join(refs) if refs else "Missing dedicated evidence."))
        lines.extend(["", "Boundary: " + row["gap"], "", "Acceptance: " + row["acceptance"]])
    return "\n".join(lines) + "\n"


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
    if not errors:
        expected = render_matrix(data)
        if args.write_matrix:
            (ROOT / PAGE).write_text(expected, encoding="utf-8", newline="\n")
        elif not (ROOT / PAGE).is_file() or (ROOT / PAGE).read_text(encoding="utf-8") != expected:
            errors.append("generated matrix is stale; run with --write-matrix")
    errors.extend(validate_docs(ROOT))
    for error in errors:
        print(error)
    if errors:
        return 1
    print(f"Repository audit passed: {len(data['features'])} capability groups; runtime verification pending.")
    print("Checked evidence paths, generated matrix, inline local Markdown links and plain MkDocs nav paths.")
    print("Not checked: runtime behavior, code coverage, remote URLs, Markdown anchors or HTML links.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

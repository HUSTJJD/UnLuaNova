#!/usr/bin/env python3
"""Generate a reproducible NOTICES.txt from license facts already in the repository.

This tool NEVER invents a license text, a copyright owner, or a rights-holder
decision. It only does two things:

  1. mechanically aggregate license / provenance facts that *already exist* in
     this repository into one deterministic, name-sorted NOTICES.txt, and
  2. offer --check to compare shipped artifacts against that generated list.

Anything this repository cannot prove from itself is emitted as an explicit
``UNRESOLVED:`` line and makes the gate fail closed. The list always carries
this declaration (also printed at the top of every generated file):

    本清单由仓库已有文件机械汇总，不构成法务结论；UNRESOLVED 项需权利人决定

The presence of a "resolver" evidence file only ever produces an ``ADDRESSED``
note; this tool does not inspect that file's contents and never asserts that a
legal question is settled. This is NOT a release compliance bundle.

Usage:
    python Tools/GenerateNotices.py
    python Tools/GenerateNotices.py --out Saved/StagedBuilds/Windows/NOTICES.txt
    python Tools/GenerateNotices.py --check Saved/StagedBuilds/Windows/NOTICES.txt Saved/StagedBuilds/WindowsServer/NOTICES.txt
    python Tools/GenerateNotices.py --check Saved/StagedBuilds/Windows/NOTICES.txt --check Saved/StagedBuilds/WindowsServer/NOTICES.txt
    python Tools/GenerateNotices.py --allow-unresolved --report notices-report.json
    python Tools/GenerateNotices.py --root /tmp/fixture --check NOTICES.txt

--check takes one or more paths after a single flag (nargs="+") and the flag
may also be repeated; the two --check lines above are equivalent and both run.

Python 3.7+ standard library only. No UE build, no UE automation.
"""

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

REQUIRED_FILES = ("LICENSE", "LICENSE-THIRD-PARTY.md", ".gitmodules")

LICENSE_IDS = (
    "MIT", "BSD-3-Clause", "BSD-2-Clause", "Apache-2.0", "GPL-3.0",
    "LGPL-2.1", "MPL-2.0", "ISC", "Zlib",
)
VERSION_RE = re.compile(r"(?<![A-Za-z0-9])(\d[\w.\-]*|scm)(?![A-Za-z0-9])")
SUBMODULE_RE = re.compile(r'^\[submodule\s+"(?P<name>.+)"\]\s*$')
BACKTICK_RE = re.compile(r"`([^`]+)`")
LINK_RE = re.compile(r"\[([^\]]+)\]\([^)]*\)")

NOTICE_PREFIXES = ("license", "licence", "notice", "copying")

# Cross-cutting provenance questions this repository documents as open (see the
# cited basis lines). They are ALWAYS listed: as UNRESOLVED when no evidence
# file exists, and as ADDRESSED (content unverified) when one does. This is a
# fail-closed gate contract, not a statement about any license or right.
DECLARED_GAPS = (
    {
        "key": "funlua-license-file",
        "question": "FUnLua 的 license 文件与授权/来源确认",
        "basis": "LICENSE-THIRD-PARTY.md:48; Tools/feature-matrix.json LEGAL",
        "resolver": "Docs/legal/funlua-license.txt",
    },
    {
        "key": "sample-assets-redistribution",
        "question": "示例资产/图片的来源与再分发授权清单",
        "basis": "LICENSE-THIRD-PARTY.md:43; Tools/feature-matrix.json LEGAL",
        "resolver": "Docs/legal/sample-assets-manifest.txt",
    },
    {
        "key": "exact-upstream-revisions",
        "question": "被复制扩展的精确上游 URL/revision 与完整随包 notice 文本",
        "basis": "LICENSE-THIRD-PARTY.md:35; Tools/feature-matrix.json LEGAL",
        "resolver": "Docs/legal/upstream-revisions.txt",
    },
)

DECLARATION = "本清单由仓库已有文件机械汇总，不构成法务结论；UNRESOLVED 项需权利人决定"


def confine(root, value):
    """Resolve ``value`` under ``root`` and reject absolute/escaping paths.

    Path.is_relative_to() is 3.9+, so containment is spelled with .parents
    (same technique as Tools/AuditRepository.py). An absolute path already
    under root is accepted; one that leaves root is not.
    """
    if not isinstance(value, str):
        value = str(value)
    target = (root / Path(value)).resolve()
    resolved_root = root.resolve()
    if target != resolved_root and resolved_root not in target.parents:
        raise ValueError("path escapes repository root (路径逃逸被拒绝): %s" % value)
    return target


def resolve_output(root, value):
    """Resolve an output destination: relative to root, absolute allowed anywhere.

    Unlike input reads (see confine()), an explicitly chosen --out/--report
    destination is not confined to the root; it never feeds back into the
    generated manifest, so it cannot affect determinism or leak a read.
    """
    return (root / Path(value)).resolve()


def read_source(path):
    """Read a UTF-8 (BOM-tolerant) text file with universal newline handling."""
    with open(str(path), "r", encoding="utf-8-sig") as handle:
        return handle.read()


def clean_md(value):
    """Strip trivial Markdown decoration; keep the visible text verbatim."""
    value = LINK_RE.sub(r"\1", value)
    value = value.replace("`", "")
    value = re.sub(r"\s+", " ", value).strip()
    return value


def find_version(value):
    match = VERSION_RE.search(value)
    return match.group(1) if match else "未提供"


def find_license(value):
    found = set()
    for ident in LICENSE_IDS:
        if re.search(r"(?<![A-Za-z0-9])" + re.escape(ident) + r"(?![A-Za-z0-9])",
                     value, re.IGNORECASE):
            found.add(ident)
    return "+".join(sorted(found)) if found else "未提供"


def extract_location(cell):
    """Return the first backtick path that looks like a repo-relative location."""
    for candidate in BACKTICK_RE.findall(cell):
        candidate = candidate.strip()
        if not candidate:
            continue
        if "/" in candidate or candidate.lower().endswith(
                (".lua", ".h", ".hpp", ".cpp", ".c", ".txt", ".md")):
            return candidate
    return None


def parse_gitmodules(text):
    items = []
    current = None
    for lineno, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        match = SUBMODULE_RE.match(line)
        if match:
            current = {"name": match.group("name"), "path": "", "url": "",
                       "line": lineno}
            items.append(current)
            continue
        if current is None or not line or line.startswith("#"):
            continue
        if "=" in line:
            key, value = line.split("=", 1)
            key = key.strip()
            if key in ("path", "url"):
                current[key] = value.strip()
    return items


def parse_third_party(text):
    """Return (components, failures, section_seen).

    Rows are read from any pipe-table inside the "Included Runtime Components"
    section regardless of column count/format. A row we cannot extract a name
    from is a parse failure (UNRESOLVED), never a silent skip.
    """
    lines = text.splitlines()
    start = None
    end = len(lines)
    for index, raw in enumerate(lines):
        stripped = raw.strip().lower()
        if stripped.startswith("## "):
            if "included runtime components" in stripped:
                start = index + 1
            elif start is not None:
                end = index
                break
    section_seen = start is not None
    components = []
    failures = []
    if not section_seen:
        return components, failures, section_seen

    for offset in range(start, end):
        lineno = offset + 1
        row = lines[offset].strip()
        if not (len(row) > 2 and row.startswith("|") and row.endswith("|")):
            continue
        cells = [cell.strip() for cell in row.strip("|").split("|")]
        # Separator row: every non-empty cell is only dashes/colons.
        if all((not cell) or re.fullmatch(r":?-{2,}:?", cell) for cell in cells):
            continue
        if not cells:
            continue
        # Header row of the components table.
        if cells[0].lower() in ("component", "component name", "组件"):
            continue
        name = clean_md(cells[0])
        if not name:
            failures.append(
                "UNRESOLVED: unparsed component row at LICENSE-THIRD-PARTY.md:%d: %s"
                % (lineno, row))
            continue
        location_cell = cells[1] if len(cells) > 1 else ""
        license_cell = cells[2] if len(cells) > 2 else ""
        components.append({
            "name": name,
            "version": find_version(name),
            "license": find_license(license_cell or location_cell or name),
            "location": extract_location(location_cell),
            "line": lineno,
        })
    return components, failures, section_seen


def find_notices(directory):
    """Return license/notice files directly inside a component directory."""
    found = []
    try:
        for path in sorted(directory.iterdir(), key=lambda item: item.name.lower()):
            if path.is_file() and path.name.lower().startswith(NOTICE_PREFIXES):
                found.append(path)
    except OSError:
        return []
    return found


def component_dir(root, location):
    target = confine(root, location)
    return target.parent if target.is_file() else target


def render_entry(name, kind, fields, src):
    detail = "; ".join("%s=%s" % (key, value) for key, value in fields)
    return "ENTRY | %s | %s | %s | src=%s" % (name, kind, detail, src)


def build_manifest(root):
    """Collect repo facts into a deterministic manifest. Pure: no writes, no UE."""
    root = Path(root).resolve()
    errors = []
    unresolved = []
    addressed = []
    sources = set()
    entry_rows = []  # (name, kind, fields, src)

    texts = {"LICENSE": "", "LICENSE-THIRD-PARTY.md": "", ".gitmodules": ""}
    for rel in REQUIRED_FILES:
        path = root / rel
        if not path.is_file():
            errors.append("missing required file: %s" % rel)
            continue
        try:
            texts[rel] = read_source(path)
        except OSError as exc:
            errors.append("cannot read %s: %s" % (rel, exc))
            continue
        sources.add(rel)

    # --- root license: title + summary + identifier, no full-text duplication ---
    license_text = texts["LICENSE"]
    if license_text:
        nonempty = [line.strip() for line in license_text.splitlines() if line.strip()]
        title = nonempty[0] if nonempty else "未提供"
        entry_rows.append((
            "UnLuaNova (root license)", "ROOT-LICENSE",
            [("title", title),
             ("summary", "见 LICENSE 全文（不随包重复粘贴）"),
             ("license_id", find_license(license_text))],
            "LICENSE:1"))

    # --- git submodules ---
    for submodule in parse_gitmodules(texts[".gitmodules"]):
        entry_rows.append((
            submodule["name"], "SUBMODULE",
            [("path", submodule["path"] or "未提供"),
             ("url", submodule["url"] or "未提供")],
            ".gitmodules:%d" % submodule["line"]))

    # --- third-party components + per-component notice check ---
    tp_text = texts["LICENSE-THIRD-PARTY.md"]
    components, failures, section_seen = parse_third_party(tp_text)
    unresolved.extend(failures)
    if tp_text and not section_seen:
        unresolved.append(
            "UNRESOLVED: '## Included Runtime Components' section not found in "
            "LICENSE-THIRD-PARTY.md; cannot parse components")
    elif tp_text and not components:
        unresolved.append(
            "UNRESOLVED: no component rows parsed from LICENSE-THIRD-PARTY.md "
            "(section present but unrecognized)")
    for comp in components:
        location = comp["location"]
        if not location:
            notice = "n/a (no in-tree location)"
            unresolved.append(
                "UNRESOLVED: no in-tree location for %s; cannot verify a "
                "license/notice file (仓库未提供可解析的组件目录)" % comp["name"])
        else:
            try:
                directory = component_dir(root, location)
                found = find_notices(directory)
                if found:
                    notice = "found (%d)" % len(found)
                    for path in found:
                        try:
                            sources.add(path.relative_to(root).as_posix())
                        except ValueError:
                            pass
                else:
                    notice = "MISSING"
                    unresolved.append(
                        "UNRESOLVED: missing notice for %s (%s)"
                        % (comp["name"], location))
            except ValueError as exc:
                notice = "invalid location"
                unresolved.append(
                    "UNRESOLVED: invalid location for %s (%s): %s"
                    % (comp["name"], location, exc))
        entry_rows.append((
            comp["name"], "COMPONENT",
            [("version", comp["version"]),
             ("license", comp["license"]),
             ("location", location or "未提供"),
             ("notice", notice)],
            "LICENSE-THIRD-PARTY.md:%d" % comp["line"]))

    # --- documented open provenance questions (fail-closed) ---
    for gap in DECLARED_GAPS:
        try:
            resolver = confine(root, gap["resolver"])
            has_evidence = resolver.is_file() and resolver.stat().st_size > 0
        except (ValueError, OSError):
            has_evidence = False
        if has_evidence:
            sources.add(gap["resolver"])
            addressed.append(
                "ADDRESSED | %s | evidence=%s (content not verified by this tool)"
                % (gap["question"], gap["resolver"]))
        else:
            unresolved.append(
                "UNRESOLVED: %s (basis: %s); 需权利人/发行方决定，本工具不断言"
                % (gap["question"], gap["basis"]))

    # --- render deterministically ---
    entry_rows.sort(key=lambda row: (row[0], row[3]))
    entries = [render_entry(*row) for row in entry_rows]
    unresolved = sorted(set(unresolved))
    addressed = sorted(set(addressed))
    src_list = sorted(sources)

    body_lines = [
        "UnLuaNova NOTICES.txt (generated by Tools/GenerateNotices.py)",
        "DECLARATION: " + DECLARATION,
        "NOTE: entries are name-sorted; UNRESOLVED items are fail-closed and "
        "are NOT legal conclusions.",
        "",
        "## ENTRIES (%d)" % len(entries),
    ]
    body_lines.extend(entries if entries else ["(no entries)"])
    body_lines += ["", "## ADDRESSED OPEN ITEMS (%d)" % len(addressed)]
    body_lines.extend(addressed if addressed else ["(none)"])
    body_lines += ["", "## UNRESOLVED (%d)" % len(unresolved)]
    body_lines.extend(unresolved if unresolved else ["(none)"])
    body_lines.append("UNRESOLVED-COUNT: %d" % len(unresolved))
    body = "\n".join(body_lines) + "\n"

    # Hash the body only; the footer (sources + hash) is not fed back into the
    # hash, so the manifest is never self-referential.
    sha256 = hashlib.sha256(body.encode("utf-8")).hexdigest()
    footer_lines = ["", "---- SOURCES (%d) ----" % len(src_list)]
    footer_lines += ["source: %s" % path for path in src_list] if src_list else ["(none)"]
    footer_lines += ["---- BODY-SHA256 ----", sha256]
    footer = "\n".join(footer_lines) + "\n"
    text = body + footer

    return {
        "text": text,
        "body": body,
        "sha256": sha256,
        "entries": entries,
        "unresolved": unresolved,
        "addressed": addressed,
        "sources": src_list,
        "errors": errors,
    }


def first_diff(expected, actual):
    """Return the 1-based first differing logical line, or None if identical."""
    exp = expected.splitlines()
    act = actual.splitlines()
    for index in range(max(len(exp), len(act))):
        left = exp[index] if index < len(exp) else None
        right = act[index] if index < len(act) else None
        if left != right:
            return index + 1
    return None


def main(argv=None):
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", default="",
                        help="write the generated NOTICES.txt (LF newlines)")
    parser.add_argument("--check", action="append", nargs="+", default=[],
                        metavar="PATH",
                        help="compare shipped files against the generated list "
                             "(accepts several paths and can be repeated)")
    parser.add_argument("--root", default="",
                        help="repository root for testing (default: repository root)")
    parser.add_argument("--strict", action="store_true",
                        help="force fail-closed on UNRESOLVED (already the default)")
    parser.add_argument("--allow-unresolved", action="store_true",
                        help="view the list without failing on UNRESOLVED "
                             "(rejected together with --check)")
    parser.add_argument("--report", default="",
                        help="write a machine-readable JSON report")
    args = parser.parse_args(argv)

    root = Path(args.root).resolve() if args.root else ROOT
    if not root.is_dir():
        print("ERROR: root is not a directory: %s" % root, file=sys.stderr)
        return 1

    result = build_manifest(root)
    errors = list(result["errors"])
    unresolved = list(result["unresolved"])
    check_errors = []
    check_results = []  # (display_path, diff_line_or_None, ok)
    io_errors = []
    check_paths = [path for group in args.check for path in group]

    # --check is a release verification: it cannot be bypassed.
    if check_paths and args.allow_unresolved:
        check_errors.append(
            "--check enforces fail-closed semantics; --allow-unresolved "
            "cannot be combined with --check")

    for value in check_paths:
        try:
            path = confine(root, value)
        except ValueError as exc:
            check_errors.append("%s: %s" % (value, exc))
            check_results.append((value, None, False))
            print("CHECK %s: REJECTED (%s)" % (value, exc))
            continue
        try:
            actual = read_source(path)
        except OSError as exc:
            check_errors.append("%s: cannot read: %s" % (value, exc))
            check_results.append((value, None, False))
            print("CHECK %s: UNREADABLE (%s)" % (value, exc))
            continue
        diff = first_diff(result["text"], actual)
        if diff is None:
            check_results.append((value, None, True))
            print("CHECK %s: OK" % value)
        else:
            check_errors.append("%s: differs from generated list at line %d" % (value, diff))
            check_results.append((value, diff, False))
            print("CHECK %s: MISMATCH at line %d" % (value, diff))

    if args.out:
        try:
            out_path = resolve_output(root, args.out)
            out_path.parent.mkdir(parents=True, exist_ok=True)
            with open(str(out_path), "w", encoding="utf-8", newline="\n") as handle:
                handle.write(result["text"])
        except OSError as exc:
            io_errors.append("--out %s: %s" % (args.out, exc))

    all_errors = errors + check_errors + io_errors
    allow = args.allow_unresolved and not args.check and not args.strict
    failed_by_unresolved = bool(unresolved) and not allow
    all_checks_match = all(item[2] for item in check_results) if check_results else True
    # `ok` is the strict gate result, independent of --allow-unresolved.
    ok = (not all_errors) and (not unresolved) and all_checks_match
    exit_code = 0 if ((not all_errors) and (not failed_by_unresolved)
                      and all_checks_match) else 1

    report = {
        "ok": ok,
        "sources": result["sources"],
        "unresolved": unresolved,
        "entries": result["entries"],
        "sha256": result["sha256"],
        "errors": all_errors,
        "addressed": result["addressed"],
        "check_results": [
            {"path": path, "diff_line": diff, "ok": matched}
            for (path, diff, matched) in check_results
        ],
        "unresolved_count": len(unresolved),
    }
    if args.report:
        try:
            report_path = resolve_output(root, args.report)
            report_path.parent.mkdir(parents=True, exist_ok=True)
            with open(str(report_path), "w", encoding="utf-8", newline="\n") as handle:
                handle.write(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
        except OSError as exc:
            print("ERROR: --report %s: %s" % (args.report, exc), file=sys.stderr)
            exit_code = 1

    for message in all_errors:
        print("ERROR: %s" % message, file=sys.stderr)
    if unresolved:
        print("FAIL-CLOSED: %d UNRESOLVED item(s):" % len(unresolved), file=sys.stderr)
        for item in unresolved:
            print("  " + item, file=sys.stderr)
    print("notices: ok=%s entries=%d unresolved=%d sha256=%s" %
          (ok, len(result["entries"]), len(unresolved), result["sha256"]),
          file=sys.stderr)

    # Default invocation (no output/verification action) prints the list.
    if not (args.check or args.out or args.report):
        sys.stdout.write(result["text"])
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())

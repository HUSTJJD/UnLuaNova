#!/usr/bin/env python3
"""Static provenance gate for release evidence. No UE invocation, no repository mutation.

This is a *static structure* check, deliberately kept separate from the claim
"the current tree has been verified". The two are not the same thing:

* `Tools/AuditRepository.py:load_evidence()` counts every record in
  `Tools/evidence/index.json` as coverage and ignores the `historical` flag, so
  it happily reports "35 have execution evidence from 9 recorded run(s)" even
  when none of those nine runs was built from the current HEAD.
* Legacy `Saved/ReleaseTests/**/metadata/release-gate.json` files are bare
  `{"passed": true, "errors": []}` with no commit, run_id or timestamp, so they
  cannot prove they belong to one run, let alone to this checkout.

`Tools/ValidateRelease.py` now also writes provenance-aware gates carrying
`schema`/`tool`/`run`/`backend`/`carrier`/`net_matrix`/`commit`/`generated_at`.
When those fields are present on all four gates this tool checks the schema,
commit agreement, backend/carrier vs gate-path scope and commit vs HEAD; legacy
gates keep the original structural treatment and are noted, so the default
command never breaks on a historical gate set. `ADOPTED_GATE_CHECKS` below lists
exactly which of the new fields are consumed and how.

This tool accumulates every problem (fail-closed, no silent early exit), keeps
`historical_runs` out of `current_tree_runs`, and can demand at least one
non-historical record whose commit matches the current HEAD
(`--require-current`). Static structure being legal never means the current
tree was executed; the output says so explicitly.

Usage:
    python Tools/ValidateProvenance.py
    python Tools/ValidateProvenance.py --run Saved/ReleaseTests/baseline \
        --summary Saved/ReleaseTests/baseline/release-summary.json
    python Tools/ValidateProvenance.py --require-current \
        --report Saved/ReleaseTests/provenance.json
    python Tools/ValidateProvenance.py --evidence-index /tmp/index.json \
        --run /tmp/baseline --head cab95a83 --require-current
"""

import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_EVIDENCE_INDEX = ROOT / "Tools" / "evidence" / "index.json"
DEFAULT_RUN = ROOT / "Saved" / "ReleaseTests" / "baseline"

# Kept in sync with Tools/AuditRepository.py. Defined locally so this module is
# self-contained (only two new files are allowed in this change).
EVIDENCE_SCHEMA = "unluanova-evidence/1"

# Verified against the real tree: these are the four release gates (2 backends x
# 2 carriers) relative to a single Release run root such as
# Saved/ReleaseTests/baseline.
GATE_PATHS = (
    "lua/editor/lua/metadata/release-gate.json",
    "lua/game/metadata/release-gate.json",
    "luau/editor/luau/metadata/release-gate.json",
    "luau/game/metadata/release-gate.json",
)

# Written by the current Tools/ValidateRelease.py. Legacy gates are bare
# {"passed", "errors"} objects and are still readable, but a set that mixes the
# two formats is never a single Release run.
GATE_SCHEMA = "unluanova-release-gate/1"
GATE_META = {
    "lua/editor/lua/metadata/release-gate.json": ("Lua", "editor"),
    "lua/game/metadata/release-gate.json": ("Lua", "game"),
    "luau/editor/luau/metadata/release-gate.json": ("Luau", "editor"),
    "luau/game/metadata/release-gate.json": ("Luau", "game"),
}

# Exactly which provenance-aware gate fields this tool consumes, and the check
# applied to each. Reported verbatim so the conclusion states what was adopted.
# Deliberately NOT consumed: `tool` (free-text), `run` (the run directory string)
# and the `generated_at` timestamp value (only its presence is required).
ADOPTED_GATE_CHECKS = (
    "schema == unluanova-release-gate/1",
    "commit identical across the four gates",
    "backend/carrier match each gate's path scope",
    "commit[:8] matches --head / git HEAD",
    "commit is a 7-40 hex hash (a null commit is rejected, fail-closed)",
    "generated_at is a non-empty string",
    "net_matrix is false (a net-matrix gate is not a release gate)",
)

# 7-40 hex characters: short hashes are allowed for historical records, full
# 40-char hashes are the normal case. Anything else is rejected.
COMMIT_RE = re.compile(r"[0-9a-fA-F]{7,40}")

# Fields that load_evidence() already requires; checked here too so a broken
# record cannot be smuggled past a lenient consumer.
REQUIRED_RECORD_FIELDS = ("commit", "backend", "carrier", "engine", "machine")


def git_head():
    """Return the current HEAD hash, or None when git is unavailable.

    A None result is a *hard* failure under --require-current (fail-closed) and
    an explicit note otherwise; it is never silently treated as "no problem".
    """
    try:
        out = subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "HEAD"],
            stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        if out.returncode != 0:
            return None
        value = out.stdout.decode("utf-8", "replace").strip()
        return value or None
    except (OSError, ValueError):
        return None


def _resolve_evidence_file(base, value):
    """Resolve a record path confined to the evidence directory."""
    relative = Path(value)
    if relative.is_absolute() or re.match(r"^[A-Za-z]:", value):
        raise ValueError("absolute path")
    target = (base / relative).resolve()
    resolved_base = base.resolve()
    # Path.is_relative_to() is 3.9+; spell containment with .parents for 3.7.
    if target != resolved_base and resolved_base not in target.parents:
        raise ValueError("path escapes evidence directory")
    return target


def validate_evidence(index_path):
    """Validate Tools/evidence/index.json and every record it names.

    Returns (errors, stats). stats carries the per-record information needed to
    count historical vs current-tree runs without ever mixing the two.
    """
    errors = []
    stats = {"total": 0, "historical": 0, "records": []}
    index_path = Path(index_path)
    try:
        raw = json.loads(index_path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as exc:
        return ["evidence index: %s: %s" % (index_path, exc)], stats
    if not isinstance(raw, list):
        return ["evidence index: %s: expected a JSON list" % index_path], stats

    base = index_path.parent
    for i, entry in enumerate(raw):
        label = "evidence[%d]" % i
        if not isinstance(entry, dict):
            errors.append("%s: entry is not an object" % label)
            continue

        name = entry.get("file")
        run_id = entry.get("run_id")
        commit = entry.get("commit")

        # Required index fields.
        if not isinstance(name, str) or not name.strip():
            errors.append("%s: missing or invalid file" % label)
        if not isinstance(run_id, str) or not run_id.strip():
            errors.append("%s: missing or invalid run_id" % label)
        if not isinstance(commit, str) or not COMMIT_RE.fullmatch(commit):
            errors.append("%s: missing or invalid commit %r" % (label, commit))

        if not isinstance(name, str) or not name.strip():
            continue
        try:
            path = _resolve_evidence_file(base, name)
        except (ValueError, OSError) as exc:
            errors.append("%s: file %r: %s" % (label, name, exc))
            continue
        if not path.is_file():
            errors.append("%s: file %r: missing file" % (label, name))
            continue
        try:
            record = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as exc:
            errors.append("%s: file %r: %s" % (label, name, exc))
            continue
        if not isinstance(record, dict):
            errors.append("%s: file %r: expected a JSON object" % (label, name))
            continue

        if record.get("schema") != EVIDENCE_SCHEMA:
            errors.append("%s: file %r: unknown schema %r"
                          % (label, name, record.get("schema")))
        if record.get("run_id") != run_id:
            errors.append("%s: file %r: run_id does not match the index" % (label, name))
        for field in REQUIRED_RECORD_FIELDS:
            if not record.get(field):
                errors.append("%s: file %r: missing %s" % (label, name, field))

        rec_commit = record.get("commit")
        if not isinstance(rec_commit, str) or not COMMIT_RE.fullmatch(rec_commit):
            errors.append("%s: file %r: invalid commit %r" % (label, name, rec_commit))

        # historical is honoured from either the index entry or the record. A
        # historical record MUST name a non-empty source, otherwise a current
        # run could be casually relabelled as historical to dodge coverage.
        historical = bool(entry.get("historical")) or bool(record.get("historical"))
        source = record.get("source") or entry.get("source")
        if historical and (not isinstance(source, str) or not source.strip()):
            errors.append("%s: file %r: historical record is missing a non-empty source"
                          % (label, name))

        stats["total"] += 1
        if historical:
            stats["historical"] += 1
        stats["records"].append({
            "run_id": run_id,
            "commit": rec_commit if isinstance(rec_commit, str) else "",
            "historical": historical,
        })
    return errors, stats


def validate_gate_set(run_dir, expected_head=None):
    """Validate the four release gates under one run root.

    Checks that all four paths exist, each is `passed: true` with an empty
    `errors` list, and that no gate is borrowed from another run (a gate whose
    real path escapes the run root, or gates that declare conflicting
    `run_root` provenance). Returns (errors, details).
    """
    errors = []
    run_dir = Path(run_dir)
    try:
        resolved_run = run_dir.resolve()
    except OSError as exc:
        return ["gate run root: %s: %s" % (run_dir, exc)], {
            "gates_checked": 0, "same_run": False, "all_green": False}
    details = {"gates_checked": 0, "same_run": True, "all_green": False,
               "declared_roots": [], "schema_gates": 0, "declared_commits": []}
    green = True

    for rel in GATE_PATHS:
        gate = run_dir / rel
        if not gate.is_file():
            errors.append("gate: %s: missing file" % rel)
            green = False
            continue
        try:
            real = gate.resolve()
        except OSError as exc:
            errors.append("gate: %s: %s" % (rel, exc))
            green = False
            continue
        if real != resolved_run and resolved_run not in real.parents:
            errors.append("gate: %s: resolves outside the run root "
                          "(borrowed from another run?)" % rel)
            green = False
            continue
        try:
            data = json.loads(gate.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as exc:
            errors.append("gate: %s: %s" % (rel, exc))
            green = False
            continue
        if not isinstance(data, dict):
            errors.append("gate: %s: expected a JSON object" % rel)
            green = False
            continue

        details["gates_checked"] += 1
        if data.get("passed") is not True:
            errors.append("gate: %s: passed is not true" % rel)
            green = False
        gate_errors = data.get("errors")
        if not isinstance(gate_errors, list) or gate_errors:
            errors.append("gate: %s: errors is not an empty list" % rel)
            green = False

        # Optional provenance: production gates are bare {"passed", "errors"},
        # but when a run_root is declared all four must agree. This is what
        # rejects a set cobbled together from two different runs.
        if "run_root" in data:
            value = data["run_root"]
            if not isinstance(value, str) or not value.strip():
                errors.append("gate: %s: invalid run_root" % rel)
                green = False
            else:
                details["declared_roots"].append(value)

        # Current-format gates declare their own provenance. Check every field we
        # can check statically; a gate whose commit disagrees with HEAD is a
        # historical artifact and must not be read as this checkout's Release.
        if data.get("schema") is not None:
            if data["schema"] != GATE_SCHEMA:
                errors.append("gate: %s: unknown schema %r" % (rel, data["schema"]))
                green = False
            else:
                details["schema_gates"] += 1
                backend, carrier = GATE_META[rel]
                if data.get("backend") != backend or data.get("carrier") != carrier:
                    errors.append("gate: %s: backend/carrier %r/%r does not match %s/%s"
                                  % (rel, data.get("backend"), data.get("carrier"),
                                     backend, carrier))
                    green = False
                if data.get("net_matrix") is not False:
                    errors.append("gate: %s: a net-matrix gate cannot stand in for a "
                                  "release gate" % rel)
                    green = False
                commit = data.get("commit")
                if not isinstance(commit, str) or not COMMIT_RE.fullmatch(commit):
                    errors.append("gate: %s: missing or invalid commit %r" % (rel, commit))
                    green = False
                else:
                    details["declared_commits"].append(commit)
                if not isinstance(data.get("generated_at"), str) or not data["generated_at"]:
                    errors.append("gate: %s: missing generated_at" % rel)
                    green = False

    schema_count = details["schema_gates"]
    if 0 < schema_count < len(GATE_PATHS):
        errors.append("gate set mixes %d current-format and %d legacy gates; "
                      "regenerate all four from one Release run"
                      % (schema_count, len(GATE_PATHS) - schema_count))
        details["same_run"] = False
        green = False
    if schema_count == len(GATE_PATHS):
        commits = set(details["declared_commits"])
        if len(commits) > 1:
            errors.append("gate commits disagree across the four gates: %s"
                          % ", ".join(sorted(commits)))
            details["same_run"] = False
            green = False
        elif len(commits) == 1 and expected_head is not None:
            declared = next(iter(commits))
            if declared[:8] != expected_head[:8]:
                errors.append("gate commit %s does not match HEAD %s; these gates "
                              "belong to another checkout" % (declared[:8], expected_head[:8]))
                details["same_run"] = False
                green = False

    if details["declared_roots"]:
        distinct = set(details["declared_roots"])
        if len(distinct) > 1:
            errors.append("gates stitched from different run roots: %s"
                          % ", ".join(sorted(distinct)))
            details["same_run"] = False
            green = False
        elif len(details["declared_roots"]) != len(GATE_PATHS):
            errors.append("gates have inconsistent run-root provenance "
                          "(%d of %d declare run_root)"
                          % (len(details["declared_roots"]), len(GATE_PATHS)))
            details["same_run"] = False
            green = False

    details["all_green"] = bool(green and details["gates_checked"] == len(GATE_PATHS))
    return errors, details


def validate_summary(summary_path, gates_all_green):
    """Check release-summary.json complete/passed agree with the four gates.

    All-green gates plus a summary that is not `complete: true` must fail: a
    historical set of green gates is not a complete Release run.
    """
    path = Path(summary_path)
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as exc:
        return ["summary: %s: %s" % (path, exc)], False
    if not isinstance(data, dict):
        return ["summary: %s: expected a JSON object" % path], False

    errors = []
    consistent = True
    if data.get("complete") is not True:
        errors.append("summary: complete is not true; all-green gates cannot "
                      "stand in for a complete Release run")
        consistent = False
    if data.get("passed") is not gates_all_green:
        errors.append("summary: passed (%r) disagrees with the gate set "
                      "(all_green=%r)" % (data.get("passed"), gates_all_green))
        consistent = False
    return errors, consistent


def evaluate(index_path=None, run_dir=None, summary_path=None,
             require_current=False, head=None, git_head_fn=None):
    """Run every static check and return a machine-readable report dict.

    `head` overrides git (for tests); `git_head_fn` injects the HEAD reader.
    Nothing here prints or writes files, so callers/tests can inspect the raw
    result. All problems are accumulated, not short-circuited.
    """
    if git_head_fn is None:
        git_head_fn = git_head

    structural_errors = []
    notes = []

    # --- evidence -----------------------------------------------------------
    if index_path is None:
        index_path = DEFAULT_EVIDENCE_INDEX
    e_errors, evidence_stats = validate_evidence(index_path)
    structural_errors.extend(e_errors)

    # --- HEAD (needed by gate provenance and evidence recency alike) --------
    resolved_head = head if head is not None else git_head_fn()
    if resolved_head is None:
        notes.append("git HEAD is unavailable; current-tree evidence cannot be "
                     "determined (this is a hard failure under --require-current)")
    if resolved_head:
        head8 = resolved_head[:8]
        current_tree_runs = sum(
            1 for r in evidence_stats["records"]
            if not r["historical"] and r["commit"][:8] == head8)
    else:
        current_tree_runs = 0

    # --- gate set -----------------------------------------------------------
    gate_details = {"gates_checked": 0, "same_run": True, "all_green": False}
    run_checked = False
    effective_run = run_dir
    if run_dir is not None:
        run_checked = True
    elif DEFAULT_RUN.is_dir():
        # Default mode validates the real baseline gate set when it is present.
        effective_run = DEFAULT_RUN
        run_checked = True
    else:
        notes.append("gate set validation skipped: default run directory %s "
                     "is absent (e.g. a CI checkout without Saved/)" % DEFAULT_RUN)
    if run_checked:
        g_errors, gate_details = validate_gate_set(
            effective_run, expected_head=resolved_head)
        structural_errors.extend(g_errors)
        if gate_details.get("schema_gates") == len(GATE_PATHS):
            notes.append("all four gates declare current-format provenance "
                         "(schema/commit/backend/carrier/timestamp) and were "
                         "checked against HEAD")
        else:
            notes.append("legacy release-gate.json files carry no commit/run_id/"
                         "timestamp; same_run reflects structural confinement only, "
                         "not proof that the four gates belong to one real run")

    # --- summary ------------------------------------------------------------
    summary_checked = summary_path is not None
    summary_consistent = True
    if summary_checked:
        s_errors, summary_consistent = validate_summary(
            summary_path, gate_details["all_green"])
        structural_errors.extend(s_errors)

    errors = list(structural_errors)

    # --- --require-current --------------------------------------------------
    current_errors = []
    if require_current:
        if resolved_head is None:
            current_errors.append(
                "cannot determine current HEAD (git unavailable); failing "
                "closed on --require-current")
            current_errors.append("no current-tree execution evidence")
        elif current_tree_runs == 0:
            current_errors.append("no current-tree execution evidence")
    errors.extend(current_errors)

    report = {
        "ok": not errors,
        "errors": errors,
        "structural_errors": structural_errors,
        "notes": notes,
        "static_structure_ok": not structural_errors,
        "current_tree_verified": bool(
            require_current and current_tree_runs > 0 and not current_errors),
        "head": resolved_head,
        "require_current": bool(require_current),
        "total_runs": evidence_stats["total"],
        "historical_runs": evidence_stats["historical"],
        "current_tree_runs": current_tree_runs,
        "gates_checked": gate_details["gates_checked"],
        "gate_schema_gates": gate_details.get("schema_gates", 0),
        "adopted_gate_checks": list(ADOPTED_GATE_CHECKS),
        "same_run": gate_details["same_run"],
        "all_gates_green": gate_details["all_green"],
        "summary_checked": summary_checked,
        "summary_consistent": summary_consistent,
        "evidence_index": str(index_path),
        "run": str(effective_run) if run_checked else None,
        "summary": str(summary_path) if summary_checked else None,
    }
    return report


def render_report(report):
    """Human-readable conclusion; keeps 'static legal' and 'verified' apart."""
    lines = []
    if report["static_structure_ok"]:
        lines.append("Static structure: LEGAL (evidence index, gate set and "
                     "optional summary checks passed).")
    else:
        lines.append("Static structure: ILLEGAL (see ERROR lines below).")
    lines.append(
        "Evidence: %d recorded run(s); %d historical, %d current-tree "
        "(HEAD %s)." % (report["total_runs"], report["historical_runs"],
                        report["current_tree_runs"],
                        report["head"] or "unavailable"))
    lines.append(
        "Gates: %d/%d checked; same_run=%s; all_green=%s; "
        "summary_checked=%s; summary_consistent=%s."
        % (report["gates_checked"], len(GATE_PATHS), report["same_run"],
           report["all_gates_green"], report["summary_checked"],
           report["summary_consistent"]))
    lines.append("Adopted provenance-aware gate checks (" +
                 str(report["gate_schema_gates"]) + "/" +
                 str(len(GATE_PATHS)) + " gates in the new format):")
    for check in report["adopted_gate_checks"]:
        lines.append("  - " + check)
    lines.append("")
    lines.append("IMPORTANT: 'static structure LEGAL' is NOT 'current tree VERIFIED'.")
    if report.get("gate_schema_gates", 0) == len(GATE_PATHS):
        lines.append("All four gates declare current-format provenance and were "
                     "checked against HEAD, but that still does not prove the UE "
                     "automation behind them ran on this checkout. Use "
                     "--require-current to demand current-tree evidence.")
    else:
        lines.append("Historical/legacy evidence did not execute against this "
                     "checkout, and these release-gate files carry no "
                     "commit/run_id/timestamp, so they cannot prove the four gates "
                     "belong to one run, nor to the current tree. Use "
                     "--require-current to demand current-tree evidence.")
    lines.append("Not checked: real execution, capability coverage, reachability of "
                 "upstream URLs, gate authenticity/age, or that a green gate was "
                 "produced by this checkout.")
    if report["require_current"]:
        if report["current_tree_verified"]:
            lines.append("--require-current: SATISFIED (at least one non-historical "
                         "record matches HEAD).")
        else:
            lines.append("--require-current: FAILED -> no current-tree execution evidence")
    for note in report["notes"]:
        lines.append("NOTE: " + note)
    for error in report["errors"]:
        lines.append("ERROR: " + error)
    lines.append("")
    lines.append("Conclusion: %s." % ("PASS" if report["ok"] else "FAIL"))
    return "\n".join(lines)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Static provenance gate for release evidence (no UE run).",
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--evidence-index", type=Path, default=None,
                        help="evidence index.json (default: Tools/evidence/index.json)")
    parser.add_argument("--run", type=Path, default=None,
                        help="Release run root holding the 4 gates "
                             "(default: Saved/ReleaseTests/baseline)")
    parser.add_argument("--summary", type=Path, default=None,
                        help="optional release-summary.json to cross-check with the gates")
    parser.add_argument("--require-current", action="store_true",
                        help="require >=1 non-historical record whose commit matches HEAD")
    parser.add_argument("--head", default=None,
                        help="override git HEAD (for tests)")
    parser.add_argument("--report", type=Path, default=None,
                        help="write the machine-readable conclusion as JSON")
    args = parser.parse_args(argv)

    report = evaluate(
        index_path=args.evidence_index,
        run_dir=args.run,
        summary_path=args.summary,
        require_current=args.require_current,
        head=args.head,
    )
    print(render_report(report))

    if args.report is not None:
        # Path.write_text(newline=) is 3.10+; use open() for 3.7 compatibility.
        args.report.parent.mkdir(parents=True, exist_ok=True)
        with open(str(args.report), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(json.dumps(report, indent=2, ensure_ascii=False))

    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

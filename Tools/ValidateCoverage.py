#!/usr/bin/env python3
"""Fail-closed gate for Luau script coverage exports. No UE invocation, no baseline mutation.

The runtime writes `Saved/Coverage/coverage-Luau.json` (see `lua.coverage dump`). This gate
recomputes every total from the per-line counters, refuses empty/mismatched/level-0 reports,
and merges several runs by taking the maximum hit count per line. "No file" is a failure,
not a zero-coverage pass.

Usage:
    python Tools/ValidateCoverage.py --report Saved/Coverage/coverage-Luau.json \
        --backend Luau --min-coverage 0 --out Saved/Coverage/coverage-gate.json
    python Tools/ValidateCoverage.py --report runA.json --report runB.json \
        --require-source "_G.UnLuaNova.Event"
"""

import argparse
import json
from pathlib import Path

SCHEMA_VERSION = 1


def _is_int(value):
    return isinstance(value, int) and not isinstance(value, bool)


def load_report(path):
    """Return (document, errors). A missing, empty or malformed file is an error."""
    file = Path(path)
    try:
        raw = file.read_bytes()
    except OSError as exc:
        return None, [f"{file}: cannot read report: {exc}"]
    if not raw.strip():
        return None, [f"{file}: empty report"]
    try:
        document = json.loads(raw)
    except (ValueError, UnicodeDecodeError) as exc:
        return None, [f"{file}: malformed JSON: {exc}"]
    if not isinstance(document, dict):
        return None, [f"{file}: top level must be an object"]
    return document, []


def validate_document(document, backend, label):
    """Validate one export and return (per-line records, excluded sources, errors)."""
    errors = []
    if document.get("schema_version") != SCHEMA_VERSION:
        errors.append(f"{label}: unsupported schema_version")
    if document.get("backend") != backend:
        errors.append(f"{label}: backend is {document.get('backend')!r}, expected {backend!r}")
    level = document.get("coverage_level")
    if not _is_int(level) or level < 1 or level > 2:
        errors.append(f"{label}: coverage_level must be 1 or 2 (chunks compiled without counters)")
    if not isinstance(document.get("generated_utc"), str) or not document["generated_utc"].strip():
        errors.append(f"{label}: missing generated_utc")

    exclusions = document.get("exclusions")
    if not isinstance(exclusions, list) or not exclusions or any(
        not isinstance(item, str) or not item.strip() for item in exclusions
    ):
        errors.append(f"{label}: exclusions must be a declared, nonempty list of strings")
        exclusions = []

    sources = document.get("sources")
    if not isinstance(sources, list) or not sources:
        return [], [], errors + [f"{label}: no sources exported"]

    records = []
    excluded = []
    # totals describes the whole document, including excluded sources: the runtime wrote
    # those counters too. Only `records` (the counted denominator) drops exclusions.
    total_functions = 0
    total_instrumented = 0
    total_hit = 0
    total_covered_functions = 0

    for source_index, source in enumerate(sources):
        if not isinstance(source, dict):
            errors.append(f"{label}: sources[{source_index}] must be an object")
            continue
        name = source.get("source")
        if not isinstance(name, str) or not name.strip():
            errors.append(f"{label}: sources[{source_index}] has no source name")
            continue

        is_excluded = any(token in name for token in exclusions)
        if is_excluded:
            excluded.append(name)

        functions = source.get("functions")
        if not isinstance(functions, list) or not functions:
            errors.append(f"{label}: {name} has no functions")
            continue

        for function_index, function in enumerate(functions):
            where = f"{label}: {name} functions[{function_index}]"
            if not isinstance(function, dict):
                errors.append(f"{where} must be an object")
                continue
            if not isinstance(function.get("name"), str):
                errors.append(f"{where} has no function name")
            for key in ("linedefined", "depth"):
                if not _is_int(function.get(key)) or function[key] < 0:
                    errors.append(f"{where} has invalid {key}")
            lines = function.get("lines")
            if not isinstance(lines, list):
                errors.append(f"{where} has no lines array")
                continue

            total_functions += 1
            function_hit = False
            seen_lines = set()
            for line_index, entry in enumerate(lines):
                line_where = f"{where} lines[{line_index}]"
                if not isinstance(entry, dict):
                    errors.append(f"{line_where} must be an object")
                    continue
                line, hits = entry.get("line"), entry.get("hits")
                if not _is_int(line) or line < 0:
                    errors.append(f"{line_where} has invalid line")
                    continue
                if not _is_int(hits) or hits < 0:
                    errors.append(f"{line_where} has invalid hits")
                    continue
                if line in seen_lines:
                    errors.append(f"{line_where} repeats line {line}")
                    continue
                seen_lines.add(line)
                total_instrumented += 1
                if hits > 0:
                    total_hit += 1
                    function_hit = True
                if not is_excluded:
                    records.append({
                        "source": name,
                        "name": function.get("name", ""),
                        "linedefined": function.get("linedefined", 0),
                        "depth": function.get("depth", 0),
                        "line": line,
                        "hits": hits,
                    })
            if function_hit:
                total_covered_functions += 1

    totals = document.get("totals")
    if not isinstance(totals, dict):
        errors.append(f"{label}: missing totals")
    else:
        expected = {
            "functions": total_functions,
            "covered_functions": total_covered_functions,
            "instrumented_lines": total_instrumented,
            "hit_lines": total_hit,
        }
        for key, value in expected.items():
            if totals.get(key) != value:
                errors.append(f"{label}: totals.{key} is {totals.get(key)!r}, recomputed {value}")

    return records, excluded, errors


def validate(report_paths, backend="Luau", required_sources=(), min_coverage=0.0):
    """Validate and merge exports. Returns a fail-closed result dictionary."""
    errors = []
    all_records = []
    excluded = []
    labels = []

    if not report_paths:
        errors.append("no coverage reports supplied")
    for path in report_paths:
        document, read_errors = load_report(path)
        label = str(path)
        labels.append(label)
        errors.extend(read_errors)
        if document is None:
            continue
        records, doc_excluded, doc_errors = validate_document(document, backend, label)
        all_records.extend(records)
        excluded.extend(doc_excluded)
        errors.extend(doc_errors)

    # One VM can be dumped several times and a module can appear in several reports.
    # Merging by (source, function, line) with max(hits) is cumulative by construction,
    # exactly like the counters themselves.
    merged = {}
    for record in all_records:
        key = (record["source"], record["name"], record["linedefined"], record["depth"], record["line"])
        if key not in merged or record["hits"] > merged[key]:
            merged[key] = record["hits"]

    instrumented = len(merged)
    hit = sum(1 for hits in merged.values() if hits > 0)
    coverage = hit / instrumented if instrumented else 0.0

    if instrumented == 0:
        errors.append("no instrumented lines: an empty denominator is not a coverage pass")
    if coverage + 1e-12 < min_coverage:
        errors.append(f"coverage {coverage:.4f} is below required {min_coverage:.4f}")

    present = {key[0] for key in merged}
    for required in required_sources:
        if required not in present:
            errors.append(f"required source absent from counted exports: {required}")

    return {
        "passed": not errors,
        "errors": errors,
        "reports": labels,
        "backend": backend,
        "required_sources": list(required_sources),
        "min_coverage": min_coverage,
        "sources": len(present),
        "excluded_sources": sorted(set(excluded)),
        "functions": len({key[:4] for key in merged}),
        "instrumented_lines": instrumented,
        "hit_lines": hit,
        "coverage": round(coverage, 6),
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--report", action="append", default=[], required=True,
                        help="coverage export; repeat to aggregate runs")
    parser.add_argument("--backend", default="Luau", choices=["Luau", "Lua"])
    parser.add_argument("--require-source", action="append", default=[],
                        help="source name that must appear in the counted (non-excluded) exports")
    parser.add_argument("--min-coverage", type=float, default=0.0)
    parser.add_argument("--out", default="", help="write the gate result as JSON")
    args = parser.parse_args(argv)

    if not 0.0 <= args.min_coverage <= 1.0:
        parser.error("--min-coverage must be between 0 and 1")
    if args.backend != "Luau":
        # Plain Lua has no lua_getcoverage; asking for it must fail rather than silently
        # validating an empty report as a pass.
        print("Coverage is a Luau-only capability; the plain Lua backend cannot produce this report.")
        return 1

    result = validate(args.report, args.backend, args.require_source, args.min_coverage)

    if args.out:
        out = Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    if result["passed"]:
        print(f"Coverage gate passed: {result['hit_lines']}/{result['instrumented_lines']} lines "
              f"({result['coverage']:.2%}) across {result['sources']} source(s) and "
              f"{len(result['reports'])} report(s).")
    else:
        print("Coverage gate FAILED:")
        for error in result["errors"]:
            print(f"  - {error}")
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())

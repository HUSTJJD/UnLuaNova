#!/usr/bin/env python3
"""Cross-project benchmark normalizer.

Reads any number of UE logs, pulls the `#UNLUANOVA_BENCH_BEGIN` blocks out of them,
checks that the runs are actually comparable, and writes one normalized JSON per run
plus a comparison table.

Why the input is a log block and not a JSON file: a binding cannot be assumed to have
a JSON writer available inside the VM. Every binding can `print`. So the contract is a
line format, and *this* script owns the schema.

Comparability is enforced, not assumed:
  - every run must report the same engine version, machine, CPU, build config and carrier
  - every run must report the same case names and the same iteration count per case

If those do not match the script refuses to produce a table, because a table built from
runs that are not comparable is worse than no table.

Usage:
    python Tools/CrossProject/runner.py --baseline UnLuaNova logs/*.log
    python Tools/CrossProject/runner.py --emit-template
"""

from __future__ import annotations

import argparse
import io
import json
import os
import re
import sys

BEGIN = "#UNLUANOVA_BENCH_BEGIN"
END = "#UNLUANOVA_BENCH_END"

# Fields that must be identical across every run for a comparison to mean anything.
ALIGNMENT_FIELDS = ("engine", "machine", "cpu", "config", "carrier", "workload_revision")

REQUIRED_META = ("project", "backend", "engine", "machine", "cpu", "config",
                 "carrier", "workload_revision")

WORKLOAD_REVISION = "1"

# A template value or an "unknown" is not a machine identity. Accepting one would make
# two unrelated runs look aligned, which is exactly the failure mode this script exists
# to prevent (the UnLuaNova workload used to ship literal "FILL-ME" strings).
PLACEHOLDER_VALUES = {"", "?", "fill-me", "unknown", "tbd", "n/a", "none", "unspecified"}


def parse_log(path):
    """Return a list of run dicts found in one log file."""
    with io.open(path, "r", encoding="utf-8", errors="replace") as handle:
        lines = handle.read().splitlines()

    runs = []
    index = 0
    while index < len(lines):
        if lines[index].strip() != BEGIN:
            index += 1
            continue

        meta = {}
        cases = []
        index += 1
        while index < len(lines) and lines[index].strip() != END:
            line = lines[index].strip()
            index += 1

            if line.startswith("#meta "):
                for token in line[len("#meta "):].split():
                    if "=" in token:
                        key, _, value = token.partition("=")
                        meta[key.strip()] = value.strip()
            elif line.startswith("#case "):
                fields = {}
                for token in line[len("#case "):].split():
                    if "=" in token:
                        key, _, value = token.partition("=")
                        fields[key.strip()] = value.strip()
                cases.append(fields)

        if cases:
            runs.append({"meta": meta, "cases": cases})

    return runs


def normalize(run, source):
    meta = run["meta"]
    cases = []
    for raw in run["cases"]:
        samples = []
        if raw.get("samples"):
            samples = [float(v) for v in raw["samples"].split(",") if v]
        cases.append({
            "Case": raw.get("name", "?"),
            "Iterations": int(raw.get("iterations", 0)),
            "PerOpUs": float(raw.get("peropus", 0.0)),
            "Samples": samples,
        })

    ordered = [
        ("Project", meta.get("project", "?")),
        ("Backend", meta.get("backend", "?")),
        ("EngineVersion", meta.get("engine", "?")),
        ("Machine", meta.get("machine", "?")),
        ("Cpu", meta.get("cpu", "?")),
        ("BuildConfig", meta.get("config", "?")),
        ("Carrier", meta.get("carrier", "?")),
        ("WorkloadRevision", meta.get("workload_revision", "?")),
        ("MeasuredUtc", meta.get("measured_utc", "?")),
        ("SourceLog", source),
        ("Cases", cases),
    ]
    return dict(ordered)


def check_alignment(runs):
    """Return a list of human-readable problems. Empty means comparable."""
    problems = []

    for run in runs:
        meta = run["meta"]
        for field in REQUIRED_META:
            if not meta.get(field):
                problems.append("run %s/%s is missing required meta field '%s'"
                                % (meta.get("project", "?"), meta.get("backend", "?"), field))
        for field in ("machine", "cpu"):
            value = str(meta.get(field, "")).strip().lower()
            if value in PLACEHOLDER_VALUES or (value.startswith("<") and value.endswith(">")):
                problems.append("run %s/%s has a placeholder %s=%r; set real hardware identity "
                                "before running (see workload/unluanova/cases.lua)"
                                % (meta.get("project", "?"), meta.get("backend", "?"),
                                   field, meta.get(field)))

    for field in ALIGNMENT_FIELDS:
        values = sorted({r["meta"].get(field, "?") for r in runs})
        if len(values) > 1:
            problems.append("field '%s' differs across runs: %s" % (field, ", ".join(values)))

    shapes = []
    for run in runs:
        # Raw blocks still carry the log's lowercase keys ("name" / "iterations").
        shape = tuple(sorted((c.get("name", "?"), int(c.get("iterations", 0)))
                             for c in run["cases"]))
        shapes.append((run["meta"].get("project", "?"), run["meta"].get("backend", "?"), shape))

    reference = shapes[0][2]
    for project, backend, shape in shapes[1:]:
        if shape != reference:
            missing = [c for c in reference if c not in shape]
            extra = [c for c in shape if c not in reference]
            if missing:
                problems.append("%s/%s is missing cases: %s"
                                % (project, backend, ", ".join("%s(%d)" % c for c in missing)))
            if extra:
                problems.append("%s/%s has extra cases: %s"
                                % (project, backend, ", ".join("%s(%d)" % c for c in extra)))

    return problems


def build_table(runs, baseline):
    """Markdown comparison table. Ratios are baseline/other, higher is faster."""
    base = None
    for run in runs:
        if run["Project"].lower() == baseline.lower():
            base = run
            break
    if base is None:
        base = runs[0]

    # Baseline first, then the rest in input order - the header and the rows must agree.
    ordered = [base] + [r for r in runs if r is not base]

    header = "| Case | Iter |"
    for run in ordered:
        header += " %s/%s |" % (run["Project"], run["Backend"])
    header += " Fastest |"

    # Case | Iter | one column per run | Fastest
    divider = "|" + "---|" * (len(runs) + 3)

    rows = []
    for case in base["Cases"]:
        name = case["Case"]
        base_us = case["PerOpUs"]

        cells = ["`%s`" % name, "%d" % case["Iterations"]]

        best_us = base_us
        best_label = "%s/%s" % (base["Project"], base["Backend"])

        for run in ordered:
            other = None
            for c in run["Cases"]:
                if c["Case"] == name:
                    other = c
                    break
            if other is None:
                cells.append("—")
                continue
            ratio = (base_us / other["PerOpUs"]) if other["PerOpUs"] else 0.0
            cells.append("%.4f µs (%.2f×)" % (other["PerOpUs"], ratio))
            if other["PerOpUs"] and other["PerOpUs"] < best_us:
                best_us = other["PerOpUs"]
                best_label = "%s/%s" % (run["Project"], run["Backend"])

        cells.append(best_label)
        rows.append("| " + " | ".join(cells) + " |")

    return "\n".join([header, divider] + rows)


TEMPLATE = """#UNLUANOVA_BENCH_BEGIN
#meta project=UnLuaNova backend=Luau engine=5.8.3 machine=<host> cpu=<cpu> config=Development carrier=Editor workload_revision=1 measured_utc=<iso>
#case name=VM.Arithmetic iterations=200000 peropus=0.005936 samples=0.0059,0.0060,0.0059,0.0059,0.0060
#case name=UE.CallEmpty iterations=20000 peropus=0.459005 samples=0.459,0.460,0.458,0.459,0.459
#UNLUANOVA_BENCH_END
"""


def force_utf8():
    """A GBK console (the Windows Chinese default) cannot encode U+00B5, so the whole run
    dies on the micro sign unless stdout is switched first. Non-fatal: a stream that has
    no reconfigure (redirected, older interpreter) is simply left alone."""
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is None:
            continue
        try:
            reconfigure(encoding="utf-8")
        except Exception:
            pass


def main():
    force_utf8()

    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("logs", nargs="*", help="UE log files to scan")
    parser.add_argument("--baseline", default="UnLuaNova",
                        help="project to express ratios against (default: UnLuaNova)")
    parser.add_argument("--out", default="", help="directory for normalized JSON")
    parser.add_argument("--emit-template", action="store_true",
                        help="print the line format and exit")
    args = parser.parse_args()

    if args.emit_template:
        sys.stdout.write(TEMPLATE)
        return 0

    if not args.logs:
        parser.error("give at least one log file, or use --emit-template")

    raw_runs = []
    for path in args.logs:
        if not os.path.isfile(path):
            sys.stderr.write("warning: %s is not a file\n" % path)
            continue
        for raw in parse_log(path):
            raw_runs.append(raw)

    if not raw_runs:
        sys.stderr.write("no %s block found in any input\n" % BEGIN)
        return 1

    # Alignment is checked on the raw blocks, before normalization hides the meta fields.
    problems = check_alignment(raw_runs)
    if problems:
        sys.stderr.write("Refusing to compare - runs are not aligned:\n")
        for problem in problems:
            sys.stderr.write("  - %s\n" % problem)
        return 1

    runs = [normalize(raw, os.path.basename(path)) for raw in raw_runs]

    out_dir = args.out or os.path.join(os.path.dirname(os.path.abspath(__file__)), "results")
    if not os.path.isdir(out_dir):
        os.makedirs(out_dir)

    for run in runs:
        name = "%s-%s.json" % (run["Project"].lower(), run["Backend"].lower())
        target = os.path.join(out_dir, name)
        with io.open(target, "w", encoding="utf-8") as handle:
            handle.write(json.dumps(run, indent=2, ensure_ascii=False))
        sys.stdout.write("wrote %s\n" % target)

    table = build_table(runs, args.baseline)
    agreed = ", ".join("%s=%s" % (f, runs[0][f])
                       for f in ("EngineVersion", "Machine", "Cpu",
                                 "BuildConfig", "Carrier", "WorkloadRevision"))

    with io.open(os.path.join(out_dir, "comparison.md"), "w", encoding="utf-8") as handle:
        handle.write("# Cross-project benchmark comparison\n\n")
        handle.write("Baseline: %s\n\n" % args.baseline)
        handle.write(table)
        handle.write("\n\nAll runs agree on: %s\n" % agreed)

    sys.stdout.write("\n")
    sys.stdout.write(table)
    sys.stdout.write("\n\nAll runs agree on: %s\n" % agreed)
    sys.stdout.write("Also written to %s\n" % os.path.join(out_dir, "comparison.md"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

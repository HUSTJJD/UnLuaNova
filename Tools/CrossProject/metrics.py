"""Raw multi-run distributions for the cross-project benchmark.

runner.py answers "which binding is faster on these aligned runs". This answers the
question that has to come first: "what does one project's own variance look like?" A
single median per case is not enough to justify a ratio, so this merges every sample of
every run of ONE project/backend into a distribution (min / median / mean / p95 / max /
stddev) plus the median of per-run medians.

Fail-closed by design: runs with different hardware, engine, config, carrier, workload
revision, project or backend are refused; so are runs with different case sets, different
iteration counts, missing cases, or cases with no samples. Mixing them would produce a
smooth-looking distribution that describes nothing.

    python -B Tools/CrossProject/metrics.py results/*.json -o results/metrics.md
"""

from __future__ import print_function

import io
import json
import math
import os
import statistics
import sys

# Same alignment idea as runner.ALIGNMENT_FIELDS, plus Project/Backend: a distribution
# must describe one binding, not a blend of two.
ALIGNMENT_FIELDS = ("Project", "Backend", "EngineVersion", "Machine", "Cpu",
                    "BuildConfig", "Carrier", "WorkloadRevision")


def load_runs(paths):
    runs = []
    for path in paths:
        with io.open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
        if not isinstance(data, dict) or not isinstance(data.get("Cases"), list) or not data["Cases"]:
            raise ValueError("%s: expected a normalized run with a non-empty Cases array" % path)
        runs.append(data)
    return runs


def check_alignment(runs):
    """Return human-readable problems; empty means the runs can be merged."""
    problems = []
    for field in ALIGNMENT_FIELDS:
        values = sorted({str(run.get(field, "?")) for run in runs})
        if len(values) > 1:
            problems.append("field '%s' differs across runs: %s" % (field, ", ".join(values)))

    shapes = [tuple(sorted((case.get("Case", "?"), int(case.get("Iterations", 0)))
                           for case in run["Cases"])) for run in runs]
    if shapes and any(shape != shapes[0] for shape in shapes):
        problems.append("case sets or iteration counts differ across runs")
    return problems


def percentile(sorted_values, fraction):
    if len(sorted_values) == 1:
        return sorted_values[0]
    position = (len(sorted_values) - 1) * fraction
    low = int(math.floor(position))
    high = int(math.ceil(position))
    if low == high:
        return sorted_values[low]
    weight = position - low
    return sorted_values[low] + (sorted_values[high] - sorted_values[low]) * weight


def summarize(runs):
    """One distribution row per case, built from every sample of every run."""
    names = [case["Case"] for case in runs[0]["Cases"]]
    rows = []
    for name in names:
        per_run_medians = []
        samples = []
        iterations = None
        for run in runs:
            case = next((item for item in run["Cases"] if item.get("Case") == name), None)
            if case is None:
                raise ValueError("case '%s' is missing from a run" % name)
            values = [float(value) for value in case.get("Samples", []) if value is not None]
            if not values:
                raise ValueError("case '%s' has no samples; refusing to invent a distribution" % name)
            samples.extend(values)
            per_run_medians.append(statistics.median(values))
            iterations = int(case.get("Iterations", 0))

        samples.sort()
        rows.append({
            "Case": name,
            "Iterations": iterations,
            "Runs": len(per_run_medians),
            "Samples": len(samples),
            "MinUs": samples[0],
            "MedianUs": statistics.median(samples),
            "MeanUs": statistics.mean(samples),
            "P95Us": percentile(samples, 0.95),
            "MaxUs": samples[-1],
            "StdDevUs": statistics.pstdev(samples) if len(samples) > 1 else 0.0,
            "RunMedianUs": statistics.median(per_run_medians),
        })
    return rows


def render_report(rows, runs):
    first = runs[0]
    lines = [
        "# Benchmark distribution (one project, all runs)",
        "",
        "Project `%s` / backend `%s` on `%s` (`%s`), engine `%s`, config `%s`, "
        "carrier `%s`, workload revision `%s`." % (
            first.get("Project"), first.get("Backend"), first.get("Machine"),
            first.get("Cpu"), first.get("EngineVersion"), first.get("BuildConfig"),
            first.get("Carrier"), first.get("WorkloadRevision")),
        "",
        "%d run(s) merged. All values are microseconds per operation." % len(runs),
        "",
        "| Case | Iterations | Runs | Samples | Median us | Mean us | P95 us | Min us | Max us | StdDev | Run-median us |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for row in rows:
        lines.append("| `%s` | %d | %d | %d | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f |" % (
            row["Case"], row["Iterations"], row["Runs"], row["Samples"],
            row["MedianUs"], row["MeanUs"], row["P95Us"], row["MinUs"],
            row["MaxUs"], row["StdDevUs"], row["RunMedianUs"]))
    lines += [
        "",
        "This is a single-project distribution, not a cross-project conclusion. Publish a",
        "comparison only after both external projects have produced aligned real runs.",
        "",
    ]
    return "\n".join(lines)


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", help="normalized *.json runs from runner.py")
    parser.add_argument("-o", "--output", default=None, help="write the Markdown report here")
    args = parser.parse_args(argv)

    try:
        runs = load_runs(args.inputs)
    except (ValueError, IOError, OSError) as error:
        sys.stderr.write("cannot load runs: %s\n" % error)
        return 1

    problems = check_alignment(runs)
    if problems:
        sys.stderr.write("Refusing to merge - runs are not aligned:\n")
        for problem in problems:
            sys.stderr.write("  - %s\n" % problem)
        return 1

    try:
        rows = summarize(runs)
    except ValueError as error:
        sys.stderr.write("Refusing to merge: %s\n" % error)
        return 1

    report = render_report(rows, runs)
    if args.output:
        directory = os.path.dirname(os.path.abspath(args.output))
        if directory and not os.path.isdir(directory):
            os.makedirs(directory)
        with io.open(args.output, "w", encoding="utf-8") as handle:
            handle.write(report)
        sys.stdout.write("wrote %s\n" % args.output)
    sys.stdout.write(report)
    return 0


if __name__ == "__main__":
    sys.exit(main())

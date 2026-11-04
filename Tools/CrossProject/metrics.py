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

# Optional non-time metrics (revision 1 extension). Emitted on #metric lines by the
# workloads, normalized into a run's "Metrics" array by runner.py, and summarised
# here. Units are an explicit enum and each metric has a fixed expected unit.
VALID_UNITS = ("kb", "us", "bytes")
METRIC_UNITS = {
    "memory.peak_kb": "kb",
    "startup.load_us": "us",
    "gc.pause_us": "us",
    "code_size.bytes": "bytes",
}
DEFAULT_METRICS = ("memory.peak_kb", "startup.load_us", "gc.pause_us", "code_size.bytes")
UNAVAILABLE = "unavailable"


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


# ---------------------------------------------------------------------------
# Optional non-time metrics (revision 1 extension).
#
# Kept in separate functions on purpose: the five functions above keep their exact
# signatures and behaviour for old JSON that has no "Metrics" block. The default
# main() path never calls any of them, so its stdout text is byte-for-byte unchanged.
# ---------------------------------------------------------------------------

def _metric_number(name, value):
    """Validate one metric value. Return a finite non-negative float, or None for the
    'unavailable' sentinel. Anything else raises ValueError (fail-closed): we would
    rather refuse than substitute 0 or a placeholder."""
    if value == UNAVAILABLE:
        return None
    if isinstance(value, bool):
        raise ValueError("metric '%s' has a non-numeric value %r" % (name, value))
    try:
        number = float(value)
    except (TypeError, ValueError):
        raise ValueError("metric '%s' has a non-numeric value %r" % (name, value))
    if math.isnan(number) or math.isinf(number):
        raise ValueError("metric '%s' is not finite: %r" % (name, value))
    if number < 0:
        raise ValueError("metric '%s' is negative: %r" % (name, value))
    return number


def check_metric_alignment(runs, required):
    """Fail-closed validation of the optional Metrics block on every run.

    Raises ValueError on: a missing Metrics block, a missing / extra / duplicate
    metric name, an unknown or mismatched or drifting unit, a metric set that differs
    across runs, or a value that is not a finite non-negative number (or the exact
    'unavailable' sentinel). `required` lists the metric names every run must carry;
    it also defines the exact allowed metric set.
    """
    if not required:
        required = list(DEFAULT_METRICS)
    required = list(required)
    allowed = set(required)
    reference_names = None
    reference_units = None
    for run in runs:
        metrics = run.get("Metrics")
        if metrics is None:
            raise ValueError("a run is missing the Metrics block required by --require-metrics")
        if not isinstance(metrics, list):
            raise ValueError("Metrics must be an array of {Name, Value, Unit} entries")
        by_name = {}
        for entry in metrics:
            if not isinstance(entry, dict):
                raise ValueError("each Metrics entry must be an object")
            name = entry.get("Name")
            if not name:
                raise ValueError("a Metrics entry has no Name")
            if name in by_name:
                raise ValueError("duplicate metric '%s' in one run" % name)
            by_name[name] = entry
        names = set(by_name.keys())
        for name in required:
            if name not in by_name:
                raise ValueError("metric '%s' is missing from a run" % name)
        extra = names - allowed
        if extra:
            raise ValueError("unexpected metric(s) in a run: %s" % ", ".join(sorted(extra)))
        if reference_names is None:
            reference_names = names
        elif names != reference_names:
            raise ValueError("metric sets differ across runs")
        units = dict((name, by_name[name].get("Unit")) for name in required)
        if reference_units is None:
            reference_units = units
        elif units != reference_units:
            raise ValueError("metric units differ across runs")
        for name in required:
            entry = by_name[name]
            unit = entry.get("Unit")
            if unit not in VALID_UNITS:
                raise ValueError("metric '%s' has unknown unit %r; expected one of %s"
                                 % (name, unit, "/".join(VALID_UNITS)))
            expected = METRIC_UNITS.get(name)
            if expected is not None and unit != expected:
                raise ValueError("metric '%s' uses unit %r, expected %r"
                                 % (name, unit, expected))
            _metric_number(name, entry.get("Value"))
    return None


def summarize_metrics(runs, names=None):
    """Distribution rows (min / median / mean / p95 / max / stddev) for the non-time
    metrics, reusing percentile(). 'unavailable' is never treated as a number (and
    never as 0): it is excluded from the distribution. A metric unavailable in every
    run gets Status='unavailable' and no numeric fields. With `names` omitted, the
    metric names are taken from the first run's Metrics array.
    """
    if names is None:
        names = []
        for entry in runs[0].get("Metrics", []) or []:
            if isinstance(entry, dict) and entry.get("Name") and entry.get("Name") not in names:
                names.append(entry["Name"])
    rows = []
    for name in names:
        values = []
        unavailable_runs = 0
        unit = None
        for run in runs:
            entry = None
            for item in run.get("Metrics", []) or []:
                if isinstance(item, dict) and item.get("Name") == name:
                    entry = item
                    break
            if entry is None:
                raise ValueError("metric '%s' is missing from a run" % name)
            this_unit = entry.get("Unit")
            if unit is None:
                unit = this_unit
            elif this_unit != unit:
                raise ValueError("metric '%s' units differ across runs" % name)
            number = _metric_number(name, entry.get("Value"))
            if number is None:
                unavailable_runs += 1
            else:
                values.append(number)
        values.sort()
        row = {
            "Name": name,
            "Unit": unit,
            "Runs": len(runs),
            "NumericRuns": len(values),
            "UnavailableRuns": unavailable_runs,
            "Status": "ok" if values else "unavailable",
        }
        if values:
            row.update({
                "Min": values[0],
                "Median": statistics.median(values),
                "Mean": statistics.mean(values),
                "P95": percentile(values, 0.95),
                "Max": values[-1],
                "StdDev": statistics.pstdev(values) if len(values) > 1 else 0.0,
            })
        rows.append(row)
    return rows


def render_metric_table(metric_rows):
    """A second Markdown table appended after render_report's output. Empty input
    renders nothing, so the default path stays byte-for-byte unchanged and the
    "Benchmark distribution" heading plus the "not a cross-project conclusion"
    statement (both asserted by tests) are preserved verbatim."""
    if not metric_rows:
        return ""
    lines = [
        "",
        "## Non-time metrics",
        "",
        "Single-project distribution of the optional revision-1 non-time metrics.",
        "unavailable values are never counted as 0; a metric unavailable in every run",
        "has no numeric distribution.",
        "",
        "| Metric | Unit | Runs | Numeric | Unavailable | Min | Median | Mean | P95 | Max | StdDev | Status |",
        "|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|",
    ]
    for row in metric_rows:
        if row["Status"] == "ok":
            cells = [
                "`%s`" % row["Name"], str(row["Unit"]), str(row["Runs"]),
                str(row["NumericRuns"]), str(row["UnavailableRuns"]),
                "%.6f" % row["Min"], "%.6f" % row["Median"], "%.6f" % row["Mean"],
                "%.6f" % row["P95"], "%.6f" % row["Max"], "%.6f" % row["StdDev"], "ok",
            ]
        else:
            cells = [
                "`%s`" % row["Name"], str(row["Unit"]), str(row["Runs"]), "0",
                str(row["UnavailableRuns"]), "-", "-", "-", "-", "-", "-", "unavailable",
            ]
        lines.append("| " + " | ".join(cells) + " |")
    lines.append("")
    return "\n".join(lines)


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", help="normalized *.json runs from runner.py")
    parser.add_argument("-o", "--output", default=None, help="write the Markdown report here")
    parser.add_argument("--require-metrics", action="store_true",
                        help="require every run to carry the non-time metrics and append "
                             "their distribution table (default: off)")
    parser.add_argument("--metrics", default=",".join(DEFAULT_METRICS),
                        help="comma-separated non-time metric names required from every "
                             "run (default: the four revision-1 metrics)")
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

    if args.require_metrics:
        required = [name.strip() for name in args.metrics.split(",") if name.strip()]
        try:
            check_metric_alignment(runs, required)
            metric_rows = summarize_metrics(runs, required)
        except ValueError as error:
            sys.stderr.write("Refusing to merge metrics: %s\n" % error)
            return 1
        report = report + render_metric_table(metric_rows)

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

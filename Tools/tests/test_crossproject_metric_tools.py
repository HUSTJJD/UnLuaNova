"""Regressions for the optional non-time metrics (revision 1 extension).

Covers the fail-closed contract: missing metrics, unit mismatches, bad values
(negative / NaN / inf / non-numeric), metric-set and unit drift, multi-run
distributions, the 'unavailable' sentinel, and the --require-metrics switch.
Also checks that the old default path (no Metrics block) is unchanged and that
runner.py parses / normalizes / aligns #metric lines without disturbing the
10-case static contract.
"""

import io
import json
import os
import re
import statistics
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CROSS = ROOT / "Tools" / "CrossProject"
sys.path.insert(0, str(CROSS))
import metrics  # noqa: E402
import runner  # noqa: E402

DEFAULT = metrics.DEFAULT_METRICS
CASE_RE = re.compile(r'Name\s*=\s*"([^"]+)"\s*,\s*Iterations\s*=\s*(\d+)')


# ---------------------------------------------------------------------------
# Helpers (normalized JSON shape consumed by metrics.py / produced by runner.py)
# ---------------------------------------------------------------------------

def metric_entry(name, value, unit=None):
    if unit is None:
        unit = metrics.METRIC_UNITS.get(name, "kb")
    return {"Name": name, "Value": value, "Unit": unit}


def default_metrics(value_overrides=None):
    values = {
        "memory.peak_kb": "1024.0",
        "startup.load_us": "800.0",
        "gc.pause_us": "2000.0",
        "code_size.bytes": "12345",
    }
    if value_overrides:
        values.update(value_overrides)
    return [metric_entry(name, values[name], metrics.METRIC_UNITS[name]) for name in DEFAULT]


def make_run(metrics_list=None, machine="host-A"):
    run = {
        "Project": "UnLuaNova", "Backend": "Luau", "EngineVersion": "5.8.3",
        "Machine": machine, "Cpu": "Ryzen-4700G", "BuildConfig": "Development",
        "Carrier": "Editor", "WorkloadRevision": "1", "MeasuredUtc": "2026-11-04T00:00:00Z",
        "Cases": [{"Case": "VM.Arithmetic", "Iterations": 200000,
                   "PerOpUs": 0.01, "Samples": [1.0, 2.0, 3.0, 4.0, 5.0]}],
    }
    if metrics_list is not None:
        run["Metrics"] = metrics_list
    return run


def write_run(path, run):
    with io.open(path, "w", encoding="utf-8") as handle:
        handle.write(json.dumps(run))


# ---------------------------------------------------------------------------
# Raw log-block shapes consumed by runner.py
# ---------------------------------------------------------------------------

def raw_metric(name, value, unit):
    return {"name": name, "value": value, "unit": unit}


def raw_metrics():
    return [
        raw_metric("memory.peak_kb", "1024.0", "kb"),
        raw_metric("startup.load_us", "800.0", "us"),
        raw_metric("gc.pause_us", "2000.0", "us"),
        raw_metric("code_size.bytes", "12345", "bytes"),
    ]


def raw_run(metrics_list=None, include_metrics_key=True):
    run = {
        "meta": {"project": "UnLuaNova", "backend": "Luau", "engine": "5.8.3",
                 "machine": "host-A", "cpu": "Ryzen-4700G", "config": "Development",
                 "carrier": "Editor", "workload_revision": "1"},
        "cases": [{"name": "VM.Arithmetic", "iterations": "200000"}],
    }
    if include_metrics_key:
        run["metrics"] = metrics_list if metrics_list is not None else []
    return run


class CheckMetricAlignmentTests(unittest.TestCase):
    def test_valid_metrics_pass(self):
        self.assertIsNone(metrics.check_metric_alignment(
            [make_run(default_metrics()), make_run(default_metrics())], DEFAULT))

    def test_missing_metric_is_refused(self):
        short = [entry for entry in default_metrics() if entry["Name"] != "gc.pause_us"]
        with self.assertRaisesRegex(ValueError, "missing"):
            metrics.check_metric_alignment([make_run(default_metrics()), make_run(short)], DEFAULT)

    def test_missing_entire_metrics_block_is_refused(self):
        with self.assertRaisesRegex(ValueError, "Metrics block"):
            metrics.check_metric_alignment([make_run()], DEFAULT)

    def test_unknown_unit_is_refused(self):
        bad = [dict(entry) for entry in default_metrics()]
        bad[0]["Unit"] = "furlongs"
        with self.assertRaisesRegex(ValueError, "unknown unit"):
            metrics.check_metric_alignment([make_run(bad)], DEFAULT)

    def test_unit_mismatch_is_refused(self):
        bad = [dict(entry) for entry in default_metrics()]
        bad[0]["Unit"] = "us"  # memory.peak_kb must be kb
        with self.assertRaisesRegex(ValueError, "uses unit"):
            metrics.check_metric_alignment([make_run(bad)], DEFAULT)

    def test_negative_value_is_refused(self):
        with self.assertRaisesRegex(ValueError, "negative"):
            metrics.check_metric_alignment(
                [make_run(default_metrics({"memory.peak_kb": "-1"}))], DEFAULT)

    def test_nan_value_is_refused(self):
        with self.assertRaisesRegex(ValueError, "not finite"):
            metrics.check_metric_alignment(
                [make_run(default_metrics({"memory.peak_kb": "nan"}))], DEFAULT)

    def test_inf_value_is_refused(self):
        with self.assertRaisesRegex(ValueError, "not finite"):
            metrics.check_metric_alignment(
                [make_run(default_metrics({"gc.pause_us": "inf"}))], DEFAULT)

    def test_non_numeric_value_is_refused(self):
        with self.assertRaisesRegex(ValueError, "non-numeric"):
            metrics.check_metric_alignment(
                [make_run(default_metrics({"code_size.bytes": "not-a-number"}))], DEFAULT)

    def test_metric_set_differs_across_runs_is_refused(self):
        short = [entry for entry in default_metrics() if entry["Name"] != "startup.load_us"]
        with self.assertRaises(ValueError):
            metrics.check_metric_alignment([make_run(default_metrics()), make_run(short)], DEFAULT)

    def test_unit_drift_across_runs_is_refused(self):
        # A metric name outside METRIC_UNITS has no fixed expected unit, so only the
        # cross-run unit consistency check can catch a drift between kb and us.
        r1 = make_run([metric_entry("custom.metric", "1.0", "kb")])
        r2 = make_run([metric_entry("custom.metric", "1.0", "us")])
        with self.assertRaisesRegex(ValueError, "units differ"):
            metrics.check_metric_alignment([r1, r2], ["custom.metric"])

    def test_required_subset_is_honoured(self):
        subset = [metric_entry("memory.peak_kb", "1024.0", "kb"),
                  metric_entry("gc.pause_us", "2000.0", "us")]
        self.assertIsNone(metrics.check_metric_alignment(
            [make_run(subset)], ["memory.peak_kb", "gc.pause_us"]))


class SummarizeMetricsTests(unittest.TestCase):
    def test_multi_run_distribution_is_correct(self):
        runs = [make_run(default_metrics({"memory.peak_kb": str(value)}))
                for value in (100.0, 200.0, 300.0)]
        row = metrics.summarize_metrics(runs, ["memory.peak_kb"])[0]
        self.assertEqual(row["Unit"], "kb")
        self.assertEqual(row["Runs"], 3)
        self.assertEqual(row["NumericRuns"], 3)
        self.assertEqual(row["Min"], 100.0)
        self.assertEqual(row["Median"], 200.0)
        self.assertEqual(row["Mean"], 200.0)
        self.assertEqual(row["Max"], 300.0)
        self.assertAlmostEqual(row["P95"], metrics.percentile([100.0, 200.0, 300.0], 0.95))
        self.assertAlmostEqual(row["StdDev"], statistics.pstdev([100.0, 200.0, 300.0]))
        self.assertEqual(row["Status"], "ok")

    def test_unavailable_is_excluded_not_zero(self):
        runs = [
            make_run(default_metrics({"memory.peak_kb": "100.0"})),
            make_run(default_metrics({"memory.peak_kb": "200.0"})),
            make_run(default_metrics({"memory.peak_kb": "unavailable"})),
        ]
        row = metrics.summarize_metrics(runs, ["memory.peak_kb"])[0]
        self.assertEqual(row["NumericRuns"], 2)
        self.assertEqual(row["UnavailableRuns"], 1)
        # If 'unavailable' had been coerced to 0, Min would be 0, not 100.
        self.assertEqual(row["Min"], 100.0)
        self.assertEqual(row["Max"], 200.0)
        self.assertEqual(row["Mean"], 150.0)

    def test_all_unavailable_has_no_numeric_distribution(self):
        runs = [make_run(default_metrics({"memory.peak_kb": "unavailable"})) for _ in range(2)]
        row = metrics.summarize_metrics(runs, ["memory.peak_kb"])[0]
        self.assertEqual(row["Status"], "unavailable")
        self.assertEqual(row["NumericRuns"], 0)
        self.assertEqual(row["UnavailableRuns"], 2)
        self.assertNotIn("Min", row)
        self.assertNotIn("Mean", row)

    def test_unavailable_survives_render(self):
        rows = [{"Name": "code_size.bytes", "Unit": "bytes", "Runs": 2,
                 "NumericRuns": 0, "UnavailableRuns": 2, "Status": "unavailable"}]
        table = metrics.render_metric_table(rows)
        self.assertIn("Non-time metrics", table)
        self.assertIn("unavailable", table)

    def test_unit_drift_in_summarize_is_refused(self):
        r1 = make_run([metric_entry("custom.metric", "1.0", "kb")])
        r2 = make_run([metric_entry("custom.metric", "1.0", "us")])
        with self.assertRaisesRegex(ValueError, "units differ"):
            metrics.summarize_metrics([r1, r2], ["custom.metric"])

    def test_empty_metric_rows_render_nothing(self):
        self.assertEqual(metrics.render_metric_table([]), "")


class MainSwitchTests(unittest.TestCase):
    def test_default_path_old_json_still_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "old.json")
            output = os.path.join(directory, "metrics.md")
            write_run(path, make_run())  # no Metrics block at all
            self.assertEqual(metrics.main([path, "-o", output]), 0)
            with io.open(output, encoding="utf-8") as handle:
                report = handle.read()
            self.assertIn("Benchmark distribution", report)
            self.assertIn("not a cross-project conclusion", report)
            self.assertNotIn("Non-time metrics", report)

    def test_default_path_ignores_present_metrics(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "new.json")
            output = os.path.join(directory, "metrics.md")
            write_run(path, make_run(default_metrics()))
            self.assertEqual(metrics.main([path, "-o", output]), 0)
            with io.open(output, encoding="utf-8") as handle:
                report = handle.read()
            self.assertNotIn("Non-time metrics", report)

    def test_require_metrics_missing_block_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "old.json")
            write_run(path, make_run())
            self.assertEqual(metrics.main([path, "--require-metrics"]), 1)

    def test_require_metrics_with_valid_metrics_succeeds(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "new.json")
            output = os.path.join(directory, "metrics.md")
            write_run(path, make_run(default_metrics()))
            self.assertEqual(metrics.main([path, "--require-metrics", "-o", output]), 0)
            with io.open(output, encoding="utf-8") as handle:
                report = handle.read()
            self.assertIn("Non-time metrics", report)
            self.assertIn("memory.peak_kb", report)
            self.assertIn("Benchmark distribution", report)
            self.assertIn("not a cross-project conclusion", report)

    def test_require_metrics_bad_value_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "bad.json")
            write_run(path, make_run(default_metrics({"memory.peak_kb": "-1"})))
            self.assertEqual(metrics.main([path, "--require-metrics"]), 1)

    def test_require_metrics_custom_subset_succeeds(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "subset.json")
            subset = [metric_entry("memory.peak_kb", "1024.0", "kb"),
                      metric_entry("gc.pause_us", "2000.0", "us")]
            write_run(path, make_run(subset))
            self.assertEqual(metrics.main(
                [path, "--require-metrics", "--metrics", "memory.peak_kb,gc.pause_us"]), 0)


class RunnerMetricTests(unittest.TestCase):
    def test_parse_log_reads_metric_lines(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "run.log")
            lines = [
                "#UNLUANOVA_BENCH_BEGIN",
                "#meta project=UnLuaNova backend=Luau engine=5.8.3 machine=host-A "
                "cpu=Ryzen-4700G config=Development carrier=Editor "
                "workload_revision=1 measured_utc=2026-11-04T00:00:00Z",
                "#case name=VM.Arithmetic iterations=200000 peropus=0.01 samples=1.0,2.0",
                "#metric name=memory.peak_kb value=1024.0 unit=kb",
                "#metric name=startup.load_us value=800.0 unit=us",
                "#metric name=gc.pause_us value=2000.0 unit=us",
                "#metric name=code_size.bytes value=12345 unit=bytes",
                "#UNLUANOVA_BENCH_END",
            ]
            with io.open(path, "w", encoding="utf-8") as handle:
                handle.write("\n".join(lines) + "\n")
            runs = runner.parse_log(path)
            self.assertEqual(len(runs), 1)
            self.assertEqual(len(runs[0]["cases"]), 1)
            self.assertEqual(len(runs[0]["metrics"]), 4)
            self.assertEqual(runs[0]["metrics"][0]["name"], "memory.peak_kb")
            self.assertEqual(runs[0]["metrics"][0]["unit"], "kb")

    def test_normalize_emits_metrics(self):
        normalized = runner.normalize(raw_run(raw_metrics()), "run.log")
        self.assertIn("Cases", normalized)
        self.assertIn("Metrics", normalized)
        self.assertEqual(len(normalized["Metrics"]), 4)
        self.assertEqual(normalized["Metrics"][0]["Name"], "memory.peak_kb")
        self.assertEqual(normalized["Metrics"][0]["Unit"], "kb")

    def test_check_alignment_unit_mismatch_is_reported(self):
        good = raw_metrics()
        bad = [dict(entry) for entry in good]
        bad[0]["unit"] = "us"  # memory.peak_kb must be kb
        problems = runner.check_alignment([raw_run(good), raw_run(bad)])
        self.assertTrue(any("unit" in problem.lower() for problem in problems), problems)

    def test_check_alignment_without_metrics_is_unchanged(self):
        self.assertEqual(runner.check_alignment(
            [raw_run(include_metrics_key=False), raw_run(include_metrics_key=False)]), [])

    def test_template_has_metric_example(self):
        self.assertIn("#metric name=memory.peak_kb", runner.TEMPLATE)
        self.assertIn("unit=kb", runner.TEMPLATE)

    def test_static_case_contract_untouched(self):
        for path in (CROSS / "workload" / "unluanova" / "cases.lua",
                     CROSS / "workload" / "external" / "cases.lua"):
            self.assertEqual(len(CASE_RE.findall(path.read_text(encoding="utf-8"))), 10)
        # #metric example lines in docs / template must never look like a #case.
        for text in (runner.TEMPLATE,
                     (CROSS / "WORKLOAD.md").read_text(encoding="utf-8"),
                     (CROSS / "README.md").read_text(encoding="utf-8")):
            for line in text.splitlines():
                if line.strip().startswith("#metric "):
                    self.assertNotIn("Name =", line)
                    self.assertNotIn("Iterations =", line)


if __name__ == "__main__":
    unittest.main()

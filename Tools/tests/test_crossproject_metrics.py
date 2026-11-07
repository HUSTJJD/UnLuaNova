"""Regressions for the raw multi-run benchmark distribution tool."""

import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "CrossProject"))
import metrics  # noqa: E402


def make_run(samples=None, machine="host-A", project="UnLuaNova", backend="Luau",
             iterations=200000, case="VM.Arithmetic"):
    return {
        "Project": project, "Backend": backend, "EngineVersion": "5.8.3",
        "Machine": machine, "Cpu": "Ryzen-4700G", "BuildConfig": "Development",
        "Carrier": "Editor", "WorkloadRevision": "1", "MeasuredUtc": "2026-11-04T00:00:00Z",
        "Cases": [{
            "Case": case, "Iterations": iterations,
            "PerOpUs": 0.01,
            "Samples": list(samples if samples is not None else [1.0, 2.0, 3.0, 4.0, 5.0]),
        }],
    }


class SummarizeTests(unittest.TestCase):
    def test_all_samples_of_all_runs_are_merged(self):
        rows = metrics.summarize([make_run([1.0, 2.0, 3.0]), make_run([4.0, 5.0, 6.0])])
        self.assertEqual(len(rows), 1)
        row = rows[0]
        self.assertEqual(row["Runs"], 2)
        self.assertEqual(row["Samples"], 6)
        self.assertEqual(row["MinUs"], 1.0)
        self.assertEqual(row["MaxUs"], 6.0)
        self.assertEqual(row["MedianUs"], 3.5)
        self.assertEqual(row["RunMedianUs"], 3.5)
        self.assertEqual(row["Iterations"], 200000)

    def test_percentile_interpolates(self):
        self.assertAlmostEqual(metrics.percentile([1.0, 2.0, 3.0, 4.0], 0.5), 2.5)
        self.assertEqual(metrics.percentile([7.0], 0.95), 7.0)

    def test_empty_samples_are_refused(self):
        with self.assertRaisesRegex(ValueError, "no samples"):
            metrics.summarize([make_run([])])

    def test_missing_case_never_silently_drops_a_slow_case(self):
        first = make_run()
        second = make_run(case="Something.Else")
        # The alignment check is what normally catches this, but summarize must not
        # quietly produce a distribution from only one of the two runs either.
        self.assertTrue(metrics.check_alignment([first, second]))
        with self.assertRaisesRegex(ValueError, "missing from a run"):
            metrics.summarize([first, second])


class AlignmentTests(unittest.TestCase):
    def test_identical_runs_align(self):
        self.assertEqual(metrics.check_alignment([make_run(), make_run()]), [])

    def test_different_hardware_is_refused(self):
        problems = metrics.check_alignment([make_run(), make_run(machine="host-B")])
        self.assertTrue(any("Machine" in problem for problem in problems), problems)

    def test_two_projects_are_not_blended_into_one_distribution(self):
        problems = metrics.check_alignment([make_run(), make_run(project="slua-unreal")])
        self.assertTrue(any("Project" in problem for problem in problems), problems)

    def test_iteration_drift_is_refused(self):
        problems = metrics.check_alignment([make_run(), make_run(iterations=199999)])
        self.assertTrue(any("iteration" in problem.lower() for problem in problems), problems)


class MainTests(unittest.TestCase):
    def test_writes_a_report_and_refuses_misaligned_runs(self):
        with tempfile.TemporaryDirectory() as directory:
            good = os.path.join(directory, "a.json")
            other_machine = os.path.join(directory, "b.json")
            output = os.path.join(directory, "nested", "metrics.md")
            for path, run in ((good, make_run()), (other_machine, make_run(machine="host-B"))):
                with io.open(path, "w", encoding="utf-8") as handle:
                    handle.write(json.dumps(run))

            self.assertEqual(metrics.main([good, "-o", output]), 0)
            with io.open(output, encoding="utf-8") as handle:
                report = handle.read()
            self.assertIn("VM.Arithmetic", report)
            self.assertIn("not a cross-project conclusion", report)

            self.assertEqual(metrics.main([good, other_machine, "-o", output]), 1)

    def test_bad_input_file_is_refused(self):
        with tempfile.TemporaryDirectory() as directory:
            path = os.path.join(directory, "bad.json")
            with io.open(path, "w", encoding="utf-8") as handle:
                handle.write('{"Cases": []}')
            self.assertEqual(metrics.main([path]), 1)


if __name__ == "__main__":
    unittest.main()

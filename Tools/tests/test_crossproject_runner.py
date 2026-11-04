"""Alignment regressions for the cross-project benchmark normalizer."""

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "Tools" / "CrossProject"))
import runner  # noqa: E402


def make_run(machine="host-A", cpu="Ryzen-4700G"):
    return {
        "meta": {
            "project": "UnLuaNova", "backend": "Luau", "engine": "5.8.3",
            "machine": machine, "cpu": cpu, "config": "Development",
            "carrier": "Editor", "workload_revision": "1",
        },
        "cases": [{"name": "VM.Arithmetic", "iterations": "200000"}],
    }


class AlignmentTests(unittest.TestCase):
    def test_comparable_runs_align(self):
        self.assertEqual(runner.check_alignment([make_run(), make_run()]), [])

    def test_different_hardware_is_refused(self):
        problems = runner.check_alignment([make_run(), make_run(machine="host-B")])
        self.assertTrue(any("machine" in problem and "differs" in problem for problem in problems))

    def test_placeholder_machine_is_refused(self):
        for value in ("FILL-ME", "unknown", "<host>", "", "?"):
            with self.subTest(value=value):
                problems = runner.check_alignment([make_run(machine=value), make_run(machine=value)])
                self.assertTrue(any("placeholder" in problem for problem in problems), problems)

    def test_placeholder_cpu_is_refused(self):
        for value in ("FILL-ME", "<cpu>", "TBD"):
            with self.subTest(value=value):
                problems = runner.check_alignment([make_run(cpu=value), make_run(cpu=value)])
                self.assertTrue(any("placeholder" in problem for problem in problems), problems)

    def test_missing_meta_is_refused(self):
        run = make_run()
        del run["meta"]["config"]
        problems = runner.check_alignment([run])
        self.assertTrue(any("missing required meta" in problem for problem in problems))

    def test_case_sets_must_match(self):
        other = make_run()
        other["cases"] = []
        problems = runner.check_alignment([make_run(), other])
        self.assertTrue(any("missing cases" in problem for problem in problems))


if __name__ == "__main__":
    unittest.main()

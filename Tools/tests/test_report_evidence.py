import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from GenerateTestReport import load_benchmark, render_memory, load_tests as load_report_tests, render


class ReportEvidenceTests(unittest.TestCase):
    def test_full_paths_identify_cases_and_skips_prevent_all_passed_verdict(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "index.json").write_text(json.dumps({"tests": [
                dict(fullTestPath="API.One.same", testDisplayName="same", state="Success"),
                dict(fullTestPath="API.Two.same", testDisplayName="same", state="NotRun")]}))
            tests = load_report_tests(root)
            self.assertEqual([t["name"] for t in tests], ["API.One.same", "API.Two.same"])
            report = render(tests, [], "", "test", root)
            self.assertNotIn("全部通过", report)
            self.assertIn("API.Two.same", report)

    def test_performance_uses_run_artifact_and_never_shared_latest(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            shared = root / "Saved/Benchmark"
            shared.mkdir(parents=True)
            (shared / "latest-Lua.csv").write_text("Case,PerOpUs\nNative,999\n")
            run = root / "run"
            (run / "metadata").mkdir(parents=True)
            self.assertEqual(load_benchmark(root, "Lua", run), (None, None))
            artifact = {"Backend": "Lua", "Cases": [{"Case": "Native", "PerOpUs": 1.25}]}
            (run / "metadata/perf.json").write_text(json.dumps(artifact))
            self.assertEqual(load_benchmark(root, "Lua", run), ({"Native": 1.25}, None))
            self.assertEqual(load_benchmark(root, "Luau", run), (None, None))
            baseline = root / "Tools/baseline/perf-lua.json"
            baseline.parent.mkdir(parents=True)
            baseline.write_text(json.dumps(artifact))
            self.assertEqual(load_benchmark(root, "Lua", run), ({"Native": 1.25}, None))

    def test_memory_report_exposes_residuals(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            memory = root / "metadata/memory"
            memory.mkdir(parents=True)
            data = dict(case="delegates", passed=False, closed_envs=24, remaining_bytes=10,
                        remaining_blocks=1, retained_resources=2, samples=[dict(live_bytes=100)])
            (memory / "delegates.json").write_text(json.dumps(data))
            report = "\n".join(render_memory(root))
            self.assertIn("delegates | False | 24 | 10 | 1 | 2 | 100 - 100", report)


if __name__ == "__main__":
    unittest.main()

import json
import subprocess
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from GenerateTestReport import load_benchmark, render_memory, load_tests as load_report_tests, render, load_net_sessions, render_net_sessions


class ReportEvidenceTests(unittest.TestCase):
    def test_legacy_recordings_are_not_encoded_or_linked_in_reports(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "index.json").write_text(json.dumps({"tests": []}))
            shots = root / "screenshots"
            shots.mkdir()
            (shots / "Showcase_Binding.png").write_bytes(b"still")
            (shots / "Showcase_Binding_Clip_0001.png").write_bytes(b"frame")
            (root / "videos").mkdir()
            (root / "videos/session.mp4").write_bytes(b"legacy recording")
            subprocess.run([sys.executable, str(Path(__file__).resolve().parents[1] /
                            "GenerateTestReport.py"), "--reports", str(root)], check=True,
                           capture_output=True, text=True)
            report = (root / "report.md").read_text(encoding="utf-8")
            self.assertIn("Showcase_Binding.png", report)
            self.assertNotIn("_Clip_", report)
            self.assertNotIn(".mp4", report)
            self.assertNotIn("<video", report)
            self.assertFalse((root / "videos/Showcase_Binding_Clip.mp4").exists())
            summary = json.loads((root / "summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["screenshots"], 1)
            self.assertFalse(summary["recording_enabled"])
            self.assertNotIn("session_recordings", summary)

    def test_network_report_exposes_disconnected_and_raw_delivery_evidence(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            directory = root / "net/DedicatedServer"
            directory.mkdir(parents=True)
            (directory / "topology.json").write_text(json.dumps(dict(session="DedicatedServer",
                peers=[dict(label="Client1", role="Client", pid=123)])))
            path = directory / "Client1.json"
            path.write_text(json.dumps(dict(NetMode=0, Connected=False)))
            report = "\n".join(render_net_sessions(load_net_sessions(root)))
            self.assertIn("| Client1 | 123 | 0 | False | missing | missing |", report)
            self.assertIn("schema 2", report)
            path.write_text(json.dumps(dict(SchemaVersion=2, NetMode=3, Connected=True,
                Complete=True, SessionId="session", Observations=[dict(Round=1, Actors=[
                    dict(ActorId=1, Owned=True, Challenge=1234, HP=1235, CallLog="FromServer:1236|ToAll:1237|")])])))
            report = "\n".join(render_net_sessions(load_net_sessions(root)))
            self.assertIn("| Client1 | 1 | 1 | True | 1234 | 1235 | FromServer:1236&#124;ToAll:1237&#124; |", report)

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

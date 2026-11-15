"""Performance baseline-reset detection in ValidateRelease.py (audit fix #1).

A baseline_reset event must make validate_performance explicitly report
"本轮为基线重置，25% 回退门禁不适用" (an information item, not a silent pass and
not a hard failure), and malformed/incomplete events must be reported as errors.

Run with: python -B -m unittest discover -s Tools/tests -p "test_*.py"
"""

import copy
import importlib.util
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "release_gate_reset", TOOLS / "ValidateRelease.py")
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)


class PerfBaselineResetTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    # ------------------------------------------------------------------ helpers
    def perf(self):
        """A minimal but valid performance artifact (no sampling block)."""
        return {
            "Backend": "Lua",
            "EngineVersion": "6.0.0-0+UE6",
            "Machine": "runner",
            "Cases": [{"Case": "call", "Iterations": 100, "PerOpUs": 2.0}],
        }

    def valid_event(self, **overrides):
        event = {
            "event": "baseline_reset",
            "backend": "Lua",
            "reason": "EngineVersion changed; committed baseline rebuilt from this run",
            "old_engine": "5.8.3-0+UE5",
            "new_engine": "6.0.0-0+UE6",
            "old_sha256": "a" * 64,
            "new_sha256": "b" * 64,
            "utc": "2026-11-15T00:00:00.0000000Z",
            "commit": "c" * 40,
        }
        event.update(overrides)
        return event

    def write_event(self, run_dir, data):
        path = Path(run_dir) / "metadata" / "perf-baseline-reset.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        return path

    def write_raw(self, run_dir, text):
        path = Path(run_dir) / "metadata" / "perf-baseline-reset.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    # ------------------------------------------------------------------ tests
    def test_valid_reset_emits_explicit_not_applicable_marker(self):
        """A valid reset event -> explicit INFO marker + markdown, no failure."""
        run = self.root / "lua/editor/lua"
        self.write_event(run, self.valid_event())
        actual = self.perf()
        baseline = copy.deepcopy(actual)
        buf = io.StringIO()
        with redirect_stdout(buf):
            errors = gate.validate_performance(actual, baseline, "Lua", run_dir=run)
        out = buf.getvalue()
        self.assertEqual(errors, [], "a reset must NOT be a hard failure")
        self.assertIn(gate.RESET_MARKER, out)
        self.assertIn(gate.BASELINE_RESET_NOTICE, out)
        self.assertIn("门禁不适用", out)
        # The notice must be durable so it appears in the release report.
        md = run / "metadata/perf-baseline-reset.md"
        self.assertTrue(md.is_file())
        self.assertIn("25% 回退门禁不适用", md.read_text(encoding="utf-8"))

    def test_valid_reset_is_not_a_silent_return(self):
        """Distinct from a normal pass: no event -> no marker, event -> marker."""
        plain = self.root / "plain"
        plain.mkdir()
        buf_plain = io.StringIO()
        with redirect_stdout(buf_plain):
            gate.validate_performance(self.perf(), self.perf(), "Lua", run_dir=plain)
        self.assertNotIn(gate.RESET_MARKER, buf_plain.getvalue())

        run = self.root / "reset"
        self.write_event(run, self.valid_event())
        buf_reset = io.StringIO()
        with redirect_stdout(buf_reset):
            gate.validate_performance(self.perf(), self.perf(), "Lua", run_dir=run)
        self.assertIn(gate.RESET_MARKER, buf_reset.getvalue())

    def test_25_percent_gate_skipped_only_on_reset(self):
        """A 30% regression fails normally but is not-applicable on a reset round."""
        actual = self.perf()
        baseline = copy.deepcopy(actual)
        actual["Cases"][0]["PerOpUs"] = 2.6  # > 2.0 * 1.25
        # No reset event -> the 25% gate applies and fails.
        no_event_dir = self.root / "no-event"
        no_event_dir.mkdir()
        normal_errors = gate.validate_performance(
            actual, baseline, "Lua", run_dir=no_event_dir)
        self.assertTrue(any("25%" in e for e in normal_errors), normal_errors)
        # Valid reset event -> gate explicitly not applicable, no regression error.
        run = self.root / "reset"
        self.write_event(run, self.valid_event())
        buf = io.StringIO()
        with redirect_stdout(buf):
            errors = gate.validate_performance(actual, baseline, "Lua", run_dir=run)
        self.assertEqual(errors, [])
        self.assertIn("门禁不适用", buf.getvalue())

    def test_missing_or_malformed_reset_event_is_an_error(self):
        """Field missing / wrong type / empty -> hard error (never silent)."""
        good = self.valid_event()
        cases = [
            ("missing backend", {k: v for k, v in good.items() if k != "backend"}),
            ("null field", self.valid_event(new_sha256=None)),
            ("wrong type", self.valid_event(utc=12345)),
            ("empty string", self.valid_event(reason="")),
            ("missing commit", {k: v for k, v in good.items() if k != "commit"}),
        ]
        for i, (label, event) in enumerate(cases):
            with self.subTest(label=label):
                run = self.root / ("bad-%d" % i)
                self.write_event(run, event)
                errors = gate.validate_performance(
                    self.perf(), copy.deepcopy(self.perf()), "Lua", run_dir=run)
                self.assertTrue(errors, label)
                self.assertTrue(any("baseline_reset" in e for e in errors), label)

    def test_invalid_json_and_non_object_reset_event_is_an_error(self):
        for i, raw in enumerate(('{"broken":', "[1, 2, 3]")):
            with self.subTest(raw=raw):
                run = self.root / ("raw-%d" % i)
                self.write_raw(run, raw)
                errors = gate.validate_performance(
                    self.perf(), copy.deepcopy(self.perf()), "Lua", run_dir=run)
                self.assertTrue(errors)
                self.assertTrue(any("baseline_reset" in e for e in errors))

    def test_other_backend_event_is_ignored(self):
        """An event for a different backend must not disable this backend's gate."""
        run = self.root / "other-backend"
        self.write_event(run, self.valid_event(backend="Luau"))
        actual = self.perf()
        baseline = copy.deepcopy(actual)
        actual["Cases"][0]["PerOpUs"] = 2.6
        buf = io.StringIO()
        with redirect_stdout(buf):
            errors = gate.validate_performance(actual, baseline, "Lua", run_dir=run)
        self.assertTrue(any("25%" in e for e in errors))
        self.assertNotIn(gate.RESET_MARKER, buf.getvalue())

    def test_upward_walk_finds_run_root_reset_event(self):
        """Mirror the real layout: --run <out>/lua/editor/lua, event at <out>/metadata."""
        out = self.root / "out"
        carrier = out / "lua/editor/lua"
        path = out / "metadata/perf-baseline-reset.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(self.valid_event()), encoding="utf-8")
        buf = io.StringIO()
        with redirect_stdout(buf):
            errors = gate.validate_performance(
                self.perf(), copy.deepcopy(self.perf()), "Lua", run_dir=carrier)
        self.assertEqual(errors, [])
        self.assertIn(gate.RESET_MARKER, buf.getvalue())

    def test_no_event_keeps_normal_25_percent_gate_and_no_marker(self):
        run = self.root / "plain2"
        run.mkdir()
        buf = io.StringIO()
        with redirect_stdout(buf):
            errors = gate.validate_performance(
                self.perf(), copy.deepcopy(self.perf()), "Lua", run_dir=run)
        self.assertEqual(errors, [])
        self.assertNotIn(gate.RESET_MARKER, buf.getvalue())


if __name__ == "__main__":
    unittest.main()

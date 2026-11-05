import importlib.util
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import tempfile
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("validate_provenance", TOOLS / "ValidateProvenance.py")
vp = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vp)


class ValidateProvenanceTests(unittest.TestCase):
    """Fixtures are built entirely inside a TemporaryDirectory; no real repo state."""

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        # A fixed, deterministic HEAD so tests never depend on a live git call.
        self.head = "cab95a83" + "0" * 32
        self.gates = self.build_gates()
        self.summary = self.build_summary(True, True)
        self.evidence_index = self.build_evidence([
            {"run_id": "hist-run", "commit": "347bb8c5", "historical": True,
             "source": "Saved/ReleaseTests/baseline/lua/game"},
            {"run_id": "current-run", "commit": self.head, "historical": False,
             "source": "Saved/MyRun"},
        ])

    # ------------------------------------------------------------------ helpers
    def write_json(self, path, data):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        return path

    def build_evidence(self, records):
        """Create a valid evidence directory from a list of record specs.

        Spec keys (all optional): run_id, commit, historical, source, schema,
        record_run_id, skip_record, file, record (dict merged into the record),
        entry (dict merged into the index entry), drop_entry (keys to remove).
        source="__NONE__" removes the source field from both sides.
        """
        base = self.root / "evidence"
        base.mkdir(parents=True, exist_ok=True)
        index = []
        for i, spec in enumerate(records):
            run_id = spec.get("run_id", "run-%d" % i)
            fname = spec.get("file", run_id + ".json")
            commit = spec.get("commit", "93b78167" + "0" * 32)
            historical = bool(spec.get("historical", False))
            source_spec = spec.get("source", "Saved/source/%s" % run_id)
            no_source = source_spec == "__NONE__"
            source = "" if no_source else source_spec

            record = {
                "schema": "unluanova-evidence/1",
                "run_id": spec.get("record_run_id", run_id),
                "commit": commit,
                "historical": historical,
                "source": source,
                "worktree_dirty": False,
                "engine": "5.8.3-0+UE5",
                "platform": "Win64",
                "config": "Development",
                "carrier": "Editor",
                "backend": "Lua",
                "machine": "test-machine",
                "cpu": "test-cpu",
                "report_created_on": "",
                "totals": {"tests": 1, "passed": 1, "failed": 0, "not_run": 0},
                "capabilities": {},
            }
            if spec.get("schema") is not None:
                record["schema"] = spec["schema"]
            if no_source:
                record.pop("source", None)
            record.update(spec.get("record", {}) or {})
            if not spec.get("skip_record", False):
                self.write_json(base / fname, record)

            entry = {
                "run_id": run_id,
                "file": fname,
                "commit": commit,
                "backend": "Lua",
                "carrier": "Editor",
                "engine": "5.8.3-0+UE5",
            }
            if historical:
                entry["historical"] = True
            if not no_source and source:
                entry["source"] = source
            entry.update(spec.get("entry", {}) or {})
            for key in spec.get("drop_entry", []):
                entry.pop(key, None)
            index.append(entry)
        return self.write_json(base / "index.json", index)

    def build_gates(self, run_rel="baseline", overrides=None, run_roots=None):
        """Write the four gates. overrides[rel]=None skips a gate; otherwise it
        replaces the gate body. run_roots[rel] adds an optional run_root field.
        """
        overrides = overrides or {}
        run_roots = run_roots or {}
        for rel in vp.GATE_PATHS:
            if rel in overrides and overrides[rel] is None:
                continue
            body = {"passed": True, "errors": []}
            if rel in run_roots:
                body["run_root"] = run_roots[rel]
            if rel in overrides:
                body = overrides[rel]
            self.write_json(self.root / run_rel / rel, body)
        return self.root / run_rel

    def build_summary(self, complete=True, passed=True, where="baseline"):
        return self.write_json(
            self.root / where / "release-summary.json",
            {"schema_version": 1, "complete": complete, "passed": passed})

    def evaluate(self, **kwargs):
        kwargs.setdefault("index_path", self.evidence_index)
        kwargs.setdefault("run_dir", self.gates)
        kwargs.setdefault("head", self.head)
        return vp.evaluate(**kwargs)

    def run_main(self, argv):
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = vp.main(argv)
        return rc, buf.getvalue()

    # ------------------------------------------------------------------ tests
    def test_valid_evidence_gates_and_summary_pass(self):
        report = self.evaluate(summary_path=self.summary)
        self.assertTrue(report["ok"])
        self.assertTrue(report["static_structure_ok"])
        self.assertFalse(report["current_tree_verified"])
        self.assertEqual(report["total_runs"], 2)
        self.assertEqual(report["historical_runs"], 1)
        self.assertEqual(report["current_tree_runs"], 1)
        self.assertEqual(report["gates_checked"], 4)
        self.assertTrue(report["same_run"])
        self.assertTrue(report["all_gates_green"])
        self.assertTrue(report["summary_consistent"])
        self.assertEqual(report["errors"], [])

    def test_historical_record_without_source_fails(self):
        index = self.build_evidence([
            {"run_id": "orphan", "commit": "347bb8c5", "historical": True,
             "source": "__NONE__"},
        ])
        report = self.evaluate(index_path=index)
        self.assertFalse(report["ok"])
        self.assertFalse(report["static_structure_ok"])
        self.assertTrue(any("missing a non-empty source" in e for e in report["errors"]))

    def test_wrong_evidence_schema_fails(self):
        index = self.build_evidence([
            {"run_id": "bad-schema", "schema": "bogus/schema/9"},
        ])
        report = self.evaluate(index_path=index)
        self.assertFalse(report["ok"])
        self.assertTrue(any("unknown schema" in e for e in report["errors"]))

    def test_run_id_mismatch_fails(self):
        index = self.build_evidence([
            {"run_id": "index-name", "record_run_id": "record-name"},
        ])
        report = self.evaluate(index_path=index)
        self.assertFalse(report["ok"])
        self.assertTrue(any("run_id does not match" in e for e in report["errors"]))

    def test_missing_evidence_file_fails(self):
        index = self.build_evidence([
            {"run_id": "gone", "skip_record": True},
        ])
        report = self.evaluate(index_path=index)
        self.assertFalse(report["ok"])
        self.assertTrue(any("missing file" in e for e in report["errors"]))

    def test_index_not_a_list_fails(self):
        bad_dir = self.root / "badindex"
        bad_dir.mkdir(parents=True, exist_ok=True)
        index = self.write_json(bad_dir / "index.json", {"not": "a list"})
        report = self.evaluate(index_path=index)
        self.assertFalse(report["ok"])
        self.assertTrue(any("expected a JSON list" in e for e in report["errors"]))

    def test_invalid_commit_format_fails(self):
        index = self.build_evidence([
            {"run_id": "bad-commit", "commit": "not-a-hex-string"},
        ])
        report = self.evaluate(index_path=index)
        self.assertFalse(report["ok"])
        self.assertTrue(any("invalid commit" in e for e in report["errors"]))

    def test_missing_gate_fails(self):
        gates = self.build_gates(run_rel="missing-gate",
                                 overrides={vp.GATE_PATHS[0]: None})
        report = self.evaluate(run_dir=gates)
        self.assertFalse(report["ok"])
        self.assertEqual(report["gates_checked"], 3)
        self.assertFalse(report["all_gates_green"])
        self.assertTrue(any("missing file" in e for e in report["errors"]))

    def test_gate_passed_false_fails(self):
        gates = self.build_gates(
            run_rel="gate-false",
            overrides={vp.GATE_PATHS[0]: {"passed": False, "errors": []}})
        report = self.evaluate(run_dir=gates)
        self.assertFalse(report["ok"])
        self.assertTrue(any("passed is not true" in e for e in report["errors"]))

    def test_gate_errors_nonempty_fails(self):
        gates = self.build_gates(
            run_rel="gate-errors",
            overrides={vp.GATE_PATHS[0]: {"passed": True, "errors": ["boom"]}})
        report = self.evaluate(run_dir=gates)
        self.assertFalse(report["ok"])
        self.assertTrue(any("errors is not an empty list" in e for e in report["errors"]))

    def test_gates_from_different_run_roots_fail(self):
        roots = {
            vp.GATE_PATHS[0]: "runA", vp.GATE_PATHS[1]: "runA",
            vp.GATE_PATHS[2]: "runB", vp.GATE_PATHS[3]: "runB",
        }
        gates = self.build_gates(run_rel="stitched", run_roots=roots)
        report = self.evaluate(run_dir=gates, summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertFalse(report["same_run"])
        self.assertTrue(any("different run roots" in e for e in report["errors"]))

    def test_all_green_but_summary_incomplete_fails(self):
        summary = self.build_summary(False, True, where="incomplete")
        report = self.evaluate(summary_path=summary)
        self.assertFalse(report["ok"])
        self.assertFalse(report["summary_consistent"])
        self.assertTrue(report["all_gates_green"])
        self.assertTrue(any("complete is not true" in e for e in report["errors"]))

    def test_summary_passed_disagrees_with_gates_fails(self):
        summary = self.build_summary(True, False, where="disagree")
        report = self.evaluate(summary_path=summary)
        self.assertFalse(report["ok"])
        self.assertFalse(report["summary_consistent"])
        self.assertTrue(any("disagrees" in e for e in report["errors"]))

    def test_require_current_all_historical_fails(self):
        index = self.build_evidence([
            {"run_id": "h1", "commit": "347bb8c5", "historical": True,
             "source": "Saved/ReleaseTests/baseline/lua/game"},
            {"run_id": "h2", "commit": "347bb8c5", "historical": True,
             "source": "Saved/ReleaseTests/baseline/luau/game"},
        ])
        report = self.evaluate(index_path=index, summary_path=self.summary,
                               require_current=True)
        self.assertFalse(report["ok"])
        self.assertTrue(report["static_structure_ok"])
        self.assertFalse(report["current_tree_verified"])
        self.assertEqual(report["current_tree_runs"], 0)
        self.assertIn("no current-tree execution evidence", report["errors"])
        rc, out = self.run_main([
            "--evidence-index", str(index),
            "--run", str(self.gates),
            "--summary", str(self.summary),
            "--require-current", "--head", self.head,
        ])
        self.assertEqual(rc, 1)
        self.assertIn("no current-tree execution evidence", out)

    def test_require_current_matching_current_record_passes(self):
        index = self.build_evidence([
            {"run_id": "fresh", "commit": self.head, "historical": False,
             "source": "Saved/MyRun"},
        ])
        report = self.evaluate(index_path=index, summary_path=self.summary,
                               require_current=True)
        self.assertTrue(report["ok"])
        self.assertTrue(report["current_tree_verified"])
        self.assertEqual(report["current_tree_runs"], 1)
        rc, _out = self.run_main([
            "--evidence-index", str(index),
            "--run", str(self.gates),
            "--summary", str(self.summary),
            "--require-current", "--head", self.head,
        ])
        self.assertEqual(rc, 0)

    def test_report_json_has_complete_fields(self):
        report_path = self.root / "out" / "provenance.json"
        rc, _out = self.run_main([
            "--evidence-index", str(self.evidence_index),
            "--run", str(self.gates),
            "--summary", str(self.summary),
            "--head", self.head,
            "--report", str(report_path),
        ])
        self.assertEqual(rc, 0)
        self.assertTrue(report_path.is_file())
        data = json.loads(report_path.read_text(encoding="utf-8"))
        required = {
            "ok", "errors", "total_runs", "historical_runs",
            "current_tree_runs", "gates_checked", "same_run",
            "summary_consistent", "static_structure_ok",
            "current_tree_verified", "head", "require_current",
            "all_gates_green", "summary_checked", "notes",
            "structural_errors", "evidence_index", "run", "summary",
            "gate_schema_gates", "adopted_gate_checks",
        }
        self.assertTrue(required.issubset(set(data.keys())),
                        "missing keys: %s" % (required - set(data.keys())))
        self.assertIsInstance(data["ok"], bool)
        self.assertIsInstance(data["errors"], list)
        self.assertIsInstance(data["total_runs"], int)
        self.assertIsInstance(data["historical_runs"], int)
        self.assertIsInstance(data["current_tree_runs"], int)
        self.assertIsInstance(data["gates_checked"], int)
        self.assertIsInstance(data["same_run"], bool)
        self.assertIsInstance(data["summary_consistent"], bool)

    def test_default_mode_separates_static_legal_from_verified(self):
        with mock.patch.object(vp, "DEFAULT_EVIDENCE_INDEX", self.evidence_index), \
             mock.patch.object(vp, "DEFAULT_RUN", self.gates), \
             mock.patch.object(vp, "git_head", return_value=self.head):
            rc, out = self.run_main([])
        self.assertEqual(rc, 0)
        self.assertIn("Static structure: LEGAL", out)
        self.assertIn("is NOT", out)
        self.assertIn("current tree VERIFIED", out)
        self.assertIn("PASS", out)
        # No unconditional claim that the current tree was executed.
        self.assertNotIn("current_tree_verified: True", out)

    def test_git_unavailable_require_current_fails_closed(self):
        report = vp.evaluate(
            index_path=self.evidence_index,
            run_dir=self.gates,
            summary_path=self.summary,
            require_current=True,
            head=None,
            git_head_fn=lambda: None,
        )
        self.assertFalse(report["ok"])
        self.assertTrue(report["static_structure_ok"])
        self.assertFalse(report["current_tree_verified"])
        self.assertIn("no current-tree execution evidence", report["errors"])
        self.assertTrue(any("git unavailable" in e for e in report["errors"]))
        self.assertTrue(any("git HEAD is unavailable" in n for n in report["notes"]))

    # ── current-format gate provenance (written by today's ValidateRelease.py) ──

    def current_gate(self, rel, **overrides):
        backend, carrier = vp.GATE_META[rel]
        body = {
            "passed": True, "errors": [],
            "schema": vp.GATE_SCHEMA,
            "tool": "ValidateRelease.py",
            "run": "Saved/ReleaseTests/baseline",
            "backend": backend, "carrier": carrier,
            "net_matrix": False,
            "commit": self.head,
            "generated_at": "2026-11-04T13:00:00+00:00",
        }
        body.update(overrides)
        return body

    def build_current_gates(self, mutate=None):
        mutate = mutate or {}
        bodies = {}
        for rel in vp.GATE_PATHS:
            body = self.current_gate(rel)
            changes = dict(mutate.get(rel, {}))
            for key in changes.pop("__drop__", []):
                body.pop(key, None)
            body.update(changes)
            bodies[rel] = body
        return self.build_gates(overrides=bodies)

    def test_current_format_gates_matching_head_pass(self):
        self.build_current_gates()
        report = self.evaluate(summary_path=self.summary)
        self.assertTrue(report["ok"], report["errors"])
        self.assertEqual(report["gate_schema_gates"], 4)
        self.assertTrue(report["same_run"])
        self.assertTrue(any("current-format provenance" in n for n in report["notes"]))

    def test_current_format_gates_reject_another_checkout(self):
        self.build_current_gates()
        report = self.evaluate(head="deadbeef" + "0" * 32,
                               summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertTrue(any("does not match HEAD" in e for e in report["errors"]))

    def test_mixing_legacy_and_current_gates_fails(self):
        only_one = {vp.GATE_PATHS[0]: self.current_gate(vp.GATE_PATHS[0])}
        self.build_gates(overrides=only_one)
        report = self.evaluate(summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertTrue(any("mixes" in e for e in report["errors"]))

    def test_current_format_gate_rejects_unknown_schema(self):
        self.build_current_gates({vp.GATE_PATHS[0]: {"schema": "made-up/9"}})
        report = self.evaluate(summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertTrue(any("unknown schema" in e for e in report["errors"]))

    def test_current_format_gate_rejects_wrong_backend_or_carrier(self):
        self.build_current_gates({vp.GATE_PATHS[0]: {"backend": "Luau", "carrier": "game"}})
        report = self.evaluate(summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertTrue(any("backend/carrier" in e for e in report["errors"]))

    def test_net_matrix_gate_cannot_stand_in_for_a_release_gate(self):
        self.build_current_gates({vp.GATE_PATHS[0]: {"net_matrix": True}})
        report = self.evaluate(summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertTrue(any("net-matrix gate" in e for e in report["errors"]))

    def test_current_format_gate_requires_commit_and_timestamp(self):
        self.build_current_gates({
            vp.GATE_PATHS[0]: {"__drop__": ["commit", "generated_at"]}})
        report = self.evaluate(summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertTrue(any("missing or invalid commit" in e for e in report["errors"]))
        self.assertTrue(any("missing generated_at" in e for e in report["errors"]))

    def test_current_format_gates_reject_disagreeing_commits(self):
        self.build_current_gates({vp.GATE_PATHS[0]: {"commit": "deadbeef" + "0" * 32}})
        report = self.evaluate(summary_path=self.summary)
        self.assertFalse(report["ok"])
        self.assertTrue(any("commits disagree" in e for e in report["errors"])
                        or any("does not match HEAD" in e for e in report["errors"]))


if __name__ == "__main__":
    unittest.main()

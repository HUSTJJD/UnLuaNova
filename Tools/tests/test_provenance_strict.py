"""Strict --require-current provenance checks (audit fix #2).

Covers the gates that previously could be bypassed: a green verdict with
summary_checked=false, a "failed" release summary, and an unclean worktree at
the release decision point (--strict-worktree). Mid-pipeline a dirty tree is
expected because RecordEvidence.py and UpdateReleaseStatus.py rewrite tracked
files while the Release is still running.
All fixtures live in a TemporaryDirectory; the worktree tests construct a REAL
throwaway git repository and run `git status --porcelain` against it.

Run with: python -B -m unittest discover -s Tools/tests -p "test_*.py"
"""

import importlib.util
import io
import json
from contextlib import redirect_stdout
from pathlib import Path
import subprocess
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_provenance_strict", TOOLS / "ValidateProvenance.py")
vp = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(vp)


class ProvenanceStrictTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        # Fixed deterministic HEAD; no test depends on the live repository.
        self.head = "a1b2c3d4" + "0" * 32
        self.evidence_index = self._build_evidence()
        # One run dir WITH the latest release-summary.json (auto-discovery),
        # one WITHOUT it (summary_checked=false).
        self.gates_run = self._build_gates("run", with_summary=True)
        self.gates_no_summary = self._build_gates("no-summary", with_summary=False)
        self._repo_counter = 0

    # ------------------------------------------------------------------ helpers
    def _write(self, path, data):
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        return path

    def _build_evidence(self):
        """A valid index with one non-historical record matching self.head, so
        the ONLY source of failure in these tests is the strict check itself."""
        base = self.root / "evidence"
        base.mkdir(parents=True, exist_ok=True)
        record = {
            "schema": "unluanova-evidence/1",
            "run_id": "current-run",
            "commit": self.head,
            "historical": False,
            "source": "Saved/MyRun",
            "worktree_dirty": False,
            "engine": "6.0.0-0+UE6",
            "platform": "Win64",
            "config": "Development",
            "carrier": "Editor",
            "backend": "Lua",
            "machine": "runner",
        }
        self._write(base / "current-run.json", record)
        index = [{
            "run_id": "current-run",
            "file": "current-run.json",
            "commit": self.head,
            "backend": "Lua",
            "carrier": "Editor",
            "engine": "6.0.0-0+UE6",
            "source": "Saved/MyRun",
        }]
        return self._write(base / "index.json", index)

    def _build_gates(self, run_rel, with_summary):
        run = self.root / run_rel
        for rel in vp.GATE_PATHS:
            self._write(run / rel, {"passed": True, "errors": []})
        if with_summary:
            self._write(run / "release-summary.json",
                        {"schema_version": 1, "complete": True, "passed": True})
        return run

    def _evaluate(self, **kwargs):
        kwargs.setdefault("index_path", self.evidence_index)
        kwargs.setdefault("run_dir", self.gates_run)
        kwargs.setdefault("head", self.head)
        return vp.evaluate(**kwargs)

    def _run_main(self, argv):
        buf = io.StringIO()
        with redirect_stdout(buf):
            rc = vp.main(argv)
        return rc, buf.getvalue()

    def _git(self, repo, *args):
        return subprocess.run(["git", "-C", str(repo), *args],
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def _make_repo(self, files=()):
        """Create a REAL temporary git repo; `files` become untracked changes."""
        self._repo_counter += 1
        repo = self.root / ("gitrepo-%d" % self._repo_counter)
        repo.mkdir(parents=True, exist_ok=True)
        init = self._git(repo, "init", "-q")
        self.assertEqual(init.returncode, 0, "git init failed: %r" % init.stderr)
        for rel in files:
            path = repo / rel
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("dirty\n", encoding="utf-8")
        return repo

    # ------------------------------------------------------------------ tests
    def test_require_current_summary_not_checked_fails(self):
        """require-current + summary_checked=false -> hard failure."""
        report = self._evaluate(run_dir=self.gates_no_summary, require_current=True)
        self.assertFalse(report["ok"])
        self.assertFalse(report["summary_checked"])
        self.assertFalse(report["current_tree_verified"])
        self.assertTrue(any("summary_checked=false" in e for e in report["errors"]),
                        report["errors"])

    def test_require_current_passed_false_summary_fails(self):
        """require-current + latest summary passed=false -> hard failure."""
        run = self._build_gates("failed-round", with_summary=True)
        self._write(run / "release-summary.json",
                    {"schema_version": 1, "complete": True, "passed": False})
        report = self._evaluate(run_dir=run, require_current=True)
        self.assertFalse(report["ok"])
        self.assertTrue(report["summary_checked"])  # auto-discovered
        self.assertTrue(any("passed is not true" in e for e in report["errors"]),
                        report["errors"])

    def test_require_current_incomplete_summary_fails(self):
        run = self._build_gates("incomplete-round", with_summary=True)
        self._write(run / "release-summary.json",
                    {"schema_version": 1, "complete": False, "passed": True})
        report = self._evaluate(run_dir=run, require_current=True)
        self.assertFalse(report["ok"])
        self.assertTrue(any("complete is not true" in e for e in report["errors"]),
                        report["errors"])

    def test_dirty_worktree_warns_by_default_and_fails_when_strict(self):
        """Mid-pipeline a dirty tree warns; --strict-worktree makes it fatal."""
        repo = self._make_repo(["source.txt"])
        worktree_fn = lambda: vp.git_worktree_changes(repo)
        self.assertTrue(worktree_fn(), "the constructed repo must really be dirty")
        loose = self._evaluate(require_current=True, worktree_fn=worktree_fn)
        self.assertTrue(loose["ok"], loose["errors"])
        self.assertTrue(loose["worktree_checked"])
        self.assertTrue(any("worktree has uncommitted changes" in n
                            for n in loose["notes"]), loose["notes"])
        strict = self._evaluate(require_current=True, worktree_fn=worktree_fn,
                                strict_worktree=True)
        self.assertFalse(strict["ok"])
        self.assertTrue(any("worktree has uncommitted changes" in e
                            for e in strict["errors"]), strict["errors"])

    def test_product_dir_changes_are_ignored_by_worktree_check(self):
        """Saved/Intermediate/Binaries/DerivedDataCache never count as dirty."""
        repo = self._make_repo([
            "Saved/foo.txt",
            "Intermediate/bar.bin",
            "Binaries/Win64/x.dll",
            "DerivedDataCache/cache.bin",
        ])
        self.assertEqual(vp.git_worktree_changes(repo), [])

    def test_require_current_clean_tree_and_good_summary_pass(self):
        """The positive path: clean tree + complete/passed summary -> verified."""
        repo = self._make_repo([])  # fresh repo, no untracked files
        worktree_fn = lambda: vp.git_worktree_changes(repo)
        self.assertEqual(worktree_fn(), [])
        report = self._evaluate(require_current=True, worktree_fn=worktree_fn)
        self.assertTrue(report["ok"], report["errors"])
        self.assertTrue(report["current_tree_verified"])
        self.assertTrue(report["summary_checked"])

    def test_non_require_current_only_warns_and_stays_ok(self):
        """Non-require-current: the same bad conditions are warnings, exit 0."""
        repo = self._make_repo(["source.txt"])
        report = self._evaluate(
            run_dir=self.gates_no_summary,
            require_current=False,
            worktree_fn=lambda: vp.git_worktree_changes(repo))
        self.assertTrue(report["ok"])
        self.assertEqual(report["errors"], [])
        notes = " ".join(report["notes"])
        self.assertIn("WARNING", notes)
        self.assertIn("summary_checked=false", notes)
        self.assertIn("worktree has uncommitted changes", notes)

    def test_non_require_current_failed_summary_is_only_a_warning(self):
        """Same passed=false summary: hard fail under require-current, warn otherwise."""
        run = self._build_gates("failed-round-diag", with_summary=True)
        self._write(run / "release-summary.json",
                    {"schema_version": 1, "complete": True, "passed": False})
        report = self._evaluate(run_dir=run, require_current=False)
        self.assertTrue(report["ok"], report["errors"])
        self.assertFalse(report["summary_checked"])
        self.assertIn("WARNING", " ".join(report["notes"]))

    def test_non_require_current_main_exits_zero(self):
        """Diagnostic CLI (no --require-current) never hard-fails."""
        rc, out = self._run_main([
            "--evidence-index", str(self.evidence_index),
            "--run", str(self.gates_no_summary),
            "--head", self.head,
        ])
        self.assertEqual(rc, 0)
        self.assertIn("WARNING", out)
        self.assertIn("PASS", out)


if __name__ == "__main__":
    unittest.main()

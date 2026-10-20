import copy
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).resolve().parents[1] / "AuditRepository.py"
SPEC = importlib.util.spec_from_file_location("audit_repository", MODULE_PATH)
audit = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(audit)


class AuditRepositoryTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        (self.root / "source.cpp").write_text("", encoding="utf-8")
        (self.root / "test.cpp").write_text("", encoding="utf-8")
        self.data = {
            "schema_version": 1, "audit_date": "2026-10-24", "scope": "test fixture",
            "upstreams": {"upstream": {"revision": "a" * 40, "url": "https://example.com/repo"}},
            "features": [{"id": "FEATURE-1", "area": "Core", "title": "A feature",
                          "status": "implemented", "priority": "P1", "verification": "pending-runtime",
                          "upstream": ["upstream"], "source": ["source.cpp"], "tests": ["test.cpp"],
                          "examples": [], "gap": "Not executed", "acceptance": "Execute the test"}],
        }

    def test_valid_inventory_is_not_execution_evidence(self):
        self.assertEqual(audit.validate_matrix(self.data, self.root), [])
        self.assertIn("runtime verification is pending", audit.render_matrix(self.data))

    def test_false_execution_claim_and_duplicate_fail(self):
        self.data["features"].append(copy.deepcopy(self.data["features"][0]))
        self.data["features"][0]["verification"] = "passed"
        errors = audit.validate_matrix(self.data, self.root)
        self.assertTrue(any("execution claims" in error for error in errors))
        self.assertTrue(any("duplicate id" in error for error in errors))

    def test_missing_evidence_and_escape_fail(self):
        self.data["features"][0]["source"] = ["../outside.cpp", "missing.cpp"]
        self.data["features"][0]["tests"] = []
        errors = audit.validate_matrix(self.data, self.root)
        self.assertTrue(any("escapes" in error for error in errors))
        self.assertTrue(any("does not exist" in error for error in errors))
        self.assertTrue(any("needs tests" in error for error in errors))

    def test_malformed_schema_and_empty_inventory_fail(self):
        self.assertTrue(audit.validate_matrix([], self.root))
        self.data["features"] = []
        self.assertTrue(audit.validate_matrix(self.data, self.root))

    def test_malformed_metadata_is_reported_without_crashing(self):
        self.data["audit_date"] = []
        self.data["upstreams"]["upstream"]["revision"] = 42
        self.data["features"][0]["status"] = []
        self.data["features"][0]["priority"] = {}
        self.data["features"][0]["upstream"] = [[]]
        self.assertGreaterEqual(len(audit.validate_matrix(self.data, self.root)), 5)

    def test_code_fences_do_not_create_links(self):
        text = "[real](source.cpp)\n```lua\n[fake](missing.cpp)\n```\n![image](picture.png)"
        self.assertEqual(list(audit.markdown_links(text)), ["source.cpp", "picture.png"])

    def test_published_page_cannot_link_outside_docs(self):
        # `mkdocs build --strict` fails a link that leaves docs_dir even when the target
        # exists, because that file is never copied into the site - the CI that builds the
        # docs treats it as an error. A page at the repository root is rendered by GitHub
        # instead, where a link into the source tree is exactly right.
        (self.root / "Docs/CN").mkdir(parents=True)
        (self.root / "third_party.md").write_text("", encoding="utf-8")
        (self.root / "Docs/CN/page.md").write_text("[versions](../../third_party.md)", encoding="utf-8")
        (self.root / "readme.md").write_text("[versions](third_party.md)", encoding="utf-8")
        errors = audit.validate_docs(self.root)
        self.assertEqual(len(errors), 1)
        self.assertIn("mkdocs", errors[0])

    def test_broken_nav_and_encoded_local_link_fail(self):
        docs = self.root / "Docs"
        docs.mkdir()
        (docs / "index.md").write_text("[missing](missing%20page.md)\n[up](../source.cpp)", encoding="utf-8")
        (self.root / "mkdocs.yml").write_text("nav:\n  - Missing: absent.md\n", encoding="utf-8")
        errors = audit.validate_docs(self.root)
        # The encoded target that is missing, the page that links out of Docs/ (the file is
        # there, but MkDocs never publishes it), and the missing nav page.
        self.assertEqual(len(errors), 3)


    def _write_release_pages(self, status="not-passed", commit="347bb8c5"):
        for page in audit.RELEASE_PAGES:
            path = self.root / page
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(
                "# page\n\n"
                f"<!-- release-status: {status} -->\n"
                f"<!-- release-provenance: {commit} -->\n\n"
                "33/33 2482/2482 25%\n\n"
                "已提交 / are committed\n",
                encoding="utf-8",
            )

    def _write_summary(self, kind, payload):
        path = self.root / "Saved" / "ReleaseTests" / kind / "release-summary.json"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(payload), encoding="utf-8")
        return path

    def test_release_pages_must_agree_with_each_other(self):
        self._write_release_pages()
        self.assertEqual(audit.validate_release_docs(self.root), [])

    def test_missing_or_conflicting_release_markers_fail(self):
        self._write_release_pages()
        page = self.root / "README_EN.md"
        page.write_text(page.read_text(encoding="utf-8").replace(
            "release-status: not-passed", "release-status: passed"), encoding="utf-8")
        errors = audit.validate_release_docs(self.root)
        self.assertTrue(any("release status markers disagree" in error for error in errors))
        (self.root / "Docs/EN/ReleaseNotes.md").unlink()
        self.assertTrue(any("release status page is missing" in error
                            for error in audit.validate_release_docs(self.root)))

    def test_stale_en_performance_claim_fails(self):
        self._write_release_pages()
        page = self.root / "Docs/EN/Performance_Parity_slua.md"
        page.write_text(page.read_text(encoding="utf-8")
                        + "\nTools/baseline/perf-*.json is not committed.\n", encoding="utf-8")
        errors = audit.validate_release_docs(self.root)
        self.assertTrue(any("conflicts with the CN page" in error for error in errors))

    def test_latest_finished_run_decides_status(self):
        self._write_release_pages()
        path = self._write_summary("test", {"complete": True, "passed": False})
        self.assertEqual(audit.validate_release_docs(self.root), [])
        path.write_text(json.dumps({"complete": True, "passed": True}), encoding="utf-8")
        errors = audit.validate_release_docs(self.root)
        self.assertTrue(any("disagrees with the latest run" in error for error in errors))
        path.write_text(json.dumps({"complete": False, "passed": False}), encoding="utf-8")
        self.assertEqual(audit.validate_release_docs(self.root), [])

    def test_baseline_summary_must_match_published_numbers(self):
        self._write_release_pages()
        summary = self._write_summary("baseline", {
            "provenance": {"commit": "347bb8c5" + "0" * 32},
            "stages": [{} for _ in range(33)],
            "automation": [{"total": 626}, {"total": 600}, {"total": 641}, {"total": 615}],
        })
        self.assertEqual(audit.validate_release_docs(self.root), [])
        summary.write_text(json.dumps({
            "provenance": {"commit": "deadbeef" + "0" * 32},
            "stages": [{} for _ in range(32)],
            "automation": [{"total": 626}, {"total": 600}, {"total": 641}, {"total": 615}],
        }), encoding="utf-8")
        errors = audit.validate_release_docs(self.root)
        self.assertTrue(any("do not match baseline commit" in error for error in errors))
        self.assertTrue(any("33/33 claim disagrees" in error for error in errors))


if __name__ == "__main__":
    unittest.main()

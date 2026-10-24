import copy
import importlib.util
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

    def test_broken_nav_and_encoded_local_link_fail(self):
        docs = self.root / "Docs"
        docs.mkdir()
        (docs / "index.md").write_text("[missing](missing%20page.md)\n[up](../source.cpp)", encoding="utf-8")
        (self.root / "mkdocs.yml").write_text("nav:\n  - Missing: absent.md\n", encoding="utf-8")
        errors = audit.validate_docs(self.root)
        self.assertEqual(len(errors), 2)


if __name__ == "__main__":
    unittest.main()

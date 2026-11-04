"""Regression tests for the fail-closed Luau coverage gate (no UE needed)."""

import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import ValidateCoverage  # noqa: E402


def report(lines=None, source="_G.UnLuaNova.Event", level=1, exclusions=None, totals=None):
    lines = [{"line": 1, "hits": 1}, {"line": 2, "hits": 0}] if lines is None else lines
    document = {
        "schema_version": 1,
        "backend": "Luau",
        "coverage_level": level,
        "generated_utc": "2026-11-03T00:00:00Z",
        "exclusions": ["IntelliSense", "<generated>"] if exclusions is None else exclusions,
        "sources": [{
            "source": source,
            "functions": [{
                "name": "Emit", "linedefined": 1, "depth": 0, "lines": lines,
            }],
        }],
    }
    if totals is None:
        totals = {
            "functions": 1,
            "covered_functions": 1 if any(line.get("hits", 0) > 0 for line in lines) else 0,
            "instrumented_lines": len(lines),
            "hit_lines": sum(1 for line in lines if line.get("hits", 0) > 0),
        }
    document["totals"] = totals
    return document


class CoverageGateTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def write(self, name, document):
        path = self.root / name
        path.write_text(json.dumps(document), encoding="utf-8")
        return path

    def check(self, *paths, **kwargs):
        return ValidateCoverage.validate([str(path) for path in paths], **kwargs)

    def test_valid_report_passes(self):
        result = self.check(self.write("ok.json", report()))
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["instrumented_lines"], 2)
        self.assertEqual(result["hit_lines"], 1)
        self.assertEqual(result["coverage"], 0.5)

    def test_missing_report_fails_closed(self):
        result = self.check(self.root / "absent.json")
        self.assertFalse(result["passed"])
        self.assertTrue(any("cannot read" in error for error in result["errors"]))

    def test_empty_document_fails(self):
        path = self.root / "empty.json"
        path.write_text(" ", encoding="utf-8")
        self.assertFalse(self.check(path)["passed"])

    def test_no_sources_fails(self):
        document = report()
        document["sources"] = []
        document["totals"] = {"functions": 0, "covered_functions": 0,
                              "instrumented_lines": 0, "hit_lines": 0}
        result = self.check(self.write("empty-sources.json", document))
        self.assertFalse(result["passed"])
        self.assertTrue(any("no sources" in error for error in result["errors"]))

    def test_totals_are_recomputed_not_trusted(self):
        document = report()
        document["totals"]["hit_lines"] = 999
        result = self.check(self.write("lying.json", document))
        self.assertFalse(result["passed"])
        self.assertTrue(any("totals.hit_lines" in error for error in result["errors"]))

    def test_backend_mismatch_fails(self):
        document = report()
        document["backend"] = "Lua"
        self.assertFalse(self.check(self.write("backend.json", document))["passed"])

    def test_level_zero_fails(self):
        result = self.check(self.write("level0.json", report(level=0)))
        self.assertFalse(result["passed"])
        self.assertTrue(any("coverage_level" in error for error in result["errors"]))

    def test_negative_hits_and_duplicate_lines_fail(self):
        bad_hits = self.check(self.write("hits.json", report(lines=[{"line": 1, "hits": -1}])))
        self.assertFalse(bad_hits["passed"])
        duplicate = self.check(self.write("dup.json", report(
            lines=[{"line": 1, "hits": 1}, {"line": 1, "hits": 2}])))
        self.assertFalse(duplicate["passed"])

    def test_undeclared_exclusions_fail(self):
        result = self.check(self.write("no-excl.json", report(exclusions=[])))
        self.assertFalse(result["passed"])
        self.assertTrue(any("exclusions" in error for error in result["errors"]))

    def test_required_source_must_be_present(self):
        path = self.write("ok.json", report())
        self.assertTrue(self.check(path, required_sources=["_G.UnLuaNova.Event"])["passed"])
        result = self.check(path, required_sources=["_G.UnLuaNova.Timer"])
        self.assertFalse(result["passed"])
        self.assertTrue(any("required source absent" in error for error in result["errors"]))

    def test_min_coverage_is_a_real_gate(self):
        path = self.write("ok.json", report())
        self.assertFalse(self.check(path, min_coverage=0.9)["passed"])

    def test_excluded_source_is_not_silently_counted(self):
        document = report()
        # totals covers the whole document; only the non-excluded source feeds the gate's
        # denominator, so a report made entirely of exclusions must still fail closed.
        document["sources"].append(report(source="_G.IntelliSense.Generated")["sources"][0])
        document["totals"] = {"functions": 2, "covered_functions": 2,
                              "instrumented_lines": 4, "hit_lines": 2}
        path = self.write("excluded.json", document)
        result = self.check(path)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["instrumented_lines"], 2)
        self.assertEqual(result["hit_lines"], 1)
        self.assertIn("_G.IntelliSense.Generated", result["excluded_sources"])

        all_excluded = self.write("only-excluded.json", report(source="_G.IntelliSense.Generated"))
        self.assertFalse(self.check(all_excluded)["passed"])

    def test_aggregation_takes_max_hits_per_line(self):
        first = self.write("a.json", report(lines=[{"line": 1, "hits": 0}, {"line": 2, "hits": 0}]))
        second = self.write("b.json", report(lines=[{"line": 1, "hits": 3}, {"line": 2, "hits": 0}]))
        result = self.check(first, second)
        self.assertTrue(result["passed"], result["errors"])
        self.assertEqual(result["instrumented_lines"], 2)
        self.assertEqual(result["hit_lines"], 1)
        self.assertEqual(result["coverage"], 0.5)

    def test_cli_writes_gate_and_returns_pass_or_fail(self):
        good = self.write("ok.json", report())
        gate = self.root / "coverage-gate.json"
        self.assertEqual(ValidateCoverage.main(["--report", str(good), "--out", str(gate)]), 0)
        self.assertTrue(json.loads(gate.read_text(encoding="utf-8"))["passed"])
        bad = self.write("bad.json", report(level=0))
        self.assertEqual(ValidateCoverage.main(["--report", str(bad)]), 1)

    def test_plain_lua_backend_is_refused(self):
        good = self.write("ok.json", report())
        self.assertEqual(ValidateCoverage.main(["--report", str(good), "--backend", "Lua"]), 1)


if __name__ == "__main__":
    unittest.main()

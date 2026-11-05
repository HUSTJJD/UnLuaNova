"""Regression tests for the reproducible NOTICES.txt generator (no UE needed).

The fixture is a minimal repository: a root LICENSE, a .gitmodules with two
submodules, an inventory with two components that each ship a notice file, and
three "open provenance" resolver files. With all of those present the generator
is clean (zero UNRESOLVED), which lets --check pass; individual tests then break
one thing at a time to exercise the fail-closed behaviour.
"""

import contextlib
import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import GenerateNotices as G  # noqa: E402


FIXTURE_LICENSE = """FixtureProject
Copyright (c) 2026 Fixture

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
"""

FIXTURE_GITMODULES = """[submodule "sub/alpha"]
\tpath = sub/alpha
\turl = https://example.invalid/alpha.git
[submodule "sub/beta"]
\tpath = sub/beta
\turl = https://example.invalid/beta.git
"""

FIXTURE_THIRD_PARTY = """# Third-party Sources and Notices

## Included Runtime Components

| Component | Location / source | License and attribution evidence |
|---|---|---|
| AlphaLib 1.2.3 | `third_party/alpha` | MIT; see headers |
| BetaLib 2.0 | `third_party/beta` | MIT; see headers |

## Version identifiers verified in-tree

| Component | Identifier | Evidence |
|---|---|---|
| AlphaLib | 1.2.3 | alpha.h:1 |
"""

FIXTURE_THIRD_PARTY_ESCAPE = """# Third-party Sources and Notices

## Included Runtime Components

| Component | Location / source | License and attribution evidence |
|---|---|---|
| EvilLib 9.9 | `../../etc/passwd` | MIT; see headers |
"""

COMPONENT_NOTICE = "Fixture component notice (placeholder, not a legal conclusion).\n"
RESOLVER_CONTENT = "Evidence placeholder; content not verified by the generator.\n"


class GenerateNoticesTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    # ---------- fixture helpers ----------

    def write(self, rel, text):
        path = self.root / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(str(path), "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        return path

    def make_clean(self):
        """A fully resolvable fixture: no errors, no UNRESOLVED, 5 entries."""
        self.write("LICENSE", FIXTURE_LICENSE)
        self.write(".gitmodules", FIXTURE_GITMODULES)
        self.write("LICENSE-THIRD-PARTY.md", FIXTURE_THIRD_PARTY)
        self.write("third_party/alpha/LICENSE", COMPONENT_NOTICE)
        self.write("third_party/beta/LICENSE", COMPONENT_NOTICE)
        for gap in G.DECLARED_GAPS:
            self.write(gap["resolver"], RESOLVER_CONTENT)

    def invoke(self, *argv):
        out, err = io.StringIO(), io.StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = G.main(list(argv))
        return rc, out.getvalue(), err.getvalue()

    def read_report(self):
        with open(str(self.root / "report.json"), encoding="utf-8") as handle:
            return json.load(handle)

    # ---------- determinism / structure ----------

    def test_stable_output_and_sha256(self):
        self.make_clean()
        first = G.build_manifest(self.root)
        second = G.build_manifest(self.root)
        self.assertEqual(first["text"], second["text"])
        self.assertEqual(first["sha256"], second["sha256"])
        self.assertEqual(first["entries"], second["entries"])
        self.assertEqual(first["unresolved"], second["unresolved"])
        self.assertEqual(first["sha256"],
                         hashlib.sha256(first["body"].encode("utf-8")).hexdigest())

    def test_entries_sorted_by_name(self):
        self.make_clean()
        entries = G.build_manifest(self.root)["entries"]
        names = [line.split(" | ")[1] for line in entries]
        self.assertEqual(names, sorted(names))
        self.assertGreater(len(entries), 1)

    def test_sha256_is_body_hash_not_self_referential(self):
        self.make_clean()
        result = G.build_manifest(self.root)
        self.assertEqual(result["sha256"],
                         hashlib.sha256(result["body"].encode("utf-8")).hexdigest())
        self.assertNotIn(result["sha256"], result["body"])
        self.assertIn(result["sha256"], result["text"])
        self.assertTrue(result["text"].startswith(result["body"]))

    # ---------- missing required inputs ----------

    def test_missing_license_fails(self):
        self.make_clean()
        (self.root / "LICENSE").unlink()
        rc, _, _ = self.invoke("--root", str(self.root), "--report", "report.json")
        self.assertEqual(rc, 1)
        self.assertTrue(any("missing required file: LICENSE" in e
                            for e in self.read_report()["errors"]))

    def test_missing_gitmodules_fails(self):
        self.make_clean()
        (self.root / ".gitmodules").unlink()
        rc, _, _ = self.invoke("--root", str(self.root), "--report", "report.json")
        self.assertEqual(rc, 1)
        self.assertTrue(any("missing required file: .gitmodules" in e
                            for e in self.read_report()["errors"]))

    def test_missing_third_party_fails(self):
        self.make_clean()
        (self.root / "LICENSE-THIRD-PARTY.md").unlink()
        rc, _, _ = self.invoke("--root", str(self.root), "--report", "report.json")
        self.assertEqual(rc, 1)
        self.assertTrue(any("missing required file: LICENSE-THIRD-PARTY.md" in e
                            for e in self.read_report()["errors"]))

    # ---------- fail-closed UNRESOLVED ----------

    def test_missing_component_notice_is_unresolved_and_fail_closed(self):
        self.make_clean()
        (self.root / "third_party/beta/LICENSE").unlink()
        rc, _, _ = self.invoke("--root", str(self.root), "--report", "report.json")
        self.assertEqual(rc, 1)
        report = self.read_report()
        self.assertTrue(any("missing notice for BetaLib 2.0" in u
                            for u in report["unresolved"]))
        self.assertFalse(report["ok"])
        self.assertGreaterEqual(len(report["unresolved"]), 1)
        # --check is a release gate: it must fail closed on UNRESOLVED even when
        # the compared file is byte-for-byte identical to the generated list.
        text = G.build_manifest(self.root)["text"]
        self.write("NOTICES.txt", text)
        rc2, _, _ = self.invoke("--root", str(self.root), "--check", "NOTICES.txt")
        self.assertEqual(rc2, 1)

    def test_allow_unresolved_still_lists_unresolved(self):
        self.make_clean()
        (self.root / "third_party/beta/LICENSE").unlink()
        # Default view: succeed, still print the declaration and UNRESOLVED lines.
        rc, out, _ = self.invoke("--root", str(self.root), "--allow-unresolved")
        self.assertEqual(rc, 0)
        self.assertIn(G.DECLARATION, out)
        self.assertIn("UNRESOLVED:", out)
        # With --out/--report: content still lists them, report is still honest.
        rc2, _, _ = self.invoke("--root", str(self.root), "--allow-unresolved",
                             "--out", "NOTICES.txt", "--report", "report.json")
        self.assertEqual(rc2, 0)
        self.assertIn("UNRESOLVED:",
                      (self.root / "NOTICES.txt").read_text(encoding="utf-8"))
        report = self.read_report()
        self.assertTrue(any("missing notice for BetaLib 2.0" in u
                            for u in report["unresolved"]))
        self.assertFalse(report["ok"])  # strict gate result stays False

    # ---------- --check ----------

    def test_check_matching_file_passes(self):
        self.make_clean()
        text = G.build_manifest(self.root)["text"]
        self.write("NOTICES.txt", text)
        rc, out, _ = self.invoke("--root", str(self.root), "--check", "NOTICES.txt")
        self.assertEqual(rc, 0)
        self.assertIn("CHECK NOTICES.txt: OK", out)

    def test_check_truncated_file_fails_with_diff(self):
        self.make_clean()
        text = G.build_manifest(self.root)["text"]
        truncated = "\n".join(text.splitlines()[:10]) + "\n"
        self.write("NOTICES.txt", truncated)
        rc, out, _ = self.invoke("--root", str(self.root), "--check", "NOTICES.txt",
                              "--report", "report.json")
        self.assertEqual(rc, 1)
        self.assertIn("MISMATCH at line", out)
        check = self.read_report()["check_results"][0]
        self.assertFalse(check["ok"])
        self.assertIsNotNone(check["diff_line"])

    def test_check_two_files_one_modified_fails(self):
        self.make_clean()
        text = G.build_manifest(self.root)["text"]
        self.write("A.txt", text)
        self.write("B.txt", text)
        with open(str(self.root / "B.txt"), "a", encoding="utf-8", newline="\n") as handle:
            handle.write("EXTRA LINE\n")
        rc, out, _ = self.invoke("--root", str(self.root), "--check", "A.txt", "B.txt",
                              "--report", "report.json")
        self.assertEqual(rc, 1)
        self.assertIn("CHECK A.txt: OK", out)
        self.assertIn("CHECK B.txt: MISMATCH", out)
        results = self.read_report()["check_results"]
        self.assertTrue(results[0]["ok"])
        self.assertFalse(results[1]["ok"])
        # Equivalent repeated-flag form (--check A --check B) must behave the same.
        rc2, out2, _ = self.invoke("--root", str(self.root),
                                   "--check", "A.txt", "--check", "B.txt")
        self.assertEqual(rc2, 1)
        self.assertIn("CHECK A.txt: OK", out2)
        self.assertIn("CHECK B.txt: MISMATCH", out2)

    def test_check_rejects_allow_unresolved(self):
        self.make_clean()
        text = G.build_manifest(self.root)["text"]
        self.write("NOTICES.txt", text)
        rc, _, err = self.invoke("--root", str(self.root), "--check", "NOTICES.txt",
                              "--allow-unresolved")
        self.assertEqual(rc, 1)
        self.assertIn("cannot be combined", err)

    # ---------- --root confinement ----------

    def test_root_path_escape_rejected(self):
        self.make_clean()
        rc, out, err = self.invoke("--root", str(self.root), "--check", "../escape.txt")
        self.assertEqual(rc, 1)
        self.assertIn("REJECTED", out)
        self.assertTrue("escape" in out.lower() or "escape" in err.lower())

    def test_component_location_escape_rejected(self):
        self.make_clean()
        self.write("LICENSE-THIRD-PARTY.md", FIXTURE_THIRD_PARTY_ESCAPE)
        result = G.build_manifest(self.root)
        self.assertTrue(any("escape" in u.lower() for u in result["unresolved"]))
        rc, _, _ = self.invoke("--root", str(self.root), "--report", "report.json")
        self.assertEqual(rc, 1)

    # ---------- --report / --out ----------

    def test_report_fields_and_counts(self):
        self.make_clean()
        rc, _, _ = self.invoke("--root", str(self.root), "--report", "report.json")
        self.assertEqual(rc, 0)
        report = self.read_report()
        for key in ("ok", "sources", "unresolved", "entries", "sha256", "errors"):
            self.assertIn(key, report)
        self.assertTrue(report["ok"])
        # 1 ROOT-LICENSE + 2 SUBMODULE + 2 COMPONENT = 5 entries.
        self.assertEqual(len(report["entries"]), 5)
        self.assertEqual(len(report["unresolved"]), 0)
        self.assertEqual(report["unresolved_count"], 0)
        self.assertEqual(len(report["sha256"]), 64)
        for rel in ("LICENSE", ".gitmodules", "LICENSE-THIRD-PARTY.md"):
            self.assertIn(rel, report["sources"])

    def test_out_writes_lf_newlines(self):
        self.make_clean()
        expected = G.build_manifest(self.root)["text"]
        rc, _, _ = self.invoke("--root", str(self.root), "--out", "NOTICES.txt")
        self.assertEqual(rc, 0)
        raw = (self.root / "NOTICES.txt").read_bytes()
        self.assertNotIn(b"\r\n", raw)
        self.assertEqual(raw.decode("utf-8"), expected)


if __name__ == "__main__":
    unittest.main()

"""Unit tests for ``Tools/CheckBuildLayoutConsistency.py``.

Every scenario is built inside a ``tempfile.TemporaryDirectory``: no real ``*.Build.cs``
is read or modified. Branches covered:

  * identical blocks -> PASS (exit 0)
  * a drifted copy  -> FAIL (exit 1) with differing files and the first differing line
  * a missing block -> FAIL (exit 1) with the offending file listed
  * block count != expected 6 -> WARNING but still PASS (non-fatal)
  * CRLF / CR line endings are normalized before comparison
  * ThirdParty / vendored Build.cs are excluded (and their drift cannot fail the gate)
  * marker extraction is verbatim and includes the comment markers
"""

from __future__ import annotations

import contextlib
import io
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import CheckBuildLayoutConsistency as chk  # noqa: E402


# A representative [UAK BACKEND LAYOUT] block. The two marker lines mirror the real ones
# (they are "//" comment lines); the middle lines stand in for the real C# code. The
# checker only cares that copies are byte-identical, so the exact body is not important.
BEGIN = "        // [UAK BACKEND LAYOUT - keep this block identical in every Build.cs]"
END = "        // [/UAK BACKEND LAYOUT]"
BODY = [
    '        const string backendTargetPrefix = "UnLuaNovaDemo";',
    '        string backendVariant = "";',
    "        if (Target.Name.StartsWith(backendTargetPrefix))",
    "        {",
    '            backendVariant = "Lua";',
    "        }",
    "        if (backendVariant.Length > 0)",
    "        {",
    '            BinariesSubFolder = "Backends/" + backendVariant;',
    "        }",
]
CANONICAL = "\n".join([BEGIN] + BODY + [END])

# A one-line drift used to make a copy inconsistent.
DRIFTED_LINE = '        string backendVariant = "X";'
DRIFTED = CANONICAL.replace('        string backendVariant = "";', DRIFTED_LINE)


def _write_build_cs(root: Path, relpath: str, block=None, eol: bytes = b"\n") -> Path:
    """Write a fake ``*.Build.cs``; ``block=None`` means no layout block at all."""
    path = root / relpath
    path.parent.mkdir(parents=True, exist_ok=True)
    inner = ("// intentionally no layout block here\n" if block is None else block + "\n")
    text = (
        "// fake module for unit tests\n"
        "using UnrealBuildTool;\n"
        "public class Fake : ModuleRules\n"
        "{\n"
        "    public Fake(ReadOnlyTargetRules Target) : base(Target)\n"
        "    {\n"
        + inner
        + "    }\n"
        "}\n"
    )
    data = text.encode("utf-8")
    if eol != b"\n":
        data = data.replace(b"\n", eol)
    # write_bytes avoids OS newline translation so the eol is exactly what we asked for.
    path.write_bytes(data)
    return path


class BuildLayoutConsistencyTests(unittest.TestCase):

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        # The checker calls Path.resolve(); on Windows TemporaryDirectory may hand back an
        # 8.3 short path (DAVIDS~1) that resolves to the long form. Resolve here too so
        # expected paths compare equal to the ones the checker discovers.
        self.root = Path(self.tmp.name).resolve()

    def _run_main(self, root: Path):
        buf = io.StringIO()
        with contextlib.redirect_stdout(buf):
            code = chk.main([str(root)])
        return code, buf.getvalue()

    # ---- consistent -----------------------------------------------------------
    def test_identical_blocks_pass(self):
        for i in range(chk.EXPECTED_BLOCK_COUNT):
            _write_build_cs(self.root, f"Mod{i}/Source/Mod{i}.Build.cs", CANONICAL)
        result = chk.check(self.root)
        self.assertTrue(result.ok, result.errors)
        self.assertEqual([], result.missing)
        self.assertEqual([], result.mismatches)
        self.assertEqual(1, len(result.groups))
        self.assertEqual(chk.EXPECTED_BLOCK_COUNT, len(result.blocks))
        self.assertEqual([], result.warnings)  # exactly 6 -> no count warning
        code, out = self._run_main(self.root)
        self.assertEqual(0, code)
        self.assertIn("RESULT: PASS", out)

    def test_line_endings_are_normalized(self):
        _write_build_cs(self.root, "A/Source/A.Build.cs", CANONICAL, eol=b"\n")
        _write_build_cs(self.root, "B/Source/B.Build.cs", CANONICAL, eol=b"\r\n")
        _write_build_cs(self.root, "C/Source/C.Build.cs", CANONICAL, eol=b"\r")
        result = chk.check(self.root)
        self.assertTrue(result.ok, result.errors)
        self.assertEqual(1, len(result.groups), "LF/CRLF/CR copies must compare equal")
        self.assertEqual(3, len(result.blocks))

    def test_extract_block_is_verbatim_and_includes_markers(self):
        text = "// leading noise\n" + CANONICAL + "\n// trailing noise\n"
        block, reason = chk.extract_block(text)
        self.assertEqual("", reason)
        self.assertEqual(CANONICAL, block)
        self.assertTrue(block.startswith(BEGIN))
        self.assertTrue(block.endswith(END))

    # ---- inconsistent ---------------------------------------------------------
    def test_drifted_block_fails_and_reports_first_diff_line(self):
        self.assertNotEqual(CANONICAL, DRIFTED)
        _write_build_cs(self.root, "A/Source/A.Build.cs", CANONICAL)
        _write_build_cs(self.root, "B/Source/B.Build.cs", CANONICAL)
        _write_build_cs(self.root, "C/Source/C.Build.cs", DRIFTED)

        result = chk.check(self.root)
        self.assertFalse(result.ok)
        self.assertEqual(2, len(result.groups))
        self.assertEqual(1, len(result.mismatches))
        m = result.mismatches[0]
        self.assertEqual(self.root / "C/Source/C.Build.cs", m["path"])
        # Block lines: 1 BEGIN, 2 prefix const, 3 the backendVariant line we changed.
        self.assertEqual(3, m["line_no"])
        self.assertIn('""', m["canonical_line"])
        self.assertIn('"X"', m["file_line"])
        self.assertTrue(any("inconsistent" in e for e in result.errors))

        code, out = self._run_main(self.root)
        self.assertEqual(1, code)
        self.assertIn("RESULT: FAIL", out)
        self.assertIn("MISMATCH", out)
        self.assertIn("first differing line", out)
        self.assertIn("C", out)

    def test_all_copies_differ_still_reports_each_file(self):
        # Two distinct drifted variants -> two differing files, both reported.
        drift2 = CANONICAL.replace('        string backendVariant = "";',
                                   '        string backendVariant = "Y";')
        _write_build_cs(self.root, "A/Source/A.Build.cs", CANONICAL)
        _write_build_cs(self.root, "B/Source/B.Build.cs", DRIFTED)
        _write_build_cs(self.root, "C/Source/C.Build.cs", drift2)
        result = chk.check(self.root)
        self.assertFalse(result.ok)
        self.assertEqual(3, len(result.groups))
        self.assertEqual(2, len(result.mismatches))

    # ---- missing / malformed --------------------------------------------------
    def test_missing_block_fails_and_lists_file(self):
        _write_build_cs(self.root, "A/Source/A.Build.cs", CANONICAL)
        _write_build_cs(self.root, "B/Source/B.Build.cs", None)  # no block
        result = chk.check(self.root)
        self.assertFalse(result.ok)
        self.assertEqual(1, len(result.missing))
        self.assertEqual(self.root / "B/Source/B.Build.cs", result.missing[0][0])
        code, out = self._run_main(self.root)
        self.assertEqual(1, code)
        self.assertIn("RESULT: FAIL", out)
        self.assertIn("MISSING", out)
        self.assertIn("B", out)

    def test_begin_without_end_is_reported_as_malformed(self):
        broken = CANONICAL.replace(END, "        // lost the end marker")
        _write_build_cs(self.root, "A/Source/A.Build.cs", broken)
        result = chk.check(self.root)
        self.assertFalse(result.ok)
        self.assertEqual(1, len(result.missing))

    # ---- exclusion ------------------------------------------------------------
    def test_thirdparty_and_vendored_files_are_excluded(self):
        _write_build_cs(self.root, "Mod/Source/Mod.Build.cs", CANONICAL)
        # A ThirdParty Build.cs with NO block must not be flagged as missing ...
        _write_build_cs(self.root, "Plugin/Source/ThirdParty/Lib/Lib.Build.cs", None)
        # ... and a drifted ThirdParty/vendored block must not cause a mismatch.
        _write_build_cs(self.root, "Plugin/Source/ThirdParty/Dep/Dep.Build.cs", DRIFTED)
        _write_build_cs(self.root, "Tools/vendored/Tool/Tool.Build.cs", DRIFTED)
        result = chk.check(self.root)
        self.assertTrue(result.ok, result.errors)
        self.assertEqual([], result.missing)
        self.assertEqual([], result.mismatches)
        self.assertEqual(1, len(result.scanned))
        self.assertEqual(3, len(result.excluded))

    # ---- non-fatal count warning ---------------------------------------------
    def test_fewer_than_expected_is_warning_not_failure(self):
        for i in range(3):
            _write_build_cs(self.root, f"Mod{i}/Source/Mod{i}.Build.cs", CANONICAL)
        result = chk.check(self.root)
        self.assertTrue(result.ok)
        self.assertEqual(1, len(result.warnings))
        self.assertIn("expected", result.warnings[0])
        code, out = self._run_main(self.root)
        self.assertEqual(0, code)  # warning must NOT fail the gate
        self.assertIn("WARNING:", out)
        self.assertIn("RESULT: PASS", out)

    def test_more_than_expected_is_warning_not_failure(self):
        # Mirrors the real repository (7 blocks found, 6 expected): still a pass.
        for i in range(7):
            _write_build_cs(self.root, f"Mod{i}/Source/Mod{i}.Build.cs", CANONICAL)
        result = chk.check(self.root)
        self.assertTrue(result.ok)
        self.assertEqual(1, len(result.warnings))
        code, out = self._run_main(self.root)
        self.assertEqual(0, code)
        self.assertIn("WARNING:", out)
        self.assertIn("RESULT: PASS", out)


if __name__ == "__main__":
    unittest.main()

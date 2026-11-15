"""Unit tests for the Android/iOS acceptance tooling.

Scope (all static / pure-Python, no Unreal Engine, no device, no PowerShell needed):

  * the PowerShell script contains the required precheck items and the verdict enum
  * the new code files contain no hardcoded machine-local absolute paths, using the
    very same pattern set as Tools/CheckNoLocalPaths.py
  * "a failing precheck must never produce passed" is enforced both as a static
    text contract on the script and as an invariant over constructed JSON results

These tests intentionally do NOT execute RunAndroidAcceptance.ps1: doing so would
either need PowerShell (absent on the Linux CI runner) or write into Saved/. The
-PrecheckOnly run is verified separately on Windows by the maintainer.
"""

from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "Tools"
sys.path.insert(0, str(TOOLS))
import CheckNoLocalPaths as chk  # noqa: E402

SCRIPT_REL = "Tools/RunAndroidAcceptance.ps1"
WORKFLOW_REL = ".github/workflows/mobile-acceptance.yml"
SELF_REL = "Tools/tests/test_mobile_acceptance.py"

SCRIPT = (ROOT / SCRIPT_REL).read_text(encoding="utf-8-sig")
WORKFLOW = (ROOT / WORKFLOW_REL).read_text(encoding="utf-8-sig")

VERDICT_ENUM = ("passed", "failed", "not-run")

# Precheck items and orchestration tokens that the script MUST mention. Each maps
# to a concrete line in RunAndroidAcceptance.ps1; keeping them here makes a
# silent regression (e.g. dropping the adb probe) fail this gate.
REQUIRED_TOKENS = (
    "ANDROID_HOME",
    "NDKROOT",
    "JAVA_HOME",
    "adb devices",
    "UE Android platform SDK",
    "Build.version",
    "arm64",
    "RunUAT.bat",
    "BuildCookRun",
    "-platform=Android",
    "-cook",
    "-stage",
    "-package",
    "Content/Script/",
    "Saved/MobileAcceptance",
    "verdict_enum",
    "-PrecheckOnly",
    "-SkipBuild",
)


def compute_verdict(result):
    """Faithful port of Resolve-Verdict in RunAndroidAcceptance.ps1.

    Kept here so JSON-level scenarios can be checked against the exact decision
    rule; test_hard_rule_static() verifies the script itself encodes this rule.
    """
    precheck, build = result["precheck"], result["build"]
    artifacts, smoke = result["artifacts"], result["smoke"]
    if not precheck["ok"]:
        return "not-run"  # HARD RULE: never passed on a failing precheck
    if build["status"] == "failed":
        return "failed"
    if build["status"] != "passed":
        return "not-run"
    if not artifacts["ok"]:
        return "failed"
    if smoke["status"] != "passed":
        return "not-run"
    return "passed"


def assert_contract(testcase, result):
    """The honesty contract every emitted JSON result must satisfy."""
    testcase.assertIn(result["verdict"], VERDICT_ENUM)
    # A failing precheck must never produce passed.
    if not result["precheck"]["ok"]:
        testcase.assertNotEqual(result["verdict"], "passed",
                                "a failing precheck produced passed")
    # passed is only credible when every section is genuinely green.
    if result["verdict"] == "passed":
        testcase.assertTrue(result["precheck"]["ok"])
        testcase.assertEqual(result["build"]["status"], "passed")
        testcase.assertTrue(result["artifacts"]["ok"])
        testcase.assertEqual(result["smoke"]["status"], "passed")


def _base_result(precheck_ok=True, build="not-run", artifacts_ok=False, smoke="not-run"):
    return {
        "precheck": {"ok": precheck_ok, "reasons": [] if precheck_ok else ["adb devices: 0 device(s)"]},
        "build": {"status": build},
        "artifacts": {"ok": artifacts_ok, "package": None, "bytes": 0, "has_script_assets": False},
        "smoke": {"status": smoke},
        "verdict": "not-run",
        "verdict_enum": list(VERDICT_ENUM),
    }


class TestMobileAcceptance(unittest.TestCase):

    def test_required_prechecks_present(self):
        missing = [tok for tok in REQUIRED_TOKENS if tok not in SCRIPT]
        self.assertEqual(missing, [], "missing required precheck/orchestration tokens: %s" % missing)

    def test_verdict_enum_present(self):
        for value in VERDICT_ENUM:
            self.assertIn("'%s'" % value, SCRIPT)
        self.assertIn("verdict_enum", SCRIPT)
        self.assertIn("$AllowedVerdicts = @('passed', 'failed', 'not-run')", SCRIPT)

    def test_no_hardcoded_local_paths(self):
        # Reuse the repository's own checker logic and pattern set verbatim.
        violations = chk.find_violations([SCRIPT_REL, WORKFLOW_REL, SELF_REL])
        self.assertEqual(violations, [], "machine-local path / GUID found: %s" % violations)
        # Sanity: the pattern set is live, so the assertion above is not vacuous.
        # The bad path is assembled at run time so no literal local path appears
        # in this source file either.
        synthetic = "C:" + chr(92) + "Users" + chr(92) + "someone" + chr(92) + "repo"
        self.assertTrue(any(p.search(synthetic) for p in chk.PATTERNS),
                        "CheckNoLocalPaths.PATTERNS did not match a known bad path")

    def test_hard_rule_static(self):
        # The script must state and implement the hard rule, and must never assign
        # a passed verdict unconditionally.
        self.assertIn("HARD RULE: a failing precheck must NEVER produce 'passed'.", SCRIPT)
        self.assertIn("if (-not $Precheck.ok) { return 'not-run' }", SCRIPT)
        self.assertIn("if ((-not $Result.precheck.ok) -and ($Result.verdict -eq 'passed')) { $Result.verdict = 'not-run' }", SCRIPT)
        self.assertNotIn("$Result.verdict = 'passed'", SCRIPT,
                         "script must never assign the passed verdict directly")

    def test_precheck_failure_never_passed_json(self):
        # Construct JSON results (round-tripped through json) and validate the
        # invariant: a failing precheck can only ever be not-run.
        scenarios = [
            _base_result(precheck_ok=False),
            _base_result(precheck_ok=False, build="failed"),
            _base_result(precheck_ok=True, build="not-run"),                 # PrecheckOnly / SkipBuild
            _base_result(precheck_ok=True, build="failed"),
            _base_result(precheck_ok=True, build="passed", artifacts_ok=False),
            _base_result(precheck_ok=True, build="passed", artifacts_ok=True, smoke="not-run"),
            _base_result(precheck_ok=True, build="passed", artifacts_ok=True, smoke="passed"),
        ]
        for scenario in scenarios:
            doc = json.loads(json.dumps(scenario))
            doc["verdict"] = compute_verdict(doc)
            assert_contract(self, doc)

        # Explicitly: every failing-precheck scenario must resolve to not-run.
        for scenario in scenarios[:2]:
            doc = json.loads(json.dumps(scenario))
            self.assertEqual(compute_verdict(doc), "not-run")

        # A tampered result that claims passed despite a failed precheck is
        # rejected by the contract (this is the fabrication we must never emit).
        tampered = _base_result(precheck_ok=False)
        tampered["verdict"] = "passed"
        tampered = json.loads(json.dumps(tampered))
        with self.assertRaises(AssertionError):
            assert_contract(self, tampered)

    def test_passed_requires_all_sections_green(self):
        doc = _base_result(precheck_ok=True, build="passed", artifacts_ok=True, smoke="passed")
        doc["verdict"] = compute_verdict(doc)
        self.assertEqual(doc["verdict"], "passed")
        assert_contract(self, doc)
        # Drop any one green section and passed is no longer reachable.
        for field, value in (("build", {"status": "not-run"}),
                             ("artifacts", {"ok": False}),
                             ("smoke", {"status": "not-run"})):
            broken = dict(doc)
            broken[field] = value
            broken["verdict"] = compute_verdict(broken)
            self.assertNotEqual(broken["verdict"], "passed")

    def test_workflow_is_standalone_and_honest(self):
        # The workflow must be dispatch-only, have both jobs, and never pretend a
        # hosted runner can build UE or produce a real acceptance verdict.
        self.assertIn("workflow_dispatch", WORKFLOW)
        self.assertIn("windows-latest", WORKFLOW)
        self.assertIn("macos-14", WORKFLOW)
        self.assertIn("timeout-minutes", WORKFLOW)
        self.assertIn("contents: read", WORKFLOW)
        self.assertIn("-PrecheckOnly", WORKFLOW)
        self.assertIn("not-run", WORKFLOW)
        self.assertIn("self-hosted", WORKFLOW)
        self.assertIn("Xcode", WORKFLOW)
        # It must NOT invoke a real UE build / claim a pass.
        self.assertNotIn("RunUAT.bat BuildCookRun", WORKFLOW)
        self.assertIn("iOS", WORKFLOW)


if __name__ == "__main__":
    unittest.main(verbosity=2)

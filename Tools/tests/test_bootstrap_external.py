"""Regressions for Tools/CrossProject/bootstrap_external.py.

All tests run fully offline. They deliberately create NO performance records:
every results directory used here is empty/missing, and no *.json run file with
performance numbers is ever written. The point of these tests is to prove the
honest, fail-closed behaviour, not to fabricate data.

Covered:
  * a missing / UNKNOWN pin is rejected before any network access
  * offline `check` outputs conclusion="not-established"
  * manifest / directory / workload-contract structure validation
  * the structured JSON reports carry every documented field
  * a missing engine path yields a diagnosable error message (no guessing)
  * `status` with empty results/ never produces any comparison conclusion
"""

import contextlib
import copy
import io
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

ROOT = Path(__file__).resolve().parents[2]
CROSS = ROOT / "Tools" / "CrossProject"
sys.path.insert(0, str(CROSS))
import bootstrap_external as be  # noqa: E402


def run_main(argv):
    """Run main() with captured streams; return (rc, stdout, stderr)."""
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        rc = be.main(argv)
    return rc, out.getvalue(), err.getvalue()


def parse_json(text):
    return json.loads(text)


class ManifestTests(unittest.TestCase):
    def setUp(self):
        self.manifest = be.load_manifest()

    def test_real_manifest_passes_validation(self):
        self.assertEqual(be.validate_manifest(self.manifest), [])

    def test_manifest_has_three_projects(self):
        self.assertEqual(set(self.manifest["projects"]), set(be.PROJECT_IDS))

    def test_no_commit_hash_is_fabricated(self):
        # Every pin is the literal UNKNOWN; a 40-hex hash would be an invented pin.
        for pid in be.PROJECT_IDS:
            with self.subTest(project=pid):
                self.assertEqual(self.manifest["projects"][pid]["pin"], "UNKNOWN")
        self.assertFalse(be.pins_all_set(self.manifest))
        import re
        for pid in be.PROJECT_IDS:
            self.assertIsNone(
                re.fullmatch(r"[0-9a-fA-F]{40}", self.manifest["projects"][pid]["pin"]))

    def test_licenses_and_urls_are_recorded(self):
        projects = self.manifest["projects"]
        self.assertIn("BSD-3-Clause", projects["slua-unreal"]["license"])
        self.assertTrue(projects["slua-unreal"]["license_verified"])
        self.assertIn("https://github.com/Tencent/sluaunreal", projects["slua-unreal"]["repo_url"])
        self.assertIn("UNVERIFIED", projects["funlua"]["license"])
        self.assertFalse(projects["funlua"]["license_verified"])  # no fake MIT claim
        self.assertIn("https://github.com/fcscript/FUnLua", projects["funlua"]["repo_url"])


class PinRejectionTests(unittest.TestCase):
    def test_unknown_pin_is_not_a_set_pin(self):
        self.assertFalse(be.pins_all_set(be.load_manifest()))

    def test_prepare_online_refuses_before_network_when_pin_unknown(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, err = run_main(["prepare", "--root", tmp])
            report = parse_json(out)
            self.assertEqual(rc, 1)
            self.assertIn("refus", err.lower())
            self.assertIn("pin", err.lower())
            self.assertFalse(report["network_used"])
            self.assertFalse(report["writes_performed"])
            self.assertFalse(report["pins_set"])
            # No side effects at all: nothing was cloned or created.
            self.assertFalse(os.path.exists(os.path.join(tmp, "Saved")))
            for action in report["actions"]:
                if action["action"] == "clone+checkout":
                    self.assertEqual(action["status"], "blocked")

    def test_missing_pin_field_is_flagged(self):
        manifest = copy.deepcopy(be.load_manifest())
        del manifest["projects"]["slua-unreal"]["pin"]
        problems = be.validate_manifest(manifest)
        self.assertTrue(any("pin" in p.lower() for p in problems), problems)

    def test_short_non_hex_pin_is_flagged(self):
        manifest = copy.deepcopy(be.load_manifest())
        manifest["projects"]["funlua"]["pin"] = "abc123"
        problems = be.validate_manifest(manifest)
        self.assertTrue(any("40-hex" in p for p in problems), problems)

    def test_a_real_40_hex_pin_is_accepted_by_validation(self):
        manifest = copy.deepcopy(be.load_manifest())
        valid = "0123456789abcdef0123456789abcdef01234567"
        for pid in be.PROJECT_IDS:
            manifest["projects"][pid]["pin"] = valid
        self.assertEqual(be.validate_manifest(manifest), [])
        self.assertTrue(be.pins_all_set(manifest))


class StructureTests(unittest.TestCase):
    def test_missing_workload_field_is_flagged(self):
        manifest = copy.deepcopy(be.load_manifest())
        del manifest["projects"]["funlua"]["workload"]
        problems = be.validate_manifest(manifest)
        self.assertTrue(any("missing field 'workload'" in p for p in problems), problems)

    def test_missing_contract_file_is_flagged(self):
        manifest = copy.deepcopy(be.load_manifest())
        manifest["projects"]["slua-unreal"]["workload"]["contract_source"] = \
            "Tools/CrossProject/workload/external/does-not-exist.lua"
        problems = be.validate_manifest(manifest)
        self.assertTrue(any("contract file not found" in p for p in problems), problems)

    def test_contract_without_begin_marker_is_flagged(self):
        # Point at a real file that is NOT a workload contract (the manifest itself).
        manifest = copy.deepcopy(be.load_manifest())
        manifest["projects"]["unluanova"]["workload"]["contract_source"] = \
            "Tools/CrossProject/external_projects.json"
        problems = be.validate_manifest(manifest)
        self.assertTrue(any("BEGIN" in p for p in problems), problems)

    def test_repo_side_workload_contract_files_are_detected(self):
        manifest = be.load_manifest()
        for pid in be.PROJECT_IDS:
            with self.subTest(project=pid):
                record = be._project_check_record(
                    pid, manifest["projects"][pid], be.REPO_ROOT)
                self.assertTrue(record["workload_contract_source_exists"], record)

    def test_prepared_checkout_structure_is_detected(self):
        manifest = be.load_manifest()
        entry = manifest["projects"]["slua-unreal"]
        rel = entry["expected_layout"]["relative_checkout_dir"]
        marker = entry["expected_layout"]["required_markers"][0]
        with tempfile.TemporaryDirectory() as tmp:
            checkout = os.path.join(tmp, rel)
            marker_path = os.path.join(checkout, marker)
            os.makedirs(os.path.dirname(marker_path))
            # Empty marker file: a structural fixture, NOT performance data.
            with io.open(marker_path, "w", encoding="utf-8") as handle:
                handle.write("")
            record = be._project_check_record("slua-unreal", entry, tmp)
            self.assertTrue(record["checkout_dir_exists"])
            self.assertTrue(record["structure_markers"][0]["present"])
            self.assertTrue(record["prepared"])


class CheckReportTests(unittest.TestCase):
    def setUp(self):
        # Make engine resolution deterministic: treat all engine env vars as unset.
        self.env_patch = mock.patch.dict(
            os.environ, {var: "" for var in be.ENGINE_ENV_VARS})
        self.env_patch.start()

    def tearDown(self):
        self.env_patch.stop()

    def test_offline_check_outputs_not_established(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, err = run_main(["check", "--root", tmp, "--offline", "--dry-run"])
            self.assertEqual(rc, 0)
            report = parse_json(out)
            self.assertEqual(report["command"], "check")
            self.assertEqual(report["conclusion"], "not-established")
            self.assertTrue(report["read_only"])
            self.assertTrue(report["offline"])
            self.assertFalse(report["network_used"])
            self.assertIsNone(report["comparison"])
            self.assertFalse(report["ready"])

    def test_json_report_fields_are_complete(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, check_out, _ = run_main(["check", "--root", tmp, "--offline"])
            check = parse_json(check_out)
            self.assertTrue(set(check) >= set(be.CHECK_REPORT_KEYS),
                            set(be.CHECK_REPORT_KEYS) - set(check))
            _, status_out, _ = run_main(["status", "--root", tmp])
            status = parse_json(status_out)
            self.assertTrue(set(status) >= set(be.STATUS_REPORT_KEYS),
                            set(be.STATUS_REPORT_KEYS) - set(status))
            # Alignment contract fields are present and honest.
            alignment = check["alignment"]
            self.assertFalse(alignment["can_align"])
            self.assertTrue(alignment["fail_closed"])
            self.assertIn("engine", alignment["required_identical_fields"])
            self.assertIn("machine", alignment["required_identical_fields"])
            self.assertEqual(set(alignment["non_time_metrics"]["names"]),
                             set(be.runner.DEFAULT_METRICS))
            # Each project record has the full field set.
            for record in check["projects"]:
                for key in ("id", "pin", "pin_set", "license", "ue_version",
                            "prepared", "workload_contract_source_exists"):
                    self.assertIn(key, record)

    def test_engine_path_missing_error_message(self):
        engine = be.resolve_engine(explicit=None, env={})
        self.assertFalse(engine["found"])
        self.assertIsNone(engine["path"])
        self.assertEqual(engine["version"], "UNKNOWN")
        self.assertIn("engine path", engine["diagnostic"].lower())
        self.assertIn("--engine-path", engine["diagnostic"])

    def test_engine_path_explicit_but_missing_is_reported(self):
        engine = be.resolve_engine(explicit="/definitely/not/a/real/engine", env={})
        self.assertFalse(engine["found"])
        self.assertIn("does not exist", engine["diagnostic"])

    def test_check_report_contains_engine_diagnostic(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, out, _ = run_main(["check", "--root", tmp, "--offline"])
            report = parse_json(out)
            self.assertFalse(report["engine"]["found"])
            # The failed engine check is surfaced as a diagnosable message.
            self.assertTrue(any("engine path" in d.lower() for d in report["diagnostics"]))
            engine_check = next(c for c in report["checks"] if c["name"] == "engine_path")
            self.assertFalse(engine_check["ok"])
            self.assertIn("engine", engine_check["detail"].lower())

    def test_check_never_fabricates_engine_or_machine_identity(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, out, _ = run_main(["check", "--root", tmp, "--offline"])
            report = parse_json(out)
            self.assertEqual(report["engine"]["version"], "UNKNOWN")
            self.assertEqual(report["host"]["machine_identity"], "UNKNOWN")
            self.assertEqual(report["host"]["cpu_identity"], "UNKNOWN")


class StatusReportTests(unittest.TestCase):
    def test_status_empty_results_is_not_established_with_no_comparison(self):
        # This mirrors the required CLI invocation: status --root <temp directory>.
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, err = run_main(["status", "--root", tmp])
            self.assertEqual(rc, 0)
            report = parse_json(out)
            self.assertEqual(report["command"], "status")
            self.assertEqual(report["conclusion"], "not-established")
            self.assertEqual(report["record_state"], "empty")
            self.assertFalse(report["results_dir_exists"])
            self.assertEqual(report["result_files"], [])
            self.assertEqual(report["projects_with_records"], [])
            self.assertEqual(set(report["projects_without_records"]), set(be.PROJECT_IDS))
            self.assertFalse(report["all_three_have_records"])
            self.assertIsNone(report["comparison"])
            self.assertIsNone(report["speed_claim"])
            self.assertFalse(report["cross_project_comparison_established"])
            # No winner / ratio / fastest field is ever produced.
            for forbidden in ("winner", "fastest", "ratios", "ratio", "speedup"):
                self.assertNotIn(forbidden, report)
            # The 2.08x boundary is stated, but never as a cross-project conclusion.
            self.assertIn("2.08x", report["readme_2_08x_note"])
            self.assertIn("NOT a comparison", report["readme_2_08x_note"])
            self.assertTrue(set(report) >= set(be.STATUS_REPORT_KEYS))
            for pid in be.PROJECT_IDS:
                self.assertFalse(report["projects"][pid]["has_real_run_record"])

    def test_status_results_dir_override_empty_is_not_established(self):
        with tempfile.TemporaryDirectory() as tmp:
            empty_results = os.path.join(tmp, "empty-results")
            os.makedirs(empty_results)  # exists but contains no run records
            rc, out, _ = run_main(
                ["status", "--root", tmp, "--results-dir", empty_results])
            report = parse_json(out)
            self.assertEqual(rc, 0)
            self.assertTrue(report["results_dir_exists"])
            self.assertEqual(report["result_files"], [])
            self.assertEqual(report["conclusion"], "not-established")
            self.assertIsNone(report["comparison"])

    def test_status_never_claims_speed_even_with_dry_run(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, out, _ = run_main(["status", "--root", tmp, "--dry-run", "--offline"])
            report = parse_json(out)
            self.assertEqual(report["conclusion"], "not-established")
            self.assertIsNone(report["comparison"])
            self.assertIsNone(report["speed_claim"])


class PrepareOfflineTests(unittest.TestCase):
    def test_prepare_offline_is_validation_only(self):
        with tempfile.TemporaryDirectory() as tmp:
            rc, out, _ = run_main(
                ["prepare", "--root", tmp, "--offline", "--dry-run"])
            self.assertEqual(rc, 0)
            report = parse_json(out)
            self.assertEqual(report["mode"], "validation-only")
            self.assertEqual(report["conclusion"], "not-established")
            self.assertFalse(report["network_used"])
            self.assertFalse(report["writes_performed"])
            self.assertFalse(report["pins_set"])
            self.assertFalse(report["ready"])
            self.assertTrue(set(report) >= set(be.PREPARE_REPORT_KEYS))
            # No checkout directory was created.
            self.assertFalse(os.path.exists(os.path.join(tmp, "Saved")))

    def test_prepare_offline_actions_are_all_blocked_or_not_applicable(self):
        with tempfile.TemporaryDirectory() as tmp:
            _, out, _ = run_main(["prepare", "--root", tmp, "--offline"])
            report = parse_json(out)
            for action in report["actions"]:
                self.assertIn(action["status"], ("blocked", "not-applicable"))


if __name__ == "__main__":
    unittest.main()

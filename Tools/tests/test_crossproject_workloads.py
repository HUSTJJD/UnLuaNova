"""Static contract checks for every cross-project workload (no UE / no Lua VM needed).

The runner refuses runs whose case sets or iteration counts differ, but it can only do
that after somebody has actually run the bindings. These checks catch a drift at edit
time: the external adapter must stay a drop-in equivalent of the UnLuaNova workload and
of WORKLOAD.md revision 1.
"""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CROSS = ROOT / "Tools" / "CrossProject"
UNLUANOVA = CROSS / "workload" / "unluanova" / "cases.lua"
EXTERNAL = CROSS / "workload" / "external" / "cases.lua"
CONTRACT = CROSS / "WORKLOAD.md"

CASE_RE = re.compile(r'Name\s*=\s*"([^"]+)"\s*,\s*Iterations\s*=\s*(\d+)')
MD_ROW_RE = re.compile(r'^\|\s*`([^`]+)`\s*\|\s*(\d+)\s*\|', re.M)


def cases(path):
    return [(m.group(1), int(m.group(2))) for m in CASE_RE.finditer(path.read_text(encoding="utf-8"))]


class WorkloadContractTests(unittest.TestCase):
    def setUp(self):
        self.unluanova = UNLUANOVA.read_text(encoding="utf-8")
        self.external = EXTERNAL.read_text(encoding="utf-8")
        self.contract = CONTRACT.read_text(encoding="utf-8")

    def test_revision_is_pinned_everywhere(self):
        self.assertIn("revision 1", self.contract)
        for name, text in (("unluanova", self.unluanova), ("external", self.external)):
            with self.subTest(workload=name):
                self.assertIn('Bench.Revision = "1"', text)
                self.assertIn("workload_revision=%s", text)

    def test_case_sets_and_iterations_match_the_contract(self):
        expected = [(name, int(count)) for name, count in MD_ROW_RE.findall(self.contract)]
        self.assertEqual(len(expected), 10)
        self.assertEqual(cases(UNLUANOVA), expected)
        self.assertEqual(cases(EXTERNAL), expected)

    def test_no_case_is_silently_dropped(self):
        for name, text in (("unluanova", self.unluanova), ("external", self.external)):
            with self.subTest(workload=name):
                self.assertIn("#UNLUANOVA_BENCH_BEGIN", text)
                self.assertIn("#UNLUANOVA_BENCH_END", text)
                self.assertIn("peropus=0 samples=", text)
                self.assertIn("-- case failed:", text)
                self.assertIn("pcall(Bench.Measure, case.Fn, case.Iterations)", text)

    def test_hardware_identity_is_not_a_placeholder(self):
        for name, text in (("unluanova", self.unluanova), ("external", self.external)):
            with self.subTest(workload=name):
                self.assertNotIn("FILL-ME", text)
                self.assertIn("Bench.SetMachineInfo", text)
                self.assertIn(":find(\"%s\")", text)

    def test_meta_line_has_all_required_fields(self):
        for field in ("project", "backend", "engine", "machine", "cpu", "config",
                      "carrier", "workload_revision", "measured_utc"):
            with self.subTest(field=field):
                self.assertIn(field + "=", self.unluanova)
                self.assertIn(field + "=", self.external)

    def test_external_adapter_is_configurable_and_isolates_ue_calls(self):
        self.assertIn("function Bench.SetProject(", self.external)
        self.assertIn("BadIdentity(Bench.Project)", self.external)
        self.assertIn("function UEAPI.Static(", self.external)
        self.assertIn("function UEAPI.Vector(", self.external)
        self.assertIn("is not reachable in this binding", self.external)
        # Pure VM cases must not go through the adapter.
        for vm_case in ("VM.Arithmetic", "VM.Call", "VM.TableRead", "GC.Collect"):
            with self.subTest(case=vm_case):
                start = self.external.index('Name = "%s"' % vm_case)
                end = self.external.find("\n    },", start)
                self.assertTrue(end > start)
                self.assertNotIn("UEAPI.", self.external[start:end])


if __name__ == "__main__":
    unittest.main()

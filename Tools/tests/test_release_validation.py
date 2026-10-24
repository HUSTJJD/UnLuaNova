import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
SPEC = importlib.util.spec_from_file_location("release_gate", TOOLS / "ValidateRelease.py")
gate = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(gate)


class ReleaseValidationTests(unittest.TestCase):
    def setUp(self):
        self.manifest = dict(backend="Lua", editor=True, cooked=False, timer=True, event=True, oop=True,
                             tests=["UnLuaNova.API.Timer.cancel", "UnLuaNova.API.Timer.repeat"])
        self.report = {"tests": [dict(fullTestPath=n, state="Success", entries=[], errors=0)
                                 for n in self.manifest["tests"]]}

    def check(self, carrier="editor", source=()):
        return gate.validate_automation(self.manifest, self.report, "Lua", carrier, source)

    def test_complete_run_and_native_lua_exclusion(self):
        self.assertEqual(self.check(source=["UnLuaNova.API.Timer", "UnLuaNova.API.LuauIntegration"]), [])

    def test_display_names_cannot_replace_full_paths(self):
        for test in self.report["tests"]:
            test["testDisplayName"] = "same display name"
        self.assertEqual(self.check(), [])
        del self.report["tests"][0]["fullTestPath"]
        self.assertTrue(self.check())

    def test_missing_new_leaf_detected_without_count_baseline(self):
        self.manifest["tests"].append("UnLuaNova.API.Timer.new")
        self.assertTrue(any("not executed" in e for e in self.check()))

    def test_empty_missing_and_duplicate_reports_fail(self):
        for tests in ([], [self.report["tests"][0]] * 2):
            with self.subTest(tests=tests):
                self.report["tests"] = tests
                self.assertTrue(self.check())

    def test_skipped_unknown_inprocess_and_failed_are_not_passes(self):
        for state in ("Skipped", "NotRun", "InProcess", "Fail", "", "Unknown"):
            with self.subTest(state=state):
                self.report["tests"][0]["state"] = state
                self.assertTrue(self.check())

    def test_errors_override_success_state(self):
        self.report["tests"][0]["entries"] = [{"event": {"type": "Error", "message": "assertion"}}]
        self.assertTrue(self.check())

    def test_wrong_binary_or_disabled_feature_fails(self):
        for field, value in (("backend", "Luau"), ("editor", False), ("cooked", True), ("timer", False)):
            original = self.manifest[field]
            self.manifest[field] = value
            self.assertTrue(self.check())
            self.manifest[field] = original

    def test_source_suite_cannot_silently_compile_out(self):
        self.assertTrue(any("Source suite" in e for e in self.check(source=["UnLuaNova.API.Missing"])))

    def test_game_must_be_cooked_and_context_specific(self):
        self.manifest.update(editor=False, cooked=True)
        self.assertEqual(self.check("game", source=["UnLuaNova.Editor.IntelliSense"]), [])

    def test_typecheck_must_run_with_real_declarations(self):
        data = dict(skipped=False, files=5, exit=0, errors=[], defs="fresh.d.lua", analyzer="lsp.exe")
        self.assertEqual(gate.validate_typecheck(data), [])
        for key, value in (("skipped", True), ("files", 0), ("exit", 3), ("errors", ["error"]), ("defs", None)):
            invalid = dict(data, **{key: value})
            self.assertTrue(gate.validate_typecheck(invalid))

    def test_performance_requires_comparable_complete_baseline(self):
        base = dict(Backend="Lua", EngineVersion="UE-test", Machine="runner", Cases=[dict(Case="call", Iterations=100, PerOpUs=2)])
        self.assertEqual(gate.validate_performance(base, base, "Lua"), [])
        for field, value in (("Machine", "other"), ("Cases", []), ("Backend", "Luau")):
            self.assertTrue(gate.validate_performance(base, dict(base, **{field: value}), "Lua"))
        actual = copy.deepcopy(base)
        actual["Cases"][0]["PerOpUs"] = 2.6
        self.assertTrue(gate.validate_performance(actual, base, "Lua"))
        actual["Cases"][0]["PerOpUs"] = float("nan")
        self.assertTrue(gate.validate_performance(actual, base, "Lua"))

    def test_missing_all_evidence_fails_each_gate(self):
        with tempfile.TemporaryDirectory() as directory:
            errors = gate.validate_run(Path(directory), "Lua", "editor")
        for name in ("automation", "network", "performance", "typecheck", "media", "memory"):
            self.assertTrue(any(e.startswith(name + ":") for e in errors))

    def memory_case(self, name, backend="Lua", carrier="editor"):
        envs, count, resources = gate.MEMORY_CASES[name]
        samples = [dict(bytes=0, blocks=0) for _ in range(count)]
        if name in ("steady_heap", "hot_reload"):
            samples = [dict(bytes=200000, blocks=500) for _ in range(count)]
        if name == "retained_reference":
            samples = [dict(bytes=200000, blocks=500), dict(bytes=1200000, blocks=10000), dict(bytes=200000, blocks=500)]
        return dict(schema_version=1, case=name, backend=backend, editor=carrier == "editor", cooked=carrier == "game",
                    run_id="current-run", passed=True, closed_envs=envs, allocation_events=10000,
                    remaining_bytes=0, remaining_blocks=0, accounting_errors=0, checked_resources=resources,
                    retained_resources=0, initial_envs=1, final_envs=1, warmup=8,
                    positive_control_detected=True, samples=samples)

    def test_memory_complete_matrix_for_both_backends_and_carriers(self):
        for backend in ("Lua", "Luau"):
            for carrier in ("editor", "game"):
                for name in gate.MEMORY_CASES:
                    with self.subTest(backend=backend, carrier=carrier, name=name):
                        self.assertEqual(gate.validate_memory_case(self.memory_case(name, backend, carrier),
                            name, backend, carrier, "current-run"), [])

    def test_memory_cannot_pass_with_retained_resources_or_bad_accounting(self):
        for key, value in (("remaining_bytes", 1), ("remaining_blocks", 1), ("accounting_errors", 1),
                           ("retained_resources", 1), ("closed_envs", 0), ("checked_resources", 0),
                           ("allocation_events", 0), ("final_envs", 2), ("remaining_bytes", -1),
                           ("remaining_bytes", float("nan")), ("remaining_bytes", False),
                           ("passed", False), ("backend", "Luau"), ("cooked", True), ("run_id", "stale")):
            with self.subTest(key=key, value=value):
                data = self.memory_case("native_payloads")
                data[key] = value
                self.assertTrue(gate.validate_memory_case(data, "native_payloads", "Lua", "editor", "current-run"))

    def test_memory_detects_growth_instead_of_trusting_passed_flag(self):
        for key, growth in (("bytes", 9000), ("blocks", 17)):
            data = self.memory_case("steady_heap")
            data["samples"][-1][key] += growth
            self.assertTrue(gate.validate_memory_case(data, "steady_heap", "Lua", "editor", "current-run"))
        data = self.memory_case("vm_cycles")
        data["samples"][5]["bytes"] = 64
        self.assertTrue(gate.validate_memory_case(data, "vm_cycles", "Lua", "editor", "current-run"))

    def test_memory_requires_measured_positive_control_and_cleanup(self):
        for change in ("undetected", "not_released", "empty", "missing_counter"):
            data = self.memory_case("retained_reference")
            if change == "undetected":
                data["samples"][1] = dict(data["samples"][0])
            elif change == "not_released":
                data["samples"][2] = dict(data["samples"][1])
            elif change == "empty":
                data["samples"] = []
            else:
                del data["remaining_bytes"]
            self.assertTrue(gate.validate_memory_case(data, "retained_reference", "Lua", "editor", "current-run"))

    def test_memory_requires_every_fresh_registered_report(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            memory = run / "metadata/memory"
            memory.mkdir(parents=True)
            manifest = dict(run_id="current-run", tests=[f"UnLuaNova.Memory.{name}" for name in gate.MEMORY_CASES])
            (run / "metadata/manifest.json").write_text(json.dumps(manifest))
            self.assertEqual(len(gate.validate_memory(run, "Lua", "editor")), len(gate.MEMORY_CASES))
            for name in gate.MEMORY_CASES:
                (memory / f"{name}.json").write_text(json.dumps(self.memory_case(name)))
            self.assertEqual(gate.validate_memory(run, "Lua", "editor"), [])
            (memory / "vm_cycles.json").unlink()
            self.assertTrue(gate.validate_memory(run, "Lua", "editor"))

    def test_network_requires_connected_matching_peers(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = run / "net/Multicast"
            (net / "logs").mkdir(parents=True)
            labels = ["Server", "Client1", "Client2"]
            (net / "topology.json").write_text(json.dumps({"peers": [{"label": n} for n in labels]}))
            for label in labels:
                data = dict(Backend="Lua", Role=label, NetMode=1 if label == "Server" else 3,
                            Clients=2, MulticastSeen=True, Rpcs=[dict(Name="ToAll", RpcId=1)])
                (net / f"{label}.json").write_text(json.dumps(data))
                (net / "logs" / f"{label}.log").write_text("peer log")
            self.assertEqual(gate.validate_network(run, "Lua"), [])
            data.update(NetMode=0, MulticastSeen=False, Backend="Luau")
            (net / "Client2.json").write_text(json.dumps(data))
            self.assertGreaterEqual(len(gate.validate_network(run, "Lua")), 3)

    def test_media_requires_every_showcase_not_a_session_recording(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / "videos").mkdir()
            (run / "screenshots").mkdir()
            (run / "videos/session.mp4").write_bytes(b"recording")
            self.assertEqual(len(gate.validate_media(run)), 8)
            for name in ("Binding", "Override", "Container", "UMG"):
                (run / f"screenshots/Showcase_{name}.png").write_bytes(b"image")
                (run / f"videos/Showcase_{name}_Clip.mp4").write_bytes(b"clip")
            self.assertEqual(gate.validate_media(run), [])


if __name__ == "__main__":
    unittest.main()

import copy
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

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

    def test_ue_json_encodings_preserve_non_ascii_test_names(self):
        manifest = dict(self.manifest, tests=["UnLuaNova.Regression.\u7ed3\u6784\u4f53"])
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "manifest.json"
            for encoding in ("utf-8", "utf-8-sig", "utf-16", "utf-16-be"):
                with self.subTest(encoding=encoding):
                    path.write_bytes(json.dumps(manifest, ensure_ascii=False).encode(encoding))
                    self.assertEqual(gate.read_json(path), manifest)
            path.write_bytes(b"\xff\xfe{\x00invalid")
            with self.assertRaises((ValueError, UnicodeError)):
                gate.read_json(path)

    def test_complete_run_and_native_lua_exclusion(self):
        self.assertEqual(self.check(source=["UnLuaNova.API.Timer", "UnLuaNova.API.LuauIntegration"]), [])

    def test_namespace_matches_ue_case_insensitive_filter(self):
        name = "UNLUANOVA.Regression.Issue362"
        self.manifest["tests"] = [name]
        self.report["tests"] = [dict(fullTestPath=name, state="Success")]
        self.assertEqual(self.check(), [])
        self.manifest["tests"] = ["UnLuaNovaOther.Regression.Issue362"]
        self.assertTrue(self.check())

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

    def performance_samples(self):
        baseline = dict(Backend="Lua", EngineVersion="UE-test", Machine="runner",
                        Cases=[dict(Case="call", Iterations=100, PerOpUs=2)])
        actual = copy.deepcopy(baseline)
        actual["Measurement"] = dict(Method="median-of-5-batch-means", WarmupMinMs=250, SampleMinMs=100)
        actual["Cases"][0].update(WarmupMs=250, WarmupBatches=1250,
            Samples=[dict(ElapsedMs=100, Batches=batches) for batches in (500, 500, 500, 10, 1000)])
        return actual, baseline

    def test_performance_median_recomputed_from_raw_samples(self):
        actual, baseline = self.performance_samples()
        self.assertEqual(gate.validate_performance(actual, baseline, "Lua", require_samples=True), [])
        actual["Cases"][0]["PerOpUs"] = 1  # The fastest sample must not replace the median.
        self.assertTrue(gate.validate_performance(actual, baseline, "Lua", require_samples=True))

    def test_performance_rejects_missing_short_or_invalid_samples(self):
        actual, baseline = self.performance_samples()
        self.assertTrue(gate.validate_performance(baseline, baseline, "Lua", require_samples=True))
        for field, value in (("WarmupMs", 249), ("WarmupBatches", 0), ("Samples", []),
                             ("Samples", [dict(ElapsedMs=99, Batches=500)] * 5),
                             ("Samples", [dict(ElapsedMs=100, Batches=0)] * 5),
                             ("Samples", [dict(ElapsedMs=float("nan"), Batches=500)] * 5)):
            with self.subTest(field=field, value=value):
                invalid = copy.deepcopy(actual)
                invalid["Cases"][0][field] = value
                self.assertTrue(gate.validate_performance(invalid, baseline, "Lua", require_samples=True))

    def memory_case(self, name, backend="Lua", carrier="editor"):
        envs, count, resources = gate.MEMORY_CASES[name]
        samples = [dict(bytes=0, blocks=0) for _ in range(count)]
        if name in ("steady_heap", "hot_reload"):
            samples = [dict(bytes=200000, blocks=500) for _ in range(count)]
        if name == "retained_reference":
            samples = [dict(bytes=200000, blocks=500), dict(bytes=1200000, blocks=10000), dict(bytes=200000, blocks=500)]
        for sample in samples:
            sample["live_bytes"] = sample["bytes"]
        return dict(schema_version=2, case=name, backend=backend, editor=carrier == "editor", cooked=carrier == "game",
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
        for key, growth in (("live_bytes", 9000), ("blocks", 17)):
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

    def test_live_heap_does_not_confuse_luau_page_reservations_with_live_objects(self):
        data = self.memory_case("hot_reload", backend="Luau")
        data["samples"][10]["bytes"] += 32768
        self.assertEqual(gate.validate_memory_case(data, "hot_reload", "Luau", "editor", "current-run"), [])
        data["samples"][10]["live_bytes"] += 9000
        self.assertTrue(gate.validate_memory_case(data, "hot_reload", "Luau", "editor", "current-run"))
        del data["samples"][10]["live_bytes"]
        self.assertTrue(gate.validate_memory_case(data, "hot_reload", "Luau", "editor", "current-run"))

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

    def connected_evidence(self, run, subpath="net/DedicatedServer"):
        directory = run / subpath
        (directory / "logs").mkdir(parents=True)
        labels = ("Server", "Client1", "Client2")
        session = "0123456789abcdef0123456789abcdef"
        peers = [dict(label=label, pid=100 + index, port=7777 + index,
                      connect_to="" if index == 0 else "127.0.0.1") for index, label in enumerate(labels)]
        (directory / "topology.json").write_text(json.dumps(dict(peers=peers)))
        for index, label in enumerate(labels):
            def actors(rounds, ack=False):
                result = []
                for actor_id, seed in ((1, 1234), (2, 1334)):
                    calls = []
                    for phase in range(rounds):
                        value = seed + phase * 10
                        if phase == 1:
                            if index == 0:
                                calls.append(f"FromClient:{seed + 1}")
                            elif index == actor_id:
                                calls.append(f"FromServer:{seed + 4}")
                        if index == 0:
                            calls.append(f"FromClient:{value + 1}")
                        elif index == actor_id:
                            calls.append(f"FromServer:{value + 2}")
                        calls.append(f"ToAll:{value + 3}")
                    if ack:
                        calls.append(f"FromClient:{seed + 11}")
                    result.append(dict(ActorId=actor_id, Challenge=seed, SessionId=session,
                                       Authority=index == 0, Owned=index == actor_id,
                                       HP=seed + (rounds - 1) * 10 + 1, HpReadable=True,
                                       OwnerSecret=777 if index == 0 or index == actor_id else -1,
                                       CallLog="|".join(calls) + "|", CallCount=len(calls)))
                return result
            data = dict(SchemaVersion=2, Complete=True, Fault="", Role=label, Backend="Lua",
                        SessionId=session, ProcessId=100 + index, NetMode=1 if index == 0 else 3,
                        Connected=True, Clients=2 if index == 0 else 0, OwnedActorId=index,
                        FirstRoundObservationMs=5000,
                        WorldPackage="/UnLuaNovaTestSuite/Tests/Misc/Empty", ServerAddress="127.0.0.1:7777",
                        Rpcs=[
                            dict(Name="FromClient", RpcId=1, Params=[dict(Name="Value", Type="IntProperty")]),
                            dict(Name="FromServer", RpcId=2, Params=[dict(Name="Value", Type="IntProperty")]),
                            dict(Name="ToAll", RpcId=3, Params=[dict(Name="Value", Type="IntProperty")]),
                            dict(Name="SendPayload", RpcId=4,
                                 Params=[dict(Name="Payload", Type="Struct:/Script/CoreUObject.Vector")]),
                        ],
                        Props=[dict(Name="HP", Type=1, Condition=0, TypeSpec=""),
                               dict(Name="OwnerSecret", Type=1, Condition=2, TypeSpec=""),
                               dict(Name="Inventory", Type=11, Condition=0, TypeSpec="Array:Int")],
                        RepSchemaVersion=12345, VariantRepSchemaVersion=54321,
                        NetStats=dict(OutTotalBytes=4096, InTotalBytes=2048, LostPackets=0),
                        Cooked=False,
                        NonOwnerRpcAttempted=index != 0,
                        NonOwnerRpcToken=f"FromClient:7000{index}|" if index else "",
                        Actors=actors(2, ack=index == 0),
                        Observations=[dict(Round=phase, Actors=actors(phase)) for phase in (1, 2)])
            (directory / f"{label}.json").write_text(json.dumps(data))
            (directory / "logs" / f"{label}.log").write_text("connected peer log")
        return directory

    def test_connected_network_rejects_disconnected_or_different_sessions(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            self.assertEqual(gate.validate_connected_session(run, "Lua"), [])
            path = net / "Client2.json"
            original = json.loads(path.read_text())
            for change in (dict(Connected=False), dict(NetMode=0), dict(SessionId="f" * 32),
                           dict(ProcessId=101), dict(ServerAddress="127.0.0.1:9999"),
                           dict(OwnedActorId=1), dict(Complete=False), dict(Observations=[])):
                with self.subTest(change=change):
                    path.write_text(json.dumps(dict(original, **change)))
                    self.assertTrue(gate.validate_connected_session(run, "Lua"))

    def test_connected_network_requires_replication_and_rpc_delivery(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            path = net / "Client1.json"
            original = json.loads(path.read_text())
            for field, value in (("HP", 100), ("CallLog", ""), ("Challenge", 9999),
                                 ("SessionId", "f" * 32), ("Authority", True), ("CallCount", 0)):
                with self.subTest(field=field):
                    data = copy.deepcopy(original)
                    data["Actors"][0][field] = value
                    path.write_text(json.dumps(data))
                    self.assertTrue(gate.validate_connected_session(run, "Lua"))
            data = copy.deepcopy(original)
            data["Observations"][0]["Actors"][1]["HP"] = 100
            path.write_text(json.dumps(data))
            self.assertTrue(gate.validate_connected_session(run, "Lua"))

    def test_connected_network_requires_round_barrier(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            path = net / "Client1.json"
            data = json.loads(path.read_text())
            original = copy.deepcopy(data)
            data["Actors"][0]["CallLog"] = data["Actors"][0]["CallLog"].replace("FromServer:1238|", "")
            data["Actors"][0]["CallCount"] -= 1
            path.write_text(json.dumps(data))
            self.assertTrue(gate.validate_connected_session(run, "Lua"))
            for delay in (None, 4999, float("nan")):
                original["FirstRoundObservationMs"] = delay
                path.write_text(json.dumps(original))
                self.assertTrue(gate.validate_connected_session(run, "Lua"))

    def test_connected_network_rejects_client_rpc_leaking_to_non_owner(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            path = net / "Client1.json"
            data = json.loads(path.read_text())
            data["Actors"][1]["CallLog"] += "FromServer:1336|"
            data["Actors"][1]["CallCount"] += 1
            path.write_text(json.dumps(data))
            self.assertTrue(gate.validate_connected_session(run, "Lua"))

    def test_connected_network_rejects_wrong_owner_only_visibility(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            self.assertEqual(gate.validate_connected_session(run, "Lua"), [])
            path = net / "Client1.json"
            data = json.loads(path.read_text())
            data["Actors"][1]["OwnerSecret"] = 777  # Client1 does not own actor 2
            path.write_text(json.dumps(data))
            self.assertTrue(any("OwnerOnly field visibility failed" in e
                               for e in gate.validate_connected_session(run, "Lua")))

    def test_connected_network_requires_the_owner_only_declaration(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            path = net / "Server.json"
            data = json.loads(path.read_text())
            data["Props"] = [dict(Name="HP", Type=1, Condition=0)]
            path.write_text(json.dumps(data))
            self.assertTrue(any("OwnerOnly field missing" in e
                               for e in gate.validate_connected_session(run, "Lua")))

    def test_connected_network_requires_the_non_owner_attempt_itself(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            self.assertEqual(gate.validate_connected_session(run, "Lua"), [])
            path = net / "Client1.json"
            original = json.loads(path.read_text())
            for change in (dict(NonOwnerRpcAttempted=False), dict(NonOwnerRpcToken=""),
                           dict(NonOwnerRpcToken=json.loads((net / "Client2.json").read_text())["NonOwnerRpcToken"])):
                with self.subTest(change=change):
                    path.write_text(json.dumps(dict(original, **change)))
                    self.assertTrue(gate.validate_connected_session(run, "Lua"))

    def test_connected_network_rejects_an_executed_non_owner_server_rpc(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            token = json.loads((net / "Client1.json").read_text())["NonOwnerRpcToken"]
            server_path = net / "Server.json"
            data = json.loads(server_path.read_text())
            data["Actors"][0]["CallLog"] += token
            server_path.write_text(json.dumps(data))
            errors = gate.validate_connected_session(run, "Lua")
            self.assertTrue(any("Non-owner Server RPC executed" in e for e in errors))

    def test_connected_network_rejects_old_declaration_only_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = self.connected_evidence(run)
            for label in ("Server", "Client1", "Client2"):
                (net / f"{label}.json").write_text(json.dumps(dict(Role=label, Backend="Lua", Rpcs=[])))
            self.assertTrue(gate.validate_connected_session(run, "Lua"))

    def test_network_requires_connected_matching_peers(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            net = run / "net/Multicast"
            self.connected_evidence(run)
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

    @staticmethod
    def png_bytes(width=640, height=480):
        return (gate.PNG_MAGIC + b"\x00\x00\x00\x0dIHDR"
                + width.to_bytes(4, "big") + height.to_bytes(4, "big")
                + b"\x08\x06\x00\x00\x00" + b"\x00" * 4)

    def test_media_requires_every_showcase_screenshot(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / "screenshots").mkdir()
            self.assertEqual(len(gate.validate_media(run)), 4)
            for name in ("Binding", "Override", "Container", "UMG"):
                (run / f"screenshots/Showcase_{name}.png").write_bytes(self.png_bytes())
            self.assertEqual(gate.validate_media(run), [])

    def test_media_rejects_placeholder_files_that_are_not_pngs(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / "screenshots").mkdir()
            for name in ("Binding", "Override", "Container", "UMG"):
                (run / f"screenshots/Showcase_{name}.png").write_bytes(b"image" * 64)
            errors = gate.validate_media(run)
            self.assertEqual(len(errors), 4)
            self.assertTrue(all("not a PNG" in error for error in errors))

    def test_media_rejects_truncated_header_and_zero_dimensions(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            (run / "screenshots").mkdir()
            for name in ("Binding", "Override", "Container", "UMG"):
                (run / f"screenshots/Showcase_{name}.png").write_bytes(
                    self.png_bytes()[:8] if name in ("Binding", "Override")
                    else self.png_bytes(0, 0))
            errors = gate.validate_media(run)
            self.assertEqual(len(errors), 4)
            self.assertEqual(len([e for e in errors if "IHDR" in e]), 2)
            self.assertEqual(len([e for e in errors if "zero PNG dimensions" in e]), 2)

    def test_gate_file_carries_provenance_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            argv = ["ValidateRelease.py", "--run", str(run), "--backend", "Lua",
                    "--carrier", "game"]
            with mock.patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(gate.main(), 1)  # no evidence at all -> fail closed
            written = json.loads((run / "metadata/release-gate.json").read_text(encoding="utf-8"))
            self.assertFalse(written["passed"])
            self.assertEqual(written["schema"], "unluanova-release-gate/1")
            self.assertEqual(written["backend"], "Lua")
            self.assertEqual(written["carrier"], "game")
            self.assertFalse(written["net_matrix"])
            self.assertIn("commit", written)
            self.assertIn("generated_at", written)
            self.assertTrue(written["generated_at"].endswith("+00:00"))

    def test_current_commit_is_none_when_git_is_unavailable(self):
        with mock.patch.object(gate.subprocess, "run", side_effect=OSError("no git")):
            self.assertIsNone(gate.current_commit(Path(".")))
        with mock.patch.object(gate.subprocess, "run",
                               return_value=mock.Mock(returncode=1, stdout=b"")):
            self.assertIsNone(gate.current_commit(Path(".")))
        with mock.patch.object(gate.subprocess, "run",
                               return_value=mock.Mock(returncode=0, stdout=b"cab95a83\n")):
            self.assertEqual(gate.current_commit(Path(".")), "cab95a83")


    def scenario_evidence(self, run, variant=""):
        session = "0123456789abcdef0123456789abcdef"
        labels = ("Server", "Client1", "Client2")
        for name in gate.NET_SCENARIOS:
            directory = run / "net/scenarios" / name
            (directory / "logs").mkdir(parents=True)
            swapped = name == "JoinOrderSwap"
            staggered = name in ("LateJoin", "JoinOrderSwap")
            peers = [
                dict(label="Server", pid=100, launch_serial=1, launch_count=1, first_pid=100),
                dict(label="Client1", pid=101, launch_serial=3 if swapped else 2,
                     launch_count=2 if name == "Reconnect" else 1,
                     first_pid=201 if name == "Reconnect" else 101),
                dict(label="Client2", pid=102, launch_serial=2 if swapped else 3,
                     launch_count=1, first_pid=102),
            ]
            (directory / "topology.json").write_text(json.dumps(
                dict(scenario=name, variant=variant, peers=peers)))
            rpcs = [
                dict(Name="FromClient", RpcId=1, Params=[dict(Name="Value", Type="IntProperty")]),
                dict(Name="FromServer", RpcId=2, Params=[dict(Name="Value", Type="IntProperty")]),
                dict(Name="ToAll", RpcId=3, Params=[dict(Name="Value", Type="IntProperty")]),
                dict(Name="SendPayload", RpcId=4,
                     Params=[dict(Name="Payload", Type="Struct:/Script/CoreUObject.Vector")]),
            ]
            props = [dict(Name="HP", Type=1, Condition=0, TypeSpec=""),
                     dict(Name="OwnerSecret", Type=1, Condition=2, TypeSpec=""),
                     dict(Name="Inventory", Type=11, Condition=0, TypeSpec="Array:Int")]
            final_value = 1888 if name == "Dormancy" else (1777 if name == "PacketLoss" else 0)
            for index, label in enumerate(labels):
                data = dict(Complete=True, Scenario=name, SessionId=session, Variant=variant,
                            Backend="Lua", ProcessId=100 + index,
                            IrisEnabled=variant == "Iris", PushModelEnabled=variant != "PushOff",
                            Cooked=variant == "Cooked",
                            NetStats=dict(OutTotalBytes=4096, InTotalBytes=2048, LostPackets=0),
                            RepSchemaVersion=12345, VariantRepSchemaVersion=54321,
                            Rpcs=rpcs, Props=props,
                            ClientsAtSpawn=1 if staggered else 2, ClientsFinal=2,
                            ConnectionDrops=1 if name in ("Reconnect", "SeamlessReconnect") else 0,
                            DormantValue=1777 if name == "Dormancy" else 0,
                            FlushedValue=final_value,
                            DormancyFlushed=name == "Dormancy",
                            RelevancyRestrictedActor=2 if name == "Relevancy" else 0,
                            JoinedAfterSpawn=(label == "Client1" and swapped)
                                             or (label == "Client2" and name == "LateJoin"),
                            DormancySuppressed=name == "Dormancy" and label != "Server",
                            DormancyResumed=name == "Dormancy" and label != "Server",
                            DormancyAfterValue=1888 if name == "Dormancy" else -1,
                            PacketLossPct=25 if name == "PacketLoss" else 0,
                            FinalValue=final_value if name == "PacketLoss" else -1,
                            ConnectionLevelReconnectCount=1
                                if name == "SeamlessReconnect" and label == "Client1" else 0,
                            ClosedConnectionAddress="127.0.0.1:5555"
                                if name == "SeamlessReconnect" and label == "Server" else "",
                            VisibleActorIds=[1] if name == "Relevancy" and label == "Client1" else [1, 2])
                (directory / f"{label}.json").write_text(json.dumps(data))
                (directory / "logs" / f"{label}.log").write_text("scenario peer log")

    def variant_evidence(self, run):
        (run / "metadata").mkdir(parents=True, exist_ok=True)
        (run / "metadata/net-variants.json").write_text(
            json.dumps(dict(variants=list(gate.NET_VARIANTS))))
        for name, spec in gate.NET_VARIANTS.items():
            directory = self.connected_evidence(run, f"net/variants/{name}/DedicatedServer")
            topology = json.loads((directory / "topology.json").read_text())
            topology["variant"] = name
            if spec["cooked"]:
                topology["exe"] = "Saved/StagedBuilds/WindowsServer/UnLuaNovaDemo/Binaries/Win64/DemoServer.exe"
            (directory / "topology.json").write_text(json.dumps(topology))
            for label in ("Server", "Client1", "Client2"):
                path = directory / f"{label}.json"
                data = json.loads(path.read_text())
                data.update(Variant=name, IrisEnabled=spec["iris"], Cooked=spec["cooked"],
                            PushModelEnabled=False if spec["push"] is False else True)
                path.write_text(json.dumps(data))

    def test_net_matrix_passes_only_with_scenarios_and_delivery_variants(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            self.assertTrue(gate.validate_net_matrix(run, "Lua"))
            self.variant_evidence(run)
            self.assertEqual(gate.validate_net_matrix(run, "Lua"), [])
            self.assertEqual(gate.validate_net_scenarios(run, "Lua"), [])
            self.assertEqual(gate.validate_net_variants(run, "Lua"), [])

    def test_net_scenarios_fail_closed_on_missing_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.assertTrue(any("missing or invalid evidence" in e
                                for e in gate.validate_net_scenarios(run, "Lua")))
            self.scenario_evidence(run)
            (run / "net/scenarios/Dormancy/Client1.json").unlink()
            self.assertTrue(gate.validate_net_scenarios(run, "Lua"))

    def test_late_join_requires_launch_order_and_preexisting_state(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            path = run / "net/scenarios/LateJoin/topology.json"
            topology = json.loads(path.read_text())
            for change in (dict(),):
                with self.subTest(change=change):
                    topology["peers"][2]["launch_serial"] = 1
                    path.write_text(json.dumps(topology))
                    self.assertTrue(any("launch order" in e for e in
                                        gate.validate_net_scenario(run, "LateJoin", "Lua")))
            topology["peers"][2]["launch_serial"] = 3
            path.write_text(json.dumps(topology))
            report = run / "net/scenarios/LateJoin/Client2.json"
            report.write_text(json.dumps(dict(json.loads(report.read_text()), JoinedAfterSpawn=False)))
            self.assertTrue(gate.validate_net_scenario(run, "LateJoin", "Lua"))

    def test_reconnect_requires_a_distinct_replacement_process(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            path = run / "net/scenarios/Reconnect/topology.json"
            topology = json.loads(path.read_text())
            topology["peers"][1]["launch_count"] = 1
            topology["peers"][1]["first_pid"] = 101
            path.write_text(json.dumps(topology))
            self.assertTrue(any("replacement process" in e for e in
                                gate.validate_net_scenario(run, "Reconnect", "Lua")))

    def test_dormancy_requires_suppression_then_recovery(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            path = run / "net/scenarios/Dormancy/Client1.json"
            data = json.loads(path.read_text())
            data.update(DormancySuppressed=False, DormancyAfterValue=1777)
            path.write_text(json.dumps(data))
            self.assertTrue(gate.validate_net_scenario(run, "Dormancy", "Lua"))

    def test_relevancy_requires_owner_only_visibility(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            path = run / "net/scenarios/Relevancy/Client1.json"
            data = json.loads(path.read_text())
            data["VisibleActorIds"] = [1, 2]
            path.write_text(json.dumps(data))
            self.assertTrue(any("non-owner" in e for e in
                                gate.validate_net_scenario(run, "Relevancy", "Lua")))

    def test_net_variants_reject_faked_iris_or_push_model_flags(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.variant_evidence(run)
            self.assertEqual(gate.validate_net_variants(run, "Lua"), [])
            for name, field, value in (("Iris", "IrisEnabled", False),
                                       ("PushOff", "PushModelEnabled", True),
                                       ("Legacy", "Variant", "Iris")):
                with self.subTest(variant=name):
                    path = run / f"net/variants/{name}/DedicatedServer/Client1.json"
                    data = json.loads(path.read_text())
                    data[field] = value
                    path.write_text(json.dumps(data))
                    self.assertTrue(gate.validate_net_variants(run, "Lua"))
                    data[field] = gate.NET_VARIANTS[name]["iris"] if field == "IrisEnabled" else (
                        False if field == "PushModelEnabled" else name)
                    path.write_text(json.dumps(data))
            manifest = run / "metadata/net-variants.json"
            manifest.write_text(json.dumps(dict(variants=["Legacy"])))
            self.assertTrue(gate.validate_net_variants(run, "Lua"))

    def test_cross_peer_schema_mismatch_is_rejected_everywhere(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            self.variant_evidence(run)
            self.assertEqual(gate.validate_net_matrix(run, "Lua"), [])
            path = run / "net/scenarios/SchemaMismatch/Client2.json"
            data = json.loads(path.read_text())
            data["RepSchemaVersion"] = 999
            path.write_text(json.dumps(data))
            self.assertTrue(any("cross-peer declaration mismatch" in e for e in
                                gate.validate_net_scenario(run, "SchemaMismatch", "Lua")))
            data["RepSchemaVersion"] = 12345
            data["VariantRepSchemaVersion"] = 12345
            path.write_text(json.dumps(data))
            self.assertTrue(gate.validate_net_scenario(run, "SchemaMismatch", "Lua"))
            # The same invariant holds in the ordinary connected gate.
            connected = run / "net/DedicatedServer"
            self.connected_evidence(run)
            data = json.loads((connected / "Client1.json").read_text())
            data["RepSchemaVersion"] = 999
            (connected / "Client1.json").write_text(json.dumps(data))
            self.assertTrue(any("Cross-peer declaration mismatch" in e for e in
                                gate.validate_connected_session(run, "Lua")))

    def test_connected_session_requires_generic_field_and_ustruct_rpc_param(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            connected = self.connected_evidence(run)
            self.assertEqual(gate.validate_connected_session(run, "Lua"), [])
            path = connected / "Server.json"
            data = json.loads(path.read_text())
            data["Props"] = [p for p in data["Props"] if not p.get("TypeSpec")]
            path.write_text(json.dumps(data))
            self.assertTrue(any("Generic replicated field" in e for e in
                                gate.validate_connected_session(run, "Lua")))
            # Restore the full declaration list, then remove only the UStruct RPC.
            data = json.loads((connected / "Client1.json").read_text())
            data["Rpcs"] = [r for r in data["Rpcs"] if r["Name"] != "SendPayload"]
            path.write_text(json.dumps(data))
            self.assertTrue(any("Missing connected RPC declarations" in e for e in
                                gate.validate_connected_session(run, "Lua")))

    def test_join_order_swap_and_packet_loss_require_distinct_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            self.assertEqual(gate.validate_net_scenario(run, "JoinOrderSwap", "Lua"), [])
            self.assertEqual(gate.validate_net_scenario(run, "PacketLoss", "Lua"), [])
            topology = run / "net/scenarios/JoinOrderSwap/topology.json"
            data = json.loads(topology.read_text())
            data["peers"][1]["launch_serial"] = 2
            data["peers"][2]["launch_serial"] = 3
            topology.write_text(json.dumps(data))
            self.assertTrue(any("launch order was not actually swapped" in e for e in
                                gate.validate_net_scenario(run, "JoinOrderSwap", "Lua")))
            path = run / "net/scenarios/PacketLoss/Client2.json"
            data = json.loads(path.read_text())
            data["FinalValue"] = 0
            path.write_text(json.dumps(data))
            self.assertTrue(any("did not converge" in e for e in
                                gate.validate_net_scenario(run, "PacketLoss", "Lua")))

    def test_seamless_reconnect_is_not_a_process_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.scenario_evidence(run)
            self.assertEqual(gate.validate_net_scenario(run, "SeamlessReconnect", "Lua"), [])
            topology = run / "net/scenarios/SeamlessReconnect/topology.json"
            data = json.loads(topology.read_text())
            data["peers"][1]["launch_count"] = 2
            data["peers"][1]["first_pid"] = 201
            topology.write_text(json.dumps(data))
            self.assertTrue(any("process replacement" in e for e in
                                gate.validate_net_scenario(run, "SeamlessReconnect", "Lua")))
            data["peers"][1]["launch_count"] = 1
            data["peers"][1]["first_pid"] = 101
            topology.write_text(json.dumps(data))
            path = run / "net/scenarios/SeamlessReconnect/Client1.json"
            report = json.loads(path.read_text())
            report["ConnectionLevelReconnectCount"] = 0
            path.write_text(json.dumps(report))
            self.assertTrue(gate.validate_net_scenario(run, "SeamlessReconnect", "Lua"))

    def test_cooked_variant_requires_staged_binary_and_compiled_flag(self):
        with tempfile.TemporaryDirectory() as directory:
            run = Path(directory)
            self.variant_evidence(run)
            self.assertEqual(gate.validate_net_variants(run, "Lua"), [])
            topology = run / "net/variants/Cooked/DedicatedServer/topology.json"
            data = json.loads(topology.read_text())
            data["exe"] = "Binaries/Win64/UnLuaNovaDemoEditor.exe"
            topology.write_text(json.dumps(data))
            self.assertTrue(any("staged/cooked binary" in e for e in
                                gate.validate_net_variants(run, "Lua")))
            data["exe"] = "Saved/StagedBuilds/WindowsServer/Demo/Binaries/Win64/DemoServer.exe"
            topology.write_text(json.dumps(data))
            path = run / "net/variants/Cooked/DedicatedServer/Client1.json"
            report = json.loads(path.read_text())
            report["Cooked"] = False
            path.write_text(json.dumps(report))
            self.assertTrue(gate.validate_net_variants(run, "Lua"))


if __name__ == "__main__":
    unittest.main()

"""Fail closed on missing release evidence. No UE invocation or baseline mutation."""

import argparse
from collections import Counter
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

# Minimum work is fixed here, not supplied by a potentially incomplete report.
MEMORY_CASES = {
    "vm_cycles": (24, 24, 0),
    "steady_heap": (1, 32, 0),
    "hot_reload": (1, 32, 0),
    "retained_reference": (1, 3, 0),
    "native_payloads": (24, 24, 60),
    "delegates": (24, 24, 72),
    "umg_resources": (24, 24, 48),
    "world_env_first": (12, 12, 36),
    "world_world_first": (12, 12, 36),
}


def read_json(path):
    # UE's AutoDetect writer emits UTF-16 when test names contain non-ASCII text.
    return json.loads(path.read_bytes())


def validate_automation(manifest, report, backend, carrier, source_tests=()):
    errors = []
    if manifest.get("backend") != backend:
        errors.append("Manifest backend differs from requested backend")
    if manifest.get("editor") is not (carrier == "editor"):
        errors.append("Manifest carrier mismatch")
    if manifest.get("cooked") is not (carrier == "game"):
        errors.append("Manifest cooked state mismatch")
    for feature in ("timer", "event", "oop"):
        if manifest.get(feature) is not True:
            errors.append(f"Required feature disabled: {feature}")
    expected = manifest.get("tests", [])
    if not isinstance(expected, list) or not expected or any(not isinstance(n, str) or not n.casefold().startswith("unluanova.") for n in expected):
        return errors + ["Invalid or empty runtime test manifest"]
    if len(expected) != len(set(expected)):
        errors.append("Duplicate manifest test paths")
    tests = report.get("tests", [])
    if not isinstance(tests, list) or not tests:
        return errors + ["Missing or empty automation tests"]
    actual = []
    for test in tests:
        name = test.get("fullTestPath")
        if not isinstance(name, str) or not name:
            errors.append("Test lacks fullTestPath; display names are not unique identifiers")
            continue
        actual.append(name)
        if str(test.get("state", "")).lower() != "success":
            errors.append(f"Incomplete or failing test: {name} ({test.get('state')})")
        entries = test.get("entries", [])
        if test.get("errors") or test.get("errorCount") or any(
            str(e.get("event", e).get("type", "")).lower() == "error" for e in entries
        ):
            errors.append(f"Test contains errors: {name}")
    for name, count in Counter(actual).items():
        if count != 1:
            errors.append(f"Duplicate executed test: {name}")
    for name in sorted(set(expected) - set(actual)):
        errors.append(f"Registered but not executed: {name}")
    for name in sorted(set(actual) - set(expected)):
        errors.append(f"Executed but absent from manifest: {name}")
    # Editor is the superset. Cooked exclusions come from UE's actual context flags.
    if carrier == "editor":
        for suite in source_tests:
            if backend == "Lua" and suite == "UnLuaNova.API.LuauIntegration":
                continue
            if not any(n == suite or n.startswith(suite + ".") for n in expected):
                errors.append(f"Source suite not registered: {suite}")
    return errors


def validate_typecheck(data):
    errors = []
    if data.get("skipped") is not False or not isinstance(data.get("files"), int) or data["files"] <= 0:
        errors.append("Type checker skipped or checked zero files")
    if data.get("exit") != 0:
        errors.append("Type checker exited nonzero or omitted exit status")
    if data.get("errors"):
        errors.append("Static type errors remain")
    if not data.get("defs") or not data.get("analyzer"):
        errors.append("Missing analyzer or declaration provenance")
    return errors


def validate_memory_case(data, name, backend, carrier, run_id):
    errors = []
    if type(data.get("schema_version")) is not int or data["schema_version"] != 2 or data.get("case") != name:
        errors.append("Memory schema/case mismatch")
    if data.get("backend") != backend or data.get("editor") is not (carrier == "editor") or data.get("cooked") is not (carrier == "game"):
        errors.append("Memory backend/carrier mismatch")
    if not run_id or data.get("run_id") != run_id:
        errors.append("Memory evidence belongs to another run")
    if data.get("passed") is not True:
        errors.append("Memory runtime assertions failed")
    fields = ("closed_envs", "allocation_events", "remaining_bytes", "remaining_blocks", "accounting_errors",
              "checked_resources", "retained_resources", "initial_envs", "final_envs", "warmup")
    if any(type(data.get(k)) is not int or data[k] < 0 for k in fields):
        return errors + ["Invalid or missing memory counters"]
    envs, sample_count, resources = MEMORY_CASES[name]
    if data["closed_envs"] < envs or data["allocation_events"] <= 0 or data["checked_resources"] < resources:
        errors.append("Memory workload incomplete")
    if any(data[k] != 0 for k in ("remaining_bytes", "remaining_blocks", "accounting_errors", "retained_resources")):
        errors.append("Unreleased memory/resources or allocator accounting errors")
    if data["initial_envs"] != data["final_envs"]:
        errors.append("Environment registry did not recover")
    samples = data.get("samples")
    if not isinstance(samples, list) or len(samples) != sample_count or any(
        not isinstance(s, dict) or any(type(s.get(k)) is not int or s[k] < 0 for k in ("bytes", "live_bytes", "blocks"))
        for s in samples
    ):
        return errors + ["Invalid or incomplete memory samples"]
    if name in ("steady_heap", "hot_reload"):
        if data["warmup"] < 8 or samples[0]["live_bytes"] == 0 or samples[0]["blocks"] == 0:
            errors.append("Missing warmed live-heap evidence")
        # Bound both high-water growth and late growth after the first sampled window.
        for key, limit in (("live_bytes", 8192), ("blocks", 16)):
            values = [s[key] for s in samples]
            if max(values) > min(values[:8]) + limit or sum(values[-8:]) > sum(values[:8]) + 8 * limit:
                errors.append(f"Post-GC {key} growth exceeds the warmed budget")
    elif name == "retained_reference":
        if (data.get("positive_control_detected") is not True
                or samples[1]["bytes"] <= samples[0]["bytes"] + 65536
                or samples[1]["blocks"] <= samples[0]["blocks"]
                or samples[2]["bytes"] > samples[0]["bytes"] + 8192):
            errors.append("Retained-reference control did not detect and release its graph")
    elif any(s["bytes"] or s["blocks"] for s in samples):
        errors.append("A create/destroy cycle left live VM allocations")
    return errors


def validate_memory(run, backend, carrier):
    manifest = read_json(run / "metadata/manifest.json")
    errors = []
    for name in MEMORY_CASES:
        try:
            data = read_json(run / f"metadata/memory/{name}.json")
            errors.extend(f"{name}: {e}" for e in validate_memory_case(data, name, backend, carrier, manifest.get("run_id")))
            if f"UnLuaNova.Memory.{name}" not in manifest.get("tests", []):
                errors.append(f"{name}: memory case absent from runtime manifest")
        except (OSError, ValueError, TypeError, AttributeError, KeyError) as exc:
            errors.append(f"{name}: missing or invalid memory evidence: {exc}")
    return errors


def validate_performance(actual, baseline, backend, require_samples=False):
    errors = []
    for field in ("Backend", "EngineVersion", "Machine"):
        if not actual.get(field) or actual.get(field) != baseline.get(field):
            errors.append(f"Performance baseline scope differs: {field}")
    if actual.get("Backend") != backend:
        errors.append("Performance artifact backend mismatch")
    maps = []
    for label, data in (("actual", actual), ("baseline", baseline)):
        rows = data.get("Cases", [])
        if not isinstance(rows, list) or not rows:
            errors.append(f"Empty performance {label}")
            maps.append({})
            continue
        values = {}
        for row in rows:
            name, value, iterations = row.get("Case"), row.get("PerOpUs"), row.get("Iterations")
            if not isinstance(name, str) or not name or name in values:
                errors.append(f"Missing/duplicate performance case in {label}")
                continue
            if not isinstance(value, (int, float)) or not math.isfinite(value) or value <= 0 or not isinstance(iterations, int) or iterations <= 0:
                errors.append(f"Invalid performance measurement: {label}/{name}")
                continue
            values[name] = row
        maps.append(values)
    current, previous = maps
    if require_samples or "Measurement" in actual:
        expected = dict(Method="median-of-5-batch-means", WarmupMinMs=250, SampleMinMs=100)
        if actual.get("Measurement") != expected:
            errors.append("Missing or unsupported performance sampling method")
        for name, row in current.items():
            warmup, batches = row.get("WarmupMs"), row.get("WarmupBatches")
            if (type(warmup) not in (int, float) or not math.isfinite(warmup) or warmup < 250
                    or type(batches) is not int or batches <= 0):
                errors.append(f"Insufficient performance warmup: {name}")
            samples = row.get("Samples")
            if not isinstance(samples, list) or len(samples) != 5:
                errors.append(f"Missing five performance samples: {name}")
                continue
            means = []
            for sample in samples:
                if not isinstance(sample, dict):
                    errors.append(f"Invalid performance sample: {name}")
                    continue
                elapsed, batches = sample.get("ElapsedMs"), sample.get("Batches")
                if (type(elapsed) not in (int, float) or not math.isfinite(elapsed) or elapsed < 100
                        or type(batches) is not int or batches <= 0):
                    errors.append(f"Insufficient performance sample: {name}")
                    continue
                means.append(elapsed * 1000 / batches / row["Iterations"])
            if len(means) == 5 and not math.isclose(row["PerOpUs"], sorted(means)[2], rel_tol=1e-9):
                errors.append(f"Performance summary differs from sample median: {name}")
    if current.keys() != previous.keys():
        errors.append("Performance case inventory changed; baseline cannot cover this run")
    for name in current.keys() & previous.keys():
        if current[name]["Iterations"] != previous[name]["Iterations"]:
            errors.append(f"Performance iteration count changed: {name}")
        if current[name]["PerOpUs"] > previous[name]["PerOpUs"] * 1.25:
            errors.append(f"Performance regression exceeds 25%: {name}")
    return errors


def validate_connected_session(run, backend):
    directory = run / "net/DedicatedServer"
    topology = read_json(directory / "topology.json")
    labels = ("Server", "Client1", "Client2")
    peers = topology.get("peers", [])
    errors = []
    if Counter(p.get("label") for p in peers) != Counter(labels):
        errors.append("DedicatedServer requires one server and two connected clients")
    reports = {label: read_json(directory / f"{label}.json") for label in labels}
    server = reports["Server"]
    session_id = server.get("SessionId", "")
    if len(session_id) != 32 or any(c not in "0123456789abcdefABCDEF" for c in session_id):
        errors.append("Missing server-generated session identity")
    reference = server.get("Rpcs", [])
    if {r.get("Name") for r in reference} != {"FromClient", "FromServer", "ToAll"} or any(
            not r.get("RpcId") for r in reference):
        errors.append("Missing connected RPC declarations")
    seeds = {a.get("ActorId"): a.get("Challenge") for a in server.get("Actors", [])}
    if set(seeds) != {1, 2} or any(type(seed) is not int or not 1000 <= seed <= 1002000 for seed in seeds.values()):
        return errors + ["Invalid server actor challenges"]
    if len(set(seeds.values())) != 2:
        errors.append("Actor challenges must differ")

    def check_actors(actors, label, owner, rounds, ack=False):
        if Counter(a.get("ActorId") for a in actors) != Counter([1, 2]):
            errors.append(f"Missing/duplicate replicated actors: {label}")
        for actor in actors:
            actor_id = actor.get("ActorId")
            if actor_id not in seeds:
                continue
            seed = seeds[actor_id]
            is_server = label == "Server"
            is_owner = actor_id == owner
            expected = ""
            for phase in range(rounds):
                value = seed + phase * 10
                if phase == 1:
                    if is_server:
                        expected += f"FromClient:{seed + 1}|"
                    elif is_owner:
                        expected += f"FromServer:{seed + 4}|"
                if is_server:
                    expected += f"FromClient:{value + 1}|"
                elif is_owner:
                    expected += f"FromServer:{value + 2}|"
                expected += f"ToAll:{value + 3}|"
            if ack:
                expected += f"FromClient:{seed + 11}|"
            if actor.get("SessionId") != session_id or actor.get("Challenge") != seed:
                errors.append(f"Actor is not from the same server session: {label}/{actor_id}")
            if actor.get("Authority") is not is_server or actor.get("Owned") is not is_owner:
                errors.append(f"Incorrect actor authority/ownership: {label}/{actor_id}")
            if actor.get("HpReadable") is not True or actor.get("HP") != seed + (rounds - 1) * 10 + 1:
                errors.append(f"Lua HP replication missing: {label}/{actor_id}/round{rounds}")
            if actor.get("CallLog") != expected or actor.get("CallCount") != expected.count("|"):
                errors.append(f"RPC delivery or owner isolation failed: {label}/{actor_id}/round{rounds}")

    owners, pids = [], []
    server_peer = next((p for p in peers if p.get("label") == "Server"), {})
    for label, data in reports.items():
        is_server = label == "Server"
        peer = next((p for p in peers if p.get("label") == label), {})
        if data.get("SchemaVersion") != 2 or data.get("Complete") is not True or data.get("Fault") != "":
            errors.append(f"Incomplete/fault-injected connected scenario: {label}")
        if data.get("Role") != label or data.get("Backend") != backend or data.get("SessionId") != session_id:
            errors.append(f"Connected session identity mismatch: {label}")
        if data.get("NetMode") != (1 if is_server else 3) or data.get("Connected") is not True:
            errors.append(f"Peer is not connected to gameplay: {label}")
        if data.get("WorldPackage") != "/UnLuaNovaTestSuite/Tests/Misc/Empty":
            errors.append(f"Wrong gameplay map: {label}")
        pid = data.get("ProcessId")
        pids.append(pid)
        if type(pid) is not int or pid <= 0 or pid != peer.get("pid"):
            errors.append(f"Report PID differs from launched peer: {label}")
        if data.get("Rpcs") != reference:
            errors.append(f"Connected RPC declarations differ: {label}")
        owner = data.get("OwnedActorId")
        delay = data.get("FirstRoundObservationMs")
        if label == "Client1" and (type(delay) not in (int, float) or not math.isfinite(delay) or delay < 5000):
            errors.append("Client1 must exercise the delayed first-round observation")
        if is_server:
            if data.get("Clients") != 2 or owner != 0:
                errors.append("Server must have two open player connections")
        else:
            owners.append(owner)
            expected_address = f"127.0.0.1:{server_peer.get('port')}"
            if peer.get("connect_to") != "127.0.0.1" or data.get("ServerAddress") != expected_address:
                errors.append(f"Client did not connect to the declared server endpoint: {label}")
        check_actors(data.get("Actors", []), label, owner, 2, ack=is_server)
        observations = data.get("Observations", [])
        if [o.get("Round") for o in observations] != [1, 2]:
            errors.append(f"Two sequential replication rounds required: {label}")
        for observation in observations:
            if observation.get("Round") in (1, 2):
                check_actors(observation.get("Actors", []), label, owner, observation["Round"])
        log = directory / "logs" / f"{label}.log"
        if not log.is_file() or log.stat().st_size == 0:
            errors.append(f"Missing connected peer log: {label}")
    if Counter(owners) != Counter([1, 2]):
        errors.append("Clients must own different server-spawned actors")
    if len(set(pids)) != 3:
        errors.append("Connected peers must be three distinct processes")
    return errors


def validate_network(run, backend):
    errors = []
    directory = run / "net/Multicast"
    topology = read_json(directory / "topology.json")
    peers = topology.get("peers", [])
    if Counter(p.get("label") for p in peers) != Counter(["Server", "Client1", "Client2"]):
        errors.append("Multicast topology must contain one server and two clients")
    reference = None
    for label in ("Server", "Client1", "Client2"):
        data = read_json(directory / f"{label}.json")
        if data.get("Backend") != backend or data.get("Role") != label:
            errors.append(f"Peer identity/backend mismatch: {label}")
        if data.get("NetMode") != (1 if label == "Server" else 3):
            errors.append(f"Peer has incorrect network mode: {label}")
        if label == "Server" and data.get("Clients", 0) < 2:
            errors.append("Two clients did not connect")
        if label != "Server" and data.get("MulticastSeen") is not True:
            errors.append(f"Multicast not received: {label}")
        rpcs = data.get("Rpcs", [])
        if not rpcs or any(not r.get("RpcId") for r in rpcs):
            errors.append(f"Missing RPC declaration evidence: {label}")
        if reference is not None and rpcs != reference:
            errors.append(f"RPC declarations differ across processes: {label}")
        reference = rpcs
        log = directory / "logs" / f"{label}.log"
        if not log.is_file() or log.stat().st_size == 0:
            errors.append(f"Missing peer log: {label}")
    errors.extend(validate_connected_session(run, backend))
    return errors


def validate_media(run):
    errors = []
    for name in ("Binding", "Override", "Container", "UMG"):
        for relative in (f"screenshots/Showcase_{name}.png", f"videos/Showcase_{name}_Clip.mp4"):
            path = run / relative
            if not path.is_file() or path.stat().st_size == 0:
                errors.append(f"Missing showcase artifact: {relative}")
    return errors


def validate_run(run, backend, carrier, root=ROOT):
    import ClassifyTests

    errors = []
    checks = {
        "automation": lambda: validate_automation(read_json(run / "metadata/manifest.json"),
            read_json(run / "index.json"), backend, carrier, ClassifyTests.scan()["tests"]),
        "network": lambda: validate_network(run, backend),
        "memory": lambda: validate_memory(run, backend, carrier),
    }
    if carrier == "editor":
        checks["typecheck"] = lambda: validate_typecheck(read_json(run / f"{backend.lower()}-analyze.json"))
        checks["performance"] = lambda: validate_performance(read_json(run / "metadata/perf.json"),
            read_json(root / f"Tools/baseline/perf-{backend.lower()}.json"), backend, require_samples=True)
        checks["media"] = lambda: validate_media(run)
    for name, check in checks.items():
        try:
            errors.extend(f"{name}: {e}" for e in check())
        except (OSError, ValueError, TypeError, AttributeError, KeyError) as exc:
            errors.append(f"{name}: missing or invalid evidence: {exc}")
    return errors


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", type=Path, required=True)
    parser.add_argument("--backend", choices=("Lua", "Luau"), required=True)
    parser.add_argument("--carrier", choices=("editor", "game"), required=True)
    args = parser.parse_args()
    errors = validate_run(args.run, args.backend, args.carrier)
    output = args.run / "metadata/release-gate.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps({"passed": not errors, "errors": errors}, ensure_ascii=False, indent=2), encoding="utf-8")
    for error in errors:
        print(error)
    print(f"Release evidence: {'FAILED' if errors else 'passed'} ({len(errors)} errors)")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())

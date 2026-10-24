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
    return json.loads(path.read_text(encoding="utf-8-sig"))


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
    if not isinstance(expected, list) or not expected or any(not isinstance(n, str) or not n.startswith("UnLuaNova.") for n in expected):
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
    if type(data.get("schema_version")) is not int or data["schema_version"] != 1 or data.get("case") != name:
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
        not isinstance(s, dict) or any(type(s.get(k)) is not int or s[k] < 0 for k in ("bytes", "blocks"))
        for s in samples
    ):
        return errors + ["Invalid or incomplete memory samples"]
    if name in ("steady_heap", "hot_reload"):
        if data["warmup"] < 8 or samples[0]["bytes"] == 0 or samples[0]["blocks"] == 0:
            errors.append("Missing warmed live-heap evidence")
        # Bound both high-water growth and late growth after the first sampled window.
        for key, limit in (("bytes", 8192), ("blocks", 16)):
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


def validate_performance(actual, baseline, backend):
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
    if current.keys() != previous.keys():
        errors.append("Performance case inventory changed; baseline cannot cover this run")
    for name in current.keys() & previous.keys():
        if current[name]["Iterations"] != previous[name]["Iterations"]:
            errors.append(f"Performance iteration count changed: {name}")
        if current[name]["PerOpUs"] > previous[name]["PerOpUs"] * 1.25:
            errors.append(f"Performance regression exceeds 25%: {name}")
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
            read_json(root / f"Tools/baseline/perf-{backend.lower()}.json"), backend)
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

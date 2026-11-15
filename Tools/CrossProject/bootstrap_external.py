#!/usr/bin/env python3
"""Bootstrap / readiness / status helper for the three-way cross-project benchmark.

    python -B Tools/CrossProject/bootstrap_external.py <prepare|check|status> [options]

This tool NEVER runs a benchmark and NEVER produces performance data. It only:

  * prepare - validate external_projects.json and (only when every pin is a real,
              fixed commit) clone/checkout the external projects into a gitignored
              location. With --offline/--dry-run it performs validation only, with
              no network and no writes, so the whole self-check runs offline.
  * check   - emit a STRUCTURED JSON report (stdout, read-only) of whether this host
              is ready to run the external projects: engine path, prepared checkouts,
              workload contract files, and whether alignment metadata can line up.
  * status  - emit a STRUCTURED, HONEST report of which projects have real run
              records in Tools/CrossProject/results/, and whether a cross-project
              conclusion exists. If results/ is empty it prints
              conclusion: "not-established". It NEVER infers speed.

Hard rules (enforced, not just documented):

  * Nothing here invents performance numbers, machine/CPU identity, engine versions,
    or commit hashes. Unknowns are reported as the string "UNKNOWN" or null with a
    diagnostic; they are never guessed.
  * status NEVER computes a ratio, never names a winner, never reads PerOpUs /
    Samples, and never infers "faster/slower". comparison is always null.
  * The README's 2.08x is UnLuaNova's Lua-5.5.1-vs-Luau self-comparison, NOT a
    comparison against slua-unreal / FUnLua. This tool repeats that boundary.
  * Every command has --dry-run / --offline semantics that run fully offline.
  * A network failure (reachable only from `prepare` without --offline/--dry-run and
    only after ALL pins are set) produces a diagnosable error and a non-zero exit
    code. It is never silently skipped.

Because every pin in the shipped manifest is "UNKNOWN", the default (online)
`prepare` refuses to check out a floating HEAD before it ever touches the network.
That is the fail-closed behaviour: a reproducible comparison requires fixed commits.
"""

from __future__ import annotations

import argparse
import datetime
import io
import json
import os
import platform
import re
import subprocess
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
if SCRIPT_DIR not in sys.path:
    sys.path.insert(0, SCRIPT_DIR)
import runner  # noqa: E402  (read-only reuse of the fail-closed alignment contract)

# SCRIPT_DIR = <repo>/Tools/CrossProject ; parents: <repo>/Tools -> <repo>
REPO_ROOT = os.path.dirname(os.path.dirname(SCRIPT_DIR))
MANIFEST_PATH = os.path.join(SCRIPT_DIR, "external_projects.json")
CANONICAL_RESULTS_DIR = os.path.join(SCRIPT_DIR, "results")
UNKNOWN = "UNKNOWN"
PROJECT_IDS = ("unluanova", "slua-unreal", "funlua")

# Keys every JSON report must carry. Tests assert against these sets so a field
# cannot be silently dropped; the docs list them as the machine-readable contract.
CHECK_REPORT_KEYS = frozenset((
    "command", "generated_utc", "schema_version", "root", "canonical_results_dir",
    "dry_run", "offline", "read_only", "network_used", "manifest", "engine", "host",
    "alignment", "projects", "checks", "diagnostics", "ready", "conclusion", "reason",
    "disclaimer", "comparison",
))
STATUS_REPORT_KEYS = frozenset((
    "command", "generated_utc", "schema_version", "root", "canonical_results_dir",
    "results_dir", "results_dir_exists", "dry_run", "offline", "read_only",
    "network_used", "result_files", "parsed_runs", "unrecognized_files", "projects",
    "projects_with_records", "projects_without_records", "all_three_have_records",
    "record_state", "conclusion", "reason", "comparison", "speed_claim",
    "cross_project_comparison_established", "readme_2_08x_note", "disclaimer",
    "next_step",
))
PREPARE_REPORT_KEYS = frozenset((
    "command", "generated_utc", "schema_version", "root", "dry_run", "offline",
    "mode", "manifest_valid", "manifest_problems", "pins_set", "network_used",
    "writes_performed", "actions", "ready", "conclusion", "reason", "disclaimer",
))

BOUNDARY_NOTE = (
    "The README's 2.08x is UnLuaNova's Lua-5.5.1-vs-Luau self-comparison on the "
    "historical UE 5.8.3 baseline, same machine and build. It is NOT a comparison "
    "against slua-unreal or FUnLua, and no 'faster than slua' claim is made."
)
NO_SPEED_DISCLAIMER = (
    "This command is read-only. It does not run benchmarks, does not read "
    "performance numbers, does not compute ratios, does not name a winner, and "
    "never infers relative speed. A cross-project conclusion is established only "
    "by aligned real runs processed fail-closed by runner.py and metrics.py."
)


class ManifestError(Exception):
    """Raised when external_projects.json cannot be loaded at all."""


def force_utf8():
    """Match runner.py: a GBK Windows console cannot encode some characters. All
    JSON emitted by this tool is pure ASCII (ensure_ascii=True), but keep this for
    the human-readable diagnostics written to stderr."""
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is None:
            continue
        try:
            reconfigure(encoding="utf-8")
        except Exception:
            pass


def now_utc():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def emit_json(obj):
    """Pure-ASCII JSON to stdout (ensure_ascii=True) -> safe on every console."""
    sys.stdout.write(json.dumps(obj, indent=2, ensure_ascii=True, sort_keys=False) + "\n")


def load_manifest(path=MANIFEST_PATH):
    try:
        with io.open(path, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (IOError, OSError) as error:
        raise ManifestError("cannot read manifest %s: %s" % (path, error))
    except ValueError as error:
        raise ManifestError("manifest %s is not valid JSON: %s" % (path, error))
    if not isinstance(data, dict):
        raise ManifestError("manifest %s must be a JSON object" % path)
    return data


# ---------------------------------------------------------------------------
# Manifest / structure validation (the "structure check")
# ---------------------------------------------------------------------------

REQUIRED_PROJECT_FIELDS = (
    "role", "repo_url", "pin", "pin_source", "license", "license_verified",
    "ue_version", "workload", "project_name_aliases", "expected_layout", "note",
)
REQUIRED_WORKLOAD_FIELDS = (
    "contract_source", "expected_entry_in_project", "entry_confirmed",
)
REQUIRED_LAYOUT_FIELDS = (
    "relative_checkout_dir", "required_markers", "confirmed",
)


def validate_manifest(manifest, repo_root=REPO_ROOT, check_files=True):
    """Return a list of human-readable problems; empty means the manifest and the
    in-repo workload contract files are structurally sound. No network, no writes."""
    problems = []
    if not isinstance(manifest, dict):
        return ["manifest is not a JSON object"]
    if "schema_version" not in manifest:
        problems.append("manifest is missing 'schema_version'")
    projects = manifest.get("projects")
    if not isinstance(projects, dict):
        problems.append("manifest is missing the 'projects' object")
        return problems

    for pid in PROJECT_IDS:
        if pid not in projects:
            problems.append("manifest is missing project '%s'" % pid)

    for pid, entry in sorted(projects.items()):
        if not isinstance(entry, dict):
            problems.append("project '%s' is not an object" % pid)
            continue
        for field in REQUIRED_PROJECT_FIELDS:
            if field not in entry:
                problems.append("project '%s' is missing field '%s'" % (pid, field))

        pin = entry.get("pin")
        if not isinstance(pin, str) or not pin:
            problems.append("project '%s' has an empty or non-string pin" % pid)
        elif pin != UNKNOWN and not re.fullmatch(r"[0-9a-fA-F]{40}", pin):
            problems.append(
                "project '%s' pin %r is neither 'UNKNOWN' nor a 40-hex commit hash"
                % (pid, pin))

        workload = entry.get("workload")
        if not isinstance(workload, dict):
            problems.append("project '%s' workload is not an object" % pid)
        else:
            for field in REQUIRED_WORKLOAD_FIELDS:
                if field not in workload:
                    problems.append("project '%s' workload is missing field '%s'"
                                    % (pid, field))
            if check_files:
                source = workload.get("contract_source")
                if not isinstance(source, str) or not source:
                    problems.append("project '%s' has no workload contract_source" % pid)
                else:
                    contract = os.path.join(repo_root, source)
                    if not os.path.isfile(contract):
                        problems.append(
                            "project '%s' workload contract file not found: %s"
                            % (pid, source))
                    else:
                        try:
                            with io.open(contract, "r", encoding="utf-8") as handle:
                                text = handle.read()
                            if runner.BEGIN not in text:
                                problems.append(
                                    "project '%s' contract file %s does not contain "
                                    "%s" % (pid, source, runner.BEGIN))
                        except (IOError, OSError) as error:
                            problems.append("project '%s' cannot read contract %s: %s"
                                            % (pid, source, error))

        layout = entry.get("expected_layout")
        if not isinstance(layout, dict):
            problems.append("project '%s' expected_layout is not an object" % pid)
        else:
            for field in REQUIRED_LAYOUT_FIELDS:
                if field not in layout:
                    problems.append("project '%s' expected_layout missing field '%s'"
                                    % (pid, field))
    return problems


def pins_all_set(manifest):
    """True only when every project carries a real, fixed 40-hex commit pin."""
    projects = manifest.get("projects", {}) if isinstance(manifest, dict) else {}
    if not isinstance(projects, dict) or not projects:
        return False
    for pid in PROJECT_IDS:
        entry = projects.get(pid)
        if not isinstance(entry, dict):
            return False
        pin = entry.get("pin")
        if not isinstance(pin, str) or pin == UNKNOWN or not re.fullmatch(r"[0-9a-fA-F]{40}", pin):
            return False
    return True


def _norm(value):
    return re.sub(r"[^a-z0-9]", "", str(value).lower())


def alias_index(manifest):
    """Map a normalized project name (as it may appear in a result JSON) to a
    manifest project id."""
    index = {}
    projects = manifest.get("projects", {}) if isinstance(manifest, dict) else {}
    if not isinstance(projects, dict):
        return index
    for pid, entry in projects.items():
        keys = {_norm(pid)}
        aliases = entry.get("project_name_aliases", []) if isinstance(entry, dict) else []
        for alias in aliases:
            keys.add(_norm(alias))
        for key in keys:
            index[key] = pid
    return index


# ---------------------------------------------------------------------------
# Engine resolution (never fabricated)
# ---------------------------------------------------------------------------

ENGINE_ENV_VARS = ("UE_ENGINE_PATH", "UNREAL_ENGINE_PATH", "UE_ROOT")


def resolve_engine(explicit=None, env=None):
    """Return a factual engine record. If no real engine path can be established,
    'found' is false, 'path' is null, and 'diagnostic' is an actionable error
    message. No engine installation is guessed and no version is invented."""
    if env is None:
        env = os.environ
    if explicit:
        if os.path.isdir(explicit):
            return {"path": os.path.abspath(explicit), "found": True,
                    "source": "--engine-path", "version": UNKNOWN,
                    "diagnostic": "engine path supplied; version is NOT inferred - "
                                  "confirm it is identical for all three projects"}
        return {"path": explicit, "found": False, "source": "--engine-path",
                "version": UNKNOWN,
                "diagnostic": "engine path was supplied but does not exist on disk: "
                              "%r; a real Unreal Engine build is required" % explicit}
    for var in ENGINE_ENV_VARS:
        value = env.get(var)
        if value:
            if os.path.isdir(value):
                return {"path": os.path.abspath(value), "found": True, "source": var,
                        "version": UNKNOWN,
                        "diagnostic": "engine path from %s; version is NOT inferred" % var}
            return {"path": value, "found": False, "source": var, "version": UNKNOWN,
                    "diagnostic": "%s is set to %r but that directory does not exist"
                                  % (var, value)}
    return {"path": None, "found": False, "source": "none", "version": UNKNOWN,
            "diagnostic": "engine path not found: pass --engine-path or set one of "
                          "%s. A real Unreal Engine build is required to run the "
                          "external projects; this tool never guesses an installation."
                          % " / ".join(ENGINE_ENV_VARS)}


# ---------------------------------------------------------------------------
# results/ scanning (factual metadata only - NO performance numbers)
# ---------------------------------------------------------------------------

def scan_results(results_dir):
    """List result files and extract ONLY factual metadata from normalized runs.
    PerOpUs / Samples / Metrics values are deliberately never read."""
    files = []
    if os.path.isdir(results_dir):
        for name in sorted(os.listdir(results_dir)):
            if name.lower().endswith(".json") and os.path.isfile(os.path.join(results_dir, name)):
                files.append(name)

    parsed = []
    unrecognized = []
    for name in files:
        path = os.path.join(results_dir, name)
        try:
            with io.open(path, "r", encoding="utf-8") as handle:
                data = json.load(handle)
        except (ValueError, IOError, OSError) as error:
            unrecognized.append({"file": name, "reason": "not valid JSON: %s" % error})
            continue
        if (not isinstance(data, dict) or "Project" not in data
                or not isinstance(data.get("Cases"), list)):
            unrecognized.append({"file": name,
                                 "reason": "not a normalized run (missing Project / Cases)"})
            continue
        parsed.append({
            "file": name,
            "project": data.get("Project"),
            "backend": data.get("Backend"),
            "engine_version": data.get("EngineVersion"),
            "machine": data.get("Machine"),
            "cpu": data.get("Cpu"),
            "build_config": data.get("BuildConfig"),
            "carrier": data.get("Carrier"),
            "workload_revision": data.get("WorkloadRevision"),
            "measured_utc": data.get("MeasuredUtc"),
            "source_log": data.get("SourceLog"),
            "case_count": len(data.get("Cases", [])),
        })
    return {"result_files": files, "parsed_runs": parsed,
            "unrecognized_files": unrecognized}


# ---------------------------------------------------------------------------
# Report builders
# ---------------------------------------------------------------------------

def _alignment_contract():
    return {
        "can_align": False,
        "reason": "no real run records exist on this host and the required engine / "
                  "machine / config metadata are not established; they are operator-"
                  "supplied at run time, never fabricated by this tool",
        "required_identical_fields": list(runner.ALIGNMENT_FIELDS),
        "required_present_fields": list(runner.REQUIRED_META),
        "placeholder_values_refused": sorted(runner.PLACEHOLDER_VALUES),
        "non_time_metrics": {
            "names": list(runner.DEFAULT_METRICS),
            "units": {name: runner.METRIC_UNITS[name] for name in runner.DEFAULT_METRICS},
            "valid_units": list(runner.VALID_UNITS),
            "unavailable_sentinel": "unavailable (never 0, never a placeholder)",
        },
        "fail_closed": True,
        "source": "runner.py (ALIGNMENT_FIELDS / REQUIRED_META / metric contract) "
                  "and metrics.py (check_metric_alignment / check_alignment)",
        "note": "runner.py / metrics.py refuse to produce a table when any of these "
                "differ across runs; see Docs/EN/CrossProject_Benchmark.md",
    }


def _project_check_record(pid, entry, root):
    layout = entry.get("expected_layout", {}) if isinstance(entry, dict) else {}
    rel = layout.get("relative_checkout_dir", "")
    checkout_dir = os.path.abspath(os.path.join(root, rel)) if rel else root
    checkout_exists = os.path.isdir(checkout_dir)
    markers = []
    for marker in layout.get("required_markers", []) or []:
        markers.append({
            "marker": marker,
            "present": checkout_exists and os.path.exists(os.path.join(checkout_dir, marker)),
        })
    workload = entry.get("workload", {}) if isinstance(entry, dict) else {}
    source = workload.get("contract_source", "")
    contract_path = os.path.join(REPO_ROOT, source) if source else ""
    contract_exists = bool(source) and os.path.isfile(contract_path)
    return {
        "id": pid,
        "role": entry.get("role", UNKNOWN) if isinstance(entry, dict) else UNKNOWN,
        "pin": entry.get("pin", UNKNOWN) if isinstance(entry, dict) else UNKNOWN,
        "pin_set": bool(isinstance(entry, dict) and entry.get("pin") not in (None, "", UNKNOWN)),
        "license": entry.get("license", UNKNOWN) if isinstance(entry, dict) else UNKNOWN,
        "license_verified": bool(isinstance(entry, dict) and entry.get("license_verified")),
        "ue_version": entry.get("ue_version", UNKNOWN) if isinstance(entry, dict) else UNKNOWN,
        "prepared": checkout_exists and all(m["present"] for m in markers) if markers else checkout_exists,
        "checkout_dir": checkout_dir,
        "checkout_dir_exists": checkout_exists,
        "structure_markers": markers,
        "layout_confirmed": bool(isinstance(entry, dict) and layout.get("confirmed")),
        "workload_contract_source": source,
        "workload_contract_source_exists": contract_exists,
        "workload_entry_in_project": workload.get("expected_entry_in_project", UNKNOWN)
                                     if isinstance(workload, dict) else UNKNOWN,
        "workload_entry_confirmed": bool(isinstance(workload, dict)
                                         and workload.get("entry_confirmed")),
    }


def build_check_report(root, manifest, manifest_problems, engine, dry_run, offline):
    projects = []
    for pid in PROJECT_IDS:
        entry = manifest.get("projects", {}).get(pid, {})
        projects.append(_project_check_record(pid, entry, root))

    pins_set = pins_all_set(manifest)
    all_prepared = all(p["prepared"] for p in projects)
    contract_files_ok = all(p["workload_contract_source_exists"] for p in projects)

    checks = [
        {"name": "manifest_valid", "ok": not manifest_problems,
         "detail": "manifest and in-repo workload contract files are valid"
                   if not manifest_problems else "; ".join(manifest_problems)},
        {"name": "engine_path", "ok": engine["found"], "detail": engine["diagnostic"]},
        {"name": "pins_set", "ok": pins_set,
         "detail": "all three projects carry a fixed commit pin"
                   if pins_set else "one or more pins are UNKNOWN; fixed commits are "
                                    "required for a reproducible checkout"},
        {"name": "external_projects_prepared", "ok": all_prepared,
         "detail": "every project checkout exists with its workload entry"
                   if all_prepared else "external project checkouts are not prepared "
                                        "(no real clone/checkout has been done)"},
        {"name": "workload_contract_files", "ok": contract_files_ok,
         "detail": "all in-repo workload contract files exist and carry the BEGIN marker"
                   if contract_files_ok else "a workload contract file is missing"},
        {"name": "metadata_alignable", "ok": False,
         "detail": "engine / machine / cpu / config / carrier metadata is operator-"
                   "supplied at run time and cannot be established by an offline "
                   "check; no alignment is claimed"},
    ]
    diagnostics = [c["detail"] for c in checks if not c["ok"]]
    ready = all(c["ok"] for c in checks)

    return {
        "command": "check",
        "generated_utc": now_utc(),
        "schema_version": 1,
        "root": os.path.abspath(root),
        "canonical_results_dir": CANONICAL_RESULTS_DIR,
        "dry_run": bool(dry_run),
        "offline": bool(offline or dry_run),
        "read_only": True,
        "network_used": False,
        "manifest": {
            "path": MANIFEST_PATH,
            "valid": not manifest_problems,
            "problems": list(manifest_problems),
            "project_count": len(manifest.get("projects", {})),
        },
        "engine": engine,
        "host": {
            "platform": platform.platform(),
            "python_version": platform.python_version(),
            "machine_identity": UNKNOWN,
            "cpu_identity": UNKNOWN,
            "note": "machine / cpu identity is supplied by the operator at run time "
                    "(SetMachineInfo in the workloads). It is never fabricated here.",
        },
        "alignment": _alignment_contract(),
        "projects": projects,
        "checks": checks,
        "diagnostics": diagnostics,
        "ready": ready,
        "conclusion": "not-established",
        "reason": "This is a readiness report only. No cross-project benchmark has "
                  "been run, so no cross-project conclusion exists.",
        "disclaimer": NO_SPEED_DISCLAIMER,
        "comparison": None,
    }


def build_status_report(root, results_dir, manifest, dry_run, offline):
    scan = scan_results(results_dir)
    index = alias_index(manifest)
    projects = {pid: {"id": pid, "has_real_run_record": False, "run_count": 0,
                      "records": []} for pid in PROJECT_IDS}
    unmapped = []
    for run in scan["parsed_runs"]:
        pid = index.get(_norm(run["project"]))
        if pid is None:
            unmapped.append(run)
            continue
        projects[pid]["has_real_run_record"] = True
        projects[pid]["run_count"] += 1
        projects[pid]["records"].append(run)

    with_records = [pid for pid in PROJECT_IDS if projects[pid]["has_real_run_record"]]
    without_records = [pid for pid in PROJECT_IDS if not projects[pid]["has_real_run_record"]]
    all_three = len(with_records) == len(PROJECT_IDS)

    if not scan["result_files"]:
        record_state = "empty"
        reason = ("results directory is empty or missing; no real run records exist "
                  "for any project")
    elif not all_three:
        record_state = "partial"
        reason = ("only %d of %d projects have real run records (%s)"
                  % (len(with_records), len(PROJECT_IDS), ", ".join(with_records) or "none"))
    else:
        record_state = "all-three-present"
        reason = ("all three projects have real run records, but status does NOT "
                  "validate alignment or compute a comparison; a fail-closed "
                  "runner.py / metrics.py pass is still required")

    report = {
        "command": "status",
        "generated_utc": now_utc(),
        "schema_version": 1,
        "root": os.path.abspath(root),
        "canonical_results_dir": CANONICAL_RESULTS_DIR,
        "results_dir": os.path.abspath(results_dir),
        "results_dir_exists": os.path.isdir(results_dir),
        "dry_run": bool(dry_run),
        "offline": bool(offline or dry_run),
        "read_only": True,
        "network_used": False,
        "result_files": scan["result_files"],
        "parsed_runs": scan["parsed_runs"],
        "unrecognized_files": scan["unrecognized_files"],
        "unmapped_runs": unmapped,
        "projects": projects,
        "projects_with_records": with_records,
        "projects_without_records": without_records,
        "all_three_have_records": all_three,
        "record_state": record_state,
        # status NEVER establishes a speed conclusion, under any record state.
        "conclusion": "not-established",
        "reason": reason,
        "comparison": None,
        "speed_claim": None,
        "cross_project_comparison_established": False,
        "readme_2_08x_note": BOUNDARY_NOTE,
        "disclaimer": NO_SPEED_DISCLAIMER,
        "next_step": ("To read the canonical results, pass the repository root as "
                      "--root (or use --results-dir). A comparison table is legitimate "
                      "only after all three projects have aligned real runs processed "
                      "fail-closed by runner.py and metrics.py; see "
                      "Docs/EN/CrossProject_Benchmark.md"),
    }
    return report


# ---------------------------------------------------------------------------
# prepare: validation/plan (offline/dry-run) and real checkout (online, gated)
# ---------------------------------------------------------------------------

def _prepare_actions(root, manifest, will_execute):
    actions = []
    projects = manifest.get("projects", {})
    pins_set = pins_all_set(manifest)
    for pid in PROJECT_IDS:
        entry = projects.get(pid, {}) if isinstance(projects, dict) else {}
        is_external = entry.get("role") == "external" if isinstance(entry, dict) else False
        layout = entry.get("expected_layout", {}) if isinstance(entry, dict) else {}
        rel = layout.get("relative_checkout_dir", "")
        destination = os.path.abspath(os.path.join(root, rel)) if rel else root
        pin = entry.get("pin", UNKNOWN) if isinstance(entry, dict) else UNKNOWN
        if not is_external:
            actions.append({
                "project": pid, "action": "none (this-repo)", "destination": destination,
                "status": "not-applicable",
                "reason": "UnLuaNova is this repository; it is not cloned",
            })
            continue
        if pin == UNKNOWN or not pins_set:
            actions.append({
                "project": pid, "action": "clone+checkout",
                "would_clone_from": entry.get("repo_url", UNKNOWN),
                "would_pin_to": pin, "destination": destination,
                "status": "blocked",
                "reason": "pin is UNKNOWN; a fixed commit is required before any "
                          "reproducible checkout (fail-closed, no network attempted)",
            })
        elif will_execute:
            actions.append({
                "project": pid, "action": "clone+checkout",
                "would_clone_from": entry.get("repo_url", UNKNOWN),
                "would_pin_to": pin, "destination": destination,
                "status": "will-execute",
                "reason": "fixed pin present; checkout will be attempted",
            })
        else:
            actions.append({
                "project": pid, "action": "clone+checkout",
                "would_clone_from": entry.get("repo_url", UNKNOWN),
                "would_pin_to": pin, "destination": destination,
                "status": "planned",
                "reason": "validation-only mode: no network and no writes performed",
            })
    return actions


def build_prepare_plan(root, manifest, manifest_problems, dry_run, offline):
    actions = _prepare_actions(root, manifest, will_execute=False)
    pins_set = pins_all_set(manifest)
    return {
        "command": "prepare",
        "generated_utc": now_utc(),
        "schema_version": 1,
        "root": os.path.abspath(root),
        "dry_run": bool(dry_run),
        "offline": bool(offline or dry_run),
        "mode": "validation-only",
        "manifest_valid": not manifest_problems,
        "manifest_problems": list(manifest_problems),
        "pins_set": pins_set,
        "network_used": False,
        "writes_performed": False,
        "actions": actions,
        "ready": pins_set and not manifest_problems,
        "conclusion": "not-established",
        "reason": ("validation-only self-check; no checkout was performed. Pins are "
                   + ("set" if pins_set else "UNKNOWN") + ", so online prepare would "
                   + ("clone/checkout" if pins_set else "refuse to clone (fail-closed)")),
        "disclaimer": NO_SPEED_DISCLAIMER,
    }


def git_clone_checkout(repo_url, pin, destination):
    """Attempt a real clone + detached checkout. Returns (returncode, message).
    Any failure (git missing, network, bad pin, timeout) is returned as a
    diagnosable error - never a silent skip."""
    try:
        if not os.path.isdir(destination):
            parent = os.path.dirname(destination)
            if parent and not os.path.isdir(parent):
                os.makedirs(parent)
            clone = subprocess.run(
                ["git", "clone", "--no-checkout", repo_url, destination],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                universal_newlines=True, timeout=300)
            if clone.returncode != 0:
                return 1, ("git clone failed for %r (exit %d): %s"
                           % (repo_url, clone.returncode, (clone.stderr or "").strip()))
        checkout = subprocess.run(
            ["git", "-C", destination, "checkout", "--detach", pin],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            universal_newlines=True, timeout=120)
        if checkout.returncode != 0:
            return 1, ("git checkout of pin %r failed in %s (exit %d): %s"
                       % (pin, destination, checkout.returncode,
                          (checkout.stderr or "").strip()))
    except FileNotFoundError:
        return 1, "git executable not found on PATH; install git to run prepare online"
    except subprocess.TimeoutExpired as error:
        return 1, "git operation timed out: %s" % error
    except OSError as error:
        return 1, "git operation failed: %s" % error
    return 0, "ok"


def execute_prepare(root, manifest, manifest_problems):
    """Online prepare. Refuses before any network if the manifest is invalid or
    any pin is UNKNOWN. Network failures produce a diagnosable error and rc=1."""
    actions = _prepare_actions(root, manifest, will_execute=True)
    pins_set = pins_all_set(manifest)

    if manifest_problems or not pins_set:
        emit_json({
            "command": "prepare", "generated_utc": now_utc(), "schema_version": 1,
            "root": os.path.abspath(root), "dry_run": False, "offline": False,
            "mode": "execute", "manifest_valid": not manifest_problems,
            "manifest_problems": list(manifest_problems), "pins_set": pins_set,
            "network_used": False, "writes_performed": False, "actions": actions,
            "ready": False, "conclusion": "not-established",
            "reason": ("refusing to clone: " +
                       ("manifest is invalid; " if manifest_problems else "") +
                       ("one or more pins are UNKNOWN. Fill a real, fixed commit for "
                        "every project after a genuine checkout before running prepare "
                        "online (fail-closed). No network was used.")),
            "disclaimer": NO_SPEED_DISCLAIMER,
        })
        sys.stderr.write(
            "prepare: REFUSED before any network access. "
            + ("manifest has problems; " if manifest_problems else "")
            + "one or more pins are UNKNOWN - fixed commits are required for a "
              "reproducible checkout. See external_projects.json.\n")
        return 1

    # Only reachable when ALL pins are real commits. Perform real checkouts.
    for action in actions:
        if action["action"] != "clone+checkout":
            continue
        returncode, message = git_clone_checkout(
            action["would_clone_from"], action["would_pin_to"], action["destination"])
        if returncode != 0:
            action["status"] = "failed"
            action["reason"] = message
            emit_json({
                "command": "prepare", "generated_utc": now_utc(), "schema_version": 1,
                "root": os.path.abspath(root), "dry_run": False, "offline": False,
                "mode": "execute", "manifest_valid": True, "manifest_problems": [],
                "pins_set": True, "network_used": True, "writes_performed": True,
                "actions": actions, "ready": False, "conclusion": "not-established",
                "reason": "network/checkout failure (diagnosable, not skipped): " + message,
                "disclaimer": NO_SPEED_DISCLAIMER,
            })
            sys.stderr.write("prepare: ERROR - %s\n" % message)
            return 1
        action["status"] = "done"

    emit_json({
        "command": "prepare", "generated_utc": now_utc(), "schema_version": 1,
        "root": os.path.abspath(root), "dry_run": False, "offline": False,
        "mode": "execute", "manifest_valid": True, "manifest_problems": [],
        "pins_set": True, "network_used": True, "writes_performed": True,
        "actions": actions, "ready": True, "conclusion": "not-established",
        "reason": "external projects checked out at fixed pins; running the benchmark "
                  "is still a separate manual step (no performance data produced)",
        "disclaimer": NO_SPEED_DISCLAIMER,
    })
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def build_parser():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("prepare", "check", "status"):
        p = sub.add_parser(name, help="%s the three-way cross-project benchmark" % name)
        p.add_argument("--root", default=REPO_ROOT,
                       help="workspace/repository root (default: this repository root). "
                            "prepare clones into <root>/Saved/CrossProject/external/; "
                            "status reads <root>/Tools/CrossProject/results/")
        p.add_argument("--dry-run", action="store_true",
                       help="no network, no writes, no benchmark; just report")
        p.add_argument("--offline", action="store_true",
                       help="validation-only, never touch the network")
        p.add_argument("--manifest", default=MANIFEST_PATH,
                       help="path to external_projects.json")
        p.add_argument("--results-dir", default=None,
                       help="status: explicit results directory override")
        p.add_argument("--engine-path", default=None,
                       help="check: explicit Unreal Engine root (never guessed)")
    return parser


def main(argv=None):
    force_utf8()
    args = build_parser().parse_args(argv)
    root = os.path.abspath(args.root)

    try:
        manifest = load_manifest(args.manifest)
    except ManifestError as error:
        emit_json({"command": args.command, "generated_utc": now_utc(),
                   "error": str(error), "conclusion": "not-established",
                   "disclaimer": NO_SPEED_DISCLAIMER})
        sys.stderr.write("bootstrap_external: %s\n" % error)
        return 1

    manifest_problems = validate_manifest(manifest, check_files=True)

    if args.command == "check":
        engine = resolve_engine(args.engine_path)
        emit_json(build_check_report(root, manifest, manifest_problems, engine,
                                     args.dry_run, args.offline))
        return 0

    if args.command == "status":
        results_dir = args.results_dir or os.path.join(root, "Tools", "CrossProject", "results")
        emit_json(build_status_report(root, results_dir, manifest,
                                      args.dry_run, args.offline))
        return 0

    if args.command == "prepare":
        if args.offline or args.dry_run:
            emit_json(build_prepare_plan(root, manifest, manifest_problems,
                                         args.dry_run, args.offline))
            return 0
        return execute_prepare(root, manifest, manifest_problems)

    return 1


if __name__ == "__main__":
    raise SystemExit(main())

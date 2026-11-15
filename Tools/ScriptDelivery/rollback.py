#!/usr/bin/env python3
"""Maintain versions/<version>/ plus an atomic .current pointer.

Usage:
    python -B rollback.py add     --repo <dir> --package <dir> [--key <hex> | --key-file <path>]
    python -B rollback.py list    --repo <dir>
    python -B rollback.py current --repo <dir>
    python -B rollback.py switch  --repo <dir> --version <int> [--key <hex> | --key-file <path>]

`add` registers a built package as one immutable version. `switch` validates the
target package's integrity (and, when a key is supplied, its signature) before
atomically replacing .current via a temp file + os.replace. Switching to a
version that is not present in history is refused, so a rollback can never point
at a non-existent build. Lowering the version to an existing older build is a
legitimate rollback and is allowed.

This is a single-node pointer tool. Grey-release bucket assignment (per device
or per user) is an upstream policy decision described in
Docs/CN/ScriptDelivery.md; it is not implemented here.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import sys
from pathlib import Path

from verify_package import (
    CURRENT_NAME,
    CURRENT_SCHEMA,
    MANIFEST_NAME,
    SIGNATURE_NAME,
    VERSIONS_DIR,
    ZIP_NAME,
    ManifestError,
    canonical_bytes,
    load_manifest,
    optional_key,
    verify_integrity,
    verify_signature,
)

PACKAGE_FILES = (MANIFEST_NAME, ZIP_NAME, SIGNATURE_NAME)


def _version_dir(repo: Path, version: int) -> Path:
    return Path(repo) / VERSIONS_DIR / str(version)


def add_version(repo, package_dir, key=None) -> int:
    """Register a package as an immutable version; return its version number."""
    repo, package_dir = Path(repo), Path(package_dir)
    manifest = load_manifest(package_dir / MANIFEST_NAME)
    verify_integrity(package_dir, manifest)
    if key is not None:
        verify_signature(package_dir, manifest, key)
    version = manifest["version"]
    target = _version_dir(repo, version)
    if target.exists():
        raise ManifestError("version %d is already registered" % version)
    target.mkdir(parents=True, exist_ok=False)
    for name in PACKAGE_FILES:
        src = package_dir / name
        if src.is_file():
            shutil.copyfile(src, target / name)
    return version


def list_versions(repo) -> list:
    """Return sorted registered versions, validating dir name vs manifest."""
    root = Path(repo) / VERSIONS_DIR
    if not root.is_dir():
        return []
    versions = []
    for entry in root.iterdir():
        if not entry.is_dir() or not entry.name.isdigit():
            continue
        mf = entry / MANIFEST_NAME
        if mf.is_file():
            manifest = load_manifest(mf)
            if manifest["version"] != int(entry.name):
                raise ManifestError(
                    "version directory %s disagrees with manifest version %d"
                    % (entry.name, manifest["version"])
                )
        versions.append(int(entry.name))
    return sorted(versions)


def read_current(repo):
    """Return the current version int, or None when no pointer exists."""
    path = Path(repo) / CURRENT_NAME
    if not path.is_file():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ManifestError("corrupt .current pointer: %s" % exc) from exc
    if not isinstance(data, dict) or data.get("schema") != CURRENT_SCHEMA:
        raise ManifestError("corrupt .current pointer (bad schema)")
    version = data.get("version")
    if not isinstance(version, int) or isinstance(version, bool) or version < 0:
        raise ManifestError("corrupt .current pointer (bad version)")
    return version


def switch_version(repo, version, key=None) -> int:
    """Validate and atomically switch .current to an existing version."""
    repo = Path(repo)
    if not isinstance(version, int) or isinstance(version, bool) or version < 0:
        raise ManifestError("version must be a non-negative integer")
    available = list_versions(repo)
    if version not in available:
        raise ManifestError(
            "cannot switch to version %d: not present in history %r"
            % (version, available)
        )
    target = _version_dir(repo, version)
    manifest = load_manifest(target / MANIFEST_NAME)
    if manifest["version"] != version:
        raise ManifestError("version directory/manifest mismatch")
    verify_integrity(target, manifest)
    if key is not None:
        verify_signature(target, manifest, key)

    pointer = {"schema": CURRENT_SCHEMA, "version": version}
    data = canonical_bytes(pointer)
    repo.mkdir(parents=True, exist_ok=True)
    tmp = repo / (".current.tmp.%d" % os.getpid())
    try:
        tmp.write_bytes(data)
        os.replace(tmp, repo / CURRENT_NAME)  # atomic on the same filesystem
    finally:
        if tmp.exists():
            tmp.unlink()
    return version


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Version registry with atomic .current pointer (rollback tool)."
    )
    sub = ap.add_subparsers(dest="command", required=True)

    p_add = sub.add_parser("add", help="register a built package as a version")
    p_add.add_argument("--repo", required=True)
    p_add.add_argument("--package", required=True)
    p_add.add_argument("--key", help="optional hex key for signature check")
    p_add.add_argument("--key-file", help="optional hex key file for signature check")

    p_list = sub.add_parser("list", help="list registered versions")
    p_list.add_argument("--repo", required=True)

    p_cur = sub.add_parser("current", help="print the current version")
    p_cur.add_argument("--repo", required=True)

    p_sw = sub.add_parser("switch", help="validate and switch .current")
    p_sw.add_argument("--repo", required=True)
    p_sw.add_argument("--version", required=True, type=int)
    p_sw.add_argument("--key", help="optional hex key for signature check")
    p_sw.add_argument("--key-file", help="optional hex key file for signature check")

    args = ap.parse_args(argv)
    try:
        if args.command == "add":
            version = add_version(args.repo, args.package, optional_key(args))
            print("ADD OK: registered version %d" % version)
        elif args.command == "list":
            versions = list_versions(args.repo)
            current = read_current(args.repo)
            print("VERSIONS: %s" % (", ".join(str(v) for v in versions) or "(none)"))
            print("CURRENT: %s" % (current if current is not None else "(none)"))
        elif args.command == "current":
            current = read_current(args.repo)
            print("CURRENT: %s" % (current if current is not None else "(none)"))
        elif args.command == "switch":
            version = switch_version(args.repo, args.version, optional_key(args))
            print("SWITCH OK: .current -> version %d" % version)
    except (ManifestError, OSError) as exc:
        print("ROLLBACK FAILED: %s" % exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())

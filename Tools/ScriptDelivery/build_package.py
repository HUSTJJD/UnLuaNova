#!/usr/bin/env python3
"""Build a deterministic SOURCE-script package plus its manifest.json.

Usage:
    python -B build_package.py --source <dir> --out <dir> --version <int> \
        [--previous <manifest.json>] [--engine-compat <str>] [--backend <str>] \
        [--created-utc <iso8601>]

Only the Python standard library is used. The zip is written with ZIP_STORED
(no compression, so no zlib-version differences), sorted entries and fixed
metadata (timestamp 1980-01-01, fixed permissions). Identical input with the
same --created-utc therefore yields byte-identical package.zip and
manifest.json on Windows and Linux.

The payload is SOURCE scripts, never bytecode: the Luau backend explicitly
refuses bytecode input and makes no bytecode/version compatibility promise
(see Docs/CN/Luau.md). Manifest engine_compat/backend are metadata strings and
do NOT imply any UE-runtime validation.
"""

from __future__ import annotations

import argparse
import sys
import zipfile
from datetime import datetime, timezone
from pathlib import Path

from verify_package import (
    MANIFEST_NAME,
    RESERVED_NAMES,
    SCHEMA,
    ZIP_NAME,
    ManifestError,
    canonical_bytes,
    load_manifest,
    sha256_bytes,
    sha256_file,
    validate_manifest,
)

# Fixed zip timestamp (the minimum DOS date) so builds are time-independent.
FIXED_DATE_TIME = (1980, 1, 1, 0, 0, 0)


def _utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def collect_files(source: Path, exclude_dir=None):
    """Return a sorted list of (relative_posix_path, bytes) for the payload.

    Hidden files/directories (e.g. .git, .current) and our own reserved metadata
    file names are skipped. Paths always use forward slashes so a zip built on
    Windows and one built on Linux are identical.
    """
    source = Path(source).resolve()
    if not source.is_dir():
        raise ManifestError("source directory not found: %s" % source)
    exclude = Path(exclude_dir).resolve() if exclude_dir is not None else None
    collected = []
    for path in sorted(source.rglob("*")):
        if not path.is_file():
            continue
        if exclude is not None:
            try:
                path.resolve().relative_to(exclude)
                continue  # never package our own output directory
            except ValueError:
                pass
        rel = path.relative_to(source)
        if any(part.startswith(".") for part in rel.parts):
            continue
        if rel.name in RESERVED_NAMES:
            continue
        collected.append((rel.as_posix(), path.read_bytes()))
    collected.sort(key=lambda item: item[0])
    return collected


def write_deterministic_zip(zip_path: Path, entries) -> None:
    """Write a byte-reproducible zip: sorted names, STORED, fixed metadata."""
    zip_path = Path(zip_path)
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_STORED) as zf:
        for arcname, data in entries:
            info = zipfile.ZipInfo(filename=arcname, date_time=FIXED_DATE_TIME)
            info.compress_type = zipfile.ZIP_STORED
            info.create_system = 3          # Unix; stable across platforms
            info.create_version = 20
            info.extract_version = 20
            info.external_attr = 0o100644 << 16
            info.internal_attr = 0
            info.flag_bits = 0
            info.volume = 0
            zf.writestr(info, data)


def build_package(source, out, version, previous=None,
                  engine_compat="unspecified", backend="unspecified",
                  created_utc=None):
    """Build package.zip + manifest.json into `out`; return the manifest dict."""
    if not isinstance(version, int) or isinstance(version, bool) or version < 0:
        raise ManifestError("version must be a non-negative integer")
    source, out = Path(source), Path(out)
    prev = None
    if previous is not None:
        prev = load_manifest(Path(previous))
        if version <= prev["version"]:
            raise ManifestError(
                "version must strictly increase: new=%d previous=%d"
                % (version, prev["version"])
            )

    files = collect_files(source, exclude_dir=out)
    out.mkdir(parents=True, exist_ok=True)
    zip_path = out / ZIP_NAME
    write_deterministic_zip(zip_path, files)

    entries = [
        {"path": arc, "sha256": sha256_bytes(data), "size": len(data)}
        for arc, data in files
    ]
    new_map = {e["path"]: e for e in entries}

    if prev is None:
        added, modified, removed = sorted(new_map), [], []
    else:
        old_map = {e["path"]: e for e in prev["files"]}
        added = sorted(set(new_map) - set(old_map))
        removed = sorted(set(old_map) - set(new_map))
        modified = sorted(
            p for p in (set(new_map) & set(old_map))
            if new_map[p]["sha256"] != old_map[p]["sha256"]
            or new_map[p]["size"] != old_map[p]["size"]
        )

    manifest = {
        "schema": SCHEMA,
        "version": version,
        "created_utc": created_utc or _utc_now(),
        "engine_compat": engine_compat,
        "backend": backend,
        "package_zip": ZIP_NAME,
        "package_sha256": sha256_file(zip_path),
        "files": entries,
        "changes": {"added": added, "modified": modified, "removed": removed},
    }
    validate_manifest(manifest)
    (out / MANIFEST_NAME).write_bytes(canonical_bytes(manifest))
    return manifest


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Build a deterministic source-script package (no bytecode)."
    )
    ap.add_argument("--source", required=True, help="directory of source scripts")
    ap.add_argument("--out", required=True, help="output package directory")
    ap.add_argument("--version", required=True, type=int, help="monotonic integer version")
    ap.add_argument("--previous", help="previous manifest.json for delta info")
    ap.add_argument("--engine-compat", default="unspecified",
                    help="metadata string; does not imply UE validation")
    ap.add_argument("--backend", default="unspecified",
                    help="lua55 | luau | both | unspecified (metadata only)")
    ap.add_argument("--created-utc", default=None,
                    help="override UTC timestamp for byte-reproducible builds")
    args = ap.parse_args(argv)
    try:
        manifest = build_package(
            source=Path(args.source), out=Path(args.out), version=args.version,
            previous=Path(args.previous) if args.previous else None,
            engine_compat=args.engine_compat, backend=args.backend,
            created_utc=args.created_utc,
        )
    except (ManifestError, OSError) as exc:
        print("BUILD FAILED: %s" % exc, file=sys.stderr)
        return 1
    changes = manifest["changes"]
    print("BUILD OK: version=%d files=%d zip=%s"
          % (manifest["version"], len(manifest["files"]), ZIP_NAME))
    print("  added=%d modified=%d removed=%d"
          % (len(changes["added"]), len(changes["modified"]), len(changes["removed"])))
    print("NOTE: source scripts only; manifest metadata does not imply UE-runtime validation.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

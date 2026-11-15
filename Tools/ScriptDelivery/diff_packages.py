#!/usr/bin/env python3
"""Compute an incremental content list between two package manifests.

Usage:
    python -B diff_packages.py --old <manifest.json> --new <manifest.json> \
        [--out <delta.json>]

The output is a JSON delta list (schema/ added/ modified/ removed), not a binary
patch. The receiving side is expected to fetch the corresponding files from the
new package zip and delete the removed ones (see Docs/CN/ScriptDelivery.md).

Refusals enforced here:
  - version rollback (new version must be greater than old version);
  - accidental deletion: every removed path must exist in old, be absent from
    new, be a safe relative path, and never overlap added/modified.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from verify_package import (
    DELTA_SCHEMA,
    ManifestError,
    is_safe_relpath,
    load_manifest,
    validate_manifest,
)


def compute_delta(old_manifest, new_manifest):
    """Return the delta dict; raise ManifestError on rollback or unsafe removal."""
    validate_manifest(old_manifest)
    validate_manifest(new_manifest)
    old_version = old_manifest["version"]
    new_version = new_manifest["version"]
    if new_version <= old_version:
        raise ManifestError(
            "delta requires new version > old version (old=%d new=%d)"
            % (old_version, new_version)
        )

    old_map = {e["path"]: e for e in old_manifest["files"]}
    new_map = {e["path"]: e for e in new_manifest["files"]}
    added = sorted(set(new_map) - set(old_map))
    removed = sorted(set(old_map) - set(new_map))
    modified = sorted(
        p for p in (set(old_map) & set(new_map))
        if old_map[p]["sha256"] != new_map[p]["sha256"]
        or old_map[p]["size"] != new_map[p]["size"]
    )

    sets = [set(added), set(modified), set(removed)]
    for i in range(len(sets)):
        for j in range(i + 1, len(sets)):
            if sets[i] & sets[j]:
                raise ManifestError("delta sets are not disjoint")
    for p in added + modified:
        if not is_safe_relpath(p):
            raise ManifestError("unsafe delta path %r" % p)

    # "No accidental deletion": a path may be deleted only when it really was in
    # the old version and really is gone from the new one, and is a safe path.
    for p in removed:
        if p not in old_map:
            raise ManifestError("removed path %r not present in old manifest" % p)
        if p in new_map:
            raise ManifestError(
                "refusing to delete %r: still present in new version" % p
            )
        if not is_safe_relpath(p):
            raise ManifestError("refusing to delete unsafe path %r" % p)

    return {
        "schema": DELTA_SCHEMA,
        "from_version": old_version,
        "to_version": new_version,
        "added": added,
        "modified": modified,
        "removed": removed,
        "checks": {
            "sets_disjoint": True,
            "removed_confirmed_absent_in_new": True,
            "removed_count": len(removed),
        },
    }


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Compute an incremental content list from two manifests."
    )
    ap.add_argument("--old", required=True, help="old manifest.json")
    ap.add_argument("--new", required=True, help="new manifest.json")
    ap.add_argument("--out", help="write the delta JSON to this file")
    args = ap.parse_args(argv)
    try:
        delta = compute_delta(
            load_manifest(Path(args.old)), load_manifest(Path(args.new))
        )
        text = json.dumps(delta, sort_keys=True, indent=2, ensure_ascii=False) + "\n"
        if args.out:
            Path(args.out).write_text(text, encoding="utf-8")
    except (ManifestError, OSError) as exc:
        print("DIFF FAILED: %s" % exc, file=sys.stderr)
        return 1
    print("DIFF OK: from=%d to=%d added=%d modified=%d removed=%d"
          % (delta["from_version"], delta["to_version"], len(delta["added"]),
             len(delta["modified"]), len(delta["removed"])))
    if not args.out:
        print(text, end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())

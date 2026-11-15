#!/usr/bin/env python3
"""Verify a signed script package; hold the shared package-format helpers.

This module is the single format authority for the Tools/ScriptDelivery CLIs.
build_package.py, sign_package.py, diff_packages.py and rollback.py import the
canonical-encoding, manifest-validation and integrity helpers from here, so all
tools agree on exactly one definition of "a valid package".

It can be executed directly:  python -B verify_package.py --package <dir> --key <hex>

SECURITY NOTICE (demo only):
    The signature scheme here is SYMMETRIC HMAC-SHA256. It exists to demonstrate
    the package/signature wire format and the verification flow. It is NOT a
    production authenticity mechanism: the same secret signs and verifies, so
    any party able to verify a package is also able to forge one. Production
    MUST use asymmetric signatures (Ed25519 / ECDSA) with key rotation, and the
    signing key must NEVER be shipped inside or alongside the package.

    This is a SOURCE-script delivery reference, not bytecode delivery. The Luau
    backend does not accept bytecode input (see Docs/CN/Luau.md).
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import os
import sys
import zipfile
from pathlib import Path

# --- Wire format constants -------------------------------------------------

SCHEMA = "unluanova.script-package/1"
SIGNATURE_SCHEMA = "unluanova.script-signature/1"
DELTA_SCHEMA = "unluanova.script-delta/1"
CURRENT_SCHEMA = "unluanova.current-pointer/1"
MANIFEST_NAME = "manifest.json"
ZIP_NAME = "package.zip"
SIGNATURE_NAME = "signature.json"
CURRENT_NAME = ".current"
VERSIONS_DIR = "versions"
HMAC_ALGORITHM = "HMAC-SHA256"

# Metadata files that must never be collected into a script payload.
RESERVED_NAMES = frozenset({MANIFEST_NAME, ZIP_NAME, SIGNATURE_NAME})

REQUIRED_MANIFEST_FIELDS = (
    "schema", "version", "created_utc", "engine_compat", "backend",
    "package_zip", "package_sha256", "files", "changes",
)
REQUIRED_FILE_FIELDS = ("path", "sha256", "size")
CHANGE_KEYS = ("added", "modified", "removed")


class ManifestError(ValueError):
    """Raised for a malformed manifest, a failed integrity check or a bad key."""


# --- Canonical encoding and hashing ----------------------------------------

def canonical_bytes(obj) -> bytes:
    """Deterministic UTF-8 JSON encoding used for on-disk files and signing.

    sort_keys + compact separators + ensure_ascii=False means equal objects
    always produce identical bytes on Windows and Linux. The signature covers
    exactly these canonical bytes of the manifest.
    """
    return json.dumps(
        obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with Path(path).open("rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


# --- Path and manifest validation ------------------------------------------

def is_safe_relpath(p) -> bool:
    """Reject absolute paths, drive letters, backslashes and traversal."""
    if not isinstance(p, str) or not p:
        return False
    if "\\" in p or ":" in p or "\x00" in p or p.startswith("/"):
        return False
    parts = p.split("/")
    return all(part not in ("", ".", "..") for part in parts)


def validate_manifest(manifest) -> None:
    """Validate schema, version, file entries and change lists.

    Single choke point used by every reader: a truncated or malformed manifest
    is refused everywhere. Raises ManifestError on the first problem.
    """
    if not isinstance(manifest, dict):
        raise ManifestError("manifest must be a JSON object")
    missing = [k for k in REQUIRED_MANIFEST_FIELDS if k not in manifest]
    if missing:
        raise ManifestError("manifest missing required fields: " + ", ".join(missing))
    if manifest["schema"] != SCHEMA:
        raise ManifestError("unsupported schema: %r" % (manifest["schema"],))
    version = manifest["version"]
    if not isinstance(version, int) or isinstance(version, bool) or version < 0:
        raise ManifestError("version must be a non-negative integer")
    for key in ("created_utc", "engine_compat", "backend",
                "package_zip", "package_sha256"):
        value = manifest[key]
        if not isinstance(value, str) or not value:
            raise ManifestError("manifest field %r must be a non-empty string" % key)
    if not is_safe_relpath(manifest["package_zip"]):
        raise ManifestError("package_zip must be a safe relative file name")
    if len(manifest["package_sha256"]) != 64:
        raise ManifestError("package_sha256 must be a 64-char hex digest")
    files = manifest["files"]
    if not isinstance(files, list):
        raise ManifestError("files must be a list")
    seen = set()
    for i, entry in enumerate(files):
        if not isinstance(entry, dict):
            raise ManifestError("files[%d] must be an object" % i)
        for field in REQUIRED_FILE_FIELDS:
            if field not in entry:
                raise ManifestError("files[%d] missing %r" % (i, field))
        if not is_safe_relpath(entry["path"]):
            raise ManifestError("files[%d] has unsafe path %r" % (i, entry["path"]))
        digest = entry["sha256"]
        if not isinstance(digest, str) or len(digest) != 64:
            raise ManifestError("files[%d] sha256 must be a 64-char hex digest" % i)
        size = entry["size"]
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise ManifestError("files[%d] size must be a non-negative integer" % i)
        if entry["path"] in seen:
            raise ManifestError("duplicate file path %r" % entry["path"])
        seen.add(entry["path"])
    changes = manifest["changes"]
    if not isinstance(changes, dict):
        raise ManifestError("changes must be an object")
    for key in CHANGE_KEYS:
        values = changes.get(key)
        if not isinstance(values, list) or any(not isinstance(v, str) for v in values):
            raise ManifestError("changes.%s must be a list of strings" % key)


def load_manifest(path: Path):
    """Read, parse and validate a manifest from disk."""
    try:
        raw = Path(path).read_bytes()
    except OSError as exc:
        raise ManifestError("cannot read manifest %s: %s" % (path, exc)) from exc
    try:
        manifest = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ManifestError("manifest is not valid UTF-8 JSON: %s" % exc) from exc
    validate_manifest(manifest)
    return manifest


# --- Integrity (content, NOT authenticity) ---------------------------------

def read_zip_entries(zip_path: Path):
    """Return {name: bytes} for every regular entry of a zip."""
    with zipfile.ZipFile(zip_path, "r") as zf:
        names = zf.namelist()
        if len(names) != len(set(names)):
            raise ManifestError("zip contains duplicate entry names")
        result = {}
        for info in zf.infolist():
            if info.is_dir():
                continue
            if not is_safe_relpath(info.filename):
                raise ManifestError("zip entry has unsafe path %r" % info.filename)
            result[info.filename] = zf.read(info)
        return result


def verify_integrity(package_dir: Path, manifest) -> None:
    """Check the zip hash and every file entry against the manifest.

    Raises ManifestError on any mismatch. This proves the bits match the
    manifest; it does NOT prove who produced the manifest (use the signature).
    """
    validate_manifest(manifest)
    zip_path = Path(package_dir) / manifest["package_zip"]
    if not zip_path.is_file():
        raise ManifestError("package zip not found: %s" % zip_path)
    if sha256_file(zip_path) != manifest["package_sha256"]:
        raise ManifestError("package zip sha256 does not match manifest")
    entries = read_zip_entries(zip_path)
    declared = {e["path"]: e for e in manifest["files"]}
    if set(entries) != set(declared):
        extra = sorted(set(entries) - set(declared))
        missing = sorted(set(declared) - set(entries))
        raise ManifestError(
            "zip/manifest file set mismatch (extra=%r missing=%r)" % (extra, missing)
        )
    for path, entry in declared.items():
        data = entries[path]
        if len(data) != entry["size"]:
            raise ManifestError("size mismatch for %s" % path)
        if sha256_bytes(data) != entry["sha256"]:
            raise ManifestError("sha256 mismatch for %s" % path)


# --- Signature (demo HMAC) -------------------------------------------------

def load_signature(path: Path):
    try:
        raw = Path(path).read_bytes()
    except OSError as exc:
        raise ManifestError("cannot read signature %s: %s" % (path, exc)) from exc
    try:
        sig = json.loads(raw.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise ManifestError("signature is not valid UTF-8 JSON: %s" % exc) from exc
    if not isinstance(sig, dict):
        raise ManifestError("signature must be a JSON object")
    if sig.get("schema") != SIGNATURE_SCHEMA:
        raise ManifestError("unsupported signature schema: %r" % (sig.get("schema"),))
    if sig.get("algorithm") != HMAC_ALGORITHM:
        raise ManifestError("unsupported signature algorithm: %r" % (sig.get("algorithm"),))
    for field in ("manifest_sha256", "signature"):
        value = sig.get(field)
        if not isinstance(value, str) or len(value) != 64:
            raise ManifestError("signature field %r must be a 64-char hex digest" % field)
    return sig


def verify_signature(package_dir: Path, manifest, key: bytes) -> None:
    """DEMO ONLY: verify the symmetric HMAC over canonical manifest bytes.

    See the module docstring. Production requires asymmetric signatures.
    """
    sig = load_signature(Path(package_dir) / SIGNATURE_NAME)
    canon = canonical_bytes(manifest)
    if not hmac.compare_digest(sig["manifest_sha256"], sha256_bytes(canon)):
        raise ManifestError("signature manifest_sha256 does not match manifest bytes")
    expected = hmac.new(key, canon, hashlib.sha256).hexdigest()
    if not hmac.compare_digest(sig["signature"], expected):
        raise ManifestError(
            "signature verification failed (wrong key or tampered manifest)"
        )


def verify_package(package_dir: Path, key: bytes):
    """Full check: manifest shape -> signature -> zip integrity.

    Returns the manifest on success, raises ManifestError on the first failure.
    """
    package_dir = Path(package_dir)
    manifest = load_manifest(package_dir / MANIFEST_NAME)
    verify_signature(package_dir, manifest, key)
    verify_integrity(package_dir, manifest)
    return manifest


# --- Shared CLI key handling -----------------------------------------------

def resolve_key(key_hex=None, key_file=None) -> bytes:
    """Resolve a hex-encoded HMAC key from --key or --key-file.

    Both forms are interpreted as hex TEXT so Windows/Linux line endings never
    change the key bytes. A key must be provided for signing/verifying.
    """
    if key_hex and key_file:
        raise ManifestError("provide only one of --key / --key-file")
    raw = key_hex
    if key_file:
        try:
            raw = Path(key_file).read_text(encoding="utf-8").strip()
        except OSError as exc:
            raise ManifestError("cannot read key file %s: %s" % (key_file, exc)) from exc
    if not raw:
        raise ManifestError("a signing key is required (--key or --key-file)")
    try:
        key = bytes.fromhex(raw)
    except ValueError as exc:
        raise ManifestError("key must be a hex string: %s" % exc) from exc
    if not key:
        raise ManifestError("key must not be empty")
    return key


def optional_key(args):
    """Return a key only when --key / --key-file was supplied, else None."""
    if getattr(args, "key", None) or getattr(args, "key_file", None):
        return resolve_key(args.key, args.key_file)
    return None


# --- CLI -------------------------------------------------------------------

def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Verify a signed script package (demo HMAC-SHA256)."
    )
    ap.add_argument("--package", required=True, help="package directory")
    ap.add_argument("--key", help="HMAC key as a hex string (DEMO ONLY)")
    ap.add_argument("--key-file", help="file containing the hex HMAC key")
    args = ap.parse_args(argv)
    try:
        key = resolve_key(args.key, args.key_file)
        verify_package(Path(args.package), key)
    except (ManifestError, OSError) as exc:
        print("VERIFY FAILED: %s" % exc, file=sys.stderr)
        return 1
    print("VERIFY OK: manifest, signature and package integrity are valid.")
    print("NOTE: HMAC is a demo-only symmetric scheme; production needs Ed25519/ECDSA.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

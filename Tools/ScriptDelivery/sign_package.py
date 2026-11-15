#!/usr/bin/env python3
"""Generate signature.json for a built script package.

Usage:
    python -B sign_package.py --package <dir> --key <hex>
    python -B sign_package.py --package <dir> --key-file <path>

SECURITY NOTICE (demo only):
    This uses SYMMETRIC HMAC-SHA256 over the canonical manifest bytes purely to
    demonstrate the package/signature wire format. It is NOT production-grade:
    the same secret signs and verifies, so any verifier can also forge packages.
    Production MUST use asymmetric signatures (Ed25519 / ECDSA) with key
    rotation, and the signing key must NEVER be shipped inside or alongside the
    package. Never commit a real key to this repository.
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import sys
from pathlib import Path

from verify_package import (
    HMAC_ALGORITHM,
    MANIFEST_NAME,
    SIGNATURE_NAME,
    SIGNATURE_SCHEMA,
    ManifestError,
    canonical_bytes,
    load_manifest,
    resolve_key,
    sha256_bytes,
)


def sign_package(package_dir: Path, key: bytes):
    """DEMO ONLY: write signature.json over the canonical manifest bytes.

    The signature covers manifest.json exactly as canonical_bytes() encodes it,
    so reformatting the JSON on disk does not change what is signed as long as
    the object is identical. See the module docstring for the security boundary.
    """
    package_dir = Path(package_dir)
    manifest = load_manifest(package_dir / MANIFEST_NAME)
    canon = canonical_bytes(manifest)
    sig = {
        "schema": SIGNATURE_SCHEMA,
        "algorithm": HMAC_ALGORITHM,
        "manifest_sha256": sha256_bytes(canon),
        "signature": hmac.new(key, canon, hashlib.sha256).hexdigest(),
    }
    (package_dir / SIGNATURE_NAME).write_bytes(canonical_bytes(sig))
    return sig


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="Sign a script package (demo HMAC-SHA256, NOT for production)."
    )
    ap.add_argument("--package", required=True, help="package directory")
    ap.add_argument("--key", help="HMAC key as a hex string (DEMO ONLY)")
    ap.add_argument("--key-file", help="file containing the hex HMAC key")
    args = ap.parse_args(argv)
    try:
        key = resolve_key(args.key, args.key_file)
        sig = sign_package(Path(args.package), key)
    except (ManifestError, OSError) as exc:
        print("SIGN FAILED: %s" % exc, file=sys.stderr)
        return 1
    print("SIGN OK: wrote %s (algorithm=%s)" % (SIGNATURE_NAME, sig["algorithm"]))
    print("WARNING: HMAC is symmetric and demo-only; production needs Ed25519/ECDSA "
          "with key rotation, and the key must not ship with the package.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

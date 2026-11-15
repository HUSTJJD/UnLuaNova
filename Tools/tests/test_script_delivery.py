"""Unit tests for the Tools/ScriptDelivery reference toolchain.

All tests run against tempfile directories and leave no residue. Only the
Python standard library is used. CLI output and comments are English.
"""

import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SCRIPT_DIR = ROOT / "Tools" / "ScriptDelivery"
sys.path.insert(0, str(SCRIPT_DIR))

import build_package  # noqa: E402
import diff_packages  # noqa: E402
import rollback  # noqa: E402
import sign_package  # noqa: E402
import verify_package  # noqa: E402
from verify_package import (  # noqa: E402
    ManifestError,
    canonical_bytes,
    load_manifest,
    read_zip_entries,
    validate_manifest,
    verify_integrity,
    verify_package as verify_pkg,
)

FIXED_UTC = "2026-11-15T00:00:00Z"
# Demo-only symmetric keys, never shipped with any package.
KEY = bytes.fromhex(
    "9f2c4a7b1e6d8035a4c9e2f7b0d1a3c5e8d6f4b2a0c8e6d4f2b0a9e7c5d3f1"
)
WRONG_KEY = bytes.fromhex(
    "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f"
)


class ScriptDeliveryTests(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.root = Path(self._tmp.name)
        self.source = self.root / "Content" / "Script"
        self.source.mkdir(parents=True)
        self._write("Main.lua", "return 1\n")
        self._write("A/B.lua", "return 2\n")
        self._write("Old/Deprecated.lua", "return 3\n")

    def tearDown(self):
        self._tmp.cleanup()

    # --- helpers ----------------------------------------------------------

    def _write(self, rel, text):
        path = self.source / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")
        return path

    def _build(self, version, previous=None, out=None, created_utc=FIXED_UTC):
        out = out or (self.root / ("pkg_v%d" % version))
        manifest = build_package.build_package(
            self.source, out, version, previous=previous,
            engine_compat="UE 6.0 / Win64", backend="both",
            created_utc=created_utc,
        )
        return manifest, out

    def _tamper_zip(self, out, relpath, new_text):
        zip_path = out / "package.zip"
        entries = read_zip_entries(zip_path)
        self.assertIn(relpath, entries)
        entries[relpath] = new_text.encode("utf-8")
        with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_STORED) as zf:
            for name in sorted(entries):
                info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
                info.compress_type = zipfile.ZIP_STORED
                zf.writestr(info, entries[name])

    def _run_cli(self, script, *args):
        cmd = [sys.executable, "-B", str(SCRIPT_DIR / script)]
        cmd += [str(a) for a in args]
        return subprocess.run(
            cmd, cwd=str(SCRIPT_DIR), capture_output=True, text=True
        )

    # --- 1. deterministic packaging ---------------------------------------

    def test_deterministic_build_same_bytes(self):
        _, out1 = self._build(1, out=self.root / "det_a")
        _, out2 = self._build(1, out=self.root / "det_b")
        self.assertEqual((out1 / "package.zip").read_bytes(),
                         (out2 / "package.zip").read_bytes())
        self.assertEqual((out1 / "manifest.json").read_bytes(),
                         (out2 / "manifest.json").read_bytes())
        # The zip must not depend on created_utc (fixed timestamp / STORED).
        _, out3 = self._build(1, out=self.root / "det_c",
                              created_utc="2001-01-01T00:00:00Z")
        self.assertEqual((out1 / "package.zip").read_bytes(),
                         (out3 / "package.zip").read_bytes())

    # --- 2. zip file hashes agree with manifest -----------------------------

    def test_zip_entry_hashes_match_manifest(self):
        manifest, out = self._build(1)
        entries = read_zip_entries(out / "package.zip")
        declared = {e["path"]: e for e in manifest["files"]}
        self.assertEqual(set(entries), set(declared))
        for path, entry in declared.items():
            data = entries[path]
            self.assertEqual(entry["sha256"], hashlib.sha256(data).hexdigest())
            self.assertEqual(entry["size"], len(data))
        self.assertEqual(
            manifest["package_sha256"],
            hashlib.sha256((out / "package.zip").read_bytes()).hexdigest(),
        )
        verify_integrity(out, manifest)  # must not raise

    # --- 3. tampering with any file fails verify ----------------------------

    def test_tampered_manifest_fails_verify(self):
        manifest, out = self._build(1)
        sign_package.sign_package(out, KEY)
        manifest["files"][0]["sha256"] = "0" * 64
        (out / "manifest.json").write_bytes(canonical_bytes(manifest))
        with self.assertRaises(ManifestError):
            verify_pkg(out, KEY)

    def test_tampered_zip_content_fails_verify(self):
        _, out = self._build(1)
        sign_package.sign_package(out, KEY)
        self._tamper_zip(out, "Main.lua", "return 999\n")
        with self.assertRaises(ManifestError):
            verify_pkg(out, KEY)

    def test_signature_tamper_fails_verify(self):
        _, out = self._build(1)
        sig = sign_package.sign_package(out, KEY)
        bad = dict(sig, signature="f" * 64)
        (out / "signature.json").write_bytes(canonical_bytes(bad))
        with self.assertRaises(ManifestError):
            verify_pkg(out, KEY)

    # --- 4. wrong key fails signature --------------------------------------

    def test_wrong_key_fails_signature(self):
        _, out = self._build(1)
        sign_package.sign_package(out, KEY)
        with self.assertRaises(ManifestError):
            verify_pkg(out, WRONG_KEY)

    # --- 5. manifest version rollback is refused ---------------------------

    def test_manifest_version_rollback_refused(self):
        m5, out5 = self._build(5)
        with self.assertRaises(ManifestError):
            build_package.build_package(
                self.source, self.root / "bad", 3,
                previous=out5 / "manifest.json",
            )
        with self.assertRaises(ManifestError):
            diff_packages.compute_delta(m5, dict(m5, version=3))
        with self.assertRaises(ManifestError):
            validate_manifest(dict(m5, version=-1))

    # --- 6. incremental delta is correct -----------------------------------

    def test_delta_content_lists_are_correct(self):
        m1, out1 = self._build(1)
        (self.source / "Old" / "Deprecated.lua").unlink()
        self._write("A/B.lua", "return 22\n")
        self._write("New/Feature.lua", "return 4\n")
        m2, out2 = self._build(2, previous=out1 / "manifest.json")
        self.assertEqual(m2["changes"], {
            "added": ["New/Feature.lua"],
            "modified": ["A/B.lua"],
            "removed": ["Old/Deprecated.lua"],
        })
        delta = diff_packages.compute_delta(m1, m2)
        self.assertEqual(delta["added"], ["New/Feature.lua"])
        self.assertEqual(delta["modified"], ["A/B.lua"])
        self.assertEqual(delta["removed"], ["Old/Deprecated.lua"])
        self.assertTrue(delta["checks"]["sets_disjoint"])
        self.assertTrue(delta["checks"]["removed_confirmed_absent_in_new"])
        self.assertEqual(delta["checks"]["removed_count"], 1)
        # out2 is unused except to prove the build succeeded.
        self.assertTrue((out2 / "manifest.json").is_file())

    # --- 7. rollback to a historical version updates .current --------------

    def test_rollback_to_history_updates_current(self):
        _, out1 = self._build(1)
        sign_package.sign_package(out1, KEY)
        self._write("A/Extra.lua", "return 9\n")
        _, out2 = self._build(2, previous=out1 / "manifest.json")
        sign_package.sign_package(out2, KEY)
        repo = self.root / "repo"
        rollback.add_version(repo, out1, KEY)
        rollback.add_version(repo, out2, KEY)
        self.assertEqual(rollback.list_versions(repo), [1, 2])
        rollback.switch_version(repo, 2, KEY)
        self.assertEqual(rollback.read_current(repo), 2)
        rollback.switch_version(repo, 1, KEY)  # one-click rollback
        self.assertEqual(rollback.read_current(repo), 1)
        pointer = json.loads((repo / ".current").read_text(encoding="utf-8"))
        self.assertEqual(pointer["schema"], "unluanova.current-pointer/1")
        self.assertEqual(pointer["version"], 1)

    def test_switch_to_missing_version_refused(self):
        _, out1 = self._build(1)
        sign_package.sign_package(out1, KEY)
        repo = self.root / "repo"
        rollback.add_version(repo, out1, KEY)
        rollback.switch_version(repo, 1, KEY)
        # Version 0 is both lower than current and absent from history.
        with self.assertRaises(ManifestError):
            rollback.switch_version(repo, 0, KEY)
        with self.assertRaises(ManifestError):
            rollback.switch_version(repo, 99, KEY)

    # --- 8. malformed manifests are refused --------------------------------

    def test_malformed_manifest_missing_fields_refused(self):
        manifest, _ = self._build(1)
        bad = dict(manifest)
        del bad["backend"]
        path = self.root / "bad_manifest.json"
        path.write_bytes(canonical_bytes(bad))
        with self.assertRaises(ManifestError):
            load_manifest(path)
        mutants = [
            dict(manifest, schema="wrong/1"),
            dict(manifest, files=[{"path": "Main.lua"}]),
            dict(manifest, files="not-a-list"),
            dict(manifest, changes={"added": "not-a-list"}),
            dict(manifest, files=[{"path": "../escape.lua", "sha256": "0" * 64,
                                  "size": 1}]),
            dict(manifest, version="1"),
        ]
        for mutant in mutants:
            with self.assertRaises(ManifestError):
                validate_manifest(mutant)

    # --- 9. full CLI round-trip (python -B) --------------------------------

    def test_cli_roundtrip(self):
        src = self.source
        out1 = self.root / "cli_v1"
        out2 = self.root / "cli_v2"
        repo = self.root / "cli_repo"
        delta_file = self.root / "delta.json"
        keyhex = KEY.hex()
        common = ["--engine-compat", "UE 6.0 / Win64", "--backend", "both",
                  "--created-utc", FIXED_UTC]

        r = self._run_cli("build_package.py", "--source", src, "--out", out1,
                          "--version", 1, *common)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self._write("A/C.lua", "return 7\n")
        r = self._run_cli("build_package.py", "--source", src, "--out", out2,
                          "--version", 2, "--previous", out1 / "manifest.json",
                          *common)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

        for pkg in (out1, out2):
            r = self._run_cli("sign_package.py", "--package", pkg,
                              "--key", keyhex)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            r = self._run_cli("verify_package.py", "--package", pkg,
                              "--key", keyhex)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
            self.assertIn("VERIFY OK", r.stdout)

        r = self._run_cli("diff_packages.py", "--old", out1 / "manifest.json",
                          "--new", out2 / "manifest.json", "--out", delta_file)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        delta = json.loads(delta_file.read_text(encoding="utf-8"))
        self.assertIn("A/C.lua", delta["added"])

        for pkg in (out1, out2):
            r = self._run_cli("rollback.py", "add", "--repo", repo,
                              "--package", pkg, "--key", keyhex)
            self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        r = self._run_cli("rollback.py", "switch", "--repo", repo,
                          "--version", 2, "--key", keyhex)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        r = self._run_cli("rollback.py", "switch", "--repo", repo,
                          "--version", 1, "--key", keyhex)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        r = self._run_cli("rollback.py", "current", "--repo", repo)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertIn("CURRENT: 1", r.stdout)
        pointer = json.loads((repo / ".current").read_text(encoding="utf-8"))
        self.assertEqual(pointer["version"], 1)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""Reject machine-local absolute paths and engine GUIDs in tracked source files.

The repository has to be cloneable and buildable on anyone's machine. Paths under
Saved/ are machine-specific by design but are ignored by Git, so only tracked
files are scanned. Markdown is excluded on purpose: troubleshooting pages quote
Windows paths as examples, and those are prose, not build inputs.

Usage:
    python -B Tools/CheckNoLocalPaths.py
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

SCANNED_SUFFIXES = {
    ".py", ".ps1", ".cs", ".cpp", ".h", ".hpp", ".inl", ".c", ".cc",
    ".ini", ".yml", ".yaml", ".json", ".lua", ".sh", ".bat",
}

SKIP_PREFIXES = (
    "Plugins/UnLuaNova/Source/ThirdParty/",
    "Tools/lua-language-server/",
    "Tools/luau-lsp/",
    "Saved/",
    "site/",
)

PATTERNS = (
    re.compile(r"[A-Za-z]:[\\/]{1,2}Users[\\/]+[A-Za-z0-9._-]+", re.IGNORECASE),
    re.compile(r"/(?:Users|home)/[A-Za-z0-9._-]+/", re.IGNORECASE),
    re.compile(r"[A-Za-z]:[\\/]{1,2}learning[\\/]+UEAbilityKit", re.IGNORECASE),
    re.compile(r"\{[0-9A-F]{8}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{4}-[0-9A-F]{12}\}",
               re.IGNORECASE),
)


def tracked_files():
    result = subprocess.run(
        ["git", "-C", str(ROOT), "-c", "core.quotepath=false", "ls-files"],
        capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=60)
    if result.returncode != 0:
        raise SystemExit("ERROR: git ls-files failed: %s" % result.stderr.strip())
    return result.stdout.splitlines()


def find_violations(names):
    violations = []
    for name in names:
        if Path(name).suffix.lower() not in SCANNED_SUFFIXES:
            continue
        if any(name.startswith(prefix) for prefix in SKIP_PREFIXES):
            continue
        try:
            text = (ROOT / name).read_text(encoding="utf-8-sig", errors="replace")
        except OSError:
            continue
        for number, line in enumerate(text.splitlines(), 1):
            if any(pattern.search(line) for pattern in PATTERNS):
                violations.append("%s:%d: %s" % (name, number, line.strip()[:140]))
    return violations


def main():
    names = tracked_files()
    violations = find_violations(names)
    if violations:
        print("Machine-local path or engine GUID found in tracked source files:")
        for item in violations:
            print("  " + item)
        return 1
    print("Hygiene check passed: %d tracked files, no machine-local paths or engine GUIDs."
          % len(names))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

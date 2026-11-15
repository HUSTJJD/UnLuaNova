#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""CI gate: every ``*.Build.cs`` must carry a byte-identical ``[UAK BACKEND LAYOUT]`` block.

Each ``*.Build.cs`` in this repository contains a verbatim copy of the same
``[UAK BACKEND LAYOUT ...]`` block (delimited by two ``//`` comment markers). The block
documents itself as "keep this block identical in every Build.cs", but until now that was
only a human convention: a copy that drifts would silently break backend isolation.

This script turns that convention into a machine-enforced CI gate. It extracts each block,
normalises line endings and compares the copies verbatim (including the marker comment
lines). A missing or drifted block fails the build; a block count that differs from the
expected number only emits a WARNING (the count may legitimately grow as modules are added).

Why the block is NOT physically extracted into a shared file
-----------------------------------------------------------
UBT (UnrealBuildTool) compiles each module's ``Build.cs`` into a ``ModuleRules`` class.
There is no reliable cross-file "include a shared fragment" mechanism for ``Build.cs``:
plugin/module rules may be compiled in separate assemblies, so a shared helper class or a
text-splice/``#include`` style include is not guaranteed to resolve. Physical extraction
(e.g. into a shared ``.cs``/``.targets`` file or a code generator) therefore changes how UBT
evaluates the rules and can only be validated by a FULL Unreal Engine compile of every
carrier (Editor / Game / Server) for BOTH backends (Lua and Luau). This task is forbidden
from running an UE compile, so that verification is impossible here. Keeping N duplicated
copies and enforcing byte-for-byte equality in CI is the safe, verifiable alternative.

为何不物理抽离：UBT 的 Build.cs 不提供可靠的跨文件共享片段机制（插件规则可能在各自独立的
程序集中编译），共享 helper / 文本拼接无法保证被 UBT 正确解析；物理抽离会改变 UBT 对规则
的求值方式，必须靠完整 UE 编译（各 carrier × 两后端）才能验证，而本次环境/约束不允许跑 UE
编译、也无法验证，故保留逐字复制并改由 CI 强制校验。

未来若要抽离必须先跑完整 UE 编译验证
------------------------------------
If someone later wants to physically de-duplicate the block, they MUST first run a
complete UE build/compile verification (all carriers x both backends, plus the isolated
target matrix) and only then change the layout mechanism. Do NOT relax or skip this
consistency gate as a shortcut to make a physical extraction "pass" without that compile.
未来若要物理抽离这些块，必须先跑完整的 UE 编译验证（全部 carrier × 双后端），验证通过后才
能改动布局机制；不要靠放宽或绕过本门禁来"凑绿"。

Usage
-----
    python -B Tools/CheckBuildLayoutConsistency.py [ROOT]

``ROOT`` defaults to the repository root (the parent directory of ``Tools/``).
Exit code: 0 = pass (warnings allowed), 1 = fail (missing block / inconsistent block).
"""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from dataclasses import dataclass
from pathlib import Path

# This file lives at <repo>/Tools/CheckBuildLayoutConsistency.py, so the repo root is
# the parent of Tools/. Resolved from __file__, so it works from any current directory.
REPO_ROOT = Path(__file__).resolve().parents[1]

# The block is deliberately duplicated in every Build.cs and is delimited by two *comment*
# marker lines. The real content was read before choosing the regex: the markers are lines
# such as
#   "        // [UAK BACKEND LAYOUT - keep this block identical in every Build.cs]"
#   "        // [/UAK BACKEND LAYOUT]"
# i.e. ordinary "//" comments with leading indentation, NOT code. The regex therefore
# allows an optional "//" comment prefix and leading whitespace, and is anchored so a
# prose mention like "// Build.cs ([UAK BACKEND LAYOUT]): ..." cannot be mistaken for a
# marker. Both markers are part of the block and are compared verbatim.
BEGIN_MARKER = "[UAK BACKEND LAYOUT"
END_MARKER = "[/UAK BACKEND LAYOUT]"
_BEGIN_RE = re.compile(r"^\s*(?://\s*)?\[UAK BACKEND LAYOUT\b")
_END_RE = re.compile(r"^\s*(?://\s*)?\[/UAK BACKEND LAYOUT\]")

# Files under these directories are third-party / vendored / generated and are not part of
# the "every Build.cs" contract the block documents. (Excluding ThirdParty is required by
# the gate; the rest are safety nets so a vendored tool's stray Build.cs cannot skew it.)
EXCLUDE_PARTS = frozenset({
    "ThirdParty", "Binaries", "Intermediate", "Saved", "DerivedDataCache", "site",
    "lua-language-server", "luau-lsp", "vendored", "vendor", "node_modules",
    ".git", ".vs", ".vscode", ".pytest_cache", ".workbuddy", "__pycache__",
})

EXPECTED_BLOCK_COUNT = 6


def _is_excluded(path: Path, root: Path) -> bool:
    rel = path.relative_to(root)
    return any(part in EXCLUDE_PARTS for part in rel.parts)


def find_build_cs(root: Path):
    """Return (scanned, excluded) sorted lists of ``*.Build.cs`` paths under ``root``."""
    root = Path(root).resolve()
    all_files = sorted(root.rglob("*.Build.cs"))
    scanned, excluded = [], []
    for path in all_files:
        (excluded if _is_excluded(path, root) else scanned).append(path)
    return scanned, excluded


def _read_normalized(path: Path) -> str:
    """Read a Build.cs, strip a UTF-8 BOM and normalize all line endings to '\\n'."""
    text = path.read_bytes().decode("utf-8-sig")
    return text.replace("\r\n", "\n").replace("\r", "\n")


def extract_block(text: str):
    """Return ``(block, reason)``: the verbatim block (markers included) or ``(None, why)``.

    ``text`` is expected to have normalized (LF) line endings. The block runs from the
    begin marker line through the end marker line, inclusive.
    """
    lines = text.split("\n")
    begins = [i for i, line in enumerate(lines) if _BEGIN_RE.search(line)]
    ends = [i for i, line in enumerate(lines) if _END_RE.search(line)]
    if not begins and not ends:
        return None, "no [UAK BACKEND LAYOUT] markers found"
    if not begins:
        return None, "end marker found but begin marker is missing"
    begin = begins[0]
    ends_after = [e for e in ends if e > begin]
    if not ends_after:
        return None, "begin marker found but [/UAK BACKEND LAYOUT] is missing after it"
    if len(begins) > 1 or len(ends_after) > 1:
        return None, "duplicate begin/end markers (malformed block)"
    end = ends_after[0]
    return "\n".join(lines[begin:end + 1]), ""


def _first_diff(a_lines, b_lines):
    """First differing line between two line lists -> (1-based line_no, a_line, b_line)."""
    for i in range(min(len(a_lines), len(b_lines))):
        if a_lines[i] != b_lines[i]:
            return i + 1, a_lines[i], b_lines[i]
    if len(a_lines) != len(b_lines):
        i = min(len(a_lines), len(b_lines))
        la = a_lines[i] if i < len(a_lines) else "<no line>"
        lb = b_lines[i] if i < len(b_lines) else "<no line>"
        return i + 1, la, lb
    return None


@dataclass
class CheckResult:
    root: Path
    scanned: list
    excluded: list
    blocks: dict            # Path -> normalized block text
    missing: list           # [(Path, reason)]
    groups: dict            # block text -> [Path]
    canonical: object       # the majority block text, or None
    mismatches: list        # [{path, line_no, canonical_line, file_line}]
    errors: list
    warnings: list

    @property
    def ok(self) -> bool:
        return not self.errors


def check(root: Path) -> CheckResult:
    """Run the consistency check against ``root`` and return a :class:`CheckResult`."""
    root = Path(root).resolve()
    scanned, excluded = find_build_cs(root)

    blocks, missing = {}, []
    for path in scanned:
        try:
            text = _read_normalized(path)
        except (OSError, UnicodeDecodeError) as exc:
            missing.append((path, f"unreadable: {exc}"))
            continue
        block, reason = extract_block(text)
        if block is None:
            missing.append((path, reason))
        else:
            blocks[path] = block

    groups = {}
    for path, block in blocks.items():
        groups.setdefault(block, []).append(path)

    canonical = None
    if groups:
        # Majority block is the reference; ties are broken by the smallest relative path
        # so the choice is deterministic.
        canonical = min(
            groups.items(),
            key=lambda kv: (-len(kv[1]), min(str(p.relative_to(root)) for p in kv[1])),
        )[0]

    mismatches = []
    errors, warnings = [], []

    if canonical is not None and len(groups) > 1:
        canon_lines = canonical.split("\n")
        for block, paths in groups.items():
            if block == canonical:
                continue
            diff = _first_diff(canon_lines, block.split("\n"))
            for path in paths:
                mismatches.append({
                    "path": path,
                    "line_no": diff[0] if diff else None,
                    "canonical_line": diff[1] if diff else None,
                    "file_line": diff[2] if diff else None,
                })

    for path, reason in missing:
        errors.append(f"missing block: {path.relative_to(root)} ({reason})")

    if mismatches:
        errors.append(
            f"inconsistent [UAK BACKEND LAYOUT] block in {len(mismatches)} file(s)"
        )
        for m in mismatches:
            errors.append(
                f"  differing file {m['path'].relative_to(root)}: first differing line "
                f"{m['line_no']}: canonical={m['canonical_line']!r} "
                f"found={m['file_line']!r}"
            )

    block_count = len(blocks)
    if block_count != EXPECTED_BLOCK_COUNT:
        warnings.append(
            f"expected {EXPECTED_BLOCK_COUNT} [UAK BACKEND LAYOUT] blocks but found "
            f"{block_count} (count may change as modules are added; non-fatal)"
        )

    return CheckResult(
        root=root, scanned=scanned, excluded=excluded, blocks=blocks, missing=missing,
        groups=groups, canonical=canonical, mismatches=mismatches, errors=errors,
        warnings=warnings,
    )


def _sha(block: str) -> str:
    return hashlib.sha256(block.encode("utf-8")).hexdigest()[:10]


def format_report(result: CheckResult) -> str:
    """Build the human-readable summary printed by :func:`main`."""
    bar = "=" * 72
    out = [bar, "[UAK] *.Build.cs [UAK BACKEND LAYOUT] consistency check", bar]
    out.append(f"Root                 : {result.root}")
    out.append(f"Scanned *.Build.cs   : {len(result.scanned)}")
    out.append(f"Excluded (3rd-party / vendored / generated): {len(result.excluded)}")
    out.append(f"Blocks found         : {len(result.blocks)}")
    out.append(f"Distinct variants    : {len(result.groups)}")
    out.append(f"Expected block count : {EXPECTED_BLOCK_COUNT}")
    out.append("")
    out.append("Per-file status:")
    if not result.scanned:
        out.append("  (no *.Build.cs found)")
    mismatch_paths = {m["path"] for m in result.mismatches}
    missing_reasons = {path: reason for path, reason in result.missing}
    for path in result.scanned:
        rel = path.relative_to(result.root)
        if path in missing_reasons:
            out.append(f"  MISSING  {rel}  ({missing_reasons[path]})")
        elif path in mismatch_paths:
            out.append(f"  MISMATCH {rel}")
        else:
            out.append(f"  OK       {rel}  sha={_sha(result.blocks[path])}")

    if result.missing:
        out.append("")
        out.append("--- Missing / malformed block (ERROR) ---")
        for path, reason in result.missing:
            out.append(f"  {path.relative_to(result.root)}: {reason}")

    if result.mismatches:
        canon_file = min(
            result.groups[result.canonical], key=lambda p: str(p)
        ).relative_to(result.root)
        out.append("")
        out.append(f"--- Inconsistent block detail (canonical = {canon_file}) ---")
        for m in result.mismatches:
            out.append(f"  MISMATCH {m['path'].relative_to(result.root)}")
            if m["line_no"] is not None:
                out.append(f"    first differing line (within block): {m['line_no']}")
                out.append(f"      canonical: {m['canonical_line']!r}")
                out.append(f"      this file: {m['file_line']!r}")

    out.append("")
    for w in result.warnings:
        out.append(f"WARNING: {w}")
    for e in result.errors:
        out.append(f"ERROR:   {e}")

    out.append("")
    if result.ok:
        suffix = f" (with {len(result.warnings)} warning(s))" if result.warnings else ""
        out.append(f"RESULT: PASS{suffix}  -> exit code 0")
    else:
        out.append(f"RESULT: FAIL ({len(result.errors)} error(s))  -> exit code 1")
    out.append(bar)
    return "\n".join(out)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        description="Fail CI when any *.Build.cs [UAK BACKEND LAYOUT] block drifts."
    )
    parser.add_argument(
        "root", nargs="?", default=None,
        help="repository root (default: parent of Tools/)",
    )
    args = parser.parse_args(argv)
    root = Path(args.root).resolve() if args.root else REPO_ROOT
    result = check(root)
    print(format_report(result))
    return 0 if result.ok else 1


if __name__ == "__main__":
    sys.exit(main())

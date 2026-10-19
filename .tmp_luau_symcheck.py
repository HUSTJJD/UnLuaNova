"""Decide whether Luau can be built as one unity translation unit.

Luau has no official single-file aggregation (unlike Lua's onelua.c), so building it
through UBT means either:
  a) one unity TU that #includes every Luau source, or
  b) one thin wrapper per Luau source, each its own TU.

(a) is tidier but breaks if two files define a same-named file-scope symbol (`static`
function or variable, or an anonymous-namespace name): in separate TUs that is fine,
in one TU it is a redefinition. This script looks for exactly that.
"""

import re
from collections import defaultdict
from pathlib import Path

ROOT = Path(r"d:/learning/UnLua/Plugins/UnLuaNova/Source/ThirdParty/Luau")
GROUPS = ["VM", "Compiler", "Ast", "Common"]

# `static <anything> <name>` at file scope, plus `namespace { ... }` style is skipped
# (anonymous namespaces are already unique per TU only if not merged - they still
# collide in a unity build, but they are rare in Luau and are reported separately).
STATIC_RE = re.compile(r"^\s*static\s+[^;{()]*?\b([A-Za-z_]\w*)\s*(?:\(|\[|=|;|,)", re.M)
ANON_NS_RE = re.compile(r"\bnamespace\s*(?=\{|$)")

owners = defaultdict(set)
total = 0
anon_ns = []

for group in GROUPS:
    for path in sorted((ROOT / group / "src").rglob("*.cpp")):
        text = path.read_text(encoding="utf-8", errors="replace")
        total += 1
        if ANON_NS_RE.search(text):
            anon_ns.append(str(path.relative_to(ROOT)))
        for match in STATIC_RE.finditer(text):
            owners[match.group(1)].add(str(path.relative_to(ROOT)))

collisions = {name: files for name, files in owners.items() if len(files) > 1}

print("luau sources scanned :", total)
print("file-scope statics   :", len(owners))
print("colliding names      :", len(collisions))
for name, files in sorted(collisions.items())[:25]:
    print("  COLLISION", name, "<-", ", ".join(sorted(files)))
print("files using anonymous namespace:", len(anon_ns))
for item in anon_ns[:10]:
    print("  ", item)

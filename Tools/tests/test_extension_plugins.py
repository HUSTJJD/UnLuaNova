"""Static contract checks for the three UnLuaNova extension plugins.

Pure static analysis only: no UE build, no automation case registration, no repository
mutations. Uses only the standard library (pathlib / re / json). Every failure message
carries ``<relative/path>:<line>`` so a broken contract can be located directly.

Run from the repository root:
    python -B -m unittest discover -s Tools/tests -p "test_extension_plugins.py"
"""

from __future__ import annotations

import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXT = ROOT / "Plugins" / "UnLuaNovaExtensions"
SMOKE = ROOT / "Content" / "Script" / "Tests" / "Extensions" / "SmokeExtensions.lua"
MODULE_DOC = ROOT / "Docs" / "CN" / "Plugins_And_Modules.md"
LICENSE = ROOT / "LICENSE-THIRD-PARTY.md"

# The three plugin directories plus the third-party component each is recorded under in
# LICENSE-THIRD-PARTY.md. These are plugin / component identifiers, NOT Lua module names:
# every Lua module name checked below is extracted from the real C++ sources and docs.
PLUGINS = {
    "LuaProtobuf": {"license": "lua-protobuf"},
    "LuaRapidjson": {"license": "lua-rapidjson"},
    "LuaSocket": {"license": "LuaSocket 3.0-rc1"},
}

# Vendored third-party source directories shipped by each extension plugin.
VENDORED_DIRS = {
    "LuaProtobuf": ["Source/src", "Content/Script"],
    "LuaRapidjson": ["Source/src", "Source/include/rapidjson"],
    "LuaSocket": ["Source/src", "Content/Script"],
}

VALID_MODULE_TYPES = {"Runtime", "Editor", "Developer", "DeveloperTool", "Program"}
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp"}

# Real built-in loader names are declared as AddBuiltInLoader(TEXT("name"), ...) in the
# plugin's *Module.cpp. Extract them; never hard-code a Lua module name in these tests.
LOADER_RE = re.compile(r'AddBuiltInLoader\s*\(\s*TEXT\(\s*"([^"]+)"')
REQUIRE_RE = re.compile(r'require\s*\(?\s*"([^"]+)"')


def _rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def _line_of(text: str, needle: str) -> int:
    for i, line in enumerate(text.splitlines(), 1):
        if needle in line:
            return i
    return 0


class ExtensionPluginContractTests(unittest.TestCase):
    # ---------- helpers ----------

    def _uplugin(self, plugin):
        return EXT / plugin / (plugin + ".uplugin")

    def _load_json(self, path, text):
        try:
            return json.loads(text)
        except json.JSONDecodeError as exc:
            self.fail(f"{_rel(path)}:{exc.lineno}: invalid JSON: {exc.msg}")

    def _module_cpp(self, plugin):
        """Return the single *Module.cpp that registers built-in loaders."""
        found = sorted((EXT / plugin / "Source").rglob("*Module.cpp"))
        self.assertEqual(
            len(found), 1,
            f"{_rel(EXT / plugin / 'Source')}: expected exactly one *Module.cpp, "
            f"found {[p.name for p in found]}",
        )
        return found[0]

    def _registered_names(self, plugin):
        """Extract the AddBuiltInLoader(TEXT("...")) names from real C++ source."""
        cpp = self._module_cpp(plugin)
        text = _read(cpp)
        names = set(LOADER_RE.findall(text))
        line = _line_of(text, "AddBuiltInLoader") or 1
        self.assertTrue(
            names,
            f'{_rel(cpp)}:{line}: could not extract AddBuiltInLoader(TEXT("...")) names',
        )
        return cpp, names

    # ---------- (1) .uplugin contracts ----------

    def test_uplugin_files_exist(self):
        for plugin in PLUGINS:
            with self.subTest(plugin=plugin):
                p = self._uplugin(plugin)
                self.assertTrue(p.is_file(), f"{_rel(p)}:1: .uplugin file is missing")

    def test_uplugin_declares_unluanova_dependency(self):
        for plugin in PLUGINS:
            with self.subTest(plugin=plugin):
                p = self._uplugin(plugin)
                text = _read(p)
                data = self._load_json(p, text)
                deps = data.get("Plugins", [])
                ok = any(
                    isinstance(d, dict)
                    and d.get("Name") == "UnLuaNova"
                    and d.get("Enabled") is True
                    for d in deps
                )
                line = _line_of(text, '"Name": "UnLuaNova"') or 1
                self.assertTrue(
                    ok,
                    f"{_rel(p)}:{line}: missing Plugins dependency "
                    f"{{'Name': 'UnLuaNova', 'Enabled': true}}",
                )

    def test_uplugin_module_type_is_sane(self):
        for plugin in PLUGINS:
            with self.subTest(plugin=plugin):
                p = self._uplugin(plugin)
                text = _read(p)
                data = self._load_json(p, text)
                modules = data.get("Modules", [])
                modules_line = _line_of(text, '"Modules"') or 1
                self.assertTrue(
                    modules,
                    f"{_rel(p)}:{modules_line}: no Modules declared",
                )
                types = {m.get("Type") for m in modules if isinstance(m, dict)}
                line = _line_of(text, '"Type"') or 1
                self.assertTrue(
                    types & VALID_MODULE_TYPES,
                    f"{_rel(p)}:{line}: Modules.Type {sorted(types)} not in "
                    f"{sorted(VALID_MODULE_TYPES)}",
                )
                self.assertIn(
                    "Runtime", types,
                    f"{_rel(p)}:{line}: extension registers Lua loaders at runtime, "
                    "so a Runtime module is expected",
                )

    def test_enabled_by_default_is_not_forced(self):
        # The extensions are optional: UE treats a missing EnabledByDefault as false.
        # Whether omitted or explicit, they must not be force-enabled by default.
        for plugin in PLUGINS:
            with self.subTest(plugin=plugin):
                p = self._uplugin(plugin)
                text = _read(p)
                data = self._load_json(p, text)
                value = data.get("EnabledByDefault", False)
                line = _line_of(text, "EnabledByDefault") or 1
                self.assertIsInstance(
                    value, bool,
                    f"{_rel(p)}:{line}: EnabledByDefault must be a bool when present",
                )
                self.assertFalse(
                    value,
                    f"{_rel(p)}:{line}: optional extension must not be EnabledByDefault",
                )

    # ---------- (2) registered module names vs. smoke script ----------

    def test_smoke_requires_real_registered_names(self):
        smoke = _read(SMOKE)
        smoke_requires = set(REQUIRE_RE.findall(smoke))
        smoke_line = _line_of(smoke, "require(") or 1
        self.assertTrue(
            smoke_requires,
            f'{_rel(SMOKE)}:{smoke_line}: no require("...") names found',
        )
        # The smoke script is allowed extra pure-Lua helper requires (e.g. protoc); the
        # contract here is that each plugin has at least one SOURCE-registered name required.
        for plugin in PLUGINS:
            with self.subTest(plugin=plugin):
                cpp, registered = self._registered_names(plugin)
                hit = registered & smoke_requires
                cpp_line = _line_of(_read(cpp), "AddBuiltInLoader") or 1
                self.assertTrue(
                    hit,
                    f"{_rel(SMOKE)}:{smoke_line}: no require() name matches the "
                    f"registered names {sorted(registered)} in {_rel(cpp)}:{cpp_line}",
                )

    # ---------- (3) vendored third-party source dirs ----------

    def test_vendored_source_dirs_exist(self):
        for plugin, dirs in VENDORED_DIRS.items():
            for rel in dirs:
                with self.subTest(plugin=plugin, dir=rel):
                    d = EXT / plugin / rel
                    self.assertTrue(
                        d.is_dir(),
                        f"{_rel(d)}: vendored third-party directory is missing",
                    )
                    files = [f for f in d.rglob("*") if f.is_file()]
                    self.assertTrue(
                        files, f"{_rel(d)}: vendored third-party directory is empty"
                    )
                    if rel == "Source/src":
                        sources = [
                            f for f in files if f.suffix.lower() in SOURCE_SUFFIXES
                        ]
                        self.assertTrue(
                            sources,
                            f"{_rel(d)}: no C/C++ source files in vendored source dir",
                        )

    # ---------- (4) LICENSE records all three ----------

    def test_license_records_all_three_components(self):
        text = _read(LICENSE)
        low = text.lower()
        for plugin, meta in PLUGINS.items():
            with self.subTest(plugin=plugin):
                marker = meta["license"]
                line = _line_of(text, marker) or 1
                self.assertIn(
                    marker.lower(), low,
                    f"{_rel(LICENSE)}:{line}: third-party component '{marker}' "
                    f"({plugin}) is not recorded",
                )

    # ---------- (5) documented names and smoke names, all backed by sources ----------

    def test_documented_and_smoke_names_match_sources(self):
        doc = _read(MODULE_DOC)
        doc_names = set(REQUIRE_RE.findall(doc))
        doc_line = _line_of(doc, "require(") or 1
        msg = (
            f"{_rel(MODULE_DOC)}:{doc_line}: expected at least three documented "
            f"require() names, found {len(doc_names)}"
        )
        self.assertGreaterEqual(len(doc_names), 3, msg)

        smoke = _read(SMOKE)
        smoke_requires = set(REQUIRE_RE.findall(smoke))
        smoke_line = _line_of(smoke, "require(") or 1

        # Fallback / cross-check per the contract: the names documented in
        # Docs/CN/Plugins_And_Modules.md must all be real registered names AND must all
        # appear as require() calls in the smoke script ("all three module names appear").
        all_registered = set()
        for plugin in PLUGINS:
            _, registered = self._registered_names(plugin)
            all_registered |= registered

        for name in sorted(doc_names):
            with self.subTest(documented=name):
                self.assertIn(
                    name, all_registered,
                    f"{_rel(MODULE_DOC)}:{doc_line}: documented '{name}' is not "
                    "registered by any *Module.cpp",
                )
                self.assertIn(
                    name, smoke_requires,
                    f"{_rel(SMOKE)}:{smoke_line}: documented '{name}' is not required "
                    "by the smoke script",
                )


if __name__ == "__main__":
    unittest.main()

"""Static regression guard for the backend-isolated Editor targets.

Nothing here compiles Unreal or reads build products: it checks the *source contract*
that makes two Lua backends coexist without overwriting each other. A build that
silently loses one of these invariants is much more expensive to diagnose than a
failed unit test, so drift is caught here.

Contract:
  - two targets named UnLuaNovaDemo<Backend>Editor
  - every module's Build.cs derives backendVariant from Target.Name and sets
    ModuleRules.BinariesSubFolder = "Backends/" + backendVariant
  - every Build.cs that reads LuaBackend from the ini lets backendVariant win
  - Tools/SwitchBackend.ps1 activates exactly one UnrealEditor.modules manifest
  - Tools/RunTests.ps1 -BackendTargets looks in the isolated paths
"""

from pathlib import Path
import unittest

TOOLS = Path(__file__).resolve().parents[1]
REPO = TOOLS.parent

PLUGIN_SOURCE = REPO / "Plugins" / "UnLuaNova" / "Source"
EXT_SOURCE = REPO / "Plugins" / "UnLuaNovaExtensions"
TESTSUITE_SOURCE = REPO / "Plugins" / "UnLuaNovaTestSuite"
PROJECT_SOURCE = REPO / "Source"

LAYOUT_BEGIN = "[UAK BACKEND LAYOUT"
LAYOUT_END = "[/UAK BACKEND LAYOUT]"

# Fragments that every layout block must carry. Compared as substrings of the stripped
# lines so that single-statement `if (...) BinariesSubFolder = ...;` and the main module's
# braced variant both satisfy the contract.
REQUIRED_LAYOUT_FRAGMENTS = (
    'const string backendTargetPrefix = "UnLuaNovaDemo";',
    'const string backendTargetSuffix = "Editor";',
    'string backendVariant = "";',
    'if (Target.Name.StartsWith(backendTargetPrefix) && Target.Name.EndsWith(backendTargetSuffix))',
    'backendVariant = Target.Name.Substring(backendTargetPrefix.Length,',
    'Target.Name.Length - backendTargetPrefix.Length - backendTargetSuffix.Length);',
    'BinariesSubFolder = "Backends/" + backendVariant;',
)

# Build.cs files that read Config/DefaultUnLuaNova.ini to pick the VM module. They must
# also honour an isolated target, otherwise the demo/test/extension modules link the VM
# the ini names while UnLuaNova itself links the one the target names.
INI_READERS = (
    PROJECT_SOURCE / "UnLuaNovaDemo" / "UnLuaNovaDemo.Build.cs",
    TESTSUITE_SOURCE / "Source" / "UnLuaNovaTestSuite" / "UnLuaNovaTestSuite.Build.cs",
    EXT_SOURCE / "LuaSocket" / "Source" / "LuaSocket.Build.cs",
    EXT_SOURCE / "LuaRapidjson" / "Source" / "LuaRapidjson.Build.cs",
    EXT_SOURCE / "LuaProtobuf" / "Source" / "LuaProtobuf.Build.cs",
    PLUGIN_SOURCE / "UnLuaNova" / "UnLuaNova.Build.cs",
)


def build_cs_files():
    return sorted(
        p for root in (REPO / "Plugins", PROJECT_SOURCE)
        for p in root.rglob("*.Build.cs")
    )


def code_lines(path):
    """Stripped, non-empty lines, with comments removed (they are documentation only)."""
    lines = []
    for raw in path.read_text(encoding="utf-8-sig").splitlines():
        line = raw.strip()
        if line and not line.startswith("//"):
            lines.append(line)
    return lines


class BackendIsolationTests(unittest.TestCase):

    def test_all_build_cs_have_the_layout_block(self):
        files = build_cs_files()
        self.assertEqual(9, len(files), "a new Build.cs needs the layout block too")
        for path in files:
            with self.subTest(module=path.name):
                text = path.read_text(encoding="utf-8-sig")
                lines = code_lines(path)
                self.assertTrue(any(LAYOUT_BEGIN in line for line in text.splitlines()),
                                f"{path.name} lost the layout begin marker")
                self.assertTrue(any(LAYOUT_END in line for line in text.splitlines()),
                                f"{path.name} lost the layout end marker")
                for fragment in REQUIRED_LAYOUT_FRAGMENTS:
                    self.assertTrue(any(fragment in line for line in lines),
                                    f"{path.name} drifted from the layout contract: {fragment}")
                self.assertNotIn(
                    "UnLuaNovaBackendLayout", text,
                    "cross-file helper references are not allowed: UBT may compile "
                    "plugin rules in separate assemblies",
                )

    def test_backend_variant_declared_exactly_once(self):
        for path in build_cs_files():
            with self.subTest(module=path.name):
                text = path.read_text(encoding="utf-8-sig")
                self.assertEqual(1, text.count('string backendVariant = ""'),
                                 "duplicate declaration would not compile")

    def test_ini_readers_let_the_target_win(self):
        for path in INI_READERS:
            with self.subTest(module=path.name):
                text = path.read_text(encoding="utf-8-sig")
                lines = code_lines(path)
                self.assertIn('GetString(section, "LuaBackend"', text,
                              f"{path.name} is no longer an ini reader - update this test")
                self.assertTrue(
                    any("backendVariant.Length > 0" in line and "backend = backendVariant" in line
                        for line in lines) or "backend = backendVariant;" in lines,
                    f"{path.name} would link the ini backend while UnLuaNova links the target one",
                )

    def test_main_module_validates_unknown_variants(self):
        text = (PLUGIN_SOURCE / "UnLuaNova" / "UnLuaNova.Build.cs").read_text(encoding="utf-8-sig")
        self.assertIn('backendVariant != "Lua" && backendVariant != "Luau"', text)
        self.assertIn("throw new BuildException", text)
        self.assertIn("backend = backendVariant;", text)

    def test_both_backend_targets_exist(self):
        for backend in ("Lua", "Luau"):
            path = PROJECT_SOURCE / f"UnLuaNovaDemo{backend}Editor.Target.cs"
            with self.subTest(backend=backend):
                self.assertTrue(path.is_file(), f"missing {path.name}")
                lines = code_lines(path)
                self.assertIn(f"public class UnLuaNovaDemo{backend}EditorTarget : TargetRules", lines)
                self.assertIn("Type = TargetType.Editor;", lines)
                self.assertIn('ExtraModuleNames.Add("UnLuaNovaDemo");', lines)
                self.assertIn("DefaultBuildSettings = BuildSettingsVersion.Latest;", lines)
                self.assertFalse(any("GlobalDefinitions" in line for line in lines),
                                 "GlobalDefinitions is [RequiresUniqueBuildEnvironment]")

    def test_ordinary_editor_target_still_exists(self):
        path = PROJECT_SOURCE / "UnLuaNovaDemoEditor.Target.cs"
        self.assertTrue(path.is_file())
        self.assertNotIn("Backends/", path.read_text(encoding="utf-8-sig"),
                         "the default target must keep the unsuffixed output paths")

    def test_switch_backend_covers_every_module_root(self):
        text = (TOOLS / "SwitchBackend.ps1").read_text(encoding="utf-8-sig")
        for needle in (
            '"Binaries/$Platform"',
            '"Plugins/UnLuaNovaTestSuite/Binaries/$Platform"',
            '"Plugins/UnLuaNovaExtensions"',
            '"UnLuaNova.uplugin"',
            '"UnrealEditor.modules"',
            '".disabled"',
            'LuaBackend=$Value',
            'UnrealEditor-$Backend.dll',
            'Build.bat UnLuaNovaDemo${Backend}Editor',
            '"Default"',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)

    def test_runtests_backendtargets_uses_isolated_paths(self):
        text = (TOOLS / "RunTests.ps1").read_text(encoding="utf-8-sig")
        for needle in (
            "[switch] $BackendTargets",
            "$script:BackendBuildSubFolder",
            '"Backends/$B"',
            '"UnLuaNovaDemo${B}Editor"',
            "SwitchBackend.ps1",
            "$ThisTargetName",
            "if ($BackendTargets) { return }",
            "-BackendTargets needs the Editor carrier",
            "-NetMatrix and -BackendTargets cannot be combined",
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)

    def test_braces_balanced_in_edited_csharp(self):
        paths = list(build_cs_files()) + [
            PROJECT_SOURCE / "UnLuaNovaDemoLuaEditor.Target.cs",
            PROJECT_SOURCE / "UnLuaNovaDemoLuauEditor.Target.cs",
        ]
        for path in paths:
            with self.subTest(module=path.name):
                text = path.read_text(encoding="utf-8-sig")
                self.assertEqual(text.count("{"), text.count("}"),
                                 f"unbalanced braces in {path.name}")


if __name__ == "__main__":
    unittest.main()

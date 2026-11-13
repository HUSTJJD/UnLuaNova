"""Static + functional regression guard for backend-isolated builds.

Nothing here compiles Unreal. It checks the source contract that lets both Lua backends
coexist without overwriting each other, across all three carriers (Editor/Game/Server),
the Development and Release pipelines, the connected network matrix and the outer
UEAbilityKit doctor. A build that silently loses one of these invariants is much more
expensive to diagnose than a failed unit test, so drift is caught here.

Contract:
  - six targets named UnLuaNovaDemo<Backend>{Editor,Server,''} plus the three ordinary ones
  - every module's Build.cs carries an *identical* [UAK BACKEND LAYOUT] block that derives
    backendVariant from Target.Name and sets ModuleRules.BinariesSubFolder
  - every Build.cs that reads LuaBackend from the ini lets backendVariant win
  - Tools/SwitchBackend.ps1 discovers *.modules dynamically (AppName differs per carrier)
    and leaves exactly one backend's manifests live
  - isolation is unconditional in RunTests/RunDevelopmentTests/RunNetMatrix/ReleasePipeline; no switch
  - Tools/RunTests.ps1 is a parameterless entry point: no switches, no partial runs
  - Tools/unluanova/{state,doctor}.py in the outer repo understand Backends/<Backend>
"""

from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
REPO = TOOLS.parent
OUTER = REPO.parent.parent
OUTER_UNLUANOVA = OUTER / "Tools" / "unluanova"

# The outer workspace is a *separate* repository (UEAbilityKit) that happens to
# sit next to this submodule on the maintainer's machine. A standalone checkout
# -- any independent clone -- has no Tools/unluanova at all. Those checks are
# valuable locally but must skip, not fail, when the outer workspace is absent.
OUTER_AVAILABLE = (
    (OUTER_UNLUANOVA / "state.py").is_file()
    and (OUTER_UNLUANOVA / "doctor.py").is_file()
)
OUTER_SKIP_REASON = "outer UEAbilityKit workspace is absent in this checkout"

PLUGIN_SOURCE = REPO / "Plugins" / "UnLuaNova" / "Source"
EXT_SOURCE = REPO / "Plugins" / "UnLuaNovaExtensions"
TESTSUITE_SOURCE = REPO / "Plugins" / "UnLuaNovaTestSuite"
PROJECT_SOURCE = REPO / "Source"

LAYOUT_BEGIN = "[UAK BACKEND LAYOUT"
LAYOUT_END = "[/UAK BACKEND LAYOUT]"

# Fragments that every layout block must carry. Compared as substrings of the stripped
# code lines (comments are documentation only), so formatting changes do not break it.
REQUIRED_LAYOUT_FRAGMENTS = (
    'const string backendTargetPrefix = "UnLuaNovaDemo";',
    'string backendVariant = "";',
    'if (Target.Name.StartsWith(backendTargetPrefix))',
    'string backendRest = Target.Name.Substring(backendTargetPrefix.Length);',
    'string[] backendSuffixes = { "Editor", "Server", "" };',
    'foreach (string backendSuffix in backendSuffixes)',
    'if (backendRest.EndsWith(backendSuffix))',
    'backendVariant = backendRest.Substring(0, backendRest.Length - backendSuffix.Length);',
    'if (backendVariant != "Lua" && backendVariant != "Luau")',
    'throw new BuildException(',
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

BACKENDS = ("Lua", "Luau")
CARRIERS = (("Editor", "Editor"), ("", "Game"), ("Server", "Server"))
ORDINARY_TARGETS = ("UnLuaNovaDemo", "UnLuaNovaDemoEditor", "UnLuaNovaDemoServer")


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


def layout_block(path):
    text = path.read_text(encoding="utf-8-sig")
    start = text.index(LAYOUT_BEGIN)
    end = text.index(LAYOUT_END, start) + len(LAYOUT_END)
    return text[start:end]


class BackendIsolationTests(unittest.TestCase):

    def test_all_build_cs_have_the_layout_block(self):
        files = build_cs_files()
        self.assertEqual(9, len(files), "a new Build.cs needs the layout block too")
        for path in files:
            with self.subTest(module=path.name):
                text = path.read_text(encoding="utf-8-sig")
                lines = code_lines(path)
                self.assertIn(LAYOUT_BEGIN, text, f"{path.name} lost the layout begin marker")
                self.assertIn(LAYOUT_END, text, f"{path.name} lost the layout end marker")
                for fragment in REQUIRED_LAYOUT_FRAGMENTS:
                    self.assertTrue(any(fragment in line for line in lines),
                                    f"{path.name} drifted from the layout contract: {fragment}")
                self.assertNotIn(
                    "UnLuaNovaBackendLayout", text,
                    "cross-file helper references are not allowed: UBT may compile "
                    "plugin rules in separate assemblies",
                )

    def test_layout_blocks_are_byte_identical(self):
        blocks = {layout_block(p) for p in build_cs_files()}
        self.assertEqual(1, len(blocks), "every Build.cs must carry the same layout block")

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

    def test_six_backend_targets_exist(self):
        for backend in BACKENDS:
            for suffix, target_type in CARRIERS:
                name = f"UnLuaNovaDemo{backend}{suffix}"
                path = PROJECT_SOURCE / f"{name}.Target.cs"
                with self.subTest(target=name):
                    self.assertTrue(path.is_file(), f"missing {path.name}")
                    lines = code_lines(path)
                    self.assertIn(f"public class {name}Target : TargetRules", lines)
                    self.assertIn(f"Type = TargetType.{target_type};", lines)
                    self.assertIn('ExtraModuleNames.Add("UnLuaNovaDemo");', lines)
                    self.assertIn("DefaultBuildSettings = BuildSettingsVersion.Latest;", lines)
                    self.assertFalse(any("GlobalDefinitions" in line for line in lines),
                                     "GlobalDefinitions is [RequiresUniqueBuildEnvironment]")

    def test_ordinary_targets_still_exist(self):
        for name in ORDINARY_TARGETS:
            path = PROJECT_SOURCE / f"{name}.Target.cs"
            with self.subTest(target=name):
                self.assertTrue(path.is_file())
                self.assertNotIn("Backends/", path.read_text(encoding="utf-8-sig"),
                                 "the default targets must keep the unsuffixed output paths")

    def test_default_editor_target_is_explicit(self):
        # UAT BuildCookRun refuses to pick an editor target when a project has more than
        # one *Editor.Target.cs and no DefaultEditorTarget, even with -skipbuildeditor.
        # The isolated targets add two, so the ordinary one must be named explicitly.
        ini = (REPO / "Config" / "DefaultEngine.ini").read_text(encoding="utf-8-sig")
        editor_targets = list(PROJECT_SOURCE.glob("*Editor.Target.cs"))
        self.assertGreater(len(editor_targets), 1,
                           "this guard matters only while several editor targets exist")
        self.assertIn("[/Script/BuildSettings.BuildSettings]", ini)
        self.assertIn("DefaultEditorTarget=UnLuaNovaDemoEditor", ini)

    def test_switch_backend_discovers_manifests_dynamically(self):
        text = (TOOLS / "SwitchBackend.ps1").read_text(encoding="utf-8-sig")
        for needle in (
            '"Binaries/$Platform"',
            '"Plugins/UnLuaNovaTestSuite/Binaries/$Platform"',
            '"Plugins/UnLuaNovaExtensions"',
            '"UnLuaNova.uplugin"',
            '$ManifestPattern = "*.modules"',
            '$DisabledSuffix = ".disabled"',
            '$IsolatedDir = "Backends"',
            'function Get-ManifestEntries',
            'Path = $LivePath; Zone = $Zone;',
            '$HasTargetIsolated',
            'Set-ManifestEnabled -Path $E.Path',
            'LuaBackend=$Value',
            'UnLuaNovaDemo${Backend}Server',
            '*-${Backend}.dll',
            '"Default"',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)
        self.assertNotIn('$ManifestName = "UnrealEditor.modules"', text,
                         "Game/Server targets use their own AppName as manifest name")

    def test_development_driver_uses_isolated_targets_unconditionally(self):
        # The selective/debug driver was split out of the parameterless entry point; it
        # still has to build the isolated targets for every carrier it runs.
        text = (TOOLS / "RunDevelopmentTests.ps1").read_text(encoding="utf-8-sig")
        self.assertNotIn("$BackendTargets", text)
        self.assertNotIn("-BackendTargets", text)
        self.assertNotIn("-Profile", text)
        for needle in (
            "$script:BackendBuildSubFolder",
            '$script:BackendGameExeName',
            '$script:BackendBuildSubFolder = "Backends/$B"',
            '$ThisTargetName = if ($IsGameCarrier) { "UnLuaNovaDemo${B}" }',
            '"UnLuaNovaDemo${B}Editor"',
            'Resolve-GameExe -Name $script:BackendGameExeName',
            '$script:BackendIntermediateFolder = if ($IsGameCarrier) { $ThisTargetName }',
            'SwitchBackend.ps1',
            "Backend isolation is unconditional",
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)
        self.assertNotIn("'Baseline', 'NoCache', 'PruneLegacy'", text,
                         "the removed Release dispatch left Release-only switches behind")

    def test_runtests_is_a_parameterless_entry_point(self):
        # One command, no switches: engine/project discovery, backends, configurations
        # and outputs are all defaults. A forgotten -Filter/-Profile must not silently
        # turn a release verdict into a partial run.
        text = (TOOLS / "RunTests.ps1").read_text(encoding="utf-8-sig")
        for banned in ("$PSBoundParameters", "-Profile", "$BackendTargets"):
            with self.subTest(banned=banned):
                self.assertNotIn(banned, text)
        # A real parameter block starts at column 0; "param()" in a comment is fine.
        self.assertNotRegex(text, r"(?m)^param\(")
        for needle in (
            "$args.Count -gt 0",
            "takes no parameters",
            "RunDevelopmentTests.ps1",
            "& (Join-Path $PSScriptRoot 'ReleasePipeline.ps1')",
            "exit $LASTEXITCODE",
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)
        self.assertNotIn("BuildCookRun", text,
                         "the entry point must not duplicate the release matrix")

    def test_runnetmatrix_uses_isolated_targets(self):
        text = (TOOLS / "RunNetMatrix.ps1").read_text(encoding="utf-8-sig")
        self.assertNotIn("$BackendTargets", text)
        self.assertNotIn("-BackendTargets", text)
        for needle in (
            '"UnLuaNovaDemo$B"',
            '"UnLuaNovaDemo${B}Server"',
            'Saved/StagedBuilds/Windows/Backends/$B',
            'Saved/StagedBuilds/WindowsServer/Backends/$B',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)

    def test_release_pipeline_isolates_every_stage(self):
        text = (TOOLS / "ReleasePipeline.ps1").read_text(encoding="utf-8-sig")
        self.assertNotIn("$BackendTargets", text)
        self.assertNotIn("-BackendTargets", text)
        for needle in (
            '"UnLuaNovaDemo${B}Editor"',
            '"UnLuaNovaDemo${B}"',
            '"UnLuaNovaDemo${B}Server"',
            '"-stagingdirectory=$StageDir"',
            '"-stagingdirectory=$ServerStageArg"',
            '$script:ReleaseDefinitionsSubFolder',
            '"isolated=True"',
            "'-Backend', $B",
            "'-Backend', 'Default'",
            'restore-backend-layout',
            "Run-Script 'RunDevelopmentTests.ps1'",
            'isolated_backends = $true',
            "$BackendOrder = @('Lua', 'Luau')",
            'if ($ConfigCaptured) {',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)
        support = (TOOLS / "ReleaseSupport.ps1").read_text(encoding="utf-8-sig")
        self.assertIn('function Get-ReleaseBuiltBackend([string] $PluginRoot, [string] $BinariesSubFolder',
                      support)
        self.assertIn('if ($BinariesSubFolder) { $Rel = "$Rel/$BinariesSubFolder" }', support)

    def test_no_backend_targets_switch_is_reintroduced(self):
        # Isolation is the only path. Re-adding a switch would silently restore the
        # single-slot layout that overwrites one backend's binaries with the other's.
        for name in ("RunTests.ps1", "RunDevelopmentTests.ps1", "RunNetMatrix.ps1",
                 "ReleasePipeline.ps1"):
            with self.subTest(script=name):
                text = (TOOLS / name).read_text(encoding="utf-8-sig")
                self.assertNotIn("BackendTargets", text)

    def test_release_pipeline_isolates_cook_outputs(self):
        text = (TOOLS / "ReleasePipeline.ps1").read_text(encoding="utf-8-sig")
        for needle in (
            '"-CookOutputDir=$CookDir"',
            '"-CookOutputDir=$ServerCookDir"',
            '$script:ReleaseCookRoot = Join-Path $RepoRoot "Saved/Cooked/Backends/$B"',
            "'Development/Windows'",
            "'Shipping/Windows'",
            "'Development/WindowsServer'",
            '"cook=isolated"',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, text)
        # A path whose last segment is not SC.CookPlatform makes the stager append an
        # extra Windows/WindowsServer directory that the cooker never wrote.
        self.assertNotIn('"-CookOutputDir=$script:ReleaseCookRoot"', text)
        self.assertEqual(2, text.count('"-CookOutputDir='),
                         "every BuildCookRun must pass its own isolated cook directory")

    def test_static_binding_sources_are_isolated_per_backend(self):
        pipeline = (TOOLS / "ReleasePipeline.ps1").read_text(encoding="utf-8-sig")
        support = (TOOLS / "ReleaseSupport.ps1").read_text(encoding="utf-8-sig")
        generator = (REPO / "Plugins" / "UnLuaNova" / "Source" / "UnLuaNovaEditor" /
                     "Private" / "Commandlets" / "UnLuaNovaStaticBindingGenerator.cpp").read_text(
                         encoding="utf-8-sig")
        # The backup/stub/restore choreography existed only because both backends shared
        # one global .cpp. None of it may come back.
        for text, label in ((pipeline, "ReleasePipeline.ps1"), (support, "ReleaseSupport.ps1")):
            with self.subTest(script=label):
                for banned in ("Backup-ReleaseStaticBindings", "Restore-ReleaseStaticBindings",
                               "Test-ReleaseBindingStub", "static-binding-backup", "$BindingState",
                               "$BindingBackups"):
                    self.assertNotIn(banned, text, f"{label} still serialises a shared binding file")
        for needle in (
            "Generated/Backends/$Backend/",
            "function Get-ReleaseStaticBindingSource([string] $Project, [string] $Backend)",
            "Test-ReleaseBindingGenerated -Path $BindingSource -Backend $B",
            '"binding=isolated"',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, pipeline if needle.startswith('"binding') else support
                              if not needle.startswith("Test-") else pipeline)
        for needle in (
            'TEXT("Generated"), TEXT("Backends"), GStaticBindingBackendName',
            "GStaticBindingBackendValue",
            '#if defined(UNLUANOVA_LUA_BACKEND) && UNLUANOVA_LUA_BACKEND == %d',
            '#endif // UNLUANOVA_LUA_BACKEND',
            "removed the legacy shared source",
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, generator)

    @unittest.skipUnless(OUTER_AVAILABLE, OUTER_SKIP_REASON)
    def test_outer_doctor_understands_isolated_layout(self):
        state_py = (OUTER_UNLUANOVA / "state.py").read_text(encoding="utf-8-sig")
        for needle in (
            'ISOLATED_ROOT = BINARIES_DIR / "Backends"',
            'MANIFEST_SUFFIX = ".modules"',
            'DISABLED_SUFFIX = ".disabled"',
            'def backend_from_demo_ini',
            'def module_manifests',
            'def _dll_backends',
            'def backend_layout',
            'def built_backends',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, state_py)
        doctor_py = (OUTER_UNLUANOVA / "doctor.py").read_text(encoding="utf-8-sig")
        for needle in (
            'state.backend_layout()',
            'state.backend_from_demo_ini()',
            'active == "ambiguous"',
            'backend-manifest',
            'Backends/<Backend>/',
        ):
            with self.subTest(needle=needle):
                self.assertIn(needle, doctor_py)

    def test_braces_balanced_in_edited_csharp(self):
        paths = list(build_cs_files()) + [
            PROJECT_SOURCE / f"UnLuaNovaDemo{b}{s}.Target.cs"
            for b in BACKENDS for s, _ in CARRIERS
        ]
        for path in paths:
            with self.subTest(module=path.name):
                text = path.read_text(encoding="utf-8-sig")
                self.assertEqual(text.count("{"), text.count("}"),
                                 f"unbalanced braces in {path.name}")


@unittest.skipUnless(OUTER_AVAILABLE, OUTER_SKIP_REASON)
class OuterDoctorFunctionalTests(unittest.TestCase):
    """Exercise the outer doctor against synthetic Binaries trees (no Unreal build)."""

    @classmethod
    def setUpClass(cls):
        sys.path.insert(0, str(OUTER_UNLUANOVA))
        import doctor  # noqa: E402
        import state  # noqa: E402
        cls.doctor = doctor
        cls.state = state

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.binaries = self.root / "Binaries" / "Win64"
        self.binaries.mkdir(parents=True)
        self.outer_ini = self.root / "outer.ini"
        self.demo_ini = self.root / "demo.ini"
        self._write_ini(self.outer_ini, "Lua")
        self._write_ini(self.demo_ini, "Lua")
        patchers = [
            mock.patch.object(self.state, "BINARIES_DIR", self.binaries),
            mock.patch.object(self.state, "ISOLATED_ROOT", self.binaries / "Backends"),
            mock.patch.object(self.state, "UNLUANOVA_INI", self.outer_ini),
            mock.patch.object(self.state, "DEMO_INI", self.demo_ini),
        ]
        for patcher in patchers:
            patcher.start()
            self.addCleanup(patcher.stop)

    def _write_ini(self, path, backend):
        path.write_text(f"[{self.state.INI_SECTION}]\nLuaBackend={backend}\n", encoding="utf-8")

    def _dll(self, backend, prefix="UnrealEditor"):
        folder = self.binaries / "Backends" / backend
        folder.mkdir(parents=True, exist_ok=True)
        (folder / f"{prefix}-{backend}.dll").write_bytes(b"")

    def _manifest(self, backend, name="UnrealEditor.modules", live=True):
        folder = self.binaries / "Backends" / backend
        folder.mkdir(parents=True, exist_ok=True)
        suffix = "" if live else self.state.DISABLED_SUFFIX
        (folder / f"{name}{suffix}").write_text("{}", encoding="utf-8")

    def _root_dll(self, backend):
        (self.binaries / f"UnrealEditor-{backend}.dll").write_bytes(b"")

    def _root_manifest(self, live=True):
        suffix = "" if live else self.state.DISABLED_SUFFIX
        (self.binaries / f"UnrealEditor.modules{suffix}").write_text("{}", encoding="utf-8")

    def _binary_items(self):
        return [i for i in self.doctor.inspect().items if i["check"] == "backend-binary"]

    def test_isolated_dlls_are_not_a_fork_error(self):
        self._dll("Lua")
        self._dll("Luau")
        self._manifest("Lua", live=True)
        self._manifest("Luau", live=False)
        self.assertEqual(["Luau", "Lua"], self.state.built_backends())
        self.assertEqual("Lua", self.state.backend_layout()["active"])
        items = self._binary_items()
        self.assertEqual(["ok"], [i["level"] for i in items])
        self.assertIn("产物并存", items[0]["detail"])

    def test_game_target_manifest_names_are_recognised(self):
        self._dll("Lua", prefix="UnLuaNovaDemoLua")
        self._dll("Luau", prefix="UnLuaNovaDemoLuau")
        self._manifest("Lua", name="UnLuaNovaDemoLua.modules", live=True)
        self._manifest("Luau", name="UnLuaNovaDemoLuau.modules", live=False)
        self.assertEqual(["Luau", "Lua"], self.state.built_backends())
        self.assertEqual(["ok"], [i["level"] for i in self._binary_items()])

    def test_two_live_isolated_manifests_are_ambiguous(self):
        self._dll("Lua")
        self._dll("Luau")
        self._manifest("Lua", live=True)
        self._manifest("Luau", live=True)
        self.assertEqual("ambiguous", self.state.backend_layout()["active"])
        self.assertIn("error", [i["level"] for i in self._binary_items()])

    def test_no_live_manifest_is_an_error(self):
        self._dll("Lua")
        self._dll("Luau")
        self._manifest("Lua", live=False)
        self._manifest("Luau", live=False)
        self.assertIsNone(self.state.backend_layout()["active"])
        self.assertIn("error", [i["level"] for i in self._binary_items()])

    def test_demo_ini_label_must_match_live_manifest(self):
        self._dll("Lua")
        self._dll("Luau")
        self._manifest("Lua", live=True)
        self._manifest("Luau", live=False)
        self._write_ini(self.demo_ini, "Luau")
        details = " ".join(i["detail"] for i in self._binary_items() if i["level"] == "error")
        self.assertIn("分叉", details)

    def test_ordinary_layout_keeps_the_old_rule(self):
        self._root_dll("Lua")
        self._root_dll("Luau")
        self._root_manifest()
        self.assertEqual(["Luau", "Lua"], self.state.built_backends())
        self.assertEqual("Default", self.state.backend_layout()["active"])
        details = " ".join(i["detail"] for i in self._binary_items() if i["level"] == "error")
        self.assertIn("两个后端 DLL 同时存在", details)

    def test_root_manifest_live_with_isolated_dlls_warns(self):
        self._dll("Lua")
        self._dll("Luau")
        self._manifest("Lua", live=False)
        self._manifest("Luau", live=False)
        self._root_manifest()
        self.assertIn("warn", [i["level"] for i in self._binary_items()])

    def test_disabled_manifests_are_reported(self):
        self._manifest("Luau", name="UnLuaNovaDemoLuauServer.modules", live=False)
        layout = self.state.backend_layout()
        self.assertEqual(["UnLuaNovaDemoLuauServer.modules"],
                         layout["per_backend"]["Luau"]["disabled"])


if __name__ == "__main__":
    unittest.main()

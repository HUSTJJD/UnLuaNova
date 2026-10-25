[CmdletBinding()]
param([string] $EnginePath = '', [string] $ProjectPath = '', [string] $OutputDir = '',
    [ValidateRange(1, 1440)] [int] $TimeoutMinutes = 60, [switch] $DryRun)

$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path -Parent $PSScriptRoot
$PowerShellExe = (Get-Process -Id $PID).Path
. (Join-Path $PSScriptRoot 'ReleaseSupport.ps1')

if ($DryRun) {
    Write-Host @'
Release / Win64 / Lua + Luau (no operations performed):
  1. Lock project; snapshot config/environment and source provenance.
  2. Initialize pinned submodules; prepare isolated Python tools and MSVC environment.
  3. Repository audit, Python and PowerShell tooling regressions; build both analyzers.
  4. Per backend: enable features; archive old bindings; build Editor/generator; regenerate; rebuild.
  5. Build/cook/archive Development game and Dedicated Server targets.
  6. Full Editor automation including Slow, connected cooked peers, screenshots and benchmark.
  7. Fresh declaration export and mandatory backend type checking.
  8. Full applicable cooked Development automation with runtime manifest verification.
  9. Build/cook/archive Shipping game (packaging gate, not Shipping runtime certification).
 10. Validate manifests, memory leak evidence, peer evidence, media and performance baseline.
 11. Restore original config/environment; write release-summary.json and release-summary.md.
Missing/skipped evidence, timeouts, failed/blocked stages and absent performance baselines fail.
'@
    exit 0
}

if (-not $OutputDir) { $OutputDir = Join-Path $RepoRoot ("Saved/ReleaseTests/" + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
$OutputDir = [IO.Path]::GetFullPath($OutputDir)
if ((Test-Path $OutputDir) -and @(Get-ChildItem -LiteralPath $OutputDir -Force).Count) {
    throw "Release output must be empty to exclude stale evidence: $OutputDir"
}
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$Stages = [Collections.Generic.List[object]]::new()
$Statuses = @{}
$RequiredStages = @('tool-environment', 'repository-audit', 'tooling-tests', 'analyzer-LuaLS', 'analyzer-LuauLsp')
foreach ($BackendName in @('Lua', 'Luau')) {
    foreach ($Step in @('config', 'editor-build', 'static-bindings', 'package-development', 'server-build', 'package-server',
        'editor-tests', 'declarations', 'typecheck', 'editor-evidence', 'game-tests', 'game-evidence', 'reports', 'package-shipping')) {
        $RequiredStages += "$BackendName-$Step"
    }
}
$Started = [DateTime]::UtcNow
$LockStream = $null
$ConfigCaptured = $false
$BindingBackups = [Collections.Generic.List[object]]::new()
$SavedEnvironment = @{}
$EnvironmentNames = @('PATH', 'LUA_LANGUAGE_SERVER', 'LUAU_LSP', 'UNLUANOVA_RELEASE_TOKEN',
    'UNLUANOVA_TEST_MANIFEST', 'UNLUANOVA_TEST_PEER_EXE', 'UNLUANOVA_TEST_SERVER_EXE', 'UNLUANOVA_TEST_NET_ROOT', 'UNLUANOVA_RECORD_FRAMES')
foreach ($Name in $EnvironmentNames) { $SavedEnvironment[$Name] = [Environment]::GetEnvironmentVariable($Name, 'Process') }
$Provenance = [ordered]@{ platform = 'Win64'; backends = @('Lua', 'Luau'); machine = $env:COMPUTERNAME }

function Invoke-Stage([string] $Name, [scriptblock] $Action, [string[]] $Needs = @()) {
    $Watch = [Diagnostics.Stopwatch]::StartNew()
    $State = 'passed'
    $Detail = ''
    $Missing = @($Needs | Where-Object { $Statuses[$_] -ne 'passed' })
    Write-Host "[$Name]" -ForegroundColor Cyan
    if ($Missing.Count) { $State = 'blocked'; $Detail = 'Dependencies: ' + ($Missing -join ', ') }
    else {
        try { & $Action | Out-Host }
        catch { $State = 'failed'; $Detail = $_.Exception.Message; Write-Warning $Detail }
    }
    $Statuses[$Name] = $State
    $Stages.Add([ordered]@{ name = $Name; status = $State; seconds = $Watch.Elapsed.TotalSeconds; detail = $Detail })
    Write-ReleaseSummary
}

function Write-ReleaseSummary {
    $Complete = $script:Finished -eq $true
    $Passed = $Complete -and $Stages.Count -gt 0 -and @($Stages | Where-Object { $_.status -ne 'passed' }).Count -eq 0
    $Result = [ordered]@{ schema_version = 1; complete = $Complete; passed = $Passed;
        started_utc = $Started.ToString('o'); updated_utc = [DateTime]::UtcNow.ToString('o');
        scope = 'Win64 Editor and cooked Development automation; packaged Development Dedicated Server peers; Shipping packaging';
        provenance = $Provenance; stages = @($Stages.ToArray()) }
    $Result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputDir 'release-summary.json') -Encoding UTF8
    $Lines = @('# Release Validation', '', "Complete: $Complete. Passed: $Passed.", '',
        $Result.scope, '', '| Stage | Status | Detail |', '| --- | --- | --- |')
    foreach ($Stage in $Stages) {
        $Detail = $Stage.detail.Replace('|', '/').Replace("`r", ' ').Replace("`n", ' ')
        $Lines += "| $($Stage.name) | $($Stage.status) | $Detail |"
    }
    $Lines += @('', '## Automation Reports', '',
        '| Backend | Carrier | Total | Passed | Failed | Skipped | Screenshots | Clips | Report |',
        '| --- | --- | --- | --- | --- | --- | --- | --- | --- |')
    foreach ($BackendName in @('Lua', 'Luau')) {
        $Tag = $BackendName.ToLowerInvariant()
        foreach ($CarrierName in @('editor', 'game')) {
            $Relative = if ($CarrierName -eq 'editor') { "$Tag/editor/$Tag" } else { "$Tag/game" }
            $SummaryPath = Join-Path $OutputDir "$Relative/summary.json"
            if (Test-Path -LiteralPath $SummaryPath) {
                try {
                    $RunSummary = Get-Content -LiteralPath $SummaryPath -Raw -Encoding UTF8 | ConvertFrom-Json
                    $Lines += "| $BackendName | $CarrierName | $($RunSummary.total) | $($RunSummary.passed) | $($RunSummary.failed) | $($RunSummary.skipped) | $($RunSummary.screenshots) | $($RunSummary.videos) | [Details]($Relative/report.md) |"
                } catch {
                    $Lines += "| $BackendName | $CarrierName | - | - | - | - | - | - | Invalid summary |"
                }
            } else {
                $Lines += "| $BackendName | $CarrierName | - | - | - | - | - | - | Not available |"
            }
        }
    }
    $Lines += @('', 'The stage verdict also requires runtime manifests, memory and network evidence, type checks, and independent performance references. Test totals alone do not establish release acceptance.', '',
        'Source provenance and file hashes are recorded in [release-summary.json](release-summary.json).')
    $Lines | Set-Content -LiteralPath (Join-Path $OutputDir 'release-summary.md') -Encoding UTF8
}

function Run([string] $File, [string[]] $Arguments, [string] $Label, [int] $Minutes = $TimeoutMinutes) {
    Invoke-ReleaseProcess -File $File -Arguments $Arguments -Log (Join-Path $OutputDir "logs/$Label.log") -Minutes $Minutes
}
function Run-Script([string] $Script, [string[]] $Arguments, [string] $Label) {
    Run $PowerShellExe (@('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $PSScriptRoot $Script)) + $Arguments) $Label
}
function Build-Target([string] $Target, [string] $Label) {
    Run $BuildBat @($Target, 'Win64', 'Development', "-Project=$ProjectPath", '-WaitMutex') $Label
}
function Package-Game([string] $Configuration, [string] $Destination, [string] $Label) {
    Run $Uat @('BuildCookRun', "-project=$ProjectPath", '-noP4', '-unattended', '-utf8output',
        '-platform=Win64', "-clientconfig=$Configuration", "-target=$ProjectName", '-build', '-skipbuildeditor', '-cook',
        '-stage', '-pak', '-iostore', '-package', '-archive', "-archivedirectory=$Destination",
        "-stagingdirectory=$Destination-stage") $Label
    Get-ReleaseGameExe $Destination $ProjectName $Configuration | Out-Null
}

try {
    if (-not $ProjectPath) {
        $Projects = @(Get-ChildItem -LiteralPath $RepoRoot -Filter '*.uproject' -File)
        if ($Projects.Count -ne 1) { throw 'Expected exactly one .uproject in the repository.' }
        $ProjectPath = $Projects[0].FullName
    }
    $ProjectPath = (Resolve-Path -LiteralPath $ProjectPath).Path
    if ((Split-Path -Parent $ProjectPath) -ne $RepoRoot) { throw 'Release uses the repository demo project and its test assets.' }
    $ProjectName = [IO.Path]::GetFileNameWithoutExtension($ProjectPath)
    $EnginePath = Find-ReleaseEngine $EnginePath $ProjectPath
    $Editor = Join-Path $EnginePath 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $BuildBat = Join-Path $EnginePath 'Engine/Build/BatchFiles/Build.bat'
    $Uat = Join-Path $EnginePath 'Engine/Build/BatchFiles/RunUAT.bat'
    foreach ($Required in @($Editor, $BuildBat, $Uat)) {
        if (-not (Test-Path -LiteralPath $Required)) { throw "Missing prerequisite: $Required" }
    }
    $LockPath = Join-Path $RepoRoot 'Saved/RunTests.lock'
    New-Item -ItemType Directory -Path (Split-Path -Parent $LockPath) -Force | Out-Null
    # Read sharing lets our child verify ownership; a second writer still cannot acquire it.
    $LockStream = [IO.File]::Open($LockPath, [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::Read)
    $env:UNLUANOVA_RELEASE_TOKEN = [guid]::NewGuid().ToString('N')
    $Stamp = [Text.Encoding]::UTF8.GetBytes("pid=$PID token=$env:UNLUANOVA_RELEASE_TOKEN")
    $LockStream.SetLength(0)
    $LockStream.Write($Stamp, 0, $Stamp.Length)
    $LockStream.Flush()
    $IniPath = Join-Path $RepoRoot 'Config/DefaultUnLuaNova.ini'
    $IniExisted = Test-Path -LiteralPath $IniPath
    $IniBytes = if ($IniExisted) { [IO.File]::ReadAllBytes($IniPath) } else { $null }
    $ConfigCaptured = $true
    $Provenance.engine = Get-Content (Join-Path $EnginePath 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
    $Provenance.commit = (& git -C $RepoRoot rev-parse HEAD) -join ''
    $Provenance.worktree = @(& git -C $RepoRoot status --short)
    Run 'git' @('-C', $RepoRoot, 'diff', 'HEAD', '--binary', '--no-ext-diff') 'source-diff'
    $Provenance.source_diff_sha256 = (Get-FileHash (Join-Path $OutputDir 'logs/source-diff.log') -Algorithm SHA256).Hash
    $Provenance.untracked_files = @(
        foreach ($Relative in (& git -C $RepoRoot -c core.quotepath=false ls-files --others --exclude-standard -- Source Plugins Tools Config Docs)) {
            $Source = Join-Path $RepoRoot $Relative
            $Destination = Join-Path $OutputDir ('metadata/untracked-source/' + $Relative)
            New-Item -ItemType Directory -Path (Split-Path -Parent $Destination) -Force | Out-Null
            Copy-Item -LiteralPath $Source -Destination $Destination
            [ordered]@{ path = $Relative; sha256 = (Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash }
        }
    )
    $Provenance.project_sha256 = (Get-FileHash -LiteralPath $ProjectPath -Algorithm SHA256).Hash

    Invoke-Stage 'tool-environment' {
        $Python = (Get-Command python -ErrorAction Stop).Source
        $Venv = Join-Path $RepoRoot 'Saved/ReleaseTools/python'
        Run $Python @('-m', 'venv', $Venv) 'python-venv'
        $script:PythonExe = Join-Path $Venv 'Scripts/python.exe'
        Run $PythonExe @('-m', 'pip', 'install', '-r', (Join-Path $PSScriptRoot 'requirements.txt'), 'cmake==3.31.6', 'ninja==1.11.1.3') 'python-dependencies'
        $env:PATH = (Join-Path $Venv 'Scripts') + ';' + $env:PATH
        $VsWhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        $VS = & $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $VS) { throw 'Visual Studio C++ tools are required by Unreal and the analyzers.' }
        $Provenance.compiler_installation = @(& $VsWhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json) -join "`n" | ConvertFrom-Json
        # Configure only the child shell; do not leak VS environment changes into the caller.
        $script:DevShell = Join-Path $VS 'Common7/Tools/Launch-VsDevShell.ps1'
        if (-not (Test-Path $DevShell)) { throw "Missing $DevShell" }
        Run 'git' @('-C', $RepoRoot, 'submodule', 'update', '--init', '--recursive') 'submodules'
        $Provenance.submodules = @(& git -C $RepoRoot submodule status --recursive)
    }
    Invoke-Stage 'repository-audit' { Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'AuditRepository.py')) 'audit' } @('tool-environment')
    Invoke-Stage 'tooling-tests' {
        Run $PythonExe @('-B', '-m', 'unittest', 'discover', '-s', (Join-Path $PSScriptRoot 'tests'), '-p', 'test_*.py') 'python-tests'
        Run-Script 'tests/TestReleaseSupport.ps1' @() 'powershell-tests'
    } @('tool-environment')
    $LspBuild = Join-Path $RepoRoot 'Saved/ReleaseTools/luau-lsp-ninja'
    foreach ($Tool in @('LuaLS', 'LuauLsp')) {
        Invoke-Stage "analyzer-$Tool" {
            $BuildScript = Join-Path $PSScriptRoot "Build-$Tool.ps1"
            $ToolArguments = if ($Tool -eq 'LuauLsp') {
                " -Generator Ninja -BuildDir '" + $LspBuild.Replace("'", "''") + "'"
            } else { '' }
            $Command = "& '" + $DevShell.Replace("'", "''") + "' -Arch amd64 -HostArch amd64 -SkipAutomaticLocation; & '" +
                $BuildScript.Replace("'", "''") + "' -Force -NoSubmodules" + $ToolArguments + "; exit `$LASTEXITCODE"
            Run $PowerShellExe @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass', '-EncodedCommand',
                [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($Command))) "build-$Tool"
        } @('tool-environment')
    }
    $env:LUA_LANGUAGE_SERVER = Join-Path $PSScriptRoot 'lua-language-server/bin/lua-language-server.exe'
    $Lsp = Get-ChildItem $LspBuild -Recurse -Filter 'luau-lsp.exe' -File -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($Lsp) { $env:LUAU_LSP = $Lsp.FullName }

    foreach ($B in @('Lua', 'Luau')) {
        $script:EditorStarted = $null
        $Lower = $B.ToLowerInvariant()
        $BackendDir = Join-Path $OutputDir $Lower
        $EditorDir = Join-Path $BackendDir "editor/$Lower"
        $GameDir = Join-Path $BackendDir 'game'
        $DevArchive = Join-Path $BackendDir 'packages/Development'
        $ServerArchive = Join-Path $BackendDir 'packages/Server'
        New-Item -ItemType Directory -Path $EditorDir, $GameDir -Force | Out-Null
        Invoke-Stage "$B-config" {
            Set-ReleaseIniValue $IniPath '[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]' 'LuaBackend' $B
            foreach ($Feature in @('bTimer', 'bEvent', 'bOOP')) {
                Set-ReleaseIniValue $IniPath '[/Script/UnLuaNova.UnLuaNovaSettings]' $Feature 'True'
            }
            Set-ReleaseIniValue $IniPath '[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]' 'bStaticBinding' 'True'
            Set-ReleaseIniValue $IniPath '[/Script/UnLuaNovaEditor.UnLuaNovaEditorSettings]' 'bAutoGenerateStaticBinding' 'False'
        }
        Invoke-Stage "$B-editor-build" {
            $script:BindingState = Backup-ReleaseStaticBindings $ProjectPath (Join-Path $BackendDir 'metadata/static-binding-backup')
            $BindingBackups.Add($BindingState)
            Build-Target ($ProjectName + 'Editor') "$B-editor-build"
        } @('tool-environment', "$B-config")
        Invoke-Stage "$B-static-bindings" {
            Run $Editor @($ProjectPath, '-run=UnLuaNovaStaticBindingGenerator', '-unattended', '-NoSplash', '-stdout') "$B-generate-bindings"
            if (-not (Test-Path -LiteralPath $BindingState.Source -PathType Leaf)) {
                throw "Static binding generator produced no source: $($BindingState.Source)"
            }
            Build-Target ($ProjectName + 'Editor') "$B-rebuild-bindings"
            Build-Target 'ShaderCompileWorker' "$B-shader-worker-build"
        } @("$B-editor-build")
        Invoke-Stage "$B-package-development" { Package-Game 'Development' $DevArchive "$B-package-development" } @("$B-static-bindings")
        Invoke-Stage "$B-server-build" { Build-Target ($ProjectName + 'Server') "$B-server-build" } @("$B-static-bindings")
        Invoke-Stage "$B-package-server" {
            Run $Uat @('BuildCookRun', "-project=$ProjectPath", '-noP4', '-unattended', '-utf8output',
                '-server', '-noclient', '-serverplatform=Win64', '-serverconfig=Development',
                "-target=$($ProjectName)Server", '-build', '-skipbuildeditor', '-cook', '-stage', '-pak', '-iostore', '-package',
                '-archive', "-archivedirectory=$ServerArchive", "-stagingdirectory=$ServerArchive-stage") "$B-package-server"
            $env:UNLUANOVA_TEST_SERVER_EXE = Get-ReleaseGameExe $ServerArchive ($ProjectName + 'Server') 'Development'
        } @("$B-server-build")
        Invoke-Stage "$B-editor-tests" {
            $env:UNLUANOVA_TEST_PEER_EXE = Get-ReleaseGameExe $DevArchive $ProjectName 'Development'
            $env:UNLUANOVA_TEST_NET_ROOT = Join-Path $EditorDir 'net'
            $env:UNLUANOVA_TEST_MANIFEST = Join-Path $EditorDir 'metadata/manifest.json'
            $script:EditorStarted = [DateTime]::UtcNow
            $env:UNLUANOVA_RECORD_FRAMES = '30'
            try {
                Run-Script 'RunTests.ps1' @('-Profile', 'Development', '-ReleaseManaged', '-EnginePath', $EnginePath,
                    '-ProjectPath', $ProjectPath, '-Backend', $B, '-Carrier', 'Editor', '-SkipBuild',
                    '-OutputDir', (Join-Path $BackendDir 'editor'), '-TimeoutMinutes', "$TimeoutMinutes") "$B-editor-tests"
            } finally { Stop-ReleasePeers $env:UNLUANOVA_TEST_NET_ROOT $EditorStarted }
        } @("$B-package-development", "$B-package-server")
        Invoke-Stage "$B-declarations" {
            $script:DeclarationStarted = [DateTime]::UtcNow
            $DeclRoot = Join-Path $RepoRoot 'Plugins/UnLuaNova/IntelliSense'
            $BackupRoot = Join-Path $BackendDir 'previous-declarations'
            New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
            foreach ($Dialect in @('Luau', 'EmmyLua')) {
                $Source = [IO.Path]::GetFullPath((Join-Path $DeclRoot $Dialect))
                $Destination = [IO.Path]::GetFullPath((Join-Path $BackupRoot $Dialect))
                if (-not $Source.StartsWith($RepoRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
                    -not $Destination.StartsWith($OutputDir + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
                    throw 'Declaration backup paths escaped their expected roots.'
                }
                if (Test-Path -LiteralPath $Source) { Move-Item -LiteralPath $Source -Destination $Destination }
            }
            Run $Editor @($ProjectPath, '-run=UnLuaNovaIntelliSense', '-BP=1', '-unattended', '-NoSplash', '-stdout') "$B-declarations"
            $Files = @((Get-Item "$DeclRoot/Luau/UnLuaNova.d.lua")) + @(Get-ChildItem "$DeclRoot/EmmyLua" -Recurse -Filter '*.lua' -File)
            if ($Files.Count -lt 2 -or @($Files | Where-Object { $_.Length -eq 0 }).Count) {
                throw 'Declaration export is empty or contains stale files.'
            }
        } @("$B-static-bindings")
        $Analyzer = if ($B -eq 'Lua') { 'LuaLS' } else { 'LuauLsp' }
        Invoke-Stage "$B-typecheck" {
            if ($B -eq 'Lua') {
                Run-Script 'CheckLuaTypes.ps1' @('-Require', '-LuaLS', $env:LUA_LANGUAGE_SERVER, '-SummaryPath', (Join-Path $EditorDir 'lua-analyze.json')) "$B-typecheck"
            } else {
                Run-Script 'CheckLuauTypes.ps1' @('-Require', '-LuauLsp', $env:LUAU_LSP, '-Path', 'Content/Script/Luau', '-Mode', 'strict', '-SummaryPath', (Join-Path $EditorDir 'luau-analyze.json')) "$B-typecheck"
            }
        } @("$B-declarations", "analyzer-$Analyzer")
        Invoke-Stage "$B-editor-evidence" {
            $Perf = Join-Path $EditorDir 'metadata/perf.json'
            if (-not $EditorStarted -or -not (Test-Path $Perf) -or (Get-Item $Perf).LastWriteTimeUtc -lt $EditorStarted) {
                throw 'Missing fresh benchmark artifact.'
            }
            Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'ValidateRelease.py'), '--run', $EditorDir, '--backend', $B, '--carrier', 'editor') "$B-editor-evidence"
        } @('tool-environment')
        Invoke-Stage "$B-game-tests" {
            $Exe = Get-ReleaseGameExe $DevArchive $ProjectName 'Development'
            $env:UNLUANOVA_TEST_PEER_EXE = $Exe
            $env:UNLUANOVA_TEST_NET_ROOT = Join-Path $GameDir 'net'
            $env:UNLUANOVA_TEST_MANIFEST = Join-Path $GameDir 'metadata/manifest.json'
            $GameStarted = [DateTime]::UtcNow
            try {
                Run $Exe @('/UnLuaNovaTestSuite/Tests/Misc/Empty', '-game', '-unattended', '-NoSplash', '-nosound', '-stdout',
                    '-ExecCmds=UnLuaNovaTest.ExportManifest, Automation RunTests UnLuaNova; Quit',
                    "-ReportExportPath=$GameDir", "-abslog=$GameDir/Game.log") "$B-game-tests"
            } finally {
                Stop-ReleasePeers $env:UNLUANOVA_TEST_NET_ROOT $GameStarted
                $PackagedProject = [IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $Exe) '../..'))
                $ShotDir = Join-Path $PackagedProject 'Saved/Screenshots/Windows'
                $Shots = @(Get-ChildItem -LiteralPath $ShotDir -Filter '*.png' -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.LastWriteTimeUtc -ge $GameStarted })
                if ($Shots.Count) {
                    $ShotDestination = Join-Path $GameDir 'screenshots'
                    New-Item -ItemType Directory -Path $ShotDestination -Force | Out-Null
                    $Shots | Copy-Item -Destination $ShotDestination -Force
                }
            }
        } @("$B-package-development", "$B-package-server")
        Invoke-Stage "$B-game-evidence" {
            Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'ValidateRelease.py'), '--run', $GameDir, '--backend', $B, '--carrier', 'game') "$B-game-evidence"
        } @('tool-environment')
        Invoke-Stage "$B-reports" {
            foreach ($Dir in @($EditorDir, $GameDir)) {
                Run $PythonExe @('-B', (Join-Path $PSScriptRoot 'GenerateTestReport.py'), '--reports', $Dir, '--out', $Dir, '--backend', $B) "$B-report-$(Split-Path $Dir -Leaf)"
            }
        } @('tool-environment')
        Invoke-Stage "$B-package-shipping" {
            Package-Game 'Shipping' (Join-Path $BackendDir 'packages/Shipping') "$B-package-shipping"
        } @("$B-static-bindings")
    }
} catch {
    $Stages.Add([ordered]@{ name = 'pipeline'; status = 'failed'; seconds = 0; detail = $_.Exception.Message })
    Write-Warning $_.Exception.Message
} finally {
    for ($Index = $BindingBackups.Count - 1; $Index -ge 0; $Index--) {
        try { Restore-ReleaseStaticBindings $BindingBackups[$Index] }
        catch { $Stages.Add([ordered]@{ name = 'restore-static-bindings'; status = 'failed'; seconds = 0; detail = $_.Exception.Message }) }
    }
    try {
        if ($ConfigCaptured) {
            if ($IniExisted) { [IO.File]::WriteAllBytes($IniPath, $IniBytes) }
            elseif (Test-Path -LiteralPath $IniPath) { Remove-Item -LiteralPath $IniPath -Force }
        }
        foreach ($Name in $EnvironmentNames) { [Environment]::SetEnvironmentVariable($Name, $SavedEnvironment[$Name], 'Process') }
    } catch {
        $Stages.Add([ordered]@{ name = 'restore'; status = 'failed'; seconds = 0; detail = $_.Exception.Message })
    } finally {
        if ($LockStream) { $LockStream.Dispose(); Remove-Item -LiteralPath $LockPath -Force -ErrorAction SilentlyContinue }
    }
    foreach ($Name in $RequiredStages) {
        if (-not $Statuses.ContainsKey($Name)) {
            $Stages.Add([ordered]@{ name = $Name; status = 'blocked'; seconds = 0; detail = 'Pipeline ended before this stage.' })
        }
    }
    $script:Finished = $true
    Write-ReleaseSummary
}
Write-Host "Release summary: $(Join-Path $OutputDir 'release-summary.md')"
if (@($Stages | Where-Object { $_.status -ne 'passed' }).Count) { exit 1 }
exit 0

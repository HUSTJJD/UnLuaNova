<#
.SYNOPSIS
    Android acceptance precheck and build orchestration for UnLuaNova (Windows).

.DESCRIPTION
    This script PRECHECKS the local Android toolchain and OPTIONALLY orchestrates a
    UE 6.0 BuildCookRun for Android. It does NOT compile Unreal Engine itself, does
    NOT invent a device, and does NOT fabricate a compile, packaging or smoke result.

    Honesty contract:
      * verdict is exactly one of: passed / failed / not-run.
      * A failing precheck ALWAYS yields not-run (never passed) with reasons.
      * -PrecheckOnly / -SkipBuild never run a build or a smoke, so they always
        yield not-run. This lets CI / a device-less host run the precheck for real
        and still report an honest not-run instead of a fake pass.
      * 'passed' is reachable ONLY when precheck, build, artifacts AND a verified
        externally-produced on-device smoke are all green. This script does NOT
        execute an on-device smoke; it can only verify the SHAPE of evidence passed
        via -SmokeEvidence, never its credibility.
      * On Windows, iOS compile / device acceptance is physically impossible
        (needs macOS + Xcode + iOS SDK); it is never attempted by this script.

    Running a full build (no -PrecheckOnly / -SkipBuild) is an OPERATOR action that
    invokes the engine's RunUAT.bat. Nothing here runs UE under -PrecheckOnly.

    Results are written as JSON to Saved/MobileAcceptance/android-<utc>.json by
    default (override with -OutputDir for CI). See Docs/CN/Mobile_Acceptance.md and
    Docs/EN/Mobile_Acceptance.md.

.EXAMPLE
    .\Tools\RunAndroidAcceptance.ps1 -PrecheckOnly
.EXAMPLE
    .\Tools\RunAndroidAcceptance.ps1 -Configuration Development
#>

[CmdletBinding()]
param(
    [string] $EngineRoot,
    [string] $Project,
    [ValidateSet('Development', 'Shipping')]
    [string] $Configuration = 'Development',
    [switch] $SkipBuild,
    [switch] $PrecheckOnly,
    [string] $SmokeEvidence,
    [string] $OutputDir
)

$ErrorActionPreference = 'Stop'

$AllowedVerdicts = @('passed', 'failed', 'not-run')

$RepoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path

function Resolve-Verdict {
    param($Precheck, $Build, $Artifacts, $Smoke)
    # HARD RULE: a failing precheck must NEVER produce 'passed'.
    if (-not $Precheck.ok) { return 'not-run' }   # precheck failure => not-run, never passed
    if ($Build.status -eq 'failed') { return 'failed' }
    if ($Build.status -ne 'passed') { return 'not-run' }
    if (-not $Artifacts.ok) { return 'failed' }
    if ($Smoke.status -ne 'passed') { return 'not-run' }
    return 'passed'
}

$script:Checks = [System.Collections.Generic.List[object]]::new()
$script:PrecheckReasons = [System.Collections.Generic.List[string]]::new()
function Add-Check {
    param([string] $Name, [bool] $Ok, [string] $Detail)
    $script:Checks.Add([ordered]@{ name = $Name; ok = $Ok; detail = $Detail })
    if (-not $Ok) { $script:PrecheckReasons.Add(('{0}: {1}' -f $Name, $Detail)) }
}

$Result = [ordered]@{
    schema             = 'unluanova.mobile-acceptance/1'
    generated_utc      = [DateTime]::UtcNow.ToString('o')
    platform           = 'Android'
    os                 = 'Windows'
    configuration      = $Configuration
    mode               = 'precheck-only'
    engine_root        = $null
    engine_root_source = 'none'
    project            = $null
    precheck           = [ordered]@{
        ok          = $false
        target_arch = 'arm64'
        checks      = @()
        reasons     = @()
        notes       = @(
            'Static / best-effort probes only; they do NOT prove a working UE Android toolchain.',
            'On Windows, iOS compile and device acceptance is physically impossible (needs macOS + Xcode + iOS SDK) and is never attempted.',
            'This repository currently has NO Android / iOS acceptance record; see Engine_Support.md.'
        )
    }
    build              = [ordered]@{ status = 'not-run'; command = $null; exit_code = $null; reason = $null }
    artifacts          = [ordered]@{ status = 'not-run'; package = $null; bytes = 0; ok = $false; has_script_assets = $false; script_asset_count = 0; notes = $null; reason = $null }
    smoke              = [ordered]@{ status = 'not-run'; evidence = $null; device = $null; backend = $null; reason = $null }
    verdict            = 'not-run'
    reason             = ''
    verdict_enum       = $AllowedVerdicts
}

# --- Resolve the project (default: the single .uproject at the repo root) ---
if (-not $Project) {
    $foundProject = Get-ChildItem -LiteralPath $RepoRoot -Filter '*.uproject' -File | Select-Object -First 1
    if ($foundProject) { $Project = $foundProject.FullName }
}
$ProjectPath = $null
if ($Project -and (Test-Path -LiteralPath $Project)) {
    $ProjectPath = (Resolve-Path -LiteralPath $Project).Path
}
$Result.project = $ProjectPath

# --- Resolve the engine root: parameter, env var, or registry GUID discovery ---
$EngineRootResolved = $null
$EngineRootSource = 'none'
if ($EngineRoot) {
    $EngineRootResolved = $EngineRoot
    $EngineRootSource = 'parameter:-EngineRoot'
}
elseif ($env:UNREAL_ENGINE_PATH) {
    $EngineRootResolved = $env:UNREAL_ENGINE_PATH
    $EngineRootSource = 'env:UNREAL_ENGINE_PATH'
}
else {
    # UnrealVersionSelector keeps GUID -> engine path under HKCU. Reading it keeps
    # this script free of any machine-specific absolute path: the mapping lives on
    # the machine (in the registry), not in this file.
    $regKey = 'HKCU:\Software\Epic Games\Unreal Engine\Builds'
    if (Test-Path -LiteralPath $regKey) {
        $props = @((Get-ItemProperty -Path $regKey).PSObject.Properties | Where-Object { $_.Name -notlike 'PS*' })
        $assoc = $null
        if ($ProjectPath) {
            try { $assoc = (Get-Content -LiteralPath $ProjectPath -Raw | ConvertFrom-Json).EngineAssociation } catch { }
        }
        $match = $null
        if ($assoc) { $match = $props | Where-Object { $_.Name -eq $assoc } | Select-Object -First 1 }
        if (-not $match -and $props.Count -gt 0) { $match = $props[0] }
        if ($match) {
            $EngineRootResolved = [string] $match.Value
            $EngineRootSource = ('registry:{0}' -f $match.Name)
        }
    }
}
$Result.engine_root = $EngineRootResolved
$Result.engine_root_source = $EngineRootSource

# =====================================================================
# PRECHECK (always runs; read-only, best-effort, never fabricates a result)
# =====================================================================

# 1. ANDROID_HOME
$ah = $env:ANDROID_HOME
$ahExists = $false
if ($ah) { $ahExists = Test-Path -LiteralPath $ah }
Add-Check 'ANDROID_HOME' $ahExists ('set={0} dir_exists={1} value={2}' -f [bool]($ah), $ahExists, $ah)

# 2. NDKROOT (ANDROID_NDK_ROOT is reported as an alternate, not a hard gate)
$ndk = $env:NDKROOT
$ndkExists = $false
if ($ndk) { $ndkExists = Test-Path -LiteralPath $ndk }
Add-Check 'NDKROOT' $ndkExists ('set={0} dir_exists={1} value={2} alt_ANDROID_NDK_ROOT={3}' -f [bool]($ndk), $ndkExists, $ndk, $env:ANDROID_NDK_ROOT)

# 3. JAVA_HOME
$java = $env:JAVA_HOME
$javaExists = $false
if ($java) { $javaExists = Test-Path -LiteralPath $java }
Add-Check 'JAVA_HOME' $javaExists ('set={0} dir_exists={1} value={2}' -f [bool]($java), $javaExists, $java)

# 4. UE Android platform SDK support (best-effort STATIC probe, not proof)
$indicators = @()
if ($EngineRootResolved) {
    foreach ($rel in @('Engine/Source/Programs/UnrealBuildTool/Platform/Android', 'Engine/Platforms/Android', 'Engine/Source/ThirdParty/Android')) {
        if (Test-Path -LiteralPath (Join-Path $EngineRootResolved $rel)) { $indicators += $rel }
    }
}
$sdkCfg = Join-Path $env:APPDATA 'Unreal Engine/UnrealBuildTool/BuildConfiguration.xml'
$sdkCfgExists = Test-Path -LiteralPath $sdkCfg
$sdkProbeOk = ($indicators.Count -gt 0)
Add-Check 'UE Android platform SDK' $sdkProbeOk ('engine_android_platform_dirs={0} ubt_buildconfig_exists={1}. Static indicators only; Android SDK/NDK configuration for UBT remains UNVERIFIED without running UnrealBuildTool.' -f $indicators.Count, $sdkCfgExists)

# 5. adb devices -- read-only probe; lists attached devices / emulators
$adb = Get-Command adb -ErrorAction SilentlyContinue
$adbOk = $false
$adbDetail = 'adb not found on PATH'
if ($adb) {
    try {
        $adbOut = @(& $adb.Source devices 2>&1)
        $deviceLines = @($adbOut | Where-Object { ($_ -match 'device\s*$') -and ($_ -notmatch 'List of devices') })
        $adbOk = ($deviceLines.Count -gt 0)
        $adbDetail = ('adb devices listed {0} device line(s)' -f $deviceLines.Count)
    }
    catch { $adbDetail = ('adb devices failed: ' + $_.Exception.Message) }
}
Add-Check 'adb devices' $adbOk $adbDetail

# 6. engine Build.version readable
$bvFile = $null
if ($EngineRootResolved) { $bvFile = Join-Path $EngineRootResolved 'Engine/Build/Build.version' }
$bvOk = $false
$bvDetail = 'engine root not resolved'
if ($bvFile -and (Test-Path -LiteralPath $bvFile)) {
    try {
        $bv = Get-Content -LiteralPath $bvFile -Raw | ConvertFrom-Json
        $bvOk = $true
        $bvDetail = ('MajorVersion={0} MinorVersion={1} PatchVersion={2}' -f $bv.MajorVersion, $bv.MinorVersion, $bv.PatchVersion)
    }
    catch { $bvDetail = ('Build.version parse failed: ' + $_.Exception.Message) }
}
elseif ($bvFile) { $bvDetail = 'Build.version not found under resolved engine root' }
Add-Check 'engine Build.version' $bvOk $bvDetail

# 7. target architecture (declarative build target, not an environment fact)
Add-Check 'target_arch arm64' $true 'Declared build target arch arm64 (-architectures=arm64); the actual APK ABI is inspected in the artifacts section.'

$Result.precheck.checks = $script:Checks.ToArray()
$Result.precheck.reasons = $script:PrecheckReasons.ToArray()
$Result.precheck.ok = ($script:PrecheckReasons.Count -eq 0)

# =====================================================================
# BUILD ORCHESTRATION / ARTIFACTS / SMOKE
# =====================================================================
if ($PrecheckOnly) {
    $Result.mode = 'precheck-only'
    $Result.build.reason = 'PrecheckOnly: build not run by request.'
    $Result.smoke.reason = 'PrecheckOnly: on-device smoke not run by request.'
}
elseif ($SkipBuild) {
    $Result.mode = 'skip-build'
    $Result.build.reason = 'SkipBuild: build skipped by request.'
    $Result.smoke.reason = 'SkipBuild: on-device smoke not run.'
}
else {
    $Result.mode = 'full'
    $uat = Join-Path $EngineRootResolved 'Engine/Build/BatchFiles/RunUAT.bat'
    if (-not (Test-Path -LiteralPath $uat)) {
        $Result.build.status = 'failed'
        $Result.build.reason = 'RunUAT.bat not found under the resolved engine root.'
    }
    else {
        # Conventional UE 6.0 BuildCookRun shape for Android. These flags are a
        # STARTING POINT and MUST be adjusted to the local engine / device: cook
        # flavor, texture format and architecture tag names differ between UE
        # point releases. Nothing here runs UE under -PrecheckOnly / -SkipBuild.
        $UatArgs = @(
            'BuildCookRun',
            ("-project=$ProjectPath"),
            '-noP4',
            '-unattended',
            '-platform=Android',
            ("-clientconfig=$Configuration"),
            '-build',
            '-cook',
            '-stage',
            '-package',
            '-pak',
            '-iostore',
            '-architectures=arm64',
            '-cookflavor=ASTC'
        )
        $Result.build.command = ('RunUAT.bat ' + ($UatArgs -join ' '))
        try {
            & $uat @UatArgs 2>&1 | Out-Null
            $code = $LASTEXITCODE
        }
        catch {
            $code = 1
            $Result.build.reason = ('UAT threw: ' + $_.Exception.Message)
        }
        $Result.build.exit_code = $code
        if ($code -eq 0) { $Result.build.status = 'passed' }
        else {
            $Result.build.status = 'failed'
            if (-not $Result.build.reason) { $Result.build.reason = ('UAT exit code ' + $code) }
        }
    }

    if ($Result.build.status -eq 'passed') {
        # --- Artifact check: APK/AAB existence, size, Content/Script/ probe ---
        $Result.artifacts.status = 'checked'
        $searchRoots = @(
            (Join-Path $RepoRoot 'Saved/StagedBuilds'),
            (Join-Path $RepoRoot 'Binaries/Android')
        )
        $pkg = $null
        foreach ($root in $searchRoots) {
            if (Test-Path -LiteralPath $root) {
                $found = Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue |
                    Where-Object { $_.Extension -in '.apk', '.aab' } |
                    Sort-Object LastWriteTime -Descending | Select-Object -First 1
                if ($found) { $pkg = $found; break }
            }
        }
        if ($pkg) {
            $rel = $pkg.FullName
            if ($rel.StartsWith($RepoRoot, [StringComparison]::OrdinalIgnoreCase)) {
                $rel = $rel.Substring($RepoRoot.Length).TrimStart('\', '/')
            }
            $Result.artifacts.package = $rel
            $Result.artifacts.bytes = $pkg.Length
            $Result.artifacts.ok = ($pkg.Length -gt 0)
            try {
                Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction SilentlyContinue
                $zip = [System.IO.Compression.ZipFile]::OpenRead($pkg.FullName)
                try {
                    $names = @($zip.Entries | ForEach-Object { $_.FullName })
                    $scriptNames = @($names | Where-Object { $_ -match '(?i)Content/Script/' })
                    $Result.artifacts.has_script_assets = ($scriptNames.Count -gt 0)
                    $Result.artifacts.script_asset_count = $scriptNames.Count
                }
                finally { $zip.Dispose() }
            }
            catch {
                $Result.artifacts.notes = 'Could not open the package as a zip to probe Content/Script/ entries.'
            }
            if (-not $Result.artifacts.has_script_assets) {
                $Result.artifacts.notes = 'Best-effort probe only: cooked script assets may live inside .pak/.utoc (IoStore), so a literal Content/Script/ zip entry is not guaranteed. A real cooked smoke needs a device.'
            }
        }
        else {
            $Result.artifacts.ok = $false
            $Result.artifacts.reason = 'No .apk/.aab found under Saved/StagedBuilds or Binaries/Android.'
        }

        # --- Smoke: this script does NOT execute an on-device smoke. ---
        $Result.smoke.status = 'not-run'
        $Result.smoke.reason = 'This script does not execute an on-device smoke. Supply -SmokeEvidence produced by a real cooked on-device run (shape-verified only).'
        if ($SmokeEvidence -and (Test-Path -LiteralPath $SmokeEvidence)) {
            try {
                $ev = Get-Content -LiteralPath $SmokeEvidence -Raw | ConvertFrom-Json
                $shapeOk = ($ev.status -eq 'passed') -and $ev.device -and $ev.backend -and ($ev.cooked_smoke -eq $true) -and $ev.recorded_utc
                if ($shapeOk) {
                    $Result.smoke.status = 'passed'
                    $Result.smoke.evidence = 'external evidence (shape-verified only; credibility NOT proven by this script)'
                    $Result.smoke.device = [string] $ev.device
                    $Result.smoke.backend = [string] $ev.backend
                }
                else {
                    $Result.smoke.reason = 'External smoke evidence failed shape verification (need status=passed, device, backend, cooked_smoke=true, recorded_utc).'
                }
            }
            catch { $Result.smoke.reason = ('Could not parse -SmokeEvidence: ' + $_.Exception.Message) }
        }
    }
}

# =====================================================================
# VERDICT (single decision point + defensive invariants)
# =====================================================================
$Result.verdict = Resolve-Verdict -Precheck $Result.precheck -Build $Result.build -Artifacts $Result.artifacts -Smoke $Result.smoke
if ($Result.verdict -notin $AllowedVerdicts) { $Result.verdict = 'not-run' }
# Defensive invariant: a failing precheck must never produce passed.
if ((-not $Result.precheck.ok) -and ($Result.verdict -eq 'passed')) { $Result.verdict = 'not-run' }

$reasonParts = [System.Collections.Generic.List[string]]::new()
if (-not $Result.precheck.ok) { $reasonParts.Add('precheck failed: ' + ($Result.precheck.reasons -join '; ')) }
if ($PrecheckOnly) { $reasonParts.Add('PrecheckOnly: no build and no on-device smoke were performed (acceptance not run).') }
elseif ($SkipBuild) { $reasonParts.Add('SkipBuild: build was skipped by request (acceptance not run).') }
if ($Result.build.status -eq 'failed') { $reasonParts.Add('build failed: ' + $Result.build.reason) }
if (($Result.build.status -eq 'passed') -and ($Result.smoke.status -eq 'not-run')) { $reasonParts.Add('on-device smoke not run; acceptance incomplete.') }
if ($reasonParts.Count -eq 0) { $reasonParts.Add('all sections green with shape-verified external smoke evidence.') }
$Result.reason = $reasonParts -join '; '

# =====================================================================
# WRITE JSON RESULT
# =====================================================================
if (-not $OutputDir) { $OutputDir = Join-Path $RepoRoot 'Saved/MobileAcceptance' }
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$stamp = [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssZ')
$outFile = Join-Path $OutputDir ('android-{0}.json' -f $stamp)
$Result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $outFile -Encoding UTF8

Write-Host ('verdict={0}' -f $Result.verdict)
Write-Host ('reason={0}' -f $Result.reason)
Write-Host ('json={0}' -f $outFile)

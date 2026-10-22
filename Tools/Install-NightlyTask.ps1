<#
.SYNOPSIS
    Register (or remove) the nightly build of the UnLuaNova test code.

.DESCRIPTION
    The test code is compiled code, so it can stop compiling - a renamed engine field, a
    helper that moved, a spec that calls something that no longer exists. Nobody notices
    until somebody runs a full suite, which takes ~20 minutes and therefore does not
    happen every day.

    This registers a Windows scheduled task that compiles one backend every night and
    nothing else: no editor, no test run, no report. RunTests.ps1 exits non-zero when the
    build fails, so the task's exit code (and its log) is the whole answer.

    If you have a self-hosted Windows runner instead, you do not need this: wire the same
    command into a GitHub Actions `schedule` and let Actions report the failure.

.PARAMETER Backend
    Which backend to compile. Default Luau - the default backend, and one build is enough
    to catch "the test code does not compile".

.PARAMETER At
    Local time to run, default 02:00.

.PARAMETER Uninstall
    Remove the scheduled task instead of registering it.

.EXAMPLE
    .\Tools\Install-NightlyTask.ps1

.EXAMPLE
    .\Tools\Install-NightlyTask.ps1 -Backend Both -At 03:30

.EXAMPLE
    .\Tools\Install-NightlyTask.ps1 -Uninstall
#>

[CmdletBinding()]
param(
    [ValidateSet("Lua", "Luau", "Both")]
    [string] $Backend = "Luau",
    [string] $At = "02:00",
    [switch] $Uninstall
)

$ErrorActionPreference = "Stop"

$RepoRoot = Split-Path -Parent $PSScriptRoot
$TaskName = "UnLuaNova-NightlyBuild"
$RunTests = Join-Path $RepoRoot "Tools/RunTests.ps1"
$LogDir = Join-Path $RepoRoot "Saved"
$LogPath = Join-Path $LogDir "nightly-build.log"

if (-not (Test-Path $RunTests)) {
    throw "'$RunTests' not found - is this script still under Tools/?"
}

if ($Uninstall) {
    Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false -ErrorAction SilentlyContinue
    Write-Host "Removed scheduled task '$TaskName' (if it existed)."
    exit 0
}

if (-not (Test-Path $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
}

# Quoted individually: the repo path can contain spaces, and the task scheduler hands the
# argument string to pwsh verbatim.
$Command = "& '$RunTests' -Backend $Backend -BuildOnly -NoReport *>&1 | Out-File -FilePath '$LogPath' -Encoding utf8"
$Argument = "-NoProfile -ExecutionPolicy Bypass -Command `"$Command`""

# Prefer PowerShell 7 when it exists, but fall back to Windows PowerShell 5.1: a machine
# with only 5.1 installed would otherwise register a task that silently never runs
# (the scheduler cannot resolve 'pwsh.exe'), which is worse than running the older host.
$Shell = (Get-Command "pwsh.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).Source
if (-not $Shell) {
    $Shell = (Get-Command "powershell.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).Source
}
if (-not $Shell) {
    throw "Neither pwsh.exe nor powershell.exe was found on PATH."
}
Write-Host "  shell   : $Shell"

$Action = New-ScheduledTaskAction -Execute $Shell -Argument $Argument -WorkingDirectory $RepoRoot
$Trigger = New-ScheduledTaskTrigger -Daily -At $At
# StartWhenAvailable: a machine that was asleep at 02:00 still gets its build, which is
# the entire point - a skipped night is a day of test code nobody compiled.
$Settings = New-ScheduledTaskSettingsSet -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Hours 2)

Register-ScheduledTask -TaskName $TaskName -Action $Action -Trigger $Trigger -Settings $Settings `
    -Description "Compile the UnLuaNova test suite once a day (build only, no test run)." -Force | Out-Null

$Registered = Get-ScheduledTask -TaskName $TaskName
Write-Host "Registered '$TaskName':"
Write-Host "  when    : daily at $At (run it now with: Start-ScheduledTask -TaskName '$TaskName')"
Write-Host "  command : $Command"
Write-Host "  log     : $LogPath"

$Info = $Registered | Get-ScheduledTaskInfo
if ($Info.NextRunTime) { Write-Host "  next    : $($Info.NextRunTime)" }

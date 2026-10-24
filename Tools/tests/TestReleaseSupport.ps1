$ErrorActionPreference = 'Stop'
$Tools = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent $Tools
$PowerShellExe = (Get-Process -Id $PID).Path
. (Join-Path $Tools 'ReleaseSupport.ps1')
$Temporary = Join-Path ([IO.Path]::GetTempPath()) ('UnLuaNova-release-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $Temporary | Out-Null
function Assert([bool] $Condition, [string] $Message) { if (-not $Condition) { throw $Message } }
try {
    foreach ($File in @(Get-ChildItem -LiteralPath $Tools -Filter '*.ps1' -File) + @(Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.ps1' -File)) {
        $Tokens = $null
        $ParseErrors = $null
        [Management.Automation.Language.Parser]::ParseFile($File.FullName, [ref]$Tokens, [ref]$ParseErrors) | Out-Null
        Assert (-not $ParseErrors) "Invalid PowerShell syntax: $($File.FullName)"
    }
    $Ini = Join-Path $Temporary 'config.ini'
    [IO.File]::WriteAllText($Ini, "[Other]`r`nValue=keep`r`n[Target]`r`nKey=old`r`nKey=duplicate`r`n")
    Set-ReleaseIniValue $Ini '[Target]' 'Key' 'new'
    Set-ReleaseIniValue $Ini '[New]' 'Enabled' 'True'
    $Text = [IO.File]::ReadAllText($Ini)
    Assert ($Text.Contains('Value=keep') -and $Text.Contains('Enabled=True')) 'INI lost unrelated content'
    Assert (([regex]::Matches($Text, 'Key=')).Count -eq 1 -and $Text.Contains('Key=new')) 'INI key not replaced exactly once'

    # Real child process, no compiler or UE. Verify quoting and nonzero exit propagation.
    $Arguments = @('space path\', 'embedded"quote', '', '-value=two words', 'literal $() and `backticks`')
    $Probe = Join-Path $PSScriptRoot 'fixtures/argv_probe.py'
    $Log = Join-Path $Temporary 'argv.log'
    Invoke-ReleaseProcess -File (Get-Command python).Source -Arguments (@($Probe) + $Arguments) -Log $Log -WorkingDirectory $Temporary
    $Actual = Get-Content $Log -Raw | ConvertFrom-Json
    Assert ($Actual.Count -eq $Arguments.Count) 'Argument count changed'
    for ($Index = 0; $Index -lt $Arguments.Count; $Index++) {
        Assert ($Actual[$Index] -ceq $Arguments[$Index]) "Argument $Index changed"
    }
    $Failed = $false
    try {
        Invoke-ReleaseProcess -File $PowerShellExe -Arguments @('-NoProfile', '-NonInteractive', '-Command', 'exit 7') `
            -Log (Join-Path $Temporary 'failure.log') -WorkingDirectory $Temporary
    } catch { $Failed = $_.Exception.Message -like 'Exit 7:*' }
    Assert $Failed 'Nonzero process exit did not fail the gate'
    $Failed = $false
    try {
        Invoke-ReleaseProcess -File $PowerShellExe -Arguments @('-NoProfile', '-NonInteractive', '-Command', 'Start-Sleep -Seconds 15') `
            -Log (Join-Path $Temporary 'timeout.log') -WorkingDirectory $Temporary -Minutes 0.01
    } catch { $Failed = $_.Exception.Message -like 'Timeout*' }
    Assert $Failed 'Process timeout did not fail the gate'

    $Before = Get-FileHash (Join-Path $RepoRoot 'Config/DefaultUnLuaNova.ini')
    Invoke-ReleaseProcess -File $PowerShellExe -Arguments @('-NoProfile', '-NonInteractive', '-File',
        (Join-Path $Tools 'RunTests.ps1'), '-DryRun') -Log (Join-Path $Temporary 'dry-run.log') -WorkingDirectory $RepoRoot
    Assert ((Get-FileHash $Before.Path).Hash -eq $Before.Hash) 'DryRun modified project config'
    $Failed = $false
    try {
        Invoke-ReleaseProcess -File $PowerShellExe -Arguments @('-NoProfile', '-NonInteractive', '-File',
            (Join-Path $Tools 'RunTests.ps1'), '-DryRun', '-SkipBuild') -Log (Join-Path $Temporary 'partial.log') -WorkingDirectory $RepoRoot
    } catch { $Failed = $true }
    Assert $Failed 'Release accepted reduced coverage'
    Write-Host 'Release support tests passed.'
} finally {
    $Resolved = [IO.Path]::GetFullPath($Temporary)
    $ExpectedParent = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if ($Resolved.StartsWith($ExpectedParent, [StringComparison]::OrdinalIgnoreCase) -and
        [IO.Path]::GetFileName($Resolved).StartsWith('UnLuaNova-release-test-')) {
        Remove-Item -LiteralPath $Resolved -Recurse -Force
    }
}

# Shared by the release runner and its process/transaction regression tests.
function ConvertTo-NativeArgument([string] $Value) {
    # UE reads its raw command line, so quote the value in -key=value, not the key.
    if ($Value -match '^(-[^=\s]+)=(.*)$') {
        $Key = $Matches[1]
        $Tail = $Matches[2]
        return $Key + '=' + (ConvertTo-NativeArgument $Tail)
    }
    if ($Value.Length -gt 0 -and $Value -notmatch '[\s"]') { return $Value }
    # Windows CommandLineToArgvW rules, including a trailing backslash in a quoted path.
    return '"' + [regex]::Replace([regex]::Replace($Value, '(\\*)"', '$1$1\"'), '(\\+)$', '$1$1') + '"'
}

# A Ctrl-C of the release pipeline does not kill the Build.bat/UBT/cl tree it launched.
# Those orphans keep UBT's mutex, so the next run sits in "Build.bat is already running,
# waiting for existing script to terminate..." for most of its 60-minute stage timeout.
# Every external process is registered here and killed by Stop-ReleaseChildProcesses from
# the pipeline's finally block, which PowerShell does run on cancellation.
$script:ReleaseChildProcesses = @{}

function Stop-ReleaseChildProcess([int] $ProcessId) {
    if ($ProcessId -le 0) { return }
    & taskkill.exe /PID $ProcessId /T /F 2>&1 | Out-Null
    for ($Attempt = 0; $Attempt -lt 20; $Attempt++) {
        if (-not (Get-Process -Id $ProcessId -ErrorAction SilentlyContinue)) { return }
        Start-Sleep -Milliseconds 250
    }
}

function Stop-ReleaseChildProcesses {
    foreach ($ProcessId in @($script:ReleaseChildProcesses.Keys)) {
        Stop-ReleaseChildProcess ([int] $ProcessId)
    }
    $script:ReleaseChildProcesses.Clear()
}

function Invoke-ReleaseProcess {
    param([string] $File, [string[]] $Arguments, [string] $Log,
        [double] $Minutes = 60, [string] $WorkingDirectory = $script:RepoRoot)
    New-Item -ItemType Directory -Path (Split-Path -Parent $Log) -Force | Out-Null
    if ([IO.Path]::GetExtension($File) -in '.bat', '.cmd') {
        $Literals = @($File) + @($Arguments) | ForEach-Object { "'" + $_.Replace("'", "''") + "'" }
        $Command = '& ' + ($Literals -join ' ') + '; exit $LASTEXITCODE'
        $Arguments = @('-NoProfile', '-NonInteractive', '-EncodedCommand',
            [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($Command)))
        $File = $script:PowerShellExe
    }
    $Line = (@($Arguments) | ForEach-Object { ConvertTo-NativeArgument $_ }) -join ' '
    [IO.File]::WriteAllText("$Log.command.txt", "$File $Line", [Text.UTF8Encoding]::new($false))
    $Process = Start-Process -FilePath $File -ArgumentList $Line -WorkingDirectory $WorkingDirectory `
        -PassThru -NoNewWindow -RedirectStandardOutput $Log -RedirectStandardError "$Log.stderr"
    $null = $Process.Handle
    $script:ReleaseChildProcesses[$Process.Id] = $Process.StartTime
    $Watch = [Diagnostics.Stopwatch]::StartNew()
    try {
        $NextProgress = 30
        while (-not $Process.WaitForExit(1000)) {
            if ($Watch.Elapsed.TotalSeconds -ge $NextProgress) {
                Write-Host ("  running {0:N0}s: {1}" -f $Watch.Elapsed.TotalSeconds, $Log)
                $NextProgress += 30
            }
            if ($Watch.Elapsed.TotalMinutes -ge $Minutes) {
                throw "Timeout after $Minutes minutes: $Log"
            }
        }
        $Process.WaitForExit()
        if ($Process.ExitCode -ne 0) {
            # The last 15 lines of a UBT log end at "Result: Failed"; the compiler error is
            # hundreds of lines earlier, so printing only the tail hides the actual cause.
            $Diagnostics = @(Select-String -Path $Log -Pattern 'error C\d+|fatal error C\d+|error LNK\d+|error:|Result: Failed|CompilationResultException' -ErrorAction SilentlyContinue |
                Select-Object -Last 20)
            if ($Diagnostics.Count) {
                Write-Host "  diagnostics from $Log :" -ForegroundColor Yellow
                $Diagnostics | ForEach-Object { Write-Host ("    {0}" -f $_.Line) }
            }
            Get-Content $Log -Tail 15 | Out-Host
            Get-Content "$Log.stderr" -Tail 15 | Out-Host
            throw "Exit $($Process.ExitCode): $File (see $Log)"
        }
    } finally {
        if (-not $Process.HasExited) { Stop-ReleaseChildProcess $Process.Id }
        $script:ReleaseChildProcesses.Remove($Process.Id)
        $Process.Dispose()
    }
}

function Stop-ReleasePeers([string] $Root, [datetime] $Since) {
    if (-not (Test-Path -LiteralPath $Root)) { return }
    foreach ($File in Get-ChildItem -LiteralPath $Root -Recurse -Filter topology.json -File) {
        $Topology = Get-Content -LiteralPath $File.FullName -Raw -Encoding UTF8 | ConvertFrom-Json
        foreach ($Peer in $Topology.peers) {
            if ([int]$Peer.pid -le 0) { continue }
            $Child = Get-Process -Id ([int]$Peer.pid) -ErrorAction SilentlyContinue
            $PeerExe = if ($Peer.exe) { $Peer.exe } else { $Topology.exe }
            if ($Child -and $Child.Path -and $PeerExe -and
                [IO.Path]::GetFullPath($Child.Path) -eq [IO.Path]::GetFullPath($PeerExe) -and
                $Child.StartTime.ToUniversalTime() -ge $Since.ToUniversalTime()) {
                Stop-Process -Id $Child.Id -Force -ErrorAction Stop
            }
        }
    }
}

function Set-ReleaseIniValue([string] $Path, [string] $Section, [string] $Key, [string] $Value) {
    $Lines = if (Test-Path -LiteralPath $Path) { [IO.File]::ReadAllLines($Path) } else { @() }
    $Result = [Collections.Generic.List[string]]::new()
    $Inside = $false
    $Written = $false
    foreach ($Line in $Lines) {
        if ($Line.Trim().StartsWith('[')) {
            if ($Inside -and -not $Written) { $Result.Add("$Key=$Value"); $Written = $true }
            $Inside = $Line.Trim() -ieq $Section
        }
        if ($Inside -and $Line -match ('^\s*' + [regex]::Escape($Key) + '\s*=')) {
            if (-not $Written) { $Result.Add("$Key=$Value"); $Written = $true }
        } else { $Result.Add($Line) }
    }
    if (-not $Written) {
        if (-not $Inside) { $Result.Add($Section) }
        $Result.Add("$Key=$Value")
    }
    [IO.File]::WriteAllLines($Path, $Result, [Text.UTF8Encoding]::new($false))
}

function Find-ReleaseEngine([string] $Hint, [string] $Project) {
    if ($Hint) { return [IO.Path]::GetFullPath($Hint) }
    if ($env:UNREAL_ENGINE_PATH) { return $env:UNREAL_ENGINE_PATH }
    $Association = (Get-Content -LiteralPath $Project -Raw | ConvertFrom-Json).EngineAssociation
    $Key = 'HKCU:\Software\Epic Games\Unreal Engine\Builds'
    if ($Association -and (Test-Path $Key)) {
        $Property = (Get-ItemProperty $Key).PSObject.Properties[$Association]
        if ($Property) { return $Property.Value }
    }
    if ($Association) {
        $InstalledKey = "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$Association"
        if (Test-Path $InstalledKey) { return (Get-ItemProperty $InstalledKey).InstalledDirectory }
    }
    throw 'Cannot resolve the project engine. Associate the .uproject with UE or set UNREAL_ENGINE_PATH.'
}

function Get-ReleaseGameExe([string] $Archive, [string] $ProjectName, [string] $Configuration) {
    $Name = if ($Configuration -eq 'Shipping') { "$ProjectName-Win64-Shipping.exe" } else { "$ProjectName.exe" }
    $Matches = @(Get-ChildItem -LiteralPath $Archive -Recurse -File -Filter $Name |
        Where-Object { $_.Directory.Name -eq 'Win64' -and $_.Directory.Parent.Name -eq 'Binaries' })
    if ($Matches.Count -ne 1) { throw "Expected one packaged $Name under Binaries/Win64, found $($Matches.Count): $Archive" }
    return $Matches[0].FullName
}

function Resolve-ReleaseGameExe([string] $Archive, [string] $ProjectName, [string] $Configuration) {
    # Non-throwing twin of Get-ReleaseGameExe: asking "is this already packaged?" must not
    # fail the run, it can only answer no.
    try { return (Get-ReleaseGameExe $Archive $ProjectName $Configuration) } catch { return $null }
}

function Get-ReleaseBuiltBackend([string] $PluginRoot, [string] $BinariesSubFolder = '') {
    # Which backend the tree currently holds. The definitions header UBT wrote is the only
    # authoritative answer: the ini is an intention, and the run directory is a label. The
    # header is what a build that actually finished compiled in.
    #
    # ModuleRules.BinariesSubFolder also redirects the intermediate directory, so a
    # backend-isolated build writes this header under .../Development/Backends/<Backend>/.
    $Rel = 'Intermediate/Build/Win64/x64/UnrealEditor/Development'
    if ($BinariesSubFolder) { $Rel = "$Rel/$BinariesSubFolder" }
    $Header = Join-Path $PluginRoot "$Rel/UnLuaNova/Definitions.UnLuaNova.h"
    if (-not (Test-Path -LiteralPath $Header)) { return $null }
    $Text = [IO.File]::ReadAllText($Header)
    if ($Text -match 'UNLUANOVA_LUA_BACKEND\s+1') { return 'Luau' }
    if ($Text -match 'UNLUANOVA_LUA_BACKEND\s+0') { return 'Lua' }
    return $null
}

function Get-ReleaseStaticBindingSource([string] $Project, [string] $Backend) {
    if ($Backend -notin @('Lua', 'Luau')) { throw "Unknown static binding backend: $Backend" }
    $ProjectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $Project)).TrimEnd('\', '/')
    $ProjectName = [IO.Path]::GetFileNameWithoutExtension($Project)
    # Mirrors UnLuaNova::GetDefaultStaticBindingOutputDir(): the generator derives the backend
    # from the compiled editor's UNLUANOVA_LUA_BACKEND and writes into its own
    # Generated/Backends/<Backend>/ directory. Two backends can therefore keep their
    # generated translation units on disk at the same time and never overwrite each other.
    $Plugin = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "Plugins/UnLuaNova/Source/UnLuaNova/Generated/Backends/$Backend/UnLuaNovaStaticBinding_Generated.cpp"))
    $ProjectOwned = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "Source/$ProjectName/Generated/Backends/$Backend/UnLuaNovaStaticBinding_Generated.cpp"))
    if (Test-Path -LiteralPath (Split-Path -Parent $Plugin) -PathType Container) { return $Plugin }
    # Legacy trees that still carry a project-local file keep working; the generator falls
    # back to the same directory when it cannot find the plugin.
    if (Test-Path -LiteralPath (Split-Path -Parent $ProjectOwned) -PathType Container) { return $ProjectOwned }
    return $Plugin
}

# The generator's ownership marker is the first line of every file it writes.
$script:ReleaseBindingHeader = '// Generated by UnLuaNovaStaticBindingGenerator. Do not edit.'

function Test-ReleaseBindingGenerated([string] $Path, [string] $Backend) {
    if ($Backend -notin @('Lua', 'Luau')) { throw "Unknown static binding backend: $Backend" }
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $false }
    $Head = @(Get-Content -LiteralPath $Path -TotalCount 12)
    if (-not $Head.Count -or $Head[0] -cne $script:ReleaseBindingHeader) { return $false }
    # The file also states which backend produced it and guards its whole body with that
    # backend's compile-time macro. A file copied from the other backend's directory must
    # not be accepted as this backend's generated output.
    if (@($Head | Where-Object { $_ -ceq "// backend: $Backend" }).Count -ne 1) { return $false }
    $Value = if ($Backend -eq 'Lua') { 0 } else { 1 }
    return ($Head -contains "#if defined(UNLUANOVA_LUA_BACKEND) && UNLUANOVA_LUA_BACKEND == $Value")
}

# A packaged/cooked game has no Engine/Content/Automation HTML template. UE logs that as
# LogAutomationController: Error but still runs every test and exports the JSON this
# pipeline actually consumes, so a zero exit code alone cannot hide *other* UE errors.
$script:ReleaseAllowedAutomationErrors = @(
    'Failed to load test report html template - No report will be generated.'
)
function Assert-ReleaseAutomationLog([string] $Log) {
    if (-not (Test-Path -LiteralPath $Log -PathType Leaf)) { return }
    $Unexpected = @(Select-String -LiteralPath $Log -Pattern 'LogAutomationController: Error:' -ErrorAction SilentlyContinue |
        Where-Object {
            $Line = $_.Line
            -not @($script:ReleaseAllowedAutomationErrors | Where-Object { $Line.Contains($_) }).Count
        })
    if ($Unexpected.Count) {
        $Sample = @($Unexpected | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() }) -join ' | '
        throw "Unexpected UE automation error(s) in $Log ($($Unexpected.Count) total): $Sample"
    }
}

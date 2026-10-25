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
            Get-Content $Log -Tail 15 | Out-Host
            Get-Content "$Log.stderr" -Tail 15 | Out-Host
            throw "Exit $($Process.ExitCode): $File (see $Log)"
        }
    } finally {
        if (-not $Process.HasExited) { & taskkill.exe /PID $Process.Id /T /F | Out-Null }
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

function Backup-ReleaseStaticBindings([string] $Project, [string] $BackupDirectory) {
    $ProjectRoot = [IO.Path]::GetFullPath((Split-Path -Parent $Project)).TrimEnd('\', '/')
    $ProjectName = [IO.Path]::GetFileNameWithoutExtension($Project)
    $Source = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "Source/$ProjectName/Generated/UnLuaNovaStaticBinding_Generated.cpp"))
    if (-not $Source.StartsWith($ProjectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Generated binding path escaped the project: $Source"
    }
    # Refuse redirected source paths and files without the generator's ownership marker.
    $Cursor = $Source
    while ($Cursor -ne $ProjectRoot) {
        if ((Test-Path -LiteralPath $Cursor) -and
            ((Get-Item -LiteralPath $Cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "Generated binding path contains a reparse point: $Cursor"
        }
        $Cursor = Split-Path -Parent $Cursor
    }
    $State = [pscustomobject]@{ Source = $Source; Backup = $null }
    if (-not (Test-Path -LiteralPath $Source)) { return $State }
    if (-not (Test-Path -LiteralPath $Source -PathType Leaf) -or
        (Get-Content -LiteralPath $Source -TotalCount 1) -cne '// Generated by UnLuaNovaStaticBindingGenerator. Do not edit.') {
        throw "Refusing to archive an unrecognized static binding file: $Source"
    }
    $Backup = [IO.Path]::GetFullPath((Join-Path $BackupDirectory 'UnLuaNovaStaticBinding_Generated.cpp.bak'))
    if (Test-Path -LiteralPath $Backup) { throw "Static binding backup already exists: $Backup" }
    New-Item -ItemType Directory -Path (Split-Path -Parent $Backup) -Force | Out-Null
    Move-Item -LiteralPath $Source -Destination $Backup -ErrorAction Stop
    $State.Backup = $Backup
    Write-Host "Archived static bindings before generator bootstrap: $Backup"
    return $State
}

function Restore-ReleaseStaticBindings($State) {
    if ($State.Backup -and -not (Test-Path -LiteralPath $State.Source)) {
        # Copy preserves the run's evidence. Never replace newly generated or user-written output.
        Copy-Item -LiteralPath $State.Backup -Destination $State.Source -ErrorAction Stop
        Write-Host "Restored static bindings after incomplete generation: $($State.Source)"
    }
}

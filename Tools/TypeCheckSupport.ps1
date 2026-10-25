function Get-TypeCheckExitCode([int] $AnalyzerExit, [int] $Errors, [int] $Warnings, [bool] $HasResult = $true) {
    if (-not $HasResult -or $Errors -gt 0) { return 1 }
    # Both pinned analyzers use 1 for diagnostics, including warnings.
    if ($AnalyzerExit -eq 1 -and $Warnings -gt 0) { return 0 }
    return $AnalyzerExit
}

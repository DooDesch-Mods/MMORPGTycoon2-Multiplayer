param([Parameter(Mandatory=$true)][int]$RootPid,
      [Parameter(Mandatory=$true)][string]$Workspace)
$ErrorActionPreference = 'Stop'
$targetBuild = Join-Path ([System.IO.Path]::GetFullPath($Workspace)) 'build'
$processes = @(Get-CimInstance Win32_Process)
$root = @($processes | Where-Object { $_.ProcessId -eq $RootPid })
if ($root.Count -ne 1 -or $root[0].Name -ne 'cmake.exe' -or
    -not $root[0].CommandLine.Contains('--build') -or
    -not $root[0].CommandLine.Contains($targetBuild)) {
    throw "Build owner mismatch root=$RootPid required_target=$targetBuild"
}
$owned = @($root[0])
$seen = [System.Collections.Generic.HashSet[int]]::new()
[void]$seen.Add($RootPid)
do {
    $added = $false
    foreach ($process in $processes) {
        if ($seen.Contains([int]$process.ParentProcessId) -and $seen.Add([int]$process.ProcessId)) {
            $owned += $process
            $added = $true
        }
    }
} while ($added)
foreach ($process in $owned) {
    $current = @(Get-CimInstance Win32_Process -Filter "ProcessId=$($process.ProcessId)")
    if ($current.Count -eq 0) {
        Write-Output "info: build child already exited pid=$($process.ProcessId)"
        continue
    }
    if ($current[0].CreationDate -ne $process.CreationDate -or $current[0].Name -ne $process.Name) {
        throw "Build child identity changed pid=$($process.ProcessId)"
    }
    Stop-Process -Id $process.ProcessId -ErrorAction Stop
    Write-Output "info: stopped owned build process pid=$($process.ProcessId) name=$($process.Name)"
}

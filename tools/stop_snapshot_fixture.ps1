param([Parameter(Mandatory=$true)][string]$EvidenceId)
$ErrorActionPreference = 'Stop'
$workspace = Split-Path -Parent $PSScriptRoot
$fixtureRoot = [IO.Path]::GetFullPath((Join-Path $workspace 'test/snapshot_stream_evidence'))
$fixtureDir = [IO.Path]::GetFullPath((Join-Path $fixtureRoot ([Guid]::Parse($EvidenceId).ToString())))
if (-not $fixtureDir.StartsWith($fixtureRoot + [IO.Path]::DirectorySeparatorChar)) { throw 'Fixture path outside evidence root' }
$executable = Join-Path $fixtureDir 'snapshot_stream_fixture.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) { throw "Owned fixture executable missing: $executable" }
$ownedProcesses = @(Get-Process | Where-Object { $_.ProcessName -eq 'snapshot_stream_fixture' -and $_.Path -eq $executable })
foreach ($owned in $ownedProcesses) {
    Write-Output "warn: terminate owned failed fixture pid=$($owned.Id) executable=$executable"
    Stop-Process -Id $owned.Id -ErrorAction Stop
}
if ($ownedProcesses.Count -eq 0) { Write-Output "info: no live fixture for evidence=$EvidenceId" }

$ErrorActionPreference = 'Stop'
Get-Process -Name MSBuild,cmake,cl,link,rc -ErrorAction SilentlyContinue | Select-Object Id, ProcessName, CPU, StartTime, Path | ConvertTo-Json -Depth 3

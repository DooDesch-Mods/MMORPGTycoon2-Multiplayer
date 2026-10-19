param([switch]$Stop)
$ErrorActionPreference = 'Stop'
$allProcesses = @(Get-CimInstance Win32_Process)
$parents = @($allProcesses | Where-Object {
    $_.Name -eq 'python.exe' -and
    ($_.CommandLine -match 'test[/\\]mcp[/\\]test_stdio\.py' -or
     $_.CommandLine -match '\-m unittest discover \-s test[/\\]mcp \-v')
})
$targets = @($parents)
foreach ($parent in $parents) {
    $targets += @($allProcesses | Where-Object {
        $_.ParentProcessId -eq $parent.ProcessId -and $_.Name -eq 'python.exe' -and
        $_.CommandLine -match 'tools[/\\]mcp[/\\]server\.py' -and
        $_.CommandLine -match '\-\-instances\-root'
    })
}
foreach ($target in $targets) {
    if ($Stop) {
        $current = Get-CimInstance Win32_Process -Filter "ProcessId=$($target.ProcessId)"
        if ($null -eq $current) {
            Write-Output "info: Probe pid=$($target.ProcessId) already exited"
            continue
        }
        if ($current.CreationDate -ne $target.CreationDate -or $current.CommandLine -ne $target.CommandLine) {
            throw "error: Probe pid=$($target.ProcessId) identity changed; stop refused"
        }
        Stop-Process -Id $target.ProcessId -ErrorAction Stop
        Write-Output "info: Stopped owned stalled MCP probe pid=$($target.ProcessId)"
    } else {
        [pscustomobject]@{pid=$target.ProcessId; command=$target.CommandLine; creation=$target.CreationDate} | ConvertTo-Json -Compress
    }
}

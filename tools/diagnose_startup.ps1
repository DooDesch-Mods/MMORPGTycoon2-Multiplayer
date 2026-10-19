param(
    [string]$GameDirectory = $PSScriptRoot,
    [string]$ReportPath = (Join-Path $env:TEMP ('MT2MP-startup-' + [DateTime]::UtcNow.ToString('yyyyMMddTHHmmssfffZ') + '.json'))
)
$ErrorActionPreference = 'Stop'
$game = (Resolve-Path -LiteralPath $GameDirectory).Path
$report = [ordered]@{schema=1;utc=[DateTime]::UtcNow.ToString('o');game=$game;diagnostic_64bit=[Environment]::Is64BitProcess;files=@();processes=@();profiles=@();runtime=@();warnings=@()}
$expected = @{
    'MT2.exe'='bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6'
    'WSOCK32.dll'='9245db14f89db7b0f3864549a9bf377955b321856700e04287d6eac4f3bf70b6'
    'MT2_Multiplayer.dll'='24327a4ef3e01e06ab222c76dbbedb2b1fe31fc23b9844feaa4e62a20164892e'
    'steam_api64.dll'='e082bf5c9f881c822b1540a76b74f9d15e18019a73ebd206a559595badcb7f65'
}
foreach ($name in @($expected.Keys) + @('steam_api64_original.dll','SDL3.dll','libphysfs.dll','mt2mp_ui/entry.win','mt2mp_ui/lobby.win')) {
    $path = Join-Path $game $name
    $entry = [ordered]@{name=$name;exists=(Test-Path -LiteralPath $path -PathType Leaf)}
    if ($entry.exists) {
        try {
            $item=Get-Item -LiteralPath $path
            $entry.bytes=$item.Length
            $entry.reparse=[bool]($item.Attributes -band [IO.FileAttributes]::ReparsePoint)
            $entry.sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($expected.ContainsKey($name)) { $entry.matches_beta3=($entry.sha256 -eq $expected[$name]) }
            $entry.zone_identifier=Test-Path -LiteralPath ($path + ':Zone.Identifier')
        } catch { $entry.error=$_.Exception.Message }
    }
    $report.files += $entry
}
foreach ($name in @('vcruntime140.dll','vcruntime140_1.dll','msvcp140.dll','ucrtbase.dll')) {
    $path=Join-Path $env:SystemRoot ('System32\' + $name)
    $entry=[ordered]@{name=$name;exists=(Test-Path -LiteralPath $path -PathType Leaf)}
    if ($entry.exists) { $entry.version=(Get-Item -LiteralPath $path).VersionInfo.FileVersion }
    $report.runtime += $entry
}
foreach ($process in @(Get-Process | Where-Object { $_.ProcessName -eq 'MT2' })) {
    $entry=[ordered]@{pid=$process.Id}
    try {
        $entry.path=$process.Path
        $entry.modules=@($process.Modules | Where-Object { $_.ModuleName -in @('WSOCK32.dll','MT2_Multiplayer.dll','steam_api64.dll','steam_api64_original.dll','SDL3.dll','libphysfs.dll') } | ForEach-Object { @{name=$_.ModuleName;path=$_.FileName} })
    } catch { $entry.error=$_.Exception.Message }
    $report.processes += $entry
}
$privateRoot=Join-Path $env:LOCALAPPDATA 'MT2Multiplayer'
if (Test-Path -LiteralPath $privateRoot -PathType Container) {
    foreach ($directory in @(Get-ChildItem -LiteralPath $privateRoot -Directory)) {
        if ($directory.Name -notmatch '^[0-9a-f]{64}$') { continue }
        $profile=[ordered]@{path=$directory.FullName;logs=@()}
        foreach ($name in @('mt2mp_bootstrap.log','mt2mp.log')) {
            $path=Join-Path $directory.FullName $name
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                try { $profile.logs += @{name=$name;modified_utc=(Get-Item -LiteralPath $path).LastWriteTimeUtc.ToString('o');tail=@(Get-Content -LiteralPath $path -Tail 60)} }
                catch { $profile.logs += @{name=$name;error=$_.Exception.Message} }
            }
        }
        $report.profiles += $profile
    }
}
$report.warnings += 'This diagnostic does not start, stop or modify the game, its installation or saves. Only this report is written.'
$steam = @($report.files | Where-Object { $_.name -eq 'steam_api64.dll' })[0]
if ($steam.sha256 -eq 'e157a9da8d041970964712d53d882433c76736bb4df530bf1310bd4fecaf955b') {
    $report.startup_finding='legacy_beta2_steam_proxy_detected'
    Write-Output 'FOUND: The old beta.2 Steam proxy is still installed. It conflicts with the beta.3 WSOCK loader.'
    Write-Output 'No files were changed. Keep steam_api64_original.dll and send this report to the maintainer.'
} elseif ($steam.matches_beta3) {
    $report.startup_finding='original_steam_library_confirmed_other_cause_unresolved'
    Write-Output 'Original Steam library confirmed. Other startup causes still need the report.'
} else {
    $report.startup_finding='steam_library_missing_or_unknown'
    Write-Output 'Steam library is missing or has an unknown hash. No files were changed.'
}
$report | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath $ReportPath -Encoding utf8
Write-Output ('Startup diagnostic saved: ' + $ReportPath)

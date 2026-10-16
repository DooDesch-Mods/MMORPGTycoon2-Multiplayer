# Private beta builds

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

Tester prereleases are in the private [GitHub repository](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/releases). Download access requires repository access. A new drop-in release requires the maintainer's native acceptance first; CI does not perform that acceptance.

## Install a drop-in beta

1. Exit MMORPG Tycoon 2 normally and verify the downloaded ZIP's `.sha256` checksum.
2. Read `package.json`: `package_mode` must be `dropin`. Check its file hashes before installation.
3. If beta.2's proxy was copied into the game installation, perform the guarded migration below first.
4. Copy `WSOCK32.dll`, `MT2_Multiplayer.dll` and the authored `mt2mp_ui` folder beside `MT2.exe`; keep the README, manifest and notices. Stop if an unknown `WSOCK32.dll` already exists.
5. Start the game normally through Steam and open Multiplayer on the native desktop.

The 15-file drop-in ZIP contains two mod DLLs, five authored UI files, six notice files, `README.txt` and `package.json`. It contains no original game/Steam DLL, Steam proxy, sidecar, Python starter, save or personal configuration. Each player needs a legal supported Windows x64 game installation. The game's `steam_api64.dll` remains original.

Private mod saves and diagnostics stay under `%LOCALAPPDATA%\MT2Multiplayer\<installation-id>\`. Preserve this directory, normal saves, and old portable test directories during updates. To disable, exit the game and move only verified mod-owned files outside its folder; do not delete private data or the original Steam library.

Normal saves for the current Steam account are visible through the game's desktop and world picker. They are mounted behind private session saves for reading. All new saves use the private profile, and multiplayer snapshots retain the selected world's folder name. Existing `MPTestSession` folders from older betas are preserved.

The Host opens a lobby before creating or opening a world. An active Host world starts shared preparation automatically. To start from the multiplayer save picker, select a world and use the single **Load world and start** button. Opening a public lobby row joins it directly; manual lobby-ID Join remains available. Both players must reach the shared ready state before building. A world name alone does not confirm synchronization.

## Migrate a legacy proxy copied into the game directory

These checks apply only when a legacy proxy actually replaced `steam_api64.dll`. The portable beta.2 package used outside the game directory does not require restoration of the normal installation. Do not install the legacy proxy and WSOCK loader together.

The supported original Steam DLL is 319584 bytes, SHA256 `e082bf5c9f881c822b1540a76b74f9d15e18019a73ebd206a559595badcb7f65`. The known beta.2 proxy SHA256 is `e157a9da8d041970964712d53d882433c76736bb4df530bf1310bd4fecaf955b`. The original hash was retained in `artifacts/auto_world/installed-inspection.json`. Do not infer originality from a filename.

The following manual PowerShell sequence changes only `steam_api64.dll`, after matching the supported executable, current proxy and preserved original backup. Replace the example path. It stops if the exact game is running, process paths cannot be read, or any hash differs. It keeps `steam_api64_original.dll` and all user data.

```powershell
& {
$ErrorActionPreference = 'Stop'
$game = (Resolve-Path -LiteralPath 'C:\Path\To\MMORPG Tycoon 2').Path
$exe = Join-Path $game 'MT2.exe'
$steam = Join-Path $game 'steam_api64.dll'
$backup = Join-Path $game 'steam_api64_original.dll'
foreach ($file in @($exe, $steam, $backup)) {
    $item = Get-Item -LiteralPath $file
    if ($item.PSIsContainer -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Expected regular installation files; no files changed.' }
}
$processes = @(Get-CimInstance Win32_Process -Filter "Name='MT2.exe'")
foreach ($process in $processes) {
    if (-not $process.ExecutablePath -or [string]::Equals([IO.Path]::GetFullPath($process.ExecutablePath), $exe, [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Exit the game normally; its installation must be idle and readable.'
    }
}
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6') { throw 'Unsupported game executable; no files changed.' }
if ((Get-Item -LiteralPath $backup).Length -ne 319584 -or (Get-FileHash -LiteralPath $backup -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'e082bf5c9f881c822b1540a76b74f9d15e18019a73ebd206a559595badcb7f65') { throw 'Original Steam backup differs; no files changed.' }
$current = (Get-FileHash -LiteralPath $steam -Algorithm SHA256).Hash.ToLowerInvariant()
if ($current -eq 'e082bf5c9f881c822b1540a76b74f9d15e18019a73ebd206a559595badcb7f65') {
    Write-Output 'Original Steam DLL already restored; no copy needed.'
} elseif ($current -eq 'e157a9da8d041970964712d53d882433c76736bb4df530bf1310bd4fecaf955b') {
    Copy-Item -LiteralPath $backup -Destination $steam -Force
    if ((Get-FileHash -LiteralPath $steam -Algorithm SHA256).Hash.ToLowerInvariant() -ne 'e082bf5c9f881c822b1540a76b74f9d15e18019a73ebd206a559595badcb7f65') { throw 'Restored Steam DLL readback differs; do not start the game.' }
    Write-Output 'Original Steam DLL restored; preserved backup retained.'
} else {
    throw 'Current Steam DLL is neither the known proxy nor the supported original; no files changed.'
}
}
```

Run this only while keeping the game closed; the check does not lock out a later manual launch. If a backup is missing or a hash is unknown, retain the files and recover through Steam's verification route instead of copying an unverified DLL. No installer or migration command is run by CI.

## Legacy portable test route

Beta.1/beta.2 archives use the isolated starter. Extract those full ZIPs outside the game installation, keep `_internal` beside `MT2MP-Start.exe`, start Steam and follow their included instructions. Their source and test route remain supported. Do not merge their files into a drop-in ZIP. New private beta CI selects `--mode dropin` explicitly; `tools/package.py` and `build_beta.py` retain the default `legacy` mode for existing callers.

## Build a local tester beta

The existing local build command accepts the full tester version. Set it before
native acceptance so the DLL and package report the same version. For example:

```powershell
python -X utf8 tools/build.py --version 0.1.0-beta.13 --target MT2_Multiplayer --target wsock32
```

Run the affected checks and native acceptance, then commit the accepted source.
Prepare a freeze JSON with `candidate` (`v0.1.0-beta.13@<full source commit>`),
`source_commit`, `dll_sha256` and `loader_sha256` from those accepted binaries.
Use an unused version and output path; existing packages are preserved.

```powershell
python -X utf8 tools/package.py --mode dropin --version 0.1.0-beta.13 --freeze artifacts/local_beta/v0.1.0-beta.13/freeze.json --output artifacts/local_beta/v0.1.0-beta.13/MMORPGTycoon2-Multiplayer-v0.1.0-beta.13-windows-x64.zip
```

Validate the result with `.github/scripts/build_beta.py`'s `validate` function and
record its checksum and actual local checks. This route uses the existing package
allowlist and requires no GitHub build, tag or release. Online acceptance remains
a separate manual test.

## Publish the next accepted beta

Commit the intended source, UI and notices with a matching CMake base version. After native acceptance, push an unused `vX.Y.Z-beta.N` tag or update `.github/beta-milestone.json` on main with exact `schema`, `version`, `summary` and `native_evidence` fields. No planned or failed milestone receives a record. The [beta workflow](../.github/workflows/beta.yml) keeps its manual-tag and same-run milestone publication paths.

CI runs source and package tests, builds official pinned SDL, `MT2_Multiplayer` and `wsock32`, then packages only the drop-in allowlist. It skips PyInstaller for drop-in releases. `package.json` binds the mode, beta version, source commit and every file hash; the build freeze also binds the WSOCK loader hash. It keeps `online_acceptance_passed=false`. Only the release job can write tags/releases. Native loader, original Steam behavior and real peer world loading still require the maintainer's actual game checks.

Existing tags and conflicting assets are never overwritten. Identical repeated publication keeps existing assets; changed rebuild bytes require a new version. Published beta.2 remains unchanged. Inspect a failed draft or unknown write before retrying. No recursive `GITHUB_TOKEN` tag trigger is required.

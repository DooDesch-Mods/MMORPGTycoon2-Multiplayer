# MMORPG Tycoon 2 Multiplayer {{MULTIPLAYER_VERSION}}

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

Build together in one Host world. This is an early beta in active development. Expect bugs, desyncs and unfinished features. Start with a small world and keep backups of saves you care about.

Both players need Multiplayer {{MULTIPLAYER_VERSION}}, MT2 Mod Framework Runtime {{FRAMEWORK_MIN_VERSION}} or newer, and the supported Windows x64 Steam game build 24993678. The Framework runtime is a separate download.

## Install with Vortex (recommended)

Install the MMORPG Tycoon 2 Vortex extension and MT2 Mod Framework Runtime first.

1. Close the game and import this ZIP into Vortex for MMORPG Tycoon 2.
2. Enable Multiplayer and deploy.
3. Start the game normally through Steam.
4. Double-click Multiplayer on the game desktop, or choose Mods... > Multiplayer > Open while in a world.
5. Host a lobby and create or load a small world, or join your friend's lobby.

Vortex selects the profile and manages the files. Use Vortex to update, disable or remove this plugin with the game closed. Keep your saves and profile data.

## Manual installation

This ZIP is a Framework mod folder. Extracting it beside MT2.exe does not install or enable Multiplayer.

1. Close the game and install the separate Framework runtime using its manual installation instructions.
2. Read `mt2fw.json` beside MT2.exe. It must enable the Framework and select an absolute profile path. If this file is missing, complete Framework setup first; the loader otherwise starts the original game without mods.
3. Create the `mods/doodesch.multiplayer` folder inside that profile and extract this entire ZIP there.
4. Confirm that `<profile>/mods/doodesch.multiplayer/mod.json` and `MT2MultiplayerPlugin.dll` are beside each other, with the assets folder beside them.
5. Start the game through Steam and open Multiplayer.

The Framework configuration has an `enabled` boolean and an absolute `profile` path. Preserve an existing configuration and profile; do not overwrite another installation's settings. Do not mix Vortex deployment with manual updates to the same mod folder.

The Framework stores game data, including saves, in `<profile>/game-data`. This profile is separate from the original Steam game data, so existing Steam saves may not appear there. The originals remain untouched. Keep the profile and its game-data folder when updating the mod.

## Troubleshooting

Check that both players use the version shown at the top of this README. If Multiplayer does not appear, check Framework setup and the mod folder location before replacing files. Open Report a problem in the Multiplayer window to send technical details privately.

Older standalone Multiplayer loaders need the documented migration to the Framework installation. Do not overwrite an unknown WSOCK32.dll or Steam DLL. Shared edits and other gameplay systems remain in development; package integrity checks do not prove a complete online game session.

The archive contains no game files, Steam DLL, Framework runtime, Profiler, sidecar, Python or legacy UI files. The Multiplayer license and third-party notices are included. MT2 Mod Framework and MMORPG Tycoon 2 retain their respective licenses and rights.

# Steam launch and lobby fixes

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

## Highlights

- **Launch through Steam.** Install the mod directly in your game folder. A separate starter is no longer needed.
- **Your Host world appears automatically.** Create or load a world after opening a lobby, then start the session from Multiplayer.
- **Leave and host again.** Leaving a lobby now completes correctly, so you can open another lobby.

## Fixes

- Fixed missing session initialization when the game was launched directly through Steam.
- Fixed starting a shared session in a freshly created, empty world.
- Fixed loading a world before the session's time controls had initialized.
- Updated Steam Friends detection used by lobby invitations.

## Install and play

1. Close the game and extract the release ZIP. Copy `WSOCK32.dll`, `MT2_Multiplayer.dll` and `mt2mp_ui` into the folder containing `MT2.exe`.
2. Both players launch the game through Steam. The Host opens a lobby; the Client selects it and presses **Join**.
3. The Host creates or loads a small world, then presses **Start session**. Wait for both players to finish loading before building.

**Updating from beta.2?** If its files were copied into the game folder, follow the [upgrade steps](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/blob/main/docs/BETA.md#migrate-a-legacy-proxy-copied-into-the-game-directory) first. Keep your saves and existing test folders.

## Beta status

This build is ready for the next two-player test. Separate-account online play still needs confirmation. Undo/Redo, complete action coverage, four players and large worlds remain in development.

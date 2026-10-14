# World loading and lobby exit fixes

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

## Highlights

- **Start a new world together.** Empty network state no longer blocks the shared world's final loading confirmation.
- **Leave after the Host disconnects.** An idle Client can return to the desktop instead of getting stuck in the lobby.
- **Find your existing saves.** Your Steam account's normal worlds appear alongside private multiplayer saves.

## Fixes

- Kept the original world folder when capturing and loading session snapshots. New sessions no longer create an extra `MPTestSession` world.
- Kept original saves unchanged while routing new saves to the private multiplayer profile.
- Prevented the local two-instance launcher from mixing the installed loader with its Steam proxy.

## Install and play

1. Close the game and extract the release ZIP. Copy `WSOCK32.dll`, `MT2_Multiplayer.dll` and `mt2mp_ui` into the folder containing `MT2.exe`.
2. Both players launch the game through Steam. The Host opens a lobby; the Client selects it and presses **Join**.
3. The Host creates or loads a small world, then presses **Start session**. Wait for both players to finish loading before building.

**Updating from beta.2?** If its files were copied into the game folder, follow the [upgrade steps](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/blob/main/docs/BETA.md#migrate-a-legacy-proxy-copied-into-the-game-directory) first. Keep your saves and existing test folders.

## Beta status

The affected loading and disconnect flows are checked locally. Separate-account online play still needs confirmation. Tutorial progression is not shared yet. Undo/Redo, complete action coverage, four players and large worlds remain in development.

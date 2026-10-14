# Shared world start fixes

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

## Highlights

- **Use your world's name.** Fixed Clients rejecting loaded worlds because an old diagnostic check still required a special MPTest name.
- **Load and start in one action.** Opening a Host world starts shared preparation automatically. The multiplayer save picker has one load/start button.
- **Join directly from the browser.** Opening a public lobby row now joins it immediately.

## Fixes

- Cleared the previous Client simulation binding after leaving, so another session can load in the same game process.
- Prevented internal world operations from repeating an already handled diagnostic load request.

## Install and play

1. Close the game and extract the release ZIP. Copy `WSOCK32.dll`, `MT2_Multiplayer.dll` and `mt2mp_ui` into the folder containing `MT2.exe`.
2. Both players launch the game through Steam. The Host opens a lobby; the Client opens its public lobby row to join.
3. The Host creates or opens a small world, or selects a save and presses **Load world and start**. Wait for both players to reach the ready state before building.

**Updating from beta.2?** If its files were copied into the game folder, follow the [upgrade steps](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/blob/main/docs/BETA.md#migrate-a-legacy-proxy-copied-into-the-game-directory) first. Keep your saves and existing test folders.

## Beta status

The affected loading and disconnect flows are checked locally. Separate-account online play still needs confirmation. Tutorial progression is not shared yet. Undo/Redo, complete action coverage, four players and large worlds remain in development.

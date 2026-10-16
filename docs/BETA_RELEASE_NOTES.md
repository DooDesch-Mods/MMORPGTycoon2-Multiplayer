# Stable world updates and shared equipment

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

## Highlights

- **Keep growing your world.** Larger live updates now cross the old single-packet limit and reach the Client as complete updates.
- **See the same weapons.** Equipment acquired through the Host simulation reaches Client characters, including the visible weapon model.
- **Keep paused characters still.** Client characters no longer consume leftover movement time while paused. Reloading also clears stale Client employee waypoints through the original game functions.
- **Leave a failed session.** Host departure and sync failures release the lobby controls for another session.

## Install and play

1. Close the game. Copy `WSOCK32.dll`, `MT2_Multiplayer.dll` and `mt2mp_ui` from the ZIP into the folder containing `MT2.exe`.
2. Both players use this beta and launch through Steam. The Host opens a lobby; the Client selects its public lobby row to join.
3. The Host creates or opens a small world, or selects a save and presses **Load world and start**. Wait for both players to reach the ready state before building.

**Updating from beta.2?** Follow the [legacy upgrade steps](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/blob/main/docs/BETA.md#migrate-a-legacy-proxy-copied-into-the-game-directory) first. Keep your saves and test folders.

## Beta status

The changes above passed local two-instance checks. Complete action coverage, the full natural tutorial, four players and large worlds remain open. The shared online test with separate Steam accounts is still manual.

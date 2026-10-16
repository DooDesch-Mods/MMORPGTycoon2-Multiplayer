# Shared actions and repair controls

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

## Highlights

- **Keep actions flowing.** Fixed a shared packet classifier that rejected build and management actions while live world updates were active, including deeper Quest NPC results.
- **Control repairs together.** Clients can use the original **Schedule Fix** and **Cancel Fix** buttons. The Host executes each request once and both players see the same repair queue.
- **Cut duplicate CI compilation.** Shared source checks now reuse compiled code across test programs.

## Install and play

1. Close the game. Copy `WSOCK32.dll`, `MT2_Multiplayer.dll` and `mt2mp_ui` from the ZIP into the folder containing `MT2.exe`.
2. Both players use this beta and launch through Steam. The Host opens a lobby; the Client selects its public lobby row to join.
3. The Host creates or opens a small world, or selects a save and presses **Load world and start**. Wait for both players to reach the ready state before building.

**Updating from beta.2?** Follow the [legacy upgrade steps](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/blob/main/docs/BETA.md#migrate-a-legacy-proxy-copied-into-the-game-directory) first. Keep your saves and test folders.

## Beta status

The repair controls passed local two-instance checks. Natural repair completion, full action coverage, the complete tutorial, four players and large worlds remain open. The online test with separate Steam accounts is still manual.

# Shared tutorial and Client research

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

## Highlights

- **See the same tutorial step.** Clients receive the Host's tutorial instructions and movement progress bar.
- **Start research together.** The Client's Research button starts the initial Starting Points project on both players, with one Host-ordered purchase.
- **Share the completed project.** The Client can Commit the completed Starting Points project. Both players receive the unlock and Release entry once.

## Fixes

- Kept research purchases, project creation and Commit on the existing shared action path.
- Applied the Client's research success display only after the shared result is confirmed.
- Added targeted regression tests for native research controls, ownership, accounting and action ordering.

## Install and play

1. Close the game and extract the release ZIP. Copy `WSOCK32.dll`, `MT2_Multiplayer.dll` and `mt2mp_ui` into the folder containing `MT2.exe`.
2. Both players launch the game through Steam. The Host opens a lobby; the Client opens its public lobby row to join.
3. The Host creates or opens a small world, or selects a save and presses **Load world and start**. Wait for both players to reach the ready state before building.

**Updating from beta.2?** If its files were copied into the game folder, follow the [upgrade steps](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/blob/main/docs/BETA.md#migrate-a-legacy-proxy-copied-into-the-game-directory) first. Keep your saves and existing test folders.

## Beta status

Client Research was checked with two local game instances while the Host was paused and running. Client Commit and the first shared tutorial steps were also checked in game. The complete natural tutorial, developer assignment/hiring and other technologies still need work. Undo/Redo, complete action coverage, four players and large worlds remain in development. The shared online test with separate Steam accounts is still manual.

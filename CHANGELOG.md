# Changelog

## 0.1.0-beta.20

### Fixed

- Receive Steam lobby invites as soon as Multiplayer is ready, before opening a lobby menu.
- Fix Steam invite callbacks on Windows so an accepted invite reaches the normal lobby join flow.
- Show the Lobby ID before loading a world.
- Search public lobbies worldwide.
- Include the correct version and manual installation steps in the packaged README.

Both players need Early Beta 20 and MT2 Mod Framework Runtime 0.2.6 or newer. Close both games before updating. This hotfix keeps the gameplay coverage from Beta 19; a new two-player online invite test is still pending.

## 0.1.0-beta.19

### Fixed

- Let developers start modular construction and share the current construction state with the joining player.
- Keep modular buildings available as destinations for NPC quests on both players.
- Continue placing scenery after leaving and rejoining without closing the game.
- Keep NPC edits in modular worlds from ending the co-op session.

### Early Beta

Dungeon, character-design and MMO-management tools remain included as experimental. Some actions can still fail or desync. Full simulation, reconnecting after a game restart, four-player sessions and larger worlds remain unconfirmed.

Both players need Early Beta 19 and MT2 Mod Framework Runtime 0.2.6 or newer. Close the game before updating and keep save backups.

## 0.1.0-beta.18

### Added

- Build with modular prefabs, add or remove blocks and entrances, and undo or redo shared layout edits.
- Save a modular world, load it together and keep editing your buildings.

### Experimental

- Try Dungeon portals, layout tools, doors, keys, treasure and lighting together.
- Create and copy character types, change costumes, edit abilities and import character PNG cards.
- Try server networks, game fees, loans, advertising, staff commands and MMO updates.
- Try the integrated terrain, region, NPC and monster tools, running construction and right-click block removal.

### Known issues

- Experimental features are included, but some interactions can fail or desync. Start with two players in a small world and keep save backups.
- Shared edits can take several seconds or longer. Full simulation, four-player play, larger worlds and performance under load remain incomplete.

### Requirements

Both players need Early Beta 18 and MT2 Mod Framework Runtime 0.2.6. Close the game before updating.


## 0.1.0-beta.17

### Added

- Copy supported buildings and scenery together.
- Group scenery, copy a group and ungroup the original or its copy.
- Clear individual scenery with one click of the game's eraser and share the refund.
- Undo and redo shared building and scenery edits.

### Fixed

- Fix a failed return to the main menu caused by Multiplayer save data.
- Keep both players in sync when copying a building after placement and undo/redo.
- Restore scenery correctly when undoing a building placement that cleared it.
- Keep group names and selection consistent when ungrouping scenery.

### Known issues

- Shared edits can take several seconds or longer to finish.
- The game can occasionally crash during startup.
- This is an Early Beta for two players in small worlds. Gameplay support is still incomplete. Keep backups of your saves.

### Requirements

Both players need Multiplayer Early Beta 17 and MT2 Mod Framework Runtime 0.2.5. Close the game before updating.

## 0.1.0-beta.16

### Added

- Create quests together and edit their titles, required counts and rewards.
- Choose buildings and NPCs as shared quest destinations.

### Fixed

- Fixed path redo being blocked by an incorrect money check.
- Reduce stutters while hosting a shared world.
- Keep building moves and refunds in sync between players.
- Let joining players change the shared game speed and pause.
- Leave a lobby and start another without restarting the game.

## 0.1.0-beta.15

### Added

- Place, move and remove individual Elite monsters together while the world is paused, using the game's original tools.

### Fixed

- Keep monster selection and the active tool consistent after a shared edit.

## 0.1.0-beta.14

### Added

- Open Multiplayer from its own game-desktop icon or the central Mods window.
- Send problem reports privately from the Multiplayer window and keep the confirmed report number for support. Automatic technical reports are optional and off by default.

### Changed

- Multiplayer menus and problem reporting now use English.
- Multiplayer runs as a native MT2 Mod Framework plugin. Install Runtime 0.2.0 separately through Vortex.
- New World opens the original game wizard while connected players wait for the Host.
- The native menu has aligned icons, player portraits and button labels, with visible creating and loading states.

### Early Beta

- In active development. Expect bugs, desyncs and unfinished features. Start with a small world and keep backups. Both players need matching Multiplayer and dependency versions.

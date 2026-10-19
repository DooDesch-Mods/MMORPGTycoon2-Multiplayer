# MMORPG Tycoon 2 Multiplayer - Early Beta

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

[Download on Nexus Mods](https://www.nexusmods.com/mmorpgtycoon2/mods/28) | [GitHub releases](https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/releases) | [Changelog](https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/blob/main/CHANGELOG.md)

![MT2 Multiplayer - Build your world together. Early Beta, in active development.](https://raw.githubusercontent.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/main/docs/images/multiplayer-cover.png)

Host a Steam lobby and invite a friend to build alongside you in MMORPG Tycoon 2. Share one world through the game's familiar tools, then save and continue together later.

**Early Beta:** Expect bugs, desyncs and unfinished features. Start with a small world and keep backups of your saves.

[Feature coverage](#what-works-in-co-op) | [Roadmap](#roadmap) | [Start playing](#start-playing)

<!-- roadmap:start -->
## What works in co-op?

Coverage in [Early Beta 16](https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/releases/tag/v0.1.0-beta.16). The current focus is one Host and one friend building a small world. Partial means only the listed parts are supported.

<picture>
  <source media="(max-width: 600px)" srcset="https://raw.githubusercontent.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/main/docs/images/coverage-mobile.svg">
  <img src="docs/images/coverage.svg" alt="Feature coverage: 4 available, 8 partial, 1 planned, across 13 tracked areas" width="1200">
</picture>

Percentages count the listed areas equally. They describe feature coverage, not overall project completion or stability.

<details>
<summary>Feature details - what is included</summary>

| Feature | Early Beta 16 | What is covered |
| --- | --- | --- |
| Steam lobbies | ✅ Available | Host a lobby or join a friend's session from the Multiplayer window. |
| A shared world | ✅ Available | The Host creates or loads a world; the joining player receives it. |
| Session chat | ◐ Partial | Basic session messages in the Multiplayer window. |
| Buildings and scenery | ◐ Partial | Place, move and remove supported buildings and individual scenery objects. |
| Terrain and roads | ◐ Partial | Paint forest terrain, place roads and remove roads. More terrain tools and path types are planned. |
| Regions | ✅ Available | Share region purchases, biome styles and region levels. |
| Pause and game speed | ✅ Available | Keep the same pause state and simulation speed. |
| Research | ◐ Partial | Start Bridges research, assign workers and unlock it together. More research options are planned. |
| NPC placement | ◐ Partial | Place, move and remove Class Trainers and Quest Givers. Broader NPC gameplay is a next priority. |
| Elite monsters | ◐ Partial | Place, move and remove individual Elite monsters while paused. Live monster gameplay is a next priority. |
| Save and resume | ◐ Partial | Continue from the Host's saved world. Wider support for saved objects is a next priority. |
| Quests and combat | ◐ Partial | Create and remove quests, edit titles, required counts and rewards, and choose building or NPC destinations. Live combat remains planned. |
| Four players and larger worlds | ○ Planned | The current focus is two-player co-op in small worlds. |

</details>

## Roadmap

<picture>
  <source media="(max-width: 600px)" srcset="https://raw.githubusercontent.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/main/docs/images/roadmap-mobile.svg">
  <img src="docs/images/roadmap.svg" alt="Roadmap: Available now - Build together. Next priorities - A living world. Longer term - Grow your world" width="1200">
</picture>

[Read the roadmap in text](docs/ROADMAP.md).
<!-- roadmap:end -->

![A shared building in MMORPG Tycoon 2.](https://raw.githubusercontent.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/main/docs/images/building-together.png)

## Start playing

You need Windows x64, the supported Steam build of MMORPG Tycoon 2, [MT2 Mod Framework Runtime 0.2.2](https://www.nexusmods.com/mmorpgtycoon2/mods/25) and [MMORPG Tycoon 2 Vortex Extension 0.1.2](https://www.nexusmods.com/site/mods/2465). Both players need matching Multiplayer and dependency versions. The Framework SDK is optional.

1. Close the game and install the Framework Runtime and Vortex Extension.
2. Install the [Multiplayer ZIP](https://www.nexusmods.com/mmorpgtycoon2/mods/28?tab=files) in Vortex for MMORPG Tycoon 2.
3. Enable the mod, deploy and start the game through Steam.
4. Open the **Multiplayer** desktop icon, or choose **Mods... > Multiplayer > Open**.
5. Host a lobby and create or load a small world, or join your friend's lobby. Wait until both players are ready before building.

The download contains the Multiplayer plugin. Install the Framework Runtime separately. Remove an older standalone Multiplayer loader or Steam proxy before switching to this plugin.

## Report a problem in game

Open **Report a problem** in the Multiplayer window, add an optional description and choose **Send report privately**. The mod prepares the technical details for you. Keep the confirmed report number and include it when [contacting support](https://support.doodesch.de/multiplayer).

Reports stay private for 30 days. They do not include saves, chat, screenshots or Steam account IDs. Automatic technical reports are optional and off by default. Please keep passwords and personal information out of the description.

## Update or remove

Close the game, then update, disable or remove Multiplayer through Vortex. Keep your saves and profile data. Native plugins require a game restart after an update.

## For developers

See [Build from source](https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/blob/main/docs/BUILDING.md) for the native plugin, packaging and test commands. The [beta guide](https://github.com/DooDesch-Mods/MMORPGTycoon2-Multiplayer/blob/main/docs/BETA.md) also documents historical standalone packages.

## Credits and permissions

Created by DooDesch. The [Multiplayer license](LICENSE.md) allows source access and personal changes. Public redistribution and modified public releases require permission.

MinHook, Dear ImGui, nlohmann/json and SDL retain their own licenses; see [Third-party notices](THIRD_PARTY_NOTICES.md). [MT2 Mod Framework](https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework) is MIT-licensed. MMORPG Tycoon 2 and its game artwork are by VectorStorm.

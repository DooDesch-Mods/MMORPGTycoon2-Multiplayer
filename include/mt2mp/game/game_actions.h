#pragma once

// ═══════════════════════════════════════════════════════════════════════
// MMORPG Tycoon 2 - Complete Game Action Reference
//
// Every player action that modifies game state. Extracted from MT2.exe
// event table, undo system strings, and CursorBehaviours.txt.
//
// For multiplayer sync: ALL of these are captured by the autosave
// mechanism. When ANY action happens, the game autosaves and we detect
// the CRC32 change via zlib hooks.
// ═══════════════════════════════════════════════════════════════════════

namespace mt2mp::game {

// ── 1. WORLD BUILDING ────────────────────────────────────────
// Cursor tools: mmoCursorBehaviourBuild, mmoCursorBehaviourPath, etc.
//
// Buildings:
//   - Tavern, Blacksmith, PotionShop, Shop, TrinketShop, RealMoneyShop
//   - Inn, SmallInn, StartingPoint, Respawn, Landmark
//   - FlightPoint, Teleporter, Depot (air vehicle)
//   - DungeonPortal, DungeonExterior
//   - Scenery (non-interactive decorative)
//
// Paths & Infrastructure:
//   - Road (mmoCursorBehaviourPath)
//   - Ramp (mmoCursorBehaviourPath with ramp mode)
//   - Wall (mmoCursorBehaviourWall)
//   - Bridge (mmoCursorBehaviourBridge)
//   - Mountain Pass (mmoCursorBehaviourPass)
//   - Flight Path (mmoCursorBehaviourFlightPath, multi-node)
//   - Vehicle Route / Zeppelin (mmoCursorBehaviourVehicle)
//
// Terrain:
//   - Raise/Lower terrain (mmoCursorBehaviourTerrainHeight)
//   - Paint terrain style 0-3 (mmoCursorBehaviourTerrainBrush)
//   - Raise/Lower region (mmoCursorBehaviourLiftRegion)
//
// Scenery (mmoCursorBehaviourScenery):
//   - Large Plants (trees, mushrooms, bamboo, cacti, spikes)
//   - Small Plants (reeds, medium/small plants)
//   - Earthworks (crystals, ice, desert, stone)
//   - Structures (towers, warfare, ruins, water)
//   - Furnishings (carts, chairs, posts, shelves, tables)
//   - Details (props, wall decorations)
//   - Extras (festivals, xeno, western, confections)
//   - Dungeon Scenery
//
// Undo Messages (from binary):
//   undo_msg_place, undo_msg_move, undo_msg_duplicate, undo_msg_destroy
//   undo_msg_erasescenery, undo_msg_addblock, undo_msg_removeblock
//   undo_msg_addentrance, undo_msg_removeentrance

// ── 2. CHARACTER DESIGN ──────────────────────────────────────
// Events from MT2.exe event table:
//   NewCharacterTypeCreated     - Create new character class
//   CharacterTypeCostumeChange  - Change class appearance/costume
//   CharacterTypeNameChange     - Rename character class
//   CharacterTypeDestroyed      - Delete character class
//   CharacterTypeCollectionMaxSlotsChanged - Change slot limits
//   CharacterTypeCollectionDestroyed       - Delete class collection
//
// Abilities:
//   AbilityEdited               - Modify ability stats/effects
//   AbilityPrefabRemoved        - Remove prefab ability
//   AbilityRemoved              - Delete ability entirely

// ── 3. MONSTER/NPC MANAGEMENT ────────────────────────────────
// Cursor tools:
//   mmoCursorBehaviourMonster     - Place normal monster
//   mmoCursorBehaviourBoss        - Place boss
//   mmoCursorBehaviourNPC         - Place QuestGiver/Guard/Trainer
//   mmoCursorBehaviourDungeonMonster - Place dungeon monster
//   mmoCursorBehaviourDungeonBoss   - Place dungeon boss
//   mmoCursorBehaviourEliteMonster  - Place elite monster
//
// Events:
//   AddMonster, RemoveMonster
//   AddNPC, RemoveNPC
//   BossPhasePreEdit, BossPhaseEdited, BossPhaseRemoved

// ── 4. DUNGEON DESIGN ────────────────────────────────────────
// Cursor tools:
//   mmoCursorBehaviourDungeonTile  - Floor/tile placement
//   mmoCursorBehaviourDungeonDoor  - Door placement
//   mmoCursorBehaviourDungeonKey   - Key placement
//   mmoCursorBehaviourDungeonPaint - Floor painting/texture
//
// Events:
//   DungeonRemoved, DungeonInstanceRemoved
//   undo_msg_opentiles, undo_msg_closetiles, undo_msg_edittiles
//   undo_msg_painteddungeon

// ── 5. ZONE MANAGEMENT ──────────────────────────────────────
// Cursor tools:
//   mmoCursorBehaviourZone - Monster/Puzzle/Story zones
//   mmoCursorBehaviourDistrict - Normal/Flat/Lake/Erase districts
//
// Events:
//   AddZone, RemoveZone
//   undo_msg_zoneedit, undo_msg_zonereset

// ── 6. REGION MANAGEMENT ─────────────────────────────────────
// Cursor tools:
//   mmoCursorBehaviourBuyRegion - Activate region
//   mmoCursorBehaviourSellRegion - Deactivate region
//   mmoCursorBehaviourSetRegionLevel - Set difficulty
//   mmoCursorBehaviourSetRegionTint - Color tinting
//   mmoCursorBehaviourSetRegionSky - Sky settings
//   mmoCursorBehaviourLiftRegion - Raise/lower height
//
// Undo Messages:
//   undo_msg_buyregion, undo_msg_sellregion
//   undo_msg_setregionlevel
//   undo_msg_raiseregion, undo_msg_lowerregion

// ── 7. NETWORK/INFRASTRUCTURE ────────────────────────────────
// Cursor tools (mmoCursorBehaviourBuild with network types):
//   Build Server, Build Uplink, Build Junction, Build Pool
//   Build Wan, Build Fiber, Build Cable
//   DestroyNetwork
//
// Events:
//   AddNode, RemoveNode (network nodes)

// ── 8. ECONOMY & BUSINESS ────────────────────────────────────
// Events:
//   AdsChanged - Advertisement changes
//   Income, Expense - Financial transactions
//   AwardsChanged, EarnedAward - Achievement system
//   NewRelease, NewMajorBuildVersion, NewMinorBuildVersion, NewBuildVersion
//
// Systems:
//   mmoPricing - Service pricing
//   mmoMarketing - Marketing campaigns
//   mmoAdCampaign - Advertisement campaigns
//   mmoLoanOffer - Loan management

// ── 9. QUESTS ────────────────────────────────────────────────
// Cursor tools:
//   mmoCursorBehaviourQuestDestination - Quest marker placement
//
// Undo Messages:
//   undo_msg_addquest, undo_msg_removequest, undo_msg_editquest

// ── 10. META/DESIGN ──────────────────────────────────────────
// Events:
//   LogoChanged - Game logo modified
//   GameFeatureEnabled - Game feature toggled
//   GameOptionEnabled - Game option toggled
//   GameFlagEnabled - Game flag toggled
//   TechUnlocked - Technology researched
//   ArtPackUnlocked - Art pack unlocked
//   ScenarioStart, ScenarioEnd - Scenario management

// ── 11. GAME MANAGEMENT (Host-Authoritative) ─────────────────
// Time Control:
//   TIME_PAUSE, TIME_UNPAUSE, TIME_SPEED_CHANGE
//   NewDay - Day progression
//
// Progression:
//   PlayerGainedLevel - Player level up
//   GameStatisticsChanged - Stats update
//
// Save/Load:
//   ModeInGame::Save, ModeInGame::LoadBinary
//   LOAD BEGINS, LOAD ENDS

// ── 12. NON-SYNCABLE (View/UI only) ─────────────────────────
// These do NOT modify game state:
//   ViewShardChanged, ViewWorldMap, ViewDungeon
//   TextEdit_Focus, TextEdit_Unfocus
//   Cursor position updates
//   Menu/loading UI states

// ═══════════════════════════════════════════════════════════════════════
// SYNC STRATEGY:
//
// ALL actions above trigger an autosave. Our sync approach:
// 1. Host performs action → game autosaves → zlib hook captures state
// 2. SaveStateTracker detects CRC32 change
// 3. GameAPI logs the action
// 4. Host streams new state to joiners via Lobby Data
// 5. Joiners receive, write .vrz, and load
//
// This is a FULL STATE SYNC approach. Individual action replay
// (placing a specific building at specific coords) would require
// reverse-engineering each action's memory layout, which is not
// feasible without source code access.
//
// The autosave-based approach guarantees 100% state consistency
// at the cost of higher bandwidth and ~1-30 second sync delay
// (depending on save size).
// ═══════════════════════════════════════════════════════════════════════

} // namespace mt2mp::game

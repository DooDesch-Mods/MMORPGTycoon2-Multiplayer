MT2 Multiplayer Mod
===================

Cooperative multiplayer mod for MMORPG Tycoon 2.
Both players need to own the game on Steam.

INSTALLATION
------------
1. Run install.bat (right-click -> Run as Administrator if needed)
   OR manually:
   - Copy steam_api64.dll and MT2_Multiplayer.dll to your game folder
   - Rename the original steam_api64.dll to steam_api64_original.dll

USAGE
-----
1. Launch MMORPG Tycoon 2 via Steam
2. Press F10 to toggle the Multiplayer overlay
3. System mouse cursor appears when overlay is open

TO HOST:
  - Go to "Host" tab
  - Click "Create Lobby"
  - Invite friends via Steam overlay, or share your Steam ID

TO JOIN:
  - Accept a Steam invite, OR
  - Go to "Join" tab, enter the host's Steam ID
  - Click "Connect"

FEATURES (current)
------------------
- Steam Lobby create/join (friends-only, invite via Steam overlay)
- Real-time cursor sync (see your friend's cursor as a colored triangle)
- Steam P2P transport via ISteamNetworkingSockets (goes through Steam relay — no port forwarding needed)
- Hardware-breakpoint command interception:
  - Captures every game action (Place, Destroy, Move) in real-time
  - Works around MinGW multi-entry-point compilation issue
  - No code patching, zero crash risk
- Command broadcasting to peers with action type + world position
- ImGui overlay with "Guild Ledger" theme (F10 to toggle)

IN PROGRESS
-----------
- Command replay on receiver side (host-authoritative sync)
- Full bidirectional placement sync between all peers
- Support for all action types (Terrain, Paths, Zones, Dungeons, etc.)

UNINSTALL
---------
Run uninstall.bat, or verify game files in Steam.

TROUBLESHOOTING
---------------
- Game doesn't start: Check steam_api64_original.dll exists in game folder
- No F10 menu: Check mt2mp.log in game folder for errors
- Can't connect: Both players must have the mod AND be Steam friends

FILES
-----
steam_api64.dll         - Proxy DLL (forwards all calls to original + loads mod)
MT2_Multiplayer.dll     - The mod itself (hooks, networking, overlay)
steam_api64_original.dll - Original Steam DLL (created during install)
mt2mp.log               - Debug log (created at runtime)
mt2mp_hwbp.log          - Hardware breakpoint capture log (diagnostic)

CREDITS
-------
Built using:
- MinHook (API hooking)
- Dear ImGui (overlay UI)
- Steam SDK (networking + lobbies)
- VectorStorm engine knowledge (from public source code)

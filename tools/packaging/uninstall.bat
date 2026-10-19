@echo off
echo ============================================
echo  MT2 Multiplayer Mod - Uninstaller
echo ============================================
echo.
echo This will restore the original game files.
echo Alternatively, right-click the game in Steam
echo and select "Verify integrity of game files".
echo.
pause

set "GAMEDIR=%~dp0"
if not exist "%GAMEDIR%MT2.exe" (
    for %%P in (
        "C:\Program Files (x86)\Steam\steamapps\common\MMORPG Tycoon 2"
        "C:\Program Files\Steam\steamapps\common\MMORPG Tycoon 2"
        "C:\SteamLibrary\steamapps\common\MMORPG Tycoon 2"
        "C:\SteamLibrary\steamapps\common\MMORPG Tycoon 2"
        "D:\Steam\steamapps\common\MMORPG Tycoon 2"
        "D:\SteamLibrary\steamapps\common\MMORPG Tycoon 2"
        "E:\SteamLibrary\steamapps\common\MMORPG Tycoon 2"
        "F:\Launcher\SteamLibrary\steamapps\common\MMORPG Tycoon 2"
    ) do (
        if exist "%%~P\MT2.exe" set "GAMEDIR=%%~P\"
    )
)

if exist "%GAMEDIR%steam_api64_original.dll" (
    del "%GAMEDIR%steam_api64.dll"
    rename "%GAMEDIR%steam_api64_original.dll" "steam_api64.dll"
    echo Restored original steam_api64.dll
)

if exist "%GAMEDIR%MT2_Multiplayer.dll" (
    del "%GAMEDIR%MT2_Multiplayer.dll"
    echo Removed MT2_Multiplayer.dll
)

if exist "%GAMEDIR%mt2mp.log" del "%GAMEDIR%mt2mp.log"

echo.
echo Uninstall complete!
pause

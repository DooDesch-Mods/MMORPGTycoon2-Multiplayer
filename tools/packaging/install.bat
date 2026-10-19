@echo off
echo ============================================
echo  MT2 Multiplayer Mod - Installer
echo ============================================
echo.

:: Find game directory
set "GAMEDIR=%~dp0"

:: Check if we're already in the game directory
if exist "%GAMEDIR%MT2.exe" (
    echo Game found in current directory.
    goto :install
)

:: Try common Steam paths
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
    if exist "%%~P\MT2.exe" (
        set "GAMEDIR=%%~P\"
        echo Game found at: %%~P
        goto :install
    )
)

echo ERROR: MMORPG Tycoon 2 not found!
echo Please copy these files to your game directory manually:
echo   - steam_api64.dll (replaces original)
echo   - MT2_Multiplayer.dll (new file)
echo And rename the original steam_api64.dll to steam_api64_original.dll
pause
exit /b 1

:install
echo.
echo Game directory: %GAMEDIR%
echo.

:: Backup original steam_api64.dll if not already done
if not exist "%GAMEDIR%steam_api64_original.dll" (
    echo Backing up original steam_api64.dll...
    copy "%GAMEDIR%steam_api64.dll" "%GAMEDIR%steam_api64_original.dll"
    if errorlevel 1 (
        echo ERROR: Failed to backup original DLL!
        pause
        exit /b 1
    )
    echo Backup created: steam_api64_original.dll
) else (
    echo Original backup already exists, skipping.
)

:: Copy mod files
echo Installing mod files...
copy /Y "%~dp0steam_api64.dll" "%GAMEDIR%steam_api64.dll"
copy /Y "%~dp0MT2_Multiplayer.dll" "%GAMEDIR%MT2_Multiplayer.dll"

echo.
echo ============================================
echo  Installation complete!
echo ============================================
echo.
echo HOW TO USE:
echo   1. Launch MMORPG Tycoon 2 normally via Steam
echo   2. Press F10 to open the Multiplayer menu
echo   3. To HOST: Click "Create Lobby" in the Host tab
echo   4. To JOIN: Enter the host's Steam ID in the Join tab
echo   5. Press F10 again to close the menu
echo.
echo TO UNINSTALL:
echo   Delete MT2_Multiplayer.dll from the game folder
echo   Rename steam_api64_original.dll back to steam_api64.dll
echo   (or verify game files in Steam)
echo.
pause

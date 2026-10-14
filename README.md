# MMORPG Tycoon 2 Multiplayer

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

Experimenteller Koop-Mod für die Windows-Version von MMORPG Tycoon 2. Dieses private Repository enthält den Mod-Quellcode und seine Build-Werkzeuge. Es ist kein freigegebener Release. Lokale Abnahmen einzelner Aktionen ersetzen keine vollständige Online-/Steam-Abnahme.

Enthalten ist der Quellcode für die Mod-DLL, den Steam-API-Proxy, das lokale Relay und die eigene native Multiplayer-Oberfläche. Spielinstallation, binäre DLLs, Saves, persönliche Konfigurationen und native Testartefakte sind nicht enthalten.

## Bauen

Voraussetzungen: Windows x64, Visual Studio 2022 mit C++/Windows SDK/MASM, CMake und Python 3. Für den Mod-Build wird eine zur enthaltenen SDL3-Version 3.2.24 passende Importbibliothek benötigt. CI baut diese aus der gepinnten offiziellen SDL-Quelle; eine DLL aus dem Spiel wird dafür nicht verwendet.

```powershell
cmake -S ci -B build/unit -A x64
cmake --build build/unit --config Release --parallel 2
ctest --test-dir build/unit -C Release --output-on-failure
python -m unittest discover -s test -p test_build_input_guard.py
```

Der Workflow [.github/workflows/windows.yml](.github/workflows/windows.yml) prüft die vorhandenen spielunabhängigen Tests und baut anschließend `MT2_Multiplayer`, `steam_api64` und `mt2mp_sidecar`. Er startet kein Spiel und lädt die Mod-DLL nicht in einen Testprozess. Ein grüner Build ist kein nativer Gameplay-Beleg.

Nach Bereitstellung der SDL3-Importbibliothek bei `include/vendor/SDL3/lib/x64/SDL3.lib`:

```powershell
cmake -S . -B build/mod -A x64 -DMT2MP_BUILD_TESTS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
cmake --build build/mod --config Release --target MT2_Multiplayer steam_api64 mt2mp_sidecar --parallel 2
```

Die separaten CI-Tests vermeiden die bisherige pauschale Bindung aller CMake-Testziele an die Mod-DLL. Die Launcher- und MCP-Werkzeuge benötigen eine eigene legale Spielinstallation und explizite lokale Pfade. Sie werden nicht vom Workflow ausgeführt.

## Abhängigkeiten

Siehe [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) für MinHook, Dear ImGui, nlohmann/json und SDL. Die Steam-Typdefinitionen sind ein kleiner lokaler Interface-Stub; das Steamworks SDK und Original-Steam-DLLs sind nicht enthalten.

Der Workflow hat nur Leserechte auf Repository-Inhalte. Er erstellt keine Releases, lädt keine Pakete hoch und veröffentlicht keine Spiel- oder Mod-Binärdateien. Für Mod-eigene Quellen wird durch diesen initialen Import keine neue Open-Source-Lizenz festgelegt.

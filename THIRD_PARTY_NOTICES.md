# Third-party notices

| Component | Included material | License and retained notice |
| --- | --- | --- |
| MinHook 1.3.3 | Required C sources, headers and CMake files | BSD-style notices, including Hacker Disassembler Engine: `include/vendor/minhook/LICENSE.txt` |
| Dear ImGui | Core C++ files and SDL3/OpenGL3 backends | MIT: `include/vendor/imgui/LICENSE.txt`; embedded imstb notices remain in their headers |
| nlohmann/json 3.12.0 | `include/vendor/nlohmann/json.hpp` | MIT: `include/vendor/nlohmann/LICENSE.MIT`; SPDX/copyright in the header retained |
| SDL 3.2.24 | Headers; CI separately builds the official pinned source | zlib: `include/vendor/SDL3/LICENSE.txt` and header notices |

SDL source used by CI is pinned to official commit `a8589a84226a6202831a3d49ff4edda4acab9acd` (`release-3.2.24`): https://github.com/libsdl-org/SDL/tree/a8589a84226a6202831a3d49ff4edda4acab9acd

The small `include/vendor/steam/steam_api.h` contains local minimal scalar/interface declarations. No Valve SDK bundle, Steam client or original `steam_api64.dll` is included. Steam and MMORPG Tycoon 2 names identify the required external products and do not imply endorsement.

The mod contains executable-version-specific native ABI constants and guard tables required by its adapters. Game executables, decompilation outputs, disassembly dumps, game assets and saves are excluded. These guard tables do not grant rights to redistribute the underlying game. This file does not set a new license for mod-authored code.

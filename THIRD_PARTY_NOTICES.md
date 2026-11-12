# Third-party notices

| Component | Included material | License and retained notice |
| --- | --- | --- |
| MinHook 1.3.3 | Required C sources, headers and CMake files, taken from the pinned Framework SDK | BSD-style notices, including Hacker Disassembler Engine: Framework SDK `third_party/vendor/minhook/LICENSE.txt` |
| nlohmann/json 3.12.0 | `json.hpp` from the pinned Framework SDK | MIT: Framework SDK `third_party/vendor/nlohmann/LICENSE.MIT`; SPDX/copyright in the header retained |
| SDL 3 | Headers and import library from the pinned Framework SDK | zlib: Framework SDK `third_party/vendor/SDL3/LICENSE.txt` and header notices |

The Framework SDK is the Framework tag pinned in `.github/framework-sdk.json`. Release packages include each license text under `licenses/`.

The small `include/vendor/steam/steam_api.h` contains local minimal scalar/interface declarations. No Valve SDK bundle, Steam client or original `steam_api64.dll` is included. Steam and MMORPG Tycoon 2 names identify the required external products and do not imply endorsement.

The mod contains executable-version-specific native ABI constants and guard tables required by its adapters. Game executables, decompilation outputs, disassembly dumps, game assets and saves are excluded. These guard tables do not grant rights to redistribute the underlying game. This file does not set a new license for mod-authored code.

# Build from source

Building requires Windows x64, Visual Studio 2022 with C++, the Windows SDK and MASM, CMake, and Python 3. The mod build needs an import library matching the included SDL 3.2.24 headers. CI builds that library from pinned official SDL source without using a game DLL.

```powershell
cmake -S ci -B build/unit -A x64
cmake --build build/unit --config Release --parallel 2
ctest --test-dir build/unit -C Release --output-on-failure --no-tests=error
python -m unittest discover -s test -p test_build_input_guard.py
python -m unittest discover -s test -p test_build_commands.py
```

The [Windows workflow](../.github/workflows/windows.yml) runs the game-independent tests and builds the native Framework plugin. It does not launch the game or load the plugin. A successful build does not prove gameplay behavior.

Once the SDL3 import library is available at `include/vendor/SDL3/lib/x64/SDL3.lib`:

```powershell
python -X utf8 tools/build.py --framework-sdk C:/path/to/ModFramework --target MT2MultiplayerPlugin
```

Current development source requires Framework Runtime 0.5.7 and its matching native SDK. Arrange the frozen SDK with its `include` and `adapters/mt2/sdk` directories. A mutable Framework checkout and the Lua authoring SDK are not required. The build writes `build/framework/bin/Release/MT2MultiplayerPlugin.dll` and does not build a second loader or MinHook runtime. Source, build recipe, assembly and consumed SDK headers are checked for changes during compilation.

The Framework version defaults to `mod.json`; an explicit version must match it. A matching CMake cache skips the redundant configure step while retaining CMake's automatic regeneration. Use `--reconfigure` to configure explicitly. The build writes `build/framework/build-receipt.json` with the exact DLL hash and version.

For a local DLL iteration, add `--native-only`. This selects only `MT2MultiplayerPlugin` and disables source-test configuration in the Framework build directory. Run affected source tests separately in the dedicated `build/unit` tree above. Default builds configure tests again; the cache match includes this setting. The receipt records `tests_configured` and still requires native game acceptance.

Package that local plugin with the canonical manifest:

```powershell
python -X utf8 tools/package.py --build build/framework/bin/Release --freeze build/framework/build-receipt.json --multiplayer-package . --output artifacts/Multiplayer-local.zip
```

Packaging reads back every ZIP entry and rejects a changed DLL or mismatched manifest version. Keep a copy of a package before the next build if you need its exact bytes later.

The separate CI test entry avoids linking every test to the plugin DLL. Launcher and MCP tools require a legally installed copy of the game and explicit local runtime paths. CI does not run these tools.


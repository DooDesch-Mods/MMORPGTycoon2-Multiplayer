# Build from source

Building requires Windows x64, Visual Studio 2022 with C++, the Windows SDK and MASM, CMake, and Python 3. `tools/build.py` finds CMake on PATH or in the Visual Studio installation.

The build needs a checkout of the Framework tag in [`.github/framework-sdk.json`](../.github/framework-sdk.json). That tag is the only Framework pin: CMake checks the checkout against it and takes the minimum Framework Runtime version from it. The SDK also supplies MinHook, nlohmann/json and the SDL3 headers and import library.

```powershell
git clone --depth 1 --branch <tag> https://github.com/DooDesch-Mods/MMORPGTycoon2-ModFramework build/framework-sdk
```

## Tests

```powershell
cmake -S ci -B build/unit -A x64 -DMT2_FRAMEWORK_SDK=build/framework-sdk
cmake --build build/unit --config Release --parallel
ctest --test-dir build/unit -C Release --output-on-failure --no-tests=error -j 8
python -m pip install -r tools/mcp/requirements.txt
$env:PYTHONPATH = "$PWD/tools/mcp"
foreach ($f in Get-ChildItem test, tools -Recurse -File -Filter 'test_*.py') {
  if (Select-String -Quiet -SimpleMatch 'TestCase' -LiteralPath $f.FullName) {
    python -X utf8 -m unittest discover -s $f.DirectoryName -p $f.Name
  } else {
    python -X utf8 $f.FullName
  }
}
```

Each Python test file runs in its own process; the `test/mcp` modules share module state and fail when loaded together.

The [Windows workflow](../.github/workflows/windows.yml) runs the same commands in one job, builds the native Framework plugin and packages the release ZIP. It does not launch the game or load the plugin. A successful build does not prove gameplay behavior.

## Plugin

```powershell
python -X utf8 tools/build.py --framework-sdk build/framework-sdk --target MT2MultiplayerPlugin
```

The build writes `build/framework/bin/Release/MT2MultiplayerPlugin.dll` and does not build a second loader or MinHook runtime. Source, build recipe, assembly and consumed SDK headers are checked for changes during compilation.

The version comes from `mod.json`. A matching CMake cache skips the redundant configure step while retaining CMake's automatic regeneration. Use `--reconfigure` to configure explicitly. The build writes `build/framework/build-receipt.json` with the exact DLL hash and version.

For a local DLL iteration, add `--native-only`. This selects only `MT2MultiplayerPlugin` and disables source-test configuration in the Framework build directory. Run affected source tests separately in the dedicated `build/unit` tree above. Default builds configure tests again; the cache match includes this setting. The receipt records `tests_configured` and still requires native game acceptance.

Package that local plugin with the canonical manifest:

```powershell
python -X utf8 tools/package.py --build build/framework/bin/Release --freeze build/framework/build-receipt.json --multiplayer-package . --framework-sdk build/framework-sdk --output artifacts/Multiplayer-local.zip
```

Packaging reads back every ZIP entry and rejects a changed DLL or mismatched manifest version. Keep a copy of a package before the next build if you need its exact bytes later.

The separate CI test entry avoids linking every test to the plugin DLL. Launcher and MCP tools require a legally installed copy of the game and explicit local runtime paths. CI does not run these tools.

## Release

1. Set the new version in `mod.json` and add a `## <version>` section to `CHANGELOG.md`.
2. Merge to `main`, then push the tag `v<version>` on that commit.
3. [`release.yml`](../.github/workflows/release.yml) builds and packages through the Windows workflow, creates the GitHub release with the changelog section as notes, and uploads the ZIP to Nexus file 8177136.

The Nexus page text in [`listing/`](../listing) is updated by hand on Nexus, never from CI.

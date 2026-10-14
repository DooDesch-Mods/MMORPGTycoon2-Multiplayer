# Private beta builds

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

Beta builds are tester prereleases in the private [GitHub repository](https://github.com/DooDesch/MMORPGTycoon2-Multiplayer/releases). Only people with repository access can download them. The repository remains private.

## Test the current beta

1. Download the Windows x64 ZIP and its `.sha256` file from the selected prerelease.
2. Compare the ZIP hash with `Get-FileHash <downloaded-zip> -Algorithm SHA256`.
3. Extract the complete ZIP into a separate folder outside the game installation, keeping `_internal` beside `MT2MP-Start.exe`.
4. Start Steam with your own account, then run `MT2MP-Start.exe` and follow the included `README.txt`.

Each player needs a legal installation of the supported game build. The starter copies required files locally from that installation; the download contains no game executable, original Steam library, save or personal configuration. Keep the test directory and its saves when updating.

This beta uses the current isolated test starter. It is not the planned drop-in installation. Separate-account online joining, world transfer and complete gameplay still need native acceptance. CI compiles and checks source and package contents. Its `--help` starter check does not launch the game and does not establish native gameplay success.

## Publish the next beta

The maintainer commits the intended source, UI, starter and notices first, with a matching CMake base version. After the required native checks, create and push an unused tag such as `v0.1.0-beta.1`. Tags matching `vX.Y.Z-beta.N` trigger the [private beta workflow](../.github/workflows/beta.yml).

The workflow runs the six current CTest targets and Python source and packaging checks, builds pinned SDL3 and the mod, builds the Python 3.13 starter with PyInstaller 6.16.0, and validates the ZIP against `package.json`. It publishes a prerelease only after those jobs pass. Only the release job has repository write permission.

The package manifest binds the beta tag and full source commit, required game hash and every payload file hash. A separate checksum covers the ZIP. Releases remain prereleases and are not marked as the latest stable version.

Identical existing assets are retained on a repeated publish. Different existing bytes fail without overwriting the release. A fresh rebuild can change binary or ZIP timestamps; use a new beta tag for a replacement build. If a publication failed after an upload, inspect the draft and failed job before retrying. The workflow does not silently delete or replace assets.

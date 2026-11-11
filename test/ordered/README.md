# Ordered core source checks

> 🛟 **Need help or found a bug?** Get support at [support.doodesch.de/multiplayer](https://support.doodesch.de/multiplayer).

These targets run in the existing Windows source-test shards. They compile the current production queue, wire and controller through the shared codec archive. The barrier and proof-owner tests include their production headers directly. The bootstrap extractor reads the current `src/net/ordered_snapshot_load.inc` at CMake configure time; changes to that input trigger reconfiguration. Only diagnostic file export and native/transport ports are doubled. The real pinned Framework SDK supplies the ABI.

| CTest name | Contract |
| --- | --- |
| `ordered_queue` | Global order, immutable dedupe, bounded queued/in-flight work, physical peer receipts, current execution capability, failure without replay |
| `ordered_wire` | Packet roundtrips and malformed input, authenticated sender/owner, byte limits, backpressure before assignment, existing gameplay codec |
| `ordered_controller` | Existing submission path, one channel owner, peer consumption and exact completion, time commands, transport/native failure, timeout, retirement readiness |
| `ordered_snapshot_barrier` | Equal prepared witnesses, exactly one capture, file/companion confirmation, all-peer release, conflicting identities, malformed snapshot-control packets |
| `ordered_bootstrap` | Once-only initial covered witness, held zero-step owner readback, physical membership agreement, duplicates, mismatch/timeout before activation |
| `ordered_lifecycle` | Registered proof identity, actual unbind acknowledgment, BUSY/invalid receipt retention, no callback after retirement, independent second owner |

Configure the existing `ci` project with `MT2_FRAMEWORK_SDK` pointing at a checkout of the Framework tag in `.github/framework-sdk.json`. A missing SDK is a configure error. The workflow checks out that tag for source tests as it does for the plugin build.

After building the source tests, run `ctest --test-dir <build> -C Release -L ordered --output-on-failure` for this group. Release builds keep bootstrap assertions enabled. The existing source-shard collector registers every executable and verifies complete coverage; no separate test runner is required.

These are protocol and ownership regressions. They do not launch the game or replace original-engine acceptance of Save, Quit, world loading or individual ordered actions.

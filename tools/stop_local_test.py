"""Stop only exact process identities owned by the local test manifest."""
import argparse
import os
from pathlib import Path
import sys

from local_test import ROOT, LaunchError, RuntimeLock, WindowsProcesses, log, stop_manifest


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--test-root", type=Path, default=ROOT / "_MPTest")
    args = parser.parse_args(argv)
    try:
        processes = WindowsProcesses()
        with RuntimeLock(args.test_root):
            return stop_manifest(Path(os.path.abspath(args.test_root)), processes)
    except (LaunchError, OSError, ValueError, KeyError) as error:
        log("error", f"stop local test failed: {error}")
        return 1


if __name__ == "__main__":
    sys.exit(main())

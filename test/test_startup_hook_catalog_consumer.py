"""Check local normal-session hook admission without a Framework SDK."""
import argparse
from pathlib import Path
import re
import unittest
from test_startup_hook_catalog import rows, startup_targets

SOURCE = Path(__file__).resolve().parents[1]
CATALOG = None


class ConsumerStartupHookTests(unittest.TestCase):
    def setUp(self):
        self.consumer = rows((CATALOG or SOURCE / "src/hooks/framework_catalog.inc").read_text(encoding="utf-8"))
        targets, _ = startup_targets(SOURCE)
        self.targets = {rva for values in targets.values() for rva in values}
        lifetime = (SOURCE / "src/game/toon_thought_lifetime.inc").read_text(encoding="utf-8")
        match = re.search(r"if\(body\.rva==(0x[0-9a-f]+)\)destroy_owner=", lifetime)
        self.assertIsNotNone(match, "Thought lifetime owner target must be extracted")
        self.thought = int(match[1], 16)
        self.assertIn("MH_CreateHook(reinterpret_cast<void*>(destroy_owner.entry)", lifetime)

    def test_capture_startup_targets_have_private_consumer_routes(self):
        missing = self.targets - self.consumer.keys()
        self.assertFalse(missing, "Missing startup consumer routes: " + ", ".join(map(hex, sorted(missing))))
        shared = {rva for rva in self.targets if self.consumer[rva][1]}
        self.assertFalse(shared, "Direct startup hooks cannot use shared routes: " + ", ".join(map(hex, sorted(shared))))

    def test_thought_lifetime_target_has_private_consumer_route(self):
        self.assertTrue(self.thought in self.consumer, "Missing Thought D1 consumer route: " + hex(self.thought))
        self.assertFalse(self.consumer[self.thought][1])

    def test_historical_startup_omissions_are_detected(self):
        required = self.targets | {self.thought}
        for rva in (0x585270, 0x5854E0, 0x5BA2C0, self.thought):
            with self.subTest(rva=hex(rva)):
                self.assertIn(rva, required)
                incomplete = dict(self.consumer)
                incomplete.pop(rva, None)
                self.assertIn(rva, required - incomplete.keys())


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=SOURCE)
    parser.add_argument("--catalog", type=Path)
    options, remaining = parser.parse_known_args()
    SOURCE, CATALOG = options.source_root, options.catalog
    unittest.main(argv=[__file__] + remaining)

"""Check normal-session capture hook admission against both compiled catalogs.

Run with --source-root <Multiplayer source> --framework-root <matching SDK>.
The catalog can be overridden with --catalog for an isolated staged candidate.
"""
import argparse
import json
from pathlib import Path
import re
import unittest

SOURCE = Path(__file__).resolve().parents[1]
FRAMEWORK = None
CATALOG = None
HEX = r"(0x[0-9a-f]+)"
CREATE = r"(?:MH_CreateHook|hooks::create_hook)\s*\("
DIRECT = CREATE + r"reinterpret_cast<void\*>\([^)\n]*\+" + HEX + r"\)"


def rows(text, provider=False):
    pattern = (r'catalog\.catalog\("([^"]+)",(0x[0-9a-f]+),(true|false)\);'
               if provider else r'\{"([^"]+)",(0x[0-9a-f]+),(true|false)\}')
    result = [(name, int(rva, 16), shared == "true")
              for name, rva, shared in re.findall(pattern, text)]
    assert result and len({row[0] for row in result}) == len(result), "Catalog names absent or duplicated"
    assert len({row[1] for row in result}) == len(result), "Catalog RVA aliases"
    return {rva: (name, shared) for name, rva, shared in result}


def startup_targets(source):
    """Read the hook arguments from the capture installers reached by start_runtime."""
    targets = {}
    observed = {}

    def text(name):
        value = (source / "src/game" / name).read_text(encoding="utf-8")
        observed[name] = value
        return value

    def add(name, values):
        assert values, "Startup target extraction is empty: " + name
        targets[name] = sorted(set(int(value, 16) for value in values))

    direct = {
        "ui_move_runtime.cpp": 1,
        "building_uptime_native.inc": 1,
        "ui_region_runtime.cpp": 1,
        "region_styles_runtime.inc": 1,
        "region_properties_runtime.inc": 3,
        "region_level_runtime.inc": 1,
        "ui_terrain_height.inc": 1,
        "boss_phase_runtime.inc": 4,
        "building_history_slice/native_bridge.cpp": 2,
    }
    for name, count in direct.items():
        value = text(name)
        found = re.findall(DIRECT, value)
        assert len(found) == len(re.findall(CREATE, value)) == count, "Unmapped direct startup hook: " + name
        add(name, found)

    tables = (
        "ui_terrain_runtime.inc", "path_execution.inc", "ui_network_runtime.inc",
        "ui_network_pipe.inc", "group_enable.inc", "district_text_runtime.inc",
        "prop_text_runtime.inc", "service_membership_runtime.inc",
    )
    for name in tables:
        value = text(name)
        assert len(re.findall(CREATE, value)) == 1, "Unmapped table startup hook: " + name
        table = re.search(r"const Hook hooks\[\]\s*(?:=)?\s*\{(.*?)\};", value, re.S)
        assert table, "Startup hook table absent: " + name
        found = re.findall(r"\{\s*" + HEX + r",\s*reinterpret_cast<void\*>\(&", table[1])
        assert len(found) == table[1].count("reinterpret_cast<void*>(&"), "Unmapped table target: " + name
        add(name, found)

    value = text("ui_build_runtime.cpp")
    assert len(re.findall(CREATE, value)) == 1
    found = re.findall(r"!install\(" + HEX + r",", value)
    assert len(found) == value.count("!install("), "Unmapped Build installer target"
    # Framework CreateBuilding uses its typed shared observer, not a private hook.
    assert "#ifndef MT2MP_FRAMEWORK_PLUGIN\n       !install(0x24ed30," in value
    add("ui_build_runtime.cpp", [rva for rva in found if int(rva, 16) != 0x24ED30])

    for owner, pins in (
        ("district_edit_routes.inc", "district_edit_route_pins.inc"),
        ("district_stroke_routes.inc", "district_stroke_route_pins.inc"),
    ):
        value = text(owner)
        assert len(re.findall(CREATE, value)) == 1 and '#include "' + pins + '"' in value
        add(pins, re.findall(r"^\s*\{" + HEX + ",", text(pins), re.M))

    value = text("district_title_runtime.inc")
    assert len(re.findall(CREATE, value)) == 1
    add("district_title_runtime.inc", re.findall(r"const auto target=reinterpret_cast<void\*>\(r->image\+" + HEX, value))

    value = text("modular_capture.inc")
    assert len(re.findall(CREATE, value)) == 1
    sequence = re.search(r"rvas\{([^}]+)\},detours", value)
    assert sequence
    add("modular_capture.inc", re.findall(HEX, sequence[1]))

    value = text("scenery_stroke_routes.inc")
    assert len(re.findall(CREATE, value)) == 1 and "scenery_stroke_code::routes[index].rva" in value
    code = text("scenery_stroke_code.h")
    sequence = re.search(r"Guard routes\[\]\{(.*?)\};", code, re.S)
    assert sequence
    add("scenery_stroke_code.h", re.findall(r"\{" + HEX + ",", sequence[1]))

    value = text("boss_placement_routes.inc")
    assert len(re.findall(CREATE, value)) == 1 and "r->image+boss_placement_reader::release_rva" in value
    add("boss_placement_reader.h", re.findall(r"release_rva=" + HEX, text("boss_placement_reader.h")))

    value = text("world_path_capture.inc")
    assert len(re.findall(CREATE, value)) == 1
    sequence = re.search(r"guards\{\{(.*?)\}\};", value, re.S)
    assert sequence
    add("world_path_capture.inc", re.findall(r"\{" + HEX + r",\{", sequence[1]))

    value = text("object_identity/remove_runtime/remove_runtime.cpp")
    assert len(re.findall(CREATE, value)) == 1
    assert "installation_.targets={uintptr_t(entries_.clear),uintptr_t(entries_.open),uintptr_t(entries_.lock),uintptr_t(entries_.prop_destructor)}" in value
    contracts = text("object_identity/remove_runtime/copy/remove_native_contract.cpp")
    found = re.findall(r"reinterpret_cast<(?:ClearSelectionFn|UndoOpenFn|GraphLockFn|PropDestructorFn)>\(image\+" + HEX, contracts)
    assert len(found) == 4
    add("object_identity/remove_runtime/copy/remove_native_contract.cpp", found)
    value = text("object_identity/native_free_runtime/native_free_runtime.cpp")
    assert len(re.findall(CREATE, value)) == 1 and "receipt_.sized_target=receipt_.image+sized_rva" in value
    add("object_identity/native_free_runtime/native_free_runtime.cpp", re.findall(r"constexpr uintptr_t sized_rva=" + HEX, value))

    # The outer startup order and its move helpers delimit this coverage scope.
    flow = text("prepare_flow.cpp")
    capture = flow[flow.index("bool start_runtime("):flow.index("bool completed(")]
    assert re.findall(r"ui_(\w+)_runtime::enable\(error\)", capture) == [
        "build", "remove", "move", "region", "terrain", "path", "network"]
    assert "typed_action_runtime::start(error)" in capture
    assert "typed_action_runtime::enable_live_state(error)" in capture
    move = observed["ui_move_runtime.cpp"]
    move = move[move.index("bool enable(std::string& e)"):move.index("bool enabled()")]
    assert re.findall(r"([\w:]+)::enable\(e\)", move) == [
        "group_edit", "district_edit", "district_stroke", "district_title",
        "district_text", "prop_text", "boss_phase", "service_membership",
        "scenery_stroke", "modular", "boss_placement"]
    return targets, observed


class StartupHookCatalogTests(unittest.TestCase):
    def setUp(self):
        catalog = CATALOG or SOURCE / "src/hooks/framework_catalog.inc"
        self.consumer = rows(catalog.read_text(encoding="utf-8"))
        if FRAMEWORK is None:
            self.fail("Pass --framework-root to bind the matching provider catalog")
        self.provider = rows((FRAMEWORK / "adapters/mt2/multiplayer_catalog.inc").read_text(encoding="utf-8"), True)

    def test_compiled_consumer_provider_catalogs_agree(self):
        absent = self.provider.keys() - self.consumer.keys()
        extra = self.consumer.keys() - self.provider.keys()
        changed = {rva for rva in self.consumer.keys() & self.provider.keys()
                   if self.consumer[rva] != self.provider[rva]}
        self.assertFalse(absent or extra or changed,
                         f"Catalog mismatch missing={list(map(hex, sorted(absent)))} "
                         f"extra={list(map(hex, sorted(extra)))} changed={list(map(hex, sorted(changed)))}")
        metadata = json.loads((FRAMEWORK / "adapters/mt2/multiplayer-catalog.json").read_text(encoding="utf-8"))
        self.assertEqual(self.provider, {row["rva"]: (row["name"], row["shared"]) for row in metadata["targets"]})

    def test_actual_capture_startup_targets_are_private_named_routes(self):
        targets, _ = startup_targets(SOURCE)
        for source, rvas in targets.items():
            for rva in rvas:
                with self.subTest(source=source, rva=hex(rva)):
                    self.assertTrue(rva in self.consumer, "Consumer startup target missing: " + hex(rva))
                    self.assertTrue(rva in self.provider, "Provider startup target missing: " + hex(rva))
                    self.assertEqual(self.consumer[rva], self.provider[rva])
                    self.assertFalse(self.consumer[rva][1], "Direct startup hook cannot use a shared route")

    def test_cursor_omission_is_detected_before_native_startup(self):
        targets, _ = startup_targets(SOURCE)
        for rva in (0x5854E0, 0x585270):
            self.assertIn(rva, targets["ui_build_runtime.cpp"])
            incomplete = dict(self.provider)
            incomplete.pop(rva)
            absent = {target for values in targets.values() for target in values} - incomplete.keys()
            self.assertEqual(absent, {rva})


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, default=SOURCE)
    parser.add_argument("--framework-root", type=Path, required=True)
    parser.add_argument("--catalog", type=Path)
    options, remaining = parser.parse_known_args()
    SOURCE, FRAMEWORK, CATALOG = options.source_root, options.framework_root, options.catalog
    unittest.main(argv=[__file__] + remaining)

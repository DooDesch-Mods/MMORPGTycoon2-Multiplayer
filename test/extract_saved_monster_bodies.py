"""Extract complete affected production bodies for bounded fixtures."""
from pathlib import Path
import argparse
import hashlib
import json

p = argparse.ArgumentParser()
p.add_argument('source', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)
rows = {}
for source, begin, end, output in [
    ('src/game/npc_quest_target_runtime.cpp', 'template<class Left,class Right>bool same_world(', 'void reset(', 'quest_same_world_actual.inc'),
    ('src/game/npc_quest_target_runtime.cpp', 'bool validate_scene(', '\nbool current(', 'dungeon_quest_seal_actual.inc'),
    ('src/game/monster_saved_identity_runtime.cpp', 'void reset(', '\nbool context(', 'saved_monster_reset_actual.inc'),
    ('src/game/monster_saved_identity_runtime.cpp', 'bool arm_client(', '\nvoid cancel(', 'saved_monster_load_finish_actual.inc'),
    ('src/game/monster_saved_identity_runtime.cpp', 'bool prepare_load(', '\nbool write_companion(', 'saved_monster_prepare_actual.inc'),
    ('src/game/network_object_identity_runtime.inc', 'bool prepare_aliases(', '\nbool snapshot_aliases(', 'saved_network_original_load_actual.inc'),
    ('src/game/network_identity.cpp', 'bool begin_client_load(', '\nvoid observe_client_registered(', 'saved_network_begin_load_actual.inc'),
    ('src/game/network_identity.cpp', 'void invalidate_world(', '\nnlohmann::json status_json(', 'saved_network_invalidate_actual.inc'),

    ('src/game/monster_saved_identity_runtime.cpp', 'bool seal_world(', '\n\n}\nbool stage_scenes(', 'saved_monster_seal_actual.inc'),
    ('src/net/snapshot_sync.cpp', 'static uint32_t monster_chunks(', 'static std::string incoming_world_name', 'saved_monster_manifest_actual.inc'),
    ('src/net/snapshot_sync.cpp', 'static void receive_chunk(', '\nvoid initialize_snapshot_sync', 'saved_monster_receive_actual.inc'),
]:
    data = (a.source / source).read_text(encoding='utf-8-sig')
    assert data.count(begin) == 1 and data.count(end) == 1, source
    body = data[data.index(begin):data.index(end)]
    target = a.output / output
    if not target.exists() or target.read_text(encoding='utf-8') != body:
        target.write_text(body, encoding='utf-8')
    rows[output] = {'source': source, 'sha256': hashlib.sha256(body.encode()).hexdigest()}
(a.output / 'bodies.json').write_text(json.dumps(rows, indent=2) + '\n')

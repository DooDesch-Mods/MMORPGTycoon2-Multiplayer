from pathlib import Path
import argparse

p = argparse.ArgumentParser()
p.add_argument('--root', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=True)

for name, relative in [('membership', 'character_membership_sample.inc'),
                       ('actor', 'character_actor_records_runtime.inc')]:
    source = (a.root / 'src/game' / relative).read_text(encoding='utf-8')
    start = source.index('bool inspect(void* user,')
    end = source.index('\n}', start) + 2
    body = source[start:end]
    scope = '    live_host_state::apply_timing::PageReadScope record_pages;\n'
    assert body.count(scope) == 1
    assert body.index(scope) < body.index('return subscriber_lifecycle::read_graph')
    assert body.count('read_graph(') == 1
    (a.output / (name + '_record_inspect.inc')).write_text(body + '\n', encoding='utf-8')
    if name == 'membership':
        (a.output / 'unscoped_record_inspect.inc').write_text(body.replace(scope, '') + '\n', encoding='utf-8')

capture = (a.root / 'src/game/subscriber_lifecycle/capture.cpp').read_text(encoding='utf-8')
assert capture.index('receipt.call_exception = save_seh(') < capture.index('receipt.call_exception = inspect_seh(')
assert capture.index('receipt.call_exception = inspect_seh(') < capture.index('receipt.record_cleanup_exception = destroy_seh(')
for relative in ['character_membership_sample.inc', 'character_actor_records_runtime.inc']:
    source = (a.root / 'src/game' / relative).read_text(encoding='utf-8')
    assert 'PageReadScope original_call(live_host_state::apply_timing::PageBoundary::OriginalCall);\n        if(!subscriber_lifecycle::capture_original(' in source

source = (a.root / 'src/game/character_membership_sample.inc').read_text(encoding='utf-8')
start = source.index('    {\n        live_host_state::apply_timing::PageReadScope region_pages(')
end = source.index('\n    }', start) + len('\n    }')
block = source[start:end]
assert block.count('region_saved_identity::capture(') == 1
assert block.count('region_saved_identity::verify(') == 1
assert block.count('owner_add(') == 1
assert block.index('capture(') < block.index('owner_add(') < block.index('verify(')
assert source[end:].lstrip().startswith('dungeon_scene_identity_runtime::Census scenes;')
(a.output / 'character_region_owners.inc').write_text(block + '\n', encoding='utf-8')

start = source.index('bool owner_add(')
end = source.index('\n}', start) + 2
(a.output / 'character_owner_add.inc').write_text(source[start:end] + '\n', encoding='utf-8')
start = source.index('    json props;')
end = source.index('\n    }\n    a::Context context;', start)
block = source[start:end]
assert block.count('remove_diagnostics::identity_bindings(') == 1
assert block.count('lookup(') == 1
assert block.count('owner_add(') == 1
(a.output / 'character_prop_owners.inc').write_text(block + '\n', encoding='utf-8')
(a.output / 'character_prop_owners_refund.inc').write_text(
    block.replace('remove_diagnostics::identity_bindings(', 'remove_diagnostics::bindings(') + '\n', encoding='utf-8')

catalog = (a.root / 'src/game/scenery_kiss/native_catalog.cpp').read_text(encoding='utf-8')
start = catalog.index('        {\n            mt2mp::game::live_host_state::apply_timing::PageReadScope owner_fields;')
end = catalog.index('\n        }', start) + len('\n        }')
block = catalog[start:end]
assert block.count('memory.copy(') == 2 and 'find(' not in block
(a.output / 'scenery_owner_fields.inc').write_text(block + '\n', encoding='utf-8')
(a.output / 'scenery_owner_fields_baseline.inc').write_text(
    block.replace('            mt2mp::game::live_host_state::apply_timing::PageReadScope owner_fields;\n', '') + '\n', encoding='utf-8')
start = catalog.index('    {mt2mp::game::live_host_state::apply_timing::PageReadScope lookup_fields;')
end = catalog.index('\n    uintptr_t object=0,ap=0;', start)
block = catalog[start:end]
assert block.count('host::resolve_lookup(') == 1 and 'find(' not in block
(a.output / 'scenery_lookup_fields.inc').write_text(block + '\n', encoding='utf-8')
(a.output / 'scenery_lookup_fields_baseline.inc').write_text(
    block.replace('mt2mp::game::live_host_state::apply_timing::PageReadScope lookup_fields;', '') + '\n', encoding='utf-8')

source = (a.root / 'src/game/general_project_runtime.cpp').read_text(encoding='utf-8')
start = source.index('template<class T>bool at(Runtime&')
end = source.index('\nuint32_t getter(', start)
(a.output / 'project_progress_readers.inc').write_text(source[start:end] + '\n', encoding='utf-8')
start = source.index('bool progress_once(')
end = source.index('\n}', start) + 2
body = source[start:end]
scope = '    live::apply_timing::PageReadScope progress_pages;\n'
assert body.count(scope) == 1 and body.count('context(') == 1
assert body.index('context(') < body.index(scope) < body.index('if(!at(')
for name, text in [('project_progress_actual', body), ('project_progress_baseline', body.replace(scope, ''))]:
    (a.output / (name + '.inc')).write_text(text + '\n', encoding='utf-8')

source = (a.root / 'src/game/gm_ticket_native.cpp').read_text(encoding='utf-8')
start = source.index('bool inspect(void* user,')
end = source.index('\n}', start) + 2
body = source[start:end]
scope = '        live_host_state::apply_timing::PageReadScope record_pages;\n'
assert body.count(scope) == 1 and body.count('c::read_graph(') == 1
assert body.index(scope) < body.index('c::read_graph(') < body.index('\n    }\n    return release_graph::capture_value(')
for name, text in [('gm_record_actual', body), ('gm_record_baseline', body.replace(scope, ''))]:
    (a.output / (name + '.inc')).write_text(text + '\n', encoding='utf-8')

source = (a.root / 'src/game/world_zone_current.inc').read_text(encoding='utf-8')
start = source.index('bool read_world_zones(')
start = source.index('    auto at=', start)
end = source.index('\n}', start)
body = source[start:end]
scope = '    mt2mp::game::live_host_state::apply_timing::PageReadScope owner_pages(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());\n'
assert body.count(scope) == 1
assert body.index('if(!capture_regions())') < body.index(scope) < body.index('for(const auto& [id,region]')
assert body.index('    }\n    opened::CurrentWorld after;') > body.index(scope)
assert 'PageReadScope zone_pages(!mt2mp::game::live_host_state::apply_timing::PageReadScope::reader_active());' in body
assert 'PageReadScope original_lookup(mt2mp::game::live_host_state::apply_timing::PageBoundary::OriginalCall);' in body
for name, text in [('world_zone_scope_actual', body), ('world_zone_scope_baseline', body.replace(scope, ''))]:
    (a.output / (name + '.inc')).write_text(text + '\n', encoding='utf-8')

source = (a.root / 'src/game/dungeon_scene_identity_runtime.cpp').read_text(encoding='utf-8')
start = source.index('static bool current_census(')
end = source.index('\n}', start) + 2
body = source[start:end]
scope = '        live_host_state::apply_timing::PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());\n'
assert body.count(scope) == 1
assert body.index(scope) < body.index('current_exteriors(') < body.index('read_census(') < body.index('    }\n    identity::Scope after;')
for name, text in [('dungeon_census_scope_actual', body), ('dungeon_census_scope_baseline', body.replace(scope, ''))]:
    (a.output / (name + '.inc')).write_text(text + '\n', encoding='utf-8')
start = source.index('    auto find=', source.index('static bool read_census('))
end = source.index('    std::map<uint64_t,std::string> exterior_records;', start)
callbacks = source[start:end]
assert callbacks.count('PageBoundary::OriginalCall') == 2
(a.output / 'dungeon_census_native_callbacks.inc').write_text(callbacks, encoding='utf-8')
for function in ['static bool read_census(', 'static bool current_exteriors(']:
    start = source.index(function)
    assert 'PageReadScope census_pages(!live_host_state::apply_timing::PageReadScope::reader_active());' in source[start:source.index('\n}', start)]
assert 'PageReadScope original_key(live_host_state::apply_timing::PageBoundary::OriginalCall);return remove_diagnostics::resolve_current_building_key(' in source

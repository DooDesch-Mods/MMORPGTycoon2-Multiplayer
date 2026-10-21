"""Extract the actual snapshot identity policy and owned observer boundaries."""
import argparse
from pathlib import Path
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source-root', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
source = (args.source_root / 'src/game/building_capture_identity.cpp').read_text()
core = (args.source_root / 'src/core/dll_main.cpp').read_text()
marker = 'if (capture_snapshot_identities) {'
if source.count(marker) != 3:
    raise RuntimeError('Expected exactly arm, seal and cancel snapshot identity boundaries')
blocks = []
start = 0
for function, called in [('begin', 'arm_host'), ('seal', 'seal_host'), ('cancel', 'cancel(reason)')]:
    start = source.index(marker, start)
    cursor = source.index('{', start)
    depth, end = 1, cursor + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    block = source[start:end]
    if ('npc_quest_target_runtime::' + called not in block
            or 'npc_saved_identity_runtime::' + called not in block):
        raise RuntimeError('Snapshot identity boundary differs: ' + function)
    blocks.append(block)
    start = end
matches = re.findall(r'const bool bind_snapshot_identities=(.*);', core)
if len(matches) != 1:
    raise RuntimeError('Expected one capture dispatch policy')
if ('capture_snapshot_identities = bind_snapshot_identities;' not in source
        or 'world_name,bind_snapshot_identities)' not in core):
    raise RuntimeError('Snapshot identity policy is not connected to the capture lifetime')
text = 'bool routing(bool distribute) { return ' + matches[0] + '; }\n'
for name, block in zip(('arm', 'seal', 'cancel'), blocks):
    text += ('void' if name == 'cancel' else 'bool') + ' actual_' + name + '() {\n' + block
    text += '\n}\n' if name == 'cancel' else '\nreturn true;\n}\n'
def guarded(text, anchor):
    if text.count(anchor) != 1:
        raise RuntimeError('Expected one actual scoped observer boundary: ' + anchor)
    begin = text.index(anchor)
    cursor = text.index('(', begin)
    depth, end = 1, cursor + 1
    while depth:
        depth += (text[end] == '(') - (text[end] == ')')
        end += 1
    while text[end].isspace():
        end += 1
    if text[end] == '{':
        depth, end = 1, end + 1
        while depth:
            depth += (text[end] == '{') - (text[end] == '}')
            end += 1
    else:
        end = text.index(';', end) + 1
    return text[begin:end]

for suffix, functions in [('arm', ['monster', 'boss', 'district', 'party']), ('seal', ['party', 'boss', 'monster', 'district'])]:
    chunks = [guarded(source, 'if(capture_snapshot_identities&&!' + family + '_saved_identity_runtime::' + suffix + '_host(') for family in functions]
    text += 'bool actual_other_' + suffix + '() {\n' + '\n'.join(chunks) + '\nreturn true;\n}\n'
text += 'void actual_other_cancel() {\n' + guarded(source, 'if(capture_snapshot_identities){') + '\n}\n'
stages = ['dungeon_scene_identity_runtime::classify_capture', 'party_saved_identity_runtime::stage_scenes',
          'boss_saved_identity_runtime::stage_scenes', 'monster_saved_identity_runtime::stage_scenes']
text += 'bool actual_stage() {\n' + '\n'.join(guarded(source, 'if(capture_snapshot_identities&&!' + name + '(') for name in stages) + '\nreturn true;\n}\n'
text += 'void actual_network_arm(bool bind_snapshot_identities) {\n' + guarded(core, 'if(bind_snapshot_identities){') + '\n}\n'
text += 'void actual_network_seal(bool publish_snapshot_identities) {\n' + guarded(core, 'if(publish_snapshot_identities){') + '\n}\n'
text += 'void actual_network_cancel(bool bind_snapshot_identities) {\n' + guarded(core, 'if(bind_snapshot_identities)game::network_identity::cancel_pending(error);') + '\n}\n'
for marker in ['{"bind_snapshot_identities",bind_snapshot_identities}', 'pending_snapshot.at("bind_snapshot_identities").get<bool>()',
               '{"snapshot_identities_published",publish_snapshot_identities}', 'if(distribute&&!net::distribute_snapshot(']:
    if marker not in core:
        raise RuntimeError('Snapshot capture/publication/cancel policy join missing: ' + marker)
args.output.parent.mkdir(parents=True, exist_ok=True)
args.output.write_text(text)

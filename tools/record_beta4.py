"""Bind the accepted beta4 milestone to retained native checkpoints."""
from pathlib import Path
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
def read(path):return json.loads((ROOT/path).read_text(encoding='utf-8'))
def receipt(path):return {'path':path,'sha256':hashlib.sha256((ROOT/path).read_bytes()).hexdigest()}

world='artifacts/acceptance/e2febe9e-402d-4d92-b737-ea9ecdcfd125/summary.json'
leave='artifacts/acceptance/71719e5f-86bf-4263-a372-d24a6939073c/summary.json'
roundtrip='artifacts/acceptance/0a91e81b-9746-4e36-8b10-98b31c25f636/summary.json'
shutdown='artifacts/acceptance/d9f88cbc-e67f-44ae-a456-d3b6c5765797/summary.json'
picker='artifacts/acceptance/5ac175ad-d65f-4df8-a03a-34481e6562e3/summary.json'
for path in (world,leave,roundtrip,shutdown):
    if read(path).get('passed') is not True:raise RuntimeError('Native acceptance failed: '+path)
if read(shutdown).get('exit_code')!=0:raise RuntimeError('Native shutdown was not clean')
selected=[value for value in read(picker)['checks'] if value['check']=='original-save-picker-visible']
if len(selected)!=1 or selected[0]['result'].get('selected_world')!='World of Stories':raise RuntimeError('Original picker checkpoint missing')
data={
    'version':'0.1.0-beta.4',
    'native_checks':[
        'The retained small friend-test world loads on both local peers with matching native network keys and network_verified acknowledgement.',
        'After native Host Leave and exact owned Host bridge loss, one Client Leave returns to Title and retires the Controller.',
        'An original-save fixture appears on the native Desktop and in the installed Steam world picker.',
        'Installed-profile capture, Title, native load and recapture retain the native world folder; original fixture unchanged and no new MPTestSession files.',
        'The owned installed proof exits through the native shutdown path with code 0.'
    ],
    'local_world':receipt(world),'host_loss_leave':receipt(leave),'installed_roundtrip':receipt(roundtrip),'installed_shutdown':receipt(shutdown),
    'original_picker_checkpoint':{**receipt(picker),'accepted_check':'original-save-picker-visible','source_overall_passed':False,
        'later_failure':'The later diagnostic-world capture used a 265-character metadata path. Only auto-generated test-world names were shortened; original save selection remains accepted.'},
    'original_desktop':receipt('test/mcp/evidence/friend-proof-after-parent_20261014T205816989778Z.json'),
    'mod_sha256':'f0183fc5ec99c65b04dfd754806f51b11d6c88853e48de4b6b05fa8f979115dc',
    'loader_sha256':'fabb86e6e9c10595d00909513d8bf18d7d359d2f9feea6c880550abc54219c7f',
    'build':'artifacts/build_history/20261014T210346187284Z',
    'regressions':['empty_network','client_leave','snapshot_world_path','snapshot_world_folder','original_saves','private_save_parents','test_loader_separation.py'],
    'carried_evidence':'Local two-peer gameplay and Leave used mod7c7d61df. The final mod only shortens MCP-generated diagnostic world names from42 to22 characters. Loader-only directory seeding does not affect the legacy local transport. Affected installed capture/load checks used the final candidate.',
    'limits':['The reported tester Client log was unavailable; the local idle session-loss cause and its correction were reproduced.',
        'The final correction still needs the next manual two-account Steam test.',
        'Tutorial progression is not yet shared. Full co-op, complete action coverage, Undo/Redo, four peers and large worlds are not proven.',
        'Original Title loading and a shared installed Steam session are not claimed by the single-account installed proof.']
}
(ROOT/'docs/evidence/beta4-native.json').write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8')
milestone={'schema':1,'version':'0.1.0-beta.4','summary':'Restore original save visibility, retain world folders, unblock new-world loading and allow idle Clients to leave after Host loss.','native_evidence':'docs/evidence/beta4-native.json'}
(ROOT/'.github/beta-milestone.json').write_text(json.dumps(milestone,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'milestone':milestone,'native_acceptance':True}))

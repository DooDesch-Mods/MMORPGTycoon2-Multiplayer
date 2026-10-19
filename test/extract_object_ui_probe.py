from pathlib import Path
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
source=(args.root/'src/game/object_ui_probe/runtime.inc').read_text(encoding='utf-8')
def body(anchor):
    assert source.count(anchor)==1,anchor
    begin=source.index(anchor);end=source.index('{',begin)+1;depth=1
    while depth:depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[begin:end]
assert 'submit_duplicate(' not in source and 'submit_group(' not in source and 'MH_CreateHook' not in source
for needle in ['common::resolve_api(', 'live_window(r,job,api,window,error)', 'job.exception=command(&api,window,job.input.command.c_str(),&job)',
               'job.exception=cursor_call(', 'duplicate_edit::operations', 'group_edit::operations', 'operation.binding!=job.binding',
               'operation.request.group.selected!=job.input.keys', 'keys!=job.input.keys', 'probe_contract::completion(',
               'jobs.contains(input.token)', 'if(unresolved())', 'if(active){stopped(*active,message,error);',
               'mt2mp::game::object_ui_code::guards', 'mt2mp::game::move_ui_probe::code::guards']:
    assert needle in source,'Actual object UI boundary absent: '+needle
assert 'image+0x2d5910' in body('uint32_t show(')
assert 'step==6?0x5c0200:step==7?0x5c05c0:0x5c01e0' in body('uint32_t cursor_call(')
args.output.mkdir(parents=True,exist_ok=True)
(args.output/'object_ui_command.inc').write_text(body('uint32_t command(')+'\n',encoding='utf-8')
print('PASS actual original command body and existing capture/identity/no-replay boundaries; no direct Controller submit or new hook.')

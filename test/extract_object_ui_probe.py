from pathlib import Path
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--root',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
source=(args.root/'src/game/object_ui_probe/runtime.inc').read_text(encoding='utf-8')
runtime=(args.root/'src/game/typed_action_runtime.cpp').read_text(encoding='utf-8')
def body(anchor,text=None):
    text=source if text is None else text
    assert text.count(anchor)==1,anchor
    begin=text.index(anchor);end=text.index('{',begin)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[begin:end]
assert 'submit_duplicate(' not in source and 'submit_group(' not in source and 'MH_CreateHook' not in source
for needle in ['common::resolve_api(', 'live_window(r,job,api,window,error)', 'job.exception=command(&api,window,job.input.command.c_str(),&job)',
               'job.exception=cursor_call(', 'duplicate_edit::operations', 'group_edit::operations', 'operation.binding!=job.binding',
               'operation.request.group.selected!=job.input.keys', 'keys!=job.input.keys', 'probe_contract::completion(',
               'jobs.contains(input.token)', 'if(unresolved())', 'if(active){stopped(*active,message,error);',
               'mt2mp::game::object_ui_code::guards', 'mt2mp::game::move_ui_probe::code::guards']:
    assert needle in source,'Actual object UI boundary absent: '+needle
assert 'typed_action_runtime::read_binding(job.input.snapshot,binding,error)' in body('bool current(')
assert 'network_binding(' not in body('bool current(')
assert 'typed_action_runtime::network_binding(job.input.snapshot,job.binding,error)' in source
assert source.index('if(reconcile_rejection(*r,job,reconciliation))') < source.index('if(job.phase=="unknown")')
assert 'job.phase="rejected"' in body('bool reconcile_rejection(')
assert source.index('if(!selection_ready(r,job,error))') < source.index('job.exception=show(')
assert 'image+0x2d5910' in body('uint32_t show(')
assert 'step==6?0x5c0200:step==7?0x5c05c0:0x5c01e0' in body('uint32_t cursor_call(')
assert 'job.input.command=="Duplicate"?duplicate_edit::sample_duplicate(' in source
assert '(job.input.command=="Group"||job.input.command=="Ungroup")?duplicate_edit::sample_untouched_scenery(' in source
args.output.mkdir(parents=True,exist_ok=True)
(args.output/'object_ui_command.inc').write_text(body('uint32_t command(')+'\n',encoding='utf-8')
(args.output/'object_ui_preparation.inc').write_text(body('bool selection_ready(')+'\n'+body('bool reconcile_preparation(')+'\n',encoding='utf-8')
(args.output/'object_ui_rejection.inc').write_text(body('bool terminal(')+'\n'+body('bool reconcile_rejection(')+'\n'+body('bool unresolved(')+'\n',encoding='utf-8')
(args.output/'object_ui_current.inc').write_text(body('bool current(')+'\n',encoding='utf-8')
read_body=source[source.index('bool read(const json& args,'):source.rfind('\n}')].strip()
assert read_body.endswith(';}') and 'catch(const std::exception& cause)' in read_body
for forbidden in ['command(', 'cursor_call(', 'show(', 'submit_', 'sample(']:
    assert forbidden not in read_body,'Read-only observation acquired an effect: '+forbidden
(args.output/'object_ui_read.inc').write_text(body('bool terminal(')+'\n'+body('bool stopped(')+'\n'+body('json format(')+'\n'+read_body+'\n',encoding='utf-8')
(args.output/'object_ui_binding.inc').write_text(body('bool read_binding(',runtime)+'\n'+body('bool network_binding(',runtime)+'\n',encoding='utf-8')
print('PASS actual original command body and existing capture/identity/no-replay boundaries; no direct Controller submit or new hook.')

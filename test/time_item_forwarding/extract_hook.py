from pathlib import Path
import argparse,json
parser=argparse.ArgumentParser()
parser.add_argument('--source',type=Path,required=True)
parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args()
core=(args.source/'src/core/dll_main.cpp').read_text(encoding='utf-8')
start=core.index('static void time_activate(');end=core.index('\nstatic void time_update(',start)
body=core[start:end]
if body.count('_ReturnAddress()')!=1:raise RuntimeError('Time entry native return capture differs')
hooks=(args.source/'src/game/control_input/control_hooks.inc').read_text(encoding='utf-8')
start=hooks.index('void item_entry(');end=hooks.index('\nvoid activate_entry(',start)
forwarding=hooks[start:end]
if 'original(item)' in forwarding or forwarding.count('game::item_activation_origin::forward(original,item,r->image,caller,route==7)')!=2:
    raise RuntimeError('Control Item forwarding must carry actual caller on both unchanged original branches')
if 'item_entry(7,item,uintptr_t(_ReturnAddress()))' not in hooks:raise RuntimeError('Control Activate must capture its physical native caller')
args.output.mkdir(parents=True,exist_ok=True)
(args.output/'time_item_hook.inc').write_text(body.replace('_ReturnAddress()','fixture_return_address()'),encoding='utf-8')
print(json.dumps({'extracted':'actual time_activate','forwarding_branches':2,'native_caller_capture':'unchanged'}))

from pathlib import Path
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--source-root',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
args=parser.parse_args();root=args.source_root;out=args.output;out.mkdir(parents=True,exist_ok=True)
s=(root/'src/game/typed_action_runtime.cpp').read_text()
a=s.index('bool read_binding(');b=s.index('bool live_state_available(',a)
(out/'binding_providers.inc').write_text(s[a:b])
s=(root/'src/game/tutorial_startingpoint_fixture/startingpoint_fixture.cpp').read_text()
a=s.index('    if(controller.at("initialized").get<bool>()){');b=s.index('    if(!hud_time_runtime::verify_window_show_hook',a)
(out/'fixture_binding.inc').write_text(s[a:b])

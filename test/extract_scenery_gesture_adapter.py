from pathlib import Path
import argparse
parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
p=args.output;p.mkdir(parents=True,exist_ok=True)
s=(args.source/'src/game/scenery_stroke_gesture.inc').read_text()
start=s.index('uint32_t gesture_call(');end=s.index('struct GesturePort',start)
s=s[:start]+'''uint32_t gesture_call(uintptr_t,uintptr_t,uintptr_t,Kind kind,gesture::Step step,const gesture::Point&)noexcept{
    ++fixture_calls;if(fail_call==fixture_calls)return 0xc0000005;
    if(step==gesture::Step::Press){capture=Capture{};capture->intent.kind=kind;capture->intent.region=7;++captures;}
    if(step==gesture::Step::Press||step==gesture::Step::Drag)capture->intent.stamps.push_back(1);
    if(step==gesture::Step::Release&&!omit_queue){queued=Pending{fixture_binding, capture->intent, fixture_snapshot, fixture_action};capture.reset();}
    if(reject_call==fixture_calls){++rejections;cause="original capture rejected";}
    if(change_world_call==fixture_calls)fixture_world.root++;
    return 0;
}
uint32_t gesture_region(uintptr_t,uintptr_t,const gesture::Point&,uintptr_t& out)noexcept{out=point_region;return 0;}
'''+s[end:]
(p/'adapter_actual.inc').write_text(s)
print('Extracted complete native adapter; only two native ABI leaf calls replaced')

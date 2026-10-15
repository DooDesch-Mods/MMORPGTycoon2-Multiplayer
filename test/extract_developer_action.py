"""Extract the actual native action boundary for controlled-port tests."""
from pathlib import Path
import argparse


def body(text,anchor):
    start=text.index(anchor);at=text.index('{',start);depth=1;end=at+1
    while depth:
        if text[end]=='{':depth+=1
        elif text[end]=='}':depth-=1
        end+=1
    return text[start:end]


p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
s=a.source.read_text(encoding='utf-8');a.output.mkdir(parents=True,exist_ok=True)
(a.output/'developer_operation.inc').write_text(body(s,'struct Operation {')+';\n',encoding='utf-8')
(a.output/'developer_pending.inc').write_text(body(s,'struct Pending {')+';\n',encoding='utf-8')
(a.output/'developer_perform.inc').write_text(body(s,'action::Outcome perform(')+'\n',encoding='utf-8')
(a.output/'developer_frame_calls.inc').write_text(body(s,'struct FrameCalls {')+';\n',encoding='utf-8')
(a.output/'developer_adopt_frame.inc').write_text(body(s,'bool preflight_frame(')+'\n'+body(s,'bool adopt_frame(')+'\n',encoding='utf-8')
(a.output/'developer_ui_fixture.inc').write_text(body(s,'bool ui_fixture(')+'\n',encoding='utf-8')
p=(a.source.parent.parent/'tutorial_replication/start_presentation.h').read_text(encoding='utf-8')
(a.output/'developer_presentation.inc').write_text(p[p.index('struct Body {'):p.index('struct BorrowedString')],encoding='utf-8')
b=(a.source.parent.parent/'live_host_state/bridge.cpp').read_text(encoding='utf-8')
(a.output/'developer_failure_latch.inc').write_text(body(b,'struct DeveloperFailureLatch {')+';\n',encoding='utf-8')
print('Extracted actual Developer operation and native perform bodies')

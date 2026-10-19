from pathlib import Path
import argparse
import hashlib
import json
import re

root=Path(__file__).resolve().parent
parser=argparse.ArgumentParser()
parser.add_argument("--source",type=Path,default=root/"candidate")
parser.add_argument("--output",type=Path,required=True)
parser.add_argument("--verify-delta",action="store_true")
args=parser.parse_args()
control=(args.source/"src/game/control_input/control_runtime.cpp").read_text()
release=(args.source/"src/game/release_runtime.cpp").read_text()
probe=(args.source/"src/game/placement_ui_probe/ui_probe.cpp").read_text()
old_control=(root/"baseline/src/game/control_input/control_runtime.cpp").read_text()
clock=control[control.index("bool verify_clock_time_scale("):control.index("bool capture_acquire(")]
if args.verify_delta:assert control.replace(clock,"")==old_control
shared='''
        if(pin.rva==0x259d70){bool owned=false;
            if(!control_input::runtime::verify_clock_time_scale(r.image,pin.bytes,pin.sha,owned,e))return false;
            if(owned)continue;
        }
        '''
old_release=(root/"baseline/src/game/release_runtime.cpp").read_text()
assert release.count(shared)==1
if args.verify_delta:assert release.replace('#include "control_input/control_runtime.h"\n',"").replace(shared,"")==old_release
probe_shared='''
    if(g.rva==0x207820){bool owned=false;
        const auto sha=mt2mp::sha256(std::vector<unsigned char>(g.bytes,g.bytes+g.size));
        if(!mt2mp::control_input::runtime::verify_item_activate(c.image,g.size,sha,owned,e))return false;
        if(owned)continue;
    }
    '''
old_probe=(root/"baseline/src/game/placement_ui_probe/ui_probe.cpp").read_text()
assert probe.count(probe_shared)==1
if args.verify_delta:assert probe.replace('#include "../control_input/control_runtime.h"\n#include "mt2mp/core/hash.h"\n',"").replace(probe_shared,"")==old_probe
base=args.source/"src/game"
pins=re.findall(r'\{0x259d70,144,(?:6,)?"([0-9a-f]+)"\}',(base/"release_cycle_pins.inc").read_text()+(base/"control_input/control_route_initializers.inc").read_text())
assert len(pins)==2 and pins[0]==pins[1]=="c7c30a3939bac9c62680b70782d7bfcdad071f3dba377104d598e783843b1304"
args.output.mkdir(parents=True,exist_ok=True)
route=control[control.index("bool verify_route("):control.index("bool routes(")]
providers=control[control.index("bool verify_item_activate("):control.index("bool capture_acquire(")]
(args.output/"control_clock_extracted.inc").write_text(route+providers,encoding="utf-8")
code=release[release.index("bool code(Runtime&"):release.index('#include "release_reference_runtime.inc"')]
(args.output/"release_clock_extracted.inc").write_text(code,encoding="utf-8")
(args.output/"release_edit_pins.inc").write_text('struct Pin {uintptr_t rva;size_t bytes;const char* sha;};\nconst Pin pins[]={{::edit_rva,32,::fixture_edit_sha.c_str()}};\n')
(args.output/"release_cycle_pins.inc").write_text('const Pin cycle_pins[]={{::clock_rva,144,::fixture_clock_sha.c_str()},{::cycle_rva,64,::fixture_cycle_sha.c_str()}};\n')
context=probe[probe.index("bool context_actual("):probe.index("bool scalar_request(")]
initialize=probe[probe.index("bool Probe::initialize("):probe.index("Receipt Probe::arm(")]
(args.output/"probe_initialize_extracted.inc").write_text(context+initialize,encoding="utf-8")
fixture=(root/"test_shared_clock.cpp").read_text()
(args.output/"shared_owner_fixture.inc").write_text(fixture[:fixture.index("int main()try{")],encoding="utf-8")
files=[args.source/name for name in ("src/game/control_input/control_runtime.cpp","src/game/release_runtime.cpp","src/game/placement_ui_probe/ui_probe.cpp")]
files += [root/"test_shared_clock.cpp",root/"test_probe_shared_item.cpp"]
(args.output/"inputs.json").write_text(json.dumps({"source":str(args.source),"clock_sha":pins[0],"files":[{"path":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()} for path in files]},indent=2)+"\n")
print("PASS selected Clock/Release/Probe providers and matching Clock pin; actual consumer fixtures extracted; whole-file delta checks are opt-in")

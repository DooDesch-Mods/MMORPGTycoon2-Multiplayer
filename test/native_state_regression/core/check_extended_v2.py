from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

root = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, default=root / "candidate")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
subprocess.run([sys.executable, "-X", "utf8", str(root / "check.py"), "--source", str(args.source), "--output", str(args.output)],check=True)
text = (args.source / "src/core/framework_plugin.cpp").read_text(encoding="utf-8")
end = text[text.index("void MT2FW_CALL end("):text.index("void prepare() {")]
prepare = text[text.index("void prepare() {"):text.index("void MT2FW_CALL update(")]
begin = text[text.index("int MT2FW_CALL begin("):text.index("void MT2FW_CALL end(")]
announcement = "Actual Multiplayer engine/session runtime ready"
assert announcement not in prepare and text.count(announcement)==1
assert announcement in end and end.index("if(!passed)")<end.index(announcement)
assert "state.ready=true;" in prepare and "!state.ready" in begin
assert "runtime_announced" not in prepare
core = (args.source / "src/core/dll_main.cpp").read_text(encoding="utf-8")
core_end = core[core.index("bool end(const void* owner,"):core.index("bool retire(const void* owner,")]
gate = next(line.strip() for line in core_end.splitlines() if "if(owned_native_profile&&!instance_hooks_ready)" in line)
assert core_end.index("game_update_after(") < core_end.index(gate) < core_end.index("error.clear();")
fixture = r'''#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#define MT2FW_CALL
struct MT2FW_MT2_Frame {size_t size=sizeof(MT2FW_MT2_Frame);uint32_t reserved=0;uint64_t id=1;void* game=nullptr;float dt=0.1f;bool original_returned=true;};
struct State {bool ready=true,runtime_announced=false,stop_requested=false;std::string failure;unsigned entered_frames=1;} state;
unsigned checks=0,announcements=0,retirements=0,failed=0;
bool fixture_pair_passes=true,fixture_pair_closes=true;
bool owned_native_profile=true,instance_hooks_ready=true;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
#define LOG_INFO(...) (++announcements)
bool callback(void* owner){return owner==&state;}
void fail(const char*,const std::string& error){++failed;state.failure=error;}
void retire(){++retirements;}
namespace game_update_pair {
struct Frame {uint64_t id;void* game;float dt;bool original_returned;};
bool end(void*,const Frame&,std::string& error,bool* was_closed){
    *was_closed=fixture_pair_closes;
    if(!fixture_pair_passes){error="Framework GameUpdate post failed: Dungeon Instance hooks: caller original route failure";return false;}
    try {
'''
fixture += gate + "\n"
fixture += r'''
        return true;
    }catch(const std::exception& e){error=std::string("Framework GameUpdate post failed: ")+e.what();return false;}
}
}
'''
fixture += end
fixture += r'''
void reset(){state={};announcements=retirements=failed=0;fixture_pair_passes=fixture_pair_closes=instance_hooks_ready=true;}
int main()try{
    MT2FW_MT2_Frame frame;reset();
    require(state.ready&&!state.runtime_announced&&announcements==0,"callback subscription alone does not announce required runtime readiness");
    end(&state,&frame);
    require(announcements==1&&state.runtime_announced&&state.entered_frames==0&&failed==0,"first successful paired end announces required runtime exactly once");
    state.entered_frames=1;end(&state,&frame);
    require(announcements==1&&state.ready&&failed==0,"later successful ends preserve callback readiness without duplicate announcement");
    reset();fixture_pair_passes=false;end(&state,&frame);
    require(announcements==0&&!state.runtime_announced&&failed==1,"failed required initialization cannot announce runtime readiness");
    require(state.failure=="Framework GameUpdate post failed: Dungeon Instance hooks: caller original route failure","original paired failure cause retained");
    fixture_pair_passes=true;state.entered_frames=1;end(&state,&frame);
    require(announcements==0&&!state.runtime_announced,"previous failure cannot become apparent readiness on another paired end");
    reset();state.stop_requested=true;end(&state,&frame);
    require(announcements==0&&retirements==1,"pending stop retires existing callback without false ready event");
    reset();frame.reserved=1;end(&state,&frame);
    require(announcements==0&&state.failure=="Framework end receipt is invalid","malformed callback receipt cannot announce readiness");
    reset();frame.reserved=0;state.entered_frames=0;end(&state,&frame);
    require(announcements==0&&state.failure=="Framework end has no entered Multiplayer frame","unmatched closed receipt cannot announce readiness");
    reset();end(nullptr,&frame);
    require(announcements==0&&!state.runtime_announced,"foreign callback owner cannot announce readiness");
    reset();instance_hooks_ready=false;end(&state,&frame);
    require(announcements==0&&!state.runtime_announced&&state.failure=="Framework GameUpdate post failed: Dungeon Instance hooks: required game-thread initialization did not complete","silent unbound Core return cannot appear as successful required initialization");
    std::cout<<checks<<" Framework runtime ready checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
'''
(args.output / "test_instance_runtime_ready.cpp").write_text(fixture,encoding="utf-8")
(args.output / "framework-inputs.json").write_text(json.dumps({"framework_plugin_sha256":hashlib.sha256(text.encode()).hexdigest(),"structural_checks":5},indent=2)+"\n")
print("PASS 5 Framework readiness source assertions; exact paired end fixture extracted")

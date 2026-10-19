from pathlib import Path
import argparse
import hashlib
import json

parser = argparse.ArgumentParser()
parser.add_argument("--source", type=Path, default=Path(__file__).resolve().parent / "candidate")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
text = (args.source / "src/core/dll_main.cpp").read_text(encoding="utf-8")
loader = text[text.index("bool initialize(HMODULE module) {"):text.index("void shutdown() {")]
after = text[text.index("static void game_update_after("):text.index("static void game_update(void*")]
start = text.index("static bool instance_initialization_attempted=")
stop = text.index("static void initialize_time_hooks() {", start)
helper = text[start:stop]
native_call = "game::dungeon_instance_native::initialize("
assert native_call not in loader and "initialize_instance_hooks();" not in loader
assert text.count(native_call) == 1 and helper.count(native_call) == 1
assert after.count("initialize_instance_hooks();") == 1
bind = "    if(!game::native::bind_game_thread()) return;"
thread = "    engine_thread=GetCurrentThreadId();"
owned = "    if(owned_native_profile) {"
init = "        initialize_instance_hooks();"
tick = next(line for line in after.splitlines() if "game::typed_action_runtime::tick();" in line)
positions = [after.index(line) for line in (bind, thread, owned, init, tick)]
assert positions == sorted(positions)
assert "REQUIRED_ENGINE_ROUTES_READY" not in loader
assert "REQUIRED_ENGINE_ROUTES_READY" in helper
assert 'result["dungeon_instance_hooks"]=' in text
args.output.mkdir(parents=True, exist_ok=True)
fixture = r'''#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
unsigned checks=0,initialize_calls=0,typed_calls=0,errors=0,infos=0;
bool bound=false,allow_bind=true,owned_native_profile=true;
uint32_t engine_thread=0;
uintptr_t native_image_begin=0x140000000;
int mode=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
uint32_t GetCurrentThreadId(){return 71;}
#define LOG_INFO(...) (++infos)
#define LOG_ERROR(...) (++errors)
#define MT2MP_FRAMEWORK_PLUGIN 1
namespace game::native {bool bind_game_thread(){bound=allow_bind;return bound;}}
namespace game::dungeon_instance_native {
bool initialize(uintptr_t image,std::string& error){
    ++initialize_calls;
    require(bound&&engine_thread==71&&image==native_image_begin,"required initializer runs on actual bound callback thread");
    if(mode==1){error="caller original route failure";return false;}
    if(mode==2)throw std::runtime_error("original C++ failure");
    if(mode==3)throw 19;
    error.clear();return true;
}}
namespace game::typed_action_runtime {void tick(){++typed_calls;}}
enum class FrameTimingPart {Typed};
namespace game_update_pair {enum class Section {Typed};}
struct FrameTimingScope {explicit FrameTimingScope(FrameTimingPart){}};
struct GameUpdateWorkScope {explicit GameUpdateWorkScope(game_update_pair::Section){}};
'''
fixture += helper
# The exact affected call statements are extracted from Core. Unchanged lobby,
# time and transport work remains outside this focused callback slice.
fixture += "static void affected_game_update() {\n" + "\n".join((bind, thread, owned, init, tick)) + "\n    }\n}\n"
fixture += r'''
void reset(int failure=0){
    mode=failure;bound=false;allow_bind=true;owned_native_profile=true;engine_thread=0;
    initialize_calls=typed_calls=errors=infos=0;
    instance_initialization_attempted=instance_hooks_ready=false;instance_initialization_error.clear();
}
int main()try{
    reset();require(initialize_calls==0&&!instance_hooks_ready,"loader startup has no deferred initializer effect");
    allow_bind=false;affected_game_update();require(initialize_calls==0&&typed_calls==0&&!instance_initialization_attempted,"unbound callback does not attempt initialization or typed tick");
    allow_bind=true;owned_native_profile=false;affected_game_update();require(initialize_calls==0&&typed_calls==0,"non-owned profile preserves existing callback scope");
    owned_native_profile=true;affected_game_update();require(initialize_calls==1&&typed_calls==1&&instance_hooks_ready,"successful required initialization precedes first typed tick");
    affected_game_update();require(initialize_calls==1&&typed_calls==2&&errors==0,"later updates reuse confirmed initialization exactly once");
    require(infos==2,"required readiness is emitted once only after initialization success");
    for(int failure:{1,2,3}){
        reset(failure);std::string cause;
        try{affected_game_update();throw std::runtime_error("failure unexpectedly returned");}
        catch(const std::runtime_error& e){cause=e.what();}
        const std::string expected=failure==1?"caller original route failure":failure==2?"original C++ failure":"unknown native initialization exception";
        require(cause=="Dungeon Instance hooks: "+expected,"required failure propagates original cause through paired callback");
        require(instance_initialization_attempted&&!instance_hooks_ready&&typed_calls==0,"failed required initialization never enters typed tick");
        require(errors==1&&infos==0,"failure logged once with no ready announcement");
        try{affected_game_update();throw std::runtime_error("failure replay unexpectedly returned");}
        catch(const std::runtime_error& e){cause=e.what();}
        require(initialize_calls==1&&typed_calls==0&&errors==1,"partial installation never retries automatically");
        require(cause=="Dungeon Instance hooks: "+expected&&instance_initialization_error==expected,"persistent failure retains exact original cause");
    }
    std::cout<<checks<<" Instance initialization thread checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
'''
(args.output / "test_instance_init_thread.cpp").write_text(fixture, encoding="utf-8")
(args.output / "inputs.json").write_text(json.dumps({"core_sha256":hashlib.sha256(text.encode()).hexdigest(),"structural_checks":7},indent=2)+"\n")
print("PASS 7 Core placement/readiness source assertions; behavioral fixture extracted")

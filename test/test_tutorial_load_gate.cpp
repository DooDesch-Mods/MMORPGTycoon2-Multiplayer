#include "game/native_open_witness.h"
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using World=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
struct Load {std::string uuid;uint64_t generation=0;bool armed=false,ready=false;std::set<uintptr_t> sequences;bool waiting_postload=false;} load;
std::string fault;
uint32_t mainthread=9,thread=9;
uintptr_t image=0x10000000;
uint32_t GetCurrentThreadId(){return thread;}
std::map<uintptr_t,std::vector<unsigned char>> values;
std::map<uintptr_t,std::string> strings;
World world;
int world_calls=0,world_failure=0,info_logs=0;
bool change_origin=false,change_owner=false,change_flag=false,observer_failure=false;
uintptr_t unreadable=0;
template<class T>void write(uintptr_t p,T value){auto& bytes=values[p];bytes.resize(sizeof(value));std::memcpy(bytes.data(),&value,sizeof(value));}
template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){
    const auto a=p+offset;
    if(a==unreadable||!values.contains(a)||values.at(a).size()!=sizeof(T)){e="fixture copy failed address="+std::to_string(a);return false;}
    std::memcpy(&out,values.at(a).data(),sizeof(T));return true;
}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool str(uintptr_t p,size_t offset,std::string& out,std::string& e){if(!strings.contains(p+offset))return fail(e,"fixture string read failed");out=strings.at(p+offset);return true;}
bool supported(const std::string& id){return id=="initial_tutorial"||id=="leveltwo_tutorial";}
bool type(uintptr_t p,uintptr_t rva,std::string& e){uintptr_t actual=0;return at(p,0,actual,e)&&(actual==image+rva||fail(e,"tutorial native vtable differs"));}
#define LOG_INFO(...) (++info_logs)
namespace native_open_runtime {
bool read_current_world(World& out,std::string& e){
    ++world_calls;if(world_calls==world_failure)return fail(e,"fixture original NativeOpen cause");
    out=world;
    if(world_calls==2){if(change_origin)++out.world_epoch;if(change_owner)write<uintptr_t>(0x1000+0x1e8,0x2100);if(change_flag)write<uint8_t>(0x2000+0x60,1);}
    return true;
}
}
namespace tutorial_observer {bool read(nlohmann::json& out,std::string& e){out=nullptr;if(observer_failure)return fail(e,"fixture original observer cause");return true;}}
#include "load_gate_body.inc"

void reset(uint8_t postloaded=0){
    load={"local-snapshot",7,true,false,{}};fault.clear();mainthread=thread=9;values.clear();strings.clear();world_calls=world_failure=info_logs=0;
    change_origin=change_owner=change_flag=observer_failure=false;unreadable=0;
    world={};world.game=reinterpret_cast<void*>(0x800);world.mode=reinterpret_cast<void*>(0x900);world.state=reinterpret_cast<void*>(0x1000);
    world.world=reinterpret_cast<void*>(0x1100);world.map=reinterpret_cast<void*>(0x1200);world.uuid=load.uuid;world.generation=7;world.world_epoch=4;world.sha256="hash";world.bytes=500;
    write<uintptr_t>(0x1000+0x1e8,0x2000);write<uint8_t>(0x2000+0x60,postloaded);write<uintptr_t>(0x2000+0x20,0x3000);write<int32_t>(0x2000+0x28,1);
    write<uintptr_t>(0x3000,0x4000);strings[0x4000+0x18]="initial_tutorial";write<uintptr_t>(0x4000,image+0x173ce70);write<uintptr_t>(0x4000+0x58,0x5000);write<uintptr_t>(0x5000,image+0x16dbd80);
}
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void rejected(const std::string& cause){bool pending=true;std::string e;require(!finish_client_load(pending,e),"expected rejection");require(!pending,"rejection mislabeled pending");require(e==cause,"rejection cause changed");require(!load.ready,"rejection admitted load");}
int main(){try{
    bool pending=false;std::string e;
    reset();load.ready=true;require(finish_client_load(pending,e)&&pending&&!load.ready&&e.empty(),"deferred PostLoad must wait");
    require(load.sequences.empty(),"pending inserted late fence");world_calls=0;require(finish_client_load(pending,e)&&pending&&info_logs==1,"pending log must be bounded");
    write<uint8_t>(0x2000+0x60,1);world_calls=0;rejected("Client tutorial was not fenced during joined PostLoad after ScenarioLog completion: initial_tutorial");
    load.sequences.insert(0x5000);world_calls=0;require(finish_client_load(pending,e)&&!pending&&load.ready&&info_logs==2,"completed PostLoad with fence must recover");
    reset(1);load.sequences.insert(0x5000);write<int32_t>(0x2000+0x28,2);write<uintptr_t>(0x3008,0x6000);strings[0x6018]="leveltwo_tutorial";write<uintptr_t>(0x6000,image+0x173ce70);write<uintptr_t>(0x6058,0x7000);write<uintptr_t>(0x7000,image+0x16dbd80);
    rejected("Client tutorial was not fenced during joined PostLoad after ScenarioLog completion: leveltwo_tutorial");
    reset(1);strings[0x4018]="other_scenario";require(finish_client_load(pending,e)&&!pending&&load.ready,"non-tutorial scenario incorrectly requires fence");
    reset();thread=8;rejected("tutorial Client load completion thread differs");
    reset();world.uuid="foreign";rejected("tutorial loaded local origin differs");
    reset();world.generation=8;rejected("tutorial loaded local origin differs");
    reset();change_origin=true;rejected("tutorial origin changed during load completion");
    reset(1);load.sequences.insert(0x5000);change_origin=true;rejected("tutorial origin changed during load completion");
    reset();world_failure=1;rejected("fixture original NativeOpen cause");
    reset();world_failure=2;rejected("fixture original NativeOpen cause");
    reset();observer_failure=true;rejected("fixture original observer cause");
    reset();unreadable=0x2060;rejected("fixture copy failed address=8288");
    reset(2);rejected("tutorial ScenarioLog PostLoad byte invalid");
    reset();change_owner=true;rejected("tutorial ScenarioLog owner changed during load completion");
    reset();change_flag=true;rejected("tutorial ScenarioLog PostLoad byte changed during load completion");
    reset(1);write<int32_t>(0x2028,65);rejected("tutorial scenario count outside0..64 during load completion");
    reset(1);unreadable=0x3000;rejected("fixture copy failed address=12288");
    reset(1);write<uintptr_t>(0x5000,0xdead);rejected("tutorial native vtable differs");
    reset();fault="original hook failure";rejected("original hook failure");
    reset();load.armed=false;rejected("tutorial Client load not armed");
    std::cout<<"PASS: actual finish_client_load body, deferred completion and all supported fences, no late insertion, origin/thread/owner/type/bounds/read failures retained\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}

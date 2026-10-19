#include "monster_move_read.h"
#include "vendor/nlohmann/json.hpp"
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

using json=nlohmann::json;
namespace mr=mt2mp::game::monster_move_read;
namespace {
constexpr uintptr_t fixture_image=0x140000000,fixture_slot=0x200000,fixture_screen=0x300000,fixture_scenes=0x400000,fixture_scene=0x500000,fixture_camera=0x600000,fixture_object=0x700000;
struct {uintptr_t image=fixture_image;} state;
struct Memory{};
struct LiveLocal {struct {uint64_t uid=7425;} entry;mr::Actor actor;};
std::map<uintptr_t,std::vector<unsigned char>> memory;
std::vector<std::string> calls;
uint32_t scene_fault=0,present_fault=0;
uintptr_t scene_result=fixture_scene;
mr::Actor observed;
bool read_actor=true;
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
template<class T>void put(uintptr_t address,T value){auto& bytes=memory[address];bytes.resize(sizeof(T));std::memcpy(bytes.data(),&value,sizeof(T));}
template<class T>bool field(Memory&,uintptr_t base,uintptr_t offset,T& out,std::string& error){
    const auto it=memory.find(base+offset);
    if(it==memory.end()||it->second.size()!=sizeof(T))return fail(error,"fixture field unreadable at "+std::to_string(base+offset));
    std::memcpy(&out,it->second.data(),sizeof(T));return true;
}
uint32_t live_scene(uintptr_t actual_screen,uintptr_t& result){
    require(actual_screen==fixture_screen,"original GetScene received the singleton slot instead of Screen");
    calls.push_back("scene");result=scene_result;return scene_fault;
}
uint32_t live_present(uintptr_t actual_object,uintptr_t character,uintptr_t actor,float dt){
    require(actual_object==fixture_object&&character==0x810000&&actor==0x820000&&dt==0,"original presentation ABI differs");
    calls.push_back("present");return present_fault;
}
bool move_actor(Memory&,uintptr_t actual_object,mr::Actor& out,std::string& error){
    require(actual_object==fixture_object,"actor read target differs");calls.push_back("read");
    if(!read_actor)return fail(error,"original actor read cause");out=observed;return true;
}
class MonsterLivePort {
public:
    uint64_t revision_=17,updates_=0;
    json presentation_=nullptr;
#include "monster_live_present.inc"
};
LiveLocal reset(){
    memory.clear();calls.clear();scene_fault=present_fault=0;scene_result=fixture_scene;read_actor=true;
    put(fixture_image+0x164b420,fixture_slot);put(fixture_slot,fixture_screen);put(fixture_screen+0x30,int32_t(1));put(fixture_screen+0x10,fixture_scenes);put(fixture_scenes,fixture_scene);put(fixture_scene+0x40,fixture_camera);
    LiveLocal local;local.actor.character=0x810000;local.actor.actor=0x820000;local.actor.skeleton=0x830000;
    local.actor.on_map=true;local.actor.spawn={8020,70,8550};local.actor.in_view=true;local.actor.distance_visible=true;local.actor.visible=true;
    observed=local.actor;return local;
}
unsigned cases=0;
template<class Setup>void rejected(Setup setup,const std::string& cause,const std::vector<std::string>& expected={},float dt=0){
    auto local=reset();MonsterLivePort port;std::string error;setup();
    require(!port.present(fixture_object,local,dt,error),"expected presentation rejection: "+cause);
    require(error.find(cause)!=std::string::npos,"lost cause: "+error+" expected "+cause);
    require(calls==expected,"unexpected original calls on rejected presentation");require(port.updates_==0,"failed presentation counted as complete");++cases;
}
void accepted(bool visible,uintptr_t local_camera,bool on_map){
    auto local=reset();local.actor.on_map=on_map;observed.on_map=on_map;
    observed.distance_visible=visible;observed.visible=on_map&&visible;observed.in_view=visible;
    put(fixture_scene+0x40,local_camera);const auto before=local.actor;MonsterLivePort port;std::string error;
    require(port.present(fixture_object,local,0,error),error);require(error.empty(),"unexpected success cause");
    require(calls==std::vector<std::string>{"scene","present","read"},"original presentation/readback order differs");
    require(local.actor==before,"local authoritative actor input changed");require(mr::same_registered_actor(before,observed),"camera changed identity/spawn/OnMap");
    require(port.updates_==1&&port.presentation_["applied_revision"]==17&&port.presentation_["local_uid"]==7425,"presentation receipt differs");
    require(port.presentation_["skeleton_visible"]==(on_map&&visible),"local visibility not retained");++cases;
}
}
int main(){try{
    accepted(true,fixture_camera,true);accepted(false,fixture_camera+0x1000,true);accepted(true,fixture_camera+0x2000,false);
    rejected([]{},"finite zero",{},1);rejected([]{},"finite zero",{},std::numeric_limits<float>::quiet_NaN());
    rejected([]{memory.erase(fixture_image+0x164b420);},"fixture field unreadable");
    rejected([]{put(fixture_image+0x164b420,uintptr_t(0));},"singleton slot absent");
    rejected([]{memory.erase(fixture_slot);},"fixture field unreadable");
    rejected([]{put(fixture_slot,uintptr_t(0));},"current Screen absent");
    rejected([]{memory.erase(fixture_screen+0x30);},"fixture field unreadable");
    rejected([]{put(fixture_screen+0x30,int32_t(0));},"scene count outside bounds: 0");
    rejected([]{put(fixture_screen+0x30,int32_t(-1));},"scene count outside bounds: -1");
    rejected([]{put(fixture_screen+0x30,int32_t(1048577));},"scene count outside bounds: 1048577");
    rejected([]{memory.erase(fixture_screen+0x10);},"fixture field unreadable");
    rejected([]{put(fixture_screen+0x10,uintptr_t(0));},"scene array absent");
    rejected([]{memory.erase(fixture_scenes);},"fixture field unreadable");
    rejected([]{put(fixture_scenes,uintptr_t(0));},"Scene zero absent");
    rejected([]{scene_fault=13;},"Scene lookup SEH=13",{"scene"});
    rejected([]{scene_result=fixture_scene+8;},"Scene lookup differs",{"scene"});
    rejected([]{memory.erase(fixture_scene+0x40);},"fixture field unreadable",{"scene"});
    rejected([]{put(fixture_scene+0x40,uintptr_t(0));},"Scene camera absent",{"scene"});
    rejected([]{present_fault=23;},"presentation SEH=23",{"scene","present"});
    rejected([]{read_actor=false;},"original actor read cause",{"scene","present","read"});
    rejected([]{++observed.character;},"changed current actor/spawn/OnMap",{"scene","present","read"});
    rejected([]{++observed.spawn[0];},"changed current actor/spawn/OnMap",{"scene","present","read"});
    rejected([]{observed.on_map=false;},"changed current actor/spawn/OnMap",{"scene","present","read"});
    rejected([]{observed.visible=false;},"visibility readback differs",{"scene","present","read"});
    std::cout<<"PASS "<<cases<<" actual Monster presentation body cases\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}

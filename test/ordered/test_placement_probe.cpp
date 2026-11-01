#include <mt2mp/game/native_engine.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#define LOG_INFO(...) ((void)0)
namespace mt2mp::game::native {
using DWORD=uint32_t;
#include "placement_result.inc"
std::atomic<uintptr_t> image{0x140000000};
std::atomic<bool> island_lock_unknown{false};
constexpr uintptr_t game_slot=0x13df8f0;
bool thread_ok=true,active=true,folder_ok=true,read_ok=true,saves_ok=true,uid_ok=true,building_ok=true;
int test_edits=0,test_saves=0,native_calls=0,building_reads=0;
unsigned char test_locked=0;
std::string test_folder="Swords of Wizards";
bool observed_apply=false,observed_purchase=false;
float observed_y=0;
PlacementResult outcome{};
BuildingState returned{};
bool fail(std::string& error,const std::string& message){error=message;return false;}
bool check_thread(std::string& error){return thread_ok||fail(error,"original wrong-thread cause");}
bool in_game(){return active;}
void* singleton(uintptr_t slot){if(slot!=game_slot)throw std::runtime_error("wrong singleton");return reinterpret_cast<void*>(0x1000);}
bool read_game_string(void* object,size_t offset,std::string& value){
    if(object!=reinterpret_cast<void*>(0x1000)||offset!=0x470)throw std::runtime_error("wrong test_folder owner");
    if(folder_ok)value=test_folder;return folder_ok;
}
template<class T>bool read_at(void* object,size_t offset,T& value){
    if(!read_ok)return false;
    if(reinterpret_cast<uintptr_t>(object)==image&&offset==0x13beda0){value=static_cast<T>(test_edits);return true;}
    if(reinterpret_cast<uintptr_t>(object)==image&&offset==0x1a30742){value=static_cast<T>(test_locked);return true;}
    if(object==outcome.building&&offset==0x18){value=static_cast<T>(uid_ok?42:0);return true;}
    throw std::runtime_error("unmodeled native read");
}
bool read_saves_in_progress(int32_t& value,std::string& error){value=test_saves;return saves_ok||fail(error,"original test_saves unreadable");}
PlacementResult place_building_native(const char* type,const char* variant,float x,float z,bool apply,bool purchase,float y){
    ++native_calls;observed_apply=apply;observed_purchase=purchase;observed_y=y;
    returned={{42,x,outcome.ground_y,z},type,variant};return outcome;
}
bool read_building(uint64_t uid,BuildingState& value,std::string& error){
    ++building_reads;if(uid!=42)throw std::runtime_error("wrong native UID");
    if(!building_ok)return fail(error,"original building read failure");value=returned;return true;
}
#include "placement_probe_actual.inc"
void reset(){
    thread_ok=active=folder_ok=read_ok=saves_ok=uid_ok=building_ok=true;
    test_edits=test_saves=native_calls=building_reads=0;test_locked=0;island_lock_unknown=false;
    test_folder="Swords of Wizards";observed_apply=observed_purchase=false;observed_y=0;
    outcome={};outcome.stage=10;outcome.ground_y=7;outcome.building=reinterpret_cast<void*>(0x2000);returned={};
}
}
namespace s=mt2mp::game::native;
int checks=0;
void check(bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);}
int main(){try{
    std::string error;s::PropState ground;s::BuildingState created;
    s::reset();check(s::probe_building_placement("Respawn","base",4,5,ground,error),"ordinary world read-only probe rejected");
    check(s::native_calls==1&&!s::observed_apply&&!s::observed_purchase&&s::building_reads==0,"probe dispatched a write");
    check(ground.uid==0&&ground.x==4&&ground.y==7&&ground.z==5,"probe ground differs");
    for(bool apply:{false,true})for(bool purchase:{false,true}){
        if(!apply&&!purchase)continue;
        s::reset();check(!s::placement_operation("SmallInn","base",4,5,apply,purchase,7,created,error)&&s::native_calls==0,
            "ordinary world diagnostic mutation admitted");
        check(error.find("MPTest")!=std::string::npos,"mutation restriction cause lost");
    }
    s::reset();check(!s::diagnostic_place_building("SmallInn","base",4,5,created,error)&&s::native_calls==0,"ordinary purchase wrapper admitted");
    s::BuildingState host{{8,4,7,5},"SmallInn","base"};
    s::reset();check(!s::diagnostic_replicate_building(host,created,error)&&s::native_calls==0,"ordinary replica wrapper admitted");
    s::reset();s::test_folder="MPTestOwned";
    check(s::diagnostic_place_building("SmallInn","base",4,5,created,error)&&s::native_calls==1&&s::observed_apply&&s::observed_purchase&&created.prop.uid==42,"MPTest purchase changed");
    s::reset();s::test_folder="MPTestOwned";
    check(s::diagnostic_replicate_building(host,created,error)&&s::native_calls==1&&s::observed_apply&&!s::observed_purchase&&s::observed_y==7,"MPTest replica changed");
    for(auto flag:{&s::thread_ok,&s::active,&s::folder_ok,&s::read_ok,&s::saves_ok}){
        s::reset();*flag=false;check(!s::probe_building_placement("Respawn","base",4,5,ground,error)&&s::native_calls==0&&!error.empty(),"preflight failure admitted");
    }
    s::reset();s::thread_ok=false;s::probe_building_placement("Respawn","base",4,5,ground,error);
    check(error=="original wrong-thread cause","thread cause lost");
    for(int mode:{0,1,2}){
        s::reset();if(mode==0)s::test_edits=1;if(mode==1)s::test_saves=1;if(mode==2)s::test_locked=1;
        check(!s::probe_building_placement("Respawn","base",4,5,ground,error)&&s::native_calls==0&&error.find("busy")!=std::string::npos,"busy probe admitted");
    }
    for(const auto& type:std::initializer_list<std::string>{"",std::string(257,'a'),std::string("a\0b",3)}){
        s::reset();check(!s::probe_building_placement(type,"base",4,5,ground,error)&&s::native_calls==0,"bad type admitted");
    }
    for(const auto& variant:std::initializer_list<std::string>{std::string(257,'a'),std::string("a\0b",3)}){
        s::reset();check(!s::probe_building_placement("Respawn",variant,4,5,ground,error)&&s::native_calls==0,"bad variant admitted");
    }
    for(float bad:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        s::reset();check(!s::probe_building_placement("Respawn","base",bad,5,ground,error)&&s::native_calls==0,"bad x admitted");
        s::reset();check(!s::probe_building_placement("Respawn","base",4,bad,ground,error)&&s::native_calls==0,"bad z admitted");
    }
    for(int stage=0;stage<=22;++stage){
        if(stage==10)continue;s::reset();s::outcome.stage=stage;
        check(!s::probe_building_placement("Respawn","base",4,5,ground,error)&&s::native_calls==1&&s::building_reads==0&&error.find("stage=")!=std::string::npos,"native failed stage accepted");
    }
    for(bool unlock:{false,true}){
        s::reset();if(unlock)s::outcome.island_unlock_exception=123;else s::outcome.exception=456;
        check(!s::probe_building_placement("Respawn","base",4,5,ground,error)&&s::native_calls==1,"native exception accepted");
        check(error.find(unlock?"island_unlock_exception=123":"exception=456")!=std::string::npos,"native exception cause lost");
    }
    s::reset();s::test_folder="MPTestOwned";s::uid_ok=false;
    check(!s::diagnostic_place_building("SmallInn","base",4,5,created,error)&&s::native_calls==1&&s::building_reads==0,"missing write UID accepted");
    s::reset();s::test_folder="MPTestOwned";s::building_ok=false;
    check(!s::diagnostic_place_building("SmallInn","base",4,5,created,error)&&error=="original building read failure","write readback failure hidden");
    std::cout<<checks<<" actual placement operation/wrapper checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

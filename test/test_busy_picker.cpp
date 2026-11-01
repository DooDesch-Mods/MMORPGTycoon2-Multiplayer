#include <cassert>
#include <cstdint>
#include <string>
#include <nlohmann/json.hpp>
#include <iostream>
int ui_mode=2;void* ui_owner=reinterpret_cast<void*>(0x1234);uint64_t generation=7;
bool choosing=true,pending=false,load_attempted=false,current_host_world=false;
std::string selected_world,selected_file,cause;
bool framework_report=false,framework_creating_world=false,framework_open=true,framework_observed_open=true;
bool framework_browser=true,framework_busy_notice=false;void* framework_picker_restore_owner=nullptr;uint64_t framework_picker_restore_generation=0;
namespace lobby_prepare {bool status_is_handshake(const nlohmann::json& value);}
#include "handshake_body.inc"
template <typename... Args> void discard_log(const Args&...) {}
#define LOG_INFO(...) discard_log(__VA_ARGS__)
#include "actual_body.inc"
void reset(){ui_mode=2;ui_owner=reinterpret_cast<void*>(0x1234);generation=7;choosing=true;pending=load_attempted=current_host_world=false;
    selected_world.clear();selected_file.clear();cause.clear();framework_report=framework_creating_world=false;framework_open=framework_observed_open=framework_browser=true;framework_busy_notice=false;framework_picker_restore_owner=nullptr;}
void rising(){update_busy_picker(true,true,true,true,{{"state",11},{"reloading",true},{"error",""}});assert(!framework_browser&&framework_picker_restore_owner==ui_owner);}
void falling(){update_busy_picker(true,true,true,false,{{"state",0},{"error",""}});}
int main(){
    reset();rising();update_busy_picker(true,true,true,true,{{"state",11},{"reloading",true},{"error",""}});assert(!framework_browser&&framework_picker_restore_owner==ui_owner);falling();falling();assert(framework_browser&&!framework_picker_restore_owner&&!framework_busy_notice);
    for(int n=0;n<12;++n){reset();rising();switch(n){case 0:framework_open=false;break;case 1:framework_observed_open=false;break;
        case 2:framework_report=true;break;case 3:ui_owner=reinterpret_cast<void*>(0x2345);break;case 4:++generation;break;
        case 5:choosing=false;break;case 6:load_attempted=true;break;case 7:current_host_world=true;break;
        case 8:selected_file="world";break;case 9:cause="error";break;case 10:pending=true;break;case 11:ui_mode=3;break;}
        falling();assert(!framework_browser&&!framework_picker_restore_owner);}
    reset();rising();update_busy_picker(true,true,true,false,{{"state",1},{"error",""}});assert(!framework_browser&&!framework_picker_restore_owner);
    reset();rising();update_busy_picker(true,true,true,false,{{"state",0},{"error","failed"}});assert(!framework_browser&&!framework_picker_restore_owner);
    reset();rising();update_busy_picker(false,true,true,false,{{"state",0},{"error",""}});assert(!framework_browser&&!framework_picker_restore_owner);
    reset();rising();update_busy_picker(true,true,false,false,{{"state",0},{"error",""}});assert(!framework_browser&&!framework_picker_restore_owner);
    reset();framework_browser=false;update_busy_picker(true,true,true,true,{{"state",11},{"reloading",true},{"error",""}});falling();assert(!framework_browser);
    reset();update_busy_picker(true,true,true,true,{{"state",3},{"reloading",true},{"error",""}});falling();assert(!framework_browser);
    reset();update_busy_picker(true,true,true,true,{{"state",11},{"reloading",false},{"error",""}});falling();assert(!framework_browser);
    reset();update_busy_picker(true,true,true,true,{{"state",1},{"error",""}});falling();assert(!framework_browser);
    std::cout<<"actual picker transition: PASS\n";
}

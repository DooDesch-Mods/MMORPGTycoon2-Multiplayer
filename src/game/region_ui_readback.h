#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>
namespace mt2mp::game::ui_region_runtime::ui_readback {
using json=nlohmann::json;
template<class Memory>bool read(Memory& memory,uintptr_t image,uintptr_t manager,json& out,std::string& e){
    out=json::object();
    auto field=[&]<class T>(uintptr_t object,size_t offset,const char* name,T& value){
        if(!object||object>UINTPTR_MAX-offset){e="region UI read "+std::string(name)+" address invalid";out["read_failed"]=name;return false;}
        if(!memory.copy(object+offset,&value,sizeof value,e)){e="region UI read "+std::string(name)+": "+e;out["read_failed"]=name;return false;}
        out[name]=value;return true;
    };
    uintptr_t cursor=0,behaviour=0,ap=0,focused=0,handler=0,window=0,owner=0,list=0;
    int32_t raw_count=0,filtered_count=0;uint8_t progress=0,queue=0,allowed=0,visible=0;
    if(!field(manager,0x30,"raw_count",raw_count)||!field(manager,0x48,"filtered_count",filtered_count)||
       !field(manager,0x80,"focused",focused)||!field(manager,0xd1,"setting_selection",progress)||!field(manager,0xd3,"queued_selection",queue)||
       !field(manager,0xb8,"handler",handler)||!field(image,0x13df9d0,"cursor",cursor))return false;
    if(cursor){if(!field(cursor,0x190,"behaviour",behaviour))return false;
        if(behaviour&&(!field(behaviour,0,"behaviour_ap",ap)||!field(behaviour,0x120,"allow_selection",allowed)))return false;}
    if(!handler)return true;
    if(!field(handler,0,"handler_ap",ap))return false;
    out["handler_ap_expected"]=image+0x17317e0;
    if(ap!=image+0x17317e0)return true;
    if(!field(handler,0x10,"window",window))return false;
    if(!window)return true;
    if(!field(window,0,"window_ap",ap))return false;
    out["window_ap_expected"]=image+0x171fad0;
    if(ap!=image+0x171fad0)return true;
    return field(window,0x45f,"visible",visible)&&field(window,0x4e8,"region",owner)&&field(window,0x590,"preset_list",list);
}
inline bool validate(const json& state,uintptr_t image,uintptr_t widget,uintptr_t region,std::string& e,bool require_preset=true){
    auto equal=[&](const char* name,uint64_t required){
        const auto found=state.find(name);
        if(found==state.end()){e="region UI "+std::string(name)+" unavailable required="+std::to_string(required);return false;}
        if(*found!=required){e="region UI "+std::string(name)+" actual="+found->dump()+" required="+std::to_string(required);return false;}return true;
    };
    auto present=[&](const char* name){const auto found=state.find(name);if(found==state.end()||!found->is_number_unsigned()||found->get<uintptr_t>()==0){e="region UI "+std::string(name)+" actual="+(found==state.end()?"unavailable":found->dump())+" required=nonzero";return false;}return true;};
    return equal("raw_count",1)&&equal("focused",widget)&&present("handler")&&equal("handler_ap",image+0x17317e0)&&present("window")&&
        equal("window_ap",image+0x171fad0)&&equal("visible",1)&&equal("region",region)&&(!require_preset||present("preset_list"));
}
}

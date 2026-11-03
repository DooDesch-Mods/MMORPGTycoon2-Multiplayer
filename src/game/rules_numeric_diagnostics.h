#pragma once
#include <cstdint>
#include <string>
#include "vendor/nlohmann/json.hpp"

namespace rules_numeric_diagnostics {
using Json=nlohmann::json;
struct Scope {
    Json*& active;Json* previous;
    Scope(Json*& slot,Json& output,bool enabled):active(slot),previous(slot){
        if(enabled){output={{"phases",Json::array()},{"route",{{"calls",0},{"state","not_entered"}}}};active=&output;}
    }
    ~Scope(){active=previous;}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
inline void route(Json* trace,const char* state,const Json& details=Json::object()){
    if(!trace)return;auto& value=(*trace)["route"];value["state"]=state;
    for(const auto& item:details.items())value[item.key()]=item.value();
}
template<class Read,class Text>
void phase(Json* trace,const char* name,uintptr_t image,uintptr_t window,uintptr_t edit,Read read,Text text){
    if(!trace)return;Json value={{"phase",name}};std::string error,current,baseline;
    uintptr_t focus=0,owner=0,parent=0,vtable=0,lost_focus=0;uint8_t editable=0,editing=0;
    const auto failed=[&](const char* field){value["state"]="failed";value["field"]=field;value["cause"]=error.empty()?"Numeric diagnostic field read failed":error;(*trace)["phases"].push_back(value);};
    if(!read(window,0x3c0,focus,error)){failed("focus");return;}
    if(!read(edit,0x280,owner,error)){failed("owner");return;}
    if(!read(edit,0x278,parent,error)){failed("parent");return;}
    if(!read(edit,0,vtable,error)){failed("vtable");return;}
    if(!read(vtable,0x100,lost_focus,error)){failed("lost_focus");return;}
    if(!read(edit,0x3d8,editable,error)){failed("editable");return;}
    if(!read(edit,0x584,editing,error)){failed("editing");return;}
    if(!text(edit+0x2d0,current,error)){failed("text");return;}
    if(!text(edit+0x4d8,baseline,error)){failed("baseline");return;}
    value.update({{"state","observed"},{"focus",focus},{"focus_matches_edit",focus==edit},{"owner",owner},{"parent",parent},
        {"lost_focus_matches_pinned",lost_focus==image+0x7f1d90},{"editable",editable},{"editing",editing},
        {"text",current},{"baseline",baseline},{"text_differs_from_baseline",current!=baseline}});
    (*trace)["phases"].push_back(value);
}
}

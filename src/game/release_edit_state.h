#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cstdint>
#include <climits>
#include <string>
namespace mt2mp::game::release_edit {
enum class Command:uint8_t {SetType=1,PlannerTitle=2,Changelog=3,Announcement=4,UseCustom=5,ResetCustom=6};
struct Intent {Command command=Command::SetType;int32_t type=0;bool custom=false;std::string text,text_sha256;bool operator==(const Intent&)const=default;};
struct State {
    std::array<int32_t,3> version{},planned{};
    int32_t type=0,state=0,changes=0,features=0,demands=0,history=0,allocated_funds=0;
    std::array<std::string,4> text;
    bool custom=false;
    std::array<uint64_t,4> random{};
    bool operator==(const State&)const=default;
};
struct Defaults {std::string announcement,changelog;bool operator==(const Defaults&)const=default;};
inline bool text(const std::string& v){return v.size()<=50000&&v.find('\0')==std::string::npos;}
inline bool digest(const std::string& v){return v.size()==64&&v.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool valid(const State& s){
    for(auto n:s.version)if(n<0)return false;for(auto n:s.planned)if(n<0)return false;
    for(const auto& value:s.text)if(!text(value))return false;
    return s.type>=0&&s.type<5&&s.state>=0&&s.state<4&&s.changes>=0&&s.changes<=4096&&s.features>=0&&s.features<=4096&&s.demands>=0&&s.demands<=4096&&s.history>=0&&s.history<=4096&&s.allocated_funds>=0;
}
inline bool valid(const Intent& i){
    if(i.command<Command::SetType||i.command>Command::ResetCustom||!text(i.text))return false;
    const bool has_text=i.command==Command::PlannerTitle||i.command==Command::Changelog||i.command==Command::Announcement;
    if(has_text?!digest(i.text_sha256):!i.text_sha256.empty())return false;
    if(i.command==Command::SetType)return i.type>=1&&i.type<=3&&!i.custom&&i.text.empty();
    if(i.type)return false;
    if(i.command==Command::UseCustom)return i.text.empty();
    return !i.custom&&(i.command!=Command::ResetCustom||i.text.empty());
}
inline bool expected_type(const State& before,int32_t type,std::array<int32_t,3>& out){
    out=before.version;const auto index=type==1?2:type==2?1:0;
    if(out[index]==INT32_MAX)return false;++out[index];for(size_t n=index+1;n<out.size();++n)out[n]=0;return true;
}
inline bool transition(const Intent& i,const State& before,const State& after,const Defaults& defaults,bool confirmation_required,std::string& e){
    if(!valid(i)||!valid(before)||!valid(after)||!text(defaults.announcement)||!text(defaults.changelog)){e="Release edit actual intent/state/default values invalid";return false;}
    auto expected=before;
    bool confirmation=false;
    if(i.command==Command::SetType){expected.type=i.type;if(!expected_type(before,i.type,expected.planned)){e="Release edit next native version overflows";return false;}}
    else if(i.command==Command::PlannerTitle)expected.text[0]=i.text;
    else if(i.command==Command::Changelog){expected.text[3]=i.text.empty()?defaults.changelog:i.text;if(!i.text.empty())expected.custom=true;}
    else if(i.command==Command::Announcement)expected.text[2]=i.text.empty()?defaults.announcement:i.text;
    else if(i.command==Command::UseCustom){
        if(i.custom)expected.custom=true;
        else if(before.text[2]==defaults.announcement&&before.text[3]==defaults.changelog)expected.custom=false;
        else {confirmation=true;expected.custom=true;}
    }else {expected.custom=false;expected.text[2]=defaults.announcement;expected.text[3]=defaults.changelog;}
    if(expected!=after||confirmation!=confirmation_required){e="Release edit actual version/text/custom flag/confirmation or unrelated state differs";return false;}e.clear();return true;
}
inline nlohmann::json encode(const Intent& i){return {{"command",uint8_t(i.command)},{"type",i.type},{"custom",i.custom},{"text",i.text},{"text_sha256",i.text_sha256}};}
inline nlohmann::json encode(const State& s){return {{"version",s.version},{"planned",s.planned},{"type",s.type},{"state",s.state},{"changes",s.changes},{"features",s.features},{"demands",s.demands},{"history",s.history},{"allocated_funds",s.allocated_funds},{"text",s.text},{"custom",s.custom},{"random",s.random}};}
struct Proof {
    std::array<int32_t,3> version{},planned{};int32_t type=0;bool custom=false,confirmation_required=false;
    std::array<std::string,4> text_sha256;std::array<std::string,2> defaults_sha256;
    std::string unrelated_sha256;
    bool operator==(const Proof&)const=default;
};
inline bool valid(const Proof& p){for(auto n:p.version)if(n<0)return false;for(auto n:p.planned)if(n<0)return false;for(const auto& v:p.text_sha256)if(!digest(v))return false;for(const auto& v:p.defaults_sha256)if(!digest(v))return false;return p.type>=0&&p.type<5&&digest(p.unrelated_sha256);}
template<class Hash>bool proof(const State& s,const Defaults& defaults,bool confirmation,Hash&& hash,Proof& out,std::string& e){
    if(!valid(s)||!text(defaults.announcement)||!text(defaults.changelog)){e="Release edit proof actual state/defaults invalid";return false;}
    out.version=s.version;out.planned=s.planned;out.type=s.type;out.custom=s.custom;out.confirmation_required=confirmation;
    for(size_t n=0;n<4;++n)out.text_sha256[n]=hash(s.text[n]);out.defaults_sha256={hash(defaults.announcement),hash(defaults.changelog)};
    out.unrelated_sha256=hash(nlohmann::json{s.state,s.changes,s.features,s.demands,s.history,s.allocated_funds}.dump());
    if(!valid(out)){e="Release edit proof hash result invalid";return false;}e.clear();return true;
}
inline bool effect(const Intent& i,const Proof& before,const Proof& after,std::string& e){
    if(!valid(i)||!valid(before)||!valid(after)||before.confirmation_required){e="Release edit complete before/after proof invalid";return false;}
    auto expected=before;expected.defaults_sha256=after.defaults_sha256;
    if(i.command==Command::SetType){State state;state.version=before.version;expected.type=i.type;if(!expected_type(state,i.type,expected.planned)){e="Release edit proof version overflow";return false;}}
    else if(i.command==Command::PlannerTitle)expected.text_sha256[0]=i.text_sha256;
    else if(i.command==Command::Changelog){expected.text_sha256[3]=i.text.empty()?before.defaults_sha256[1]:i.text_sha256;if(!i.text.empty())expected.custom=true;}
    else if(i.command==Command::Announcement)expected.text_sha256[2]=i.text.empty()?before.defaults_sha256[0]:i.text_sha256;
    else if(i.command==Command::UseCustom){if(i.custom)expected.custom=true;else if(before.text_sha256[2]==before.defaults_sha256[0]&&before.text_sha256[3]==before.defaults_sha256[1])expected.custom=false;else{expected.custom=true;expected.confirmation_required=true;}}
    else{expected.custom=false;expected.text_sha256[2]=before.defaults_sha256[0];expected.text_sha256[3]=before.defaults_sha256[1];}
    if(expected!=after){e="Release edit compact original effect/confirmation differs";return false;}e.clear();return true;
}
inline nlohmann::json encode(const Proof& p){return {{"version",p.version},{"planned",p.planned},{"type",p.type},{"custom",p.custom},{"confirmation_required",p.confirmation_required},{"text_sha256",p.text_sha256},{"defaults_sha256",p.defaults_sha256},{"unrelated_sha256",p.unrelated_sha256}};}
}

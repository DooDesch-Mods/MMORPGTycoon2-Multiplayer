#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
namespace mt2mp::game::game_rules {
enum class Command:uint8_t {ApplyXP=1,RespawnSeconds=2,RespawnDelay=3,Features=4};
struct State {
    // Apply copies this complete native block. Word0 has no editable property.
    std::array<uint32_t,14> xp{};int32_t respawn_seconds=0;bool respawn_delay=false;
    std::array<uint8_t,2> features{},available{};
    bool operator==(const State&)const=default;
};
struct Intent {Command command=Command::ApplyXP;State before,after;bool operator==(const Intent&)const=default;};
struct Proof {State state;std::string tutorial_sha256;bool operator==(const Proof&)const=default;};
inline bool valid(const State& value){if(value.respawn_seconds<0)return false;for(size_t i=1;i<value.xp.size();++i)if(value.xp[i]>uint32_t(INT32_MAX))return false;for(auto v:value.features)if(v>1)return false;for(auto v:value.available)if(v>1)return false;return true;}
inline bool valid(const Intent& value){
    if(!valid(value.before)||!valid(value.after)||(value.before==value.after&&value.command!=Command::Features))return false;auto expected=value.before;
    switch(value.command){
    case Command::ApplyXP:if(value.before.xp[0]!=value.after.xp[0]||int32_t(value.after.xp[1]+value.after.xp[2])<=0)return false;expected.xp=value.after.xp;break;
    case Command::RespawnSeconds:expected.respawn_seconds=value.after.respawn_seconds;break;
    case Command::RespawnDelay:expected.respawn_delay=value.after.respawn_delay;break;
    case Command::Features:if(!value.before.available[0]&&!value.before.available[1])return false;for(size_t i=0;i<2;++i)if(value.before.available[i])expected.features[i]=value.after.features[i];break;
    default:return false;}
    return expected==value.after;
}
inline bool digest(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool valid(const Proof& value){return valid(value.state)&&digest(value.tutorial_sha256);}
inline bool effect(const Intent& intent,const Proof& before,const Proof& after){return valid(intent)&&valid(before)&&valid(after)&&before.state==intent.before&&after.state==intent.after&&(intent.command==Command::Features||before.tutorial_sha256==after.tutorial_sha256);}
inline uint8_t feature_calls(const State& before){return uint8_t(before.available[0]|(before.available[1]<<1));}
inline uint8_t feature_events(const State& before){return uint8_t((before.available[0]&&!before.features[0]?1:0)+(before.available[1]&&!before.features[1]?1:0));}
inline nlohmann::json encode(const State& value){return {{"xp",value.xp},{"respawn_seconds",value.respawn_seconds},{"respawn_delay",value.respawn_delay},{"features",value.features},{"available",value.available}};}
inline nlohmann::json encode(const Intent& value){return {{"command",uint8_t(value.command)},{"before",encode(value.before)},{"after",encode(value.after)}};}
inline nlohmann::json encode(const Proof& value){return {{"rules",encode(value.state)},{"tutorial_sha256",value.tutorial_sha256}};}
namespace codec {
using json=nlohmann::json;
inline uint32_t number(const json& v,uint32_t max){if(!v.is_number_integer()||(v.is_number_unsigned()?v.get<uint64_t>()>max:v.get<int64_t>()<0||uint64_t(v.get<int64_t>())>max))throw std::runtime_error("Design rule integer outside native domain");return v.get<uint32_t>();}
inline State state(const json& j){if(!j.is_object()||j.size()!=5)throw std::runtime_error("Design rules require exact five fields");State out;
    const auto& xp=j.at("xp");if(!xp.is_array()||xp.size()!=14)throw std::runtime_error("Design XP requires complete14-word native block");for(size_t i=0;i<14;++i)out.xp[i]=number(xp[i],i?uint32_t(INT32_MAX):UINT32_MAX);
    out.respawn_seconds=int32_t(number(j.at("respawn_seconds"),INT32_MAX));if(!j.at("respawn_delay").is_boolean())throw std::runtime_error("Design respawn delay requires Boolean");out.respawn_delay=j.at("respawn_delay").get<bool>();
    for(auto [name,target]:{std::pair{"features",&out.features},std::pair{"available",&out.available}}){const auto& a=j.at(name);if(!a.is_array()||a.size()!=2)throw std::runtime_error("Design features require exact two native entries");for(size_t i=0;i<2;++i)(*target)[i]=uint8_t(number(a[i],1));}return out;}
inline Intent intent(const json& j){if(!j.is_object()||j.size()!=3)throw std::runtime_error("Design intent requires exact command/before/after");Intent out{Command(number(j.at("command"),4)),state(j.at("before")),state(j.at("after"))};if(!valid(out))throw std::runtime_error("Design intent changes another rule family or has no native change");return out;}
inline Proof proof(const json& j){if(!j.is_object()||j.size()!=2)throw std::runtime_error("Design proof requires exact full rules/tutorial digest");Proof out{state(j.at("rules")),j.at("tutorial_sha256").get<std::string>()};if(!valid(out))throw std::runtime_error("Design complete proof invalid");return out;}
}
struct Pin {uintptr_t software=0,design=0,features=0,tree=0;std::array<uintptr_t,2> tech{},records{};bool operator==(const Pin&)const=default;};
template<class Read,class Copy>bool array(Read& read,Copy& copy,uintptr_t object,size_t offset,std::vector<uintptr_t>& out,std::string& error){
    uintptr_t data=0;int32_t count=0,capacity=0;if(!read(object,offset,data,error)||!read(object,offset+8,count,error)||!read(object,offset+12,capacity,error))return false;
    if(count<0||count>4096||capacity<count||capacity>65536||(!data&&capacity)||data>UINTPTR_MAX-size_t(capacity)*8){error="Design current owned catalogue extent invalid";return false;}out.resize(size_t(count));return !count||copy(data,out.data(),out.size()*8,error);
}
template<class Read,class Copy,class Text,class Find>bool read(Read& read,Copy& copy,Text& text,Find& find,uintptr_t image,Pin& pin,State& out,std::string& error){
    out={};pin={};uintptr_t ap=0;
    const auto owner=[&](size_t global,uintptr_t expected,uintptr_t& object){return read(image,global,object,error)&&read(object,0,ap,error)&&(ap==image+expected||(error="Design actual current singleton class differs",false));};
    if(!owner(0x13df500,0x1715450,pin.software)||!owner(0x13df9e0,0x1786ef0,pin.design)||!owner(0x13df3c0,0x16ffdd0,pin.features)||!owner(0x13df250,0x16dc400,pin.tree))return false;
    if(pin.design!=pin.software+0x98){error="Design current Software embedded owner differs";return false;}
    if(!read(pin.design+0x170,0,ap,error)||ap!=image+0x16d9f40||!read(pin.design+0x1c0,0,ap,error)||ap!=image+0x173cd30){if(error.empty())error="Design embedded XP/Respawn native classes differ";return false;}
    uint8_t delay=0;if(!read(pin.design,0x188,out.xp,error)||!read(pin.design,0x1d8,out.respawn_seconds,error)||!read(pin.design,0x1dc,delay,error)||!read(pin.features,0x18,out.features,error))return false;
    if(delay>1){error="Design actual respawn delay is not Boolean";return false;}out.respawn_delay=delay!=0;
    std::vector<uintptr_t> techs,records;if(!array(read,copy,pin.tree,0x10,techs,error)||!array(read,copy,pin.software,0x280,records,error))return false;
    for(size_t i=0;i<2;++i){const std::string key=i?"parties":"pvpduels";if(!find(pin.tree,key,pin.tech[i],error))return false;if(!pin.tech[i])continue;
        std::string name;const auto tech=pin.tech[i];if(std::count(techs.begin(),techs.end(),tech)!=1||!read(tech,0,ap,error)||(ap!=image+0x1744c40&&ap!=image+0x16dc330&&ap!=image+0x16e0ac0&&ap!=image+0x16fdb10)||!text(tech+0x20,name,error)||name!=key||!read(tech,0x1e8,pin.records[i],error)){if(error.empty())error="Design current keyed TechTree owner differs";return false;}
        const auto record=pin.records[i];uintptr_t back=0;std::array<uint8_t,3> flags{};
        if(std::count(records.begin(),records.end(),record)!=1||!read(record,0,ap,error)||ap!=image+0x16e79d0||!read(record,0x60,back,error)||back!=tech||!text(record+0x38,name,error)||name!=key||!read(record,0x58,flags,error)){if(error.empty())error="Design current Software TechRecord reciprocity differs";return false;}
        for(auto flag:flags)if(flag>1){error="Design required technology flag is not Boolean";return false;}out.available[i]=uint8_t(flags[1]||flags[2]);
    }
    if(!valid(out)){error="Design actual native rule value outside domain";return false;}error.clear();return true;
}
}

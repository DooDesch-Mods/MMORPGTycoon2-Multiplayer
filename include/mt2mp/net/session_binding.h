#pragma once
#include "vendor/nlohmann/json.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mt2mp::net::session_binding {
using Slots=std::array<uint64_t,5>;
struct Binding {
    uint64_t lobby=0,session=0;
    Slots slots{};
    bool operator==(const Binding&)const=default;
};
inline bool valid(const Slots& slots){
    if(slots[0]||!slots[1])return false;
    std::vector<uint64_t> members;
    for(size_t i=1;i<slots.size();++i)if(slots[i])members.push_back(slots[i]);
    std::sort(members.begin(),members.end());
    return std::adjacent_find(members.begin(),members.end())==members.end();
}
inline uint8_t mask(const Slots& slots){
    uint8_t result=0;
    for(size_t i=1;i<slots.size();++i)if(slots[i])result|=uint8_t(1u<<(i-1));
    return result;
}
inline bool topology(std::vector<uint64_t> members,uint64_t host,Slots& out,std::string& error){
    out={};
    if(members.empty()||members.size()>4||!host||std::find(members.begin(),members.end(),host)==members.end()){
        error="Steam lobby requires its Host and at most3 other members";return false;
    }
    std::sort(members.begin(),members.end());
    if(!members.front()||std::adjacent_find(members.begin(),members.end())!=members.end()){
        error="Steam lobby physical members contain zero or duplicate IDs";return false;
    }
    Slots slots{};slots[1]=host;size_t index=2;
    for(auto member:members)if(member!=host)slots[index++]=member;
    out=slots;error.clear();return true;
}
inline nlohmann::json encode(const Binding& binding,const std::string& version,const std::string& game){
    return {{"revision",2},{"lobby",binding.lobby},{"session",binding.session},{"host",binding.slots[1]},
        {"mod",version},{"game",game},{"slots",binding.slots}};
}
inline bool decode(const std::string& text,const std::string& version,const std::string& game,Binding& out,std::string& error){
    try{
        if(text.empty()||text.size()>2048){error="Steam binding metadata length outside1..2048";return false;}
        const auto value=nlohmann::json::parse(text);
        if(!value.is_object()||value.size()!=7){error="Steam binding metadata exact fields required";return false;}
        for(const char* key:{"revision","lobby","session","host","mod","game","slots"})
            if(!value.contains(key)){error=std::string("Steam binding metadata missing ")+key;return false;}
        if(!value.at("revision").is_number_integer()||value.at("revision")!=2||value.at("mod")!=version||value.at("game")!=game){error="Steam binding protocol/mod/game differs";return false;}
        const auto integer=[](const nlohmann::json& number,uint64_t& result){
            if(!number.is_number_integer()||(!number.is_number_unsigned()&&number.get<int64_t>()<0))return false;
            result=number.get<uint64_t>();return true;
        };
        Binding candidate;uint64_t host=0;
        if(!integer(value.at("lobby"),candidate.lobby)||!candidate.lobby||!integer(value.at("session"),candidate.session)||!candidate.session||
           !integer(value.at("host"),host)||!host){error="Steam binding lobby/session/Host integer invalid";return false;}
        const auto& slots=value.at("slots");
        if(!slots.is_array()||slots.size()!=candidate.slots.size()){error="Steam binding exact five physical slots required";return false;}
        for(size_t i=0;i<candidate.slots.size();++i)if(!integer(slots[i],candidate.slots[i])){error="Steam binding slot integer invalid";return false;}
        if(!valid(candidate.slots)||host!=candidate.slots[1]){error="Steam binding Host/physical slots invalid";return false;}
        out=candidate;error.clear();return true;
    }catch(const std::exception& cause){error=std::string("Steam binding decode: ")+cause.what();return false;}
}
enum class Change {Same,Members,HostLost,LobbyLost};
inline Change compare(const Binding& current,uint64_t actual_lobby,const Slots& actual){
    if(!actual_lobby||actual_lobby!=current.lobby)return Change::LobbyLost;
    if(!valid(actual)||actual[1]!=current.slots[1])return Change::HostLost;
    return actual==current.slots?Change::Same:Change::Members;
}
}

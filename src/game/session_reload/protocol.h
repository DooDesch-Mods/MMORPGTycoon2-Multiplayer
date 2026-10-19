#pragma once
#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <string>

namespace mt2mp::game::session_reload {
inline constexpr uint8_t channel=14;
enum class Kind:uint8_t {Begin=1,Ready,Failed,Request};
struct Packet {
    Kind kind=Kind::Begin;
    uint64_t session=0;
    uint8_t required=0;
    std::string token,cause;
    bool operator==(const Packet&)const=default;
};
inline bool uuid(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    }
    return true;
}
inline bool valid(const Packet& value){
    if(!value.session||!uuid(value.token)||value.required>15||!(value.required&1)||value.cause.size()>512)return false;
    switch(value.kind){
    case Kind::Begin:case Kind::Ready:case Kind::Request:return value.cause.empty();
    case Kind::Failed:return !value.cause.empty();
    }
    return false;
}
inline nlohmann::json encode(const Packet& value){
    return {{"revision",1},{"kind",uint64_t(value.kind)},{"session",value.session},
        {"required",value.required},{"token",value.token},{"cause",value.cause}};
}
inline bool decode(const uint8_t* bytes,uint32_t size,Packet& out,std::string& error){
    if(!bytes||!size||size>2048){error="world reload packet length outside1..2048";return false;}
    try{
        const auto value=nlohmann::json::parse(bytes,bytes+size);
        if(!value.is_object()||value.size()!=6){error="world reload exact packet fields required";return false;}
        for(const char* key:{"revision","kind","session","required","token","cause"})
            if(!value.contains(key)){error=std::string("world reload packet missing ")+key;return false;}
        const auto integer=[](const nlohmann::json& v,uint64_t& result){
            if(!v.is_number_integer()||(!v.is_number_unsigned()&&v.get<int64_t>()<0))return false;
            result=v.get<uint64_t>();return true;
        };
        uint64_t revision=0,kind=0,session=0,required=0;
        if(!integer(value.at("revision"),revision)||revision!=1||!integer(value.at("kind"),kind)||kind<1||kind>4||
           !integer(value.at("session"),session)||!integer(value.at("required"),required)||required>15){
            error="world reload packet numeric field invalid";return false;
        }
        Packet candidate{Kind(kind),session,uint8_t(required),value.at("token").get<std::string>(),value.at("cause").get<std::string>()};
        if(!valid(candidate)){error="world reload packet identity/quorum/cause invalid";return false;}
        out=std::move(candidate);error.clear();return true;
    }catch(const std::exception& cause){error=std::string("world reload packet decode: ")+cause.what();return false;}
}
class Barrier {
public:
    bool begin(const Packet& packet,uint8_t local,std::string& error){
        if(!valid(packet)||packet.kind!=Kind::Begin||local<1||local>4||!(packet.required&(1u<<(local-1)))){
            error="world reload barrier requires current Host begin and local membership";return false;
        }
        if(active_){
            if(begin_==packet&&local_==local){error.clear();return true;}
            error="different world reload is already active";return false;
        }
        begin_=packet;local_=local;required_=packet.required;ready_=0;active_=true;error.clear();return true;
    }
    bool acknowledge(uint64_t sender,const Packet& packet,std::string& error){
        if(!active_||!valid(packet)||packet.kind!=Kind::Ready||packet.session!=begin_.session||packet.token!=begin_.token||
           packet.required!=begin_.required||sender<1||sender>4||!(required_&(1u<<(sender-1)))){
            error="world reload Ready differs from current physical barrier";return false;
        }
        ready_|=uint8_t(1u<<(sender-1));error.clear();return true;
    }
    bool retain_members(uint8_t still_present,std::string& error){
        if(!active_||!(still_present&1)||still_present>15){error="world reload original Host no longer present";return false;}
        required_&=still_present;ready_&=required_;error.clear();return true;
    }
    bool complete()const{return active_&&ready_==required_;}
    bool active()const{return active_;}
    const Packet& packet()const{return begin_;}
    Packet ready_packet()const{auto result=begin_;result.kind=Kind::Ready;return result;}
    uint8_t pending()const{return uint8_t(required_&~ready_);}
    void clear(){begin_={};local_=required_=ready_=0;active_=false;}
private:
    Packet begin_;
    uint8_t local_=0,required_=0,ready_=0;
    bool active_=false;
};
}

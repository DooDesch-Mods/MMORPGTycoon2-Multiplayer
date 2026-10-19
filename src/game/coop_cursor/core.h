#pragma once
#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace mt2mp::game::coop_cursor {
inline constexpr uint8_t channel=13;
inline constexpr uint64_t stale_after_ms=1500;
struct Context {
    uint64_t session=0,local_peer=0;
    uint8_t peer_mask=0;
    std::string snapshot,scene;
    bool operator==(const Context&)const=default;
};
struct Sample {
    std::optional<std::array<float,3>> position;
    bool operator==(const Sample&)const=default;
};
struct Packet {
    uint64_t session=0,sequence=0;
    std::string snapshot,scene;
    Sample sample;
};
inline bool identifier(const std::string& value){
    if(value.empty()||value.size()>160)return false;
    for(const unsigned char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
        (c>='0'&&c<='9')||c==':'||c=='-'||c=='_'||c=='.'||c=='/'||c=='{'||c=='}'||c=='['||c==']'))return false;
    return true;
}
inline bool valid(const Sample& sample){
    if(sample.position)for(float value:*sample.position)if(!std::isfinite(value)||std::abs(value)>100000000)return false;
    return true;
}
inline bool valid(const Context& context){
    return context.session&&context.local_peer>=1&&context.local_peer<=4&&
        !(context.peer_mask&0xf0)&&bool(context.peer_mask&1)&&
        bool(context.peer_mask&(1u<<(context.local_peer-1)))&&identifier(context.snapshot)&&identifier(context.scene);
}
inline nlohmann::json encode(const Packet& packet){
    return {{"revision",1},{"session",packet.session},{"snapshot",packet.snapshot},
        {"scene",packet.scene},{"sequence",packet.sequence},
        {"position",packet.sample.position?nlohmann::json(*packet.sample.position):nlohmann::json(nullptr)}};
}
inline bool decode(const uint8_t* bytes,uint32_t size,Packet& out,std::string& error){
    const auto fail=[&](const std::string& cause){error=cause;return false;};
    if(!bytes||!size||size>1024)return fail("cursor packet length outside1..1024");
    try{
        const auto value=nlohmann::json::parse(bytes,bytes+size);
        if(!value.is_object()||value.size()!=6)return fail("cursor packet exact fields required");
        for(const char* field:{"revision","session","snapshot","scene","sequence","position"})
            if(!value.contains(field))return fail(std::string("cursor packet missing ")+field);
        if(value.at("revision")!=1)return fail("cursor packet revision differs");
        const auto positive=[&](const char* field,uint64_t& result){
            const auto& v=value.at(field);
            if(!v.is_number_integer()||(!v.is_number_unsigned()&&v.get<int64_t>()<=0))return false;
            result=v.get<uint64_t>();return result!=0;
        };
        Packet packet;
        if(!positive("session",packet.session)||!positive("sequence",packet.sequence))return fail("cursor packet positive session/sequence required");
        packet.snapshot=value.at("snapshot").get<std::string>();packet.scene=value.at("scene").get<std::string>();
        if(!identifier(packet.snapshot)||!identifier(packet.scene))return fail("cursor packet snapshot/scene invalid");
        const auto& position=value.at("position");
        if(!position.is_null()){
            if(!position.is_array()||position.size()!=3)return fail("cursor packet exact world position required");
            std::array<float,3> point{};
            for(size_t i=0;i<point.size();++i){
                if(!position[i].is_number())return fail("cursor world position numeric required");
                const double coordinate=position[i].get<double>();
                if(!std::isfinite(coordinate)||std::abs(coordinate)>100000000)return fail("cursor world position outside finite bound");
                point[i]=float(coordinate);
            }
            packet.sample.position=point;
        }
        if(!valid(packet.sample))return fail("cursor world position invalid");
        out=std::move(packet);error.clear();return true;
    }catch(const std::exception& cause){return fail(std::string("cursor packet decode: ")+cause.what());}
}
enum class Reception { Applied,Stale,OtherWorld,Rejected };
struct Remote {
    uint64_t sequence=0,received_ms=0;
    std::string scene;
    Sample sample;
};
class Core {
public:
    void configure(const Context& context){
        if(!valid(context)){
            context_={};
            for(auto& peer:remote_)peer.sample={};
            return;
        }
        if(context.session!=identity_.session||context.snapshot!=identity_.snapshot||context.local_peer!=identity_.local_peer)remote_={};
        context_=identity_=context;
        for(size_t peer=1;peer<remote_.size();++peer)
            if(peer==context.local_peer||!(context.peer_mask&(1u<<(peer-1))))remote_[peer]={};
    }
    bool publish(const Sample& sample,Packet& packet,std::string& error){
        if(!valid(context_)||!valid(sample)||next_==UINT64_MAX){error="cursor publication requires active context, finite sample and sequence space";return false;}
        packet={context_.session,++next_,context_.snapshot,context_.scene,sample};error.clear();return true;
    }
    Reception receive(uint64_t sender,const Packet& packet,uint64_t now,std::string& error){
        if(!valid(context_)||sender<1||sender>4||sender==context_.local_peer||!(context_.peer_mask&(1u<<(sender-1)))||
           !packet.sequence||!identifier(packet.scene)||!valid(packet.sample)){
            error="cursor sender/context/sample invalid";return Reception::Rejected;
        }
        if(packet.session!=context_.session||packet.snapshot!=context_.snapshot){error.clear();return Reception::OtherWorld;}
        auto& row=remote_[sender];
        if(packet.sequence<=row.sequence){error.clear();return Reception::Stale;}
        row={packet.sequence,now,packet.scene,packet.sample};error.clear();return Reception::Applied;
    }
    std::optional<std::array<float,3>> visible(uint64_t peer,uint64_t now)const{
        if(!valid(context_)||peer<1||peer>4||peer==context_.local_peer||!(context_.peer_mask&(1u<<(peer-1))))return {};
        const auto& row=remote_[peer];
        if(!row.sequence||row.scene!=context_.scene||now<row.received_ms||now-row.received_ms>=stale_after_ms)return {};
        return row.sample.position;
    }
    const Context& context()const{return context_;}
    const std::array<Remote,5>& remotes()const{return remote_;}
private:
    Context context_;
    Context identity_;
    std::array<Remote,5> remote_{};
    uint64_t next_=0;
};
}

#pragma once
#include "gm_ticket_state.h"
#include "release_graph.h"

namespace mt2mp::game::gm_ticket {
struct Publication {
    State state;release_graph::Value queue;
    bool operator==(const Publication&)const=default;
};
inline bool valid(const Publication& value,std::string& error){
    if(value.queue.kind!=release_graph::Kind::GameMasterTickets)return fail(error,"GM publication carries another native Record value family");
    return valid(value.state,error)&&release_graph::valid(value.queue,error);
}
inline nlohmann::json encode(const Publication& value){return {{"state",encode(value.state)},{"queue",release_graph::encode(value.queue)}};}
inline bool decode(const nlohmann::json& source,Publication& out,std::string& error){
    out={};try{
        if(!source.is_object()||source.size()!=2||!source.contains("state")||!source.contains("queue"))return fail(error,"GM publication requires exact state and queue fields");
        Publication value;if(!decode(source.at("state"),value.state,error))return false;value.queue=release_graph::decode(source.at("queue"));
        if(!valid(value,error))return false;out=std::move(value);return true;
    }catch(const std::exception& exception){error=std::string("GM publication decode: ")+exception.what();return false;}
}
}

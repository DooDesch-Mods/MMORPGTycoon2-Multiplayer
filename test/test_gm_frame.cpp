#include "live_host_state/state.h"
#include "gm_ticket_publication.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace gm=mt2mp::game::gm_ticket;
namespace graph=mt2mp::game::release_graph;
namespace {
unsigned comparisons=0;
void need(bool value,const std::string& cause){++comparisons;if(!value)throw std::runtime_error(cause);}
gm::Publication publication(){gm::Publication out;out.queue.kind=graph::Kind::GameMasterTickets;
    out.queue.records.push_back({SIZE_MAX,0,{1,0,"vsObjectArray<mmoTicket>"},{}});
    for(int32_t slot=0;slot<50;++slot){gm::MasterValue master;master.slot=slot;master.orientation={0,0,0,1};master.scale={1,1,1};out.state.masters.push_back(master);}
    return out;
}
void codec(){std::string error;live::Frame frame;frame.session=77;frame.epoch=3;frame.source_generation=9;frame.revision=1;
    frame.snapshot="10000000-0000-0000-0000-000000000001";frame.values.calendar={1,1,1,0,0,7};frame.management.emplace();
    frame.management->gm_employment.resize(50);frame.management->gm=publication();
    auto& queue=frame.management->gm->queue;
    // Native ancestry stays in flat parent indices even for long-lived queues.
    for(size_t row=1;row<40;++row)queue.records.push_back({row-1,0,{1,0,"ticket"},{{1,0,"value"}}});
    std::vector<uint8_t> bytes;live::Frame decoded;
    need(live::encode(frame,bytes,error)&&live::decode(bytes.data(),bytes.size(),decoded,error)&&decoded==frame,error);
    auto wire=nlohmann::json::parse(bytes);need(wire.at("schema")==21,"GM full frame must select schema21");
    auto reject=[&](nlohmann::json invalid,const char* cause){const auto text=invalid.dump();need(!live::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),cause);};
    auto invalid=wire;invalid["schema"]=20;reject(invalid,"old Frame20 accepted GM values");
    invalid=wire;invalid["management"]["gm"]["queue"]["kind"]=0;reject(invalid,"GM accepted a Release Manager graph");
    invalid=wire;invalid["management"]["gm"]["state"]["masters"][0]["employed"]=true;reject(invalid,"GM employment disagreed with the native Management pool");
    invalid=wire;invalid["management"].erase("gm");reject(invalid,"Frame21 accepted missing GM publication");
    invalid=wire;invalid["management"]["gm"]["extra"]=0;reject(invalid,"unknown GM publication field accepted");
    frame.management->gm.reset();need(live::encode(frame,bytes,error)&&live::decode(bytes.data(),bytes.size(),decoded,error)&&decoded==frame,error);
    need(nlohmann::json::parse(bytes).at("schema")==19,"legacy Management19 changed without GM values");
}
}
int main(){try{codec();std::cout<<comparisons<<" full GM Frame21 and legacy Management assertions passed\n";return 0;}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

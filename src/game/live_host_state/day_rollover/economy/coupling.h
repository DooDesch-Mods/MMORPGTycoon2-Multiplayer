#pragma once
#include "codec.h"
#include "live_host_state/party_port.h"
#include "live_host_state/day_rollover/sync.h"
namespace mt2mp::game::live_host_state::day::economy {
// This is a field provider for the existing Bridge, not an action authority.
class Coupling {
    day::Sync& day_;
    Port& port_;
    live_host_state::PartyPort* parties_=nullptr;
    Apply apply_;
    bool locked_=false;
    std::string capture_cause_;
    Receipt receipt_;
    mt2mp::subscriber_lifecycle::additions::Cache additions_;
    nlohmann::json addition_receipt_=nullptr,next_id_receipt_=nullptr;
public:
    Coupling(day::Sync& sync,Port& port):day_(sync),port_(port){}
    bool acknowledge_subscribers(uint8_t peer,uint8_t peers,uint64_t revision,const nlohmann::json& rows,std::string& e){return additions_.acknowledge(peer,peers,revision,rows,e);}
    nlohmann::json addition_receipt()const{return {{"last_creation",addition_receipt_},{"last_counter",next_id_receipt_},{"cache",additions_.status()}};}
    void subscriber_frame_committed(uint64_t revision){for(auto* receipt:{&addition_receipt_,&next_id_receipt_})if(!receipt->is_null()&&!receipt->value("frame_committed",false)){(*receipt)["frame_committed"]=true;(*receipt)["frame_revision"]=revision;}}
    bool bind_parties(live_host_state::PartyPort& value,std::string& e){
        if(parties_&&parties_!=&value){e="economic Party provider already bound";return false;}
        if(!port_.bind_party_references(&value,e))return false;parties_=&value;e.clear();return true;
    }
    bool capture(const Target&,State&,std::string&);
    bool read_current(const Frame& lineage,State&,std::string&);
    bool before_day(const State&,Owners&,std::string&);
    bool after_day(const State&,const Owners&,std::string&);
    bool rebind_snapshot(std::string&);
    bool locked()const{return locked_||apply_.locked()||additions_.locked();}
    const Receipt& receipt()const{return receipt_;}
};
}

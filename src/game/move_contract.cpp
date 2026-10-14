#include "move_contract.h"
namespace mt2mp::game::ui_move_runtime::contract {
bool cleaned(uintptr_t image,const move_kiss::Intent& i,const CleanupView& c,std::string& e){
    if(c.addresspoint!=image+0x172f1f0){e="move cleanup behaviour identity changed";return false;}
    if(c.records!=i.record||c.capacity<1){e="move cleanup record storage changed";return false;}
    if(c.count!=0||c.owned_first||c.owned_second||c.options){e="move native Deactivate/records/D0 cleanup not returned";return false;}
    if(c.current_behaviour==i.behaviour){e="move native Cursor still owns captured behaviour";return false;}
    return true;
}
bool pending(const nlohmann::json& s,const action_session::Request& req,uint64_t& revision,std::string& e){
    revision=0;
    if(!s.value("started",false)||s.value("locked",true)||!s.value("pending",false)||s.value("action_uuid",std::string{})!=req.action_uuid){e="move has no exact pending Controller request";return false;}
    if(!s.contains("host_revision")||!s.at("host_revision").is_number_unsigned()){e="move Controller revision is not uint64";return false;}
    revision=s.at("host_revision").get<uint64_t>();
    if(!revision){e="move Controller revision is zero";return false;}
    return true;
}
}

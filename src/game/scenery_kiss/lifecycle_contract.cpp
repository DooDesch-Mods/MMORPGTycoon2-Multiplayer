#include "lifecycle_contract.h"
namespace mt2mp::scenery_kiss {
bool classify_scenery(uintptr_t image,uintptr_t addresspoint,uint64_t uid,const game::native::BuildingState& actual,bool& supported,std::string& e){
    supported=false;if(!image||image>UINTPTR_MAX-0x16c1390||addresspoint!=image+0x16c1390||!uid||actual.prop.uid!=uid){e="native Scenery actual class or UID differs";return false;}
    if(actual.dungeon_membership||actual.dungeon_instance_membership){e="native Scenery belongs to a separate dungeon world";return false;}
    State state{actual.type,actual.variant,actual.prop.x,actual.prop.y,actual.prop.z,actual.rotation,actual.scale,true};
    if(!valid_state(state,e))return false;supported=true;return true;
}
uintptr_t can_move_rva(uintptr_t image,uintptr_t ap)noexcept {
    if(!image||image>UINTPTR_MAX-0x16d93c0)return 0;
    if(ap==image+0x16c1390)return 0xcf3130;
    if(ap==image+0x16d93c0)return 0xd31530;
    return 0;
}
bool place_cleanup(const nlohmann::json& r,bool scenery,std::string& e){
    const auto stage=r.find("private_stage");
    if(stage==r.end()||!stage->is_number_integer()||*stage!=(scenery?19:18)){e="native Place original cleanup stage differs";return false;}
    if(!scenery)return true;
    for(const auto* flag:{"update_attempted","update_returned","cursor_dtor_attempted","cursor_dtor_returned","options_dtor_attempted","options_dtor_returned"}){
        auto v=r.find(flag);if(v==r.end()||!v->is_boolean()||*v!=true){e=std::string("native Scenery original cleanup flag unconfirmed: ")+flag;return false;}}
    for(const auto* field:{"exception","cleanup_exception"}){auto v=r.find(field);if(v==r.end()||!v->is_number_integer()||*v!=0){e=std::string("native Scenery original cleanup exception differs: ")+field;return false;}}
    return true;
}
bool metadata_matches(const nlohmann::json& m,const game::native::BuildingState& b){
    for(const auto* key:{"type","variant","x","y","z"})if(!m.contains(key))return false;
    if(m.at("type")!=b.type||m.at("variant")!=b.variant||m.at("x")!=b.prop.x||m.at("y")!=b.prop.y||m.at("z")!=b.prop.z)return false;
    if(m.contains("rotation")&&m.at("rotation")!=b.rotation)return false;
    if(m.contains("scale")&&m.at("scale")!=b.scale)return false;
    return true;
}
}

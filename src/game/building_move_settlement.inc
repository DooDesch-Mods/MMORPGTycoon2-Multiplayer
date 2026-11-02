#include "scenery_kiss/native_catalog.h"
#include "multiplayer_world/native_binding_adapter.h"

namespace mt2mp::game::ui_region_runtime {
namespace {
uint32_t moved_building_ground(uintptr_t image,uintptr_t root,const std::array<float,3>& position,float& height){
    __try{height=reinterpret_cast<float(*)(void*,const void*)>(image+0x508390)(reinterpret_cast<void*>(root),position.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t selection_building_lookup(host::FindUid find,uint64_t uid,uintptr_t& object){
    __try{object=uintptr_t(find(uid));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
template<class Ground>bool validate_moved_height(const action::BuildingState& immediate,const action::BuildingState& settled,
                                               const json& publication,Ground&& ground,json& proof,std::string& e){
    proof={{"confirmed",false},{"height_changed",false}};
    auto expected=immediate;expected.y=settled.y;
    if(!immediate.alive||!settled.alive||!std::isfinite(immediate.y)||!std::isfinite(settled.y)||expected!=settled)
        return fail(e,"Building settlement changed type, XZ, rotation, scale or liveness");
    if(!publication.value("confirmed",false))return fail(e,"Building settlement lacks completed original terrain publication");
    if(immediate.y!=settled.y){
        terrain_history_height::PublicationScope scope;
        if(!terrain_history_height::publication_scope(publication,scope,e))return false;
        terrain_scenery_diagnostic::Row position;
        position.pose[0]=std::bit_cast<uint32_t>(settled.x);position.pose[2]=std::bit_cast<uint32_t>(settled.z);
        if(!scope.contains(position))return fail(e,"Building changed height outside original publication boxes");
        float native_height=0;
        if(!ground(native_height,e))return false;
        if(!std::isfinite(native_height)||std::bit_cast<uint32_t>(native_height)!=std::bit_cast<uint32_t>(settled.y))
            return fail(e,"Building settled height differs from original GetHeightAt: actual_bits="+
                std::to_string(std::bit_cast<uint32_t>(settled.y))+" required_bits="+std::to_string(std::bit_cast<uint32_t>(native_height)));
        proof["height_changed"]=true;proof["before_y_bits"]=std::bit_cast<uint32_t>(immediate.y);
        proof["native_ground_bits"]=std::bit_cast<uint32_t>(native_height);
    }
    proof["confirmed"]=true;return true;
}
}
bool settle_building_move(const action::Binding& binding,const action::Request& request,uint64_t uid,
                         const action::BuildingState& immediate,action::BuildingState& settled,json& receipt,std::string& e){
    receipt={{"confirmed",false}};settled={};auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready.load()||GetCurrentThreadId()!=r->thread||!uid||request.dungeon_building||
       (request.action!=action::Action::MoveBuilding&&request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding))
        return fail(e,"Building terrain settlement requires its ordered Root Move owner");
    auto owner=[&]{return action::tick_commands::authority::matches(binding,request,e)&&
        remove_diagnostics::current_registry_world(request.snapshot_uuid,binding.native_epoch,binding.native_generation,e);};
    host::WorldState before,after;native::BuildingState original,actual;
    if(!owner()||!world(*r,before,e)||!scenery_kiss::read_action_prop(uid,original,e))return false;
    if(original.prop.uid!=uid||world_state::building_state(original)!=immediate||original.dungeon_membership||original.dungeon_instance_membership)
        return fail(e,"Building terrain settlement immediate UID or complete pose differs");
    if(!terrain_settlement::enable(*r,e)||!terrain_settlement::run(*r,request.action_uuid,true,receipt,e))return false;
    if(!owner()||!world(*r,after,e)||!host::same_world(before.origin,after.origin)||before.root!=after.root||
       !scenery_kiss::read_action_prop(uid,actual,e))return fail(e,e.empty()?"Building terrain settlement World changed":e);
    if(actual.prop.uid!=uid||actual.dungeon_membership||actual.dungeon_instance_membership)
        return fail(e,"Building terrain settlement native UID or Root membership changed");
    const auto current=world_state::building_state(actual);
    auto ground=[&](float& height,std::string& why){
        const auto code=moved_building_ground(r->image,after.root,{current.x,current.y,current.z},height);
        return !code||fail(why,"Building original GetHeightAt failed SEH="+std::to_string(code));
    };
    json proof;
    if(!validate_moved_height(immediate,current,receipt,ground,proof,e)){receipt["building_height"]=proof;return false;}
    receipt["building_height"]=proof;settled=current;return true;
}
bool settle_selection_buildings(const action::Binding& binding,const action::Request& request,
    const std::vector<SelectionBuildingSettlement>& inputs,std::vector<action::BuildingState>& settled,json& receipt,std::string& e){
    settled.clear();receipt={{"confirmed",false}};auto* r=published.load(std::memory_order_acquire);
    const bool history=(request.action==action::Action::UndoBuilding||request.action==action::Action::RedoBuilding)&&
        !request.target_action_uuid.empty()&&request.canonical_record_key=="selection:"+request.target_action_uuid;
    if(!r||!r->ready.load()||GetCurrentThreadId()!=r->thread||inputs.empty()||request.dungeon_building||
       (request.action!=action::Action::MoveSelection&&!history))return fail(e,"Selection Building settlement requires its ordered Move or exact history owner");
    auto owner=[&]{return action::tick_commands::authority::matches(binding,request,e)&&
        remove_diagnostics::current_registry_world(request.snapshot_uuid,binding.native_epoch,binding.native_generation,e);};
    host::WorldState before,after;std::set<std::string> keys;std::set<uint64_t> uids;std::vector<uintptr_t> objects;
    if(!owner()||!world(*r,before,e))return false;
    for(const auto& row:inputs){remove_diagnostics::LocalTarget target;native::BuildingState actual;
        if(row.key.empty()||!row.uid||!keys.insert(row.key).second||!uids.insert(row.uid).second||
           !remove_diagnostics::resolve_record_target(request.snapshot_uuid,row.key,target,e)||!target.normal_building||!target.object||target.uid!=row.uid||
           !scenery_kiss::read_action_prop(row.uid,actual,e)||actual.prop.uid!=row.uid||actual.dungeon_membership||actual.dungeon_instance_membership||
           world_state::building_state(actual)!=row.immediate)return fail(e,e.empty()?"Selection Building settlement immediate owner or pose differs":e);
        if(request.action==action::Action::MoveSelection){const auto found=std::find_if(request.selection_move.rows.begin(),request.selection_move.rows.end(),
                [&](const auto& candidate){return candidate.key==row.key&&candidate.after==row.immediate;});
            if(found==request.selection_move.rows.end())return fail(e,"Selection Building settlement differs from exact requested row");}
        objects.push_back(target.object);
    }
    if(!terrain_settlement::enable(*r,e)||!terrain_settlement::run(*r,request.action_uuid,true,receipt,e))return false;
    receipt["buildings_confirmed"]=false;
    if(!owner()||!world(*r,after,e)||!host::same_world(before.origin,after.origin)||before.root!=after.root)
        return fail(e,e.empty()?"Selection Building settlement World changed":e);
    json proofs=json::array();std::vector<action::BuildingState> values;values.reserve(inputs.size());
    for(size_t i=0;i<inputs.size();++i){const auto& row=inputs[i];native::BuildingState actual;uintptr_t object=0;
        if(!r->api.find)return fail(e,"Selection Building settlement native UID lookup unavailable");
        const auto exception=selection_building_lookup(r->api.find,row.uid,object);
        if(exception||object!=objects[i])return fail(e,"Selection Building settlement native UID/object changed SEH="+std::to_string(exception));
        if(!scenery_kiss::read_action_prop(row.uid,actual,e)||actual.prop.uid!=row.uid||actual.dungeon_membership||actual.dungeon_instance_membership)
            return fail(e,e.empty()?"Selection Building settlement native UID or Root membership changed":e);
        const auto value=world_state::building_state(actual);
        auto ground=[&](float& height,std::string& why){const auto code=moved_building_ground(r->image,after.root,{value.x,value.y,value.z},height);
            return !code||fail(why,"Selection Building original GetHeightAt failed SEH="+std::to_string(code));};
        json proof;if(!validate_moved_height(row.immediate,value,receipt,ground,proof,e)){proofs.push_back({{"key",row.key},{"height",proof}});receipt["buildings"]=std::move(proofs);return false;}
        proofs.push_back({{"key",row.key},{"height",proof}});values.push_back(value);
    }
    receipt["buildings"]=std::move(proofs);receipt["buildings_confirmed"]=true;settled=std::move(values);return true;
}
}

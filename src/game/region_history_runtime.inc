struct RegionHistoryOperation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    int32_t region_id=-1;std::array<int32_t,2> range{};uintptr_t inverse_graph=0;
    bool attempted=false,confirmed=false,retired=false,ownership=false;std::string cause;
};
std::map<std::string,RegionHistoryOperation> region_history_operations;

bool history_current(Runtime& r,const action::Binding& binding,int32_t id,const std::array<int32_t,2>& range,
                     action::RegionPropertyState& out,LocalView& view,uintptr_t& global,std::string& e){
    action::RegionLevelState actors;
    if(!property_read(r,id,0,out,view,e)||!bound(view,binding,net::current_snapshot_id(),e)||
       !level_state(r,view,range,actors,global,e))return false;
    out.actors=std::move(actors);return true;
}

bool history_inverse(Runtime& r,uintptr_t graph,uintptr_t region,const action::RegionPropertyState& before,std::string& e){
    undo::GraphWitness value;uintptr_t data=0,operation=0,ap=0,owner=0;int32_t type=-1;std::array<int32_t,2> range{};
    if(!undo::read_graph(r.memory,r.image,graph,value,e)||!at(r,graph,0x20,data,e)||!at(r,data,0,operation,e)||
       !at(r,operation,0,ap,e)||!at(r,operation,0xb8,owner,e)||!at(r,operation,0xc0,type,e)||!at(r,operation,0xc4,range,e))return false;
    if(value.operations!=1||!value.locked||value.pending_delete||ap!=r.image+0x1723230||owner!=region||
       type!=before.type||range!=std::array<int32_t,2>{before.level,before.max_level}){
        e="Region history original inverse owner/type/range differs";return false;}
    return true;
}

#include "region_ownership_history.inc"

action::Outcome region_history_perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||!properties.ready){e="Region history native runtime unavailable";return action::Outcome::RejectedNoEffect;}
    if(GetCurrentThreadId()!=r->thread){e="Region history Mainthread differs";return action::Outcome::RejectedNoEffect;}
    if(request.snapshot_uuid!=net::current_snapshot_id()){e="Region history snapshot differs";return action::Outcome::RejectedNoEffect;}
    if(bool(authority)==Config::instance().is_host){e="Region history authority/role differs";return action::Outcome::RejectedNoEffect;}
    if(!ui_move_runtime::contract::pending(typed_action_runtime::status(),request,revision,e))return action::Outcome::RejectedNoEffect;
    if(region_history_operations.contains(request.action_uuid)){e="Region history already attempted; no replay";return action::Outcome::Unknown;}
    const auto found=properties.operations.find(request.target_action_uuid);
    if((request.action!=action::Action::UndoBuilding&&request.action!=action::Action::RedoBuilding)||found==properties.operations.end()){
        e="Region history original action/direction missing";return action::Outcome::RejectedNoEffect;}
    const auto& source=found->second;
    const bool ownership=source.request.region_property.kind==action::RegionPropertyKind::Purchase||source.request.region_property.kind==action::RegionPropertyKind::Sale;
    if(!source.confirmed||!source.retired||source.binding!=binding||source.request.snapshot_uuid!=request.snapshot_uuid||
       source.request.action!=action::Action::EditRegionProperty||(!ownership&&source.request.region_property.kind!=action::RegionPropertyKind::LevelRange)||
       source.request.canonical_record_key!=request.canonical_record_key||(!ownership&&!source.level_graph)){
        e="Region history lacks current confirmed source";return action::Outcome::RejectedNoEffect;}
    if(ownership)return ownership_history_perform(*r,binding,request,source,authority,e);
    const bool undo_direction=request.action==action::Action::UndoBuilding;
    const auto& destination=undo_direction?source.evidence.property_before:source.evidence.property_after;
    const std::array<int32_t,2> range{destination.level,destination.max_level};
    action::RegionPropertyState before;LocalView view;uintptr_t global=0,source_graph=0;action::Ledger money;
    if(!history_current(*r,binding,source.request.region_property.id,range,before,view,global,e)||!ledger(money,e)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,source_graph,e))return action::Outcome::RejectedNoEffect;
    if(!before.owned||range[0]<1||range[1]<range[0]||
       std::array<int32_t,2>{before.level,before.max_level}==range){e="Region history requires changed owned level range";return action::Outcome::RejectedNoEffect;}
    if(authority&&(!authority->property_before.actors||!authority->property_after.actors||authority->after!=money||
       !authority->undo_confirmed||authority->selection_confirmed||authority->target_retired||
       authority->before!=money||!property_same_before(before,authority->property_before)||
       authority->property_after.level!=range[0]||authority->property_after.max_level!=range[1])){
        e="Region history Client complete before/desired range differs from Host";return action::Outcome::RejectedNoEffect;}
    RegionHistoryOperation value;value.binding=binding;value.request=request;value.region_id=source.request.region_property.id;value.range=range;
    value.evidence.before=money;value.evidence.property_before=authority?authority->property_before:before;
    auto& op=region_history_operations.emplace(request.action_uuid,std::move(value)).first->second;
    auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    if(authority){
        std::memcpy(reinterpret_cast<void*>(view.random),authority->property_before.content.random.data(),32);
        std::memcpy(reinterpret_cast<void*>(view.region+0x268),authority->property_before.type_random.data(),32);
        std::memcpy(reinterpret_cast<void*>(global),authority->property_before.actors->global_random.data(),32);
    }
    op.attempted=true;
    const auto outcome=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(outcome!=action::Outcome::Confirmed){op.cause=e;return authority||outcome==action::Outcome::Unknown?unknown(e):outcome;}
    const auto inverse=undo_direction?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(inverse,request.target_action_uuid,op.inverse_graph,e)||
       !history_inverse(*r,op.inverse_graph,view.region,before,e))return unknown(e);
    action::RegionPropertyState after;LocalView after_view;uintptr_t after_global=0;
    if(!history_current(*r,binding,op.region_id,range,after,after_view,after_global,e)||after_view.region!=view.region||after_global!=global||
       !ledger(op.evidence.after,e))return unknown(e.empty()?"Region history native current owner changed":e);
    op.evidence.property_after=after;op.evidence.undo_confirmed=true;
    if(!sha256(json::array().dump(),op.evidence.region_retired_sha256,e))return unknown(e);
    // The native inverse stores the old range. It clamps the current roster;
    // it does not restore each actor's historical level.
    auto expected=op.evidence.property_before;expected.level=range[0];expected.max_level=range[1];
    expected.content.random=after.content.random;expected.type_random=after.type_random;expected.actors=after.actors;
    if(after!=expected||before.actors->counts!=after.actors->counts||before.actors->roster_sha256!=after.actors->roster_sha256||
       before.actors->expected_sha256!=after.actors->levels_sha256||after.actors->expected_sha256!=after.actors->levels_sha256||money!=op.evidence.after)
        return unknown("Region history original clamp/roster/Ledger consequence differs");
    if(authority&&(op.evidence.property_after!=authority->property_after||op.evidence.after!=authority->after||
       op.evidence.region_retired_sha256!=authority->region_retired_sha256))return unknown("Region history original result differs from Host");
    op.confirmed=true;return action::Outcome::Confirmed;
}

bool region_history_observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=region_history_operations.find(request.action_uuid);
    if(!r||GetCurrentThreadId()!=r->thread||found==region_history_operations.end()||found->second.binding!=binding||
       found->second.request!=request||!found->second.confirmed){e="Region history observation lacks exact confirmed source";return false;}
    const auto& op=found->second;action::RegionPropertyState current;LocalView view;uintptr_t global=0;action::Ledger money;
    if(op.ownership){
        if(!ownership_read(*r,op.region_id,0,op.range,current,view,global,e)||!bound(view,binding,request.snapshot_uuid,e))return false;
    }else if(!history_current(*r,binding,op.region_id,op.range,current,view,global,e))return false;
    if(!ledger(money,e))return false;
    current.actors->global_random=op.evidence.property_after.actors->global_random;
    if(current!=op.evidence.property_after||money!=op.evidence.after){e="Region history result changed before ACK";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}

bool region_history_retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence actual;if(!region_history_observe(binding,request,actual,e))return false;
    auto& op=region_history_operations.at(request.action_uuid);
    if(op.retired){e="Region history already retired";return false;}op.retired=true;return true;
}

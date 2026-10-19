namespace quest_history {
namespace {
struct Source {
    a::Binding binding;std::string snapshot,key;int32_t region=-1;
    uintptr_t owner=0,manager=0;qr::Owner before,after;
};
struct Operation {
    a::Binding binding;a::Request request;Pin pin;uintptr_t owner=0,inverse=0;
    qr::Witness before,after;quest_list::References refs_before,refs_after;
    d::Sample sample,result;a::Evidence evidence;
    bool attempted=false,confirmed=false,retired=false;std::string cause;
};
std::map<std::string,Source> sources;std::map<std::string,Operation> operations;
bool graph(Memory& m,uintptr_t pointer,const qr::Owner& destination,std::string& e){
    undo_probe::GraphWitness witness;uintptr_t data=0,operation=0;
    return undo_probe::read_graph(m.process,state.image,pointer,witness,e)&&
        ((witness.locked&&!witness.nodes&&!witness.pending_delete&&witness.operations==1)||fail(e,"Quest history requires one drained original DoSetNPCQuests"))&&
        field(m,pointer,0x20,data,e)&&field(m,data,0,operation,e)&&qr::inverse(m,state.image,operation,destination,e);
}
bool destinations(Memory& m,const Pin& p,const qr::Owner& value,std::string& e){
    if(value.state.records.size()!=value.destination_uids.size())return fail(e,"Quest history original destinations incomplete");
    for(size_t i=0;i<value.state.records.size();++i){const auto& row=value.state.records[i];if(row.action==8)continue;
        uintptr_t pointer=0,ap=0;uint64_t uid=0;std::string key;
        const auto seh=lookup(p.api.find,value.destination_uids[i],pointer);
        if(seh||!pointer||!field(m,pointer,0x18,uid,e)||uid!=value.destination_uids[i]||!field(m,pointer,0,ap,e))
            return fail(e,e.empty()?"Quest history stored destination no longer resolves current full UID SEH="+std::to_string(seh):e);
        if(ap==state.image+0x16c0770){if(!quest_authoring::direct_destination(pointer,key,e))return false;}
        else if(ap==state.image+0x1742980){if(!quest_authoring::npc_destination(pointer,key,e))return false;}
        else {bool in_dungeon=false;if(!npc_quest_target_runtime::bind_current_destination(pointer,key,in_dungeon,e))return false;}
        if(key!=row.destination_key)return fail(e,"Quest history stored destination current canonical owner differs");
    }
    return true;
}
qr::Witness seed(const Source& source){qr::Witness out;out.manager=source.manager;qr::Owner owner;
    owner.pointer=source.owner;owner.uid=source.before.uid;owner.key=source.key;out.owners.emplace(source.owner,std::move(owner));return out;}
}
bool remember(const a::Binding& binding,const a::Request& request,uintptr_t owner,uintptr_t pointer,
              const qr::Witness& before,const qr::Witness& after,std::string& e){
    if(sources.contains(request.action_uuid)||!owner||!before.owners.contains(owner)||!after.owners.contains(owner)||
       before.manager!=after.manager||before.owners.at(owner).uid!=after.owners.at(owner).uid||
       before.owners.at(owner).key!=request.canonical_record_key||after.owners.at(owner).key!=request.canonical_record_key)
        return fail(e,"Quest history source identity or original receipt differs");
    Memory m;if(!graph(m,pointer,before.owners.at(owner),e)||
       !building_history_slice::remember_quest(request.action_uuid,pointer,e))return false;
    auto& source=sources.try_emplace(request.action_uuid).first->second;source.binding=binding;source.snapshot=request.snapshot_uuid;
    source.key=request.canonical_record_key;source.region=request.action==a::Action::SetNpcQuestDestination?request.npc_quest.region:request.npc_quest_list.region;
    source.owner=owner;source.manager=before.manager;source.before=before.owners.at(owner);source.after=after.owners.at(owner);return true;
}
void clear(){sources.clear();operations.clear();}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e){
    uint64_t revision=0;
    if(!state.ready||GetCurrentThreadId()!=state.thread||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||request.canonical_record_key!="quests:"+request.target_action_uuid||
       (request.action!=a::Action::UndoBuilding&&request.action!=a::Action::RedoBuilding)||
       !ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,e)){
        if(e.empty())e="Quest history requires current bound pending Mainthread/authority";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Quest history already attempted; no replay";return a::Outcome::Unknown;}
    const auto found=sources.find(request.target_action_uuid);
    if(found==sources.end()||found->second.binding!=binding||found->second.snapshot!=request.snapshot_uuid){e="Quest history confirmed source is absent in this World";return a::Outcome::RejectedNoEffect;}
    const auto& source=found->second;const bool undo=request.action==a::Action::UndoBuilding;const auto& destination=undo?source.before:source.after;
    Memory m;Pin p;action_runtime::ui::LocalBinding local;qr::Witness before;quest_list::References refs;d::Sample sample;a::NpcQuestListState value;uintptr_t pointer=0;
    auto previous=seed(source);
    if(!quest_list::inner_codes(m,e)||!quest_list::bind_owner(m,binding,request.snapshot_uuid,source.key,source.region,local,p,e)||
       local.object!=source.owner||local.uid!=source.before.uid||
       !quest_list::state_read(m,binding,request.snapshot_uuid,p,local,source.key,before,refs,sample,value,e,&previous)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,pointer,e)||!graph(m,pointer,destination,e)||
       !destinations(m,p,destination,e))return a::Outcome::RejectedNoEffect;
    if(authority){auto current=value;current.global_random=authority->quest_list_before.global_random;
        if(current!=authority->quest_list_before||ledger(sample.money)!=authority->before||authority->before!=authority->after||
           !authority->undo_confirmed||authority->target_retired||authority->selection_confirmed){e="Quest history Client complete before differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.owner=local.object;op.pin=p;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause.empty()?"Quest history failed without cause":cause;return a::Outcome::Unknown;};
    if(authority){const auto seh=action_runtime::seed(state.image,authority->quest_list_before.global_random);
        if(seh)return unknown("Quest history Host RNG seed SEH="+std::to_string(seh));
        const auto regional=sample.state.region_random;
        if(!quest_list::state_read(m,binding,request.snapshot_uuid,p,local,source.key,before,refs,sample,value,e,&previous)||
           value!=authority->quest_list_before||sample.state.region_random!=regional)return unknown(e.empty()?"Quest history Host RNG seed readback differs":e);}
    op.before=std::move(before);op.refs_before=std::move(refs);op.sample=sample;op.evidence.before=ledger(sample.money);op.evidence.quest_list_before=value;
    op.attempted=true;const auto result=building_history_slice::perform(request.action,request.target_action_uuid,e);
    if(result!=a::Outcome::Confirmed){op.cause=e;return authority||result==a::Outcome::Unknown?unknown(e):result;}
    const auto opposite=undo?a::Action::RedoBuilding:a::Action::UndoBuilding;Pin after_pin;action_runtime::ui::LocalBinding after_local;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,op.inverse,e)||!graph(m,op.inverse,op.before.owners.at(op.owner),e)||
       !quest_list::bind_owner(m,binding,request.snapshot_uuid,source.key,source.region,after_local,after_pin,e)||
       after_local.object!=op.owner||after_local.uid!=local.uid||!same_world(after_pin.world,p.world)||
       !quest_list::state_read(m,binding,request.snapshot_uuid,after_pin,after_local,source.key,op.after,op.refs_after,op.result,value,e,&previous))return unknown(e);
    auto expected=seed(source);expected.owners[op.owner]=destination;
    auto protected_state=op.result.state;protected_state.global_random=op.sample.state.global_random;
    if(!duplicate_quest_history::restored(expected,op.before,op.after,{op.owner},e)||
       !quest_history_read::references(op.refs_before.rows,op.refs_after.rows,source.key,e)||
       op.refs_before.manager!=op.refs_after.manager||op.refs_before.data!=op.refs_after.data||op.refs_before.local!=op.refs_after.local||
       protected_state!=op.sample.state||op.result.release_edits!=op.sample.release_edits||ledger(op.result.money)!=op.evidence.before)
        return unknown(e.empty()?"Quest history changed protected Region, release or Ledger values":e);
    op.evidence.after=ledger(op.result.money);op.evidence.quest_list_after=value;op.evidence.undo_confirmed=true;
    if(authority&&(op.evidence.quest_list_after!=authority->quest_list_after||op.evidence.after!=authority->after))
        return unknown("Quest history complete original result differs from Host");
    op.confirmed=true;return a::Outcome::Confirmed;
}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=operations.find(request.action_uuid);
    if(GetCurrentThreadId()!=state.thread||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(e,"Quest history observation lacks exact confirmed current receipt");
    const auto& op=found->second;const auto& source=sources.at(request.target_action_uuid);Memory m;Pin p;action_runtime::ui::LocalBinding local;
    qr::Witness witness;quest_list::References refs;d::Sample sample;a::NpcQuestListState value;auto previous=seed(source);uintptr_t graph_pointer=0;
    const auto direction=request.action==a::Action::UndoBuilding?a::Action::RedoBuilding:a::Action::UndoBuilding;
    if(!quest_list::bind_owner(m,binding,request.snapshot_uuid,source.key,source.region,local,p,e)||local.object!=op.owner||
       !same_world(p.world,op.pin.world)||!quest_list::state_read(m,binding,request.snapshot_uuid,p,local,source.key,witness,refs,sample,value,e,&previous)||
       !building_history_slice::graph_for(direction,request.target_action_uuid,graph_pointer,e)||graph_pointer!=op.inverse||
       !graph(m,graph_pointer,op.before.owners.at(op.owner),e))return false;
    value.global_random=op.evidence.quest_list_after.global_random;
    if(value!=op.evidence.quest_list_after||ledger(sample.money)!=op.evidence.after||refs.local!=op.refs_after.local)
        return fail(e,"Quest history complete original result changed before ACK");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& e){a::Evidence out;
    if(!observe(binding,request,out,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired)return fail(e,"Quest history receipt already retired");op.retired=true;return true;
}
}

namespace history {
namespace {
struct Graph {
    uintptr_t pointer=0;std::vector<std::pair<uintptr_t,std::string>> nodes;
    bool operator==(const Graph&)const=default;
};
struct Operation {
    a::Binding binding;a::Request request;Pin pin;d::Sample before,after;
    qr::Witness quests_before,quests_after;quest_list::References refs_before,refs_after;
    a::Evidence evidence;Graph inverse;uintptr_t object=0;uint64_t uid=0;
    bool attempted=false,confirmed=false,retired=false;std::string cause;
};
std::map<std::string,Operation> operations;
std::map<std::string,Graph> retained;
bool history_code(Memory& memory,std::string& error){
    for(const auto& pin:monster_history_codes::values){std::string bytes(pin.size,'\0'),sha;
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,sha,error))return false;
        if(sha!=pin.sha)return fail(error,"Monster original history body differs RVA="+std::to_string(pin.rva));
    }
    return true;
}
bool reset(std::string& error){
    for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Monster history retains an unretired operation="+id);
    operations.clear();retained.clear();return true;
}
bool graph(Memory& memory,uintptr_t pointer,uint64_t uid,bool moving,Graph& out,std::string& error){
    out={};undo_probe::GraphWitness witness;
    if(!undo_probe::read_graph(memory.process,state.image,pointer,witness,error))return false;
    if(!witness.locked||witness.nodes||witness.pending_delete||witness.operations<1||witness.operations>4097)
        return fail(error,"Monster history requires a drained locked original graph");
    uintptr_t data=0;if(!field(memory,pointer,0x20,data,error))return false;
    unsigned target=0;std::set<uintptr_t> unique;out.pointer=pointer;
    for(int32_t i=0;i<witness.operations;++i){uintptr_t node=0,ap=0,method=0;uint64_t actual=0;size_t end=0;
        if(!field(memory,data,size_t(i)*8,node,error))return false;
        if(!node||!unique.insert(node).second)return fail(error,"Monster history original graph node is null or repeated");
        if(!field(memory,node,0,ap,error)||!field(memory,ap,0x10,method,error))return false;
        if(moving){if(ap!=state.image+0x1786ff0||method!=state.image+0x55ed80)return fail(error,"Monster Move history graph contains a foreign original node");end=0xe8;}
        else if(ap==state.image+0x1706550&&method==state.image+0x5507c0)end=0x178;
        else if(ap==state.image+0x16de860&&method==state.image+0x53e1c0)end=0xc0;
        else if(ap==state.image+0x1713330&&method==state.image+0x563f50)end=0xe0;
        else return fail(error,"Monster Remove history graph contains a foreign original node");
        if(!field(memory,node,0xb8,actual,error))return false;
        if(!actual)return fail(error,"Monster history original node full UID is zero");
        if(ap!=state.image+0x1713330){if(actual!=uid)return fail(error,"Monster history graph targets another full UID");++target;}
        std::string bytes(end,'\0'),sha;
        if(!memory.copy(node,bytes.data(),bytes.size(),error))return false;
        if(ap==state.image+0x1706550){std::string name;if(!npc_quest_records::read_string(memory,node,0xc0,"Monster retained Make name",name,error))return false;
            bytes+=json(name).dump();}
        if(ap==state.image+0x1713330){uintptr_t records=0;int32_t count=0,capacity=0;
            if(!field(memory,node,0xd0,records,error)||!field(memory,node,0xd8,count,error)||!field(memory,node,0xdc,capacity,error))return false;
            if(count<0||count>4096||capacity<count||capacity>100000||(count&&!records)||records>UINTPTR_MAX-size_t(count)*0x58)
                return fail(error,"Monster retained Quest inverse record bounds differ");
            for(int32_t index=0;index<count;++index){const auto row=records+size_t(index)*0x58;std::string raw(0x58,'\0'),name,description;
                if(!memory.copy(row,raw.data(),raw.size(),error)||!npc_quest_records::read_string(memory,row,0x18,"Monster retained Quest name",name,error)||
                   !npc_quest_records::read_string(memory,row,0x38,"Monster retained Quest description",description,error))return false;
                bytes+=raw;bytes+=json::array({name,description}).dump();}
        }
        if(!quest_bridge::sha256(bytes,sha,error))return false;
        // This is local lifetime integrity only. No native address crosses peers.
        out.nodes.emplace_back(node,std::move(sha));
    }
    return target==1&&(!moving||out.nodes.size()==1)||fail(error,"Monster history original target is absent or repeated");
}
bool protected_others(const monster_zone_read::State& before,const monster_zone_read::State& after,uint64_t uid,bool restoring,bool removing,std::string& error){
    if(before.manager!=after.manager||before.monster_manager!=after.monster_manager||before.zones!=after.zones)
        return fail(error,"Monster history changed manager/Zone ownership or generated membership");
    auto old=before.monsters,current=after.monsters;
    const auto strip=[&](auto& rows){unsigned count=0;for(auto it=rows.begin();it!=rows.end();){if(it->uid==uid){++count;it=rows.erase(it);}else ++it;}return count;};
    const auto old_count=strip(old),new_count=strip(current);
    if(old_count!=unsigned(!restoring)||new_count!=unsigned(!removing)||old!=current)
        return fail(error,"Monster history changed another native row/order or target membership");
    return true;
}
bool proof(const a::MonsterHistoryOrigin& origin,const qr::Witness& before,const qr::Witness& after,
           const quest_list::References& refs_before,const quest_list::References& refs_after,a::MonsterHistoryProof& out,std::string& error){
    out={};out.origin=origin;
    out.origin.before.tag=0;if(out.origin.after!=a::MonsterActor{})out.origin.after.tag=0;
    out.count_before=uint32_t(before.quests.size());out.count_after=uint32_t(after.quests.size());
    out.completed_before=before.completed;out.completed_after=after.completed;
    return quest_digest(before,out.quests_before,error)&&quest_digest(after,out.quests_after,error)&&
        quest_bridge::sha256(refs_before.rows.dump(),out.references_before,error)&&quest_bridge::sha256(refs_after.rows.dump(),out.references_after,error)&&
        (a::valid_monster_history(out)||fail(error,"Monster history complete actual proof invalid"));
}
bool parity(const a::Evidence& local,const a::Evidence& authority){
    if(!a::monster_shared_state(local.monster_before,authority.monster_before)||!a::monster_shared_state(local.monster_after,authority.monster_after))return false;
    auto actual=local,expected=authority;actual.monster_before={};actual.monster_after={};expected.monster_before={};expected.monster_after={};
    actual.retirement_confirmed=expected.retirement_confirmed=false;return actual==expected;
}
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    uint64_t revision=0;
    if(!state.ready||GetCurrentThreadId()!=state.thread||!a::monster_history(request)||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||!ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="Monster history requires its exact pending current Mainthread/authority";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Monster history already entered; replay forbidden";return a::Outcome::Unknown;}
    const auto moved=moves.find(request.target_action_uuid);const auto removed=removals.find(request.target_action_uuid);
    const bool moving=moved!=moves.end(),undo=request.action==a::Action::UndoBuilding;
    if(moving==(removed!=removals.end())||
       (moving?(!moved->second.confirmed||!moved->second.retired||moved->second.binding!=binding):(!removed->second.confirmed||!removed->second.retired||removed->second.binding!=binding))){
        error="Monster history source is not one confirmed retired current operation";return a::Outcome::RejectedNoEffect;}
    const auto& original=moving?moved->second.request:removed->second.request;
    const auto& evidence=moving?moved->second.evidence:removed->second.evidence;
    const int32_t region=moving?original.monster_move.region:original.monster_remove.region;
    const auto key=original.canonical_record_key;const auto argument=monster_target_key::argument(key);
    const auto uid=moving?moved->second.uid:removed->second.birth.uid;
    const bool restoring=!moving&&undo,removing=!moving&&!undo;
    if(original.snapshot_uuid!=request.snapshot_uuid){error="Monster history source snapshot differs";return a::Outcome::RejectedNoEffect;}
    Memory memory;Pin pin;d::Sample before;monster_zone_read::Monster actor;a::MonsterState value;Graph source;uintptr_t pointer=0;
    if(!move_code(memory,error)||!remove_code(memory,error)||!history_code(memory,error)||
       !(restoring?direct_target_pin(memory,binding,request.snapshot_uuid,argument,region,pin,error,true):move_owner_pin(memory,binding,request.snapshot_uuid,argument,region,pin,error))||
       !effect(memory,pin,before,value,error,restoring?0:uid)||!building_history_slice::graph_for(request.action,request.target_action_uuid,pointer,error)||
       !graph(memory,pointer,uid,moving,source,error))return a::Outcome::RejectedNoEffect;
    const auto known=retained.find(request.target_action_uuid);
    if(known!=retained.end()&&known->second!=source){error="Monster history retained graph receiver/payload changed";return a::Outcome::RejectedNoEffect;}
    if(!restoring){const auto found=std::find_if(before.state.monsters.begin(),before.state.monsters.end(),[&](const auto& row){return row.uid==uid;});
        if(found==before.state.monsters.end()){error="Monster history current target absent";return a::Outcome::RejectedNoEffect;}actor=*found;}
    const auto& expected_actor=undo?evidence.monster_after.actor:evidence.monster_before.actor;
    if(!a::monster_shared_actor(value.actor,expected_actor)||pin.scene!=(moving?moved->second.pin.scene:removed->second.pin.scene)){
        error="Monster history target actor/static pose or source scene changed before original";return a::Outcome::RejectedNoEffect;}
    if(moving&&!move_destination(memory,pin,actor.pointer,(undo?evidence.monster_before.actor:evidence.monster_after.actor).position,error))return a::Outcome::RejectedNoEffect;
    qr::Witness quests;quest_list::References refs;
    const auto* previous=moving?nullptr:&removed->second.quests;
    if(!read_remove_quests(memory,binding,request.snapshot_uuid,restoring?0:actor.pointer,key,quests,error,previous)||
       !quest_list::references(memory,binding,request.snapshot_uuid,quests,refs,error))return a::Outcome::RejectedNoEffect;
    a::MonsterHistoryOrigin origin{uint8_t(original.action),key,pin.scene,region,evidence.monster_before.actor,evidence.monster_after.actor};
    a::MonsterHistoryProof current;if(!proof(origin,quests,quests,refs,refs,current,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=value;portable.roster.global_random=authority->monster_before.roster.global_random;
        if(!authority->monster_history_proof||authority->monster_history_proof->origin!=current.origin||
           authority->monster_history_proof->quests_before!=current.quests_before||authority->monster_history_proof->references_before!=current.references_before||
           !a::monster_shared_state(portable,authority->monster_before)||ledger(before.money)!=authority->before){error="Monster history Client source/before/Quest/reference/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.pin=pin;op.uid=uid;
    const auto unknown=[&](const std::string& cause){error=op.cause=cause.empty()?"Monster history failed without original cause":cause;return a::Outcome::Unknown;};
    if(authority){op.attempted=true;const auto seh=action_runtime::seed(state.image,authority->monster_before.roster.global_random);
        if(seh)return unknown("Monster history Host RNG adoption SEH="+std::to_string(seh));
        if(!effect(memory,pin,before,value,error,restoring?0:uid)||!a::monster_shared_state(value,authority->monster_before))return unknown(error.empty()?"Monster history adopted RNG before proof differs":error);}
    op.before=before;op.quests_before=quests;op.refs_before=refs;op.evidence.before=ledger(before.money);op.evidence.monster_before=value;
    op.attempted=true;const auto result=building_history_slice::perform(request.action,request.target_action_uuid,error);
    if(result!=a::Outcome::Confirmed)return unknown(error);
    if(restoring){uintptr_t restored=0;const auto seh=lookup(pin.api.find,uid,restored);d::Sample fresh;
        if(seh||!restored||!direct_sample(memory,pin,fresh,error))return unknown(error.empty()?"Monster original Make full UID lookup failed SEH="+std::to_string(seh):error);
        const auto found=std::find_if(fresh.state.monsters.begin(),fresh.state.monsters.end(),[&](const auto& row){return row.pointer==restored&&row.uid==uid&&!row.zone;});
        if(found==fresh.state.monsters.end()||!full_uid(memory,pin,*found,error))return unknown(error.empty()?"Monster original Make did not restore its exact scene/full UID":error);
        identity::Entry old=removed->second.birth,actual{scope(pin,fresh),restored,uid,true,false};
        if(!identity::registry.restore_retired(argument,old,actual,error))return unknown(error);removed->second.birth=actual;op.object=restored;
    }else op.object=actor.pointer;
    if(removing){identity::Entry current_entry;if(!identity::registry.lookup(argument,scope(pin,before),current_entry,error)||
        !identity::registry.retire(argument,current_entry,error))return unknown(error);removed->second.birth=current_entry;}
    if(!effect(memory,pin,op.after,op.evidence.monster_after,error,removing?0:uid)||
       !protected_others(before.state,op.after.state,uid,restoring,removing,error)||
       !read_remove_quests(memory,binding,request.snapshot_uuid,removing?0:op.object,key,op.quests_after,error,&op.quests_before)||
       !quest_list::references(memory,binding,request.snapshot_uuid,op.quests_after,op.refs_after,error))return unknown(error);
    std::set<std::string> changed;
    if(!moving){const auto owners=qr::inverse_owners(removed->second.quests);for(auto owner:owners)changed.insert(removed->second.quests.owners.at(owner).key);
        if(restoring){if(!duplicate_quest_history::restored(removed->second.quests,op.quests_before,op.quests_after,owners,error))return unknown(error);}
        else if(!qr::survivors(op.quests_before,op.quests_after,error)||!qr::absent(memory,state.image,op.quests_before,error))return unknown(error);
    }else if(!qr::unchanged(op.quests_before,op.quests_after,error))return unknown(error);
    if(!monster_history_read::references(op.refs_before.rows,op.refs_after.rows,changed,error)||
       op.refs_before.manager!=op.refs_after.manager||op.refs_before.data!=op.refs_after.data||op.refs_before.local!=op.refs_after.local)return unknown(error.empty()?"Monster history changed Subscriber owner lifetime":error);
    a::MonsterHistoryProof final;if(!proof(origin,op.quests_before,op.quests_after,op.refs_before,op.refs_after,final,error))return unknown(error);
    op.evidence.after=ledger(op.after.money);op.evidence.monster_history_proof=final;op.evidence.undo_confirmed=true;op.evidence.target_retired=removing;
    const auto opposite=undo?a::Action::RedoBuilding:a::Action::UndoBuilding;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,pointer,error)||!graph(memory,pointer,uid,moving,op.inverse,error)||
       !a::monster_history_transition(undo,final,op.evidence.monster_before,op.evidence.monster_after)||op.evidence.before!=op.evidence.after)
        return unknown(error.empty()?"Monster history original actor/roster/ledger/inverse effect differs":error);
    if(authority&&!parity(op.evidence,*authority))return unknown("Monster history Client full original result differs from Host");
    retained[request.target_action_uuid]=op.inverse;op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){error=std::string("Monster history original boundary: ")+exception.what();auto found=operations.find(request.action_uuid);if(found!=operations.end())found->second.cause=error;return a::Outcome::Unknown;}
catch(...){error="Monster history non-standard original boundary exception";auto found=operations.find(request.action_uuid);if(found!=operations.end())found->second.cause=error;return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=operations.find(request.action_uuid);
    if(GetCurrentThreadId()!=state.thread||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(error,"Monster history observation lacks exact current confirmed operation");
    const auto& op=found->second;Memory memory;Pin pin;d::Sample sample;a::MonsterState value;qr::Witness quests;quest_list::References refs;uintptr_t pointer=0;Graph inverse;
    const auto& origin=op.evidence.monster_history_proof->origin;const bool absent=op.evidence.target_retired;
    if(!(absent?direct_target_pin(memory,binding,request.snapshot_uuid,monster_target_key::argument(origin.key),origin.region,pin,error,true):move_owner_pin(memory,binding,request.snapshot_uuid,monster_target_key::argument(origin.key),origin.region,pin,error))||
       !same_world(pin.world,op.pin.world)||!effect(memory,pin,sample,value,error,absent?0:op.uid)||value!=op.evidence.monster_after||ledger(sample.money)!=op.evidence.after||
       !read_remove_quests(memory,binding,request.snapshot_uuid,absent?0:op.object,origin.key,quests,error,&op.quests_after)||!qr::unchanged(op.quests_after,quests,error)||
       !quest_list::references(memory,binding,request.snapshot_uuid,quests,refs,error)||refs.rows!=op.refs_after.rows||refs.local!=op.refs_after.local||
       !building_history_slice::graph_for(request.action==a::Action::UndoBuilding?a::Action::RedoBuilding:a::Action::UndoBuilding,request.target_action_uuid,pointer,error)||
       !graph(memory,pointer,op.uid,origin.action==29,inverse,error)||inverse!=op.inverse)return fail(error,error.empty()?"Monster history current original receipt changed before ACK":error);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence actual;if(!observe(binding,request,actual,error))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Monster history already retired");op.retired=true;return true;}
}
namespace {bool monster_history_reset(std::string& error){return history::reset(error);}}

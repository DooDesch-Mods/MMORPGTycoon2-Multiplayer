namespace history {
namespace {
namespace qr=monster_quest_retirement;
using Graph=npc_history_graph::Graph;
struct Source {
    action::Binding binding;std::string snapshot,key;
    int32_t region=-1;uint64_t uid=0;uintptr_t object=0;
    action::NpcHistoryOrigin origin;
    Graph retained;qr::Witness restore;bool registered=false;
};
struct HistoryOperation {
    action::Binding binding;action::Request request;host::WorldState world;
    action::Evidence evidence;qr::Witness quests;json references,spawn;
    Graph inverse;uintptr_t object=0;uint64_t uid=0;
    bool attempted=false,confirmed=false,retired=false;std::string cause;
};
std::map<std::string,Source> sources;
std::map<std::string,HistoryOperation> operations;
bool code(Memory& memory,std::string& error){
    struct Pin {uintptr_t rva;size_t size;const char* sha;};
    const Pin pins[]{
        {0x556ff0,3280,"726ed5c71359de4e3b010d368813c41a57e02dbdd00f65daebd45052e05fe54e"},
        {0x3c2310,256,"0be2b237b8c8b26d6f14b45e1135d0c09902e96198a3bfa9b5b804b8dcbab707"},
        {0x53e1c0,320,"c3adcbffa31dfb98e2e5dfac02ec8ccea4d646320eea9425348d27b8ccf62a4f"},
        {0x563f50,480,"ba39c0cc1f2e40cbb908fd363b3162b1c87660f0517608dffa9ef3c428f709e6"},
        {0x55ed80,704,"555aef77c8b022b696a31e5c65a0c6c8a8f860449323943d4980e48187214aee"},
        {0x53ccf0,2080,"56268bdf8aacde4fa4241d462a614c8a58fd9cd3e1e6ea10244189254e8047fa"}};
    for(const auto& pin:pins){std::string bytes(pin.size,'\0'),sha;
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(pin.rva==0x55ed80){
            if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
                return fail(error,"NPC history original full body differs RVA="+std::to_string(pin.rva)+" original="+error);
            continue;
        }
        if(!quest_bridge::sha256(bytes,sha,error))return false;
        if(sha!=pin.sha)return fail(error,"NPC history original full body differs RVA="+std::to_string(pin.rva));}
    std::array<int32_t,3> costs{};
    if(!memory.copy(state.image+0x1493b28,costs.data(),sizeof costs,error))return false;
    if(costs!=std::array<int32_t,3>{1000,100,500})return fail(error,"NPC history original TypeCost table differs");
    return true;
}
bool scope(Memory& memory,const action::Binding& binding,const std::string& snapshot,int32_t id,
           host::WorldState& world,uintptr_t& region,host::LookupApi& api,std::string& error){
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return field(memory,object,offset,value,cause);};
    return reader.current(world,true,error)&&bound(world,binding,snapshot,error)&&
        region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,id,0,region,at,error);
}
bool graph(Memory& memory,uintptr_t pointer,const Source& source,bool restoring,Graph& out,std::string& error){
    undo::GraphWitness witness;
    if(!undo::read_graph(memory.process,state.image,pointer,witness,error))return false;
    if(!witness.locked||witness.nodes||witness.pending_delete)return fail(error,"NPC history requires a drained locked original graph");
    return npc_history_graph::read(memory,state.image,pointer,witness.operations,source.uid,source.origin.action==17,restoring,
        quest_bridge::sha256,out,error);
}
bool digest(const qr::Witness& witness,std::string& out,std::string& error){
    json rows=json::array();for(const auto& row:witness.rows)rows.push_back({{"owner",row.owner_key},{"record",quest_bridge::records_json({row.record}).at(0)}});
    return quest_bridge::sha256(json{{"completed",witness.completed},{"rows",rows}}.dump(),out,error);
}
bool read_quests(const Source& source,const qr::Witness& previous,uintptr_t current,qr::Witness& out,json& references,std::string& error){
    if((source.origin.action==15?source.origin.after.type:source.origin.before.type)!=0)current=0;
    auto seed=npc_history_quests::seed(previous,source.key,current,source.uid);
    return world_quest_history::read(source.binding,source.snapshot,out,error,&seed)&&
        world_quest_history::references(source.binding,source.snapshot,out,references,error);
}
bool target(Memory& memory,const host::LookupApi& api,uintptr_t region,const Source& source,bool present,
            action::NpcState& value,action::Ledger& money,undo::UndoWitness& witness,npc_census::State& census,
            uintptr_t& object,std::string& error){
    object=0;const auto seh=lookup(api.find,source.uid,object);
    if(seh)return fail(error,"NPC history current full UID lookup SEH="+std::to_string(seh));
    if(bool(object)!=present)return fail(error,"NPC history full UID live/absent direction differs");
    if(!read_effect(memory,state.image,region,present?source.uid:0,value,money,witness,census,error,
        (source.origin.action==15?source.origin.after.type:source.origin.before.type)==0))return false;
    unsigned count=0;for(const auto& row:census.rows)if(row.local_uid==source.uid||row.local_pointer==object){
        if(!present||row.local_uid!=source.uid||row.local_pointer!=object)return fail(error,"NPC history current Manager and full UID lookup differ");++count;}
    return count==unsigned(present)||fail(error,"NPC history target is missing or repeated in complete current census");
}
bool static_actor(const action::NpcActor& current,const action::NpcActor& expected){
    auto left=current,right=expected;left.quests.clear();right.quests.clear();left.quest_count=right.quest_count=0;return left==right;
}
bool unaffected(const npc_census::State& before,const npc_census::State& after,uint64_t uid,bool restoring,bool removing,const std::vector<uint64_t>& quest_changes,std::string& error){
    if(before.manager!=after.manager)return fail(error,"NPC history changed the owning native Manager");
    auto old=before.rows,now=after.rows;
    auto strip=[&](auto& rows){unsigned count=0;for(auto it=rows.begin();it!=rows.end();)if(it->local_uid==uid){++count;it=rows.erase(it);}else ++it;return count;};
    unsigned a=0;
    if(removing){
        for(size_t i=0;i<old.size();++i)if(old[i].local_uid==uid){
            ++a;size_t last=i;for(size_t j=i+1;j<old.size();++j)if(old[j].type==old[i].type)last=j;
            old[i]=old[last];old.erase(old.begin()+static_cast<ptrdiff_t>(last));break;
        }
        if(std::any_of(old.begin(),old.end(),[&](const auto& row){return row.local_uid==uid;}))return fail(error,"NPC history prior target full UID repeats");
    }else a=strip(old);
    const auto b=strip(now);
    if(a!=unsigned(!restoring)||b!=unsigned(!removing)||old.size()!=now.size())return fail(error,"NPC history changed another complete NPC row or survivor order");
    for(size_t i=0;i<old.size();++i){auto prior=old[i];
        if(prior.local_uid==now[i].local_uid&&std::find(quest_changes.begin(),quest_changes.end(),prior.local_uid)!=quest_changes.end())prior.quest_count=now[i].quest_count;
        if(prior!=now[i])return fail(error,"NPC history changed an unrelated NPC field or survivor order");}
    if(before.next_local_tag!=after.next_local_tag&&(!restoring||after.next_local_tag!=before.next_local_tag+1))return fail(error,"NPC history original next tag delta differs");
    return true;
}
bool alias(Source& source,const action::Request& request,uintptr_t current,bool absent,std::string& error){
    if(source.key.starts_with("action:")){
        const auto found=state.operations.find(npc_target_key::argument(source.key));
        if(found==state.operations.end()||found->second.request.action!=action::Action::PlaceNPC||
           found->second.binding!=source.binding||found->second.request.snapshot_uuid!=source.snapshot||found->second.local_uid!=source.uid)
            return fail(error,"NPC history original Place canonical alias is absent or differs");
        found->second.target_absent=absent;if(!absent)found->second.local_pointer=current;
    }else if(!absent&&!npc_saved_identity_runtime::restore_history(source.binding,request,source.key,source.region,source.uid,source.object,current,error))return false;
    if(!absent)source.object=current;return true;
}
std::vector<uint64_t> quest_owners(const Graph& value){std::vector<uint64_t> result;
    for(const auto& node:value.nodes)if(node.ap==state.image+0x1713330)result.push_back(node.uid);return result;}
bool inverse_values(Memory& memory,const Source& source,const Graph& value,const host::WorldState& world,const host::LookupApi& api,uintptr_t region,
                    const action::NpcActor& destination,const qr::Witness& original,std::string& error){
    for(const auto& node:value.nodes){
        if(node.ap==state.image+0x1786ff0){npc_census::Row row;row.rotation=destination.rotation;row.position=destination.position;row.scale=destination.scale;
            if(!move_inverse(memory,state.image,value.pointer,source.uid,row,error))return false;}
        if(node.ap==state.image+0x16de8a0){action::NpcActor stored;uintptr_t stored_region=0;std::array<int32_t,2> character{};
            if(!npc_quest_records::read_string(memory,node.object,0xc0,"NPC history Make name",stored.name,error)||
               !field(memory,node.object,0xe0,stored.type,error)||!field(memory,node.object,0xf0,stored_region,error)||
               !field(memory,node.object,0xf8,stored.rotation,error)||!field(memory,node.object,0x108,stored.position,error)||
               !field(memory,node.object,0x114,stored.scale,error)||!field(memory,node.object,0x164,stored.level,error)||!field(memory,node.object,0x168,character,error))return false;
            stored.character_slot=character[0];stored.character_type=character[1];
            if(stored_region!=region||!static_actor(stored,destination))return fail(error,"NPC history original MakeNPC stored Region/actor/pose differs");
            auto at=[&](uintptr_t object,size_t offset,auto& result,std::string& cause){return field(memory,object,offset,result,cause);};
            uintptr_t manager=0;character_definition_identity::Census definitions;
            if(!field(memory,uintptr_t(world.origin.state),0x168,manager,error)||
               !character_definition_identity::read(at,state.image,uintptr_t(world.origin.state),manager,definitions,error))return false;
            const character_definition_identity::Definition* definition=nullptr;
            for(const auto& row:definitions.definitions)if(character_definition_identity::matches_actor(row,stored)){
                if(definition)return fail(error,"NPC history original Make CharacterTypeID is ambiguous");definition=&row;}
            if(!definition||definition->deleted||!definition->uid)return fail(error,"NPC history original Make CharacterTypeID is absent or deleted");
            uintptr_t registered=0;const auto seh=lookup(api.find,definition->uid,registered);
            if(seh||registered!=definition->object)return fail(error,"NPC history original Make Character full UID differs SEH="+std::to_string(seh));
        }
        if(node.ap==state.image+0x1713330){const qr::Owner* owner=nullptr;
            for(const auto& [pointer,item]:original.owners){(void)pointer;if(item.uid==node.uid){if(owner)return fail(error,"NPC history original Quest owner full UID repeats");owner=&item;}}
            qr::Owner empty;if(!owner&&node.uid==source.uid&&destination.quests.empty()){empty.uid=source.uid;empty.key=source.key;owner=&empty;}
            // A single owner can have several original removal snapshots,
            // including self-targeting incoming Quests before its own true
            // node. Their complete stored bytes/strings are retained when the
            // original action commits and must remain exact before execution.
            if(!owner)return fail(error,"NPC history retained original Quest inverse owner is absent");
        }
    }
    return true;
}
}
bool reset(std::string& error){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"NPC history retains an unretired operation="+id);
    operations.clear();sources.clear();error.clear();return true;}
bool remember(const action::Binding& binding,const action::Request& request,std::string& error){
    if(request.action!=action::Action::PlaceNPC&&request.action!=action::Action::RemoveNPC&&request.action!=action::Action::MoveNPC)return true;
    const auto found=state.operations.find(request.action_uuid);
    if(found==state.operations.end()||!found->second.confirmed||!found->second.retired||found->second.binding!=binding||sources.contains(request.action_uuid))
        return fail(error,"NPC history registration requires its unique confirmed retired current source");
    const auto& op=found->second;Source source;source.binding=binding;source.snapshot=request.snapshot_uuid;
    source.key=request.action==action::Action::PlaceNPC?"action:"+request.action_uuid:request.canonical_record_key;
    source.region=request.action==action::Action::PlaceNPC?request.npc.region:request.action==action::Action::RemoveNPC?op.target_region:op.target_region;
    if(request.action==action::Action::MoveNPC){Memory memory;if(!field(memory,op.local_pointer,0x178,source.object,error)||!field(memory,source.object,0x834,source.region,error))return false;}
    source.object=op.local_pointer;source.uid=op.local_uid;source.origin={uint8_t(request.action),source.key,source.region,op.evidence.npc_before.created,op.evidence.npc_after.created};
    Memory memory;uintptr_t pointer=0;const bool restoring=request.action==action::Action::RemoveNPC;
    if(!code(memory,error))return false;
    // The graph is not registered until all source-specific reads succeed.
    undo::UndoWitness witness;if(!undo::read_undo(memory.process,state.image,witness,error)||witness.active_graph||witness.undo.empty())return false;
    pointer=witness.undo.back();
    if(!graph(memory,pointer,source,restoring,source.retained,error))return false;
    if(restoring){source.restore=op.npc_remove_quests_before;if(!source.restore.manager)return fail(error,"NPC Remove history lacks its complete pre-original Quest witness");}
    if(!building_history_slice::remember_npc(request.action_uuid,pointer,error))return false;
    source.registered=true;sources.emplace(request.action_uuid,std::move(source));error.clear();return true;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error)try{
    uint64_t revision=0;
    if(!state.ready||GetCurrentThreadId()!=state.thread||!action::npc_history(request)||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||!ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="NPC history requires its exact pending current Mainthread/authority";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="NPC history already attempted; no replay";return action::Outcome::Unknown;}
    const auto found=sources.find(request.target_action_uuid);
    if(found==sources.end()||found->second.binding!=binding||found->second.snapshot!=request.snapshot_uuid){error="NPC history source lacks its confirmed current native graph";return action::Outcome::RejectedNoEffect;}
    auto& source=found->second;const bool undoing=request.action==action::Action::UndoBuilding,moving=source.origin.action==17;
    const bool restoring=!moving&&((source.origin.action==16)==undoing),removing=!moving&&!restoring;
    const auto& destination=undoing?source.origin.before:source.origin.after;
    Memory memory;host::WorldState world;host::LookupApi api;uintptr_t region=0,object=0,pointer=0;
    action::NpcState before;action::Ledger money;undo::UndoWitness stack;npc_census::State census;Graph current;
    if(!code(memory,error)||!scope(memory,binding,request.snapshot_uuid,source.region,world,region,api,error)||
       !target(memory,api,region,source,!restoring,before,money,stack,census,object,error)||
       !building_history_slice::graph_for(request.action,request.target_action_uuid,pointer,error)||
       !graph(memory,pointer,source,restoring,current,error))return action::Outcome::RejectedNoEffect;
    if(current!=source.retained){error="NPC history retained original graph receiver or complete payload changed";return action::Outcome::RejectedNoEffect;}
    const auto& expected=undoing?source.origin.after:source.origin.before;
    if(!restoring&&!static_actor(before.created,expected)){error="NPC history current authored actor/pose differs from original source";return action::Outcome::RejectedNoEffect;}
    uintptr_t actor_before=0,marker_before=0;json spawn_before;
    if(moving&&(!move_spawn(memory,state.image,object,region,"before",spawn_before,error)||
       !field(memory,object,0x6f0,actor_before,error)||!field(memory,object,0x810,marker_before,error)))return action::Outcome::RejectedNoEffect;
    qr::Witness quests;json refs;qr::Witness seed;
    if(!read_quests(source,seed,object,quests,refs,error))return action::Outcome::RejectedNoEffect;
    if(removing&&!npc_quest_cascade::select(quests,object,source.uid,source.key,error))return action::Outcome::RejectedNoEffect;
    const auto reconstructed=quest_owners(current);auto changed=reconstructed;
    if(removing)for(size_t i=0;i<quests.quests.size();++i)if(std::find(quests.removed.begin(),quests.removed.end(),quests.quests[i])!=quests.removed.end()){
        const auto owner=quests.owners.find(quests.links[i][3]);if(owner==quests.owners.end()){error="NPC history removed Quest owner absent from complete witness";return action::Outcome::RejectedNoEffect;}
        if(std::find(changed.begin(),changed.end(),owner->second.uid)==changed.end())changed.push_back(owner->second.uid);}
    const auto& original=restoring?source.restore:quests;
    if(!inverse_values(memory,source,current,world,api,region,destination,original,error))return action::Outcome::RejectedNoEffect;
    const std::array<int32_t,3> costs{1000,100,500};const auto type=source.origin.action==15?source.origin.after.type:source.origin.before.type;
    const auto cash=source.origin.action==15?(undoing?costs.at(size_t(type)):-costs.at(size_t(type))):0;
    if(current.cash!=cash){error="NPC history actual original Cash differs from its operation/type/direction";return action::Outcome::RejectedNoEffect;}
    action::Context context;
    if(!action::selected_session().current(context,error)||!npc_history_budget::preflight(context,request,source.origin,before,money,cash,uint32_t(quests.quests.size()),quests.completed,error))return action::Outcome::RejectedNoEffect;
    if(authority){auto comparable=before;comparable.global_random=authority->npc_before.global_random;
        if(comparable!=authority->npc_before||money!=authority->before||!authority->npc_history_proof||authority->npc_history_proof->origin!=source.origin){error="NPC history Client complete current before/source differs from Host";return action::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.world=world;op.uid=source.uid;
    auto unknown=[&](const std::string& why){error=op.cause=why.empty()?"NPC history failed without cause":why;return action::Outcome::Unknown;};
    if(authority){const auto seh=action_runtime::seed(state.image,authority->npc_before.global_random);if(seh)return unknown("NPC history Host RNG seed SEH="+std::to_string(seh));
        action::NpcState seeded;action::Ledger seeded_money;undo::UndoWitness seeded_stack;npc_census::State seeded_census;uintptr_t seeded_object=0;
        if(!target(memory,api,region,source,!restoring,seeded,seeded_money,seeded_stack,seeded_census,seeded_object,error)||seeded!=authority->npc_before||seeded_money!=money||seeded_object!=object)return unknown(error.empty()?"NPC history Host RNG seed readback differs":error);
        before=std::move(seeded);}
    op.evidence.before=money;op.evidence.npc_before=before;op.attempted=true;
    const auto result=building_history_slice::perform(request.action,request.target_action_uuid,error);
    if(result!=action::Outcome::Confirmed)return unknown(error);
    host::WorldState after_world;host::LookupApi after_api;uintptr_t after_region=0,after_object=0;
    action::NpcState after;action::Ledger after_money;undo::UndoWitness after_stack;npc_census::State after_census;
    if(!scope(memory,binding,request.snapshot_uuid,source.region,after_world,after_region,after_api,error)||!same_world(world,after_world)||after_region!=region||
       !npc_census::read(memory,state.image,region,after_census,error)||
       !unaffected(census,after_census,source.uid,restoring,removing,changed,error))return unknown(error);
    const auto lookup_seh=lookup(after_api.find,source.uid,after_object);
    if(lookup_seh||bool(after_object)!=!removing)return unknown("NPC history original full UID restoration/retirement differs SEH="+std::to_string(lookup_seh));
    if(!removing){unsigned exact=0;for(const auto& row:after_census.rows)if(row.local_uid==source.uid&&row.local_pointer==after_object)++exact;
        if(exact!=1)return unknown("NPC history original restored full UID is absent or repeated in current owner census");}
    const auto old_pointer=source.object;
    if(!moving&&!alias(source,request,after_object,removing,error))return unknown(error);
    if(!target(memory,after_api,region,source,!removing,after,after_money,after_stack,after_census,after_object,error))return unknown(error);
    if(!removing){
        if(!move_spawn(memory,state.image,after_object,region,"after",op.spawn,error)||op.spawn.at("position")!=json(after.created.position))return unknown(error.empty()?"NPC history original SpawnPoint differs from its current actor pose":error);
        if(moving){uintptr_t actor_after=0,marker_after=0;
            if(!field(memory,after_object,0x6f0,actor_after,error)||!field(memory,after_object,0x810,marker_after,error)||
               actor_after!=actor_before||marker_after!=marker_before||spawn_before.at("follow_region")!=op.spawn.at("follow_region"))return unknown(error.empty()?"NPC Move history replaced its Actor, marker or SpawnPoint owner":error);
        }
    }
    qr::Witness actual;json actual_refs;
    if(!read_quests(source,quests,after_object,actual,actual_refs,error))return unknown(error);
    if(removing){if(!npc_history_quests::removed(quests,actual,source.key,reconstructed,error))return unknown(error);source.restore=quests;}
    else if(restoring){if(!npc_history_quests::restored(original,quests,actual,source.key,old_pointer,after_object,source.uid,changed,error,destination.type==0))return unknown(error);}
    else if(!qr::unchanged(quests,actual,error))return unknown(error);
    if(!npc_history_quests::references(quests,refs,actual_refs,source.key,removing,reconstructed,error))return unknown(error);
    const auto opposite=undoing?action::Action::RedoBuilding:action::Action::UndoBuilding;
    if(!building_history_slice::graph_for(opposite,request.target_action_uuid,pointer,error)||!graph(memory,pointer,source,removing,op.inverse,error))return unknown(error);
    action::NpcHistoryProof proof;proof.origin=source.origin;proof.cash=cash;
    proof.count_before=uint32_t(quests.quests.size());proof.count_after=uint32_t(actual.quests.size());proof.completed_before=quests.completed;proof.completed_after=actual.completed;
    if(!digest(quests,proof.quests_before,error)||!digest(actual,proof.quests_after,error)||!quest_bridge::sha256(refs.dump(),proof.references_before,error)||!quest_bridge::sha256(actual_refs.dump(),proof.references_after,error))return unknown(error);
    op.evidence.npc_history_proof=std::move(proof);op.evidence.npc_after=after;op.evidence.after=after_money;
    op.evidence.undo_confirmed=true;op.evidence.target_retired=removing;op.object=after_object;op.quests=std::move(actual);op.references=std::move(actual_refs);
    if(!action::valid_npc_history_effect(request,op.evidence))return unknown("NPC history complete original effect differs from its operation");
    if(authority){auto expected_evidence=std::make_unique<action::Evidence>(*authority);expected_evidence->retirement_confirmed=false;
        if(op.evidence!=*expected_evidence)return unknown("NPC history complete original effect differs from Host");}
    source.retained=op.inverse;op.confirmed=true;error.clear();return action::Outcome::Confirmed;
}catch(const std::exception& exception){error=std::string("NPC original history boundary: ")+exception.what();return action::Outcome::Unknown;}
catch(...){error="NPC original history non-standard boundary exception";return action::Outcome::Unknown;}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    const auto found=operations.find(request.action_uuid);
    if(GetCurrentThreadId()!=state.thread||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(error,"NPC history observation lacks exact current confirmed receipt");
    const auto& op=found->second;const auto& source=sources.at(request.target_action_uuid);Memory memory;host::WorldState world;host::LookupApi api;
    uintptr_t region=0,object=0,pointer=0;action::NpcState value;action::Ledger money;undo::UndoWitness stack;npc_census::State census;Graph inverse;qr::Witness quests;json refs;
    if(!scope(memory,binding,request.snapshot_uuid,source.region,world,region,api,error)||!same_world(op.world,world)||
       !target(memory,api,region,source,!op.evidence.target_retired,value,money,stack,census,object,error)||object!=op.object||value!=op.evidence.npc_after||money!=op.evidence.after||
       !read_quests(source,op.quests,object,quests,refs,error)||!qr::unchanged(op.quests,quests,error)||refs!=op.references||
       !building_history_slice::graph_for(request.action==action::Action::UndoBuilding?action::Action::RedoBuilding:action::Action::UndoBuilding,request.target_action_uuid,pointer,error)||
       !graph(memory,pointer,source,op.evidence.target_retired,inverse,error)||inverse!=op.inverse)return fail(error,error.empty()?"NPC history exact original result changed before ACK":error);
    if(object){json spawn;if(!move_spawn(memory,state.image,object,region,"after",spawn,error)||spawn!=op.spawn)return fail(error,error.empty()?"NPC history SpawnPoint/Actor presentation changed before ACK":error);}
    out=op.evidence;out.retirement_confirmed=op.retired;error.clear();return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& error){action::Evidence actual;if(!observe(binding,request,actual,error))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"NPC history receipt already retired");op.retired=true;return true;}
}

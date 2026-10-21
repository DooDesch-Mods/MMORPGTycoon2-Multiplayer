namespace {
namespace mr=monster_move_read;
struct MoveOperation {
    a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;
    uintptr_t pointer=0;uint64_t uid=0;move_kiss::Receipt receipt;
    mr::Actor actor_before,actor_after;bool confirmed=false,retired=false,seed_attempted=false;
    std::string cause;
};
std::map<std::string,MoveOperation> moves;
bool move_fault=false;
json move_preflight=json::object();
using MonsterLocalMemory=Memory;
struct MonsterMoveMemory final:move_kiss::Memory {
    MonsterLocalMemory& source;explicit MonsterMoveMemory(MonsterLocalMemory& value):source(value){}
    bool read(uintptr_t address,void* output,size_t size,std::string& e)override{return source.copy(address,output,size,e);}
};
a::BuildingState move_pose(const monster_zone_read::Monster& row){
    a::BuildingState out;out.alive=true;out.x=row.position[0];out.y=row.position[1];out.z=row.position[2];
    out.rotation=row.rotation;out.scale=row.scale;return out;
}
bool move_code(Memory& m,std::string& e){
    for(const auto& c:monster_move_codes::values){std::string bytes(c.size,'\0'),sha;
        if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e)||!quest_bridge::sha256(bytes,sha,e))return false;
        if(sha!=c.sha)return fail(e,"Monster Move native body differs RVA="+std::to_string(c.rva));}
    return true;
}
bool target_region(Memory& m,const a::Binding& binding,const std::string& snapshot,
                   const std::string& target,int32_t& saved,std::string& e){
    saved=-1;if(!monster_move_key::input(target))return fail(e,"Monster canonical target syntax invalid");
    if(interventions::zone_monster_key(target)){Pin owner;if(!generated_pin(m,binding,snapshot,target,-1,owner,e))return false;saved=owner.region.saved_region;return true;}
    host::LookupApi api;if(!host::resolve_lookup(m.process,state.image,exe_sha,api,e))return false;
    host::NativeOrigin origin;host::WorldState world;host::Reader reader(m.process,origin,state.image,state.thread,api);
    if(!reader.current(world,true,e)||!action_runtime::bound(world,binding,snapshot,e))return false;
    std::vector<std::pair<std::string,identity::Entry>> entries;
    if(!identity::registry.enumerate(world.origin.uuid,world.origin.world_epoch,world.origin.generation,world.root,entries,e))return false;
    for(const auto& [key,entry]:entries)if(key==target&&!entry.retired){
        if(!field(m,entry.scope.region,0x834,saved,e))return false;
        return saved>=0||fail(e,"Monster canonical owner saved Region invalid");
    }
    return fail(e,"Monster canonical target absent from current full-UID registry");
}
bool move_target(Memory& m,const a::Binding& binding,const std::string& snapshot,int32_t region,
                 const std::string& birth,Pin& p,d::Sample& sample,monster_zone_read::Monster& row,std::string& e){
    if(!(interventions::zone_monster_key(birth)?generated_pin(m,binding,snapshot,birth,region,p,e):direct_target_pin(m,binding,snapshot,birth,region,p,e))||!direct_sample(m,p,sample,e))return false;
    identity::Entry entry;
    if(interventions::zone_monster_key(birth)){interventions::Actor member;if(!generated_member(binding,snapshot,birth,member,e))return false;entry={scope(p,sample),member.object,member.uid,true,false};}
    else if(!identity::registry.lookup(birth,scope(p,sample),entry,e))return false;
    const auto found=std::find_if(sample.state.monsters.begin(),sample.state.monsters.end(),[&](const auto& v){return v.uid==entry.uid&&v.pointer==entry.pointer;});
    if(found==sample.state.monsters.end()||bool(found->zone)!=interventions::zone_monster_key(birth))return fail(e,"Monster Move confirmed canonical member is absent from its reciprocal current-manager roster");
    row=*found;return full_uid(m,p,row,e);
}
bool move_actor(Memory& m,uintptr_t object,mr::Actor& out,std::string& e){
    const auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return field(m,base,offset,value,cause);};
    return mr::actor(state.image,object,at,out,e);
}
uint32_t move_set_on_map(uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(void*,bool)>(state.image+0x39a920)(reinterpret_cast<void*>(object),true);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool move_inverse(Memory& m,uintptr_t graph,uint64_t uid,const a::MonsterActor& before,std::string& e){
    undo_probe::GraphWitness witness;
    if(!undo_probe::read_graph(m.process,state.image,graph,witness,e))return false;
    if(!witness.locked||witness.operations!=1||witness.nodes||witness.pending_delete)
        return fail(e,"Monster Move inverse graph differs operations="+std::to_string(witness.operations)+" pending_delete="+std::to_string(witness.pending_delete));
    uintptr_t data=0,operation=0,ap=0;uint64_t actual=0;
    std::array<float,4> rotation{};std::array<float,3> position{},scale{};
    if(!field(m,graph,0x20,data,e)||!field(m,data,0,operation,e)||!field(m,operation,0,ap,e)||
       !field(m,operation,0xb8,actual,e)||!field(m,operation,0xc0,rotation,e)||
       !field(m,operation,0xd0,position,e)||!field(m,operation,0xdc,scale,e))return false;
    return (ap==state.image+0x1786ff0&&actual==uid&&rotation==before.rotation&&position==before.position&&scale==before.scale)||
        fail(e,"Monster Move inverse full UID or prior pose differs");
}
bool move_selected(Memory& m,const Pin& p,uintptr_t object,std::string& e){
    undo_probe::SelectionWitness witness;undo_probe::SelectionCounts counts;
    if(!undo_probe::read_selection(m.process,state.image,p.world.selection,witness,counts,e))return false;
    const auto& rows=witness.lists[1];return std::find(rows.begin(),rows.end(),object)!=rows.end()||fail(e,"Monster Move selection lacks current target");
}
bool move_reset(std::string& e){
    if(move_fault)return fail(e,"Monster Move retains unresolved visibility mutation");
    for(const auto& [id,op]:moves)if(!op.retired)return fail(e,"Monster Move previous action unretired="+id);
    moves.clear();move_preflight=json::object();return true;
}
json move_status(){
    const auto value=[](const a::MonsterState& v){const auto& r=v.roster;const auto& p=v.actor;
        return json{{"roster",{{"zones",r.zones},{"monsters",r.monsters},{"undo_count",r.undo_count},{"next_tag",r.next_tag},
            {"created_monsters",r.created_monsters},{"global_random",r.global_random},{"region_random",r.region_random},
            {"release_edits",r.release_edits},{"content_sha256",r.content_sha256}}},
            {"actor",{{"tag",p.tag},{"status",p.status},{"rank",p.rank},{"character_id",{p.character_slot,p.character_type}},
                {"level",p.level},{"name",p.name},{"position",p.position},{"rotation",p.rotation},{"scale",p.scale}}}};};
    const auto money=[](const a::Ledger& v){return json{{"cash",v.cash},{"flat_day",v.flat_day},{"record_present",v.record_present},
        {"record_day",v.record_day},{"record_cash",v.record_cash},{"record_income",v.record_income},{"record_expense",v.record_expense},
        {"line_counts",v.line_counts},{"line_income",v.line_income}};};
    json out=json::object();for(const auto& [id,op]:moves)out[id]={{"target_action_uuid",op.request.target_action_uuid},
        {"attempted",op.seed_attempted||op.receipt.started},{"seed_attempted",op.seed_attempted},
        {"move_attempted",op.receipt.move_attempted},{"move_returned",op.receipt.move_returned},{"confirmed",op.confirmed},
        {"retired",op.retired},{"cause",op.cause},{"local_uid",op.uid},{"spawn_before",op.actor_before.spawn},
        {"spawn_after",op.actor_after.spawn},{"on_map",op.actor_after.on_map},{"actor_visible",op.actor_after.visible},
        {"on_map_before",op.actor_before.on_map},{"skeleton_visible_before",op.actor_before.visible},{"skeleton_visible",op.actor_after.visible},
        {"evidence",{{"before",money(op.evidence.before)},{"after",money(op.evidence.after)},
            {"monster_before",value(op.evidence.monster_before)},{"monster_after",value(op.evidence.monster_after)},
            {"undo_confirmed",op.evidence.undo_confirmed},{"selection_confirmed",op.evidence.selection_confirmed},{"retirement_confirmed",op.retired}}}};
    return {{"fault",move_fault},{"preflight",move_preflight},{"operations",out}};
}
}

bool move_capture_binding(const std::string& snapshot,uint64_t epoch,uint64_t generation,
                          uintptr_t object,uint64_t uid,a::BuildingState& before,std::string& key,std::string& e){
    before={};key.clear();if(!state.ready||GetCurrentThreadId()!=state.thread||move_fault)return fail(e,"Monster Move requires ready Mainthread without unresolved effect");
    a::Binding binding;if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    if(binding.native_epoch!=epoch||binding.native_generation!=generation)return fail(e,"Monster Move capture epoch/generation differs");
    Memory m;uintptr_t ap=0,region=0;uint64_t actual=0;int32_t saved=-1;
    if(!field(m,object,0,ap,e)||ap!=state.image+0x16c0770||!field(m,object,0x18,actual,e)||actual!=uid||
       !field(m,object,0x178,region,e)||!field(m,region,0x834,saved,e))return fail(e,e.empty()?"Monster Move capture class/full UID/Region differs":e);
    Pin p;d::Sample sample;if(!move_receiver(m,binding,snapshot,object,uid,key,e))return false;
    monster_zone_read::Monster row;
    if(!move_target(m,binding,snapshot,saved,monster_target_key::argument(key),p,sample,row,e))return false;
    if(p.region.region!=region)return fail(e,"Monster Move capture Region owner differs");
    if(row.pointer!=object||row.uid!=uid)return fail(e,"Monster Move capture birth pointer/full UID differs");
    before=move_pose(row);return true;
}
bool submit_restored_move(const move_kiss::Intent& intent,const std::string& id,std::string& e)try{
    if(intent.family!=move_kiss::Family::DirectMonster||!monster_move_key::valid(intent.object_key))return fail(e,"Monster Move requires explicit direct family and birth key");
    a::BuildingState before;std::string key;
    if(!move_capture_binding(intent.origin.snapshot_uuid,intent.origin.world_epoch,intent.origin.generation,intent.target,intent.local_uid,before,key,e)||
       before!=intent.before||key!=intent.object_key)return fail(e,e.empty()?"Monster Move restored target/old pose differs":e);
    Memory m;if(!move_code(m,e))return false;mr::Actor hidden,visible;unsigned calls=0;
    a::Binding binding;uintptr_t region=0;int32_t saved=-1;Pin p;d::Sample prior,current;
    if(!typed_action_runtime::network_binding(intent.origin.snapshot_uuid,binding,e)||
       !field(m,intent.target,0x178,region,e)||!field(m,region,0x834,saved,e)||
       !move_owner_pin(m,binding,intent.origin.snapshot_uuid,monster_target_key::argument(key),saved,p,e)||!direct_sample(m,p,prior,e))return false;
    if(prior.undo.active_graph)return fail(e,"Monster Move visibility restore has an active Undo graph");
    const auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return field(m,base,offset,value,cause);};
    const auto original=[&](uintptr_t object,std::string& cause){const auto seh=move_set_on_map(object);return !seh||fail(cause,"Monster Move SetOnMap SEH="+std::to_string(seh));};
    if(!mr::restore(state.image,intent.target,at,original,hidden,visible,calls,e)){if(calls)move_fault=true;return false;}
    if(!move_capture_binding(intent.origin.snapshot_uuid,intent.origin.world_epoch,intent.origin.generation,intent.target,intent.local_uid,before,key,e)||
       before!=intent.before||key!=intent.object_key){move_fault=true;return fail(e,e.empty()?"Monster Move target changed after visibility restore":e);}
    if(!direct_sample(m,p,current,e)||d::sample_json(prior)!=d::sample_json(current)||prior.state!=current.state){
        move_fault=true;return fail(e,e.empty()?"Monster Move visibility restore changed roster/RNG/Undo/ledger":e);}
    a::MonsterMoveIntent desired{saved,{intent.after.x,intent.after.y,intent.after.z},intent.after.scale,intent.after.rotation};
    if(!typed_action_runtime::submit_move_monster(id,monster_target_key::argument(key),desired,e))return false;
    move_ui_submitted(id);return true;
}catch(const std::exception& x){move_fault=true;return fail(e,std::string("Monster Move visibility boundary: ")+x.what());}
 catch(...){move_fault=true;return fail(e,"Monster Move visibility non-standard exception");}

a::Outcome move_perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e)try{
    if(request.action!=a::Action::MoveMonster||!a::valid_monster_move(request.monster_move)||
       !monster_move_key::request(request.canonical_record_key,request.target_action_uuid)||!state.ready||GetCurrentThreadId()!=state.thread||move_fault){
        e="Monster Move runtime/Mainthread/intent unavailable";return a::Outcome::RejectedNoEffect;}
    if(moves.contains(request.action_uuid)){e="Monster Move already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="Monster Move lacks exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory m;MonsterMoveMemory reader(m);move_kiss::Calls calls;Pin p;d::Sample before;monster_zone_read::Monster row;
    if(!move_code(m,e)||!move_kiss::resolve_calls(reader,state.image,exe_sha,calls,e)||
       !move_target(m,binding,request.snapshot_uuid,request.monster_move.region,monster_target_key::argument(request.canonical_record_key),p,before,row,e))
        return a::Outcome::RejectedNoEffect;
    if(before.undo.active_graph){e="Monster Move requires no active Undo graph";return a::Outcome::RejectedNoEffect;}
    if(!move_destination(m,p,row.pointer,request.monster_move.position,e))return a::Outcome::RejectedNoEffect;
    a::MonsterState value;mr::Actor actor;
    move_preflight={{"action_uuid",request.action_uuid},{"target_action_uuid",request.target_action_uuid},
        {"local_uid",row.uid},{"local_pointer",row.pointer},{"phase","effect"},{"actor_read",false}};
    if(!effect(m,p,before,value,e,row.uid)){move_preflight["cause"]=e;return a::Outcome::RejectedNoEffect;}
    move_preflight["phase"]="actor";
    if(!move_actor(m,row.pointer,actor,e)){move_preflight["cause"]=e;return a::Outcome::RejectedNoEffect;}
    move_preflight["actor_read"]=true;move_preflight["character_handle"]=actor.character;move_preflight["actor_pointer"]=actor.actor;
    move_preflight["skeleton_pointer"]=actor.skeleton;move_preflight["on_map"]=actor.on_map;
    move_preflight["skeleton_visible"]=actor.visible;move_preflight["in_view"]=actor.in_view;
    move_preflight["distance_visible"]=actor.distance_visible;move_preflight["spawn"]=actor.spawn;
    if(!actor.on_map){e="Monster Move target is not on-map: on_map="+std::to_string(actor.on_map)+
        " skeleton_visible="+std::to_string(actor.visible)+" local_uid="+std::to_string(row.uid);
        move_preflight["cause"]=e;return a::Outcome::RejectedNoEffect;}
    move_preflight["phase"]="registered_actor";move_preflight["cause"]="";
    if(authority&&(!a::zone_shared_content(value.roster,authority->monster_before.roster)||!a::monster_shared_actor(value.actor,authority->monster_before.actor)||ledger(before.money)!=authority->before)){
        e="Monster Move Client before content/actor/Undo/ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    a::BuildingState pose=move_pose(row);pose.x=request.monster_move.position[0];pose.y=request.monster_move.position[1];pose.z=request.monster_move.position[2];
    pose.rotation=request.monster_move.rotation;pose.scale=request.monster_move.scale;
    std::array<unsigned char,0x6c> transform{};if(!move_kiss::character_transform(pose,transform,e))return a::Outcome::RejectedNoEffect;
    auto& op=moves.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.pin=p;op.pointer=row.pointer;op.uid=row.uid;op.actor_before=actor;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause.empty()?"Monster Move failed without cause":cause;return a::Outcome::Unknown;};
    if(authority){op.seed_attempted=true;const auto seh=action_runtime::seed(state.image,authority->monster_before.roster.global_random);
        if(seh)return unknown("Monster Move Host RNG adoption SEH="+std::to_string(seh));
        const auto local=before.state.region_random;
        if(!effect(m,p,before,value,e,row.uid)||before.state.region_random!=local||!a::monster_shared_state(value,authority->monster_before))
            return unknown(e.empty()?"Monster Move Host RNG readback differs":e);}
    op.evidence.before=ledger(before.money);op.evidence.monster_before=value;
    if(!move_kiss::execute_once(calls,row.pointer,transform,op.receipt,e))return unknown(e);
    Pin after;d::Sample result;monster_zone_read::Monster moved;
    if(!move_target(m,binding,request.snapshot_uuid,request.monster_move.region,monster_target_key::argument(request.canonical_record_key),after,result,moved,e)||
       !same_world(p.world,after.world)||after.region.region!=p.region.region||after.world.saves||after.world.edits||after.world.active_graph||
       !mr::unchanged_others(before.state,result.state,row.uid,e))return unknown(e.empty()?"Monster Move current world/roster differs":e);
    a::MonsterState final;if(!effect(m,after,result,final,e,row.uid)||!move_actor(m,row.pointer,op.actor_after,e)||
       !mr::same_actor(actor,op.actor_after)||!op.actor_after.on_map||op.actor_after.spawn!=request.monster_move.position)
        return unknown(e.empty()?"Monster Move actor or direct spawn point differs":e);
    if(result.undo.undo.empty()||result.undo.active_graph||result.undo.undo.back()!=op.receipt.graph)return unknown("Monster Move committed Undo graph absent/different");
    undo_probe::GraphWitness graph;
    if(!undo_probe::read_graph(m.process,state.image,op.receipt.graph,graph,e)||!undo_probe::validate_undo_commit(before.undo,result.undo,graph,e)||
       !move_inverse(m,op.receipt.graph,row.uid,value.actor,e)||!move_selected(m,after,row.pointer,e))return unknown(e);
    op.evidence.after=ledger(result.money);op.evidence.monster_after=final;op.evidence.undo_confirmed=true;op.evidence.selection_confirmed=true;
    if(!a::monster_move_transition(request.monster_move,value,final)||op.evidence.before!=op.evidence.after||
       (authority&&(!a::monster_shared_state(final,authority->monster_after)||op.evidence.after!=authority->after)))
        return unknown("Monster Move pose/identity/counters/Release/RNG/Undo/ledger differs from Host");
    if(!building_history_slice::remember_monster(request.action_uuid,op.receipt.graph,e))return unknown(e);
    op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}catch(const std::exception& x){e=std::string("Monster Move perform boundary: ")+x.what();const auto found=moves.find(request.action_uuid);if(found!=moves.end())found->second.cause=e;return a::Outcome::Unknown;}
 catch(...){e="Monster Move perform non-standard exception";return a::Outcome::Unknown;}

bool move_observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=moves.find(request.action_uuid);
    if(found==moves.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.receipt.move_returned)
        return fail(e,"Monster Move observation lacks exact confirmed operation");
    const auto& op=found->second;Memory m;Pin p;d::Sample sample;monster_zone_read::Monster row;a::MonsterState value;mr::Actor actor;
    if(!move_target(m,binding,request.snapshot_uuid,request.monster_move.region,monster_target_key::argument(request.canonical_record_key),p,sample,row,e)||
       !same_world(p.world,op.pin.world)||row.pointer!=op.pointer||row.uid!=op.uid||!effect(m,p,sample,value,e,row.uid)||
       value!=op.evidence.monster_after||ledger(sample.money)!=op.evidence.after||!move_actor(m,row.pointer,actor,e)||
       !mr::same_registered_actor(actor,op.actor_after)||!move_selected(m,p,row.pointer,e))return fail(e,e.empty()?"Monster Move current state differs before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool move_retire(const a::Binding& binding,const a::Request& request,std::string& e){
    a::Evidence actual;if(!move_observe(binding,request,actual,e))return false;auto& op=moves.at(request.action_uuid);
    if(op.retired)return fail(e,"Monster Move receipt already retired");op.retired=true;return true;
}

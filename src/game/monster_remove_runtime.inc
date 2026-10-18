namespace {
namespace ur=a::ui_capture;
namespace rr=monster_remove_read;
#include "monster_quest_runtime.inc"
struct RemoveOperation {
    a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;
    identity::Entry birth;monster_zone_read::Monster row;d::Sample before;
    qr::Witness quests;uintptr_t graph=0;uint64_t character_id=0;
    std::map<uint64_t,std::string> names_before;
    bool attempted=false,returned=false,confirmed=false,retired=false,seed_attempted=false;
    uint64_t original_calls=0;std::string cause;
};
std::map<std::string,RemoveOperation> removals;
const ur::Replay* remove_replay=nullptr;
std::string remove_ui_token;
bool remove_reset(std::string& e){
    if(remove_replay)return fail(e,"Monster Remove private replay remains active");
    for(const auto& [id,op]:removals)if(!op.retired)return fail(e,"Monster Remove action remains unretired="+id);
    removals.clear();return true;
}
bool remove_code(Memory& m,std::string& e){
    for(const auto& c:monster_remove_codes::values){std::string bytes(c.size,'\0'),sha;
        if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e)||!quest_bridge::sha256(bytes,sha,e))return false;
        if(sha!=c.sha)return fail(e,"Monster Remove native body differs RVA="+std::to_string(c.rva));}
    const std::array<std::array<uintptr_t,3>,4> quest_methods{{
        {0x174ac10,8,0x4d80f0},{0x1713330,0x18,0xbeef60},{0x1713330,0x20,0xbeef80},{0x1713330,0x28,0xbeef70}}};
    for(const auto& method:quest_methods){uintptr_t actual=0;
        if(!field(m,state.image+method[0],method[1],actual,e)||actual!=state.image+method[2])
            return fail(e,e.empty()?"Monster Quest destructor or inverse virtual method differs":e);}
    uintptr_t command=0,destroy=0,cost=0,undo=0;
    return field(m,state.image+0x16c0770,0xb0,command,e)&&command==state.image+0x3bc9a0&&
        field(m,state.image+0x16c0770,0x160,destroy,e)&&destroy==state.image+0x3bc530&&
        field(m,state.image+0x16c0770,0x278,cost,e)&&cost==state.image+0xe3f510&&
        field(m,state.image+0x16c0770,0xe0,undo,e)&&undo==state.image+0xcf3050||fail(e,e.empty()?"Monster Remove original virtual methods differ":e);
}
uint32_t remove_original(uintptr_t manager,uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(state.image+0x2d5520)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t spatial_has(uintptr_t spatial,uintptr_t token,bool& present)noexcept{
    __try{present=reinterpret_cast<bool(*)(const void*,const void*)>(state.image+0x874df0)(reinterpret_cast<const void*>(spatial),reinterpret_cast<const void*>(token));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool remove_absent(Memory& m,const Pin& p,const d::Sample& after,const RemoveOperation& op,std::string& e){
    if(!rr::removed_one(op.before.state,after.state,op.birth.uid,e))return false;
    const auto name_read=[&](uintptr_t object,std::string& name,std::string& cause){return npc_census::read_name(m,object,name,cause);};
    if(!rr::unchanged_names(op.names_before,after.state,op.birth.uid,name_read,e))return false;
    uintptr_t resolved=0;const auto seh=lookup(p.api.find,op.birth.uid,resolved);
    if(seh||resolved)return fail(e,"Monster Remove full UID remains registered or lookup failed SEH="+std::to_string(seh));
    uintptr_t data=0,spatial=0;int32_t count=0,capacity=0;
    if(!field(m,after.state.monster_manager,0x70,data,e)||!field(m,after.state.monster_manager,0x78,count,e)||
       !field(m,after.state.monster_manager,0x7c,capacity,e)||!field(m,after.state.monster_manager,0x88,spatial,e))return false;
    if(count<0||count>4096||capacity<count||capacity>1000000||(count&&!data)||!spatial||data>UINTPTR_MAX-size_t(count)*8)
        return fail(e,"Monster Remove active array bounds or spatial owner differs");
    std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t value=0;if(!field(m,data,size_t(i)*8,value,e))return false;
        if(!value||!seen.insert(value).second||value==op.birth.pointer)return fail(e,"Monster Remove target remains active or active membership duplicates");}
    // _Has compares pointer keys only; its pinned body never dereferences token.
    const std::pair<uintptr_t,const char*> registries[]{{spatial,"MonsterManager spatial hash"},{p.region.region+0x7c8,"Region CharacterRegistry"}};
    for(const auto& [registry,label]:registries){bool present=true;
        const auto code=spatial_has(registry,op.birth.pointer,present);
        if(code||present)return fail(e,"Monster Remove "+std::string(label)+" absence differs present="+std::to_string(present)+" SEH="+std::to_string(code));}
    if(!action_runtime::selection_absent(m,state.image,p.world.selection,op.birth.pointer,e))return false;
    qr::Witness quests;
    if(!read_remove_quests(m,op.binding,op.request.snapshot_uuid,op.birth.pointer,op.request.canonical_record_key,quests,e,&op.quests)||
       !qr::survivors(op.quests,quests,e)||!qr::absent(m,state.image,op.quests,e))return false;
    std::string digest;if(!quest_digest(quests,digest,e))return false;
    return op.evidence.monster_quests.after_sha256.empty()||
        (digest==op.evidence.monster_quests.after_sha256&&quests.quests.size()==op.evidence.monster_quests.after_count&&
         quests.completed==op.evidence.monster_quests.completed_after)||fail(e,"Monster Quest post-commit receipt changed");
}
bool remove_inverse(Memory& m,const Pin& p,const RemoveOperation& op,const d::Sample& after,std::string& e){
    if(after.undo.undo.empty())return fail(e,"Monster Remove committed Undo graph absent");
    const auto graph=after.undo.undo.back();undo_probe::GraphWitness witness;
    if(!undo_probe::read_graph(m.process,state.image,graph,witness,e)||
       !undo_probe::validate_undo_commit(op.before.undo,after.undo,witness,e))return false;
    const auto owners=qr::inverse_owners(op.quests);
    if(size_t(witness.operations)!=owners.size()+1||witness.nodes||witness.pending_delete)return fail(e,"Monster Remove inverse graph count or PendingDelete differs");
    uintptr_t data=0,inverse=0,ap=0,root=0,region=0;uint64_t uid=0,zone=1,id=0;
    int32_t rank=0,level=0;std::array<float,4> rotation{};std::array<float,3> position{},scale{};std::string name;
    if(!field(m,graph,0x20,data,e))return false;
    for(size_t i=0;i<owners.size();++i){uintptr_t quest_inverse=0;
        if(!field(m,data,i*8,quest_inverse,e)||!qr::inverse(m,state.image,quest_inverse,op.quests.owners.at(owners[i]),e))return false;}
    if(!field(m,data,owners.size()*8,inverse,e)||!field(m,inverse,0,ap,e)||
       !field(m,inverse,0xb8,uid,e)||!npc_quest_records::read_string(m,inverse,0xc0,"Monster inverse name",name,e)||
       !field(m,inverse,0xe0,root,e)||!field(m,inverse,0xe8,region,e)||!field(m,inverse,0xf0,rank,e)||
       !field(m,inverse,0xf8,zone,e)||!field(m,inverse,0x100,rotation,e)||!field(m,inverse,0x110,position,e)||
       !field(m,inverse,0x11c,scale,e)||!field(m,inverse,0x16c,level,e)||!field(m,inverse,0x170,id,e))return false;
    const auto& actor=op.evidence.monster_before.actor;
    return (ap==state.image+0x1706550&&uid==op.birth.uid&&root==p.world.root&&region==p.region.region&&
        rank==actor.rank&&!zone&&rotation==actor.rotation&&position==actor.position&&scale==actor.scale&&level==actor.level&&
        name==actor.name&&id==op.character_id)||fail(e,"Monster Remove original DoMakeMonster identity/name/owner/pose/level inverse differs");
}
json remove_status(){
    const auto value=[](const a::MonsterState& v){const auto& r=v.roster;const auto& p=v.actor;
        return json{{"roster",{{"zones",r.zones},{"monsters",r.monsters},{"undo_count",r.undo_count},{"next_tag",r.next_tag},
            {"created_monsters",r.created_monsters},{"global_random",r.global_random},{"region_random",r.region_random},
            {"release_edits",r.release_edits},{"content_sha256",r.content_sha256}}},
            {"actor",{{"tag",p.tag},{"status",p.status},{"rank",p.rank},{"character_id",{p.character_slot,p.character_type}},
                {"level",p.level},{"name",p.name},{"position",p.position},{"rotation",p.rotation},{"scale",p.scale}}}};};
    const auto money=[](const a::Ledger& v){return json{{"cash",v.cash},{"flat_day",v.flat_day},{"record_present",v.record_present},
        {"record_day",v.record_day},{"record_cash",v.record_cash},{"record_income",v.record_income},{"record_expense",v.record_expense},
        {"line_counts",v.line_counts},{"line_income",v.line_income}};};
    json out=json::object();for(const auto& [id,op]:removals)out[id]={{"target_action_uuid",op.request.target_action_uuid},
        {"attempted",op.attempted},{"returned",op.returned},{"seed_attempted",op.seed_attempted},{"confirmed",op.confirmed},
        {"retired",op.retired},{"cause",op.cause},{"local_uid",op.birth.uid},{"original_calls",op.original_calls},
        {"evidence",{{"before",money(op.evidence.before)},{"after",money(op.evidence.after)},
        {"monster_before",value(op.evidence.monster_before)},{"monster_after",value(op.evidence.monster_after)},
        {"undo_confirmed",op.evidence.undo_confirmed},{"target_retired",op.evidence.target_retired},
        {"selection_confirmed",op.evidence.selection_confirmed},{"retirement_confirmed",op.retired},
        {"quest_retirement",quest_retirement_json(op.evidence.monster_quests)}}}};
    return out;
}
}

static bool remove_bound_local(const a::Binding& binding,const std::string& snapshot,
                               uintptr_t manager,uintptr_t object,ur::LocalBinding& out,std::string& e){
    out={};
    if(!state.ready||GetCurrentThreadId()!=state.thread||!ui_remove_runtime::enabled())
        return fail(e,"Monster Remove capture requires ready Mainthread and enabled original hook");
    Memory m;uintptr_t ap=0,region=0;uint64_t uid=0;int32_t saved=-1;
    if(!remove_code(m,e)||!field(m,object,0,ap,e)||ap!=state.image+0x16c0770||!field(m,object,0x18,uid,e)||
       !field(m,object,0x178,region,e)||!field(m,region,0x834,saved,e))return fail(e,e.empty()?"Monster Remove capture class/full UID/Region differs":e);
    Pin p;d::Sample sample;std::string key;
    if(!pin(m,binding,snapshot,saved,p,e))return false;
    if(p.world.selection!=manager||p.region.region!=region)
        return fail(e,"Monster Remove current SelectionManager or reciprocal Region differs");
    if(!d::sample(m,p.region,sample,e)||!identity::registry.resolve(scope(p,sample),object,uid,key,e))return false;
    monster_zone_read::Monster row;
    if(!move_target(m,binding,snapshot,saved,key.substr(7),p,sample,row,e)||row.pointer!=object||row.uid!=uid||row.rank!=1)
        return fail(e,e.empty()?"Monster Remove requires confirmed current direct Elite birth":e);
    qr::Witness quests;if(!read_remove_quests(m,binding,snapshot,object,key,quests,e))return false;
    out={manager,object,uid,binding.native_epoch,binding.native_generation,snapshot,key,false,false,false,true};return true;
}
bool remove_local_binding(uintptr_t manager,uintptr_t object,ur::LocalBinding& out,std::string& e){
    out={};
    if(remove_replay){
        if(manager!=remove_replay->binding.manager||object!=remove_replay->binding.object)
            return fail(e,"Monster Remove private resolver target differs");
        // The synchronous DestroyProps continuation rechecks the reserved
        // request and full current binding through the private replay guard.
        if(!authorize_remove_replay(*remove_replay,e))return false;
        out=remove_replay->binding;return true;
    }
    a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!typed_action_runtime::network_binding(snapshot,binding,e))return false;
    return remove_bound_local(binding,snapshot,manager,object,out,e);
}
bool submit_remove(const ur::Intent& intent,std::string& e){
    if(intent.snapshot_uuid!=net::current_snapshot_id()||!npc_target_key::valid(intent.canonical_record_key)||
       !intent.canonical_record_key.starts_with("action:"))return fail(e,"Monster Remove requires current birth action key");
    // The key lookup supplies a candidate only. resolve performs the complete
    // current class, full UID and owner census before Controller submission.
    const auto birth=intent.canonical_record_key.substr(7);auto place=state.operations.find(birth);
    if(place==state.operations.end()||!place->second.confirmed||!place->second.retired)return fail(e,"Monster Remove confirmed placement receipt absent");
    ur::LocalBinding actual;
    if(!remove_local_binding(place->second.pin.world.selection,place->second.pointer,actual,e))return false;
    if(actual.canonical_record_key!=intent.canonical_record_key)return fail(e,"Monster Remove submitted key differs from current birth binding");
    if(!typed_action_runtime::submit_remove_monster(intent.action_uuid,birth,{place->second.request.monster.region},e))return false;
    if(!remove_ui_token.empty()){
        auto job=state.ui_jobs.find(remove_ui_token);
        if(job==state.ui_jobs.end()||job->second.value("canonical_record_key",std::string{})!=intent.canonical_record_key)
            return fail(e,"Monster Remove UI token differs after Controller submit");
        job->second["captured"]=true;job->second["submitted"]=true;job->second["action_uuid"]=intent.action_uuid;
    }
    return true;
}
bool authorize_remove_replay(const ur::Replay& replay,std::string& e){
    const auto found=removals.find(replay.intent.action_uuid);const auto controller=typed_action_runtime::controller_status();
    if(!remove_replay||found==removals.end()||remove_replay->binding!=replay.binding||
       remove_replay->intent.action_uuid!=replay.intent.action_uuid||remove_replay->intent.snapshot_uuid!=replay.intent.snapshot_uuid||
       remove_replay->intent.canonical_record_key!=replay.intent.canonical_record_key||remove_replay->purpose!=replay.purpose||
       !replay.binding.supported_monster||!state.ready||GetCurrentThreadId()!=state.thread||
       found->second.request.action!=a::Action::RemoveMonster||found->second.birth.uid!=replay.binding.uid||
       found->second.birth.pointer!=replay.binding.object||found->second.binding.native_epoch!=replay.binding.native_epoch||
       found->second.binding.native_generation!=replay.binding.native_generation||
       found->second.request.snapshot_uuid!=replay.intent.snapshot_uuid||found->second.request.canonical_record_key!=replay.intent.canonical_record_key||
       !controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=replay.intent.action_uuid||replay.intent.snapshot_uuid!=net::current_snapshot_id()||
       (replay.purpose==ur::ReplayPurpose::HostExecution)!=Config::instance().is_host)
        return fail(e,"Monster Remove private replay binding, Controller or role differs");
    ur::LocalBinding actual;return remove_bound_local(found->second.binding,found->second.request.snapshot_uuid,replay.binding.manager,replay.binding.object,actual,e)&&
        (actual==replay.binding||fail(e,"Monster Remove private replay target changed"));
}

a::Outcome remove_perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e)try{
    if(request.action!=a::Action::RemoveMonster||request.monster_remove.region<0||request.canonical_record_key!="action:"+request.target_action_uuid||
       !state.ready||GetCurrentThreadId()!=state.thread||!ui_remove_runtime::enabled()){
        e="Monster Remove runtime, Mainthread, capture or intent unavailable";return a::Outcome::RejectedNoEffect;}
    if(removals.contains(request.action_uuid)){e="Monster Remove already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="Monster Remove lacks exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory m;Pin p;d::Sample before;monster_zone_read::Monster row;a::MonsterState value;identity::Entry birth;qr::Witness quests;
    if(!remove_code(m,e)||!move_target(m,binding,request.snapshot_uuid,request.monster_remove.region,request.target_action_uuid,p,before,row,e)||
       !identity::registry.lookup(request.target_action_uuid,scope(p,before),birth,e)||
       !read_remove_quests(m,binding,request.snapshot_uuid,row.pointer,request.canonical_record_key,quests,e)||!effect(m,p,before,value,e,row.uid))return a::Outcome::RejectedNoEffect;
    if(before.undo.active_graph||row.rank!=1){e="Monster Remove requires idle Undo and confirmed Elite rank";return a::Outcome::RejectedNoEffect;}
    uintptr_t character=0;uint64_t character_id=0;
    if(!field(m,row.pointer,0x498,character,e)||!field(m,character,0x2c,character_id,e))return a::Outcome::RejectedNoEffect;
    std::map<uint64_t,std::string> names;
    const auto name_read=[&](uintptr_t object,std::string& name,std::string& cause){return npc_census::read_name(m,object,name,cause);};
    if(!rr::read_names(before.state,name_read,names,e))return a::Outcome::RejectedNoEffect;
    a::MonsterQuestRetirement quest_receipt;
    if(!quest_before(quests,quest_receipt,e))return a::Outcome::RejectedNoEffect;
    if(authority&&!quest_before_matches(quest_receipt,authority->monster_quests)){
        e="Monster Quest Client before records, order or counts differ from Host";return a::Outcome::RejectedNoEffect;}
    // Keep the existing transport budget before the original can retire anything.
    if(!quest_receipt_fits(quest_receipt)){
        e="Monster Quest retirement receipt exceeds existing action payload budget";return a::Outcome::RejectedNoEffect;}
    if(authority&&(!a::zone_shared_content(value.roster,authority->monster_before.roster)||value.actor!=authority->monster_before.actor||ledger(before.money)!=authority->before)){
        e="Monster Remove Client before roster, actor, Undo or ledger differs from Host";return a::Outcome::RejectedNoEffect;}
    auto& op=removals.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.pin=p;op.birth=birth;op.row=row;op.quests=quests;op.character_id=character_id;op.evidence.monster_quests=quest_receipt;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause.empty()?"Monster Remove failed without cause":cause;return a::Outcome::Unknown;};
    if(authority){op.seed_attempted=true;const auto seh=action_runtime::seed(state.image,authority->monster_before.roster.global_random);
        if(seh)return unknown("Monster Remove Host RNG adoption SEH="+std::to_string(seh));
        const auto local=before.state.region_random;
        if(!effect(m,p,before,value,e,row.uid)||before.state.region_random!=local||!a::monster_shared_state(value,authority->monster_before))
            return unknown(e.empty()?"Monster Remove Host RNG readback differs":e);}
    op.before=before;op.names_before=std::move(names);op.evidence.before=ledger(before.money);op.evidence.monster_before=value;
    ur::LocalBinding local;if(!remove_bound_local(binding,request.snapshot_uuid,p.world.selection,row.pointer,local,e))return unknown(e);
    qr::Witness ready;
    if(!read_remove_quests(m,binding,request.snapshot_uuid,row.pointer,request.canonical_record_key,ready,e)||!qr::unchanged(op.quests,ready,e))return unknown(e);
    ur::Replay replay{authority?ur::ReplayPurpose::ConfirmedHostApplication:ur::ReplayPurpose::HostExecution,
        {request.action_uuid,request.snapshot_uuid,request.canonical_record_key},local};
    if(remove_replay)return unknown("Monster Remove private replay is reentrant");
    const auto calls=ui_remove_runtime::status().value("replay_originals",uint64_t{});bool performed=false;op.attempted=true;
    {remove_replay=&replay;struct Reset{~Reset(){remove_replay=nullptr;}} reset;
        performed=ui_remove_runtime::with_replay(replay,[&](std::string& cause){const auto seh=remove_original(local.manager,local.object);
            return !seh||fail(cause,"Monster original DestroyProp SEH="+std::to_string(seh));},e);}
    const auto after_calls=ui_remove_runtime::status().value("replay_originals",uint64_t{});
    op.original_calls=after_calls>=calls?after_calls-calls:0;
    if(!performed||op.original_calls!=1)return unknown(e.empty()?"Monster Remove expected exactly one returned DestroyProps original":e);
    op.returned=true;Pin after;d::Sample result;a::MonsterState final;
    if(!pin(m,binding,request.snapshot_uuid,request.monster_remove.region,after,e)||!same_world(p.world,after.world)||
       after.world.saves||after.world.edits||after.world.active_graph||after.region.region!=p.region.region||
       !effect(m,after,result,final,e)||!remove_absent(m,after,result,op,e)||!remove_inverse(m,after,op,result,e))return unknown(e.empty()?"Monster Remove current world or absence proof differs":e);
    qr::Witness quest_after;
    if(!read_remove_quests(m,binding,request.snapshot_uuid,op.birth.pointer,request.canonical_record_key,quest_after,e,&op.quests)||
       !qr::survivors(op.quests,quest_after,e)||!quest_digest(quest_after,op.evidence.monster_quests.after_sha256,e))return unknown(e);
    op.evidence.monster_quests.after_count=uint32_t(quest_after.quests.size());
    op.evidence.monster_quests.completed_after=quest_after.completed;
    if(authority&&op.evidence.monster_quests!=authority->monster_quests)return unknown("Monster Quest retirement result differs from Host");
    op.graph=result.undo.undo.back();op.evidence.after=ledger(result.money);op.evidence.monster_after=final;
    op.evidence.undo_confirmed=true;op.evidence.target_retired=true;op.evidence.selection_confirmed=true;
    if(!a::monster_remove_transition(value,final)||op.evidence.before!=op.evidence.after||
       (authority&&(!a::monster_shared_state(final,authority->monster_after)||op.evidence.after!=authority->after)))
        return unknown("Monster Remove actor/roster/counters/RNG/Release/Undo/ledger differs from Host");
    if(!identity::registry.retire(request.target_action_uuid,birth,e))return unknown(e);
    op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}catch(const std::exception& x){e=std::string("Monster Remove perform boundary: ")+x.what();auto found=removals.find(request.action_uuid);if(found!=removals.end())found->second.cause=e;return a::Outcome::Unknown;}
 catch(...){e="Monster Remove perform non-standard exception";return a::Outcome::Unknown;}

bool remove_observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=removals.find(request.action_uuid);
    if(found==removals.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||
       !found->second.returned||found->second.original_calls!=1||!identity::registry.tombstone(request.target_action_uuid,found->second.birth))
        return fail(e,"Monster Remove observation lacks exact confirmed OriginalOnce and terminal birth tombstone");
    const auto& op=found->second;Memory m;Pin p;d::Sample sample;a::MonsterState value;
    if(!pin(m,binding,request.snapshot_uuid,request.monster_remove.region,p,e)||!same_world(p.world,op.pin.world)||
       !effect(m,p,sample,value,e)||value!=op.evidence.monster_after||ledger(sample.money)!=op.evidence.after||
       sample.undo.undo.empty()||sample.undo.undo.back()!=op.graph||!remove_absent(m,p,sample,op,e)||!remove_inverse(m,p,op,sample,e))
        return fail(e,e.empty()?"Monster Remove current absence/Undo/effect differs before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool remove_retire(const a::Binding& binding,const a::Request& request,std::string& e){
    a::Evidence actual;if(!remove_observe(binding,request,actual,e))return false;auto& op=removals.at(request.action_uuid);
    if(op.retired)return fail(e,"Monster Remove execution receipt already retired");op.retired=true;return true;
}

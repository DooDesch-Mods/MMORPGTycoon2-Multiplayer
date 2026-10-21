namespace quest_authoring {
namespace {
using SetDisplay=void(*)(void*,void*);
struct Pending {a::Binding binding;std::string snapshot,owner,token;a::NpcQuestIntent intent;};
struct Operation {
    a::Binding binding;a::Request request;Pin pin;qr::Witness before,after;d::Sample sample;
    uintptr_t owner=0,quest=0,destination=0,graph=0;uint64_t destination_uid=0;
    a::Evidence evidence;bool attempted=false,returned=false,seed_attempted=false,confirmed=false,retired=false;
    std::string cause;
};
SetDisplay original=nullptr;bool installed=false,ready=false,fault=false;
std::optional<Pending> pending;std::map<std::string,Operation> operations;
std::map<std::string,json> jobs;std::string ui_token,cause,last_action;uint64_t captures=0,submitted=0,rejected=0;
void entry(void*,void*);
bool reject(const std::string& why){++rejected;if(cause!=why){cause=why;LOG_ERROR("[NPCQuestEdit] {}",why);}return false;}
bool codes(Memory& m,bool hooked,std::string& e){
    for(const auto& c:npc_quest_authoring_codes::values){
        if(c.rva==0x4db155)continue;
        if(c.rva==0x78ed30){if(!quest_list::verify_ui(c.rva,true,e))return false;continue;}
        std::string bytes(c.size,'\0'),sha;if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e))return false;
        if(c.rva==0x4db150&&hooked){
            constexpr std::array<unsigned char,5> prefix{0x41,0x54,0x55,0x57,0x56};
            constexpr std::array<unsigned char,6> jump{0xff,0x25,0,0,0,0};
            std::array<unsigned char,5> actual{};std::array<unsigned char,6> stub{};uintptr_t target=0;
            if(uint8_t(bytes[0])!=0xe9||!original||!m.copy(uintptr_t(original),actual.data(),5,e)||actual!=prefix||
               !m.copy(uintptr_t(original)+5,stub.data(),6,e)||stub!=jump||!m.copy(uintptr_t(original)+11,&target,8,e)||target!=state.image+0x4db155)
                return fail(e,e.empty()?"Quest display owned trampoline differs":e);
            int32_t delta=0;std::memcpy(&delta,bytes.data()+1,4);const auto relay=uintptr_t(int64_t(state.image+0x4db155)+delta);
            if(!m.copy(relay,stub.data(),6,e)||stub!=jump||!m.copy(relay+6,&target,8,e)||target!=uintptr_t(&entry)||
               !quest_bridge::sha256(bytes.substr(5),sha,e)||sha!="784d1365f7c13f4ba84d6ed6deccad3e40f54905f4b944f05bb8bcb9940eb711")
                return fail(e,e.empty()?"Quest display owned relay or immutable tail differs":e);
        }else if(!quest_bridge::sha256(bytes,sha,e)||sha!=c.sha)return fail(e,e.empty()?"Quest original body differs RVA="+std::to_string(c.rva):e);
    }
    return true;
}
uint32_t default_action(uintptr_t destination,int32_t& action)noexcept{
    __try{action=reinterpret_cast<int32_t(*)(void*)>(state.image+0x4d5ea0)(reinterpret_cast<void*>(destination));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t set_destination(uintptr_t quest,uintptr_t destination,int32_t action,int32_t kills)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*,int32_t,int32_t)>(state.image+0x4d8720)(reinterpret_cast<void*>(quest),reinterpret_cast<void*>(destination),action,kills);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool destination(Memory& m,const a::Binding& binding,const std::string& snapshot,int32_t region,
                 a::QuestDestinationFamily family,const std::string& key,uintptr_t& object,uint64_t& uid,std::string& e){
    object=0;uid=0;Pin p;if(!pin(m,binding,snapshot,region,p,e))return false;
    if(family==a::QuestDestinationFamily::NPC){
        if(!npc_target_key::valid(key))return fail(e,"Quest NPC destination key invalid");
        action_runtime::ui::LocalBinding local;uintptr_t native_region=0;npc_census::State census;
        if(!action_runtime::target_binding(npc_target_key::argument(key),binding,snapshot,local,native_region,census,e))return false;
        if(native_region!=p.region.region||local.canonical_record_key!=key)return fail(e,"Quest NPC destination current Region/key differs");
        std::string actual;if(!npc_destination(local.object,actual,e))return false;
        if(actual!=key)return fail(e,"Quest NPC destination exact canonical binding differs");
        object=local.object;uid=local.uid;return true;
    }
    if(family==a::QuestDestinationFamily::DirectMonster){
        if(!monster_target_key::valid(key))return fail(e,"Quest direct Monster requires confirmed birth key");
        d::Sample sample;monster_zone_read::Monster row;
        if(!move_target(m,binding,snapshot,region,monster_target_key::argument(key),p,sample,row,e)||row.rank!=1)return fail(e,e.empty()?"Quest target requires original Elite Monster rank":e);
        object=row.pointer;uid=row.uid;return true;
    }
    npc_quest_target_runtime::CurrentTargets keys;
    if(!npc_quest_target_runtime::read_current(region,keys,e))return false;
    d::ReadMemory bounded{m};uintptr_t buildings=0,zones=0;if(!q::owner(bounded,p.region,buildings,zones,e))return false;
    const auto array=family==a::QuestDestinationFamily::Building?buildings+0x408:zones+0x48;
    const auto& pool=family==a::QuestDestinationFamily::Building?keys.building_keys:keys.monster_zone_keys;
    q::Bounds span;if(!q::bounds(bounded,array,100000,span,e)||size_t(span.count)!=pool.size())return fail(e,e.empty()?"Quest destination exact pool span differs":e);
    for(size_t i=0;i<pool.size();++i)if(pool[i]==key){
        if(object)return fail(e,"Quest destination key duplicated");
        if(!field(m,span.data,i*8,object,e))return false;
    }
    if(!object)return fail(e,"Quest destination key absent from original family pool");
    std::string actual;uintptr_t found=0;
    if(!npc_quest_target_runtime::bind_destination(region,object,actual,e)||actual!=key||!field(m,object,0x18,uid,e)||!uid)
        return fail(e,e.empty()?"Quest destination full local binding differs":e);
    const auto seh=lookup(p.api.find,uid,found);
    return (!seh&&found==object&&q::unchanged(bounded,array,100000,span,e))||fail(e,e.empty()?"Quest destination full UID lookup differs":e);
}
bool target_intent(Memory& m,const a::Binding& binding,const std::string& snapshot,uintptr_t object,
                   a::NpcQuestIntent& intent,std::string& e){
    uintptr_t region=0,ap=0,method=0;int32_t type=0;
    if(!field(m,object,0,ap,e)||!field(m,object,0x178,region,e)||!field(m,region,0x834,intent.destination_region,e))return false;
    if(ap==state.image+0x16c0770){intent.family=a::QuestDestinationFamily::DirectMonster;
        if(!direct_destination(object,intent.destination_key,e))return false;intent.kills=1;
    }else if(ap==state.image+0x1742980){intent.family=a::QuestDestinationFamily::NPC;intent.kills=0;
        if(!npc_destination(object,intent.destination_key,e))return false;
    }else if(ap==state.image+0x1745ce0){intent.family=a::QuestDestinationFamily::Zone;
        if(!field(m,object,0x24c,type,e)||type!=1)return fail(e,e.empty()?"Quest destination Zone is not a Monster pool":e);
        if(!npc_quest_target_runtime::bind_destination(intent.destination_region,object,intent.destination_key,e))return false;
        intent.kills=intent.before.monsters_to_kill?intent.before.monsters_to_kill:8;
    }else {intent.family=a::QuestDestinationFamily::Building;intent.kills=0;
        if(!npc_quest_target_runtime::bind_destination(intent.destination_region,object,intent.destination_key,e))return false;}
    uintptr_t actual=0;uint64_t uid=0;
    if(!destination(m,binding,snapshot,intent.destination_region,intent.family,intent.destination_key,actual,uid,e)||actual!=object)
        return fail(e,e.empty()?"Quest selected destination does not match full current family binding":e);
    const auto seh=default_action(object,intent.action);if(seh)return fail(e,"Quest original default action SEH="+std::to_string(seh));
    if(!field(m,ap,0x2d0,method,e))return false;
    if(intent.family==a::QuestDestinationFamily::Zone){
        if(method!=state.image+0xe3f5c0||!field(m,object,0x560,intent.monster_type,e))return fail(e,e.empty()?"Quest Zone GetMonsterType differs":e);
    }else {if(method!=state.image+0xdf1670)return fail(e,"Quest destination GetMonsterType differs");intent.monster_type=0;}
    return true;
}
bool owner(Memory& m,const a::Binding& binding,const std::string& snapshot,const std::string& key,int32_t region,
           action_runtime::ui::LocalBinding& local,Pin& p,std::string& e){
    uintptr_t native_region=0;npc_census::State census;
    if(!action_runtime::target_binding(npc_target_key::argument(key),binding,snapshot,local,native_region,census,e)||
       !pin(m,binding,snapshot,region,p,e)||p.region.region!=native_region)return fail(e,e.empty()?"Quest NPC current Region differs":e);
    const auto row=std::find_if(census.rows.begin(),census.rows.end(),[&](const auto& r){return r.local_pointer==local.object&&r.local_uid==local.uid;});
    return (row!=census.rows.end()&&row->type==0&&row->quest_count>0)||fail(e,"Quest edit requires existing QuestGiver slots");
}
bool state_read(Memory& m,const a::Binding& binding,const std::string& snapshot,const Pin& p,uintptr_t npc,int32_t slot,
                qr::Witness& witness,d::Sample& sample,a::NpcQuestState& value,uintptr_t& quest,std::string& e){
    if(!read_remove_quests(m,binding,snapshot,0,{},witness,e)||!d::sample(m,p.region,sample,e))return false;
    const auto found=witness.owners.find(npc);
    if(found==witness.owners.end()||slot<0||size_t(slot)>=found->second.state.records.size())return fail(e,"Quest selected owner slot absent");
    value={};value.record=found->second.state.records[size_t(slot)];quest=found->second.state.local[size_t(slot)].quest;
    value.count=uint32_t(witness.quests.size());value.completed=witness.completed;value.undo_count=sample.undo.undo.size();value.global_random=sample.state.global_random;
    return quest_digest(witness,value.all_sha256,e);
}
bool inverse(Memory& m,const Operation& op,const d::Sample& after,std::string& e){
    if(after.undo.undo.empty())return fail(e,"Quest edit original Undo absent");
    const auto graph=after.undo.undo.back();undo_probe::GraphWitness witness;uintptr_t data=0,inverse=0;
    return undo_probe::read_graph(m.process,state.image,graph,witness,e)&&
        undo_probe::validate_undo_commit(op.sample.undo,after.undo,witness,e)&&
        ((witness.operations==1&&!witness.nodes&&!witness.pending_delete)||fail(e,"Quest edit inverse graph differs"))&&
        field(m,graph,0x20,data,e)&&field(m,data,0,inverse,e)&&qr::inverse(m,state.image,inverse,op.before.owners.at(op.owner),e);
}
bool ui_binding(Memory& m,uintptr_t display,uintptr_t quest,const action_runtime::ui::LocalBinding& local,
                uintptr_t& selector,std::string& e){
    a::ui_diagnostic::Window window;json receipt;
    if(!action_runtime::live_npc_window(m,state.image,local,window,receipt,e))return false;
    uintptr_t list=0,ap=0,npc=0,proxy=0,weak=0,display_actual=0,selected_quest=0,data=0;
    int32_t count=0,capacity=0;uint8_t selected=0,selecting=0;uintptr_t cursor=0,behaviour=0,listener=0;
    if(!field(m,state.image,0x13df400,display_actual,e)||display_actual!=display||!field(m,display,0,ap,e)||ap!=state.image+0x1701f80||
       !field(m,display,0x1b0,selected_quest,e)||selected_quest!=quest||!field(m,display,0x1b8,proxy,e)||!field(m,proxy,0,weak,e)||weak!=quest+0x18||
       !field(m,window.window,0x530,list,e)||!field(m,list,0,ap,e)||ap!=state.image+0x16e02b0||
       !field(m,list,0x360,npc,e)||npc!=local.object||!field(m,list,0x368,proxy,e)||!field(m,proxy,0,weak,e)||weak!=local.object+0x20||
       !field(m,list,0x350,selector,e)||!field(m,selector,0,ap,e)||ap!=state.image+0x17088a0||
       !field(m,selector,0x320,npc,e)||npc!=list||!field(m,selector,0x3e0,selected_quest,e)||selected_quest!=quest||
       !field(m,selector,0x3e8,proxy,e)||!field(m,proxy,0,weak,e)||weak!=quest+0x18||
       !field(m,selector,0x3f8,selected,e)||selected!=1||!field(m,selector,0x3fa,selecting,e)||selecting!=1||
       !field(m,list,0x340,data,e)||!field(m,list,0x348,count,e)||!field(m,list,0x34c,capacity,e)||count<1||count>4096||capacity<count||capacity>100000||!data||
       !field(m,state.image,0x13df9d0,cursor,e)||!field(m,cursor,0,ap,e)||ap!=state.image+0x1786dd0||
       !field(m,cursor,0x190,behaviour,e)||!field(m,behaviour,0,ap,e)||ap!=state.image+0x17418b0||
       !field(m,behaviour,0x148,listener,e)||listener!=selector+0x318)
        return fail(e,e.empty()?"Quest edit original Display/List/Selector/active Cursor binding differs":e);
    size_t matches=0;for(int32_t i=0;i<count;++i){uintptr_t row=0;if(!field(m,data,size_t(i)*8,row,e))return false;matches+=row==selector;}
    return matches==1||fail(e,"Quest selected row membership is not unique");
}
void capture(uintptr_t caller,void* display,void* destination_pointer)try{
    if(caller!=state.image+0x5ce471||!net::session_wire::connected()){
        if(original)original(display,destination_pointer);else {fault=true;reject("Quest display original unavailable");}return;
    }
    std::string e;a::Binding binding;const auto snapshot=net::current_snapshot_id();Memory m;
    if(!ready||fault||pending||GetCurrentThreadId()!=state.thread||!codes(m,true,e)||!typed_action_runtime::network_binding(snapshot,binding,e)){
        reject(e.empty()?"Quest player capture requires ready idle Controller":e);return;}
    uintptr_t quest=0,npc=0,region=0;uint64_t uid=0;int32_t region_id=-1;std::string key;
    if(!field(m,uintptr_t(display),0x1b0,quest,e)||!field(m,quest,0xb8,npc,e)||!field(m,npc,0x18,uid,e)||
       !action_runtime::local_key(npc,uid,key,e)||!field(m,npc,0x178,region,e)||!field(m,region,0x834,region_id,e)){
        reject(e);return;}
    action_runtime::ui::LocalBinding local;Pin p;uintptr_t selector=0;
    if(!owner(m,binding,snapshot,key,region_id,local,p,e)||local.object!=npc||!ui_binding(m,uintptr_t(display),quest,local,selector,e)){reject(e.empty()?"Quest selected owner differs":e);return;}
    int32_t sequence=-1;if(!field(m,quest,0x84,sequence,e)){reject(e);return;}
    qr::Witness witness;d::Sample sample;a::NpcQuestState before;uintptr_t actual=0;
    if(!state_read(m,binding,snapshot,p,npc,sequence,witness,sample,before,actual,e)||actual!=quest||sample.undo.active_graph){reject(e.empty()?"Quest capture slot/Undo differs":e);return;}
    a::NpcQuestIntent intent;intent.region=region_id;intent.before=before.record;
    if(!target_intent(m,binding,snapshot,uintptr_t(destination_pointer),intent,e)){reject(e);return;}
    auto expected=intent.before;expected.destination_key=intent.destination_key;expected.action=intent.action;expected.monsters_to_kill=intent.kills;expected.monster_type=intent.monster_type;
    if(!a::npc_quest_record_transition(intent,expected)){reject("Quest destination edit is an original no-op");return;}
    pending=Pending{binding,snapshot,key,ui_token,intent};++captures;
    if(!ui_token.empty()){auto& job=jobs.at(ui_token);job["captured"]=true;job["caller_rva"]="0x5ce471";}
}catch(const std::exception& x){fault=true;reject(std::string("Quest capture boundary: ")+x.what());}
 catch(...){fault=true;reject("Quest capture non-standard exception");}
void entry(void* display,void* destination_pointer){capture(uintptr_t(_ReturnAddress()),display,destination_pointer);}
}

bool npc_destination(uintptr_t pointer,std::string& key,std::string& e){
    key.clear();Memory m;host::LookupApi api;
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(e,"Quest NPC destination requires ready Mainthread");
    if(!host::resolve_lookup(m.process,state.image,exe_sha,api,e))return false;
    host::NativeOrigin origin;host::Reader reader(m.process,origin,state.image,state.thread,api);host::WorldState world;
    if(!reader.current(world,true,e))return false;
    const a::Binding binding{world.origin.world_epoch,world.origin.generation};
    const auto snapshot=net::current_snapshot_id();
    auto resolve=[&](uintptr_t object,uint64_t uid,uintptr_t region,std::string& canonical,std::string& cause){
        if(!action_runtime::local_key(object,uid,canonical,cause))return false;
        action_runtime::ui::LocalBinding local;uintptr_t actual_region=0;npc_census::State census;
        if(!action_runtime::target_binding(npc_target_key::argument(canonical),binding,snapshot,local,actual_region,census,cause))return false;
        return (local.object==object&&local.uid==uid&&actual_region==region)||fail(cause,"Quest NPC destination full current UID/manager binding differs");
    };
    npc_quest_npc_destination::Witness witness;
    if(!npc_quest_npc_destination::bind(m,state.image,pointer,resolve,witness,e))return false;
    host::WorldState after;if(!reader.current(after,true,e))return false;
    if(!action_runtime::same_world(world,after))return fail(e,"Quest NPC destination native world changed during binding");
    key=witness.key;return true;
}
bool direct_destination(uintptr_t pointer,std::string& key,std::string& e){
    key.clear();Memory m;Pin p;uint64_t uid=0;uintptr_t region=0;int32_t saved=-1;
    if(!state.ready||GetCurrentThreadId()!=state.thread||!host::resolve_lookup(m.process,state.image,exe_sha,p.api,e))return false;
    host::NativeOrigin origin;host::Reader reader(m.process,origin,state.image,state.thread,p.api);
    if(!reader.current(p.world,true,e)||!field(m,pointer,0x18,uid,e)||!field(m,pointer,0x178,region,e))return false;
    uintptr_t actor_shard=0;
    if(!field(m,pointer,0x170,actor_shard,e))return false;
    if(actor_shard!=p.world.root){
        bool held=false;
        if(!typed_action_runtime::resolve_held_dungeon_destination(pointer,key,held,e))return false;
        if(held)return true;
        dungeon_scene_identity_runtime::Census scenes;
        if(!dungeon_scene_identity_runtime::read_current(scenes,e))return false;
        if(scenes.world.uuid!=p.world.origin.uuid||scenes.world.world_epoch!=p.world.origin.world_epoch||
           scenes.world.generation!=p.world.origin.generation||uintptr_t(scenes.world.map)!=uintptr_t(p.world.origin.map))
            return fail(e,"Quest Dungeon direct Monster current world lifetime differs");
        const auto pool=std::find_if(scenes.pools.begin(),scenes.pools.end(),[&](const auto& row){return row.scene.owner.shard==actor_shard&&row.scene.owner.region==region;});
        if(pool==scenes.pools.end())return fail(e,"Quest Dungeon direct Monster scene is absent from the current exterior/template/Instance roster");
        const identity::Scope actual_scope{p.world.origin.uuid,p.world.origin.world_epoch,p.world.origin.generation,p.world.root,region,pool->managers[2],pool->scene.owner.key};
        auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return field(m,object,offset,value,cause);};
        auto find=[&](uint64_t full_uid,uintptr_t& found,std::string& cause){const auto exception=lookup(p.api.find,full_uid,found);
            return !exception||fail(cause,"Quest Dungeon Monster full UID lookup SEH="+std::to_string(exception));};
        auto resolve=[&](const identity::Scope& actual,uintptr_t object,uint64_t full_uid,std::string& canonical,std::string& cause){return identity::registry.resolve(actual,object,full_uid,canonical,cause);};
        dungeon_monster_reference::Witness reference;
        if(!dungeon_monster_reference::read(at,find,resolve,state.image,actual_scope,pool->scene,pool->managers[2],pointer,reference,e))return false;
        host::WorldState after;
        if(!reader.current(after,true,e)||!action_runtime::same_world(p.world,after))return fail(e,e.empty()?"Quest Dungeon direct Monster world changed during reference binding":e);
        key=reference.key;return true;
    }
    if(!field(m,region,0x834,saved,e))return false;
    p.region.image=state.image;p.region.map=uintptr_t(p.world.origin.map);p.region.root=p.world.root;p.region.saved_region=saved;
    const auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return field(m,object,offset,value,cause);};
    if(!region_saved_identity::resolve(state.image,p.region.map,p.region.root,saved,region,p.region.region,at,e))return false;
    d::Sample sample;if(!d::sample(m,p.region,sample,e))return false;
    const auto found=std::find_if(sample.state.monsters.begin(),sample.state.monsters.end(),[&](const auto& row){return row.pointer==pointer&&row.uid==uid;});
    if(found==sample.state.monsters.end()||found->zone||!full_uid(m,p,*found,e))return fail(e,e.empty()?"Quest direct Monster current full UID/owner absent":e);
    return identity::registry.resolve(scope(p,sample),pointer,uid,key,e);
}
#include "npc_quest_rating_runtime.inc"
bool enable(std::string& e){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(e,"Quest authoring requires bound Monster/NPC runtime Mainthread");
    Memory m;if(ready){if(pending||fault||!ui_token.empty())return fail(e,"Quest authoring retains unresolved capture");
        for(const auto& [id,op]:operations)if(!op.retired)return fail(e,"Quest authoring unretired action="+id);
        if(!codes(m,true,e)||!rating_enable(e))return false;operations.clear();jobs.clear();cause.clear();return true;}
    if(installed)return fail(e,"Quest capture earlier installation failed: "+cause);installed=true;
    if(!codes(m,false,e))return false;
    const auto failure=[&](const char* step,MH_STATUS value){cause=std::string("Quest display ")+step+" status="+std::to_string(value);
#if defined(MT2MP_FRAMEWORK_PLUGIN)
        const auto& upstream=mt2mp::hooks::framework::process_backend().error();if(!upstream.empty())cause+=" cause="+upstream;
#endif
        return fail(e,cause);};
    auto result=MH_CreateHook(reinterpret_cast<void*>(state.image+0x4db150),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(result!=MH_OK||!original)return failure("CreateHook",result);
    result=MH_EnableHook(reinterpret_cast<void*>(state.image+0x4db150));if(result!=MH_OK)return failure("EnableHook",result);
    if(!codes(m,true,e)||!rating_enable(e))return false;ready=true;return true;
}
bool pending_ui(){return pending.has_value()||!ui_token.empty();}
void tick(){
    const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:jobs){(void)token;if(job.value("submitted",false)&&!job.value("terminal",false)&&
       controller.value("action_uuid",std::string{})==job.value("action_uuid",std::string{})&&!controller.value("pending",true)){
        job["terminal"]=true;job["outcome"]=controller.value("outcome",0);job["confirmed"]=controller.value("outcome",0)==int(a::Outcome::Confirmed);
        job["unknown"]=controller.value("outcome",0)==int(a::Outcome::Unknown);job["cause"]=controller.value("cause",std::string{});}}
    if(!pending)return;auto saved=*pending;pending.reset();std::string e;a::Binding binding;
    const auto failed=[&](){fault=true;reject(e);if(!saved.token.empty()){auto& job=jobs.at(saved.token);job["terminal"]=true;job["unknown"]=true;job["cause"]=e;}};
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,e)||binding!=saved.binding){if(e.empty())e="Quest capture world or Mainthread changed";failed();return;}
    const auto id=action_runtime::uuid(e);
    if(id.empty()||!typed_action_runtime::submit_npc_quest(id,saved.owner,saved.intent,e)){failed();return;}
    ++submitted;last_action=id;if(!saved.token.empty()){jobs.at(saved.token)["submitted"]=true;jobs.at(saved.token)["action_uuid"]=id;}
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::SetNpcQuestDestination){e="Quest authoring runtime or action unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Quest edit already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="Quest edit requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory m;Pin p;action_runtime::ui::LocalBinding local;qr::Witness before;d::Sample sample;a::NpcQuestState value;
    uintptr_t quest=0,target=0;uint64_t target_uid=0;const auto& intent=request.npc_quest;
    if(!codes(m,true,e)||!owner(m,binding,request.snapshot_uuid,request.canonical_record_key,intent.region,local,p,e)||
       !destination(m,binding,request.snapshot_uuid,intent.destination_region,intent.family,intent.destination_key,target,target_uid,e)||
       !state_read(m,binding,request.snapshot_uuid,p,local.object,intent.before.sequence_id,before,sample,value,quest,e))return a::Outcome::RejectedNoEffect;
    auto actual=intent;if(!target_intent(m,binding,request.snapshot_uuid,target,actual,e)||actual!=intent||value.record!=intent.before||sample.undo.active_graph){
        if(e.empty())e="Quest edit intent, original destination semantics, before row or Undo differs";return a::Outcome::RejectedNoEffect;}
    if(authority){auto local_before=value;local_before.global_random=authority->quest_before.global_random;
        if(local_before!=authority->quest_before||ledger(sample.money)!=authority->before){e="Quest Client before row/global order/Undo/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.pin=p;op.owner=local.object;op.quest=quest;op.destination=target;op.destination_uid=target_uid;
    const auto unknown=[&](const std::string& why){e=op.cause=why.empty()?"Quest edit failed without cause":why;return a::Outcome::Unknown;};
    if(authority){op.seed_attempted=true;const auto seh=action_runtime::seed(state.image,authority->quest_before.global_random);
        if(seh)return unknown("Quest Host RNG adoption SEH="+std::to_string(seh));
        const auto region_random=sample.state.region_random;
        if(!state_read(m,binding,request.snapshot_uuid,p,local.object,intent.before.sequence_id,before,sample,value,quest,e)||
           value!=authority->quest_before||sample.state.region_random!=region_random)return unknown(e.empty()?"Quest Host RNG readback differs":e);}
    op.before=before;op.sample=sample;op.evidence.before=ledger(sample.money);op.evidence.quest_before=value;
    op.attempted=true;const auto seh=set_destination(quest,target,intent.action,intent.kills);
    if(seh)return unknown("Quest original SetDestination SEH="+std::to_string(seh));op.returned=true;
    Pin after_pin;action_runtime::ui::LocalBinding after_local;d::Sample after;a::NpcQuestState final;uintptr_t after_quest=0;
    if(!owner(m,binding,request.snapshot_uuid,request.canonical_record_key,intent.region,after_local,after_pin,e)||after_local!=local||!same_world(p.world,after_pin.world)||
       !state_read(m,binding,request.snapshot_uuid,after_pin,local.object,intent.before.sequence_id,op.after,after,final,after_quest,e)||after_quest!=quest||
       !npc_quest_authoring_read::transition(before,op.after,quest,target,target_uid,intent,e)||!inverse(m,op,after,e))return unknown(e.empty()?"Quest original changed current owner or slot identity":e);
    op.evidence.quest_after=final;op.evidence.after=ledger(after.money);op.evidence.undo_confirmed=true;op.graph=after.undo.undo.back();
    if(!a::npc_quest_transition(intent,value,final)||op.evidence.before!=op.evidence.after||sample.state.region_random!=after.state.region_random||
       (authority&&(final!=authority->quest_after||op.evidence.after!=authority->after)))return unknown("Quest original row/RNG/Undo/ledger differs from Host");
    op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}catch(const std::exception& x){e=std::string("Quest edit perform boundary: ")+x.what();auto it=operations.find(request.action_uuid);if(it!=operations.end())it->second.cause=e;return a::Outcome::Unknown;}
 catch(...){e="Quest edit perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned)
        return fail(e,"Quest observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory m;Pin p;action_runtime::ui::LocalBinding local;qr::Witness now;d::Sample sample;a::NpcQuestState value;uintptr_t quest=0;
    if(!owner(m,binding,request.snapshot_uuid,request.canonical_record_key,request.npc_quest.region,local,p,e)||local.object!=op.owner||!same_world(p.world,op.pin.world)||
       !state_read(m,binding,request.snapshot_uuid,p,local.object,request.npc_quest.before.sequence_id,now,sample,value,quest,e)||quest!=op.quest||value!=op.evidence.quest_after||
       ledger(sample.money)!=op.evidence.after||sample.undo.undo.empty()||sample.undo.undo.back()!=op.graph||!inverse(m,op,sample,e)||
       !npc_quest_authoring_read::transition(op.before,now,op.quest,op.destination,op.destination_uid,request.npc_quest,e))return fail(e,e.empty()?"Quest current original receipt differs before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& e){
    a::Evidence out;if(!observe(binding,request,out,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired)return fail(e,"Quest receipt already retired");
    if(!quest_history::remember(binding,request,op.owner,op.graph,op.before,op.after,e))return false;
    op.retired=true;return true;
}
json status(){
    const auto value=[](const a::NpcQuestState& v){return json{{"record",quest_bridge::record_json(v.record)},
        {"all_sha256",v.all_sha256},{"count",v.count},{"completed",v.completed},{"undo_count",v.undo_count},{"global_random",v.global_random}};};
    const auto money=[](const a::Ledger& v){return json{{"cash",v.cash},{"flat_day",v.flat_day},{"record_present",v.record_present},
        {"record_day",v.record_day},{"record_cash",v.record_cash},{"record_income",v.record_income},{"record_expense",v.record_expense},
        {"line_counts",v.line_counts},{"line_income",v.line_income}};};
    json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"owner_key",op.request.canonical_record_key},{"sequence_id",op.request.npc_quest.before.sequence_id},
        {"destination_key",op.request.npc_quest.destination_key},{"attempted",op.attempted},{"returned",op.returned},{"seed_attempted",op.seed_attempted},
        {"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"before",quest_bridge::record_json(op.evidence.quest_before.record)},
        {"after",quest_bridge::record_json(op.evidence.quest_after.record)},{"before_sha256",op.evidence.quest_before.all_sha256},{"after_sha256",op.evidence.quest_after.all_sha256},
        {"evidence",{{"before",money(op.evidence.before)},{"after",money(op.evidence.after)},
            {"quest_before",value(op.evidence.quest_before)},{"quest_after",value(op.evidence.quest_after)},
            {"undo_confirmed",op.evidence.undo_confirmed},{"retirement_confirmed",op.retired}}}};
    return {{"rating_authority",rating_status()},{"enabled",ready},{"pending_ui",pending_ui()},{"fault",fault},{"captures",captures},{"submitted",submitted},{"rejected",rejected},
        {"cause",cause},{"last_action",last_action},{"operations",rows},{"ui_jobs",jobs}};
}
#include "npc_quest_authoring_ui.inc"
}

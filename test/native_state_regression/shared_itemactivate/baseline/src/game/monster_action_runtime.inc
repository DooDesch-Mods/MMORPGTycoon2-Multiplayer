namespace monster_action_runtime {
namespace {
namespace a=mt2mp::action_session;
namespace d=zone_probe_detail;
namespace q=npc_quest_targets;
namespace identity=direct_monster_identity;
using Release=void(*)(void*,void*,const float*);
#include "monster_tool_read.inc"
struct Pin {host::WorldState world;q::World region;host::LookupApi api;};
struct Pending {a::MonsterIntent intent;a::Binding binding;std::string snapshot,token;};
struct Operation {
    a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;
    uintptr_t pointer=0;uint64_t uid=0;unsigned attempted=0,returned=0;
    bool confirmed=false,retired=false;std::string cause;json preparation=json::object();
};
struct Runtime {
    uintptr_t image=0;uint32_t thread=0;Release release=nullptr;
    bool installed=false,ready=false,replaying=false,fault=false;
    std::optional<Pending> pending;std::map<std::string,Operation> operations;
    uint64_t captures=0,submitted=0,rejected=0;std::string cause,last_action,ui_token;std::map<std::string,json> ui_jobs;
} state;
void release_entry(void*,void*,const float*);
bool move_reset(std::string&);
json move_status();
bool remove_reset(std::string&);
json remove_status();
json remove_quest_preflight_status();
a::Ledger ledger(const native::SimulationState& m){return {m.cash,m.clock.flat_day,int32_t(m.record_present),
    m.record_day,m.record_cash,m.record_income,m.record_expense,m.line_counts,m.line_income};}
bool reject(const std::string& cause){
    ++state.rejected;if(state.cause!=cause){state.cause=cause;LOG_ERROR("[UIMonster] {}",cause);}return false;
}
bool same_world(const host::WorldState& x,const host::WorldState& y){return action_runtime::same_world(x,y);}
identity::Scope scope(const Pin& p,const d::Sample& s){return {p.world.origin.uuid,p.world.origin.world_epoch,
    p.world.origin.generation,p.world.root,p.region.region,s.state.monster_manager};}
bool code(Memory& m,bool hooked,std::string& e){
    for(const auto& c:elite_probe_codes::values){
        if(c.rva==0x67fb40){if(!zone_action_runtime::verify_owned_entry(c.rva,e))return false;continue;}
        std::string bytes(c.size,'\0'),sha;if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e))return false;
        if(c.rva==0x5bf3c0&&hooked){
            std::array<unsigned char,5> prefix{};std::array<unsigned char,6> jump{};uintptr_t target=0;
            constexpr std::array<unsigned char,6> ff25{0xff,0x25,0,0,0,0};
            if(uint8_t(bytes[0])!=0xe9||!state.release||
               !m.copy(uintptr_t(state.release),prefix.data(),prefix.size(),e)||prefix!=monster_hook_contract::prefix||
               !m.copy(uintptr_t(state.release)+5,jump.data(),jump.size(),e)||jump!=ff25||
               !m.copy(uintptr_t(state.release)+11,&target,8,e)||target!=state.image+c.rva+5)
                return fail(e,e.empty()?"Monster Release trampoline ownership differs":e);
            int32_t delta=0;std::memcpy(&delta,bytes.data()+1,4);const auto relay=uintptr_t(int64_t(state.image+c.rva+5)+delta);
            if(!m.copy(relay,jump.data(),jump.size(),e)||jump!=ff25||!m.copy(relay+6,&target,8,e)||
               target!=uintptr_t(&release_entry))return fail(e,e.empty()?"Monster Release owned relay differs":e);
            if(!quest_bridge::sha256(bytes.substr(5),sha,e)||sha!=monster_hook_contract::tail_sha)
                return fail(e,e.empty()?"Monster immutable Release tail differs":e);
        }else if(!quest_bridge::sha256(bytes,sha,e)||sha!=c.sha)return fail(e,e.empty()?"Monster native body differs RVA="+std::to_string(c.rva):e);
    }
    uintptr_t actual=0;
    return field(m,state.image+0x1739500,0xe8,actual,e)&&
        ((actual==state.image+0x5bf3c0)||fail(e,"Monster primary Release address point differs"));
}
bool pin(Memory& m,const a::Binding& binding,const std::string& snapshot,int32_t region,Pin& p,std::string& e){
    if(!state.ready||state.thread!=GetCurrentThreadId())return fail(e,"Monster action requires installed bound Mainthread");
    if(!host::resolve_lookup(m.process,state.image,exe_sha,p.api,e))return false;
    host::NativeOrigin origin;host::Reader reader(m.process,origin,state.image,state.thread,p.api);
    if(!reader.current(p.world,true,e)||!action_runtime::bound(p.world,binding,snapshot,e))return false;
    p.region.image=state.image;p.region.map=uintptr_t(p.world.origin.map);p.region.root=p.world.root;p.region.saved_region=region;
    const auto at=[&](uintptr_t address,size_t offset,auto& value,std::string& cause){return field(m,address,offset,value,cause);};
    uint8_t owned=0;uintptr_t dungeon=0;
    if(!region_saved_identity::resolve(state.image,p.region.map,p.region.root,region,0,p.region.region,at,e)||
       !field(m,p.region.region,0x850,owned,e)||owned!=1||!field(m,p.world.root,0x18,dungeon,e)||dungeon)
        return fail(e,e.empty()?"Monster action requires the current owned surface Region":e);
    return true;
}
bool full_uid(Memory& m,const Pin& p,const monster_zone_read::Monster& row,std::string& e){
    uintptr_t found=0,ap=0,proxy=0;uint64_t actual=0;const auto seh=lookup(p.api.find,row.uid,found);
    if(seh||found!=row.pointer||!field(m,row.pointer,0x18,actual,e)||actual!=row.uid||
       !field(m,row.pointer,0,ap,e)||ap!=state.image+0x16c0770||!field(m,row.pointer,0x8d8,proxy,e)||(!row.zone&&proxy))
        return fail(e,e.empty()?"Monster full UID/class/current-manager/null-Zone proxy differs SEH="+std::to_string(seh):e);
    return true;
}
bool effect(Memory& m,const Pin& p,d::Sample& sample,a::MonsterState& value,std::string& e,
            uint64_t actor_uid=0,const std::string& pending={}){
    if(!d::sample(m,p.region,sample,e))return false;
    npc_quest_target_runtime::CurrentTargets targets;
    if(!npc_quest_target_runtime::read_current(p.region.saved_region,targets,e)||targets.monster_zone_keys.size()!=sample.state.zones.size())
        return fail(e,e.empty()?"Monster canonical Zone pool count differs":e);
    auto content=d::state_json(sample.state);for(const char* key:{"local_identity_scope","global_random","region_random","next_monster_tag","created_monsters"})content.erase(key);
    std::map<uint64_t,std::string> keys;for(size_t i=0;i<sample.state.zones.size();++i)keys.emplace(sample.state.zones[i].value.uid,targets.monster_zone_keys[i]);
    const auto name_read=[&](uintptr_t object,std::string& name,std::string& cause){return npc_census::read_name(m,object,name,cause);};
    const auto verify=[&](const monster_zone_read::Monster& row,std::string& cause){return full_uid(m,p,row,cause);};
    if(!monster_canonical::normalize(sample.state,keys,scope(p,sample),content,name_read,verify,e,pending))return false;
    value={};auto& out=value.roster;out.zones=uint32_t(sample.state.zones.size());out.monsters=uint32_t(sample.state.monsters.size());
    out.undo_count=uint32_t(sample.undo.undo.size());out.next_tag=sample.state.next_monster_tag;out.created_monsters=sample.state.created_monsters;
    out.global_random=sample.state.global_random;out.region_random=sample.state.region_random;out.release_edits=sample.release_edits;
    if(!quest_bridge::sha256(content.dump(),out.content_sha256,e))return false;
    if(actor_uid){const auto found=std::find_if(sample.state.monsters.begin(),sample.state.monsters.end(),[&](const auto& row){return row.uid==actor_uid;});
        if(found==sample.state.monsters.end()||found->zone)return fail(e,"Monster created full UID is absent/direct Zone differs");
        const auto& row=*found;value.actor={row.tag,row.status,row.rank,row.slot,row.type,row.level,{},row.position,row.scale,row.rotation};
        if(!name_read(row.pointer,value.actor.name,e))return false;}
    return true;
}
uint32_t input(uintptr_t rva,const ToolPin& tool,const std::array<float,3>& point)noexcept{
    __try{auto call=rva==0x5bf3c0?state.release:reinterpret_cast<Release>(state.image+rva);
        call(reinterpret_cast<void*>(tool.behaviour),reinterpret_cast<void*>(tool.cursor),point.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct ActivationReadback {
    uintptr_t lookup_item=0;bool lookup_returned=false,attempted=false,returned=false;
};
uint32_t activate(uintptr_t player,const char* definition,uintptr_t expected,ActivationReadback& out)noexcept{
    struct String{uintptr_t data=0;size_t size=0;unsigned char storage[16]{};};String value;bool made=false;uint32_t code=0;
    out={};
    __try{reinterpret_cast<void(*)(void*,const char*)>(state.image+0x49e090)(&value,definition);made=true;
        out.lookup_item=reinterpret_cast<uintptr_t(*)(void*,const void*)>(state.image+0xd2fc0)(reinterpret_cast<void*>(player),&value);out.lookup_returned=true;
        if(out.lookup_item==expected){out.attempted=true;
            reinterpret_cast<void(*)(void*,const void*)>(state.image+0xd30c0)(reinterpret_cast<void*>(player),&value);out.returned=true;}}
    __except(EXCEPTION_EXECUTE_HANDLER){code=GetExceptionCode();}
    if(made){__try{reinterpret_cast<void(*)(void*)>(state.image+0x11bd1e0)(&value);}__except(EXCEPTION_EXECUTE_HANDLER){if(!code)code=GetExceptionCode();}}
    return code;
}
uint32_t set_transform(const ToolPin& tool,const a::MonsterIntent& intent)noexcept{
    __try{reinterpret_cast<void(*)(void*,const float*,const float*,const float*)>(state.image+0x9ff370)(
        reinterpret_cast<void*>(tool.behaviour+0x36c),intent.rotation.data(),intent.position.data(),intent.scale.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool no_gameplay_change(const d::Sample& before,const d::Sample& after){
    auto x=d::sample_json(before),y=d::sample_json(after);x["state"].erase("global_random");y["state"].erase("global_random");
    return x==y&&before.state.monsters==after.state.monsters&&before.state.zones==after.state.zones&&
        before.state.manager==after.state.manager&&before.state.monster_manager==after.state.monster_manager;
}
bool pose(Memory& m,const ToolPin& tool,const a::MonsterIntent& intent,uintptr_t expected_region,std::string& e,bool complete=true){
    uintptr_t active=0,preview=0,character=0,region=0;uint8_t alternate=2;
    std::array<float,3> position{},scale{};std::array<float,4> rotation{};
    return field(m,tool.cursor,0x190,active,e)&&active==tool.behaviour&&
        field(m,tool.behaviour,0x338,preview,e)&&preview&&field(m,tool.behaviour,0x340,character,e)&&character==tool.character&&
        field(m,tool.behaviour,0x350,region,e)&&region==expected_region&&field(m,tool.behaviour,0x3e5,alternate,e)&&alternate==0&&
        field(m,tool.behaviour,0x37c,position,e)&&position==intent.position&&
        field(m,tool.behaviour,0x36c,rotation,e)&&field(m,tool.behaviour,0x388,scale,e)&&
        (!complete||(rotation==intent.rotation&&scale==intent.scale))||
        fail(e,e.empty()?"Monster original preview/selection/pose/alternate differs":e);
}
bool prepare(Memory& m,const Pin& p,const a::MonsterIntent& intent,ToolPin& tool,Operation& op,std::string& e){
    op.preparation={{"phase","tutorial"},{"cause",""}};
    const auto stopped=[&](const std::string& why){const auto cause=e.empty()?why:why+"; cause="+e;op.preparation["cause"]=cause;return fail(e,cause);};
    json inventory,options,region,tutorial;bool clear=false;
    if(!tutorial_clear(m,state.image,p.world,tutorial,clear,e)||!clear)return fail(e,e.empty()?"Monster original tutorial message blocks action":e);
    uintptr_t manager=0,player=0,active=0;
    if(!field(m,state.image,0x13df650,manager,e)||manager!=uintptr_t(p.world.origin.world)+0x180)return fail(e,"Monster WorldViewManager owner differs");
    op.preparation["phase"]="region-entry";
    op.attempted|=1;auto seh=prepare_region_call(state.image,manager,p.region.region);
    if(seh)return fail(e,"Monster original Region entry SEH="+std::to_string(seh));op.returned|=1;
    op.preparation["phase"]="inventory";
    if(!tools(m,state.image,tool,inventory,e)||!tool.behaviour||tool.rank!=intent.rank||tool.loose!=intent.loose||
       !field(m,tool.cursor,0x190,active,e))return fail(e,e.empty()?"Monster actual inventory configuration differs":e);
    op.preparation["inventory"]={{"definition_name",tool.definition_name},{"behaviour_key",intent.tool},
        {"local_item",tool.item},{"local_type",tool.type},{"local_behaviour",tool.behaviour},{"local_cursor",tool.cursor},{"before_active",active}};
    op.preparation["phase"]="activation";
    op.preparation["activation"]={{"needed",active!=tool.behaviour},{"lookup_returned",false},{"attempted",false},{"returned",false}};
    if(active!=tool.behaviour){
        if(!field(m,state.image,0x13df9f0,player,e))return stopped("Monster activation Player read failed");
        ActivationReadback readback;op.attempted|=2;seh=activate(player,tool.definition_name.c_str(),tool.item,readback);
        op.preparation["activation"]={{"needed",true},{"requested_definition",tool.definition_name},{"expected_item",tool.item},
            {"lookup_item",readback.lookup_item},{"lookup_returned",readback.lookup_returned},
            {"attempted",readback.attempted},{"returned",readback.returned},{"exception",seh}};
        if(seh)return stopped("Monster original GetEditItem/ActivateItem SEH="+std::to_string(seh));op.returned|=2;
        if(!readback.lookup_returned||readback.lookup_item!=tool.item||!readback.attempted||!readback.returned)
            return stopped("Monster original GetEditItem definition lookup did not resolve the exact inventory item");
    }
    op.preparation["phase"]="inventory-readback";ToolPin actual;
    if(!tools(m,state.image,actual,inventory,e))return stopped("Monster original inventory readback failed");
    op.preparation["active_tool"]={{"expected_behaviour",tool.behaviour},{"actual_inventory_behaviour",actual.behaviour},
        {"expected_cursor",tool.cursor},{"actual_cursor",actual.cursor},{"expected_item",tool.item},{"actual_item",actual.item},
        {"expected_type",tool.type},{"actual_type",actual.type},{"expected_definition",tool.definition_name},{"actual_definition",actual.definition_name}};
    if(actual.behaviour!=tool.behaviour||actual.cursor!=tool.cursor||actual.item!=tool.item||actual.type!=tool.type||actual.definition_name!=tool.definition_name)
        return stopped("Monster original active inventory binding changed");
    op.preparation["phase"]="active-behaviour";
    if(!field(m,actual.cursor,0x190,active,e))return stopped("Monster active Cursor behaviour read failed");
    op.preparation["active_tool"]["actual_active"]=active;
    if(active!=actual.behaviour)return stopped("Monster original active behaviour differs expected="+std::to_string(actual.behaviour)+" actual="+std::to_string(active));
    op.preparation["phase"]="theme-window";
    if(!selection(m,state.image,uintptr_t(p.world.origin.mode),actual,options,e,true)){
        op.preparation["theme_window"]={{"local_window",actual.window},{"confirmed",false},{"cause",e}};
        return stopped("Monster original ThemeWindow ownership/catalogue did not confirm");}
    op.preparation["theme_window"]={{"local_window",actual.window},{"confirmed",true},{"selected_character",actual.character},
        {"selected_id",actual.character_id},{"options",options}};
    d::ReadMemory bounded{m};monster_zone_selection::Catalogue chosen;
    if(!monster_zone_selection::read(bounded,state.image,intent.character_type,chosen,e)||chosen.id!=intent.character_type||intent.character_slot!=1)return false;
    const std::array<int32_t,2> chosen_id{1,chosen.id};
    bool offered=false;for(const auto& row:options)if(row==json(chosen_id))offered=true;
    if(!offered)return fail(e,"Monster CharacterTypeID absent from actual ThemeWindow options");
    op.preparation["phase"]="select";
    if(actual.character!=chosen.character){op.attempted|=4;seh=original_select(state.image,actual.window,chosen.character);
        if(seh)return fail(e,"Monster original ThemeWindow::Select SEH="+std::to_string(seh));op.returned|=4;}
    if(!selection(m,state.image,uintptr_t(p.world.origin.mode),actual,options,e)||actual.character!=chosen.character)return fail(e,e.empty()?"Monster original selection readback differs":e);
    op.preparation["phase"]="update-region";
    op.attempted|=8;seh=update_call(state.image,actual.behaviour);if(seh)return fail(e,"Monster original Update(0) SEH="+std::to_string(seh));op.returned|=8;
    if(!region_state(m,state.image,p.world,p.region,actual,region,e)||region.at("ready")!=true)return fail(e,e.empty()?"Monster original Region lifecycle not ready":e);
    op.preparation["phase"]="level";
    LevelState levels;if(!level_state(m,state.image,p.world,p.region,actual,levels,e))return false;
    bool enabled=false;for(const auto& row:levels.output.at("rows"))if(row.at("level")==intent.level&&row.at("enabled")==true)enabled=true;
    if(!enabled)return fail(e,"Monster requested level is not an actual enabled row");
    if(levels.output.at("ready")!=true||levels.output.at("selection").at("minimum")!=intent.level){
        const auto command="MessageTo Parent SetValue "+std::to_string(intent.level)+" "+std::to_string(intent.level);
        op.attempted|=16;seh=level_command(state.image,levels.widget,command.c_str());if(seh)return fail(e,"Monster original level command SEH="+std::to_string(seh));op.returned|=16;}
    if(!tools(m,state.image,tool,inventory,e)||!selection(m,state.image,uintptr_t(p.world.origin.mode),tool,options,e)||
       tool.behaviour!=actual.behaviour||tool.character!=chosen.character||tool.character_id!=chosen_id||tool.level!=intent.level||
       !level_state(m,state.image,p.world,p.region,tool,levels,e)||levels.output.at("ready")!=true)return fail(e,e.empty()?"Monster prepared tool/level/selection readback differs":e);
    op.preparation["phase"]="ready";return true;
}
void release_entry(void* receiver,void* cursor,const float* point)try{
    if(state.replaying||!net::session_wire::connected()){state.release(receiver,cursor,point);return;}
    const auto controller=typed_action_runtime::controller_status();std::string e;
    if(!state.ready||GetCurrentThreadId()!=state.thread||uintptr_t(_ReturnAddress())!=state.image+0x587e6c||
       state.pending||state.fault||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)){
        reject("Monster Release requires exact native Cursor caller and idle bound Controller; original suppressed");return;}
    Memory m;ToolPin tool;json inventory,options;uintptr_t region=0;uint8_t pressed=0;
    a::MonsterIntent intent;intent.tool="item_elitemonster";const auto snapshot=net::current_snapshot_id();a::Binding binding;
    if(!code(m,true,e)||!typed_action_runtime::network_binding(snapshot,binding,e)||!tools(m,state.image,tool,inventory,e)||
       tool.behaviour!=uintptr_t(receiver)||tool.cursor!=uintptr_t(cursor)||!point||
       !m.copy(uintptr_t(point),intent.position.data(),12,e)||!field(m,tool.behaviour,0x36c,intent.rotation,e)||
       !field(m,tool.behaviour,0x388,intent.scale,e)||!field(m,tool.behaviour,0x350,region,e)||
       !field(m,region,0x834,intent.region,e)||!field(m,tool.behaviour,0x3e6,pressed,e)||pressed!=1){reject(e.empty()?"Monster Release actual receiver/Region/pressed gate differs":e);return;}
    Pin current;if(!pin(m,binding,snapshot,intent.region,current,e)||region!=current.region.region||
       !selection(m,state.image,uintptr_t(current.world.origin.mode),tool,options,e)){reject(e);return;}
    intent.character_slot=tool.character_id[0];intent.character_type=tool.character_id[1];intent.rank=tool.rank;intent.loose=tool.loose;intent.level=tool.level;
    json actual_region,tutorial;LevelState levels;bool clear=false;
    if(!a::valid_monster_intent(intent)||!pose(m,tool,intent,current.region.region,e)||
       !region_state(m,state.image,current.world,current.region,tool,actual_region,e)||actual_region.at("ready")!=true||
       !level_state(m,state.image,current.world,current.region,tool,levels,e)||levels.output.at("ready")!=true||
       !tutorial_clear(m,state.image,current.world,tutorial,clear,e)||!clear){reject(e.empty()?"Monster Release selection/level/tutorial guard differs":e);return;}
    d::Sample before,after;a::MonsterState value,check;
    if(!effect(m,current,before,value,e)||before.undo.active_graph||!effect(m,current,after,check,e)||value!=check||d::sample_json(before)!=d::sample_json(after)){
        reject(e.empty()?"Monster capture current census/RNG/ledger/Undo changed":e);return;}
    // The original Release owns clearing its pressed byte. It is deferred with
    // creation until Host ordering; no guessed field cleanup runs here.
    state.pending=Pending{intent,binding,snapshot,state.ui_token};++state.captures;
    if(!state.ui_token.empty()){auto& job=state.ui_jobs.at(state.ui_token);job["captured"]=true;job["normal_dispatcher_return_rva"]="0x587e6c";}
}catch(const std::exception& x){state.fault=true;reject(std::string("Monster capture boundary: ")+x.what());}
 catch(...){state.fault=true;reject("Monster capture non-standard boundary exception");}
}

bool enable(std::string& e){
    if(state.ready){
        if(GetCurrentThreadId()!=state.thread||state.pending||state.fault||!state.ui_token.empty())return fail(e,"Monster previous capture/Mainthread/Unknown unresolved");
        for(const auto& [id,op]:state.operations)if(!op.retired)return fail(e,"Monster previous action unretired="+id);
        Memory m;if(!code(m,true,e)||!move_reset(e)||!remove_reset(e))return false;state.operations.clear();state.ui_jobs.clear();state.cause.clear();return true;
    }
    if(state.installed)return fail(e,"Monster capture previous installation failed: "+state.cause);
    state.installed=true;std::string sha;uint32_t thread=0;
    if(!native::read_resolver_receipt(state.image,thread,sha,e)||sha!=exe_sha||thread!=GetCurrentThreadId()||!thread)return false;
    state.thread=thread;Memory m;if(!code(m,false,e))return false;
    const auto hook_failure=[&](const char* operation,MH_STATUS status){
        state.cause=std::string("Monster Release ")+operation+" status="+std::to_string(status);
#if defined(MT2MP_FRAMEWORK_PLUGIN)
        const auto& upstream=mt2mp::hooks::framework::process_backend().error();
        if(!upstream.empty())state.cause+=" cause="+upstream;
#endif
        return fail(e,state.cause);
    };
    const auto created=MH_CreateHook(reinterpret_cast<void*>(state.image+0x5bf3c0),reinterpret_cast<void*>(&release_entry),reinterpret_cast<void**>(&state.release));
    if(created!=MH_OK||!state.release)return hook_failure("MH_CreateHook",created);
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(state.image+0x5bf3c0));
    if(enabled!=MH_OK)return hook_failure("MH_EnableHook",enabled);
    if(!code(m,true,e))return false;state.ready=true;return true;
}
void tick(){
    const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:state.ui_jobs){(void)token;if(!job.value("submitted",false)||job.value("terminal",false))continue;
        if(controller.value("action_uuid",std::string{})==job.value("action_uuid",std::string{})&&!controller.value("pending",true)){
            job["terminal"]=true;job["outcome"]=controller.value("outcome",0);job["confirmed"]=controller.value("outcome",0)==int(a::Outcome::Confirmed);
            job["unknown"]=controller.value("outcome",0)==int(a::Outcome::Unknown);job["cause"]=controller.value("cause",std::string{});}}
    if(!state.pending)return;const auto pending=*state.pending;state.pending.reset();std::string e;a::Binding binding;
    const auto failed=[&](const std::string& cause){state.fault=true;reject(cause);if(!pending.token.empty()){
        auto& job=state.ui_jobs.at(pending.token);job["unknown"]=true;job["terminal"]=true;job["cause"]=cause;}};
    if(!state.ready||state.fault||GetCurrentThreadId()!=state.thread||pending.snapshot!=net::current_snapshot_id()||
       !typed_action_runtime::network_binding(pending.snapshot,binding,e)||binding!=pending.binding){failed(e.empty()?"Monster pending capture world/Mainthread changed":e);return;}
    const auto id=action_runtime::uuid(e);if(id.empty()||!typed_action_runtime::submit_monster(id,pending.intent,e)){failed(e.empty()?"Monster Controller submit failed":e);return;}
    state.last_action=id;++state.submitted;
    if(!pending.token.empty()){auto& job=state.ui_jobs.at(pending.token);job["submitted"]=true;job["action_uuid"]=id;}
}
bool pending_ui(){return state.pending.has_value();}
json status(){
    const auto value=[](const a::MonsterState& v){const auto& r=v.roster;const auto& p=v.actor;
        return json{{"roster",{{"zones",r.zones},{"monsters",r.monsters},{"undo_count",r.undo_count},{"next_tag",r.next_tag},
            {"created_monsters",r.created_monsters},{"global_random",r.global_random},{"region_random",r.region_random},
            {"release_edits",r.release_edits},{"content_sha256",r.content_sha256}}},
            {"actor",{{"tag",p.tag},{"status",p.status},{"rank",p.rank},{"character_id",{p.character_slot,p.character_type}},
                {"level",p.level},{"name",p.name},{"position",p.position},{"rotation",p.rotation},{"scale",p.scale}}}};};
    const auto money=[](const a::Ledger& v){return json{{"cash",v.cash},{"flat_day",v.flat_day},{"record_present",v.record_present},
        {"record_day",v.record_day},{"record_cash",v.record_cash},{"record_income",v.record_income},{"record_expense",v.record_expense},
        {"line_counts",v.line_counts},{"line_income",v.line_income}};};
    json operations=json::object();for(const auto& [id,op]:state.operations)operations[id]={{"attempted",op.attempted},{"returned",op.returned},
        {"confirmed",op.confirmed},{"retired",op.retired},{"local_uid",op.uid},{"object_key","action:"+id},{"cause",op.cause},{"preparation",op.preparation},
        {"intent",{{"region",op.request.monster.region},{"tool",op.request.monster.tool},
            {"character_id",{op.request.monster.character_slot,op.request.monster.character_type}},
            {"level",op.request.monster.level},{"rank",op.request.monster.rank},{"loose",op.request.monster.loose},
            {"position",op.request.monster.position},{"rotation",op.request.monster.rotation},{"scale",op.request.monster.scale}}},
        {"evidence",{{"before",money(op.evidence.before)},{"after",money(op.evidence.after)},
            {"monster_before",value(op.evidence.monster_before)},{"monster_after",value(op.evidence.monster_after)},
            {"undo_confirmed",op.evidence.undo_confirmed},{"retirement_confirmed",op.retired}}}};
    return {{"enabled",state.ready},{"pending_ui",pending_ui()},{"capture_fault",state.fault},{"captures",state.captures},{"submitted",state.submitted},
        {"rejected",state.rejected},{"last_action",state.last_action},{"cause",state.cause},{"operations",operations},{"ui_jobs",state.ui_jobs},
        {"move",move_status()},{"remove",remove_status()},{ "remove_quest_preflight",remove_quest_preflight_status()},{"unproved",{"complete generated combat/loadout state","ongoing Monster simulation","saved direct Monster identity"}}};}

bool native_ui(const json& args,json& result,std::string& e)try{
    result=json::object();
    if(!args.is_object()||args.size()!=7||!args.contains("token")||!args.contains("region_id")||!args.contains("tool")||
       !args.contains("character_slot")||!args.contains("character_type")||!args.contains("level")||!args.contains("position")||
       !args.at("token").is_string()||!args.at("region_id").is_number_integer()||!args.at("tool").is_string()||
       args.at("tool")!="item_elitemonster"||!args.at("character_slot").is_number_integer()||!args.at("character_type").is_number_integer()||
       !args.at("level").is_number_integer()||!args.at("position").is_array()||args.at("position").size()!=3)
        return fail(e,"Monster native UI requires exact token, region, Elite tool, CharacterTypeID, level and position");
    const auto token=args.at("token").get<std::string>();const auto region=args.at("region_id").get<int64_t>();
    const auto slot=args.at("character_slot").get<int64_t>(),type=args.at("character_type").get<int64_t>(),level=args.at("level").get<int64_t>();
    if(!token_valid(token)||state.ui_jobs.contains(token)||state.ui_jobs.size()>=64||region<0||region>INT32_MAX||
       slot!=1||type<0||type>INT32_MAX||level<1||level>INT32_MAX)return fail(e,"Monster native UI token/configuration invalid or already attempted");
    a::MonsterIntent intent;intent.region=int32_t(region);intent.tool="item_elitemonster";intent.character_slot=1;intent.character_type=int32_t(type);intent.level=int32_t(level);
    for(size_t i=0;i<3;++i){if(!args.at("position")[i].is_number())return fail(e,"Monster native UI point requires finite numbers");
        const auto n=args.at("position")[i].get<double>();if(!std::isfinite(n)||std::abs(n)>1000000.0)return fail(e,"Monster native UI point outside domain");intent.position[i]=float(n);}
    if(!state.ready||state.fault||state.pending||!state.ui_token.empty()||GetCurrentThreadId()!=state.thread||!net::session_wire::connected())
        return fail(e,"Monster native UI requires idle connected bound Mainthread hook");
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"Monster native UI requires idle Controller");
    Memory m;a::Binding binding;Pin p;ToolPin tool;json inventory;
    const auto snapshot=net::current_snapshot_id();
    if(!code(m,true,e)||!typed_action_runtime::network_binding(snapshot,binding,e)||!pin(m,binding,snapshot,intent.region,p,e)||
       !tools(m,state.image,tool,inventory,e)||tool.rank<=0||tool.loose!=0)return fail(e,e.empty()?"Monster native UI actual Elite inventory differs":e);
    intent.rank=tool.rank;intent.loose=tool.loose;uintptr_t target=0;
    const auto target_seh=shard_region(state.image,p.world.root,intent.position,target);
    if(target_seh||target!=p.region.region)return fail(e,"Monster native UI requested point Region differs SEH="+std::to_string(target_seh));
    d::Sample before;a::MonsterState before_value;
    if(!effect(m,p,before,before_value,e)||before.undo.active_graph)return fail(e,e.empty()?"Monster native UI requires idle current census/Undo":e);
    result={{"token",token},{"attempted",false},{"returned",false},{"captured",false},{"submitted",false},{"terminal",false},
        {"confirmed",false},{"unknown",false},{"cause",""},{"requested_position",intent.position},{"native_attempted_mask",0},{"native_returned_mask",0}};
    state.ui_jobs.emplace(token,result);auto& job=state.ui_jobs.at(token);Operation preparation;
    const auto publish=[&]{job["preparation"]=preparation.preparation;job["native_attempted_mask"]=preparation.attempted;job["native_returned_mask"]=preparation.returned;result=job;};
    const auto unknown=[&](const std::string& cause){e=cause.empty()?"Monster native UI failed without cause":cause;job["unknown"]=true;job["terminal"]=true;job["cause"]=e;
        state.ui_token.clear();state.pending.reset();state.fault=true;reject(e);publish();return false;};
    job["attempted"]=true;
    if(!prepare(m,p,intent,tool,preparation,e))return unknown(e);
    bool warped=false;preparation.attempted|=1024;auto seh=action_runtime::warp_cursor(state.image,tool.cursor,intent.position,warped);
    if(seh||!warped)return unknown("Monster original Cursor Warp differs SEH="+std::to_string(seh)+" returned="+std::to_string(warped));preparation.returned|=1024;
    const float* hit_pointer=nullptr;preparation.attempted|=2048;seh=action_runtime::calculate_cursor_hit(state.image,tool.cursor,hit_pointer);
    if(seh)return unknown("Monster original Cursor hit SEH="+std::to_string(seh));preparation.returned|=2048;
    std::array<float,3> hit{};uint8_t hovered=0;
    if(uintptr_t(hit_pointer)!=tool.cursor+0x1e4||!m.copy(uintptr_t(hit_pointer),hit.data(),12,e)||!field(m,tool.cursor,0x189,hovered,e)||!hovered)
        return unknown(e.empty()?"Monster original cursor ray did not hit":e);
    for(float n:hit)if(!std::isfinite(n)||std::abs(n)>1000000.f)return unknown("Monster actual cursor hit outside domain");
    seh=shard_region(state.image,p.world.root,hit,target);if(seh||target!=p.region.region)return unknown("Monster actual cursor hit Region differs SEH="+std::to_string(seh));
    job["actual_hit"]=hit;intent.position=hit;
    preparation.attempted|=32;seh=input(0x5bf6e0,tool,hit);if(seh)return unknown("Monster native UI MouseOver SEH="+std::to_string(seh));preparation.returned|=32;
    if(!pose(m,tool,intent,p.region.region,e,false))return unknown(e);
    preparation.attempted|=64;seh=input(0x5beaf0,tool,hit);if(seh)return unknown("Monster native UI Press SEH="+std::to_string(seh));preparation.returned|=64;
    uint8_t monster_pressed=0,released=1,enabled=0,behavior_active=0;int32_t pressed=1;bool modal=true;
    preparation.attempted|=8192;seh=action_runtime::modal_window(state.image,modal);if(seh)return unknown("Monster original modal query SEH="+std::to_string(seh));preparation.returned|=8192;
    if(!field(m,tool.behaviour,0x3e6,monster_pressed,e)||!field(m,tool.cursor,0x238,pressed,e)||!field(m,tool.cursor,0x23c,released,e)||
       !field(m,tool.cursor,0x256,enabled,e)||!field(m,tool.cursor,0x189,hovered,e)||!field(m,tool.behaviour,0x121,behavior_active,e)||
       monster_pressed!=1||pressed>0||released||!enabled||(!hovered&&!behavior_active)||modal)
        return unknown(e.empty()?"Monster original dispatcher gate differs monster_pressed="+std::to_string(monster_pressed)+" cursor_pressed="+std::to_string(pressed)+
            " released="+std::to_string(released)+" enabled="+std::to_string(enabled)+" hovered="+std::to_string(hovered)+" behavior_active="+std::to_string(behavior_active)+" modal="+std::to_string(modal):e);
    d::Sample prepared;a::MonsterState prepared_value;
    if(!effect(m,p,prepared,prepared_value,e)||!no_gameplay_change(before,prepared))return unknown(e.empty()?"Monster UI preparation changed gameplay":e);
    const auto captures=state.captures;float sx=0,sy=0;SDL_GetMouseState(&sx,&sy);const std::array<float,2> screen{sx,sy};
    state.ui_token=token;preparation.attempted|=4096;seh=action_runtime::generic_release(state.image,tool.cursor,screen,hit);state.ui_token.clear();
    if(seh)return unknown("Monster original Cursor Release SEH="+std::to_string(seh));preparation.returned|=4096;job["returned"]=true;
    if(state.captures!=captures+1||!state.pending||state.pending->token!=token||!job.value("captured",false))return unknown("Monster original Cursor Release returned without exact normal hook capture: "+state.cause);
    d::Sample after;a::MonsterState after_value;
    if(!effect(m,p,after,after_value,e)||prepared_value!=after_value||d::sample_json(prepared)!=d::sample_json(after))
        return unknown(e.empty()?"Monster captured Release changed census/RNG/ledger/Undo before ordering":e);
    publish();e.clear();return true;
}catch(const std::exception& x){state.ui_token.clear();state.pending.reset();state.fault=true;e=std::string("Monster native UI boundary: ")+x.what();
    if(result.is_object()&&result.contains("token")){const auto token=result.at("token").get<std::string>();const auto found=state.ui_jobs.find(token);if(found!=state.ui_jobs.end()){
        found->second["unknown"]=true;found->second["terminal"]=true;found->second["cause"]=e;result=found->second;}}
    reject(e);return false;
}

a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& e)try{
    if(request.action!=a::Action::PlaceMonster||!a::valid_monster_intent(request.monster)||!state.ready||GetCurrentThreadId()!=state.thread){e="Monster runtime/Mainthread/typed intent unavailable";return a::Outcome::RejectedNoEffect;}
    if(state.operations.contains(request.action_uuid)){e="Monster action already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){e="Monster lacks exact pending Controller UUID";return a::Outcome::RejectedNoEffect;}
    Memory m;Pin p;d::Sample initial;a::MonsterState initial_state;
    if(!code(m,true,e)||!pin(m,binding,request.snapshot_uuid,request.monster.region,p,e)||!effect(m,p,initial,initial_state,e)||initial.undo.active_graph||initial.state.monsters.size()>=4096)
        return a::Outcome::RejectedNoEffect;
    uintptr_t target_region=0;const auto region_seh=shard_region(state.image,p.world.root,request.monster.position,target_region);
    if(region_seh||target_region!=p.region.region){e="Monster original target point Region differs SEH="+std::to_string(region_seh);return a::Outcome::RejectedNoEffect;}
    auto& op=state.operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;op.pin=p;
    const auto unknown=[&](const std::string& cause){e=op.cause=cause.empty()?"Monster effect failed without cause":cause;return a::Outcome::Unknown;};
    ToolPin tool;if(!prepare(m,p,request.monster,tool,op,e))return unknown(e);
    d::Sample prepared;a::MonsterState prepared_state;
    if(!effect(m,p,prepared,prepared_state,e)||!no_gameplay_change(initial,prepared))return unknown(e.empty()?"Monster original UI preparation changed gameplay":e);
    op.attempted|=32;auto seh=input(0x5bf6e0,tool,request.monster.position);if(seh)return unknown("Monster MouseOver SEH="+std::to_string(seh));op.returned|=32;
    if(!pose(m,tool,request.monster,p.region.region,e,false))return unknown(e);
    op.attempted|=64;seh=input(0x5beaf0,tool,request.monster.position);if(seh)return unknown("Monster Press SEH="+std::to_string(seh));op.returned|=64;
    uint8_t pressed=0;if(!field(m,tool.behaviour,0x3e6,pressed,e)||pressed!=1)return unknown(e.empty()?"Monster original Press did not confirm":e);
    // Press invokes MouseOver again and resets scale from its local size mode.
    // Reconstruct the captured Transform after Press, before original Release.
    op.attempted|=256;seh=set_transform(tool,request.monster);if(seh)return unknown("Monster original Transform constructor SEH="+std::to_string(seh));op.returned|=256;
    if(!pose(m,tool,request.monster,p.region.region,e))return unknown(e);
    d::Sample before;a::MonsterState value;
    if(!effect(m,p,before,value,e)||!no_gameplay_change(prepared,before))return unknown(e.empty()?"Monster MouseOver/Press changed gameplay":e);
    if(authority){
        if(!a::zone_shared_content(value.roster,authority->monster_before.roster)||ledger(before.money)!=authority->before)
            return unknown("Monster Client canonical BEFORE census/counters/Undo/Release/ledger differs from Host");
        const auto local=before.state.region_random;op.attempted|=512;seh=action_runtime::seed(state.image,authority->monster_before.roster.global_random);
        if(seh)return unknown("Monster Host RNG adoption SEH="+std::to_string(seh));
        op.returned|=512;
        if(!effect(m,p,before,value,e)||value.roster.region_random!=local||!a::monster_shared_state(value,authority->monster_before))
            return unknown(e.empty()?"Monster Host RNG adoption/current readback differs":e);
    }
    op.evidence.before=ledger(before.money);op.evidence.monster_before=value;
    op.attempted|=128;state.replaying=true;seh=input(0x5bf3c0,tool,request.monster.position);state.replaying=false;
    if(seh)return unknown("Monster original complete Release SEH="+std::to_string(seh));op.returned|=128;
    Pin after;if(!pin(m,binding,request.snapshot_uuid,request.monster.region,after,e)||!same_world(p.world,after.world)||after.region.region!=p.region.region)return unknown(e.empty()?"Monster Release current world/Region changed":e);
    d::Sample result;if(!d::sample(m,p.region,result,e)||result.state.monsters.size()!=before.state.monsters.size()+1||
       result.state.zones!=before.state.zones||result.state.monster_manager!=before.state.monster_manager||result.state.manager!=before.state.manager)
        return unknown(e.empty()?"Monster original append manager/count/Zone roster differs":e);
    for(size_t i=0;i<before.state.monsters.size();++i)if(before.state.monsters[i]!=result.state.monsters[i])return unknown("Monster Release changed an existing native member/order");
    const auto& added=result.state.monsters.back();if(added.zone||!full_uid(m,p,added,e))return unknown(e.empty()?"Monster new member Zone/full UID differs":e);
    for(const auto& old:before.state.monsters)if(old.uid==added.uid||old.pointer==added.pointer)return unknown("Monster original append reused identity");
    op.pointer=added.pointer;op.uid=added.uid;
    if(!identity::registry.stage(request.action_uuid,scope(p,result),op.pointer,op.uid,e))return unknown(e);
    a::MonsterState final;if(!effect(m,p,result,final,e,op.uid,request.action_uuid))return unknown(e);
    op.evidence.after=ledger(result.money);op.evidence.monster_after=final;
    if(result.undo.undo.empty()||result.undo.active_graph)return unknown("Monster original closed Undo graph absent");
    undo_probe::GraphWitness graph;
    if(!undo_probe::read_graph(m.process,state.image,result.undo.undo.back(),graph,e)||
       !undo_probe::validate_undo_commit(before.undo,result.undo,graph,e)||!graph.locked||graph.operations!=1||graph.nodes||graph.pending_delete)
        return unknown(e.empty()?"Monster original one-operation Undo graph differs":e);
    if(!field(m,tool.behaviour,0x3e6,pressed,e)||pressed!=0)return unknown(e.empty()?"Monster original Release did not clear its pressed flag":e);
    op.evidence.undo_confirmed=true;
    if(op.attempted!=op.returned)return unknown("Monster original call completion mask differs");
    if(const char* cause=a::monster_transition_error(request.monster,value,final))return unknown(std::string("Monster original transition: ")+cause);
    if(op.evidence.before!=op.evidence.after)return unknown("Monster original ledger changed");
    if(authority&&!a::monster_shared_state(final,authority->monster_after))return unknown("Monster Client final actor/census/RNG differs from Host");
    if(authority&&op.evidence.after!=authority->after)return unknown("Monster Client final ledger differs from Host");
    if(!identity::registry.confirm(request.action_uuid,e))return unknown(e);
    op.confirmed=true;e.clear();return a::Outcome::Confirmed;
}catch(const std::exception& x){state.replaying=false;e=std::string("Monster perform boundary: ")+x.what();const auto found=state.operations.find(request.action_uuid);if(found!=state.operations.end())found->second.cause=e;return a::Outcome::Unknown;}
 catch(...){state.replaying=false;e="Monster perform non-standard boundary exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=state.operations.find(request.action_uuid);
    if(found==state.operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)return fail(e,"Monster observe lacks exact confirmed action");
    const auto& op=found->second;Memory m;Pin p;d::Sample sample;a::MonsterState value;
    if(!code(m,true,e)||!pin(m,binding,request.snapshot_uuid,request.monster.region,p,e)||!same_world(p.world,op.pin.world)||
       p.region.region!=op.pin.region.region||!effect(m,p,sample,value,e,op.uid)||
       value!=op.evidence.monster_after||ledger(sample.money)!=op.evidence.after)
        return fail(e,e.empty()?"Monster confirmed current identity/state/ledger changed before ACK":e);
    const auto row=std::find_if(sample.state.monsters.begin(),sample.state.monsters.end(),[&](const auto& v){return v.uid==op.uid&&v.pointer==op.pointer;});
    if(row==sample.state.monsters.end()||!full_uid(m,p,*row,e))return fail(e,e.empty()?"Monster local binding retired before ACK":e);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& e){
    a::Evidence actual;if(!observe(binding,request,actual,e))return false;auto& op=state.operations.at(request.action_uuid);
    if(op.retired)return fail(e,"Monster action receipt already retired");op.retired=true;return true;
}
#include "monster_move_runtime.inc"
#include "monster_info_ui.inc"
#include "monster_move_ui.inc"
#include "monster_remove_runtime.inc"
#include "monster_remove_ui.inc"
#include "monster_live_runtime.inc"
#include "npc_quest_authoring_runtime.inc"
#include "npc_quest_list_runtime.inc"
#include "quest_history_runtime.inc"
#include "character_membership_runtime.inc"
#include "character_rename_runtime.inc"
#include "ability_edit_runtime.inc"
#include "ability_membership_runtime.inc"
#include "costume_edit_runtime.inc"
#include "dungeon_config_runtime.inc"
#include "dungeon_light_runtime.inc"
#include "dungeon_sound_runtime.inc"
#include "dungeon_palette_runtime.inc"
#include "dungeon_gizmo_runtime.inc"
#include "building_status_runtime.inc"
}

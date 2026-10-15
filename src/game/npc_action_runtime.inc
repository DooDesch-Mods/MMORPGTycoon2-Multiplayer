namespace action_runtime {
namespace {
namespace action=mt2mp::action_session;
namespace undo=build_actions::private_native;
namespace ui=action::ui_capture;
using Release=void(__fastcall*)(void*,void*,const float*);
struct Pending {action::NpcIntent intent;action::Binding binding;std::string snapshot,token;};
struct Operation {
    action::Binding binding;action::Request request;action::Evidence evidence;
    uint64_t local_uid=0;uintptr_t local_pointer=0;
    unsigned attempted=0,returned=0;bool confirmed=false,retired=false;
    std::string target_action_uuid;
    int32_t target_region=-1,target_type=-1;
    uint32_t destroyprops_original_calls=0;
    uint32_t move_original_calls=0;
    bool spawn_confirmed=false,actor_confirmed=false;
    json spawn_before=json::object(),spawn_after=json::object();
    bool target_absent=false,selection_cleared=false,undo_confirmed=false;
    json quest_tasks_before=json::array(),quest_tasks_after=json::array();
    std::vector<uintptr_t> remove_quest_pointers;
    std::vector<int32_t> remove_quest_sequence_ids;
    json quest_retirement=json::object();
    std::string cause;
};
struct Runtime {
    uint32_t thread=0;uintptr_t image=0;Release release=nullptr;
    bool install_attempted=false,ready=false,replaying=false,capture_failed=false;
    std::optional<Pending> pending;std::map<std::string,Operation> operations;
    const ui::Replay* remove_replay=nullptr;
    std::map<std::string,json> ui_jobs;std::string diagnostic_token;
    std::string move_ui_token;
    uint64_t captures=0,submits=0,rejections=0;std::string last_action,cause,install_cause;
};
Runtime state;
void rejected(const std::string& cause) {
    ++state.rejections;if(state.cause!=cause){state.cause=cause;LOG_ERROR("[UINPC] {}",cause);}
}
bool same_world(const host::WorldState& a,const host::WorldState& b) {
    return host::same_world(a.origin,b.origin)&&a.root==b.root&&a.saves==b.saves&&
        a.edits==b.edits&&a.active_graph==b.active_graph;
}
action::NpcActor actor(const npc_census::Row& row) {
    return quest_bridge::actor(row,npc_quest_records::State{});
}
bool read_effect(Memory& memory,uintptr_t image,uintptr_t region,uint64_t created_uid,
                 action::NpcState& output,action::Ledger& ledger,undo::UndoWitness& witness,
                 npc_census::State& census,std::string& error,bool quest_pool=false,
                 json* task_receipt=nullptr) {
    if(!npc_census::read(memory,image,region,census,error)||
       !undo::read_undo(memory.process,image,witness,error))return false;
    native::SimulationState money;
    if(!native::read_simulation_state(money,error))return false;
    ledger={money.cash,money.clock.flat_day,int32_t(money.record_present),money.record_day,
        money.record_cash,money.record_income,money.record_expense,money.line_counts,money.line_income};
    int32_t saved_region=-1;
    if(!field(memory,region,0x834,saved_region,error)||saved_region<0)return fail(error,
        error.empty()?"NPC effect saved Region ID invalid":error);
    return quest_bridge::effect_rows(memory,image,saved_region,created_uid,census,witness.undo.size(),
                                     quest_pool,output,task_receipt,error);
}
bool bound(const host::WorldState& world,const action::Binding& binding,
           const std::string& snapshot,std::string& error) {
    return world.origin.world_epoch==binding.native_epoch&&
        world.origin.generation==binding.native_generation&&
        snapshot==net::current_snapshot_id()||
        fail(error,"NPC action current native World or snapshot differs from Controller");
}
uint32_t transform(uintptr_t image,uintptr_t behaviour,const action::NpcIntent& intent) noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*,const float*,const float*,const float*)>(image+0x9ff370)(
        reinterpret_cast<void*>(behaviour+0x368),intent.rotation.data(),intent.position.data(),intent.scale.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t seed(uintptr_t image,const std::array<uint64_t,4>& words) noexcept {
    __try {auto* source=*reinterpret_cast<void**>(image+0x164b250);
        if(!source)return ERROR_INVALID_ADDRESS;std::memcpy(source,words.data(),32);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
std::string uuid(std::string& error) {
    unsigned char bytes[16];const auto result=BCryptGenRandom(nullptr,bytes,sizeof bytes,BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(result){error="NPC UUID NTSTATUS="+std::to_string(result);return {};}
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;char out[37];
    std::snprintf(out,sizeof out,"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    error.clear();return out;
}
void release_entry(void* receiver,void* cursor,const float* point) {
    if(state.replaying||!net::session_wire::connected()){
        if(state.release)state.release(receiver,cursor,point);return;
    }
    std::string error;const auto controller=typed_action_runtime::controller_status();
    if(!state.ready||GetCurrentThreadId()!=state.thread||uintptr_t(_ReturnAddress())!=state.image+0x587e6c||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       state.pending||state.capture_failed){rejected("NPC original Release requires ready idle Controller and exact cursor caller");return;}
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,error)){rejected(error);return;}
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error)){rejected(error);return;}
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world;std::map<std::string,Tool> inventory;
    if(!reader.current(world,true,error)||!bound(world,binding,snapshot,error)||
       !tools(memory,state.image,inventory,error)){rejected(error);return;}
    const auto tool=std::find_if(inventory.begin(),inventory.end(),[receiver](const auto& entry){
        return quest_bridge::tool_type(entry.first)>=0&&
            entry.second.count==1&&entry.second.behaviour==uintptr_t(receiver);});
    if(tool==inventory.end()||tool->second.native_type!=quest_bridge::tool_type(tool->first)){
        rejected("NPC Release receiver is not one unique NPC inventory tool");return;}
    action::NpcIntent intent;intent.type=uint8_t(tool->second.native_type);
    uintptr_t region=0,preview=0,selected_ptr=0,window=0;uint8_t alternate=1,dirty=0;
    std::vector<Option> choices;json selected=nullptr;std::array<float,3> release_point{};
    if(!point||!memory.copy(uintptr_t(point),release_point.data(),sizeof release_point,error)||
       !field(memory,uintptr_t(receiver),0x368,intent.rotation,error)||
       !field(memory,uintptr_t(receiver),0x378,intent.position,error)||
       !field(memory,uintptr_t(receiver),0x384,intent.scale,error)||
       !field(memory,uintptr_t(receiver),0x390,dirty,error)||
       !field(memory,uintptr_t(receiver),0x358,preview,error)||
       !field(memory,uintptr_t(receiver),0x360,selected_ptr,error)||
       !field(memory,uintptr_t(receiver),0x3e1,alternate,error)||
       !options(memory,state.image,uintptr_t(receiver),window,choices,selected,error)){
        rejected(error.empty()?"NPC Release point/transform/selection unreadable":error);return;}
    if(!preview||!selected_ptr||alternate||release_point!=intent.position||
       !selected.is_array()||selected.size()!=2){
        if(!state.diagnostic_token.empty())state.ui_jobs.at(state.diagnostic_token)["capture_guard"]={
            {"preview_present",bool(preview)},{"selected_present",bool(selected_ptr)},
            {"alternate",alternate},{"matrix_cache_dirty",dirty},
            {"release_position",release_point},{"transform_position",intent.position},
            {"pose_equal",release_point==intent.position},{"selected",selected}};
        rejected(!preview?"NPC Release preview absent":
            !selected_ptr?"NPC Release selected Character pointer absent":
            alternate?"NPC Release alternate cursor mode active":
            release_point!=intent.position?"NPC Release point differs from immediate MouseOver transform":
            "NPC Release selected CharacterTypeID shape differs");return;}
    intent.character_slot=selected[0].get<int32_t>();intent.character_type=selected[1].get<int32_t>();
    const auto map_seh=map_region(state.image,uintptr_t(world.origin.map),release_point,region);
    if(map_seh||!region||!field(memory,region,0x834,intent.region,error)||
       !point_and_cursor(memory,state.image,uintptr_t(world.origin.map),world.root,region,
           uintptr_t(cursor),uintptr_t(receiver),release_point,error)){
        rejected(error.empty()?"NPC Release map/shard/cursor region differs":error);return;}
    if(!action::valid_npc(intent)){
        if(!state.diagnostic_token.empty())state.ui_jobs.at(state.diagnostic_token)["typed_capture"]={
            {"region",intent.region},{"type",intent.type},
            {"character_id",{intent.character_slot,intent.character_type}},
            {"position",intent.position},{"rotation",intent.rotation},{"scale",intent.scale}};
        rejected("NPC Release typed region, point, transform or CharacterTypeID invalid region="+
            std::to_string(intent.region)+" character_slot="+std::to_string(intent.character_slot)+
            " character_type="+std::to_string(intent.character_type));return;}
    uintptr_t resolved=0;const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e){
        return field(memory,base,offset,value,e);};
    if(!region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,
        intent.region,region,resolved,read_field,error)||resolved!=region){
        rejected(error.empty()?"NPC Release saved Region identity differs":error);return;}
    npc_census::State census;undo::UndoWitness undo_before;action::Ledger money;action::NpcState before;
    if(!read_effect(memory,state.image,region,0,before,money,undo_before,census,error,intent.type==0)||undo_before.active_graph){
        rejected(error.empty()?"NPC Release requires idle native Undo":error);return;}
    state.pending=Pending{intent,binding,snapshot,state.diagnostic_token};++state.captures;
    if(!state.diagnostic_token.empty()){
        auto& job=state.ui_jobs.at(state.diagnostic_token);job["captured"]=true;
        job["capture_count"]=state.captures;job["intent"]={{"region",intent.region},
            {"type",intent.type},{"character_id",{intent.character_slot,intent.character_type}},
            {"position",intent.position},{"rotation",intent.rotation},{"scale",intent.scale}};
    }
}
uint32_t generic_release(uintptr_t image,uintptr_t cursor,const std::array<float,2>& screen,
                         const std::array<float,3>& world) noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*,const float*,const float*)>(image+0x5879d0)(
        reinterpret_cast<void*>(cursor),screen.data(),world.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t warp_cursor(uintptr_t image,uintptr_t cursor,const std::array<float,3>& target,
                     bool& warped) noexcept {
    __try {warped=reinterpret_cast<bool(__fastcall*)(void*,const float*)>(image+0x5863e0)(
        reinterpret_cast<void*>(cursor),target.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t calculate_cursor_hit(uintptr_t image,uintptr_t cursor,const float*& hit) noexcept {
    __try {hit=reinterpret_cast<const float*(__fastcall*)(void*)>(image+0x588d10)(
        reinterpret_cast<void*>(cursor));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t modal_window(uintptr_t image,bool& modal) noexcept {
    __try {modal=reinterpret_cast<bool(__fastcall*)()>(image+0x81a350)();return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
action::Outcome remove_perform(const action::Binding&,const action::Request&,const action::Evidence*,std::string&);
bool remove_observe(const action::Binding&,const action::Request&,action::Evidence&,std::string&);
bool remove_retire(const action::Binding&,const action::Request&,std::string&);
action::Outcome move_perform(const action::Binding&,const action::Request&,const action::Evidence*,std::string&);
bool move_observe(const action::Binding&,const action::Request&,action::Evidence&,std::string&);
bool move_retire(const action::Binding&,const action::Request&,std::string&);
bool enable(std::string& error) {
    const auto install_fail=[&](const std::string& cause){error=state.install_cause=cause;return false;};
    if(!state.install_attempted)state.thread=GetCurrentThreadId();
    if(state.thread!=GetCurrentThreadId())return fail(error,"NPC Release hook requires bound Mainthread");
    if(state.ready){
        if(!state.release||state.pending)return fail(error,"NPC ready Release trampoline/pending state invalid");
        for(const auto& [id,op]:state.operations)if(!op.retired)
            return fail(error,"NPC previous session has unretired action="+id);
        state.operations.clear();state.ui_jobs.clear();state.move_ui_token.clear();state.capture_failed=false;
        state.cause.clear();state.last_action.clear();error.clear();return true;
    }
    if(state.install_attempted)return fail(error,"NPC Release hook prior installation failed: "+state.install_cause);
    state.install_attempted=true;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(state.image,thread,sha,error)||sha!=exe_sha||thread!=state.thread)
        return install_fail("NPC Release exact EXE/Mainthread differs: "+error);
    Memory memory;uintptr_t actual=0;
    if(!memory.copy(state.image+0x172b450+0xe8,&actual,sizeof actual,error)||actual!=state.image+0x5c3ef0)
        return install_fail("NPC Release vtable entry differs: "+error);
    const auto created=MH_CreateHook(reinterpret_cast<void*>(state.image+0x5c3ef0),
        reinterpret_cast<void*>(&release_entry),reinterpret_cast<void**>(&state.release));
    if(created!=MH_OK||!state.release)return install_fail("NPC Release MH_CreateHook status="+std::to_string(created));
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(state.image+0x5c3ef0));
    if(enabled!=MH_OK)return install_fail("NPC Release MH_EnableHook status="+std::to_string(enabled));
    state.ready=true;state.install_cause.clear();error.clear();return true;
}
void tick() {
    const auto controller=typed_action_runtime::controller_status();
    for(auto& [token,job]:state.ui_jobs){
        if(!job.value("submitted",false)||job.value("terminal",false))continue;
        if(controller.value("action_uuid",std::string{})==job.value("action_uuid",std::string{})&&
           !controller.value("pending",true)){
            job["terminal"]=true;job["outcome"]=controller.value("outcome",0);
            job["confirmed"]=controller.value("outcome",0)==int(action::Outcome::Confirmed);
            job["cause"]=controller.value("cause",std::string{});
        }
    }
    if(!state.pending)return;const auto pending=*state.pending;state.pending.reset();
    std::string error;action::Binding binding;
    if(pending.snapshot!=net::current_snapshot_id()||
       !typed_action_runtime::network_binding(pending.snapshot,binding,error)||binding!=pending.binding){
        state.capture_failed=true;rejected(error.empty()?"NPC pending UI World or Controller changed":error);
        if(!pending.token.empty())state.ui_jobs.at(pending.token)["cause"]=state.cause;return;}
    const auto id=uuid(error);
    if(id.empty()||!typed_action_runtime::submit_npc(id,pending.intent,error)){
        state.capture_failed=true;rejected(error.empty()?"NPC Controller submit failed":error);
        if(!pending.token.empty())state.ui_jobs.at(pending.token)["cause"]=state.cause;return;}
    state.last_action=id;++state.submits;
    if(!pending.token.empty()){auto& job=state.ui_jobs.at(pending.token);
        job["submitted"]=true;job["action_uuid"]=id;}
}
bool pending_ui(){return bool(state.pending);}
json status() {
    const auto npc_state=[](const action::NpcState& value){
        const auto& created=value.created;
        return json{{"count",value.count},{"undo_count",value.undo_count},
            {"roster_sha256",value.roster_sha256},{"global_random",value.global_random},
            {"quest_targets_sha256",value.quest_targets_sha256},
            {"created",{{"name",created.name},{"type",created.type},
                {"character_id",{created.character_slot,created.character_type}},
                {"level",created.level},{"quest_count",created.quest_count},
                {"position",created.position},{"rotation",created.rotation},{"scale",created.scale},
                {"quests",quest_bridge::records_json(created.quests)}}}};
    };
    json operations=json::object();for(const auto& [id,op]:state.operations){
        const char* operation=op.request.action==action::Action::PlaceNPC?"place":
            op.request.action==action::Action::RemoveNPC?"remove":
            op.request.action==action::Action::MoveNPC?"move":"unknown";
        operations[id]={{"operation",operation},
            {"attempted_mask",op.attempted},{"returned_mask",op.returned},
            {"confirmed",op.confirmed},{"retired",op.retired},{"local_uid",op.local_uid},
            {"target_action_uuid",op.target_action_uuid},
            {"canonical_record_key",op.request.canonical_record_key},
            {"destroyprops_original_calls",op.destroyprops_original_calls},
            {"move_original_calls",op.move_original_calls},
            {"spawn_confirmed",op.spawn_confirmed},{"actor_confirmed",op.actor_confirmed},
            {"spawn_before",op.spawn_before},{"spawn_after",op.spawn_after},
            {"target_absent",op.target_absent},{"selection_cleared",op.selection_cleared},
            {"undo_confirmed",op.undo_confirmed},
            {"selection_confirmed",op.evidence.selection_confirmed},
            {"quest_tasks_before",op.quest_tasks_before},
            {"quest_tasks_after",op.quest_tasks_after},
            {"quest_retirement",op.quest_retirement},
            {"npc_before",npc_state(op.evidence.npc_before)},
            {"npc_after",npc_state(op.evidence.npc_after)},
            {"cause",op.cause}};}
    return {{"enabled",state.ready},{"capture_count",state.captures},{"submitted",state.submits},
        {"rejected",state.rejections},{"pending_ui",pending_ui()},{"last_action",state.last_action},
        {"cause",state.cause},{"operations",operations},{"ui_jobs",state.ui_jobs}};
}
bool native_ui(const json& args,json& result,std::string& error) {
    result=json::object();
    if(!args.is_object()||args.size()!=6||!args.contains("token")||
       !args.contains("region_id")||!args.contains("tool")||
       !args.contains("character_slot")||!args.contains("character_type")||
       !args.contains("position")||!args.at("token").is_string()||
       !args.at("region_id").is_number_integer()||!args.at("tool").is_string()||
       !args.at("character_slot").is_number_integer()||
       !args.at("character_type").is_number_integer()||
       !args.at("position").is_array()||args.at("position").size()!=3)
        return fail(error,"NPC native UI requires exact token, region, tool, CharacterTypeID and position");
    const auto token=args.at("token").get<std::string>();const auto id=args.at("region_id").get<int64_t>();
    const auto tool=args.at("tool").get<std::string>();const auto slot=args.at("character_slot").get<int64_t>();
    const auto type=args.at("character_type").get<int64_t>();
    if(!token_valid(token)||state.ui_jobs.contains(token)||state.ui_jobs.size()>=64||
       id<0||id>INT32_MAX||slot<0||slot>INT32_MAX||type<0||type>INT32_MAX||
       quest_bridge::tool_type(tool)<0)
        return fail(error,"NPC native UI token, region, tool or CharacterTypeID invalid/used");
    std::array<float,3> point{};
    for(size_t i=0;i<3;++i){if(!args.at("position")[i].is_number())return fail(error,"NPC native UI point requires numbers");
        const auto value=args.at("position")[i].get<double>();
        if(!std::isfinite(value)||std::abs(value)>1000000.0)return fail(error,"NPC native UI point outside domain");
        point[i]=float(value);}
    result={{"token",token},{"attempted",false},{"returned",false},
        {"captured",false},{"submitted",false},{"confirmed",false},{"terminal",false}};
    if(!state.ready||GetCurrentThreadId()!=state.thread||state.pending||state.capture_failed||
       !net::session_wire::connected())return fail(error,"NPC native UI requires ready connected Mainthread hook");
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(error,"NPC native UI requires idle bound Controller");
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world;uintptr_t region=0,cursor=0,preview=0,window=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    if(!reader.current(world,true,error)||
       !region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,
           int32_t(id),0,region,read_field,error))return false;
    std::map<std::string,Tool> inventory;if(!tools(memory,state.image,inventory,error))return false;
    const auto& item=inventory.at(tool);std::vector<Option> available;json selected=nullptr;
    uint8_t alternate=1;
    if(item.count!=1||item.native_type!=quest_bridge::tool_type(tool)||!item.behaviour||
       !field(memory,state.image,0x13df9d0,cursor,error)||!cursor||
       !point_and_cursor(memory,state.image,uintptr_t(world.origin.map),world.root,
           region,cursor,item.behaviour,point,error)||
       !field(memory,item.behaviour,0x3e1,alternate,error)||alternate||
       !options(memory,state.image,item.behaviour,window,available,selected,error)||
       selected!=json(std::array<int32_t,2>{int32_t(slot),int32_t(type)}))
        return fail(error,error.empty()?"NPC native UI active cursor or ThemeWindow selection differs":error);
    npc_census::State before,hover;undo::UndoWitness undo_before,undo_hover;
    action::NpcState before_state,hover_state;action::Ledger before_money,hover_money;
    if(!read_effect(memory,state.image,region,0,before_state,before_money,undo_before,before,error,tool=="QuestGiver")||
       undo_before.active_graph)return fail(error,error.empty()?"NPC native UI requires idle Undo":error);
    state.ui_jobs.emplace(token,result);auto& job=state.ui_jobs.at(token);
    job["attempted"]=true;job["requested_position"]=point;result=job;
    bool warped=false;const auto warp_seh=warp_cursor(state.image,cursor,point,warped);
    if(warp_seh||!warped){job["cause"]="NPC original Cursor Warp failed seh="+
        std::to_string(warp_seh)+" returned="+std::to_string(warped);
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    const float* hit_pointer=nullptr;
    const auto calculate_seh=calculate_cursor_hit(state.image,cursor,hit_pointer);
    std::array<float,3> hit{};uint8_t calculated_hover=0;
    if(calculate_seh||uintptr_t(hit_pointer)!=cursor+0x1e4||
       !memory.copy(uintptr_t(hit_pointer),hit.data(),sizeof hit,error)||
       !field(memory,cursor,0x189,calculated_hover,error)){
        job["cause"]=error.empty()?"NPC original Cursor hit differs calculate_seh="+
            std::to_string(calculate_seh)+" hit_pointer="+std::to_string(uintptr_t(hit_pointer))+
            " expected="+std::to_string(cursor+0x1e4):error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    const bool finite_hit=std::all_of(hit.begin(),hit.end(),[](float value){
        return std::isfinite(value)&&std::abs(value)<=1000000.0f;});
    job["cursor_hit"]={{"hovered",calculated_hover},{"finite",finite_hit}};
    if(finite_hit)job["cursor_hit"]["position"]=hit;
    if(!calculated_hover||!finite_hit||
       !point_and_cursor(memory,state.image,uintptr_t(world.origin.map),world.root,
           region,cursor,item.behaviour,hit,error)){
        job["cause"]=error.empty()?"NPC original Cursor ray missed target Region hovered="+
            std::to_string(calculated_hover)+" finite="+std::to_string(finite_hit):error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    const auto hover_seh=npc_input(state.image,item.behaviour,cursor,hit,false);
    if(hover_seh){job["cause"]="NPC native UI MouseOver SEH="+std::to_string(hover_seh);job["unknown"]=true;
        result=job;return fail(error,job.at("cause").get<std::string>());}
    if(!field(memory,item.behaviour,0x358,preview,error)||!preview||
       !read_effect(memory,state.image,region,0,hover_state,hover_money,undo_hover,hover,error,tool=="QuestGiver")||
       before.rows!=hover.rows||before.next_local_tag!=hover.next_local_tag||
       before_money!=hover_money||undo_before!=undo_hover){
        job["cause"]=error.empty()?"NPC native UI MouseOver changed world, roster, Ledger or Undo":error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    int32_t pressed=1;uint8_t released=1,hovered=0,enabled=0,behavior_active=0;
    bool modal=true;const auto seh_modal=modal_window(state.image,modal);
    if(!field(memory,cursor,0x238,pressed,error)||!field(memory,cursor,0x23c,released,error)||
       !field(memory,cursor,0x189,hovered,error)||!field(memory,cursor,0x256,enabled,error)||
       !field(memory,item.behaviour,0x121,behavior_active,error)||
       seh_modal||pressed>0||released||(!hovered&&!behavior_active)||modal||!enabled){
        job["cause"]=error.empty()?"NPC original Cursor Release gate differs pressed="+
            std::to_string(pressed)+" released="+std::to_string(released)+
            " hovered="+std::to_string(hovered)+" behavior="+std::to_string(behavior_active)+
            " modal="+std::to_string(modal)+" enabled="+std::to_string(enabled)+
            " modal_seh="+std::to_string(seh_modal):error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    float sx=0,sy=0;SDL_GetMouseState(&sx,&sy);
    const std::array<float,2> screen{sx,sy};const auto captures=state.captures;
    const auto rejections=state.rejections;
    state.diagnostic_token=token;
    const auto release_seh=generic_release(state.image,cursor,screen,hit);
    state.diagnostic_token.clear();
    if(release_seh){state.pending.reset();state.capture_failed=true;
        job["cause"]="NPC native Cursor Release SEH="+std::to_string(release_seh);
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    job["returned"]=true;
    if(state.captures!=captures+1||!state.pending||!job.value("captured",false)){
        state.pending.reset();state.capture_failed=true;
        job["hook_rejected"]=state.rejections!=rejections;
        job["cause"]=state.rejections!=rejections?
            "NPC native Cursor Release hook rejected: "+state.cause:
            "NPC native Cursor Release returned without reaching exact hook capture";
        job["unknown"]=true;
        result=job;return fail(error,job.at("cause").get<std::string>());}
    action::NpcState after_state;action::Ledger after_money;undo::UndoWitness undo_after;npc_census::State after;
    if(!read_effect(memory,state.image,region,0,after_state,after_money,undo_after,after,error,tool=="QuestGiver")||
       after_state!=hover_state||after.rows!=hover.rows||after.next_local_tag!=hover.next_local_tag||
       after_money!=hover_money||undo_after!=undo_hover){
        state.pending.reset();state.capture_failed=true;
        job["cause"]=error.empty()?"NPC native captured Release changed world, NPC, Ledger or Undo":error;
        job["unknown"]=true;result=job;return fail(error,job.at("cause").get<std::string>());}
    result=job;error.clear();return true;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,
                        const action::Evidence* authority,std::string& error) {
    if(request.action==action::Action::RemoveNPC)return remove_perform(binding,request,authority,error);
    if(request.action==action::Action::MoveNPC)return move_perform(binding,request,authority,error);
    if(!state.ready||GetCurrentThreadId()!=state.thread||request.action!=action::Action::PlaceNPC||
       !action::valid_npc(request.npc)){
        error="NPC action runtime, Mainthread or typed intent invalid";return action::Outcome::RejectedNoEffect;}
    if(state.operations.contains(request.action_uuid)){
        error="NPC action already attempted; no replay";return action::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("action_uuid",std::string{})!=request.action_uuid){
        error="NPC action lacks exact pending Controller request";return action::Outcome::RejectedNoEffect;}
    Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return action::Outcome::RejectedNoEffect;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world,after_world;uintptr_t region=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e){
        return field(memory,base,offset,value,e);};
    if(!reader.current(world,true,error)||!bound(world,binding,request.snapshot_uuid,error)||
       !region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,
           request.npc.region,0,region,read_field,error))return action::Outcome::RejectedNoEffect;
    uintptr_t cursor=0,actual_region=0,from_shard=0;std::map<std::string,Tool> inventory;
    if(!field(memory,state.image,0x13df9d0,cursor,error)||!cursor||
       !tools(memory,state.image,inventory,error))return action::Outcome::RejectedNoEffect;
    const auto name=request.npc.type==0?"QuestGiver":request.npc.type==1?"Guard":"ClassTrainer";
    const auto& item=inventory.at(name);
    const auto map_seh=map_region(state.image,uintptr_t(world.origin.map),request.npc.position,actual_region);
    const auto shard_seh=shard_region(state.image,world.root,request.npc.position,from_shard);
    if(item.count!=1||item.native_type!=request.npc.type||!item.behaviour){
        error="NPC Host pre-press tool differs count="+std::to_string(item.count)+
            " native_type="+std::to_string(item.native_type)+
            " behavior_present="+std::to_string(bool(item.behaviour));
        return action::Outcome::RejectedNoEffect;
    }
    if(map_seh||shard_seh||actual_region!=region||from_shard!=region){
        error="NPC Host pre-press point Region differs map_seh="+std::to_string(map_seh)+
            " shard_seh="+std::to_string(shard_seh)+
            " map_match="+std::to_string(actual_region==region)+
            " shard_match="+std::to_string(from_shard==region);
        return action::Outcome::RejectedNoEffect;
    }
    npc_census::State original,hover,after;undo::UndoWitness undo_original,undo_hover,undo_after;
    action::NpcState original_state,hover_state,after_state;action::Ledger original_money,hover_money,after_money;
    if(!read_effect(memory,state.image,region,0,original_state,original_money,undo_original,original,error,
                    request.npc.type==0)||
       undo_original.active_graph||original_state.count>=100000||original_state.undo_count>=100000){
        if(error.empty())error="NPC action requires idle Undo and available census/Codec count";
        return action::Outcome::RejectedNoEffect;}
    if(authority&&(original_state.count!=authority->npc_before.count||
        original_state.roster_sha256!=authority->npc_before.roster_sha256||
        original_state.undo_count!=authority->npc_before.undo_count||
        original_money!=authority->before)){
        error="NPC Client before roster/Undo/full Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    auto& op=state.operations.emplace(request.action_uuid,Operation{binding,request}).first->second;
    auto unknown=[&](const std::string& cause){error=op.cause=cause;return action::Outcome::Unknown;};
    uintptr_t active=0;if(!field(memory,cursor,0x190,active,error))return unknown(error);
    if(active!=item.behaviour){
        placement_ui_probe::Request ui;ui.family=placement_ui_probe::Family::NPC;
        ui.type=name;ui.variant="npc";placement_ui_probe::Binding existing;
        placement_ui_probe::Context context{state.image,uintptr_t(world.origin.state),
            world.origin.world_epoch,world.origin.generation,state.thread};
        placement_ui_probe::ProcessMemory ui_memory;
        if(!placement_ui_probe::read_existing_tool(ui_memory,context,ui,existing,error)||
           existing.behaviour!=item.behaviour)return unknown(error.empty()?"NPC original ItemButton owner differs":error);
        op.attempted|=1;const auto seh=original_press(state.image,existing.button);
        if(seh)return unknown("NPC original ItemButton Press SEH="+std::to_string(seh));op.returned|=1;
    }
    uint8_t alternate=1;
    if(!field(memory,item.behaviour,0x3e1,alternate,error))return unknown(error);
    if(alternate)return unknown("NPC original active tool alternate Release mode="+std::to_string(alternate));
    if(!point_and_cursor(memory,state.image,uintptr_t(world.origin.map),world.root,
        region,cursor,item.behaviour,request.npc.position,error))return unknown(error);
    uintptr_t window=0,preview=0,selected_ptr=0;std::vector<Option> available;json selected=nullptr;
    if(!options(memory,state.image,item.behaviour,window,available,selected,error))return unknown("NPC original ThemeWindow options changed: "+error);
    const auto choice=std::find_if(available.begin(),available.end(),[&](const Option& option){
        return option.id==std::array<int32_t,2>{request.npc.character_slot,request.npc.character_type};});
    if(choice==available.end())return unknown("NPC requested CharacterTypeID absent from original ThemeList");
    if(selected!=json(choice->id)){
        op.attempted|=8;const auto seh=original_select(state.image,window,choice->character);
        if(seh)return unknown("NPC original Select SEH="+std::to_string(seh));op.returned|=8;
    }
    op.attempted|=2;state.replaying=true;
    const auto hover_seh=npc_input(state.image,item.behaviour,cursor,request.npc.position,false);
    state.replaying=false;
    if(hover_seh)return unknown("NPC original MouseOver SEH="+std::to_string(hover_seh));op.returned|=2;
    if(!field(memory,item.behaviour,0x358,preview,error)||!preview||
       !field(memory,item.behaviour,0x360,selected_ptr,error)||selected_ptr!=choice->character||
       !read_effect(memory,state.image,region,0,hover_state,hover_money,undo_hover,hover,error,
                    request.npc.type==0)||
       !reader.current(after_world,true,error)||!same_world(world,after_world)||
       original.rows!=hover.rows||original.next_local_tag!=hover.next_local_tag||
       original_state.roster_sha256!=hover_state.roster_sha256||
       original_state.quest_targets_sha256!=hover_state.quest_targets_sha256||
       original_money!=hover_money||undo_original!=undo_hover)
        return unknown(error.empty()?"NPC original MouseOver changed world, roster, Ledger or Undo":error);
    const auto transform_seh=transform(state.image,item.behaviour,request.npc);
    if(transform_seh)return unknown("NPC original Transform constructor SEH="+std::to_string(transform_seh));
    std::array<float,4> quaternion{};std::array<float,3> position{},scale{};uint8_t dirty=0;
    if(!field(memory,item.behaviour,0x368,quaternion,error)||
       !field(memory,item.behaviour,0x378,position,error)||
       !field(memory,item.behaviour,0x384,scale,error)||
       !field(memory,item.behaviour,0x390,dirty,error)||
       quaternion!=request.npc.rotation||position!=request.npc.position||
       scale!=request.npc.scale||!dirty)return unknown(error.empty()?"NPC original Transform readback differs":error);
    npc_census::State pre_release;undo::UndoWitness pre_release_undo;action::Ledger pre_release_money;
    if(!read_effect(memory,state.image,region,0,hover_state,pre_release_money,pre_release_undo,pre_release,error,
                    request.npc.type==0,&op.quest_tasks_before)||
       pre_release.rows!=hover.rows||pre_release.next_local_tag!=hover.next_local_tag||
       pre_release_money!=original_money||pre_release_undo!=undo_hover)
        return unknown(error.empty()?"NPC original Transform changed NPC, Ledger or Undo before Release":error);
    if(authority){const auto seh=seed(state.image,authority->npc_before.global_random);
        if(seh)return unknown("NPC Client global RNG adoption SEH="+std::to_string(seh));
        if(!read_effect(memory,state.image,region,0,hover_state,hover_money,pre_release_undo,pre_release,error,
                        request.npc.type==0)||
           hover_state!=authority->npc_before||hover_money!=authority->before||
           pre_release_undo!=undo_hover)
            return unknown(error.empty()?"NPC Client post-hover Host RNG/roster differs":error);}
    op.evidence.before=hover_money;op.evidence.npc_before=hover_state;
    op.attempted|=4;state.replaying=true;
    const auto release_seh=npc_input(state.image,item.behaviour,cursor,request.npc.position,true);
    state.replaying=false;
    if(release_seh)return unknown("NPC original Release SEH="+std::to_string(release_seh));op.returned|=4;
    if(!read_effect(memory,state.image,region,0,after_state,after_money,undo_after,after,error,
                    request.npc.type==0)||
       !reader.current(after_world,false,error)||!host::same_world(world.origin,after_world.origin)||
       world.root!=after_world.root||after_world.saves||after_world.edits||after_world.active_graph)
        return unknown(error.empty()?"NPC original Release world/readback differs":error);
    npc_census::Row created;
    if(!npc_census::added_one(hover,after,created,error))return unknown(error);
    uintptr_t registered=0;const auto lookup_seh=lookup(api.find,created.local_uid,registered);
    if(lookup_seh||registered!=created.local_pointer)return unknown("NPC created UID table differs SEH="+std::to_string(lookup_seh));
    if(undo_after.undo.empty())return unknown("NPC original committed Undo graph absent");
    undo::GraphWitness graph;
    if(!undo::read_graph(memory.process,state.image,undo_after.undo.back(),graph,error)||
       !undo::validate_undo_commit(undo_hover,undo_after,graph,error)||
       !graph.locked||graph.operations!=2||graph.nodes!=0||graph.pending_delete)
        return unknown(error.empty()?"NPC original Undo graph differs":error);
    action::NpcState verified_after;action::Ledger verified_money;
    undo::UndoWitness verified_undo;npc_census::State verified_census;
    if(!read_effect(memory,state.image,region,created.local_uid,verified_after,verified_money,
                    verified_undo,verified_census,error,request.npc.type==0,&op.quest_tasks_after))return unknown(error);
    if(verified_after.count!=after_state.count||verified_after.undo_count!=after_state.undo_count||
       verified_after.roster_sha256!=after_state.roster_sha256||
       verified_after.global_random!=after_state.global_random||
       verified_after.quest_targets_sha256!=after_state.quest_targets_sha256||
       verified_money!=after_money||verified_undo!=undo_after||verified_census.rows!=after.rows)
        return unknown("NPC Quest or native effect changed during original post-Release readback");
    after_state=std::move(verified_after);
    op.local_uid=created.local_uid;op.local_pointer=created.local_pointer;
    op.evidence.after=after_money;op.evidence.npc_after=after_state;op.evidence.undo_confirmed=true;
    if(op.attempted!=op.returned||!action::valid_npc_effect(request,op.evidence)||
       (authority&&(op.evidence.npc_before!=authority->npc_before||
                    op.evidence.npc_after!=authority->npc_after||
                    op.evidence.before!=authority->before||op.evidence.after!=authority->after)))
        return unknown("NPC original result, Host actor/RNG or full Ledger differs");
    op.confirmed=true;error.clear();return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error) {
    if(request.action==action::Action::RemoveNPC)return remove_observe(binding,request,out,error);
    if(request.action==action::Action::MoveNPC)return move_observe(binding,request,out,error);
    const auto found=state.operations.find(request.action_uuid);
    if(found==state.operations.end()||found->second.binding!=binding||
       found->second.request!=request||!found->second.confirmed)
        return fail(error,"NPC observation lacks exact confirmed action");
    const auto& op=found->second;Memory memory;host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world;uintptr_t region=0;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    if(!reader.current(world,true,error)||!bound(world,binding,request.snapshot_uuid,error)||
       !region_saved_identity::resolve(state.image,uintptr_t(world.origin.map),world.root,
           request.npc.region,0,region,read_field,error))return false;
    npc_census::State census;undo::UndoWitness witness;action::Ledger ledger;action::NpcState current;
    json current_tasks=json::array();
    if(!read_effect(memory,state.image,region,op.local_uid,current,ledger,witness,census,error,
                    request.npc.type==0,&current_tasks))return false;
    uintptr_t registered=0;const auto seh=lookup(api.find,op.local_uid,registered);
    if(seh||registered!=op.local_pointer)
        return fail(error,"NPC local UID membership changed SEH="+std::to_string(seh));
    if(current!=op.evidence.npc_after||ledger!=op.evidence.after)
        return fail(error,request.npc.type==0?
            "NPC Quest post-commit roster, task-dependent records, RNG or full Ledger changed task_phase_changed="+
                std::to_string(current_tasks!=op.quest_tasks_after):
            "NPC canonical actor, RNG or full Ledger changed");
    out=op.evidence;out.retirement_confirmed=op.retired;error.clear();return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& error) {
    if(request.action==action::Action::RemoveNPC)return remove_retire(binding,request,error);
    if(request.action==action::Action::MoveNPC)return move_retire(binding,request,error);
    action::Evidence observed;if(!observe(binding,request,observed,error))return false;
    auto& op=state.operations.at(request.action_uuid);
    if(op.retired)return fail(error,"NPC action already retired");
    op.retired=true;error.clear();return true;
}
#include "npc_remove_runtime.inc"
#include "npc_move_runtime.inc"
bool bug_parent(const action::Context& context,const action::Binding& binding,const std::string& key,uintptr_t& object,std::string& error){
    object=0;ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
    if(!target_binding(npc_target_key::argument(key),binding,context.snapshot_uuid,local,region,census,error))return false;
    object=local.object;return true;
}
}

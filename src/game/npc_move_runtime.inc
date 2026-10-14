namespace {
using LocalNpcMemory=Memory;
struct NativeMoveReader final:move_kiss::Memory {
    LocalNpcMemory& source;
    explicit NativeMoveReader(LocalNpcMemory& value):source(value){}
    bool read(uintptr_t address,void* output,size_t bytes,std::string& error)override {
        return source.copy(address,output,bytes,error);
    }
};
action::BuildingState move_state(const npc_census::Row& row) {
    action::BuildingState value;value.alive=true;value.x=row.position[0];
    value.y=row.position[1];value.z=row.position[2];value.rotation=row.rotation;
    value.scale=row.scale;return value;
}
bool move_row(const npc_census::State& census,uint64_t uid,npc_census::Row& out,std::string& error) {
    bool found=false;
    for(const auto& row:census.rows)if(row.local_uid==uid) {
        if(found)return fail(error,"NPC Move UID occurs twice in current Manager");
        out=row;found=true;
    }
    return found||fail(error,"NPC Move UID absent from current Manager");
}
bool move_target(const std::string& target,const action::Binding& binding,
                 const std::string& snapshot,ui::LocalBinding& local,
                 uintptr_t& region,npc_census::State& census,npc_census::Row& row,
                 std::string& error) {
    if(!target_binding(target,binding,snapshot,local,region,census,error)||
       !move_row(census,local.uid,row,error)||row.local_pointer!=local.object)
        return fail(error,error.empty()?"NPC Move fresh Place binding or Manager row differs":error);
    return true;
}
bool move_unchanged_other_rows(const npc_census::State& before,const npc_census::State& after,
                               uint64_t uid,std::string& error) {
    if(before.manager!=after.manager||before.next_local_tag!=after.next_local_tag||
       before.rows.size()!=after.rows.size())
        return fail(error,"NPC Move Manager identity, tag or count differs");
    for(const auto& old:before.rows) {
        npc_census::Row now;
        if(!move_row(after,old.local_uid,now,error))return false;
        if(old.local_uid!=uid&&!npc_census::same_semantic(old,now))
            return fail(error,"NPC Move changed another Manager actor");
        if(old.local_uid==uid&&(old.local_pointer!=now.local_pointer||
            old.local_tag!=now.local_tag||old.type!=now.type||
            old.character_slot!=now.character_slot||old.character_type!=now.character_type||
            old.level!=now.level||old.quest_count!=now.quest_count||old.name!=now.name))
            return fail(error,"NPC Move changed target identity or character fields");
    }
    return true;
}
bool move_spawn(Memory& memory,uintptr_t image,uintptr_t object,uintptr_t region,
                const char* phase,json& out,std::string& error) {
    uintptr_t ap=0,actor_pointer=0,marker=0,owner=0;uint8_t follow=1;
    std::array<float,3> point{};
    if(!field(memory,object,0,ap,error)||!field(memory,object,0x6f0,actor_pointer,error)||
       !field(memory,object,0x810,marker,error)||!field(memory,object,0x818,point,error)||
       !field(memory,object,0x178,owner,error)||!field(memory,object,0x490,follow,error))return false;
    uintptr_t notify=0;
    if(ap!=image+0x1742980||!actor_pointer||owner!=region||follow!=0||
       !field(memory,ap,0x2a0,notify,error)||notify!=image+0x3c2a40)
        return fail(error,"NPC Move NotifyMoved class, Actor or callback differs");
    npc_move_visibility::Snapshot visible;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){
        return field(memory,base,offset,value,cause);};
    if(!npc_move_visibility::read_spawn(image,object,read_field,visible,phase,error))return false;
    out={{"position",point},{"actor_present",true},{"marker_present",bool(marker)},
         {"owner_present",bool(owner)},{"follow_region",follow},
         {"on_map",visible.on_map},{"actor_visible",visible.actor_visible}};
    return true;
}
bool move_inverse(Memory& memory,uintptr_t image,uintptr_t graph,uint64_t uid,
                  const npc_census::Row& before,std::string& error) {
    undo::GraphWitness witness;
    if(!undo::read_graph(memory.process,image,graph,witness,error))return false;
    if(!witness.locked||witness.operations<1||witness.pending_delete)
        return fail(error,"NPC Move inverse Undo graph state differs locked="+
            std::to_string(witness.locked)+" operations="+
            std::to_string(witness.operations)+" pending_delete="+
            std::to_string(witness.pending_delete));
    uintptr_t data=0;if(!field(memory,graph,0x20,data,error))return false;
    int matches=0;
    for(int32_t index=0;index<witness.operations;++index) {
        uintptr_t operation=0,ap=0;uint64_t actual_uid=0;
        if(!field(memory,data,size_t(index)*8,operation,error)||
           !field(memory,operation,0,ap,error))return false;
        if(ap!=image+0x1786ff0)continue;
        if(!field(memory,operation,0xb8,actual_uid,error))return false;
        if(actual_uid!=uid)continue;
        std::array<float,4> rotation{};std::array<float,3> position{},scale{};
        if(!field(memory,operation,0xc0,rotation,error)||
           !field(memory,operation,0xd0,position,error)||
           !field(memory,operation,0xdc,scale,error))return false;
        if(rotation==before.rotation&&position==before.position&&scale==before.scale)++matches;
    }
    return matches==1||fail(error,"NPC Move inverse Undo action not unique or old pose differs");
}
bool move_selected(Memory& memory,uintptr_t image,uintptr_t selection,uintptr_t object,
                   std::string& error) {
    undo::SelectionWitness witness;undo::SelectionCounts counts;
    if(!undo::read_selection(memory.process,image,selection,witness,counts,error))return false;
    const auto& rows=witness.lists[1];
    return std::find(rows.begin(),rows.end(),object)!=rows.end()||
        fail(error,"NPC Move original selection lacks target");
}
uint32_t move_input(uintptr_t image,uintptr_t behaviour,uintptr_t cursor,
                    const std::array<float,3>& point,uintptr_t rva) noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*,void*,const float*)>(image+rva)(
        reinterpret_cast<void*>(behaviour),reinterpret_cast<void*>(cursor),point.data());return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t move_set_on_map(uintptr_t image,uintptr_t object) noexcept {
    __try {reinterpret_cast<void(__fastcall*)(void*,bool)>(image+0x3c2a00)(
        reinterpret_cast<void*>(object),true);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool move_capture_binding(const std::string& snapshot,uint64_t epoch,uint64_t generation,
                          uintptr_t object,uint64_t uid,action::BuildingState& before,
                          std::string& key,std::string& error) {
    before={};key.clear();if(!state.ready||GetCurrentThreadId()!=state.thread)
        return fail(error,"NPC Move capture requires ready Mainthread");
    action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,error))return false;
    if(binding.native_epoch!=epoch||binding.native_generation!=generation)
        return fail(error,"NPC Move capture World binding differs epoch="+
            std::to_string(binding.native_epoch)+" required="+std::to_string(epoch)+
            " generation="+std::to_string(binding.native_generation)+
            " required="+std::to_string(generation));
    std::string target;if(!local_target_key(object,uid,target,error))return false;
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;npc_census::Row row;
    if(!move_target(target,binding,snapshot,local,region,census,row,error)||
       local.object!=object||local.uid!=uid)
        return fail(error,error.empty()?"NPC Move captured target differs from current Place binding":error);
    before=move_state(row);key=npc_target_key::canonical(target);error.clear();return true;
}
bool submit_restored_move(const move_kiss::Intent& intent,const std::string& action_uuid,
                          std::string& error) {
    if(!state.ready||GetCurrentThreadId()!=state.thread||
       !npc_move_visibility::target(intent.before,intent.object_key)||
       !npc_target_key::valid(intent.object_key))
        return fail(error,"NPC Move visibility restore requires bound captured NPC intent");
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,error)||
       snapshot!=intent.origin.snapshot_uuid||
       binding.native_epoch!=intent.origin.world_epoch||
       binding.native_generation!=intent.origin.generation)
        return fail(error,error.empty()?"NPC Move visibility restore World binding differs":error);
    const auto target=npc_target_key::argument(intent.object_key);
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;npc_census::Row row;
    if(!move_target(target,binding,snapshot,local,region,census,row,error)||
       local.object!=intent.target||local.uid!=intent.local_uid||
       move_state(row)!=intent.before)
        return fail(error,error.empty()?"NPC Move visibility restore fresh target differs":error);
    Memory memory;npc_move_visibility::Receipt receipt;
    const auto read_field=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){
        return field(memory,base,offset,value,cause);};
    const auto invoke=[&](uintptr_t object,std::string& cause){
        const auto seh=move_set_on_map(state.image,object);
        return !seh||fail(cause,"NPC Move original SetOnMap SEH="+std::to_string(seh));};
    bool visibility_restored=false;
    const auto submit=[&](std::string& cause){
        visibility_restored=true;
        ui::LocalBinding current;uintptr_t current_region=0;
        npc_census::State current_census;npc_census::Row current_row;
        if(!move_target(target,binding,snapshot,current,current_region,current_census,current_row,cause)||
           current.object!=local.object||current.uid!=local.uid||current_region!=region||
           current_census.rows!=census.rows)
            return fail(cause,cause.empty()?"NPC Move restored Actor but fresh target changed":cause);
        return typed_action_runtime::submit_move_npc(action_uuid,target,intent.after,cause);
    };
    const auto submitted=npc_move_visibility::restore_then_submit(state.image,local.object,
        read_field,invoke,submit,receipt,error);
    if(!state.move_ui_token.empty()){
        const auto found=state.ui_jobs.find(state.move_ui_token);
        if(found!=state.ui_jobs.end())found->second["visibility_restore"]={
            {"before_on_map",receipt.before.on_map},
            {"before_actor_visible",receipt.before.actor_visible},
            {"original_calls",receipt.original_calls},
            {"after_on_map",receipt.after.on_map},
            {"after_actor_visible",receipt.after.actor_visible},
            {"confirmed",visibility_restored}};
    }
    return submitted;
}
bool move_captured(const move_kiss::Intent& intent,const std::string& action_uuid,
                   std::string& error) {
    if(!npc_target_key::valid(intent.object_key)||
       state.pending||state.capture_failed)return fail(error,"NPC Move capture key or pending state invalid");
    if(!state.move_ui_token.empty()) {
        auto found=state.ui_jobs.find(state.move_ui_token);
        if(found==state.ui_jobs.end()||found->second.value("operation",std::string{})!="move"||
           found->second.value("canonical_record_key",std::string{})!=intent.object_key||
           found->second.value("captured",false))
            return fail(error,"NPC Move diagnostic target or capture count differs");
        found->second["captured"]=true;found->second["action_uuid"]=action_uuid;
        found->second["intent"]={{"position",{intent.after.x,intent.after.y,intent.after.z}},
            {"rotation",intent.after.rotation},{"scale",intent.after.scale}};
    }
    ++state.captures;error.clear();return true;
}
void move_submitted(const std::string& action_uuid) {
    ++state.submits;state.last_action=action_uuid;
    if(state.move_ui_token.empty())return;
    auto found=state.ui_jobs.find(state.move_ui_token);
    if(found!=state.ui_jobs.end()&&found->second.value("action_uuid",std::string{})==action_uuid)
        found->second["submitted"]=true;
    state.move_ui_token.clear();
}
void move_failed(const std::string& cause) {
    if(state.move_ui_token.empty())return;
    const auto found=state.ui_jobs.find(state.move_ui_token);
    if(found==state.ui_jobs.end())return;
    found->second["cause"]=cause;found->second["unknown"]=true;
}
action::Outcome move_perform(const action::Binding& binding,const action::Request& request,
                             const action::Evidence* authority,std::string& error) {
    if(!state.ready||GetCurrentThreadId()!=state.thread||
       request.action!=action::Action::MoveNPC||!npc_target_key::request(request.canonical_record_key,request.target_action_uuid))
        return fail(error,"NPC Move runtime or typed target invalid"),action::Outcome::RejectedNoEffect;
    if(state.operations.contains(request.action_uuid))
        return fail(error,"NPC Move action already attempted; no replay"),action::Outcome::Unknown;
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid)
        return fail(error,"NPC Move lacks exact pending Controller request"),action::Outcome::RejectedNoEffect;
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;npc_census::Row old;
    if(!move_target(npc_target_key::argument(request.canonical_record_key),binding,request.snapshot_uuid,
                    local,region,census,old,error))return action::Outcome::RejectedNoEffect;
    Memory memory;NativeMoveReader move_memory(memory);move_kiss::Calls calls;
    if(!move_kiss::resolve_calls(move_memory,state.image,exe_sha,calls,error))
        return action::Outcome::RejectedNoEffect;
    std::array<unsigned char,0x6c> pose{};
    if(!move_kiss::npc_transform(request.building,pose,error))
        return action::Outcome::RejectedNoEffect;
    action::NpcState before;action::Ledger money;undo::UndoWitness undo_before;
    json before_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,before,money,undo_before,census,error,
                    old.type==0,old.type==0?&before_tasks:nullptr)||
       undo_before.active_graph||before.undo_count>=100000)
        return fail(error,error.empty()?"NPC Move requires idle Undo and valid count":error),
            action::Outcome::RejectedNoEffect;
    if(authority&&(before.count!=authority->npc_before.count||
        before.roster_sha256!=authority->npc_before.roster_sha256||
        before.undo_count!=authority->npc_before.undo_count||
        before.quest_targets_sha256!=authority->npc_before.quest_targets_sha256||
        before.created!=authority->npc_before.created||money!=authority->before))
        return fail(error,"NPC Move Client before Quest pool, roster, Actor, Undo or Ledger differs"),
            action::Outcome::RejectedNoEffect;
    host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))
        return action::Outcome::RejectedNoEffect;
    host::NativeOrigin origin;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    host::WorldState world,after_world;
    if(!reader.current(world,true,error)||!bound(world,binding,request.snapshot_uuid,error))
        return action::Outcome::RejectedNoEffect;
    if(world.edits||world.active_graph)
        return fail(error,"NPC Move requires idle native edit state edits="+
            std::to_string(world.edits)+" active_graph="+
            std::to_string(world.active_graph)),action::Outcome::RejectedNoEffect;
    json spawn_before;
    if(!move_spawn(memory,state.image,local.object,region,"before",spawn_before,error))
        return action::Outcome::RejectedNoEffect;
    uintptr_t actor_before=0,marker_before=0;
    if(!field(memory,local.object,0x6f0,actor_before,error)||
       !field(memory,local.object,0x810,marker_before,error))
        return action::Outcome::RejectedNoEffect;
    if(old.type==0&&!marker_before)
        return fail(error,"NPC Move QuestGiver original marker absent before Move"),
            action::Outcome::RejectedNoEffect;
    auto& op=state.operations.emplace(request.action_uuid,Operation{binding,request}).first->second;
    op.target_action_uuid=request.target_action_uuid;op.local_uid=local.uid;
    op.local_pointer=local.object;op.spawn_before=spawn_before;
    op.quest_tasks_before=before_tasks;
    auto unknown=[&](const std::string& cause){error=op.cause=cause;return action::Outcome::Unknown;};
    if(authority){const auto seh=seed(state.image,authority->npc_before.global_random);
        if(seh)return unknown("NPC Move Host RNG adoption SEH="+std::to_string(seh));
        if(!read_effect(memory,state.image,region,local.uid,before,money,undo_before,census,error,
                        old.type==0,old.type==0?&op.quest_tasks_before:nullptr)||
           before!=authority->npc_before||money!=authority->before)
            return unknown(error.empty()?"NPC Move Host before RNG/Actor differs":error);}
    op.evidence.before=money;op.evidence.npc_before=before;
    op.attempted|=1;move_kiss::Receipt receipt;
    const auto moved=move_kiss::execute_once(calls,local.object,pose,receipt,error);
    op.move_original_calls=receipt.move_attempted?1:0;
    if(!moved)return unknown(error.empty()?"NPC Move original DoMove did not return":error);
    op.returned|=1;
    npc_census::State after_census;action::NpcState after;action::Ledger after_money;
    undo::UndoWitness undo_after;npc_census::Row current;
    if(!read_effect(memory,state.image,region,local.uid,after,after_money,undo_after,after_census,error,
                    old.type==0,old.type==0?&op.quest_tasks_after:nullptr)||
       !move_row(after_census,local.uid,current,error)||
       !move_unchanged_other_rows(census,after_census,local.uid,error)||
       !reader.current(after_world,false,error)||
       !host::same_world(world.origin,after_world.origin)||
       world.root!=after_world.root||after_world.saves||after_world.edits||after_world.active_graph)
        return unknown(error.empty()?"NPC Move original Manager, World or actor readback differs":error);
    uintptr_t registered=0;const auto seh=lookup(api.find,local.uid,registered);
    if(seh||registered!=local.object||current.local_pointer!=local.object)
        return unknown("NPC Move original local UID lookup or Manager pointer differs SEH="+
            std::to_string(seh));
    json spawn_after;uintptr_t actor_after=0,marker_after=0;
    if(!move_spawn(memory,state.image,local.object,region,"after",spawn_after,error)||
       !field(memory,local.object,0x6f0,actor_after,error)||
       !field(memory,local.object,0x810,marker_after,error))return unknown(error);
    op.spawn_after=spawn_after;
    const auto target=std::array<float,3>{request.building.x,request.building.y,request.building.z};
    if(current.position!=target||current.rotation!=request.building.rotation||
       current.scale!=request.building.scale||
       spawn_after.at("position")!=json(target)||
       actor_before!=actor_after||marker_before!=marker_after||
       spawn_before.at("follow_region")!=spawn_after.at("follow_region"))
        return unknown("NPC Move pose, SpawnPoint, Actor binding or marker differs");
    op.spawn_confirmed=true;op.actor_confirmed=true;
    if(undo_after.undo.empty())return unknown("NPC Move original committed Undo graph absent");
    undo::GraphWitness graph;
    if(!undo::read_graph(memory.process,state.image,undo_after.undo.back(),graph,error)||
       !undo::validate_undo_commit(undo_before,undo_after,graph,error)||
       !move_inverse(memory,state.image,receipt.graph,local.uid,old,error))
        return unknown(error.empty()?"NPC Move inverse Undo graph differs":error);
    op.undo_confirmed=true;
    if(!move_selected(memory,state.image,after_world.selection,local.object,error))return unknown(error);
    if(old.type==0&&!quest_bridge::move_quest_stable(before,after,
            op.quest_tasks_before,op.quest_tasks_after,error))return unknown(error);
    op.evidence.after=after_money;op.evidence.npc_after=after;
    op.evidence.undo_confirmed=true;op.evidence.selection_confirmed=true;
    if(op.attempted!=op.returned||op.move_original_calls!=1||
       !action::valid_npc_effect(request,op.evidence)||
       (authority&&(op.evidence.npc_before!=authority->npc_before||
                    op.evidence.npc_after!=authority->npc_after||
                    op.evidence.before!=authority->before||op.evidence.after!=authority->after)))
        return unknown("NPC Move Host Actor, Commit RNG or full Ledger differs");
    op.confirmed=true;error.clear();return action::Outcome::Confirmed;
}
bool move_observe(const action::Binding& binding,const action::Request& request,
                  action::Evidence& out,std::string& error) {
    const auto found=state.operations.find(request.action_uuid);
    if(found==state.operations.end()||found->second.request!=request||
       found->second.binding!=binding||!found->second.confirmed||
       found->second.move_original_calls!=1||!found->second.spawn_confirmed||
       !found->second.actor_confirmed||!found->second.undo_confirmed)
        return fail(error,"NPC Move observation lacks confirmed OriginalOnce operation");
    const auto& op=found->second;ui::LocalBinding local;uintptr_t region=0;
    npc_census::State census;npc_census::Row row;
    if(!move_target(npc_target_key::argument(request.canonical_record_key),binding,request.snapshot_uuid,
                    local,region,census,row,error))return false;
    if(local.uid!=op.local_uid||local.object!=op.local_pointer)
        return fail(error,"NPC Move observation fresh UID/Object binding differs uid="+
            std::to_string(local.uid)+" required="+std::to_string(op.local_uid)+
            " object_match="+std::to_string(local.object==op.local_pointer));
    Memory memory;action::NpcState current;action::Ledger money;undo::UndoWitness witness;
    if(!read_effect(memory,state.image,region,local.uid,current,money,witness,census,error,
                    row.type==0)||
       current!=op.evidence.npc_after||money!=op.evidence.after||
       !move_selected(memory,state.image,local.manager,local.object,error))
        return fail(error,error.empty()?"NPC Move current Actor, Undo, selection or Ledger differs":error);
    out=op.evidence;out.retirement_confirmed=op.retired;error.clear();return true;
}
bool move_retire(const action::Binding& binding,const action::Request& request,std::string& error) {
    action::Evidence evidence;if(!move_observe(binding,request,evidence,error))return false;
    auto& op=state.operations.at(request.action_uuid);
    if(op.retired)return fail(error,"NPC Move already retired");
    op.retired=true;error.clear();return true;
}
bool native_move_ui(const json& args,json& result,std::string& error) {
    result=json::object();
    if(!args.is_object()||args.size()!=3||!args.contains("token")||
       args.contains("target_action_uuid")==args.contains("record_key")||!args.contains("position")||
       !args.at("token").is_string()||!args.at(args.contains("record_key")?"record_key":"target_action_uuid").is_string()||
       !args.at("position").is_array()||args.at("position").size()!=3)
        return fail(error,"NPC Move UI requires exact token, target_action_uuid and position");
    const auto token=args.at("token").get<std::string>();
    const auto target=args.at(args.contains("record_key")?"record_key":"target_action_uuid").get<std::string>();
    if(!token_valid(token)||!npc_target_key::input(target)||
       (args.contains("record_key")&&!snapshot_npc_key::valid(target))||token==target||
       state.ui_jobs.contains(token)||state.ui_jobs.size()>=64||!state.move_ui_token.empty())
        return fail(error,"NPC Move UI token or Place target invalid/used");
    std::array<float,3> point{};
    for(size_t index=0;index<3;++index) {
        if(!args.at("position")[index].is_number())
            return fail(error,"NPC Move UI position requires numbers");
        const auto value=args.at("position")[index].get<double>();
        if(!std::isfinite(value)||std::abs(value)>1000000.0)
            return fail(error,"NPC Move UI position outside finite domain");
        point[index]=float(value);
    }
    result={{"operation","move"},{"token",token},{"target_action_uuid",snapshot_npc_key::valid(target)?std::string{}:target},
        {"canonical_record_key",npc_target_key::canonical(target)},
        {"attempted",false},{"returned",false},{"captured",false},
        {"submitted",false},{"terminal",false},{"confirmed",false}};
    if(!state.ready||GetCurrentThreadId()!=state.thread||!ui_move_runtime::enabled()||
       !net::session_wire::connected()||state.pending||state.capture_failed)
        return fail(error,"NPC Move UI requires connected idle Mainthread hooks");
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||
       controller.value("pending",true))
        return fail(error,"NPC Move UI requires idle bound Controller");
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,error))return false;
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;npc_census::Row row;
    if(!move_target(target,binding,snapshot,local,region,census,row,error))return false;
    Memory memory;action::NpcState before;action::Ledger money;undo::UndoWitness undo_before;
    json before_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,before,money,undo_before,census,error,
                    row.type==0,row.type==0?&before_tasks:nullptr)||
       undo_before.active_graph)return fail(error,error.empty()?"NPC Move UI requires idle Undo":error);
    action::ui_diagnostic::Api api;
    if(!action::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    for(const auto& guard:move_ui_probe::code::guards){
        std::vector<unsigned char> bytes(guard.size);
        if(!memory.copy(state.image+guard.rva,bytes.data(),bytes.size(),error))return false;
        if(std::memcmp(bytes.data(),guard.bytes,guard.size))
            return fail(error,"NPC Move original cursor code differs RVA="+
                std::to_string(guard.rva));
    }
    uintptr_t command=0;
    if(!field(memory,state.image+0x1707d20,0x168,command,error)||
       command!=state.image+0x759ad0)
        return fail(error,"NPC Move InfoWindow exact UICommand slot differs");
    api.command=reinterpret_cast<action::ui_diagnostic::UiCommand>(state.image+0x759ad0);
    api.destroy_label="Move";
    action::ui_diagnostic::Context context{state.image,local.manager,local.object,local.uid,
        binding.native_epoch,binding.native_generation,exe_sha,
        action::ui_diagnostic::Family::Building};
    action::ui_diagnostic::Receipt receipt;
    state.ui_jobs.emplace(token,result);auto& job=state.ui_jobs.at(token);
    job["attempted"]=true;result=job;state.move_ui_token=token;
    const auto unknown=[&](const std::string& cause){
        job["cause"]=cause;job["unknown"]=true;result=job;
        return fail(error,cause);};
    json tool_probe;
    if(!prepare_info_selection(memory,state.image,tool_probe,error)){
        job["tool_probe"]=tool_probe;return unknown(error);}
    job["tool_probe"]=tool_probe;
    auto& calls=action::ui_diagnostic::process_calls();
    if(!calls.select(api,context,receipt,error))return unknown(error);
    action::ui_diagnostic::Window window;json window_probe;
    if(!live_npc_window(memory,state.image,local,window,window_probe,error)){
        job["window_probe"]=window_probe;return unknown(error);}
    job["window_probe"]=window_probe;
    npc_census::State selected_census;action::NpcState selected;
    action::Ledger selected_money;undo::UndoWitness selected_undo;
    json selected_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,selected,selected_money,
                    selected_undo,selected_census,error,row.type==0,
                    row.type==0?&selected_tasks:nullptr)||
       selected_census.rows!=census.rows||selected_money!=money||selected_undo!=undo_before)
        return unknown(error.empty()?"NPC Move UI original Select changed Manager, Undo or Ledger":error);
    if(row.type==0&&(!quest_bridge::move_quest_stable(before,selected,
            before_tasks,selected_tasks,error)||before.roster_sha256!=selected.roster_sha256))
        return unknown(error.empty()?"NPC Move UI original Select changed Quest roster":error);
    if(!calls.command(api,window,receipt,error))return unknown(error);
    job["command_returned"]=receipt.ui_command_returned;
    job["record_cleanup_confirmed"]=receipt.record_cleanup_confirmed;
    uintptr_t cursor=0,behaviour=0,ap=0,records=0,ghost=0,selected_object=0;
    int32_t count=0,capacity=0;unsigned char flags[2]{};
    if(!field(memory,state.image,0x13df9d0,cursor,error)||!cursor||
       !field(memory,cursor,0x190,behaviour,error)||!behaviour||
       !field(memory,behaviour,0,ap,error)||ap!=state.image+0x172f1f0||
       !field(memory,behaviour,0x460,records,error)||
       !field(memory,behaviour,0x468,count,error)||
       !field(memory,behaviour,0x46c,capacity,error)||
       count!=1||capacity<1||!records||
       !field(memory,records,8,ghost,error)||
       !field(memory,records,0x10,selected_object,error)||
       !field(memory,ghost,0,ap,error)||ap!=state.image+0x1705230||
       selected_object!=local.object||
       !memory.copy(behaviour+0x400,flags,2,error)||flags[0]!=1||flags[1]!=0)
        return unknown(error.empty()?"NPC Move original cursor, ghost or selected record differs":error);
    const std::array<uintptr_t,3> steps{0x5c0200,0x5c05c0,0x5c01e0};
    for(size_t index=0;index<steps.size();++index){
        job["input_step"]=index==0?"MouseOver":index==1?"Press":"Release";
        const auto seh=move_input(state.image,behaviour,cursor,point,steps[index]);
        job["input_attempted_mask"]=job.value("input_attempted_mask",0u)|(1u<<index);
        if(seh)return unknown("NPC Move original cursor input SEH="+std::to_string(seh));
        job["input_returned_mask"]=job.value("input_returned_mask",0u)|(1u<<index);
    }
    if(!memory.copy(behaviour+0x400,flags,2,error)||flags[0]!=0||flags[1]!=1)
        return unknown(error.empty()?"NPC Move original Release did not arm one pending purchase":error);
    npc_census::State after_census;action::NpcState after;
    action::Ledger after_money;undo::UndoWitness undo_after;
    json after_tasks=json::array();
    if(!read_effect(memory,state.image,region,local.uid,after,after_money,undo_after,
                    after_census,error,row.type==0,row.type==0?&after_tasks:nullptr)||
       after_census.rows!=census.rows||
       after_money!=money||undo_after!=undo_before)
        return unknown(error.empty()?"NPC Move Client UI changed Manager, Undo or Ledger before Host order":error);
    if(row.type==0&&(!quest_bridge::move_quest_stable(before,after,
            before_tasks,after_tasks,error)||before.roster_sha256!=after.roster_sha256))
        return unknown(error.empty()?"NPC Move Client UI changed Quest roster before Host order":error);
    job["returned"]=true;result=job;error.clear();return true;
}

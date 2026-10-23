namespace {
struct PropertyString {const char* data=nullptr;size_t size=0;std::array<uint8_t,16> storage{};};
using PropertyRename=void(__fastcall*)(void*,const PropertyString*);
using PropertyOnline=bool(__fastcall*)(void*,int32_t);
using PropertyCommand=bool(__fastcall*)(void*,const PropertyString*);
PropertyRename property_rename=nullptr;
PropertyOnline property_online=nullptr;
PropertyCommand property_command=nullptr;
bool property_installed=false,property_install_attempted=false;
std::string property_install_cause;
bool property_fields(Memory& memory,uintptr_t object,action::NpcPropertyState& out,std::string& error){
    uint8_t forced=0;uintptr_t region=0;
    if(!field(memory,object,0x838,out.online,error)||!field(memory,object,0x6c0,forced,error)||
       !field(memory,object,0x6b0,out.tint,error)||!field(memory,object,0x178,region,error)||!region||
       !field(memory,object,0x890,out.command_level,error)||
       !field(memory,region,0x838,out.region_min,error)||!field(memory,region,0x83c,out.region_max,error))
        return fail(error,error.empty()?"NPC property Region owner absent":error);
    if(out.online<0||out.online>2||forced>1||bool(forced)!=(out.online!=2))
        return fail(error,"NPC online mode/forced tint invalid mode="+std::to_string(out.online)+" forced="+std::to_string(forced));
    for(float v:out.tint)if(!std::isfinite(v))return fail(error,"NPC forced tint is nonfinite");
    if(out.region_min<0||out.region_max<out.region_min||out.command_level<0)
        return fail(error,"NPC command level/Region bounds invalid level="+std::to_string(out.command_level)+
            " min="+std::to_string(out.region_min)+" max="+std::to_string(out.region_max));
    out.force_tint=forced!=0;return true;
}
bool property_read(Memory& memory,uintptr_t region,uint64_t uid,uintptr_t object,
                   action::NpcPropertyState& output,action::Ledger& money,undo::UndoWitness& witness,
                   npc_census::State& census,json& tasks,std::string& error){
    if(!npc_census::read(memory,state.image,region,census,error))return false;
    npc_census::Row row;if(!move_row(census,uid,row,error)||row.local_pointer!=object)
        return fail(error,error.empty()?"NPC property full UID/Manager object differs":error);
    return read_effect(memory,state.image,region,uid,output.actor,money,witness,census,error,row.type==0,&tasks)&&
        property_fields(memory,object,output,error);
}
bool property_capture(uintptr_t object,action::NpcPropertyIntent intent,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!state.ready||GetCurrentThreadId()!=state.thread||state.pending||property_pending||state.capture_failed||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(error,"NPC property UI requires ready idle Mainthread Controller");
    if(!action::valid_npc_property(intent))return fail(error,"NPC property UI value invalid");
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    if(!typed_action_runtime::network_binding(snapshot,binding,error))return false;
    Memory memory;uint64_t uid=0;std::string key;
    if(!field(memory,object,0x18,uid,error)||!local_target_key(object,uid,key,error))return false;
    ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
    if(!target_binding(key,binding,snapshot,local,region,census,error)||local.uid!=uid||local.object!=object)
        return fail(error,error.empty()?"NPC property UI target binding differs":error);
    action::ui_diagnostic::Window window;json probe;
    if(!live_npc_window(memory,state.image,local,window,probe,error))return false;
    action::NpcPropertyState before;action::Ledger money;undo::UndoWitness witness;json tasks;
    if(!property_read(memory,region,uid,object,before,money,witness,census,tasks,error)||witness.active_graph)
        return fail(error,error.empty()?"NPC property capture requires idle Undo":error);
    if(action::npc_property_unchanged(before,intent))
        return fail(error,"NPC property UI value is unchanged");
    property_pending=PropertyPending{intent,binding,snapshot,npc_target_key::canonical(key)};
    ++state.captures;return true;
}
void property_rename_entry(void* object,const PropertyString* value){
    const auto caller=uintptr_t(_ReturnAddress());
    if(state.replaying||!net::session_wire::connected()||caller!=state.image+0x628dea){
        if(property_rename)property_rename(object,value);return;}
    try{
        Memory memory;uintptr_t ap=0;std::string error;
        if(!field(memory,uintptr_t(object),0,ap,error)){rejected(error);return;}
        if(ap!=state.image+0x1742980){bool handled=false;
            if(!ui_move_runtime::prop_text::capture_rename(uintptr_t(object),value,handled,error)){rejected(error);return;}
            if(!handled&&property_rename)property_rename(object,value);return;}
        PropertyString text;
        if(!value||!memory.copy(uintptr_t(value),&text,sizeof text,error)||!text.data||text.size>65536||
           uintptr_t(text.data)>UINTPTR_MAX-text.size){rejected(error.empty()?"NPC Rename native string span invalid":error);return;}
        action::NpcPropertyIntent intent;intent.name.resize(text.size);
        if(text.size&&!memory.copy(uintptr_t(text.data),intent.name.data(),text.size,error)){rejected(error);return;}
        if(!property_capture(uintptr_t(object),intent,error))rejected(error);
    }catch(const std::exception& exception){rejected(std::string("NPC Rename capture: ")+exception.what());}
}
bool property_online_entry(void* object,int32_t online){
    const auto caller=uintptr_t(_ReturnAddress());
    const bool player=caller==state.image+0x759e92||caller==state.image+0x75a141||caller==state.image+0x75a169;
    if(state.replaying||!net::session_wire::connected()||!player)return property_online&&property_online(object,online);
    try{action::NpcPropertyIntent intent;intent.kind=action::NpcPropertyKind::OnlineState;intent.online=online;
        std::string error;if(!property_capture(uintptr_t(object),intent,error)){rejected(error);return false;}return true;
    }catch(const std::exception& exception){rejected(std::string("NPC online capture: ")+exception.what());return false;}
}
uint32_t property_call(const action::NpcPropertyIntent& intent,uintptr_t object,bool& returned){
    PropertyString text{intent.name.data(),intent.name.size(),{}};
    if(intent.kind==action::NpcPropertyKind::RaiseLevel)text={"Raise Level",11,{}};
    if(intent.kind==action::NpcPropertyKind::LowerLevel)text={"Lower Level",11,{}};
    __try{
        if(intent.kind==action::NpcPropertyKind::Name){property_rename(reinterpret_cast<void*>(object),&text);returned=true;}
        else if(intent.kind==action::NpcPropertyKind::OnlineState)returned=property_online(reinterpret_cast<void*>(object),intent.online);
        else returned=property_command(reinterpret_cast<void*>(object),&text);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool property_level_request(uintptr_t caller,std::string_view command,action::NpcPropertyIntent& intent,std::string& error){
    if(caller==state.image+0x759ceb&&command=="Raise Level")intent.kind=action::NpcPropertyKind::RaiseLevel;
    else if(caller==state.image+0x75a0f8&&command=="Lower Level")intent.kind=action::NpcPropertyKind::LowerLevel;
    else return fail(error,"NPC level command caller/text differs");
    intent.name.clear();intent.online=0;return true;
}
bool property_command_entry(void* object,const PropertyString* value){
    const auto caller=uintptr_t(_ReturnAddress());
    const bool player=caller==state.image+0x759ceb||caller==state.image+0x75a0f8;
    if(state.replaying||!net::session_wire::connected()||!player)return property_command&&property_command(object,value);
    try{
        Memory memory;PropertyString text;std::array<char,11> bytes{};std::string error;
        if(!value||!memory.copy(uintptr_t(value),&text,sizeof text,error)||!text.data||text.size!=bytes.size()||
           !memory.copy(uintptr_t(text.data),bytes.data(),bytes.size(),error)){
            rejected(error.empty()?"NPC level command native string invalid":error);return false;}
        action::NpcPropertyIntent intent;
        if(!property_level_request(caller,std::string_view(bytes.data(),bytes.size()),intent,error)||
           !property_capture(uintptr_t(object),intent,error)){rejected(error);return false;}
        return true;
    }catch(const std::exception& exception){rejected(std::string("NPC level capture: ")+exception.what());return false;}
}
bool property_census(const npc_census::State& before,const npc_census::State& after,uint64_t target,
                     const action::NpcPropertyIntent& intent,std::string& error){
    if(before.manager!=after.manager||before.next_local_tag!=after.next_local_tag||before.rows.size()!=after.rows.size()||
       before.global_random!=after.global_random)return fail(error,"NPC property Manager/count/local counter/RNG changed");
    for(size_t i=0;i<before.rows.size();++i){auto expected=before.rows[i];
        if(expected.local_uid==target&&intent.kind==action::NpcPropertyKind::Name)expected.name=intent.name;
        if(after.rows[i]!=expected)return fail(error,"NPC property changed unrelated row/field or Manager order index="+std::to_string(i));}
    return true;
}
}
bool property_enable(std::string& error){
    if(property_pending)return fail(error,"NPC property prior captured command remains pending");
    if(property_installed)return true;
    if(property_install_attempted)return fail(error,"NPC property prior hook installation failed: "+property_install_cause);
    property_install_attempted=true;
    const auto failure=[&](const char* operation,MH_STATUS status){
        property_install_cause=std::string(operation)+" status="+std::to_string(status)+" "+
            mt2mp::hooks::framework::process_backend().error();return fail(error,property_install_cause);};
    const auto rename=MH_CreateHook(reinterpret_cast<void*>(state.image+0x4cf710),
        reinterpret_cast<void*>(&property_rename_entry),reinterpret_cast<void**>(&property_rename));
    if(rename!=MH_OK||!property_rename)return failure("NPC Rename hook create",rename);
    const auto online=MH_CreateHook(reinterpret_cast<void*>(state.image+0x3c4800),
        reinterpret_cast<void*>(&property_online_entry),reinterpret_cast<void**>(&property_online));
    if(online!=MH_OK||!property_online)return failure("NPC online hook create",online);
    const auto command=MH_CreateHook(reinterpret_cast<void*>(state.image+0x3c4c10),
        reinterpret_cast<void*>(&property_command_entry),reinterpret_cast<void**>(&property_command));
    if(command!=MH_OK||!property_command)return failure("NPC level command hook create",command);
    const uintptr_t targets[]{state.image+0x4cf710,state.image+0x3c4800,state.image+0x3c4c10};
    if(!mt2mp::hooks::enable_prepared_targets(targets,"NPC property hooks",error)){
        property_install_cause=error;return false;}
    property_installed=true;return true;
}
void property_tick(){
    if(!property_pending)return;
    auto pending=*property_pending;property_pending.reset();std::string error;action::Binding binding;
    if(pending.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(pending.snapshot,binding,error)||
       binding!=pending.binding){state.capture_failed=true;rejected(error.empty()?"NPC property pending World changed":error);return;}
    const auto id=uuid(error);
    if(id.empty()||!typed_action_runtime::submit_npc_property(id,pending.key,pending.intent,error)){
        state.capture_failed=true;rejected(error.empty()?"NPC property submit failed":error);return;}
    ++state.submits;state.last_action=id;
}
action::Outcome property_perform(const action::Binding& binding,const action::Request& request,
                                 const action::Evidence* authority,std::string& error){
    if(!state.ready||!property_installed||GetCurrentThreadId()!=state.thread||
       request.action!=action::Action::EditNpcProperty||!action::valid_npc_property(request.npc_property)||
       !npc_target_key::valid(request.canonical_record_key))
        return fail(error,"NPC property runtime/request invalid"),action::Outcome::RejectedNoEffect;
    if(state.operations.contains(request.action_uuid))return fail(error,"NPC property action already attempted; no replay"),action::Outcome::Unknown;
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid||bool(authority)==Config::instance().is_host)
        return fail(error,"NPC property exact pending Controller/role differs"),action::Outcome::RejectedNoEffect;
    Memory memory;ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
    if(!target_binding(npc_target_key::argument(request.canonical_record_key),binding,request.snapshot_uuid,local,region,census,error))
        return action::Outcome::RejectedNoEffect;
    action::NpcPropertyState before;action::Ledger money;undo::UndoWitness undo_before;json tasks;
    if(!property_read(memory,region,local.uid,local.object,before,money,undo_before,census,tasks,error)||undo_before.active_graph)
        return fail(error,error.empty()?"NPC property requires idle Undo":error),action::Outcome::RejectedNoEffect;
    const auto& intent=request.npc_property;
    if(action::npc_property_unchanged(before,intent))
        return fail(error,"NPC property value is unchanged"),action::Outcome::RejectedNoEffect;
    std::array<float,4> expected_tint=before.tint;
    if(intent.kind==action::NpcPropertyKind::OnlineState&&intent.online!=2){uintptr_t color=0;
        if(!field(memory,state.image,intent.online==1?0x164c5e0:0x164c5c0,color,error)||
           !field(memory,color,0,expected_tint,error))return action::Outcome::RejectedNoEffect;
        for(float v:expected_tint)if(!std::isfinite(v))return fail(error,"NPC native online color invalid"),action::Outcome::RejectedNoEffect;}
    if(authority){
        if(!authority->npc_property_before||!authority->npc_property_after||!action::valid_npc_property_effect(request,*authority))
            return fail(error,"NPC property Host evidence invalid"),action::Outcome::RejectedNoEffect;
        auto comparable=before;comparable.actor.global_random=authority->npc_property_before->actor.global_random;
        if(comparable!=*authority->npc_property_before||money!=authority->before)
            return fail(error,"NPC property Client before actor/Quest/online/tint/Undo/Ledger differs"),action::Outcome::RejectedNoEffect;
    }
    auto& op=state.operations.emplace(request.action_uuid,Operation{binding,request}).first->second;
    op.local_uid=local.uid;op.local_pointer=local.object;
    const auto unknown=[&](const std::string& cause){error=op.cause=cause;return action::Outcome::Unknown;};
    if(authority){op.attempted|=2;const auto seh=seed(state.image,authority->npc_property_before->actor.global_random);
        if(seh)return unknown("NPC property Host RNG bind SEH="+std::to_string(seh));op.returned|=2;
        if(!property_read(memory,region,local.uid,local.object,before,money,undo_before,census,tasks,error)||
           before!=*authority->npc_property_before)return unknown(error.empty()?"NPC property Host RNG readback differs":error);}
    op.evidence.npc_property_before=before;op.evidence.before=money;op.quest_tasks_before=tasks;
    op.attempted|=1;bool returned=false;const auto seh=property_call(intent,local.object,returned);
    if(seh||!returned)return unknown("NPC property original did not return SEH="+std::to_string(seh));op.returned|=1;
    ui::LocalBinding current;uintptr_t after_region=0;npc_census::State after_census;
    if(!target_binding(npc_target_key::argument(request.canonical_record_key),binding,request.snapshot_uuid,current,after_region,after_census,error)||
       current!=local||after_region!=region)return unknown(error.empty()?"NPC property current full binding changed":error);
    action::NpcPropertyState after;action::Ledger after_money;undo::UndoWitness undo_after;
    if(!property_read(memory,region,local.uid,local.object,after,after_money,undo_after,after_census,op.quest_tasks_after,error)||
       !property_census(census,after_census,local.uid,intent,error))return unknown(error);
    if(undo_before!=undo_after||op.quest_tasks_before!=op.quest_tasks_after||after.tint!=expected_tint)
        return unknown("NPC property original Undo/Quest tasks/native tint differs");
    op.evidence.npc_property_after=after;op.evidence.after=after_money;
    if(!action::valid_npc_property_effect(request,op.evidence)||
       (authority&&(op.evidence.npc_property_after!=authority->npc_property_after||op.evidence.after!=authority->after)))
        return unknown("NPC property original full effect differs from requested/Host result");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool property_observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    const auto found=state.operations.find(request.action_uuid);
    if(found==state.operations.end()||found->second.request!=request||found->second.binding!=binding||!found->second.confirmed)
        return fail(error,"NPC property observe lacks exact confirmed action");
    const auto& op=found->second;ui::LocalBinding local;uintptr_t region=0;npc_census::State census;
    if(!target_binding(npc_target_key::argument(request.canonical_record_key),binding,request.snapshot_uuid,local,region,census,error)||
       local.object!=op.local_pointer||local.uid!=op.local_uid)return fail(error,error.empty()?"NPC property current UID changed":error);
    Memory memory;action::NpcPropertyState current;action::Ledger money;undo::UndoWitness witness;json tasks;
    if(!property_read(memory,region,local.uid,local.object,current,money,witness,census,tasks,error))return false;
    if(current!=op.evidence.npc_property_after||money!=op.evidence.after||tasks!=op.quest_tasks_after||witness.active_graph)
        return fail(error,"NPC property confirmed actor/Quest/online/tint/Undo/Ledger changed");
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}

namespace zone_edit_runtime {
namespace {
namespace remove_ui=a::ui_capture;
bool local_target(Memory& memory,uintptr_t manager,uintptr_t object,remove_ui::LocalBinding& out,std::string& error){
    out={};a::Binding binding;const auto snapshot=net::current_snapshot_id();uint64_t uid=0;identity::Current owner;
    if(!typed_action_runtime::network_binding(snapshot,binding,error)||!q::typed(memory,za::state.image,object,0x1745ce0,error)||
       !field(memory,object,0x18,uid,error)||!identity::resolve_current(binding,snapshot,object,uid,owner,error)||owner.scene_key!="root")
        return fail(error,error.empty()?"Zone Remove current full UID/root owner differs":error);
    int32_t region=-1;if(!field(memory,owner.region,0x834,region,error))return false;
    if(remove_replay){
        const auto& operation=*remove_replay;
        const auto* before=selected(operation.before,operation.request.zone_edit.selected);
        if(!before||before->owner!=owner||operation.binding!=binding||operation.request.snapshot_uuid!=snapshot||
           operation.before.world.selection!=manager||operation.region!=region)return fail(error,"Zone Remove prepared canonical owner differs");
    }else{
        Sample current;const auto id=action_runtime::uuid(error);
        if(id.empty()||!sample(memory,binding,snapshot,region,id,current,error)||current.world.selection!=manager)
            return fail(error,error.empty()?"Zone Remove current SelectionManager differs":error);
    }
    out.manager=manager;out.object=object;out.uid=uid;out.native_epoch=binding.native_epoch;out.native_generation=binding.native_generation;
    out.snapshot_uuid=snapshot;out.canonical_record_key=owner.key;out.supported_zone=true;return true;
}
bool invoke_remove(Memory& memory,Operation& operation,std::string& error){
    const auto* target=selected(operation.before,operation.request.zone_edit.selected);
    if(!target||remove_replay)return fail(error,"Zone Remove current target or replay owner differs");
    remove_replay=&operation;struct Reset{~Reset(){remove_replay=nullptr;}} reset;
    remove_ui::Replay replay;
    replay.purpose=Config::instance().is_host?remove_ui::ReplayPurpose::HostExecution:remove_ui::ReplayPurpose::ConfirmedHostApplication;
    replay.intent={operation.request.action_uuid,operation.request.snapshot_uuid,target->owner.key};
    if(!local_target(memory,operation.before.world.selection,target->owner.object,replay.binding,error))return false;
    operation.attempted=true;
    const bool success=ui_remove_runtime::with_replay(replay,[&](std::string& cause){
        return za::native_ok(zone_edit_calls::remove(za::state.image,replay.binding.manager,replay.binding.object,
            operation.remove_entered,operation.remove_returned),"DestroyProp",cause)&&operation.remove_entered&&operation.remove_returned;
    },error);
    operation.returned=success&&operation.remove_returned;return operation.returned;
}
}
bool remove_local_binding(uintptr_t manager,uintptr_t object,a::ui_capture::LocalBinding& out,std::string& error){
    if(!ready||GetCurrentThreadId()!=za::state.thread)return fail(error,"Zone Remove bound Mainthread unavailable");
    Memory memory;return code(memory,error)&&local_target(memory,manager,object,out,error);
}
bool submit_remove(const a::ui_capture::Intent& intent,std::string& error){
    if(!idle(error)||intent.snapshot_uuid!=net::current_snapshot_id())return false;
    a::Binding binding;std::vector<identity::Current> owners;
    if(!typed_action_runtime::network_binding(intent.snapshot_uuid,binding,error)||!identity::read_current(binding,intent.snapshot_uuid,owners,error))return false;
    const auto found=std::find_if(owners.begin(),owners.end(),[&](const auto& row){return row.key==intent.canonical_record_key&&row.scene_key=="root";});
    if(found==owners.end())return fail(error,"Zone Remove requested canonical root target is absent");
    Memory memory;a::ZoneEditIntent request;request.kind=Kind::Delete;request.selected=found->key;
    if(!field(memory,found->region,0x834,request.region,error))return false;
    return typed_action_runtime::submit_zone_edit(intent.action_uuid,request,error);
}
bool authorize_remove_replay(const a::ui_capture::Replay& replay,std::string& error){
    uint64_t revision=0;
    if(!ready||GetCurrentThreadId()!=za::state.thread||!remove_replay||!remove_replay->attempted||remove_replay->remove_entered||
       remove_replay->request.action!=a::Action::EditZone||remove_replay->request.zone_edit.kind!=Kind::Delete||
       replay.intent.action_uuid!=remove_replay->request.action_uuid||replay.intent.snapshot_uuid!=remove_replay->request.snapshot_uuid||
       replay.intent.canonical_record_key!=remove_replay->request.zone_edit.selected||
       (replay.purpose==a::ui_capture::ReplayPurpose::HostExecution)!=Config::instance().is_host||
       !ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),remove_replay->request,revision,error))
        return fail(error,error.empty()?"Zone Remove private replay differs from exact ordered request":error);
    Memory memory;remove_ui::LocalBinding current;
    return local_target(memory,replay.binding.manager,replay.binding.object,current,error)&&
        ((current==replay.binding)||fail(error,"Zone Remove complete replay binding changed"));
}
}

namespace district_stroke {
namespace {
namespace remove_ui=action::ui_capture;
bool local_target(Runtime& runtime,uintptr_t manager,uintptr_t object,remove_ui::LocalBinding& out,std::string& error){
    out={};action::Binding binding;std::string snapshot_id=net::current_snapshot_id();host::WorldState world;
    if(!typed_action_runtime::network_binding(snapshot_id,binding,error)||!bound_world(runtime,binding,world,0,error)||world.selection!=manager)
        return q::fail(error,error.empty()?"District Remove current selection manager differs":error);
    uint64_t uid=0;uintptr_t actual=0;
    if(!q::typed(runtime.memory,runtime.image,object,0x16da000,error)||!q::at(runtime.memory,object,0x18,uid,error)||
       !group_edit::find(runtime,uid,actual,error)||actual!=object)return q::fail(error,error.empty()?"District Remove full current UID differs":error);
    std::string key;
    if(remove_replay){
        const auto& value=*remove_replay;const auto found=value.before.rows.find(value.request.district_stroke.selected);
        if(value.binding!=binding||value.request.snapshot_uuid!=snapshot_id||found==value.before.rows.end()||
           found->second.row.pointer!=object||found->second.row.uid!=uid||value.before.world.selection!=manager||
           !host::same_world(value.before.world.origin,world.origin))return q::fail(error,"District Remove prepared original owner differs");
        auto contains=[&](uintptr_t shape,uintptr_t point,bool& inside,std::string& cause){return fault(district_text::contains_call(runtime.image,shape,point,inside),"title containment",cause);};
        action::DistrictState current;
        if(!district_reader::read(runtime.memory,runtime.image,uintptr_t(world.origin.map),found->second,contains,current,error)||
           current!=value.before.values.at(found->first))return q::fail(error,error.empty()?"District Remove prepared complete values changed":error);
        key=found->first;
    }else if(!identity::bind_local(object,uid,key,error))return false;
    out.manager=manager;out.object=object;out.uid=uid;out.native_epoch=binding.native_epoch;out.native_generation=binding.native_generation;
    out.snapshot_uuid=snapshot_id;out.canonical_record_key=std::move(key);out.supported_district=true;return true;
}
bool invoke_remove(Runtime& runtime,Operation& value,std::string& error){
    const auto found=value.before.rows.find(value.request.district_stroke.selected);
    if(found==value.before.rows.end()||found->second.saved_region!=value.region||remove_replay)
        return q::fail(error,"District Remove current canonical target/Region or replay owner differs");
    remove_replay=&value;struct Reset{~Reset(){remove_replay=nullptr;}} reset;
    remove_ui::Replay replay;
    replay.purpose=Config::instance().is_host?remove_ui::ReplayPurpose::HostExecution:remove_ui::ReplayPurpose::ConfirmedHostApplication;
    replay.intent={value.request.action_uuid,value.request.snapshot_uuid,found->first};
    if(!local_target(runtime,value.before.world.selection,found->second.row.pointer,replay.binding,error))return false;
    value.attempted=true;
    const bool okay=ui_remove_runtime::with_replay(replay,[&](std::string& cause){
        return fault(calls::remove(runtime.image,replay.binding.manager,replay.binding.object,value.remove_entered,value.remove_returned),"DestroyProp",cause)&&
            value.remove_entered&&value.remove_returned;
    },error);
    value.returned=okay&&value.remove_returned;return value.returned;
}
}
bool remove_local_binding(uintptr_t manager,uintptr_t object,action::ui_capture::LocalBinding& out,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);
    if(!runtime||!runtime->ready||!installed||runtime->thread!=GetCurrentThreadId()||!code(*runtime,error))
        return q::fail(error,error.empty()?"District Remove bound runtime unavailable":error);
    return local_target(*runtime,manager,object,out,error);
}
bool submit_remove(const action::ui_capture::Intent& intent,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);identity::Current target;
    if(!runtime||!idle(*runtime,error)||intent.snapshot_uuid!=net::current_snapshot_id()||!identity::resolve(intent.canonical_record_key,target,error))return false;
    action::DistrictStrokeIntent request;request.kind=Kind::Delete;request.region=target.saved_region;request.selected=target.key;
    return typed_action_runtime::submit_district_stroke(intent.action_uuid,request,error);
}
bool authorize_remove_replay(const action::ui_capture::Replay& replay,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!runtime||!remove_replay||runtime->thread!=GetCurrentThreadId()||!remove_replay->attempted||remove_replay->remove_entered||
       remove_replay->request.action!=action::Action::DistrictStroke||remove_replay->request.district_stroke.kind!=Kind::Delete||
       replay.intent.action_uuid!=remove_replay->request.action_uuid||replay.intent.snapshot_uuid!=remove_replay->request.snapshot_uuid||
       replay.intent.canonical_record_key!=remove_replay->request.district_stroke.selected||
       (replay.purpose==action::ui_capture::ReplayPurpose::HostExecution)!=Config::instance().is_host||
       !contract::pending(typed_action_runtime::controller_status(),remove_replay->request,revision,error))
        return q::fail(error,error.empty()?"District Remove private replay does not match current ordered action":error);
    action::ui_capture::LocalBinding current;
    return local_target(*runtime,replay.binding.manager,replay.binding.object,current,error)&&
        (current==replay.binding||q::fail(error,"District Remove complete replay binding changed"));
}
}

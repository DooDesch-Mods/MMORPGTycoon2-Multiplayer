namespace gizmo_text=mt2mp::game::npc_probe::monster_action_runtime::dungeon_gizmo;
bool capture_gizmo(Runtime& runtime,uintptr_t object,const void* native_text,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!installed||!runtime.ready||GetCurrentThreadId()!=runtime.thread||captured||runtime.pending||
       !controller.value("started",false)||controller.value("pending",true)||controller.value("locked",true))
        return mt2mp::game::npc_quest_targets::fail(error,"Gizmo Rename requires the existing idle bound Prop text Controller");
    action::PropTextIntent intent;std::string key;
    if(!text(runtime,uintptr_t(native_text),intent.text,error)||!gizmo_text::rename_target(object,key,intent.gizmo_scene,error)||!action::valid_prop_text(intent))
        return mt2mp::game::npc_quest_targets::fail(error,error.empty()?"Gizmo Rename original Header text/owner is invalid":error);
    const auto snapshot=net::current_snapshot_id();action::Binding binding;
    action::DungeonGizmoState before;dungeon_gizmo_identity::Member member;action::Ledger money;witness::UndoWitness undo;
    if(!typed_action_runtime::network_binding(snapshot,binding,error)||
       !gizmo_text::rename_read(binding,snapshot,key,intent.gizmo_scene,before,member,money,undo,error))return false;
    bool same=false;for(const auto& scene:before.scenes)for(const auto& row:scene.rows)if(row.key==key)same=row.value.name==intent.text;
    if(same)return true;
    captured=Pending{binding,snapshot,key,std::move(intent)};++captures;return true;
}
bool gizmo_wire(const action::Request& request,const action::PropTextState& before,const action::Ledger& money,std::string& error){
    auto packet=std::make_unique<action::Message>();packet->session=packet->revision=UINT64_MAX;packet->kind=action::Kind::ApplyAck;
    packet->ack_peer=4;packet->required_mask=15;packet->outcome=action::Outcome::Confirmed;packet->request=request;
    packet->request.sequence=packet->request.host_revision=UINT64_MAX;
    auto& proof=packet->evidence;proof.before=proof.after=money;proof.prop_text_before=proof.prop_text_after=before;proof.retirement_confirmed=true;
    for(auto& scene:proof.prop_text_after->gizmos->scenes)for(auto& row:scene.rows)if(row.key==request.canonical_record_key)row.value.name=request.prop_text.text;
    std::vector<uint8_t> bytes;return action::encode(*packet,bytes,error);
}
action::Outcome perform_gizmo(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& error){
    auto* runtime=published.load(std::memory_order_acquire);const auto controller=typed_action_runtime::controller_status();
    if(!runtime||!runtime->ready||!installed||GetCurrentThreadId()!=runtime->thread||request.action!=action::Action::EditPropText||
       request.prop_text.gizmo_scene.empty()||!action::valid_prop_text(request.prop_text)||
       !controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid||bool(authority)==Config::instance().is_host){
        error="Gizmo Rename requires exact pending Prop text role/Mainthread/Controller";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Gizmo Rename already attempted; no replay";return action::Outcome::Unknown;}
    action::PropTextState before;before.gizmos.emplace();dungeon_gizmo_identity::Member member;
    action::Ledger money;witness::UndoWitness undo;
    if(!gizmo_text::rename_read(binding,request.snapshot_uuid,request.canonical_record_key,request.prop_text.gizmo_scene,*before.gizmos,member,money,undo,error))
        return action::Outcome::RejectedNoEffect;
    const auto local_before=*before.gizmos;
    if(authority&&authority->prop_text_before&&authority->prop_text_before->gizmos)
        before.gizmos->global_random=authority->prop_text_before->gizmos->global_random;
    if(!gizmo_wire(request,before,money,error))return action::Outcome::RejectedNoEffect;
    if(authority&&(!action::valid_prop_text_effect(request,*authority)||authority->prop_text_before!=before||authority->before!=money)){
        error="Gizmo Rename Client complete authored scene/Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    auto& operation=operations.try_emplace(request.action_uuid).first->second;operation.binding=binding;operation.request=request;
    operation.gizmo=member;operation.gizmo_random=local_before.global_random;operation.undo=undo;operation.evidence.before=money;operation.evidence.prop_text_before=before;
    const auto unknown=[&](const std::string& why){operation.cause=error=why.empty()?"Gizmo Rename original effect is Unknown without cause":why;return action::Outcome::Unknown;};
    operation.attempted=true;
    if(!gizmo_text::rename_apply(binding,request.snapshot_uuid,request.canonical_record_key,request.prop_text.gizmo_scene,member,local_before,money,undo,
       request.prop_text.text,operation.returned,error))return unknown(error);
    action::PropTextState after;after.gizmos.emplace();dungeon_gizmo_identity::Member actual;action::Ledger after_money;witness::UndoWitness after_undo;
    if(!gizmo_text::rename_read(binding,request.snapshot_uuid,request.canonical_record_key,request.prop_text.gizmo_scene,*after.gizmos,actual,after_money,after_undo,error))return unknown(error);
    if(actual!=member||undo!=after_undo||after.gizmos->global_random!=local_before.global_random)return unknown("Gizmo Rename changed the original complete lifetime, RNG or Undo owner");
    after.gizmos->global_random=before.gizmos->global_random;
    operation.evidence.after=after_money;operation.evidence.prop_text_after=after;
    if(!action::valid_prop_text_effect(request,operation.evidence)||
       (authority&&(authority->prop_text_after!=after||authority->after!=after_money)))return unknown("Gizmo Rename full original result differs from exact requested/Host state");
    operation.confirmed=true;return action::Outcome::Confirmed;
}
bool observe_gizmo(const Operation& operation,action::Evidence& evidence,std::string& error){
    action::DungeonGizmoState value;dungeon_gizmo_identity::Member member;action::Ledger money;witness::UndoWitness undo;
    const auto& request=operation.request;
    if(!gizmo_text::rename_read(operation.binding,request.snapshot_uuid,request.canonical_record_key,request.prop_text.gizmo_scene,value,member,money,undo,error))return false;
    if(value.global_random!=operation.gizmo_random)return mt2mp::game::npc_quest_targets::fail(error,"Gizmo Rename local RNG changed before ACK");
    if(operation.evidence.prop_text_after&&operation.evidence.prop_text_after->gizmos)
        value.global_random=operation.evidence.prop_text_after->gizmos->global_random;
    if(member!=*operation.gizmo||undo!=operation.undo||money!=operation.evidence.after||
       !operation.evidence.prop_text_after||!operation.evidence.prop_text_after->gizmos||value!=*operation.evidence.prop_text_after->gizmos)
        return mt2mp::game::npc_quest_targets::fail(error,"Gizmo Rename current complete scene/lifetime/Undo/Ledger differs before ACK");
    evidence=operation.evidence;evidence.retirement_confirmed=operation.retired;return true;
}

bool rules_feature_events_observe(nlohmann::json& out,std::string& e){
    out={{"read_only",true},{"execution_ack",false},{"event_dispatched",false},{"features_ordered_eligible",false}};
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||!Config::instance().local_test||GetCurrentThreadId()!=r->thread)return fail(e,"Feature event observation requires owned ready local-test Mainthread");
    bool inside=false;if(!typed_action_runtime::original_go_admission(inside,e)||!inside)return fail(e,e.empty()?"Feature event observation requires actual original Go":e);
    const auto controller=typed_action_runtime::controller_status();
    if(r->replay||r->pending||policy_pending()||rules_pending()||intervention_pending()||control_input::runtime::pending()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"Feature event observation requires idle unlocked owners");
    a::Context context;a::Binding binding;bool ordered=false;
    if(!a::selected_session().current(context,e)||!rules_capture_binding(context.snapshot_uuid,binding,ordered,e)||!ordered||Config::instance().is_host!=(context.role==a::Role::Host)||!rules_routes(*r,e))return fail(e,e.empty()?"Feature event observation current ordered role/Binding differs":e);
    Local local;rules::Pin pin;a::GameRulesProof before;a::Ledger money;
    if(!rules_proof(*r,context,binding,local,pin,before,money,e))return false;
    auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(*r,object,offset,value,cause);};
    rules::event_observer::State recipients;if(!rules::event_observer::capture(read,r->image,recipients,e))return false;
    Local final;rules::Pin final_pin;a::GameRulesProof after;a::Ledger final_money;a::Context final_context;a::Binding final_binding;bool final_ordered=false;
    if(!rules_proof(*r,context,binding,final,final_pin,after,final_money,e)||!a::selected_session().current(final_context,e)||!rules_capture_binding(context.snapshot_uuid,final_binding,final_ordered,e))return false;
    if(final_context!=context||final_binding!=binding||!final_ordered||final.pin!=local.pin||final_pin!=pin||after!=before||final_money!=money)return fail(e,"Feature event observation complete current owners/proof changed");
    rules::event_observer::State checked;if(!rules::event_observer::capture(read,r->image,checked,e))return false;
    if(checked!=recipients)return fail(e,"Feature event observation recipients changed during complete proof");
    auto rva=[&](uintptr_t address)->nlohmann::json{return address>=r->image&&address-r->image<0x1d8d000?nlohmann::json(address-r->image):nlohmann::json(nullptr);};
    nlohmann::json rows=nlohmann::json::array();
    for(const auto& row:recipients.recipients)rows.push_back({{"node",row.node},{"next",row.next},{"listener",row.listener},{"vtable",row.vtable},{"callback",row.callback},{"vtable_rva",rva(row.vtable)},{"callback_rva",rva(row.callback)}});
    out.update({{"snapshot_uuid",context.snapshot_uuid},{"epoch",binding.native_epoch},{"generation",binding.native_generation},{"role",context.role==a::Role::Host?"host":"client"},{"proof",rules::encode(before)},{"ledger",a::management_ledger_observation(money)},{"event_type",59},{"events_enabled",recipients.enabled!=0},{"recipient_count",recipients.count},{"recipients",rows},{"recipient_observation_only",true}});
    e.clear();return true;
}

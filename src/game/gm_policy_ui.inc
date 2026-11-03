namespace {
struct PolicyUi {uintptr_t window=0,tabs=0,view=0,checkbox=0,spinner=0;bool visible=false,ready=false,checked=false;int32_t current=0;};
bool policy_ui_find(Runtime& r,uintptr_t queue,uint8_t policy,gp::Field field,PolicyUi& out,std::string& e){
    out={};if(policy>1||size_t(field)>=gp::names.size())return fail(e,"GM policy UI target outside original domain");
    if(!gm_fixture_window(r,out.window,false,e))return false;uint8_t shown=0;
    if(!at(r,out.window,0x45f,shown,e)||shown>1)return fail(e,e.empty()?"GM policy window visibility invalid":e);out.visible=shown!=0;if(!out.visible)return true;
    const bool integer=gp::integer(field);const auto target=queue+(policy?0x70:0x48);std::vector<uintptr_t> pending{out.window};std::set<uintptr_t> seen;
    while(!pending.empty()){const auto pane=pending.back();pending.pop_back();if(!seen.insert(pane).second||seen.size()>2048)return fail(e,"GM policy active pane cycle/extent invalid");uintptr_t klass=0;if(!at(r,pane,0,klass,e))return false;
        if(klass==r.image+0x17015b0){std::string name;uintptr_t owner=0;if(!text(r,pane+0x1b0,name,256,e)||!at(r,pane,0x280,owner,e)||owner!=out.window)return fail(e,e.empty()?"GM policy tab owner differs":e);
            if(name=="multi"){if(out.tabs)return fail(e,"GM policy duplicate actual tabs");out.tabs=pane;if(!at(r,pane,0x3b8,out.view,e)||!ap(r,out.view,0x16dfe90,e))return false;}}
        if(klass==r.image+(integer?0x16e7070:0x16d98c0)){uintptr_t value=0,owner=0,address=0;if(!at(r,pane,integer?0x420:0x3c8,value,e))return false;
            if(value){if(!at(r,value,8,owner,e)||!at(r,value,0x38,address,e))return false;
                if(owner==target&&address==target+gp::offsets[size_t(field)]){std::string property;uint8_t clickable=0;bool visible=false;
                    if(!ap(r,value,integer?0x17496b0:0x1749610,e)||!text(r,value+0x18,property,64,e)||property!=gp::names[size_t(field)]||!at(r,pane,0x174,clickable,e)||clickable>1||!rules_numeric_visible(r,pane,out.window,visible,e))return fail(e,e.empty()?"GM policy original bound control differs":e);
                    if(field==gp::Field::Enabled||field==gp::Field::CheckLog){std::string name;if(!text(r,pane+0x1b0,name,256,e)||name!=(field==gp::Field::Enabled?"enable_toxic_complaints":"checklogs"))return fail(e,e.empty()?"GM policy original checkbox ID differs":e);}
                    if(out.checkbox||out.spinner)return fail(e,"GM policy actual bound control is not unique");
                    if(integer){int32_t min=0,max=0;uint8_t enabled=0;if(!at(r,pane,0x320,min,e)||!at(r,pane,0x324,max,e)||min||max!=gp::maximum(field)||!at(r,pane,0x428,out.current,e)||out.current<0||out.current>max||!at(r,pane,0x3f1,enabled,e)||enabled>1)return fail(e,e.empty()?"GM policy original spinner range/current/enabled differs":e);out.spinner=pane;out.ready=visible&&clickable==1&&enabled==1;}
                    else{uint8_t checked=0;if(!at(r,pane,0x346,checked,e)||checked>1)return fail(e,e.empty()?"GM policy original checkbox state differs":e);out.checkbox=pane;out.checked=checked!=0;out.current=int32_t(checked);out.ready=visible&&clickable==1;}
                }
            }
        }
        if(!rules_numeric_children(r,pane,klass,out.window,pending,e))return false;
    }
    return true;
}
bool policy_ui_find(Runtime& r,uintptr_t queue,bool enabled,PolicyUi& out,std::string& e){return policy_ui_find(r,queue,0,enabled?gp::Field::Enabled:gp::Field::CheckLog,out,e);}
uint32_t policy_ui_page(uintptr_t image,uintptr_t tabs)noexcept{struct View{const char* data;size_t length;unsigned char rest[16];};static constexpr char name[]="{gamemaster_tab_policy}";const View value{name,sizeof(name)-1,{}};
    __try{reinterpret_cast<void(*)(void*,const void*)>(image+0x7524f0)(reinterpret_cast<void*>(tabs),&value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t policy_ui_set_integer(uintptr_t image,uintptr_t spinner,int32_t value)noexcept{
    __try{reinterpret_cast<void(*)(void*,int32_t)>(image+0x7c9e70)(reinterpret_cast<void*>(spinner),value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool policy_observer_rows(Runtime& r,const Local& local,const a::Context& context,const a::Binding& binding,nlohmann::json& out,std::string& e){
    gm_ticket::Census census;building_mixed_identity::opened_file::CurrentWorld world;if(!gm_census(r,context,binding,local,census,world,e))return false;out=nlohmann::json::array();
    for(size_t index=0;index<census.values.size();++index)for(size_t base:{size_t(0),size_t(0x28)}){
        uintptr_t array=0;if(!at(r,census.values[index].object,base+0x10,array,e))return false;if(!array)continue;gp::PolicyArray members;
        auto read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return at(r,object,offset,value,cause);};
        if(!gp::policy_array(read,array,8,4096,members,e))return false;
        for(auto listener:members.members){uintptr_t table=0,callback=0;if(!at(r,listener,0,table,e)||!table||!at(r,table,0x38,callback,e)||!callback)return fail(e,e.empty()?"GM policy diagnostic observer target absent":e);
            auto relative=[&](uintptr_t pointer)->nlohmann::json{return pointer>=r.image&&pointer-r.image<0x1d8d000?nlohmann::json(pointer-r.image):nlohmann::json(nullptr);};
            out.push_back({{"ticket",index},{"null_object_offset",base},{"vtable_rva",relative(table)},{"callback_rva",relative(callback)}});
        }
    }
    gm_ticket::Census checked;building_mixed_identity::opened_file::CurrentWorld final_world;if(!gm_census(r,context,binding,local,checked,final_world,e)||checked!=census)return fail(e,e.empty()?"GM policy diagnostic queue changed":e);
    return true;
}
}
bool policy_ui_fixture(const nlohmann::json& args,nlohmann::json& out,std::string& e){
    out={{"execution_ack",false},{"ui_attempted",false},{"captured",false},{"unknown",false}};auto* r=published.load(std::memory_order_acquire);std::string token;
    try{
        if(!r||!r->ready||!Config::instance().local_test||GetCurrentThreadId()!=r->thread)return fail(e,"GM policy UI requires owned ready local-test Mainthread");bool inside=false;
        if(!typed_action_runtime::original_go_admission(inside,e)||!inside)return fail(e,e.empty()?"GM policy UI requires actual original Go":e);
        a::Context context;a::Binding binding;bool ordered=false;if(!a::selected_session().current(context,e)||!policy_capture_binding(context.snapshot_uuid,binding,ordered,e)||!ordered||Config::instance().is_host!=(context.role==a::Role::Host)||!policy_route_code(*r,e))return fail(e,e.empty()?"GM policy UI current ordered role/Binding differs":e);
        const auto controller=typed_action_runtime::controller_status();if(r->replay||r->pending||policy_pending()||rules_pending()||intervention_pending()||control_input::runtime::pending()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"GM policy UI requires idle unlocked owners");
        Local local;a::GmPolicyProof before;a::Ledger money;uintptr_t queue=0;PolicyCascadeLocal cascade;
        if(!policy_proof(*r,context,binding,local,before,money,queue,e,&cascade))return false;PolicyUi ui;if(!policy_ui_find(*r,queue,false,ui,e))return false;
        if(args.is_object()&&args.size()==1&&args.value("command",std::string{})=="Read"){
            nlohmann::json observers,controls=nlohmann::json::array();PolicyUi enabled;if(!policy_observer_rows(*r,local,context,binding,observers,e)||!policy_ui_find(*r,queue,true,enabled,e))return false;
            for(uint8_t p=0;p<2;++p)for(size_t f=0;f<gp::names.size();++f){PolicyUi control;if(!policy_ui_find(*r,queue,p,gp::Field(f),control,e))return false;controls.push_back({{"policy",p?"cheat":"chat"},{"field",gp::names[f]},{"ready",control.ready},{"current",control.current},{"integer",gp::integer(gp::Field(f))}});}
            out={{"controls",controls},{"read_only",true},{"execution_ack",false},{"snapshot_uuid",context.snapshot_uuid},{"epoch",binding.native_epoch},{"generation",binding.native_generation},{"proof",gp::encode(before)},{"ledger",a::management_ledger_observation(money)},{"cascade",gp::encode(cascade.value)},{"window_visible",ui.visible},{"tabs_ready",ui.tabs!=0},{"check_log_ready",ui.ready},{"checkbox_checked",ui.checked},{"chat_enabled_ready",enabled.ready},{"chat_enabled_checked",enabled.checked},{"ticket_observers_diagnostic",observers},{"observer_lists_are_canonical",false},{"ui_proofs",policy.ui_proofs}};e.clear();return true;
        }
        if(!args.is_object()||!args.contains("command")||!args.at("command").is_string()||!args.contains("token")||!args.at("token").is_string()||!args.contains("snapshot_uuid")||!args.at("snapshot_uuid").is_string()||!args.contains("epoch")||!args.at("epoch").is_number_unsigned()||!args.contains("generation")||!args.at("generation").is_number_unsigned()||!args.contains("expected_value"))return fail(e,"GM policy UI requires exact typed fields");
        const auto command=args.at("command").get<std::string>();token=args.at("token").get<std::string>();uint8_t selected_policy=0;auto field=command=="ToggleChatEnabled"?gp::Field::Enabled:gp::Field::CheckLog;int32_t expected=0,desired=0;
        if(command=="SetPolicy"){
            if(args.size()!=9||!args.contains("policy")||!args.at("policy").is_string()||!args.contains("field")||!args.at("field").is_string()||!args.contains("value"))return fail(e,"GM policy SetPolicy requires exact nine fields");
            const auto target=args.at("policy").get<std::string>();if(target!="chat"&&target!="cheat")return fail(e,"GM policy UI policy invalid");selected_policy=uint8_t(target=="cheat");
            size_t f=0;while(f<gp::names.size()&&args.at("field")!=gp::names[f])++f;if(f==gp::names.size())return fail(e,"GM policy UI property invalid");field=gp::Field(f);
            auto scalar=[&](const nlohmann::json& value,int32_t& result){if(!gp::integer(field)){if(!value.is_boolean())return false;result=int32_t(value.get<bool>());return true;}if(!value.is_number_integer()||value.is_boolean()||value<0||value>gp::maximum(field))return false;result=value.get<int32_t>();return true;};
            if(!scalar(args.at("expected_value"),expected)||!scalar(args.at("value"),desired)||expected==desired)return fail(e,"GM policy UI value type/range or unchanged input invalid");
        }else{
            if(args.size()!=6||(command!="SelectPolicy"&&command!="ToggleCheckLog"&&command!="ToggleChatEnabled")||!args.at("expected_value").is_boolean())return fail(e,"GM policy UI legacy command/fields invalid");expected=int32_t(args.at("expected_value").get<bool>());desired=1-expected;
        }
        if(!monster_target_key::uuid(token)||!monster_target_key::uuid(args.at("snapshot_uuid").get<std::string>()))return fail(e,"GM policy UI UUID invalid");
        if(!policy_ui_find(*r,queue,selected_policy,field,ui,e))return false;
        if(policy.ui_proofs.contains(token)){out=policy.ui_proofs.at(token);return fail(e,"GM policy UI token consumed; no replay");}
        if(policy.ui_unknown||policy.ui_proofs.size()>=64||!ui.visible||args.at("snapshot_uuid").get<std::string>()!=context.snapshot_uuid||args.at("epoch").get<uint64_t>()!=binding.native_epoch||args.at("generation").get<uint64_t>()!=binding.native_generation||before.state.policies[selected_policy][size_t(field)]!=expected)return fail(e,"GM policy UI identity/current visible value differs");
        if(command=="SelectPolicy"&&!ui.tabs)return fail(e,"GM policy original tabs absent");if(command!="SelectPolicy"&&(!ui.ready||ui.current!=expected))return fail(e,"GM policy original visible bound control is not ready");
        const auto captures=r->captures;out.update({{"token",token},{"command",command},{"unknown",true},{"ui_attempted",true}});policy.ui_proofs.emplace(token,out);policy.ui_unknown=true;
        bool returned=false;uint32_t fault=0;if(command=="SelectPolicy")fault=policy_ui_page(r->image,ui.tabs);else if(gp::integer(field)){fault=policy_ui_set_integer(r->image,ui.spinner,desired);returned=!fault;}else fault=rules_ui_toggle(r->image,ui.checkbox,desired!=0,returned);out["exception"]=fault;out["ui_returned"]=command=="SelectPolicy"?!fault:returned;
        bool exact=false;if(command=="SelectPolicy"){std::string page;exact=!fault&&text(*r,ui.view+0x3e8,page,256,e)&&page=="{gamemaster_tab_policy}"&&!policy.pending;out["page_requested"]=exact;}
        else{exact=!fault&&returned&&policy.pending&&policy.pending->ordered&&policy.pending->context==context&&policy.pending->binding==binding&&policy.pending->intent==gp::Intent{selected_policy,field,expected,desired}&&r->captures==captures+1;if(exact){out["action_uuid"]=policy.pending->id;out["captured"]=true;}}
        Local final;a::GmPolicyProof after;a::Ledger after_money;uintptr_t after_queue=0;PolicyCascadeLocal after_cascade;
        if(!policy_proof(*r,context,binding,final,after,after_money,after_queue,e,&after_cascade)||after!=before||after_money!=money||after_queue!=queue||final.pin!=local.pin||after_cascade.value!=cascade.value||after_cascade.managers!=cascade.managers||after_cascade.models!=cascade.models||after_cascade.spatial!=cascade.spatial){if(e.empty())e="GM policy original UI changed gameplay before ordering; token consumed";out["cause"]=e;policy.ui_proofs[token]=out;return false;}
        if(!exact){e=e.empty()?"GM policy original UI/capture unconfirmed; token consumed":e;out["cause"]=e;policy.ui_proofs[token]=out;return false;}
        out["unknown"]=false;policy.ui_proofs[token]=out;policy.ui_unknown=false;e.clear();return true;
    }catch(const std::exception& error){e=std::string("GM policy UI exception: ")+error.what();}catch(...){e="GM policy UI nonstandard exception";}
    if(r&&policy.ui_proofs.contains(token)){policy.ui_unknown=true;out["cause"]=e;policy.ui_proofs[token]=out;}return false;
}

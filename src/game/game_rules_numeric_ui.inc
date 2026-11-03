namespace {
struct RulesNumericUi {uintptr_t window=0,view=0,pane=0,edit=0,apply=0;bool visible=false,ready=false,apply_ready=false;std::array<uint32_t,14> draft{};};
bool rules_numeric_integer(const nlohmann::json& value){return value.is_number_integer()&&(value.is_number_unsigned()?value.get<uint64_t>()<=uint64_t(INT32_MAX):value.get<int64_t>()>=-1&&value.get<int64_t>()<=INT32_MAX);}
bool rules_numeric_visible(Runtime& r,uintptr_t object,uintptr_t window,bool& visible,std::string& e){
    visible=true;std::set<uintptr_t> seen;while(object!=window){uintptr_t parent=0,owner=0;uint8_t shown=0;
        if(!object||!seen.insert(object).second||seen.size()>64||!at(r,object,0x278,parent,e)||!at(r,object,0x280,owner,e)||owner!=window||!at(r,object,0x69,shown,e)||shown>1)return fail(e,e.empty()?"Design numeric ancestor binding invalid":e);visible=visible&&shown==1;object=parent;
    }return true;
}
bool rules_numeric_children(Runtime& r,uintptr_t pane,uintptr_t klass,uintptr_t window,std::vector<uintptr_t>& pending,std::string& e){
    if(klass!=r.image+0x17015b0)return service_ui_children(r,pane,klass,window,pending,e);
    uintptr_t view=0,page=0,next=0,owner=0;int32_t transition=0;std::string name,requested;
    if(!at(r,pane,0x3b8,view,e))return false;if(!view)return true;
    if(!ap(r,view,0x16dfe90,e)||!at(r,view,0x280,owner,e)||owner!=window||!at(r,view,0x410,page,e)||!at(r,view,0x418,next,e)||!at(r,view,0x408,transition,e)||transition<0||transition>2)return fail(e,e.empty()?"Design numeric original tab owner/flags differ":e);
    if(!page||next!=page||transition)return true;
    if(!text(r,page+0x1b0,name,256,e)||!text(r,view+0x3e8,requested,256,e))return false;
    if(name!=requested){
        if(!requested.empty()||name.empty())return true;std::string starting;
        if(!text(r,pane+0x360,starting,256,e))return false;if(!starting.empty())return true;
        // OnShow can request the empty default while retaining the first settled page.
        uintptr_t parent=0;
        if(!at(r,view,0x278,parent,e)||parent!=pane||!at(r,pane,0x280,owner,e)||owner!=window||
           !at(r,page,0x278,parent,e)||parent!=view||!at(r,page,0x280,owner,e)||owner!=window)
            return fail(e,e.empty()?"Design numeric default page ancestry differs":e);
    }
    pending.push_back(page);return true;
}
bool rules_numeric_find(Runtime& r,const rules::Pin& pin,bool xp,RulesNumericUi& out,std::string& e){
    out={};uintptr_t data=0;int32_t count=0,capacity=0;if(!at(r,r.image,0x1a3c478,data,e)||!at(r,r.image,0x1a3c480,count,e)||!at(r,r.image,0x1a3c484,capacity,e))return false;
    if(count<0||count>1024||capacity<count||capacity>65536||(!data&&count))return fail(e,"Design numeric registry extent invalid");std::vector<uintptr_t> windows(size_t(count),0);if(count&&!r.memory.copy(data,windows.data(),windows.size()*8,e))return false;
    for(auto window:windows){uintptr_t klass=0;if(!at(r,window,0,klass,e))return false;if(klass!=r.image+(xp?0x17883d0:0x16ff0a0))continue;std::string name;uintptr_t logic=1;uint8_t shown=0;
        if(!text(r,window+0x1b0,name,256,e)||!at(r,window,0x318,logic,e)||!at(r,window,0x45f,shown,e))return false;if(name!=(xp?"design_levels":"design_combat")||logic||shown>1)continue;if(out.window)return fail(e,"Design numeric duplicate registered window");out.window=window;out.visible=shown!=0;
    }
    if(!out.window||!out.visible)return true;std::vector<uintptr_t> pending{out.window},properties;std::set<uintptr_t> seen;
    while(!pending.empty()){const auto pane=pending.back();pending.pop_back();if(!seen.insert(pane).second||seen.size()>2048)return fail(e,"Design numeric pane cycle/extent invalid");uintptr_t klass=0;if(!at(r,pane,0,klass,e))return false;
        if(klass==r.image+0x1703ce0){if(out.view)return fail(e,"Design numeric multiple active XP views");out.view=pane;}
        if(klass==r.image+0x1701d20)properties.push_back(pane);
        if(!rules_numeric_children(r,pane,klass,out.window,pending,e))return false;
    }
    if(xp){if(!out.view)return true;if(!ap(r,out.view+0x338,0x16d9f40,e)||!at(r,out.view,0x350,out.draft,e)||!at(r,out.view,0x318,out.apply,e)||!ap(r,out.apply,0x16e3ec0,e))return false;
        std::string name,command;uint8_t disabled=1,clickable=0;bool shown=false;
        if(!text(r,out.apply+0x1b0,name,256,e)||name!="xp_apply"||!text(r,out.apply+0x388,command,256,e)||command!="MessageTo Parent Apply"||!at(r,out.apply,0x46d,disabled,e)||disabled>1||!at(r,out.apply,0x174,clickable,e)||clickable>1||!rules_numeric_visible(r,out.apply,out.window,shown,e))return fail(e,e.empty()?"Design numeric original XP Apply button differs":e);
        out.apply_ready=shown&&!disabled&&clickable==1;
    }
    const auto model=xp?out.view+0x338:pin.design+0x1c0;const auto scalar=xp?out.view+0x354:pin.design+0x1d8;
    for(auto pane:properties){uintptr_t value=0,owner=0,address=0;if(!at(r,pane,0x450,value,e))return false;if(!value)continue;if(!at(r,value,8,owner,e)||!at(r,value,0x38,address,e))return false;if(owner!=model||address!=scalar)continue;
        std::string property;if(!ap(r,value,0x17496b0,e)||!text(r,value+0x18,property,64,e)||property!=(xp?"xpPerMob":"seconds"))return fail(e,e.empty()?"Design numeric bound property differs":e);if(out.pane)return fail(e,"Design numeric bound property is not unique");out.pane=pane;
    }
    if(!out.pane)return true;uintptr_t parent=0,owner=0,direct=0;std::string command,preview;uint8_t clickable=0;bool visible=false;
    if(!at(r,out.pane,0x458,out.edit,e)||!ap(r,out.edit,0x1703290,e)||!at(r,out.edit,0x278,parent,e)||parent!=out.pane||!at(r,out.edit,0x280,owner,e)||owner!=out.window||!at(r,out.edit,0x4a0,direct,e)||direct||!text(r,out.edit+0x3e0,command,256,e)||command!="MessageTo Parent ValueEdited"||!text(r,out.edit+0x400,preview,256,e)||!preview.empty()||!at(r,out.edit,0x174,clickable,e)||clickable>1||!rules_numeric_visible(r,out.edit,out.window,visible,e))return fail(e,e.empty()?"Design numeric original unbound editor/commit contract differs":e);
    out.ready=visible&&clickable==1;return true;
}
uint32_t rules_numeric_focus(uintptr_t image,uintptr_t window,uintptr_t edit)noexcept{__try{reinterpret_cast<void(*)(void*,void*)>(image+0x81ada0)(reinterpret_cast<void*>(window),reinterpret_cast<void*>(edit));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t rules_numeric_text(uintptr_t image,uintptr_t edit,const char* text,size_t length)noexcept{struct View{const char* data;size_t length;unsigned char rest[16];};const View value{text,length,{}};__try{reinterpret_cast<void(*)(void*,const View&)>(image+0x7f1630)(reinterpret_cast<void*>(edit),value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t rules_numeric_commit(uintptr_t image,uintptr_t edit)noexcept{__try{reinterpret_cast<void(*)(void*)>(image+0x771f50)(reinterpret_cast<void*>(edit));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
void rules_numeric_apply(uintptr_t image,uintptr_t view,GmFixtureCall& out)noexcept{
    initial_release::Record record{};__try{reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,"Apply");out.constructed=true;out.label=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,"Apply");if(out.label){out.attempted=true;out.accepted=reinterpret_cast<Ui>(image+0x8286c0)(reinterpret_cast<void*>(view),&record);out.returned=true;}}__except(EXCEPTION_EXECUTE_HANDLER){out.exception=GetExceptionCode();}
    if(out.constructed){__try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out.cleaned=true;}__except(EXCEPTION_EXECUTE_HANDLER){out.cleanup_exception=GetExceptionCode();}}
}
}
bool rules_numeric_ui_fixture(const nlohmann::json& args,nlohmann::json& out,std::string& e){
    out={{"execution_ack",false},{"ui_attempted",false},{"captured",false},{"unknown",false}};auto* r=published.load(std::memory_order_acquire);std::string token;
    try{
        if(!r||!r->ready||GetCurrentThreadId()!=r->thread||!Config::instance().local_test)return fail(e,"Design numeric UI requires owned ready local-test Mainthread");bool inside=false;if(!typed_action_runtime::original_go_admission(inside,e)||!inside)return fail(e,e.empty()?"Design numeric UI requires actual original Go":e);
        a::Context context;a::Binding binding;bool ordered=false;if(!a::selected_session().current(context,e)||!rules_capture_binding(context.snapshot_uuid,binding,ordered,e)||!ordered||Config::instance().is_host!=(context.role==a::Role::Host))return fail(e,e.empty()?"Design numeric UI current ordered role/Binding differs":e);
        Local local;rules::Pin pin;a::GameRulesProof before;a::Ledger money;if(!rules_proof(*r,context,binding,local,pin,before,money,e))return false;
        if(args.is_object()&&args.size()==1&&args.value("command",std::string{})=="Read"){RulesNumericUi combat,levels;if(!rules_numeric_find(*r,pin,false,combat,e)||!rules_numeric_find(*r,pin,true,levels,e))return false;out={{"read_only",true},{"snapshot_uuid",context.snapshot_uuid},{"epoch",binding.native_epoch},{"generation",binding.native_generation},{"proof",rules::encode(before)},{"ledger",a::management_ledger_observation(money)},{"combat_registered",combat.window!=0},{"combat_visible",combat.visible},{"seconds_ready",combat.ready},{"levels_registered",levels.window!=0},{"levels_visible",levels.visible},{"xp_per_mob_ready",levels.ready},{"xp_apply_ready",levels.apply_ready},{"xp_draft",levels.draft},{"ui_proofs",design_rules.ui_proofs}};return true;}
        if(!args.is_object()||args.size()!=7||!args.contains("command")||!args.at("command").is_string()||!args.contains("token")||!args.at("token").is_string()||!args.contains("snapshot_uuid")||!args.at("snapshot_uuid").is_string()||!args.contains("epoch")||!args.at("epoch").is_number_unsigned()||!args.contains("generation")||!args.at("generation").is_number_unsigned()||!args.contains("expected")||!args.at("expected").is_number_integer()||!args.contains("value")||!args.at("value").is_number_integer())return fail(e,"Design numeric UI requires exact seven typed fields");
        if(!rules_numeric_integer(args.at("expected"))||!rules_numeric_integer(args.at("value")))return fail(e,"Design numeric integer is outside native domain");
        const auto command=args.at("command").get<std::string>();token=args.at("token").get<std::string>();const auto expected=args.at("expected").get<int64_t>(),value=args.at("value").get<int64_t>();const bool xp=command=="ShowLevels"||command=="XpPerMob"||command=="ApplyXP",edit=command=="Seconds"||command=="XpPerMob",show=command=="ShowCombat"||command=="ShowLevels";
        if((!xp&&!edit&&!show)||!monster_target_key::uuid(token)||!monster_target_key::uuid(args.at("snapshot_uuid").get<std::string>())||(edit?(expected<0||expected>INT32_MAX||value<0||value>INT32_MAX||expected==value):(expected!=-1||value!=-1)))return fail(e,"Design numeric command/UUID/value invalid");
        if(design_rules.ui_proofs.contains(token)){out=design_rules.ui_proofs.at(token);return fail(e,"Design numeric UI token consumed; no replay");}const auto controller=typed_action_runtime::controller_status();
        if(design_rules.ui_unknown||design_rules.ui_proofs.size()>=64||r->replay||r->pending||policy_pending()||rules_pending()||intervention_pending()||control_input::runtime::pending()||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))return fail(e,"Design numeric UI requires idle unlocked owners");
        RulesNumericUi ui;if(!rules_numeric_find(*r,pin,xp,ui,e)||!ui.window||args.at("snapshot_uuid").get<std::string>()!=context.snapshot_uuid||args.at("epoch").get<uint64_t>()!=binding.native_epoch||args.at("generation").get<uint64_t>()!=binding.native_generation)return fail(e,e.empty()?"Design numeric UI current identity/window differs":e);
        rules::Intent intent;intent.before=intent.after=before.state;intent.command=xp?rules::Command::ApplyXP:rules::Command::RespawnSeconds;
        if(edit){if(!ui.ready||expected!=(xp?int64_t(ui.draft[1]):before.state.respawn_seconds))return fail(e,"Design numeric actual visible value differs");if(!xp)intent.after.respawn_seconds=int32_t(value);}
        if(command=="ApplyXP"){intent.after.xp=ui.draft;if(!ui.apply_ready||!rules::valid(intent))return fail(e,"Design numeric original XP Apply is not ready");}
        if(!rules_routes(*r,e)||!policy_route_code(*r,e))return false;if(show&&(!hud_time_runtime::verify_window_show_hook(r->image+0x81ec20,e)||!loan_ui_body(*r,0x6bf990,467,"e41f9ed9d4a7af627ff4b5ad82faad352928e50a5928e5b1eb33c835c697fb37",e)))return false;
        if(command=="ApplyXP")for(const auto& guard:initial_release::bytes::guards)if(guard.rva==0x967370||guard.rva==0x9676a0||guard.rva==0x1113d80){std::vector<unsigned char> bytes(guard.size);if(!r->memory.copy(r->image+guard.rva,bytes.data(),bytes.size(),e)||std::memcmp(bytes.data(),guard.data,guard.size))return fail(e,e.empty()?"Design numeric original Record body differs":e);}
        uintptr_t focused=0;if(edit&&(!at(*r,ui.window,0x3c0,focused,e)||(focused&&focused!=ui.edit)))return fail(e,e.empty()?"Design numeric UI has another active editor":e);
        out["token"]=token;out["command"]=command;out["ui_attempted"]=true;out["unknown"]=true;design_rules.ui_unknown=true;design_rules.ui_proofs.emplace(token,out);const auto captures=r->captures;bool ok=false;numeric_diagnostics::Scope diagnostic_scope(rules_numeric_trace,out["numeric_diagnostics"],edit);
        auto phase=[&](const char* name){numeric_diagnostics::phase(rules_numeric_trace,name,r->image,ui.window,ui.edit,[&](auto object,auto offset,auto& value,auto& cause){return at(*r,object,offset,value,cause);},[&](auto address,auto& value,auto& cause){return text(*r,address,value,128,cause);});};
        if(show){const auto fault=developer_staff::original_show(r->image,ui.window);out["exception"]=fault;out["ui_returned"]=fault==0;ok=!fault&&r->captures==captures&&!design_rules.pending;}
        else if(edit){phase("before_focus");auto fault=focused?0:rules_numeric_focus(r->image,ui.window,ui.edit);phase("after_focus");uintptr_t current=0;const auto source=std::to_string(value);std::string actual;
            if(!fault&&at(*r,ui.window,0x3c0,current,e)&&current==ui.edit){fault=rules_numeric_text(r->image,ui.edit,source.data(),source.size());phase("after_set_text");if(!fault&&text(*r,ui.edit+0x2d0,actual,128,e)&&actual==source&&r->captures==captures&&!design_rules.pending){fault=rules_numeric_commit(r->image,ui.edit);phase("after_drop_focus");}else if(!fault)fault=ERROR_INVALID_DATA;}else if(!fault)fault=ERROR_INVALID_DATA;
            out["exception"]=fault;out["ui_returned"]=fault==0;ok=!fault;
            if(xp){RulesNumericUi drafted;auto wanted=ui.draft;wanted[1]=uint32_t(value);ok=ok&&rules_numeric_find(*r,pin,true,drafted,e)&&drafted.draft==wanted&&r->captures==captures&&!design_rules.pending;}
        }else{GmFixtureCall call;rules_numeric_apply(r->image,ui.view,call);out["exception"]=call.exception;out["cleanup_exception"]=call.cleanup_exception;out["record_cleanup_confirmed"]=call.cleaned;out["ui_returned"]=call.returned;ok=call.constructed&&call.label&&call.attempted&&call.accepted&&call.returned&&call.cleaned&&!call.exception&&!call.cleanup_exception;}
        if(!show&&(!edit||!xp)){ok=ok&&r->captures==captures+1&&design_rules.pending&&design_rules.pending->ordered&&design_rules.pending->context==context&&design_rules.pending->binding==binding&&design_rules.pending->intent==intent;if(ok){out["captured"]=true;out["action_uuid"]=design_rules.pending->id;}}
        Local final;rules::Pin final_pin;a::GameRulesProof after;a::Ledger after_money;RulesNumericUi retained;a::Context final_context;a::Binding final_binding;bool final_ordered=false;
        if(!rules_proof(*r,context,binding,final,final_pin,after,after_money,e)||after!=before||after_money!=money||final.pin!=local.pin||final_pin!=pin||!a::selected_session().current(final_context,e)||final_context!=context||!rules_capture_binding(context.snapshot_uuid,final_binding,final_ordered,e)||final_binding!=binding||!final_ordered||!rules_numeric_find(*r,pin,xp,retained,e)||retained.window!=ui.window||!retained.visible||(edit&&(retained.pane!=ui.pane||retained.edit!=ui.edit)))ok=false;
        if(!ok){if(e.empty())e="Design numeric original commit/capture or unchanged gameplay proof unconfirmed";out["cause"]=e;design_rules.ui_proofs[token]=out;return false;}out["unknown"]=false;design_rules.ui_unknown=false;design_rules.ui_proofs[token]=out;e.clear();return true;
    }catch(const std::exception& cause){e=std::string("Design numeric UI exception: ")+cause.what();}catch(...){e="Design numeric UI nonstandard exception";}
    if(r&&design_rules.ui_proofs.contains(token)){design_rules.ui_unknown=true;out["unknown"]=true;out["cause"]=e;design_rules.ui_proofs[token]=out;}return false;
}

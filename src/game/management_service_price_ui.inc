namespace {
void price_ui_call(uintptr_t image,uintptr_t target,const char* label,GmFixtureCall& out)noexcept{
    initial_release::Record record{};__try{reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,label);out.constructed=true;
        out.label=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,label);if(out.label){out.attempted=true;out.accepted=reinterpret_cast<Ui>(image+0x7ce7d0)(reinterpret_cast<void*>(target),&record);out.returned=true;}}
    __except(EXCEPTION_EXECUTE_HANDLER){out.exception=GetExceptionCode();}
    if(out.constructed){__try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out.cleaned=true;}__except(EXCEPTION_EXECUTE_HANDLER){out.cleanup_exception=GetExceptionCode();}}
}
bool price_ui_target(Runtime& r,const ServiceUi& ui,const a::ManagementServices& services,int32_t ordinal,int direction,int32_t& currency,int32_t& next,std::string& e){
    if(!ui.window||!ui.spinner||ordinal<0||size_t(ordinal)>=services.rows.size())return fail(e,"Service price visible selected spinner absent");
    const auto spinner=ui.spinner;std::string live,final,sync;uintptr_t klass=0,button=0,button_interface=0,text_child=0,click=0,finalize=0;
    uint8_t enabled=0,editing=1,text_editing=1,shown=0,disabled=1,clickable=0,synced=2;int32_t min=0,max=0,step=0,current=0,scale=0;
    if(!at(r,spinner,0,klass,e)||klass!=r.image+0x17019c0||!at(r,klass,0x1b8,click,e)||click!=r.image+0x7cede0||!at(r,klass,0x1d8,finalize,e)||finalize!=r.image+0x7cc520||
       !text(r,spinner+0x368,live,256,e)||!text(r,spinner+0x388,final,256,e)||!text(r,spinner+0x3a8,sync,256,e)||
       !at(r,spinner,0x409,enabled,e)||!at(r,spinner,0x418,text_child,e)||!text_child||!at(r,text_child,0x584,text_editing,e)||!at(r,spinner,0x475,editing,e)||
       !at(r,spinner,0x320,min,e)||!at(r,spinner,0x324,max,e)||!at(r,spinner,0x328,step,e)||!ap(r,spinner+0x450,0x174ab50,e)||!at(r,spinner,0x468,currency,e)||!at(r,spinner,0x46c,current,e)||!at(r,spinner,0x474,synced,e))return fail(e,e.empty()?"Service price spinner fields or callback differ":e);
    if(live!=""||final!="MessageTo Parent SetPrice "+std::to_string(ordinal)||sync!="MessageTo Parent SetSync "+std::to_string(ordinal)||enabled!=1||editing||text_editing||synced>1||step<=0||min>max||current<min||current>max)return fail(e,"Service price actual spinner command, enabled/edit state or bounds differ");
    const auto& row=services.rows[size_t(ordinal)];auto actual=row.price;if(row.synced&&services.config_present){const auto found=std::find_if(services.configs.begin(),services.configs.end(),[&](const auto& config){return config.key==row.config_key;});if(found!=services.configs.end())actual=found->price;}
    if(synced!=uint8_t(row.synced)||currency!=actual[0]||current!=actual[1])return fail(e,"Service price visible value or sync differs from actual Service");
    if(!at(r,spinner,direction>0?0x430:0x438,button,e)||!button||!ap(r,button,0x1708f10,e)||!at(r,button,0x598,button_interface,e)||button_interface!=spinner+0x318||!at(r,button,0x588,scale,e)||scale!=direction||
       !at(r,button,0x69,shown,e)||!at(r,button,0x46d,disabled,e)||!at(r,button,0x174,clickable,e))return fail(e,e.empty()?"Service price arrow actual owner or flags differ":e);
    std::string name;if(!text(r,button+0x1b0,name,256,e))return false;if(name!=(direction>0?"buttonUp":"buttonDown")||shown!=1||disabled||clickable!=1)return fail(e,"Service price actual arrow is not visible and enabled");
    uintptr_t pane=button;std::set<uintptr_t> parents;bool contains_spinner=false;
    while(pane!=ui.window){if(!pane||!parents.insert(pane).second||parents.size()>64)return fail(e,"Service price arrow ancestor chain invalid");if(pane==spinner)contains_spinner=true;uintptr_t parent=0,owner=0;uint8_t visible=0;
        if(!at(r,pane,0x69,visible,e)||!at(r,pane,0x278,parent,e)||!at(r,pane,0x280,owner,e))return false;if(visible!=1||owner!=ui.window)return fail(e,"Service price arrow ancestor is hidden or foreign");pane=parent;}
    if(!contains_spinner)return fail(e,"Service price arrow does not belong to the selected spinner");
    pane=text_child;parents.clear();while(pane!=spinner){if(!pane||!parents.insert(pane).second||parents.size()>64)return fail(e,"Service price text child ancestor chain invalid");uintptr_t parent=0,owner=0;
        if(!at(r,pane,0x278,parent,e)||!at(r,pane,0x280,owner,e))return false;if(owner!=ui.window)return fail(e,"Service price text child belongs to another window");pane=parent;}
    const int64_t raw=int64_t(current)+int64_t(direction)*step;if(raw<INT32_MIN||raw>INT32_MAX)return fail(e,"Service price native arrow arithmetic would overflow int32");
    const int64_t result=(std::clamp)(raw,int64_t(min),int64_t(max));
    if(result==current)return fail(e,"Service price arrow is already at its native limit");next=int32_t(result);return true;
}
}
bool service_price_ui_fixture(const nlohmann::json& args,nlohmann::json& out,std::string& e){
    out={{"read_only",false},{"execution_ack",false},{"ui_attempted",false},{"captured",false},{"unknown",false}};auto* r=published.load(std::memory_order_acquire);std::string token;bool attempted=false;
    try{
        if(!r||!r->ready||GetCurrentThreadId()!=r->thread||!Config::instance().local_test)return fail(e,"Service price UI requires owned ready local-test Mainthread");
        bool inside=false;if(!typed_action_runtime::original_go_admission(inside,e)||!inside)return fail(e,e.empty()?"Service price UI requires original Go scope":e);
        a::Context c;a::Binding b;bool ordered=false;if(!a::selected_session().current(c,e)||!typed_action_runtime::network_binding_with_mode(c.snapshot_uuid,b,e,&ordered)||!ordered||Config::instance().is_host!=(c.role==a::Role::Host))return fail(e,e.empty()?"Service price UI requires exact ordered role/binding":e);
        if(!args.is_object()||args.size()!=7||!args.contains("command")||!args.at("command").is_string()||!args.contains("key")||!args.at("key").is_string()||!args.contains("ordinal")||!args.at("ordinal").is_number_integer()||!args.contains("token")||!args.at("token").is_string()||!args.contains("snapshot_uuid")||!args.at("snapshot_uuid").is_string()||!args.contains("epoch")||!args.at("epoch").is_number_unsigned()||!args.contains("generation")||!args.at("generation").is_number_unsigned())return fail(e,"Service price UI requires exact typed fields");
        const auto command=args.at("command").get<std::string>();const int direction=command=="Increment"?1:command=="Decrement"?-1:0;if(!direction)return fail(e,"Service price UI direction invalid");
        token=args.at("token").get<std::string>();if(!monster_target_key::uuid(token)||args.at("snapshot_uuid")!=c.snapshot_uuid||args.at("epoch").get<uint64_t>()!=b.native_epoch||args.at("generation").get<uint64_t>()!=b.native_generation)return fail(e,"Service price UI token or current identity invalid");
        if(r->service_ui_proofs.contains(token)){out=r->service_ui_proofs.at(token);return fail(e,"Service price UI token consumed; no replay");}
        if(r->service_ui_locked||r->service_ui_proofs.size()>=64||r->pending||policy_pending()||rules_pending()||intervention_pending()||control_input::runtime::pending()||r->replay)return fail(e,r->service_ui_locked?r->service_ui_cause:"Service price UI requires empty pending owners");
        a::ManagementIntent intent;intent.op=a::ManagementOp::SetServicePrice;intent.key=args.at("key").get<std::string>();intent.service_ordinal=args.at("ordinal").get<int32_t>();intent.currency=0;intent.value=0;if(!a::management_valid(intent))return fail(e,"Service price UI Building key/ordinal invalid");
        Local local,after_local;a::ManagementState before,after;a::Ledger money,after_money;if(!read(*r,c,b,local,before,money,e,&intent)||!before.services||!before.services->price)return false;
        const bool synced=before.services->rows[size_t(intent.service_ordinal)].synced;ServiceUi ui;if(!service_ui_find(*r,local,intent.service_ordinal,synced,ui,e)||!price_ui_target(*r,ui,*before.services,intent.service_ordinal,direction,intent.currency,intent.value,e))return false;
        if(!loan_ui_body(*r,0x7ce7d0,832,"8c0eae47eb7fb09979214b10c47193e6ccd4040ff5763ab6d6924e84ba0bf872",e)||!loan_ui_body(*r,0x7cede0,272,"bfa108dff5af0223e22205127c9b696ead62194c6460258b199053638217e1db",e)||!loan_ui_body(*r,0x7cc520,1552,"17878ce2afc44c0047071ebb4b2b5176dc9fc3dd98d5c51a98b39046c7fd87d6",e))return false;
        for(const auto& guard:initial_release::bytes::guards)if(guard.rva==0x967370||guard.rva==0x9676a0||guard.rva==0x1113d80){std::vector<unsigned char> bytes(guard.size);if(!r->memory.copy(r->image+guard.rva,bytes.data(),bytes.size(),e)||std::memcmp(bytes.data(),guard.data,guard.size))return fail(e,e.empty()?"Service price UI record body differs":e);}
        const auto captures=r->captures,rejected=r->rejected;out["token"]=token;out["unknown"]=true;r->service_ui_proofs.emplace(token,out);r->service_ui_locked=true;r->service_ui_cause="Service price original callback unconfirmed; no replay";out["ui_attempted"]=true;r->service_ui_proofs[token]=out;attempted=true;
        GmFixtureCall call;price_ui_call(r->image,ui.spinner,direction>0?"SpinnerIncrement":"SpinnerDecrement",call);
        out["record_constructed"]=call.constructed;out["record_confirmed"]=call.label;out["callback_attempted"]=call.attempted;out["ui_returned"]=call.returned;out["ui_accepted"]=call.accepted;out["record_cleanup_confirmed"]=call.cleaned;out["exception"]=call.exception;out["cleanup_exception"]=call.cleanup_exception;
        bool ok=call.constructed&&call.label&&call.attempted&&call.returned&&call.accepted&&call.cleaned&&!call.exception&&!call.cleanup_exception&&r->captures==captures+1&&r->pending&&r->pending->ordered&&r->pending->context==c&&r->pending->binding==b&&r->pending->intent==intent;
        if(ok){out["captured"]=true;out["action_uuid"]=r->pending->id;out["currency"]=intent.currency;out["value"]=intent.value;}
        std::string why;a::Context after_context;a::Binding after_binding;bool after_ordered=false;
        if(!read(*r,c,b,after_local,after,after_money,why,&intent)||!a::selected_session().current(after_context,why)||after_context!=c||!typed_action_runtime::network_binding_with_mode(c.snapshot_uuid,after_binding,why,&after_ordered)||after_binding!=b||!after_ordered||after_local.pin!=local.pin||after_local.software!=local.software||after_local.release!=local.release||after_local.current_release!=local.current_release||after_local.lounge!=local.lounge||after_local.lounge_count!=local.lounge_count||after_local.marketing!=local.marketing||after_local.gm_rows!=local.gm_rows||after_local.gm_uids!=local.gm_uids||after_local.loan_rows!=local.loan_rows||after_local.offer_rows!=local.offer_rows||after_local.service_uid!=local.service_uid||after_local.service_building!=local.service_building||after_local.service_object!=local.service_object||after_local.service_rows!=local.service_rows||after_local.price_manager!=local.price_manager||after!=before||after_money!=money){ok=false;if(why.empty())why="Service price UI changed gameplay/RNG/owners/Ledger before ordering";}
        if(ok){ServiceUi retained;int32_t shown_currency=-1,shown_value=-1;uint8_t editing=1;
            if(!service_ui_find(*r,after_local,intent.service_ordinal,synced,retained,why)||retained.window!=ui.window||retained.spinner!=ui.spinner||
               !at(*r,retained.spinner,0x468,shown_currency,why)||!at(*r,retained.spinner,0x46c,shown_value,why)||!at(*r,retained.spinner,0x475,editing,why)||shown_currency!=intent.currency||shown_value!=intent.value||editing){ok=false;if(why.empty())why="Service price retained spinner presentation differs";}}
        if(!ok&&why.empty()&&r->rejected>rejected)why=r->cause;if(!ok){e=why.empty()?"Service price original callback or capture unconfirmed":why;r->service_ui_cause=e;out["cause"]=e;r->service_ui_proofs[token]=out;return false;}
        out["unknown"]=false;r->service_ui_proofs[token]=out;r->service_ui_locked=false;r->service_ui_cause.clear();e.clear();return true;
    }catch(const std::exception& ex){e=std::string("Service price UI exception: ")+ex.what();}catch(...){e="Service price UI nonstandard exception";}
    out["ui_attempted"]=attempted;if(r&&r->service_ui_proofs.contains(token)){r->service_ui_locked=true;r->service_ui_cause=e;out["cause"]=e;r->service_ui_proofs[token]=out;}return false;
}

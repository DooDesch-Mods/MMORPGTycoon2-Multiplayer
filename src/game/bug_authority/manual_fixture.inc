namespace {
const auto fixture_copy=[](auto p,auto out,auto n,auto& e){return runtime().memory.copy(p,out,n,e);};
struct FixtureState {action::Context context;action::Binding binding;staff::Capture capture;State bugs;staff::PoolState pool;};
bool fixture_code(Owner& o,std::string& e){
    struct Body {uintptr_t rva;size_t size;const char* sha;};
    constexpr Body bodies[]{
        {0x7524f0,32,"ed2ada29b3a6e0a4c7c7fe4601dbac7fd0c246dc3664a476502dbc5dc2287846"},
        {0x69dd10,288,"07dc4524d021205116833d4b4b4c1ee6b9bf1720cd7af2bfde6c6ff25859ec67"},
        {0x5acb0,64,"55b6ecbd204fef6e169bbff5da54ab1394a7ae3729597919faa784aaa8a28c2e"},
        {0x66f1f0,128,"0acfaa84c298223dd13aecbf8fa69da2bf49bde5422c38158f6ab8d348ea0979"},
        {0x674c40,656,"1dbe00619fc4a6098d26e4f6f2ded48dd5ec48cd2d6553e1602b18e3beafee99"},
        {0x674ed0,176,"284dc683610dbf0a067be0e946b8d4d5d9b21ba24dad31a2ad59afcc95a5d3e2"},
        {0x674780,1008,"5505cdfa2a51f468b54c61cc5f79ab121499fb4c841f69a84d3a7ab4e6715f2c"},
        {0x770300,2256,"914d60f246a11adb97219371e8e5c51ce57d1bd1efbd57ed46685213132f9c14"},
        {0xb63ee0,16,"499f1f307c1cb989f968a6b7fcaec591e1828877223d0b0e7e8e8b76cde8c9ca"}};
    for(const auto& body:bodies){std::vector<unsigned char> bytes(body.size);
        if(!fixture_copy(o.image+body.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=body.sha)return fail(e,"BugButton fixture original body differs RVA="+std::to_string(body.rva));}
    return staff::action_runtime::verify_entry(o.image+0x6c7b70,e)&&hud_time_runtime::verify_window_show_hook(o.image+0x81ec20,e);
}
bool fixture_read(Owner& o,FixtureState& s,std::string& e){
    staff::Capture pin;
    if(!staff::authorize_ui_update(pin,e))return false;
    if(!Config::instance().local_test||GetCurrentThreadId()!=o.thread||pin.image!=o.image||pin.thread!=o.thread)
        return fail(e,"BugButton fixture requires owned local GameUpdate/Mainthread/process");
    if(!staff::bug_policy::ready_controller(typed_action_runtime::controller_status(),typed_action_runtime::live_state_active(),e)||
       !action::selected_session().current(s.context,e)||!typed_action_runtime::network_binding(s.context.snapshot_uuid,s.binding,e))return false;
    if(s.binding.native_epoch!=pin.pin.origin.world_epoch||s.binding.native_generation!=pin.pin.origin.generation)
        return fail(e,"BugButton fixture Controller/native Origin differs");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=3||world.in_game_state!=4||!world.world_present||world.quitting||world.title_transition_pending||world.map_task_pending||world.saves_in_progress||world.edit_operations)
        return fail(e,"BugButton fixture requires idle InGame without edit/save/transition");
    if(pin.pin.time_scale!=0||!std::isfinite(pin.pin.pause_scale)||pin.pin.pause_scale<0)return fail(e,"BugButton fixture requires actual Clock0");
    if(!read_state(o,s.context,s.binding,s.capture,s.bugs,s.pool,e)||!fixture_code(o,e))return false;
    return (staff::same_origin(pin.pin.origin,s.capture.pin.origin)&&pin.local==s.capture.local&&pin.state==s.capture.state&&
        pin.pin.ledger==s.capture.pin.ledger)||fail(e,"BugButton fixture double native pin differs");
}
bool fixture_same(const FixtureState& a,const FixtureState& b){
    return a.context==b.context&&a.binding==b.binding&&staff::same_origin(a.capture.pin.origin,b.capture.pin.origin)&&
        a.capture.local==b.capture.local&&a.capture.state==b.capture.state&&a.capture.pin.ledger==b.capture.pin.ledger&&
        a.capture.pin.time_scale==b.capture.pin.time_scale&&a.capture.pin.pause_scale==b.capture.pin.pause_scale&&a.bugs==b.bugs&&a.pool==b.pool;
}
bool fixture_intent(const fixture::Request& request,const FixtureState& s,action::BugFixIntent& intent,uintptr_t& bug,int32_t& display,std::string& e){
    intent={request.bug,request.schedule,{}};bug=0;
    const auto* row=selected(s.bugs,intent);if(!row||!row->value.known)return fail(e,"BugButton fixture selected Bug is not in actual known Tracker state");
    const auto* local=runtime().lifetimes.identity(*row);
    if(!local)return fail(e,"BugButton fixture stable Bug occurrence absent");
    uintptr_t job=0;if(!field(local->bug,0x48,job,e)||!field(local->bug,0x28,display,e))return false;
    if(display<0)return fail(e,"BugButton fixture native display ID is negative");
    if(job){const auto* work=runtime().jobs.pointer(job);if(!work)return fail(e,"BugButton fixture selected Job birth absent");intent.expected_job=work->row.birth;}
    if(request.schedule!=!job)return fail(e,"BugButton fixture actual Fix/Cancel eligibility differs");
    if(!intent_now(intent,s.bugs,bug,e))return false;
    uintptr_t data=0;int32_t count=0,capacity=0;size_t matches=0;
    if(!field(runtime().tracker,0x18,data,e)||!field(runtime().tracker,0x20,count,e)||!field(runtime().tracker,0x24,capacity,e))return false;
    if(count<0||capacity<count||capacity>1000000||(count&&(!data||data>UINTPTR_MAX-size_t(count)*8)))return fail(e,"BugButton fixture actual Tracker array bounds invalid");
    for(int32_t i=0;i<count;++i){uintptr_t item=0;int32_t id=0;if(!field(data,size_t(i)*8,item,e)||!field(item,0x28,id,e))return false;
        if(id==display){if(item!=bug)return fail(e,"BugButton fixture display ID resolves to foreign Bug");++matches;}}
    return matches==1||fail(e,"BugButton fixture native display ID is not unique");
}
}
bool read_ui_fixture(const json& args,json& out,std::string& e){
    out={{"observed",false}};
    if(!args.is_object()||!args.empty())return fail(e,"BugButton UI read requires empty arguments");
    auto* o=owner.load(std::memory_order_acquire);if(!o||!o->ready)return fail(e,"BugButton UI owner is not enabled");
    std::lock_guard lock(runtime().gate);FixtureState s;if(!fixture_read(*o,s,e))return false;
    out["observed"]=true;out["native_origin_uuid"]=s.capture.pin.origin.uuid;out["epoch"]=s.capture.pin.origin.world_epoch;out["generation"]=s.capture.pin.origin.generation;
    out["snapshot_uuid"]=s.context.snapshot_uuid;out["clock"]=s.capture.pin.time_scale;out["locked"]=o->fixture_locked;
    out["receipts"]=json::array();for(const auto& [token,receipt]:o->fixture_receipts){(void)token;out["receipts"].push_back(receipt);}
    fixture::Control control;std::string cause;const bool visible=fixture::visible(fixture_copy,s.capture,control,cause);
    out["bugs_page_ready"]=visible;out["page_cause"]=cause;out["known_bugs"]=json::array();
    for(const auto& row:s.bugs.rows)if(row.value.known){fixture::Request request;request.bug=bug_identity(row);
        const auto* local=runtime().lifetimes.identity(row);uintptr_t job=0;
        if(!local||!field(local->bug,0x48,job,e))return fail(e,e.empty()?"BugButton known Bug local witness absent":e);
        request.schedule=!job;action::BugFixIntent intent;uintptr_t bug=0;int32_t display=0;
        if(!fixture_intent(request,s,intent,bug,display,e))return false;
        cause.clear();const bool ready=fixture::button(fixture_copy,s.capture,bug,false,control,cause);
        out["known_bugs"].push_back({{"bug",fixture::identity(request.bug)},{"schedule",request.schedule},{"display_id",display},{"button_ready",ready},{"button_cause",cause}});
    }return true;
}
bool run_ui_fixture(const json& args,json& out,std::string& e){
    out={{"confirmed",false},{"attempted_mask",0},{"returned_mask",0},{"unknown",false},{"peer_ack",false}};
    fixture::Request request;if(!fixture::parse(args,request,e))return false;
    auto* o=owner.load(std::memory_order_acquire);if(!o||!o->ready||GetCurrentThreadId()!=o->thread)return fail(e,"BugButton UI requires ready Mainthread owner");
    std::lock_guard lock(runtime().gate);staff::Capture access;if(!staff::authorize_ui_update(access,e))return false;
    if(const auto found=o->fixture_receipts.find(request.token);found!=o->fixture_receipts.end()){out=found->second;out["replay_rejected"]=true;return fail(e,"BugButton UI token consumed; inspect retained receipt, no replay");}
    if(o->fixture_locked||o->pending)return fail(e,"BugButton UI has an unresolved attempt or pending captured action");
    if(o->fixture_receipts.size()>=64)return fail(e,"BugButton UI retained fixture receipt limit64 reached");
    FixtureState before,again;fixture::Control control,stable;action::BugFixIntent intent;uintptr_t bug=0;int32_t display=0;
    if(!fixture_read(*o,before,e)||!fixture_intent(request,before,intent,bug,display,e))return false;
    const auto& origin=before.capture.pin.origin;
    if(origin.uuid!=request.origin||origin.world_epoch!=request.epoch||origin.generation!=request.generation)return fail(e,"BugButton UI requested native Origin differs");
    const auto control_read=[&](const FixtureState& s,fixture::Control& c){
        if(request.op==fixture::Op::Show)return fixture::tabs(fixture_copy,s.capture,c,e);
        if(request.op==fixture::Op::Select)return fixture::visible(fixture_copy,s.capture,c,e);
        return fixture::button(fixture_copy,s.capture,bug,false,c,e);
    };
    if(!control_read(before,control)||!fixture_read(*o,again,e)||!fixture_same(before,again)||!control_read(again,stable)||control!=stable)
        return fail(e,e.empty()?"BugButton UI full owner/state changed before input":e);
    auto& receipt=o->fixture_receipts.emplace(request.token,json{{"token",request.token},{"command",args.at("command")},{"bug",args.at("bug")},
        {"schedule",request.schedule},{"native_origin_uuid",request.origin},{"epoch",request.epoch},{"generation",request.generation},
        {"attempted_mask",0},{"returned_mask",0},{"confirmed",false},{"unknown",true},{"peer_ack",false},{"capture_action_uuid",""},
        {"cause","BugButton UI admitted attempt outstanding; no replay"}}).first->second;
    o->fixture_locked=true;uint32_t attempted=0,returned=0;const auto captured=o->captured;
    const auto stop=[&](){if(e.empty())e="BugButton UI failed without original cause";receipt["cause"]=e;receipt["attempted_mask"]=attempted;receipt["returned_mask"]=returned;out=receipt;return false;};
    try{
        if(request.op==fixture::Op::Show){
            attempted|=1;auto fault=staff::original_show(o->image,control.window);if(fault){e="BugButton original Show SEH="+std::to_string(fault);return stop();}returned|=1;
            attempted|=2;fault=fixture::original_page(o->image,control.tabs);if(fault){e="BugButton original Bugs SetPage SEH="+std::to_string(fault);return stop();}returned|=2;
            uint8_t shown=0;std::string requested;
            if(!field(control.window,0x45f,shown,e)||!staff::bug_policy::detail::text(fixture_copy,control.view,0x3e8,requested,e))return stop();
            if(shown!=1||requested!="{dev_tab_bugs}"){e="BugButton original Show/page request readback differs";return stop();}
        }else if(request.op==fixture::Op::Select){
            attempted=1;fixture::NativeCall call;fixture::original_select(o->image,control.window,display,&call);
            receipt["record_confirmed"]=call.record_confirmed;receipt["cleanup_confirmed"]=call.cleanup_confirmed;receipt["exception"]=call.fault;receipt["cleanup_exception"]=call.cleanup_fault;
            if(call.returned)returned=1;
            if(!call.returned||!call.accepted||!call.record_confirmed||!call.cleanup_confirmed||call.fault||call.cleanup_fault){e="BugButton original SelectBug call/record/cleanup failed";return stop();}
            uintptr_t view=0,selected=0;if(!field(control.window,0x558,view,e)||!selected_bug(fixture_copy,before.capture,view,selected,e))return stop();
            if(selected!=bug){e="BugButton original SelectBug selected a different receiver";return stop();}
        }else{
            struct ClickPort {
                Owner& owner;const FixtureState& pin;const fixture::Control& expected;uintptr_t bug;uint64_t captured;
                bool before(fixture::Input phase,std::string& e){fixture::Control actual;
                    return fixture::button(fixture_copy,pin.capture,bug,phase==fixture::Input::Release,actual,e)&&
                        (actual==expected||fail(e,"BugButton original pane changed between input phases"));}
                bool input(fixture::Input phase,std::string& e){const auto fault=fixture::original_input(owner.image,expected.button,phase);
                    return !fault||fail(e,"BugButton original input SEH="+std::to_string(fault)+" phase="+std::to_string(int(phase)));}
                bool after(fixture::Input phase,std::string& e){fixture::Control actual;FixtureState state;
                    if(!fixture_read(owner,state,e)||!fixture_same(pin,state)||!fixture::button(fixture_copy,state.capture,bug,phase==fixture::Input::Press,actual,e)||actual!=expected)
                        return fail(e,e.empty()?"BugButton input changed gameplay state or UI owner":e);
                    if(phase!=fixture::Input::Release&&(owner.pending||owner.captured!=captured))return fail(e,"BugButton action captured before original Release");return true;}
            } port{*o,before,control,bug,captured};
            if(!fixture::click(port,attempted,returned,e))return stop();
            if(!o->pending||o->captured!=captured+1||o->pending->intent!=intent||o->pending->context!=before.context||o->pending->binding!=before.binding){e="BugButton original Release did not retain exactly one expected action";return stop();}
            receipt["capture_action_uuid"]=o->pending->id;
        }
        FixtureState after;if(!fixture_read(*o,after,e)||!fixture_same(before,after)){if(e.empty())e="BugButton diagnostic changed gameplay state before ordered action";return stop();}
        if(request.op!=fixture::Op::Click&&(o->pending||o->captured!=captured)){e="BugButton Show/Select unexpectedly captured an action";return stop();}
        receipt["attempted_mask"]=attempted;receipt["returned_mask"]=returned;receipt["confirmed"]=true;receipt["unknown"]=false;receipt["cause"]="";
        o->fixture_locked=false;out=receipt;e.clear();return true;
    }catch(const std::exception& ex){e=std::string("BugButton UI exception: ")+ex.what();}
    catch(...){e="BugButton UI nonstandard exception";}
    return stop();
}

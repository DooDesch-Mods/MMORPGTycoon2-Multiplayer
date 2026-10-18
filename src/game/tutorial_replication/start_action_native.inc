// Included in native.cpp's existing private namespace after StartingpointCalls.
// The typed runtime retains the action UUID before either native call. Client
// calls execute inside the existing authoritative EconomicFence action scope.
DWORD start_pay_call(uintptr_t source,bool& accepted){
    __try{accepted=reinterpret_cast<bool(*)(void*)>(image+start_intent::pay_rva)(reinterpret_cast<void*>(source));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct StartActionCalls {
    const action_session::Context& context;const action_session::Binding& binding;
    tutorial_development::StartingpointSnapshot before;
    live_host_state::Pin before_pin{},paid_pin{};
    bool paid=false,created=false;uintptr_t created_project=0;
    std::string key="startingpoint";

    bool snapshot(tutorial_development::StartingpointSnapshot& value,live_host_state::Pin& pin,std::string& e){
        World first,last;nlohmann::json diagnostic;
        if(!origin(context,binding,first,e)||!tutorial_development::read_research(key,value,diagnostic,e,true)||
           !origin(context,binding,last,e))return false;
        if(!same_world(first,last)||!same_world(first,value.origin))return fail(e,"Startingpoint Start NativeOpen changed during read");
        if(context.role==action_session::Role::Client&&!startingpoint_clock(e))return false;
        native::WorldStatus activity;if(!native::read_world_status(activity,e))return false;
        if(activity.saves_in_progress>0||activity.edit_operations)return fail(e,"Startingpoint Start requires Save/Edit idle saves="+
            std::to_string(activity.saves_in_progress)+" edits="+std::to_string(activity.edit_operations));
        live_host_state::NativePort economy(context,binding,nullptr);
        if(!economy.read(pin,e))return false;
        return pin.world==reinterpret_cast<uintptr_t>(value.origin.world)||fail(e,"Startingpoint Start Economy/NativeOpen world differs");
    }
    bool same(const tutorial_development::StartingpointSnapshot& value,std::string& e){
        if(!same_world(before.origin,value.origin)||before.image!=value.image||before.lounge!=value.lounge||
           before.tech!=value.tech||before.record!=value.record||before.player!=value.player||
           before.inventory!=value.inventory||before.slots!=value.slots||before.release!=value.release||
           before.items!=value.items||before.admission!=value.admission||!before.creation||!value.creation)
            return fail(e,"Startingpoint Start original native owners/inventory/Release changed");
        auto expected=*before.creation;
        if(created){
            if(!created_project||value.project!=created_project)return fail(e,"Startingpoint Start created project owner differs");
            const auto source=std::find_if(expected.catalogue.begin(),expected.catalogue.end(),[&](const auto& entry){return entry.address==expected.source;});
            if(source==expected.catalogue.end())return fail(e,"Startingpoint Start source left owned catalogue");
            auto copy=*source;copy.address=created_project;expected.current=std::move(copy);
            for(auto& developer:expected.developers)if(developer.job>=5&&developer.job<=7)developer.job=0;
        }else if(value.project)return fail(e,"Startingpoint Start Pay unexpectedly created project");
        return *value.creation==expected||fail(e,"Startingpoint Start original catalogue/project copy/Developer readback differs");
    }
    bool read(action_session::Ledger& ledger,startingpoint::Row& row,std::string& e){
        tutorial_development::StartingpointSnapshot value;live_host_state::Pin pin;
        if(!snapshot(value,pin,e)||!same(value,e))return false;
        if(!value.row)return fail(e,"Startingpoint Start native row absent");
        ledger=live_host_state::ledger(pin.values);row=*value.row;
        if(paid&&!created){paid_pin=pin;}
        if(created&&pin!=paid_pin)return fail(e,"Startingpoint Start creation changed full paid Economy Pin");
        return true;
    }
    bool preflight(std::string& e){
        uintptr_t base=0;uint32_t thread=0;std::string sha;
        if(!native::read_resolver_receipt(base,thread,sha,e))return false;
        if(base!=image||thread!=mainthread||thread!=GetCurrentThreadId()||
           sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")
            return fail(e,"Startingpoint Start actual EXE/Mainthread differs");
        if((context.role==action_session::Role::Client)!=load.armed)return fail(e,"Startingpoint Start actual role/joined-load authority differs");
        if(!verify_pay_entry(image+start_intent::pay_rva,e)||!startingpoint::creation::code(memory,image,e))return false;
        if(key=="bridges"){
            nlohmann::json eligibility;if(!tutorial_development::read(eligibility,e))return false;
            const auto& view=eligibility.at("research");size_t matches=0;
            for(const auto& row:view.at("rows"))if(row.at("key")==key){
                ++matches;if(row.at("visible")!=true||row.at("owned")!=false||row.at("purchased")!=false||
                    row.at("can_choose_now")!=true||row.at("can_afford")!=true||row.at("cost")!=10000)
                    return fail(e,"Research original selection/affordability getters reject Bridges");
            }
            if(matches!=1||view.at("project_present")!=false)return fail(e,"Research original catalogue/current-project eligibility differs");
        }
        return true;
    }
    bool pay(std::string& e){
        bool accepted=false;const auto code=start_pay_call(before.creation->source,accepted);
        if(code)return fail(e,"Startingpoint Start original Pay exception="+std::to_string(code));
        if(!accepted)return fail(e,"Startingpoint Start original Pay rejected; no replay");
        paid=true;return true;
    }
    bool create(const startingpoint::Row& paid_row,const startingpoint::Row& target,std::string& e){
        if(context.role==action_session::Role::Client){
            StartingpointCalls adopter{context,binding};adopter.key=key;bool attempted=false,unknown=false;
            if(!startingpoint::apply(paid_row,target,adopter,attempted,unknown,e))return false;
            created_project=adopter.created_project;
        }else{
            const auto code=startingpoint::creation::call_set_current_project(image,before.lounge,before.creation->source);
            if(code)return fail(e,"Startingpoint Start original SetCurrentDevProject exception="+std::to_string(code));
            tutorial_development::StartingpointSnapshot value;live_host_state::Pin pin;
            if(!snapshot(value,pin,e))return false;
            created_project=value.project;
        }
        created=true;return true;
    }
};

bool run_start_action(const action_session::Context& context,const action_session::Binding& binding,
    const action_session::Ledger& ledger_before,const startingpoint::Row& row_before,
    const action_session::Ledger* host_after,const startingpoint::Row* host_row,
    action_session::Ledger& ledger_after,startingpoint::Row& row_after,
    bool& attempted,bool& unknown,std::string& e,const std::string& key="startingpoint"){
    attempted=unknown=false;
    if(bool(host_after)!=bool(host_row)||(context.role==action_session::Role::Client)!=(host_after!=nullptr))return fail(e,"Startingpoint Start role/HostResult evidence differs");
    StartActionCalls calls{context,binding};calls.key=key;
    if(!calls.snapshot(calls.before,calls.before_pin,e)||!calls.before.row||!calls.before.creation||!calls.before.record||
       !calls.before.creation->source||calls.before.creation->current||calls.before.project||
       *calls.before.row!=row_before||live_host_state::ledger(calls.before_pin.values)!=ledger_before)
        return e.empty()?fail(e,"Startingpoint Start exact current baseline differs"):false;
    if(row_before.project_present||row_before.purchased||row_before.deployed||row_before.custom)
        return fail(e,"Startingpoint Start requires actual unpurchased Startingpoint and no project");
    int32_t price=0;if(!at(calls.before.tech,0xac,price,e))return false;
    if(key=="bridges"&&price!=10000)return fail(e,"Research original Bridges price differs");
    if(!start_intent::expected_payment(ledger_before,price,ledger_after,e))return false;
    row_after=row_before;row_after.purchased=row_after.project_present=true;
    row_after.work=0;row_after.lines=row_after.bugs_total=row_after.bugs_known=row_after.desired_workers=0;
    const auto& workers=*calls.before.creation;
    if(workers.automatic_workers)row_after.desired_workers=workers.total_workers<=workers.max_workers?workers.total_workers-1:workers.max_workers;
    if(host_after&&(*host_after!=ledger_after||*host_row!=row_after))return fail(e,"Startingpoint Start native price/worker policy differs from Host result");
    start_intent::Application receipt;
    const bool applied=start_intent::apply(ledger_before,ledger_after,row_before,row_after,calls,receipt,e);
    attempted=receipt.attempted;unknown=receipt.unknown;return applied;
}

DWORD pay_project(uintptr_t image,uintptr_t project,bool& accepted){
    __try{accepted=reinterpret_cast<bool(*)(void*)>(image+0x262ec0)(reinterpret_cast<void*>(project));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool begin_snapshot(const Pin& owner,tutorial_development::StartingpointSnapshot& s,json& diagnostic,std::string& e){
    if(!tutorial_development::read_research(owner.tech_key,s,diagnostic,e,true))return false;
    // Compare the same public diagnostic shape on both sides of the private
    // selected snapshot. read() also includes the getter eligibility section.
    if(!tutorial_development::read(diagnostic,e))return false;
    if(!same(owner.world,s.origin)||owner.lounge!=s.lounge||owner.player!=s.player||owner.inventory!=s.inventory||
       owner.slots!=s.slots||owner.items!=s.items||!s.creation||!s.row||!s.record)
        return fail(e,"startingpoint begin native Origin/catalogue/record/inventory differs");
    return true;
}
bool begin_project(const Request& request,json& result,Progress*& phase,std::string& key,std::string& e){
    Pin p;if(!pin(request,p,e)||!before_begin(p.development,p.project,e))return false;
    if(!same_expected_development(p.development,request.expected_development))return fail(e,"startingpoint begin expected development differs");
    tutorial_development::StartingpointSnapshot before;json diagnostic;
    if(!begin_snapshot(p,before,diagnostic,e)||diagnostic!=p.development||before.project)return fail(e,e.empty()?"startingpoint begin private baseline differs":e);
    if(!tutorial_replication::startingpoint::creation::code(memory,p.image,e))return false;
    if(!tutorial_replication::verify_pay_entry(p.image+host_begin_pay_entry.rva,e))return false;
    int32_t price=0;if(!at(before.tech,0xac,price,e))return false;
    native::SimulationState ledger_before;if(!native::read_simulation_state(ledger_before,e))return false;
    json expected_ledger;if(!begin_payment(ledger_before,price,expected_ledger,e))return false;
    Pin again;tutorial_development::StartingpointSnapshot stable;json stable_diagnostic;
    native::SimulationState stable_ledger;
    if(!pin(request,again,e)||!begin_snapshot(again,stable,stable_diagnostic,e)||!native::read_simulation_state(stable_ledger,e))return false;
    if(!same(p.world,again.world)||p.development!=again.development||p.tutorial!=again.tutorial||
       before.creation!=stable.creation||before.row!=stable.row||before.record!=stable.record||before.tech!=stable.tech||
       begin_ledger(ledger_before)!=begin_ledger(stable_ledger))return fail(e,"startingpoint begin owners or ledger changed before payment");
    key=request.origin_uuid+":"+std::to_string(request.epoch)+":"+std::to_string(request.generation);
    phase=&worlds[key];if(phase->locked)return fail(e,phase->cause);
    if(phase->begun)return fail(e,"startingpoint begin already consumed; no replay");
    result.update({{"operation","begin"},{"token",request.token},{"before",p.development},{"price",price},
        {"ledger_before",begin_ledger(ledger_before)},{"attempted",true},{"unknown",true}});
    phase->locked=true;phase->cause="startingpoint begin native payment outstanding; no replay";receipts[request.token]=result;
    bool accepted=false;const auto pay_code=pay_project(p.image,before.creation->source,accepted);
    result["pay_exception"]=pay_code;result["payment_accepted"]=accepted;result["returned"]=pay_code==0;
    tutorial_development::StartingpointSnapshot paid;json paid_diagnostic;
    native::SimulationState paid_ledger;
    if(pay_code)e="startingpoint begin original Pay exception="+std::to_string(pay_code);
    else if(!accepted)e="startingpoint begin original Pay did not accept purchase; no retry";
    else if(!begin_snapshot(p,paid,paid_diagnostic,e)||!native::read_simulation_state(paid_ledger,e)){result["payment_readback_failed"]=true;}
    else{
        result["ledger_after_pay"]=begin_ledger(paid_ledger);
        auto expected=*before.row;expected.purchased=true;
        if(paid.row!=expected||paid.creation!=before.creation||paid.record!=before.record||paid.tech!=before.tech||paid.project||
           begin_ledger(paid_ledger)!=expected_ledger)e="startingpoint begin original Pay row/catalogue/ledger readback differs";
        else{
            result["set_current_attempted"]=true;
            const auto code=tutorial_replication::startingpoint::creation::call_set_current_project(p.image,p.lounge,before.creation->source);
            result["set_current_exception"]=code;result["returned"]=code==0;
            tutorial_development::StartingpointSnapshot after;json after_diagnostic;native::SimulationState ledger_after;
            if(code)e="startingpoint begin original SetCurrentDevProject exception="+std::to_string(code);
            else if(!begin_snapshot(p,after,after_diagnostic,e)||!native::read_simulation_state(ledger_after,e)){result["creation_readback_failed"]=true;}
            else{
                result["after"]=after_diagnostic;result["ledger_after"]=begin_ledger(ledger_after);
                auto owners=*before.creation;const auto item=std::find_if(owners.catalogue.begin(),owners.catalogue.end(),[&](const auto& value){return value.address==owners.source;});
                if(item==owners.catalogue.end())e="startingpoint begin source left owned catalogue";
                else{
                    auto current=*item;current.address=after.project;owners.current=current;
                    for(auto& developer:owners.developers)if(developer.job>=5&&developer.job<=7)developer.job=0;
                    expected.project_present=true;expected.work=0;expected.lines=expected.bugs_total=expected.bugs_known=expected.desired_workers=0;
                    if(owners.automatic_workers)expected.desired_workers=owners.total_workers<=owners.max_workers?owners.total_workers-1:owners.max_workers;
                    if(!after.project||after.project==before.creation->source||after.row!=expected||after.creation!=owners||
                       after.record!=before.record||after.tech!=before.tech||begin_ledger(ledger_after)!=expected_ledger)
                        e="startingpoint begin native allocation/copy/worker reset or ledger differs";
                    else phase->begun=true;
                }
            }
        }
    }
    if(e.empty()){phase->locked=false;phase->cause.clear();result["confirmed"]=true;result["unknown"]=false;}
    else{phase->cause=e;result["cause"]=e;}
    receipts[request.token]=result;return e.empty();
}

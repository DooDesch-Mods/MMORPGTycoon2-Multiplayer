bool research_capture(const action_session::Context& c,const action_session::Binding& b,
                      const World& world,State& out,std::string& e){
    tutorial_development::StartingpointSnapshot first; nlohmann::json diagnostic;
    if(!tutorial_development::read_startingpoint(first,diagnostic,e,false,true))return false;
    if(!same_world(world,first.origin))return fail(e,"Research capture Startingpoint Origin differs");
    if(!first.row||!first.row->deployed)return true;
    tutorial_development::StartingpointSnapshot current;
    if(!tutorial_development::read_research("bridges",current,diagnostic,e,true,true))return false;
    if(!same_world(world,current.origin))return fail(e,"Research capture full Origin differs");
    if(current.row){
        out.research=research::Row{"bridges",*current.row};
        if(current.row->deployed&&research_commit_receipt.bugs&&same_world(world,research_commit_receipt.origin)&&
           current.tech==research_commit_receipt.tech&&current.record==research_commit_receipt.record)
            out.research->value.commit_bugs=research_commit_receipt.bugs;
        if(current.row->project_present&&!display_native::capture(c,b,current,out.staff_display,e,true))return false;
    }
    return true;
}

// Project only the shared Release counters whose native effect was proved by
// this exact deployment. Historical terminal receipts remain immutable.
bool research_projection(const State& local,const State& target,State& projected,research::Plan& operation,std::string& e){
    projected=local;operation=research::Plan::None;
    if(!local.research&&target.research){
        startingpoint::Plan first;
        if(!startingpoint::plan(local.startingpoint,target.startingpoint,first,e)||first!=startingpoint::Plan::Deploy)
            return fail(e,"Research baseline appearance requires original Startingpoint deployment");
        const auto& row=target.research->value;
        if(row.project_present||row.purchased||row.deployed||row.custom||row.work||row.lines||row.desired_workers||
           row.bugs_total||row.bugs_known||row.inventory_count||row.release_feature_count||row.commit_bugs)
            return fail(e,"Research first visible baseline already has project effects");
        return true;
    }
    if(!research::plan(local.research,target.research,operation,e))return false;
    if(operation==research::Plan::Deploy){
        if(projected.startingpoint){
            if(!target.startingpoint)return fail(e,"Research deployment lost live Startingpoint receipt");
            auto expected=*projected.startingpoint;
            expected.release_change_count=target.research->value.release_change_count;
            expected.release_counter=target.research->value.release_counter;
            if(expected!=*target.startingpoint)return fail(e,"Research deployment changed unrelated Startingpoint fields");
            projected.startingpoint=expected;
        }
        projected.staff_display.reset();
        projected.research=target.research;
    }
    return true;
}

bool research_preflight(const action_session::Context& c,const action_session::Binding& b,
                        const State& local,const State& target,State& projected,std::string& e,StartingpointCalls* progress_pin=nullptr){
    research::Plan operation;
    if(!research_projection(local,target,projected,operation,e))return false;
    if(operation==research::Plan::None)return true;
    StartingpointCalls fresh{c,b};
    auto& calls=progress_pin?*progress_pin:fresh;calls.key=target.research->key;
    calls.share_reader_metadata=true;
    return calls.pin(local.research->value,operation==research::Plan::Deploy,e);
}

bool research_apply(const action_session::Context& c,const action_session::Binding& b,
                    const State& local,const State& target,bool& attempted,bool& unknown,std::string& e,StartingpointCalls* progress_pin=nullptr,
                    tutorial_development::StartingpointSnapshot* observed=nullptr){
    State projected;research::Plan operation;
    if(!research_projection(local,target,projected,operation,e))return false;
    if(operation==research::Plan::None)return true;
    StartingpointCalls fresh{c,b};
    auto& calls=progress_pin?*progress_pin:fresh;
    if(progress_pin){
        if(operation!=research::Plan::Project||calls.key!=target.research->key||!calls.before.row||
           *calls.before.row!=local.research->value||calls.creating)
            return fail(e,"Research prepared Progress owner differs");
    }else{calls.key=target.research->key;if(!calls.pin(local.research->value,operation==research::Plan::Deploy,e))return false;}
    attempted=unknown=true;
    if(operation==research::Plan::Deploy){
        if(!calls.deploy(e)||!calls.retire(e))return false;
    }else{
        if(operation==research::Plan::Create&&!calls.create(e))return false;
        if(!calls.project(target.research->value,e))return false;
    }
    if(!calls.readback(target.research->value,e,observed))return false;
    if(operation==research::Plan::Deploy)
        research_commit_receipt={calls.before.origin,calls.before.tech,calls.before.record,target.research->value.commit_bugs,{}};
    unknown=false;return true;
}

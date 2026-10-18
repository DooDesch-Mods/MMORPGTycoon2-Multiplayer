namespace {
bool research_snapshot(const action_session::Context& c,const action_session::Binding& b,
                       tutorial_development::StartingpointSnapshot& out,std::string& e){
    World first,last;nlohmann::json diagnostic;
    if(!origin(c,b,first,e)||!tutorial_development::read_research("bridges",out,diagnostic,e,true)||!origin(c,b,last,e))return false;
    return same_world(first,last)&&same_world(first,out.origin)||fail(e,"Research Commit full NativeOpen changed");
}
bool submit_client_research_commit(void* lounge,std::string& e){
    action_session::Context c;action_session::Binding b;
    if(!action_session::selected_session().current(c,e)||c.role!=action_session::Role::Client)
        return fail(e,e.empty()?"Research Commit intent requires joined Client":e);
    if(!typed_action_runtime::network_binding(c.snapshot_uuid,b,e))return false;
    tutorial_development::StartingpointSnapshot before,after;
    if(!research_snapshot(c,b,before,e)||!startingpoint_clock(e)||!complete_entry(e)||!verify_commit_entry(image+0x25fb10,e))return false;
    if(before.lounge!=reinterpret_cast<uintptr_t>(lounge)||!before.project||!before.row||
       !before.row->project_present||!before.row->purchased||before.row->deployed||before.row->custom)
        return fail(e,"Research Commit intent current native project differs");
    bool complete=false;const auto code=startingpoint_complete(before.lounge,complete);
    if(code)return fail(e,"Research Commit completion getter exception="+std::to_string(code));
    if(!complete)return fail(e,"Research Commit project is not natively complete");
    if(!research_snapshot(c,b,after,e))return false;
    if(!same_world(before.origin,after.origin)||before.project!=after.project||before.lounge!=after.lounge||
       before.tech!=after.tech||before.record!=after.record||before.row!=after.row||before.creation!=after.creation)
        return fail(e,"Research Commit intent changed during admission");
    std::string id;if(!intent_uuid(id,e))return false;client_commit_action=id;
    return typed_action_runtime::submit_commit_research(id,"bridges",e);
}
}
bool read_research_row(const action_session::Context& c,const action_session::Binding& b,
                       const std::string& key,research::Row& out,std::string& e){
    if(key!="bridges")return fail(e,"Research action Tech key differs");
    State state;std::vector<NativeCursor> cursors;if(!native_state(c,b,state,cursors,e))return false;
    if(!state.research||state.research->key!=key)return fail(e,"Research current keyed row unavailable");
    out=*state.research;return research::valid(out,e);
}
bool start_research(const action_session::Context& c,const action_session::Binding& b,
    const action_session::Ledger& before,const research::Row& row,action_session::Ledger& after,
    research::Row& result,bool& attempted,std::string& e){
    if(!research::valid(row,e))return false;
    result.key=row.key;bool unknown=false;
    return run_start_action(c,b,before,row.value,nullptr,nullptr,after,result.value,attempted,unknown,e,row.key);
}
bool apply_research_start(const action_session::Context& c,const action_session::Binding& b,const std::string& action,
    const action_session::Ledger& before,const action_session::Ledger& after,
    const research::Row& row,const research::Row& result,bool& attempted,bool& unknown,std::string& e){
    if(!research::valid(row,e)||!research::valid(result,e)||row.key!=result.key)return fail(e,e.empty()?"Research Start before/after key differs":e);
    action_session::Ledger measured;startingpoint::Row actual;
    auto local_result=result.value;local_result.release_counter=row.value.release_counter;
    if(!run_start_action(c,b,before,row.value,&after,&local_result,measured,actual,attempted,unknown,e,row.key))return false;
    bool presentation_attempted=false;std::string cause;
    if(!finish_start_presentation(action,c,b,presentation_attempted,cause)){
        client_start_presentation_cause=cause;
        if(presentation_attempted){unknown=true;e=cause;return false;}
        LOG_WARN("[Tutorial] Research StartTech presentation skipped cause={}",cause);
    }else client_start_presentation_cause.clear();
    return true;
}
bool set_research_workers(const action_session::Context& c,const action_session::Binding& b,
    const research::Row& expected,int32_t desired,bool& attempted,std::string& e){
    if(!research::valid(expected,e))return false;
    StaffCalls calls{c,b};calls.key=expected.key;
    return staff::apply(expected.value,desired,calls,attempted,e);
}
bool commit_research(const action_session::Context& c,const action_session::Binding& b,
    const research::Row& expected,bool& attempted,std::string& e){
    attempted=false;
    if(c.role!=action_session::Role::Host||load.armed)return fail(e,"Research original Commit requires Host");
    if(!research::valid(expected,e)||!verify_commit_entry(image+0x25fb10,e)||!startingpoint_code(e)||!complete_entry(e))return false;
    tutorial_development::StartingpointSnapshot before,again,after;
    if(!research_snapshot(c,b,before,e))return false;
    const auto& row=expected.value;
    if(!before.project||!before.row||*before.row!=row||!row.project_present||row.deployed||!row.purchased||row.custom||
       row.inventory_count||row.release_feature_count||row.release_change_count>=4096||row.release_counter==UINT32_MAX)
        return fail(e,"Research Host Commit current owned project/deployment baseline differs");
    if(!before.admitted)return fail(e,"Research Host Commit admission: "+before.admission.dump());
    if(std::find(before.items.begin(),before.items.end(),uintptr_t(0))==before.items.end())return fail(e,"Research Host Commit inventory full");
    research_release::Graph release_before,release_again,release_after;
    if(!research_release::read(before.release,release_before,e)||!research_release::admissible(release_before,before.tech,e))return false;
    bool complete=false;const auto getter=startingpoint_complete(before.lounge,complete);
    if(getter)return fail(e,"Research Host completion getter exception="+std::to_string(getter));
    if(!complete)return fail(e,"Research Host project is not natively complete");
    if(!research_snapshot(c,b,again,e))return false;
    if(!same_world(before.origin,again.origin)||before.lounge!=again.lounge||before.project!=again.project||
       before.tech!=again.tech||before.record!=again.record||before.player!=again.player||before.inventory!=again.inventory||
       before.slots!=again.slots||before.release!=again.release||before.row!=again.row||before.items!=again.items||
       before.creation!=again.creation||before.admission!=again.admission)
        return fail(e,"Research Host Commit owners changed before execution");
    if(!research_release::read(before.release,release_again,e))return false;
    if(release_again!=release_before)return fail(e,"Research Host Release graph changed before Commit");
    attempted=true;const auto code=startingpoint_commit_call(before.lounge);
    if(code)return fail(e,"Research Host original Commit exception="+std::to_string(code));
    if(!research_snapshot(c,b,after,e))return false;
    StartingpointCalls owners{c,b};owners.before=before;
    if(!owners.same_owners(after,e)||after.project||!before.creation||!after.creation)
        return fail(e,e.empty()?"Research Host original Commit retirement differs":e);
    size_t inserted=0;for(size_t i=0;i<before.items.size();++i)if(before.items[i]!=after.items[i]){
        if(before.items[i]||!after.items[i])return fail(e,"Research Commit replaced/removed existing inventory item");++inserted;
    }
    if(inserted!=1||after.creation->release_changes.size()!=before.creation->release_changes.size()+1)
        return fail(e,"Research Commit item/Release insertion count differs");
    for(auto member:before.creation->release_changes)
        if(std::count(after.creation->release_changes.begin(),after.creation->release_changes.end(),member)!=1)
            return fail(e,"Research Commit replaced/removed existing Release member");
    nlohmann::json release_receipt;
    if(!research_release::read(after.release,release_after,e)||!research_release::preserve(release_before,release_after,before.tech,release_receipt,e))return false;
    research_proof_origin=before.origin;
    research_commit_proof={{"key",expected.key},{"confirmed",false},{"release",std::move(release_receipt)}};
    research::Row actual;research::Plan operation=research::Plan::None;
    if(!read_research_row(c,b,expected.key,actual,e)||!research::plan(expected,actual,operation,e))return false;
    if(operation!=research::Plan::Deploy)return fail(e,"Research Host original Commit effect differs");
    research_commit_proof["release"]["scan_stamp_before"]=row.release_counter;
    research_commit_proof["release"]["scan_stamp_after"]=actual.value.release_counter;
    return true;
}
bool apply_research_commit(const action_session::Context& c,const action_session::Binding& b,
    const research::Row& before,const research::Row& after,bool& attempted,bool& unknown,std::string& e){
    attempted=unknown=false;if(c.role!=action_session::Role::Client)return fail(e,"Research Commit adoption requires Client");
    State target;std::vector<NativeCursor> cursors;if(!native_state(c,b,target,cursors,e))return false;
    if(target.research!=std::optional(before))return fail(e,"Research Client differs from exact Host BEFORE");
    target.research=after;target.staff_display.reset();
    if(target.startingpoint){target.startingpoint->release_change_count=after.value.release_change_count;target.startingpoint->release_counter=after.value.release_counter;}
    return apply(c,b,target,attempted,unknown,e);
}

bool retain_research_commit_effect(const action_session::Context& c,const action_session::Binding& b,
    const std::string& action,const bug_authority::State* before,const bug_authority::State* after,std::string& e){
    World current;if(!origin(c,b,current,e))return false;
    if(!same_world(current,research_proof_origin)||!research_commit_proof.is_object()||
       !research_commit_proof.contains("release")||!research_commit_proof.at("release").value("preserved",false))
        return fail(e,"Research Commit actual Release preservation receipt absent");
    if(c.role==action_session::Role::Host){
        if(!before||!after||!bug_authority::valid(*before,e)||!bug_authority::valid(*after,e))
            return fail(e,e.empty()?"Research Commit Host Bug before/after missing":e);
        const auto identity=[](const auto& a,const auto& b){return a.parent_kind==b.parent_kind&&a.parent_key==b.parent_key&&a.birth==b.birth;};
        bug_authority::State added,changed,removed;
        for(const auto& row:after->rows){const auto found=std::find_if(before->rows.begin(),before->rows.end(),[&](const auto& old){return identity(row,old);});
            if(found==before->rows.end()){
                if(row.birth.kind!=bug_authority::BirthKind::Action||row.birth.source!=action||row.birth.sequence)
                    return fail(e,"Research Commit new Bug lacks exact ordered Action birth");
                added.rows.push_back(row);
            }else if(*found!=row)changed.rows.push_back(row);
        }
        for(const auto& row:before->rows)if(std::none_of(after->rows.begin(),after->rows.end(),[&](const auto& now){return identity(row,now);}))removed.rows.push_back(row);
        research_commit_proof["map_bugs"]={{"observed",true},{"before_count",before->rows.size()},{"after_count",after->rows.size()},
            {"added",bug_authority::encode(added)},{"changed",bug_authority::encode(changed)},{"removed",bug_authority::encode(removed)}};
    }else if(before||after)return fail(e,"Research Client must adopt Host map Bugs through existing frame");
    research_commit_proof["action_uuid"]=action;research_commit_proof["snapshot_uuid"]=c.snapshot_uuid;
    research_commit_proof["native_origin_uuid"]=current.uuid;research_commit_proof["confirmed"]=true;
    return true;
}

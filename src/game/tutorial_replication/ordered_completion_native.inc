#include "ordered_commit_map.inc"
DWORD startingpoint_complete(uintptr_t,bool&);
bool complete_entry(std::string&);
DWORD ordered_completion_call(uintptr_t lounge){
    __try{original_commit(reinterpret_cast<void*>(lounge));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool completion_snapshot(const action_session::Context& c,const action_session::Binding& b,
    const action_session::Request* request,tutorial_development::StartingpointSnapshot& value,live_host_state::Pin& pin,std::string& e){
    if(request){StartActionCalls reader{c,b};reader.request=request;return reader.snapshot(value,pin,e);}
    bool admitted=false;action_session::Context actual;action_session::Binding current;
    if(!typed_action_runtime::original_go_admission(admitted,e)||!admitted)return fail(e,e.empty()?"Startingpoint completion requires current original Go":e);
    if(load.armed||Config::instance().is_host!=(c.role==action_session::Role::Host)||
        !action_session::selected_session().current(actual,e)||actual!=c||!typed_action_runtime::read_binding(c.snapshot_uuid,current,e)||current!=b)
        return fail(e,e.empty()?"Startingpoint completion actual role/Context/Binding or load differs":e);
    World first,last;nlohmann::json diagnostic;
    if(!native_open_runtime::read_current_world(first,e)||GetCurrentThreadId()!=mainthread||first.world_epoch!=b.native_epoch||first.generation!=b.native_generation)
        return fail(e,e.empty()?"Startingpoint completion local Mainthread/native binding differs":e);
    if(!fault.empty())return fail(e,fault);
    if(!tutorial_development::read_startingpoint(value,diagnostic,e,false)||!native_open_runtime::read_current_world(last,e)||
       !same_world(first,last)||!same_world(first,value.origin))return fail(e,e.empty()?"Startingpoint completion full NativeOpen changed":e);
    native::WorldStatus activity;if(!native::read_world_status(activity,e))return false;
    if(activity.saves_in_progress>0||activity.edit_operations)return fail(e,"Startingpoint completion requires Save/Edit idle");
    live_host_state::NativePort economy(c,b,nullptr);
    if(!economy.read(pin,e)||pin.world!=reinterpret_cast<uintptr_t>(value.origin.world))return fail(e,e.empty()?"Startingpoint completion Economy/NativeOpen differs":e);
    return typed_action_runtime::original_go_admission(admitted,e)&&admitted;
}
bool run_ordered_completion(const action_session::Context& c,const action_session::Binding& b,
    const action_session::Request* request,const startingpoint::Row& expected,bool& attempted,std::string& e){
    attempted=false;
    if(!startingpoint::valid(expected,e))return false;
    if(request&&(request->action!=action_session::Action::CommitStartingpoint||!action_session::tick_commands::authority::matches(c,b,*request,e)))
        return fail(e,e.empty()?"Startingpoint original Commit requires exact command":e);
    if(!verify_commit_entry(image+0x25fb10,e)||!startingpoint_code(e)||!complete_entry(e)||!ordered_commit_map::code(e))return false;
    tutorial_development::StartingpointSnapshot before;live_host_state::Pin economy;
    if(!completion_snapshot(c,b,request,before,economy,e))return false;
    if(!before.project||!before.row||*before.row!=expected||!expected.project_present||expected.deployed||!expected.purchased||expected.custom||
       expected.inventory_count||expected.release_feature_count||expected.release_change_count||expected.release_counter)
        return fail(e,"Ordered Startingpoint Commit requires exact completed initial project");
    if(!before.admitted)return fail(e,"Ordered Startingpoint Commit admission: "+before.admission.dump());
    if(std::find(before.items.begin(),before.items.end(),uintptr_t(0))==before.items.end())return fail(e,"Ordered Startingpoint Commit inventory full");
    bool complete=false;const auto getter=startingpoint_complete(before.lounge,complete);
    if(getter)return fail(e,"Ordered Startingpoint completion getter exception="+std::to_string(getter));
    if(!complete)return fail(e,"Ordered Startingpoint current project is not natively complete");
    tutorial_development::StartingpointSnapshot again;live_host_state::Pin checked;
    if(!completion_snapshot(c,b,request,again,checked,e))return false;
    if(!same_world(before.origin,again.origin)||before.lounge!=again.lounge||before.project!=again.project||before.tech!=again.tech||
       before.record!=again.record||before.player!=again.player||before.inventory!=again.inventory||before.slots!=again.slots||before.release!=again.release||
       before.row!=again.row||before.items!=again.items||before.admission!=again.admission||economy!=checked)
        return fail(e,"Ordered Startingpoint Commit native owners/full Economy changed before execution");
    nlohmann::json map_before,map_checked;research_release::Graph release_before,release_checked;
    if(!ordered_commit_map::read(before,map_before,e)||!ordered_commit_map::read(again,map_checked,e)||map_before!=map_checked||
       !research_release::read(before.release,release_before,e)||!research_release::read(again.release,release_checked,e)||release_before!=release_checked||
       !research_release::admissible(release_before,before.tech,e))return fail(e,e.empty()?"Startingpoint pre-Commit Map/RNG/Release changed":e);
    commit_receipt={before.origin,before.tech,before.record,std::nullopt,{}};
    attempted=true;const auto code=ordered_completion_call(before.lounge);
    if(code)return fail(e,"Ordered Startingpoint original Commit exception="+std::to_string(code));
    tutorial_development::StartingpointSnapshot after;live_host_state::Pin final_pin;
    if(!completion_snapshot(c,b,request,after,final_pin,e))return false;
    if(!same_world(before.origin,after.origin)||before.lounge!=after.lounge||before.tech!=after.tech||before.record!=after.record||
       before.player!=after.player||before.inventory!=after.inventory||before.slots!=after.slots||before.release!=after.release||after.project||!after.row||economy!=final_pin)
        return fail(e,"Ordered Startingpoint original Commit owners/retirement/full Economy differs");
    nlohmann::json map_after,release_receipt;research_release::Graph release_after;
    if(!ordered_commit_map::read(after,map_after,e)||!startingpoint::map_effect::check(map_before,map_after,expected.bugs_total,expected.bugs_known,e)||
       !research_release::read(after.release,release_after,e)||!research_release::preserve(release_before,release_after,before.tech,release_receipt,e))return false;
    size_t inserted=0;for(size_t i=0;i<before.items.size();++i){if(before.items[i]==after.items[i])continue;
        if(before.items[i]||!after.items[i])return fail(e,"Startingpoint Commit replaced/removed an existing inventory item");++inserted;}
    if(inserted!=1)return fail(e,"Startingpoint Commit did not insert exactly one inventory item");
    auto actual=*after.row;actual.commit_bugs=startingpoint::CommitBugs{before.row->bugs_total,before.row->bugs_known};actual.commit_bugs->map_before=ordered_commit_map::digest(map_before.dump());actual.commit_bugs->map_after=ordered_commit_map::digest(map_after.dump());startingpoint::Plan operation;
    if(!startingpoint::plan(expected,actual,operation,e)||operation!=startingpoint::Plan::Deploy)return fail(e,e.empty()?"Ordered Startingpoint original Commit effect differs":e);
    commit_receipt.bugs=actual.commit_bugs;return true;
}
bool ordered_commit_route(void* lounge){
    try{
    std::string e;action_session::Context c;action_session::Binding b;bool original_go=false,ordered=false;
    if(!typed_action_runtime::original_go_admission(original_go,e)){block(e);return false;}
    if(!action_session::selected_session().current(c,e)){block(e);return false;}
    if(original_go){
        if(!typed_action_runtime::read_binding(c.snapshot_uuid,b,e)){block(e);return false;}
        tutorial_development::StartingpointSnapshot before;live_host_state::Pin pin;
        if(!completion_snapshot(c,b,nullptr,before,pin,e)||before.lounge!=reinterpret_cast<uintptr_t>(lounge)||!before.row){block(e.empty()?"Startingpoint original Go Commit receiver differs":e);return false;}
        bool attempted=false;if(!run_ordered_completion(c,b,nullptr,*before.row,attempted,e)){block(e);return false;}
        return true;
    }
    // Outside original Go, the existing Commit entry is an intent. Commands call
    // the pinned original trampoline and never reenter this capture hook.
    client_commit_suppressed=true;client_commit_submitted=false;
    if(typed_action_runtime::network_binding_with_mode(c.snapshot_uuid,b,e,&ordered)&&ordered){
        StartActionCalls reader{c,b};reader.input=true;tutorial_development::StartingpointSnapshot before;live_host_state::Pin pin;
        std::string key;
        if(current_project_key(lounge,key,e)&&key=="startingpoint"&&reader.snapshot(before,pin,e)&&before.lounge==reinterpret_cast<uintptr_t>(lounge)&&
           complete_entry(e)&&verify_commit_entry(image+0x25fb10,e)&&before.project&&before.row&&before.row->project_present&&before.row->purchased&&!before.row->deployed&&!before.row->custom){
            bool complete=false;const auto getter=startingpoint_complete(before.lounge,complete);
            if(getter)e="Ordered Commit intent completion getter exception="+std::to_string(getter);
            else if(!complete)e="Ordered Commit intent project is not natively complete";
            else{
                tutorial_development::StartingpointSnapshot after;live_host_state::Pin checked;
                if(reader.snapshot(after,checked,e)&&same_world(before.origin,after.origin)&&before.lounge==after.lounge&&before.project==after.project&&
                   before.tech==after.tech&&before.record==after.record&&before.row==after.row&&pin==checked){
                    if(intent_uuid(client_commit_action,e))client_commit_submitted=typed_action_runtime::submit_commit_startingpoint(client_commit_action,e);
                }else if(e.empty())e="Ordered Commit intent owners/full Economy changed during admission";
            }
        }else if(e.empty())e="Ordered Commit intent requires current Startingpoint receiver";
    }
    if(!client_commit_submitted&&e.empty())e="Ordered Commit intent rejected without cause";
    if(client_commit_submitted)LOG_INFO("[Tutorial] Ordered Startingpoint Commit submitted action={}",client_commit_action);
    else if(e!=client_commit_cause)LOG_WARN("[Tutorial] Ordered Startingpoint Commit not submitted cause={}",e);
    client_commit_cause=e;return client_commit_submitted;
    }catch(const std::exception& x){block(std::string("Ordered Startingpoint Commit exception: ")+x.what());return false;}
    catch(...){block("Ordered Startingpoint Commit nonstandard exception");return false;}
}

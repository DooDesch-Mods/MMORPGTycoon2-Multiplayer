namespace {
DWORD startingpoint_complete(uintptr_t lounge,bool& result){
    __try{result=reinterpret_cast<bool(*)(void*)>(image+0x25ed50)(reinterpret_cast<void*>(lounge));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD startingpoint_commit_call(uintptr_t lounge){
    // Enter the owned hook so the original Commit also records its bug receipt.
    __try{reinterpret_cast<Call>(image+0x25fb10)(reinterpret_cast<void*>(lounge));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool complete_entry(std::string& e){
    constexpr std::array<unsigned char,16> expected={0x53,0x48,0x83,0xec,0x20,0x48,0x89,0xcb,0x48,0x8b,0x89,0xa8,0x00,0x00,0x00,0x48};
    std::array<unsigned char,16> actual{};
    return memory.copy(image+0x25ed50,actual.data(),actual.size(),e)&&(actual==expected||fail(e,"startingpoint native completion entry differs"));
}
bool commit_snapshot(const action_session::Context& c,const action_session::Binding& b,
                     tutorial_development::StartingpointSnapshot& value,std::string& e){
    World before,after;nlohmann::json diagnostic;
    if(!origin(c,b,before,e)||!tutorial_development::read_startingpoint(value,diagnostic,e,false)||!origin(c,b,after,e))return false;
    return same_world(before,after)&&same_world(before,value.origin)||fail(e,"startingpoint Commit full NativeOpen changed");
}
bool intent_uuid(std::string& out,std::string& e){
    unsigned char bytes[16];const auto code=BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(code<0)return fail(e,"startingpoint Commit UUID BCryptGenRandom status="+std::to_string(code));
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    char buffer[37];std::snprintf(buffer,sizeof(buffer),"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        bytes[0],bytes[1],bytes[2],bytes[3],bytes[4],bytes[5],bytes[6],bytes[7],bytes[8],bytes[9],bytes[10],bytes[11],bytes[12],bytes[13],bytes[14],bytes[15]);
    out=buffer;return true;
}
bool submit_client_commit(void* lounge,std::string& e){
    action_session::Context c;action_session::Binding b;
    if(!action_session::selected_session().current(c,e))return false;
    if(c.role!=action_session::Role::Client)return fail(e,"startingpoint Commit intent requires joined Client");
    if(!typed_action_runtime::network_binding(c.snapshot_uuid,b,e))return false;
    tutorial_development::StartingpointSnapshot before;
    if(!commit_snapshot(c,b,before,e)||!startingpoint_clock(e)||!complete_entry(e)||!verify_commit_entry(image+0x25fb10,e))return false;
    if(before.lounge!=reinterpret_cast<uintptr_t>(lounge)||!before.project||!before.row||
       !before.row->project_present||before.row->deployed||!before.row->purchased||before.row->custom)
        return fail(e,"startingpoint Commit intent current native project differs");
    bool complete=false;const auto code=startingpoint_complete(before.lounge,complete);
    if(code)return fail(e,"startingpoint Commit intent completion getter exception="+std::to_string(code));
    if(!complete)return fail(e,"startingpoint Commit intent project is not natively complete");
    tutorial_development::StartingpointSnapshot after;
    if(!commit_snapshot(c,b,after,e))return false;
    if(before.project!=after.project||before.lounge!=after.lounge||before.tech!=after.tech||
       before.record!=after.record||before.row!=after.row||!same_world(before.origin,after.origin))
        return fail(e,"startingpoint Commit intent project changed during admission");
    std::string id;if(!intent_uuid(id,e))return false;
    client_commit_action=id;
    // Client submission only sends the existing typed request. The Host invokes
    // Commit, and the Client ACK follows native adoption and readback.
    return typed_action_runtime::submit_commit_startingpoint(id,e);
}
}
bool read_startingpoint_row(const action_session::Context& c,const action_session::Binding& b,startingpoint::Row& out,std::string& e){
    State state;std::vector<NativeCursor> cursors;
    if(!native_state(c,b,state,cursors,e))return false;
    if(!state.startingpoint)return fail(e,"startingpoint Commit requires active authoritative tutorial project state");
    out=*state.startingpoint;return startingpoint::valid(out,e);
}
bool commit_startingpoint(const action_session::Context& c,const action_session::Binding& b,
                         const startingpoint::Row& expected,bool& attempted,std::string& e){
    attempted=false;
    if(c.role!=action_session::Role::Host||load.armed)return fail(e,"startingpoint original Commit requires Host");
    if(!verify_commit_entry(image+0x25fb10,e)||!startingpoint_code(e)||!complete_entry(e))return false;
    tutorial_development::StartingpointSnapshot before;
    if(!commit_snapshot(c,b,before,e))return false;
    if(!before.project||!before.row||*before.row!=expected||!expected.project_present||expected.deployed||
       !expected.purchased||expected.custom||expected.bugs_total||expected.bugs_known||expected.inventory_count||
       expected.release_feature_count||expected.release_change_count||expected.release_counter)
        return fail(e,"startingpoint Host Commit requires exact completed zero-bug initial project");
    if(!before.admitted)return fail(e,"startingpoint Host Commit admission: "+before.admission.dump());
    if(std::find(before.items.begin(),before.items.end(),uintptr_t(0))==before.items.end())return fail(e,"startingpoint Host Commit inventory full");
    bool complete=false;const auto getter=startingpoint_complete(before.lounge,complete);
    if(getter)return fail(e,"startingpoint Host Commit completion getter exception="+std::to_string(getter));
    if(!complete)return fail(e,"startingpoint Host Commit current project is not natively complete");
    tutorial_development::StartingpointSnapshot again;
    if(!commit_snapshot(c,b,again,e))return false;
    if(!same_world(before.origin,again.origin)||before.lounge!=again.lounge||before.project!=again.project||
       before.tech!=again.tech||before.record!=again.record||before.player!=again.player||before.inventory!=again.inventory||
       before.slots!=again.slots||before.release!=again.release||before.row!=again.row||before.items!=again.items||before.admission!=again.admission)
        return fail(e,"startingpoint Host Commit native owners changed before execution");
    attempted=true;const auto code=startingpoint_commit_call(before.lounge);
    if(code)return fail(e,"startingpoint Host original Commit exception="+std::to_string(code));
    tutorial_development::StartingpointSnapshot after;
    if(!commit_snapshot(c,b,after,e))return false;
    if(!same_world(before.origin,after.origin)||before.lounge!=after.lounge||before.tech!=after.tech||before.record!=after.record||
       before.player!=after.player||before.inventory!=after.inventory||before.slots!=after.slots||before.release!=after.release||after.project)
        return fail(e,"startingpoint Host original Commit owner/retirement differs");
    startingpoint::Row actual;startingpoint::Plan operation;
    if(!read_startingpoint_row(c,b,actual,e)||!startingpoint::plan(expected,actual,operation,e))return false;
    return operation==startingpoint::Plan::Deploy||fail(e,"startingpoint Host original Commit effect differs");
}
bool apply_startingpoint_commit(const action_session::Context& c,const action_session::Binding& b,
                               const startingpoint::Row& before,const startingpoint::Row& after,
                               bool& attempted,bool& unknown,std::string& e){
    attempted=unknown=false;
    State target;std::vector<NativeCursor> cursors;
    if(c.role!=action_session::Role::Client)return fail(e,"startingpoint Commit adoption requires Client");
    if(!native_state(c,b,target,cursors,e))return false;
    if(target.startingpoint!=std::optional<startingpoint::Row>(before))return fail(e,"startingpoint Commit Client differs from Host before row");
    target.startingpoint=after;
    return apply(c,b,target,attempted,unknown,e);
}

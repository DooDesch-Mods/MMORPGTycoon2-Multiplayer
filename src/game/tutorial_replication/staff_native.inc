staff::Receipt client_staff_receipt;
DWORD staff_debug_complete(uintptr_t lounge,bool& done){
    __try{done=reinterpret_cast<bool(*)(void*)>(image+0x25edb0)(reinterpret_cast<void*>(lounge));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD staff_allocate(uintptr_t lounge,int32_t count){
    __try{original_allocate(reinterpret_cast<void*>(lounge),count);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool staff_code(std::string& e){
    uintptr_t base=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(base,thread,sha,e))return false;
    if(base!=image||thread!=mainthread||thread!=GetCurrentThreadId()||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6")
        return fail(e,"Startingpoint staff actual EXE/Mainthread differs");
    if(!staff::verify_owned_allocate(owned_staff,owned_staff.target,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e))return false;
    std::vector<unsigned char> body(0x70);if(!memory.copy(image+0x25edb0,body.data(),body.size(),e))return false;
    return mt2mp::sha256(body)=="570f31fa2dead3d5677ab4bdecccb3d8c09e20433e500a523588b2de4c14c223"||fail(e,"Startingpoint staff native debugging completion body differs");
}
struct StaffCalls {
    const action_session::Context& context;const action_session::Binding& binding;
    tutorial_development::StartingpointSnapshot before;live_host_state::Pin economy;
    int32_t available=0;std::string key="startingpoint";
    bool snapshot(tutorial_development::StartingpointSnapshot& value,live_host_state::Pin& pin,std::string& e){
        StartActionCalls reader{context,binding};reader.key=key;return reader.snapshot(value,pin,e);
    }
    bool admit(int32_t desired,std::string& e){
        if((context.role==action_session::Role::Client)!=load.armed)return fail(e,"Startingpoint staff actual joined role differs");
        if(!staff_code(e)||!snapshot(before,economy,e))return false;
        if(!before.project||!before.row||!before.creation||!before.row->project_present||!before.row->purchased||before.row->deployed||before.row->custom)
            return fail(e,"Startingpoint staff requires actual current purchased project");
        if(!at(economy.business,0x4c,available,e))return false;
        if(!staff::value(available)||!staff::value(desired)||before.creation->max_workers<0||desired>std::min(available,before.creation->max_workers))
            return fail(e,"Startingpoint staff desired="+std::to_string(desired)+" exceeds native available="+std::to_string(available)+"/maximum="+std::to_string(before.creation->max_workers));
        bool done=false;const auto code=staff_debug_complete(before.lounge,done);
        if(code)return fail(e,"Startingpoint staff native completion getter exception="+std::to_string(code));
        return !done||fail(e,"Startingpoint staff native debugging already complete");
    }
    bool read(startingpoint::Row& row,action_session::Ledger& ledger,std::string& e){
        tutorial_development::StartingpointSnapshot value;live_host_state::Pin pin;int32_t count=0;
        if(!snapshot(value,pin,e)||!at(pin.business,0x4c,count,e))return false;
        if(!same_world(before.origin,value.origin)||before.image!=value.image||before.lounge!=value.lounge||before.project!=value.project||
           before.tech!=value.tech||before.record!=value.record||before.player!=value.player||before.inventory!=value.inventory||
           before.slots!=value.slots||before.items!=value.items||before.release!=value.release||before.admission!=value.admission||
           before.creation!=value.creation||pin!=economy||count!=available||!value.row)
            return fail(e,"Startingpoint staff current native owners/census/full Economy changed");
        row=*value.row;ledger=live_host_state::ledger(pin.values);return true;
    }
    bool allocate(int32_t desired,std::string& e){
        if(!staff_code(e))return false;
        const auto code=staff_allocate(before.lounge,desired);
        return !code||fail(e,"Startingpoint staff original AllocateTechWorkers exception="+std::to_string(code));
    }
};
bool staff_window(const tutorial_development::StartingpointSnapshot& snapshot,int32_t desired,std::string& e){
    uintptr_t data=0,window=0;int32_t count=0,capacity=0;
    const auto mode=reinterpret_cast<uintptr_t>(snapshot.origin.mode);
    if(!at(mode,0xa0,data,e)||!at(mode,0xa8,count,e)||!at(mode,0xac,capacity,e))return false;
    if(count<1||capacity<count||capacity>512)return fail(e,"Startingpoint staff Mode window bounds invalid");
    std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t entry=0;std::string name;
        if(!at(data,size_t(i)*8,entry,e)||!entry||!seen.insert(entry).second)return fail(e,e.empty()?"Startingpoint staff Mode window owner duplicated":e);
        if(!str(entry,0x1b0,name,e))return false;
        if(name=="development"){if(window)return fail(e,"Startingpoint staff DevelopmentWindow duplicated");window=entry;}}
    if(!window||!type(window,0x1722e60,e))return fail(e,e.empty()?"Startingpoint staff owned DevelopmentWindow absent":e);
    uintptr_t logic=0,pips=0,owner=0;uint8_t visible=0;int32_t selected=0;
    if(!at(window,0x318,logic,e)||!at(window,0x45f,visible,e)||!at(window,0x588,pips,e)||!type(pips,0x1744290,e)||
       !at(pips,0x280,owner,e)||!at(pips,0x3e0,selected,e))return false;
    return (!logic&&visible==1&&owner==window&&selected==desired)||fail(e,"Startingpoint staff actual visible Window/Pips owner or selected value differs");
}
struct StaffCaptureCalls {
    void* receiver;int32_t desired;std::string key="startingpoint";
    void original(){original_allocate(receiver,desired);}
    bool pin(std::string& e){
        action_session::Context context;action_session::Binding binding;
        if(!action_session::selected_session().current(context,e)||context.role!=action_session::Role::Client)
            return fail(e,e.empty()?"Startingpoint staff intent requires joined Client":e);
        if(!typed_action_runtime::network_binding(context.snapshot_uuid,binding,e))return false;
        if(!current_project_key(receiver,key,e))return false;
        StaffCalls calls{context,binding};calls.key=key;
        if(!calls.admit(desired,e)||calls.before.lounge!=reinterpret_cast<uintptr_t>(receiver))return fail(e,e.empty()?"Startingpoint staff UI receiver differs from current lounge":e);
        if(!start_intent::verify_ui(image,[](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e,developer_staff::action_runtime::verify_entry)||!staff_window(calls.before,desired,e))return false;
        startingpoint::Row row;action_session::Ledger ledger;
        return calls.read(row,ledger,e)&&row==*calls.before.row||fail(e,e.empty()?"Startingpoint staff capture changed native row":e);
    }
    bool uuid(std::string& id,std::string& e){return intent_uuid(id,e);}
    bool submit(const std::string& id,std::string& e){return key=="bridges"?typed_action_runtime::submit_research_workers(id,key,desired,e):typed_action_runtime::submit_startingpoint_workers(id,desired,e);}
};
void allocate_current(void* receiver,int32_t desired){
    if(load.armed&&GetCurrentThreadId()!=mainthread){block("Client staff allocation thread differs");return;}
    StaffCaptureCalls calls{receiver,desired};staff::Receipt receipt;
    staff::capture(staff::capture_route(load.armed,GetCurrentThreadId()==mainthread,image,reinterpret_cast<uintptr_t>(_ReturnAddress())),desired,calls,receipt);
    if(receipt.suppressed){
        if(receipt.submitted)LOG_INFO("[Tutorial] Client Startingpoint staff submitted action={} desired={}",receipt.action_uuid,desired);
        else if(receipt.cause!=client_staff_receipt.cause)LOG_WARN("[Tutorial] Client Startingpoint staff not submitted cause={}",receipt.cause);
        client_staff_receipt=std::move(receipt);
    }
}
